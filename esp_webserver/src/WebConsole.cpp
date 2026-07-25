// Copyright (C) 2026 Lone Crow Design, LLC
// Licensed under GPLv3 — see LICENSE
//
// See WebConsole.h for the design and the concurrency contract. In short:
// inbound WS/HTTP requests arrive on the async TCP task; command handlers and
// variable setters are deferred onto an owned queue and executed from tick()
// in the loop() task, so user code never runs in the async context.
//
// WebConsole is intended as a single instance per sketch — the cross-task
// queues below are guarded by file-static mutexes shared across instances,
// which is correct (if slightly over-serialized) should you ever run two.

#include "WebConsole.h"
#include "WebConsoleUI.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>  // pulls in AsyncWebSocket, AwsFrameInfo, WS_TEXT

#include <mutex>
#include <cstdarg>
#include <cstring>

namespace jelly::webconsole {

static std::mutex gJobsMx;  // guards jobs_
static std::mutex gLogMx;   // guards logRing_/logSeq_/lastPushedSeq_

// ── A Serial-like sink: each newline-terminated line becomes a log entry. ─────
class WebConsole::LineSink : public Print {
 public:
  explicit LineSink(WebConsole* owner) : owner_(owner) {}
  size_t write(uint8_t b) override {
    if (b == '\n') { owner_->log(buf_); buf_ = ""; }
    else if (b != '\r') buf_ += (char)b;
    return 1;
  }
  size_t write(const uint8_t* p, size_t n) override {
    for (size_t i = 0; i < n; i++) write(p[i]);
    return n;
  }
 private:
  WebConsole* owner_;
  String buf_;
};

// A JSON value of unknown type → the flat string the variable setters parse.
static String variantToStr(JsonVariantConst v) {
  if (v.is<const char*>()) return String(v.as<const char*>());
  if (v.is<bool>())        return v.as<bool>() ? "1" : "0";
  if (v.isNull())          return String();
  return String(v.as<double>(), 6);
}

static const char* typeName(VarType t) {
  switch (t) {
    case VarType::Bool:   return "bool";
    case VarType::Int:    return "int";
    case VarType::Float:  return "float";
    case VarType::String: return "string";
  }
  return "string";
}

static const char* fieldTypeName(FieldType t) {
  switch (t) {
    case FieldType::Text:   return "text";
    case FieldType::Number: return "number";
    case FieldType::Bool:   return "bool";
    case FieldType::Enum:   return "enum";
  }
  return "text";
}

// Read a collection's backing file into doc. False (and doc left empty) if the
// file is missing or not valid JSON — callers treat that as "start from empty".
static bool loadColl(fs::FS* fs, const char* path, JsonDocument& doc) {
  if (!fs || !fs->exists(path)) return false;
  File f = fs->open(path, FILE_READ);
  if (!f) return false;
  DeserializationError e = deserializeJson(doc, f);
  f.close();
  return !e;
}

// ─── Registration (call before begin) ────────────────────────────────────────

// Serialize a Field[] schema into a manifest array — shared by commands (arg
// schema) and collections (record schema) so both describe fields identically.
static void serializeFields(JsonArray arr, const Field* fields, size_t n) {
  for (size_t i = 0; i < n; i++) {
    const Field& f = fields[i];
    JsonObject fo = arr.add<JsonObject>();
    fo["name"] = f.name; fo["label"] = f.label ? f.label : f.name;
    fo["type"] = fieldTypeName(f.type); fo["required"] = f.required;
    if (f.options) fo["options"] = f.options;
  }
}

void WebConsole::onCommand(const char* name, const char* help, CmdHandler fn,
                           const CommandOpts& opts) {
  cmds_.push_back(Cmd{name, help ? help : "", std::move(fn),
                      opts.args, opts.argCount, opts.pinned, opts.confirm});
}

void WebConsole::addVar(const char* name, VarType type, const char* help, bool persist,
                        VarGetter get, VarSetter set) {
  vars_.push_back(Var{name, help ? help : "", type, persist, std::move(get), std::move(set)});
}

int WebConsole::findCmd(const char* name) const {
  for (size_t i = 0; i < cmds_.size(); i++)
    if (!strcmp(cmds_[i].name, name)) return (int)i;
  return -1;
}
int WebConsole::findVar(const char* name) const {
  for (size_t i = 0; i < vars_.size(); i++)
    if (!strcmp(vars_[i].name, name)) return (int)i;
  return -1;
}

void WebConsole::addFile(const char* name, const char* path, const char* contentType, bool allowClear) {
  files_.push_back(FileEntry{name, path, contentType ? contentType : "application/octet-stream", allowClear});
}
int WebConsole::findFile(const char* name) const {
  for (size_t i = 0; i < files_.size(); i++)
    if (!strcmp(files_[i].name, name)) return (int)i;
  return -1;
}

void WebConsole::onUpload(const char* name, const char* path, UploadDoneFn onDone, const char* help) {
  uploads_.push_back(UploadEntry{name, path, help ? help : "", std::move(onDone)});
}
int WebConsole::findUpload(const char* name) const {
  for (size_t i = 0; i < uploads_.size(); i++)
    if (!strcmp(uploads_[i].name, name)) return (int)i;
  return -1;
}

void WebConsole::collection(const char* name, const char* path, const Field* fields,
                            size_t fieldCount, const CollectionOpts& opts) {
  colls_.push_back(Collection{name, path, opts.rootKey ? opts.rootKey : "records",
                              opts.help ? opts.help : "", fields, fieldCount,
                              opts.maxRecords, opts.validate, opts.onChanged});
}
int WebConsole::findColl(const char* name) const {
  for (size_t i = 0; i < colls_.size(); i++)
    if (!strcmp(colls_[i].name, name)) return (int)i;
  return -1;
}

// Compose form params into a JSON record per the schema. Text/Enum add when
// non-empty; Number parses when present; Bool adds `true` only when checked
// (kept out of the record otherwise, so records stay clean). Runs in async ctx
// but writes into owned Job storage.
void WebConsole::assembleRecord(const Collection& c, AsyncWebServerRequest* req, JsonObject rec) const {
  for (size_t i = 0; i < c.fieldCount; i++) {
    const Field& f = c.fields[i];
    if (!req->hasParam(f.name, true)) continue;
    String v = req->getParam(f.name, true)->value();
    switch (f.type) {
      case FieldType::Bool:
        if (v == "1" || v == "true" || v == "on") rec[f.name] = true;
        break;
      case FieldType::Number:
        if (v.length()) rec[f.name] = v.toDouble();
        break;
      default:  // Text, Enum
        if (v.length()) rec[f.name] = v;
        break;
    }
  }
}

// ─── Lifecycle ───────────────────────────────────────────────────────────────

bool WebConsole::begin(const Config& cfg) {
  if (started_) return true;
  cfg_ = cfg;

  // Only manage the radio when we own the AP; otherwise the caller already
  // connected in STA mode and we just attach a server to it.
  if (cfg_.apSsid) {
    WiFi.mode(WIFI_AP);
    if ((uint32_t)cfg_.apIp != 0) WiFi.softAPConfig(cfg_.apIp, cfg_.apIp, cfg_.apNetmask);
    const char* pass = (cfg_.apPassword && strlen(cfg_.apPassword) >= 8) ? cfg_.apPassword : nullptr;
    WiFi.softAP(cfg_.apSsid, pass);
  }

  loadPersistedVars();

  server_ = new AsyncWebServer(cfg_.port);
  ws_ = new AsyncWebSocket("/ws");

  ws_->onEvent([this](AsyncWebSocket*, AsyncWebSocketClient* c, AwsEventType type,
                      void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
      // Live client pointer — safe to use synchronously inside the callback.
      String m = buildManifest();
      c->text(m);
    } else if (type == WS_EVT_DATA) {
      AwsFrameInfo* info = (AwsFrameInfo*)arg;
      // Handle only self-contained text frames; console messages are small.
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
        handleWsText(c, data, len);
    }
  });
  server_->addHandler(ws_);

  // ── HTTP routes (curl-friendly mirror of the WS surface) ──
  server_->on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    String p;
    p.reserve(sizeof(WEBCONSOLE_CSS) + sizeof(WEBCONSOLE_BODY) + 256);
    p += F("<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
           "<title>Web Console</title><style>");
    p += FPSTR(WEBCONSOLE_CSS);
    p += F("</style></head><body>");
    p += FPSTR(WEBCONSOLE_BODY);
    p += F("</body></html>");
    req->send(200, "text/html", p);
  });

  server_->on("/api/manifest", HTTP_GET, [this](AsyncWebServerRequest* req) {
    req->send(200, "application/json", buildManifest());
  });

  // GET /api/log            → live RAM tail as text
  // GET /api/log?since=N    → only lines newer than seq N
  // GET /api/log?file=1     → stream the full persisted log file (needs cfg.fs)
  server_->on("/api/log", HTTP_GET, [this](AsyncWebServerRequest* req) {
    if (req->hasParam("file") && cfg_.fs) {
      if (cfg_.fs->exists(cfg_.logPath)) { req->send(*cfg_.fs, cfg_.logPath, "text/plain"); return; }
      req->send(404, "text/plain", "no log file yet");
      return;
    }
    uint32_t since = req->hasParam("since") ? (uint32_t)req->getParam("since")->value().toInt() : 0;
    String body;
    {
      std::lock_guard<std::mutex> lk(gLogMx);
      for (auto& l : logRing_)
        if (l.seq > since) { body += l.text; body += '\n'; }
    }
    req->send(200, "text/plain; charset=utf-8", body);
  });

  server_->on("/api/vars", HTTP_GET, [this](AsyncWebServerRequest* req) {
    JsonDocument d;
    JsonArray a = d.to<JsonArray>();
    for (auto& v : vars_) {
      JsonObject o = a.add<JsonObject>();
      o["name"] = v.name; o["type"] = typeName(v.type); o["value"] = v.get();
    }
    String out; serializeJson(d, out);
    req->send(200, "application/json", out);
  });

  // GET /api/file?name=<n>  → download a registered domain file.
  server_->on("/api/file", HTTP_GET, [this](AsyncWebServerRequest* req) {
    if (!req->hasParam("name")) { req->send(400, "text/plain", "missing name"); return; }
    int idx = findFile(req->getParam("name")->value().c_str());
    if (idx < 0) { req->send(404, "text/plain", "no such file"); return; }
    const FileEntry& fe = files_[idx];
    if (!cfg_.fs || !cfg_.fs->exists(fe.path)) { req->send(404, "text/plain", "file not present"); return; }
    req->send(*cfg_.fs, fe.path, fe.contentType, /*download=*/true);
  });

  // POST /api/file/clear  name=<n> [token]  → erase a clearable registered file.
  server_->on("/api/file/clear", HTTP_POST, [this](AsyncWebServerRequest* req) {
    if (!authOkHttp(req)) { req->send(401, "text/plain", "auth required"); return; }
    if (!req->hasParam("name", true)) { req->send(400, "text/plain", "missing name"); return; }
    int idx = findFile(req->getParam("name", true)->value().c_str());
    if (idx < 0 || !files_[idx].allowClear) { req->send(404, "text/plain", "not clearable"); return; }
    if (cfg_.fs) cfg_.fs->remove(files_[idx].path);
    req->send(200, "application/json", "{\"ok\":true}");
  });

  // POST /api/upload?name=<n>  (multipart) — stream the body to the slot's file
  // on cfg.fs, then validate in loop(). The upload callback runs before the
  // request callback, so auth is checked there (index 0) to reject *before*
  // overwriting the target file.
  server_->on(
      "/api/upload", HTTP_POST,
      [this](AsyncWebServerRequest* req) {  // onRequest: whole body received
        if (uploadIdx_ < 0) { req->send(uploadFile_ ? 500 : 401, "text/plain", "upload rejected"); return; }
        Job j; j.kind = Job::Upload; j.index = uploadIdx_; j.clientId = 0;
        enqueue(std::move(j));
        uploadIdx_ = -1;
        req->send(202, "application/json", "{\"queued\":true}");
      },
      [this](AsyncWebServerRequest* req, const String&, size_t index, uint8_t* data, size_t len, bool final) {
        if (index == 0) {  // first chunk: resolve slot + auth, open the file
          uploadIdx_ = (authOkHttp(req) && req->hasParam("name"))
                           ? findUpload(req->getParam("name")->value().c_str())
                           : -1;
          if (uploadIdx_ >= 0 && cfg_.fs) {
            cfg_.fs->remove(uploads_[uploadIdx_].path);
            uploadFile_ = cfg_.fs->open(uploads_[uploadIdx_].path, FILE_WRITE);
          }
        }
        if (uploadIdx_ >= 0 && uploadFile_) uploadFile_.write(data, len);
        if (final && uploadFile_) uploadFile_.close();
      });

  // GET /api/collection?name=<n>  → the records array as JSON ("[]" if none).
  server_->on("/api/collection", HTTP_GET, [this](AsyncWebServerRequest* req) {
    if (!req->hasParam("name")) { req->send(400, "text/plain", "missing name"); return; }
    int idx = findColl(req->getParam("name")->value().c_str());
    if (idx < 0) { req->send(404, "text/plain", "no such collection"); return; }
    const Collection& c = colls_[idx];
    JsonDocument doc; loadColl(cfg_.fs, c.path, doc);
    String body;
    if (doc[c.rootKey].is<JsonArray>()) serializeJson(doc[c.rootKey], body);
    else body = "[]";
    req->send(200, "application/json", body);
  });

  // POST /api/collection/{create,edit,delete}?name=<coll>[&index=<i>]  with the
  // record's fields in the POST body. The collection id + index live in the QUERY
  // string (post=false) so they never collide with a record field literally named
  // "name" or "index" — assembleRecord reads fields from the body (post=true).
  // All mutate the backing file in loop() context; return 202, result over WS.
  auto collMutate = [this](AsyncWebServerRequest* req, int op) {
    if (!authOkHttp(req)) { req->send(401, "text/plain", "auth required"); return; }
    if (!req->hasParam("name")) { req->send(400, "text/plain", "missing name"); return; }
    int idx = findColl(req->getParam("name")->value().c_str());
    if (idx < 0) { req->send(404, "text/plain", "no such collection"); return; }
    Job j; j.kind = Job::Collection; j.index = idx; j.collOp = op; j.clientId = 0;
    if (req->hasParam("index")) j.recIndex = req->getParam("index")->value().toInt();
    if (op != 2) { JsonObject rec = j.args.to<JsonObject>(); assembleRecord(colls_[idx], req, rec); }
    enqueue(std::move(j));
    req->send(202, "application/json", "{\"queued\":true}");
  };
  server_->on("/api/collection/create", HTTP_POST, [collMutate](AsyncWebServerRequest* r) { collMutate(r, 0); });
  server_->on("/api/collection/edit",   HTTP_POST, [collMutate](AsyncWebServerRequest* r) { collMutate(r, 1); });
  server_->on("/api/collection/delete", HTTP_POST, [collMutate](AsyncWebServerRequest* r) { collMutate(r, 2); });

  // POST /api/var  name=<n> value=<v> [token=<t>]  — applied in loop(), returns 202.
  server_->on("/api/var", HTTP_POST, [this](AsyncWebServerRequest* req) {
    if (!authOkHttp(req)) { req->send(401, "text/plain", "auth required"); return; }
    if (!req->hasParam("name", true) || !req->hasParam("value", true)) {
      req->send(400, "text/plain", "missing name/value"); return;
    }
    int idx = findVar(req->getParam("name", true)->value().c_str());
    if (idx < 0) { req->send(404, "text/plain", "no such variable"); return; }
    Job j; j.kind = Job::VarSet; j.index = idx; j.clientId = 0;
    j.value = req->getParam("value", true)->value();
    enqueue(std::move(j));
    req->send(202, "application/json", "{\"queued\":true}");
  });

  // POST /api/cmd  name=<n> [args=<json>] [token=<t>]  — fire-and-forget; the
  // result is delivered over the WebSocket. HTTP stays single-round-trip and
  // the handler still runs in loop() context.
  server_->on("/api/cmd", HTTP_POST, [this](AsyncWebServerRequest* req) {
    if (!authOkHttp(req)) { req->send(401, "text/plain", "auth required"); return; }
    if (!req->hasParam("name", true)) { req->send(400, "text/plain", "missing name"); return; }
    int idx = findCmd(req->getParam("name", true)->value().c_str());
    if (idx < 0) { req->send(404, "text/plain", "no such command"); return; }
    Job j; j.kind = Job::Cmd; j.reqId = 0; j.index = idx; j.clientId = 0;
    if (req->hasParam("args", true))
      deserializeJson(j.args, req->getParam("args", true)->value());
    enqueue(std::move(j));
    req->send(202, "application/json",
              "{\"queued\":true,\"note\":\"result is delivered over the WebSocket\"}");
  });

  server_->begin();

  if (cfg_.deviceName && MDNS.begin(cfg_.deviceName))
    MDNS.addService("http", "tcp", cfg_.port);

  sink_ = new LineSink(this);
  started_ = true;
  log(String("web console up on ") + ip().toString());
  return true;
}

void WebConsole::stop() {
  if (!started_) return;
  if (server_) { server_->end(); delete server_; server_ = nullptr; }
  if (ws_) { delete ws_; ws_ = nullptr; }
  // Release mDNS before any WiFi teardown, mirroring the begin() guard. Without
  // this the responder stays initialized and the next begin()'s MDNS.begin()
  // returns false (ESP_ERR_INVALID_STATE), so the service is never re-advertised.
  if (cfg_.deviceName) MDNS.end();
  if (cfg_.apSsid) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);  // restore station mode so the host's scanning resumes
  }
  delete sink_; sink_ = nullptr;
  started_ = false;
}

// ─── Per-loop service ────────────────────────────────────────────────────────

void WebConsole::tick() {
  if (!started_) return;

  // 1) Run queued commands / variable writes in loop() context.
  std::vector<Job> jobs;
  { std::lock_guard<std::mutex> lk(gJobsMx); jobs.swap(jobs_); }
  for (auto& j : jobs) {
    if (j.kind == Job::Cmd) {
      Cmd& c = cmds_[j.index];
      String res = c.fn ? c.fn(j.args.as<JsonVariantConst>()) : String();
      JsonDocument d;
      d["t"] = "cmd_result"; d["id"] = j.reqId; d["name"] = c.name; d["ok"] = true; d["result"] = res;
      String out; serializeJson(d, out);
      ws_->textAll(out);
      log(String("[cmd] ") + c.name + " → " + res);
    } else if (j.kind == Job::VarSet) {
      Var& v = vars_[j.index];
      v.set(j.value);
      if (v.persist) persistVar(v, v.get());
      JsonDocument d;
      d["t"] = "var"; d["name"] = v.name; d["value"] = v.get();
      String out; serializeJson(d, out);
      ws_->textAll(out);
    } else if (j.kind == Job::Upload) {  // file already written; validate/ingest here
      UploadEntry& u = uploads_[j.index];
      String res = u.onDone ? u.onDone(String(u.path)) : String("uploaded");
      JsonDocument d;
      d["t"] = "upload_result"; d["name"] = u.name; d["ok"] = true; d["result"] = res;
      String out; serializeJson(d, out);
      ws_->textAll(out);
      log(String("[upload] ") + u.name + " → " + res);
    } else {  // Job::Collection — read-modify-write the backing file in loop context
      Collection& c = colls_[j.index];
      JsonDocument doc; loadColl(cfg_.fs, c.path, doc);
      JsonArray arr = doc[c.rootKey].is<JsonArray>() ? doc[c.rootKey].as<JsonArray>()
                                                     : doc[c.rootKey].to<JsonArray>();
      bool ok = true; String res;
      if (j.collOp == 2) {  // delete
        if (j.recIndex >= 0 && j.recIndex < (int)arr.size()) { arr.remove(j.recIndex); res = "deleted"; }
        else { ok = false; res = "bad index"; }
      } else {
        JsonObject rec = j.args.as<JsonObject>();
        if (c.validate) { String err = c.validate(rec); if (err.length()) { ok = false; res = err; } }
        if (ok && j.collOp == 0) {  // create (append)
          if (arr.size() >= c.maxRecords) { ok = false; res = "list full"; }
          else { arr.add(rec); res = "added"; }
        } else if (ok) {  // edit (replace by index)
          if (j.recIndex >= 0 && j.recIndex < (int)arr.size()) { arr[j.recIndex].set(rec); res = "updated"; }
          else { ok = false; res = "bad index"; }
        }
      }
      if (ok && cfg_.fs) {
        File f = cfg_.fs->open(c.path, FILE_WRITE);
        if (f) { serializeJson(doc, f); f.close(); }
        if (c.onChanged) c.onChanged();  // let the host refresh derived in-memory state
      }
      JsonDocument d;
      d["t"] = "collection_changed"; d["name"] = c.name; d["ok"] = ok; d["result"] = res;
      String out; serializeJson(d, out);
      ws_->textAll(out);
      log(String("[coll] ") + c.name + " " + res);
    }
  }

  // 2) Flush new log lines to WS clients and the persisted file.
  std::vector<LogLine> fresh;
  {
    std::lock_guard<std::mutex> lk(gLogMx);
    for (auto& l : logRing_)
      if (l.seq > lastPushedSeq_) fresh.push_back(l);
    lastPushedSeq_ = logSeq_;
  }
  if (!fresh.empty()) {
    for (auto& l : fresh) {
      JsonDocument d;
      d["t"] = "log"; d["seq"] = l.seq; d["ms"] = l.ms; d["line"] = l.text;
      String out; serializeJson(d, out);
      ws_->textAll(out);
    }
    if (cfg_.fs) {
      File f = cfg_.fs->open(cfg_.logPath, FILE_APPEND);
      if (f) {
        for (auto& l : fresh) { f.print(l.text); f.print('\n'); }
        size_t sz = f.size();
        f.close();
        if (sz > cfg_.logMaxBytes) {  // single-generation rotation
          String bak = String(cfg_.logPath) + ".1";
          cfg_.fs->remove(bak);
          cfg_.fs->rename(cfg_.logPath, bak);
        }
      }
    }
  }

  // 3) Reap disconnected WebSocket clients (async lib requires periodic sweeps).
  ws_->cleanupClients();
}

// ─── Logging ─────────────────────────────────────────────────────────────────

void WebConsole::log(const String& line) {
  std::lock_guard<std::mutex> lk(gLogMx);
  logRing_.push_back(LogLine{++logSeq_, millis(), line});
  if (cfg_.ramLines && logRing_.size() > cfg_.ramLines)
    logRing_.erase(logRing_.begin(), logRing_.begin() + (logRing_.size() - cfg_.ramLines));
}

void WebConsole::logf(const char* fmt, ...) {
  char buf[256];
  va_list ap; va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  log(String(buf));
}

Print& WebConsole::io() {
  if (!sink_) sink_ = new LineSink(this);  // usable even before begin()
  return *sink_;
}

// ─── Inbound WS message handling (async task) ────────────────────────────────

void WebConsole::handleWsText(AsyncWebSocketClient* c, const uint8_t* data, size_t len) {
  JsonDocument doc;
  if (deserializeJson(doc, data, len)) return;  // ignore malformed frames
  const char* t = doc["t"] | "";

  if (!strcmp(t, "hello")) {
    String m = buildManifest();
    c->text(m);
    return;
  }
  if (!strcmp(t, "cmd")) {
    if (!authOkWs(doc)) { c->text("{\"t\":\"error\",\"msg\":\"auth required\"}"); return; }
    const char* name = doc["name"] | (doc["cmd"] | "");
    int idx = findCmd(name);
    if (idx < 0) return;
    Job j; j.kind = Job::Cmd; j.reqId = doc["id"] | 0u; j.index = idx; j.clientId = c->id();
    j.args.set(doc["args"]);  // deep copy into owned storage; the frame is freed on return
    enqueue(std::move(j));
    return;
  }
  if (!strcmp(t, "var_set")) {
    if (!authOkWs(doc)) { c->text("{\"t\":\"error\",\"msg\":\"auth required\"}"); return; }
    int idx = findVar(doc["name"] | "");
    if (idx < 0) return;
    Job j; j.kind = Job::VarSet; j.index = idx; j.clientId = c->id();
    j.value = variantToStr(doc["value"]);
    enqueue(std::move(j));
    return;
  }
}

void WebConsole::enqueue(Job&& j) {
  std::lock_guard<std::mutex> lk(gJobsMx);
  jobs_.push_back(std::move(j));
}

bool WebConsole::authOkWs(JsonVariantConst msg) const {
  if (!cfg_.authToken) return true;
  const char* tok = msg["token"] | "";
  return strcmp(tok, cfg_.authToken) == 0;
}
bool WebConsole::authOkHttp(AsyncWebServerRequest* req) const {
  if (!cfg_.authToken) return true;
  if (req->hasParam("token")) return req->getParam("token")->value() == cfg_.authToken;
  if (req->hasHeader("X-Auth-Token")) return req->getHeader("X-Auth-Token")->value() == cfg_.authToken;
  return false;
}

// ─── Manifest + persistence ──────────────────────────────────────────────────

String WebConsole::buildManifest() const {
  JsonDocument d;
  d["t"] = "hello";
  d["schema"] = 1;
  d["name"] = cfg_.deviceName;
  d["auth"] = (cfg_.authToken != nullptr);
  JsonArray cs = d["commands"].to<JsonArray>();
  for (auto& c : cmds_) {
    JsonObject o = cs.add<JsonObject>();
    o["name"] = c.name; o["help"] = c.help;
    o["pinned"] = c.pinned; o["confirm"] = c.confirm;
    if (c.argCount) serializeFields(o["args"].to<JsonArray>(), c.args, c.argCount);
  }
  JsonArray vs = d["vars"].to<JsonArray>();
  for (auto& v : vars_) {
    JsonObject o = vs.add<JsonObject>();
    o["name"] = v.name; o["type"] = typeName(v.type);
    o["help"] = v.help; o["persist"] = v.persist; o["value"] = v.get();
  }
  JsonArray fl = d["files"].to<JsonArray>();
  for (auto& f : files_) {
    JsonObject o = fl.add<JsonObject>();
    o["name"] = f.name; o["clear"] = f.allowClear;
  }
  JsonArray up = d["uploads"].to<JsonArray>();
  for (auto& u : uploads_) {
    JsonObject o = up.add<JsonObject>();
    o["name"] = u.name; o["help"] = u.help;
  }
  JsonArray co = d["collections"].to<JsonArray>();
  for (auto& c : colls_) {
    JsonObject o = co.add<JsonObject>();
    o["name"] = c.name; o["help"] = c.help; o["max"] = c.maxRecords;
    serializeFields(o["fields"].to<JsonArray>(), c.fields, c.fieldCount);
  }
  JsonArray pg = d["pages"].to<JsonArray>();
  for (auto& p : pages_) {
    JsonObject o = pg.add<JsonObject>();
    o["label"] = p.label; o["path"] = p.path;
  }
  String out; serializeJson(d, out);
  return out;
}

void WebConsole::loadPersistedVars() {
  Preferences p;
  if (!p.begin(cfg_.nvsNamespace, /*readOnly=*/true)) return;
  for (auto& v : vars_)
    if (v.persist && p.isKey(v.name)) v.set(p.getString(v.name, ""));
  p.end();
}

void WebConsole::persistVar(const Var& v, const String& value) {
  Preferences p;
  if (!p.begin(cfg_.nvsNamespace, /*readOnly=*/false)) return;
  p.putString(v.name, value);
  p.end();
}

// ─── Introspection ───────────────────────────────────────────────────────────

IPAddress WebConsole::ip() const {
  return cfg_.apSsid ? WiFi.softAPIP() : WiFi.localIP();
}
size_t WebConsole::clientCount() const {
  return ws_ ? ws_->count() : 0;
}

AsyncWebServer& WebConsole::server() { return *server_; }

bool WebConsole::authorized(AsyncWebServerRequest* req) const { return authOkHttp(req); }

void WebConsole::addPage(const char* label, const char* path) {
  pages_.push_back(NavPage{label, path});
}

String WebConsole::pageShell(const char* title, const String& body) const {
  String nav = F("<a class=\"navbtn\" href=\"/\">Home</a>");
  for (auto& pg : pages_) {
    nav += F("<a class=\"navbtn\" href=\"");
    nav += pg.path; nav += F("\">"); nav += pg.label; nav += F("</a>");
  }
  String out;
  out.reserve(body.length() + sizeof(WEBCONSOLE_CSS) + nav.length() + 384);
  out += F("<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>");
  out += title;
  out += F("</title><style>");
  out += FPSTR(WEBCONSOLE_CSS);
  out += F("</style></head><body><header><b class=\"brand\">");
  out += (cfg_.deviceName ? cfg_.deviceName : "Web Console");
  out += F("</b><nav class=\"nav\">");
  out += nav;
  out += F("</nav></header><main>");
  out += body;
  out += F("</main>"
           "<script>document.querySelectorAll('.navbtn').forEach(a=>{"
           "if(a.getAttribute('href')===location.pathname)a.classList.add('active')});</script>"
           "</body></html>");
  return out;
}

}  // namespace jelly::webconsole
