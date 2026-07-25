// Copyright (C) 2026 Lone Crow Design, LLC
// Licensed under GPLv3 — see LICENSE
//
// WebConsole — a drop-in, schema-driven web console for ESP32 projects.
//
// The device advertises a *manifest* (its commands + variables) over one
// WebSocket. A single self-contained browser page reads that manifest and
// builds itself: a live log pane, a command palette, and a type-aware
// variable table. New projects register commands/variables — the transport
// and the UI never change. That is the whole point: you never hand-build a
// per-project interface or schema again.
//
// Everything is reachable two ways so scripts and browsers both work:
//   • WebSocket /ws  — live, bidirectional (the rich interactive path)
//   • plain HTTP     — curl-friendly (pull logs, read manifest/vars, fire
//                      commands, set vars)
//
// Concurrency model (read this before writing handlers):
//   ESPAsyncWebServer callbacks run in the async TCP task, NOT loop(). To keep
//   your handlers safe by default, WebConsole NEVER runs a command handler or
//   a variable setter from that task. Inbound requests are copied into an
//   owned queue; tick() (which you call from loop()) drains it and runs your
//   code in the loop context. So your handlers may touch hardware freely.
//
// Typical wiring:
//   WebConsole console;
//   void setup() {
//     console.onCommand("reboot", "restart the device",
//                       [](JsonVariantConst){ ESP.restart(); return String("ok"); });
//     console.bindVar("gain", &gain, "signal gain multiplier", /*persist=*/true);
//     WebConsole::Config cfg;
//     cfg.apSsid = "MyDevice-Setup";   // or leave null if already on WiFi (STA)
//     cfg.deviceName = "mydevice";
//     console.begin(cfg);
//   }
//   void loop() { console.tick(); /* ... */ }
//
// Register commands/vars BEFORE begin() — begin() loads persisted variable
// values and publishes the manifest.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <IPAddress.h>
#include <functional>
#include <vector>

// Forward-declared so the heavy ESPAsync headers stay out of your build unless
// WebConsole.cpp needs them. Instances are heap-allocated in begin().
class AsyncWebServer;
class AsyncWebSocket;
class AsyncWebSocketClient;
class AsyncWebServerRequest;

namespace jelly::webconsole {

enum class VarType { Bool, Int, Float, String };

// ─── Collection field schema ────────────────────────────────────────────────
// A record collection is a list of structured records persisted as a JSON file
// on cfg.fs. You declare the field schema; the bean renders a table + add/edit
// form, validates on submit, and composes the fields into a consistent JSON
// record. This is the untethered structured-data-entry primitive — foundational
// for devices with a limited on-board control set.
enum class FieldType { Text, Number, Bool, Enum };
struct Field {
  const char* name;     // JSON key and form field name
  const char* label;    // UI label (nullptr → name)
  FieldType   type;
  const char* options;  // Enum only: comma-separated choices ("open,wep,wpa2"); else nullptr
  bool        required;
};
// Runs in loop() context on the assembled record before it is committed. Mutate
// `rec` in place to normalize; return "" to accept, or an error string to reject
// (nothing is written and the client sees the error).
using RecordHook = std::function<String(JsonObject rec)>;
struct CollectionOpts {
  const char* rootKey = "records";  // file is {"<rootKey>":[ … ]}
  size_t      maxRecords = 32;
  const char* help = nullptr;
  RecordHook  validate = nullptr;
  // Fires in loop() context after a successful create/edit/delete has been
  // written — for the host to refresh in-memory state derived from the file.
  std::function<void()> onChanged = nullptr;
};

// ─── Command options ─────────────────────────────────────────────────────────
// Optional extras for onCommand(). Everything here is opt-in; a command with
// default opts is a bare verb typed by name in the console.
//   • args   — a typed argument schema (reusing Field). The console has no arg
//              types otherwise, so untyped console text like `led on=true` could
//              only be sent as the *string* "true", which handlers reading
//              `a["on"] | false` would silently treat as false. Declaring the
//              schema lets the console parse human-readable input and still put a
//              correctly-typed object on the wire ({"on":true}). Enum `options`
//              are validated; the array must outlive the console (static storage).
//   • pinned — render this command as a button (with a typed mini-form when it
//              has args) in the "Controls" card. Unpinned commands are invoked by
//              name from the console input line. Default false → the out-of-the-box
//              UI is a pure typed console; pin only the critical controls.
//   • confirm — ask the browser to confirm before running (destructive controls).
struct CommandOpts {
  const Field* args = nullptr;
  size_t       argCount = 0;
  bool         pinned = false;
  bool         confirm = false;
};

// ─── Type plumbing for bindVar<T>() ─────────────────────────────────────────
// One public registration pattern (bindVar) covers every scalar type; these
// map a C++ type to a wire VarType and to/from its string form. Add a type by
// adding three lines here — nothing else changes.
namespace detail {
template <class T> VarType varTypeOf();
template <> inline VarType varTypeOf<bool>()   { return VarType::Bool; }
template <> inline VarType varTypeOf<int>()    { return VarType::Int; }
template <> inline VarType varTypeOf<long>()   { return VarType::Int; }
template <> inline VarType varTypeOf<float>()  { return VarType::Float; }
template <> inline VarType varTypeOf<double>() { return VarType::Float; }
template <> inline VarType varTypeOf<String>() { return VarType::String; }

inline String toStr(bool v)          { return v ? "1" : "0"; }
inline String toStr(int v)           { return String(v); }
inline String toStr(long v)          { return String(v); }
inline String toStr(float v)         { return String(v, 4); }
inline String toStr(double v)        { return String(v, 6); }
inline String toStr(const String& v) { return v; }

inline void fromStr(const String& s, bool& v)   { v = (s == "1" || s == "true" || s == "on"); }
inline void fromStr(const String& s, int& v)    { v = (int)s.toInt(); }
inline void fromStr(const String& s, long& v)   { v = s.toInt(); }
inline void fromStr(const String& s, float& v)  { v = s.toFloat(); }
inline void fromStr(const String& s, double& v) { v = s.toDouble(); }
inline void fromStr(const String& s, String& v) { v = s; }
}  // namespace detail

class WebConsole {
 public:
  struct Config {
    // SoftAP: non-null apSsid brings up an access point (WPA2 if apPassword is
    // 8+ chars, open otherwise). Leave apSsid null to run on an existing WiFi
    // connection the caller already established (STA + mDNS use case) — begin()
    // then never touches WiFi mode.
    const char* apSsid = nullptr;
    const char* apPassword = nullptr;
    // SoftAP IP (used as address + gateway). Leave default (0.0.0.0) for the ESP
    // default 192.168.4.1; set a distinctive subnet (e.g. 10.99.7.1) to avoid
    // clashing when a client is also on a 192.168/10.0.0 network.
    IPAddress apIp = IPAddress((uint32_t)0);
    IPAddress apNetmask = IPAddress(255, 255, 255, 0);

    uint16_t port = 80;
    const char* deviceName = "esp-device";  // also the mDNS name: <name>.local

    // Non-null → cmd and var_set require this shared token (WS: a "token" field;
    // HTTP: an X-Auth-Token header or ?token= param). Read-only endpoints (log,
    // manifest, var GET) are always open. Set this whenever the console is
    // reachable on anything but a trusted SoftAP.
    const char* authToken = nullptr;

    // Non-null fs → the log is also appended to a rotating file you can curl
    // (GET /api/log?file=1). Null → live RAM tail only. Pass whatever filesystem
    // the host project already mounts (SPIFFS, LittleFS, SD) — WebConsole does
    // not mount one for you.
    fs::FS* fs = nullptr;
    const char* logPath = "/console.log";
    size_t logMaxBytes = 128 * 1024;  // roll to <logPath>.1 past this size

    size_t ramLines = 200;             // live tail depth held in RAM
    const char* nvsNamespace = "webcon";  // Preferences namespace for persisted vars
  };

  // A command handler runs in loop() context (see the concurrency note above).
  // It receives the caller-supplied args object (may be null) and returns a
  // short result string echoed back to clients.
  using CmdHandler = std::function<String(JsonVariantConst args)>;
  using VarGetter = std::function<String()>;
  using VarSetter = std::function<void(const String&)>;
  // Runs in loop() context after an upload finishes writing `path`; return a
  // short status (e.g. "loaded 4 targets" or "parse error: …") echoed to clients.
  using UploadDoneFn = std::function<String(const String& path)>;

  bool begin(const Config& cfg);
  void tick();  // call every loop(): drains the request queue, flushes logs,
                // prunes dead WS clients. Cheap when idle.
  void stop();

  // ─── Logging ──────────────────────────────────────────────────────────────
  // Thread-safe; may be called from any task. Lines are buffered and pushed to
  // WS clients + the log file from tick(), so logging never blocks on the network.
  void log(const String& line);
  void logf(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
  Print& io();  // a Serial-like sink; each '\n'-terminated line becomes a log entry

  // ─── Command registry ───────────────────────────────────────────────────────
  // name/help must be static strings (stored by pointer). Register before begin().
  // opts adds an argument schema, pins the command as a button, and/or requires a
  // confirm — see CommandOpts. A command with no opts is a bare typed verb.
  void onCommand(const char* name, const char* help, CmdHandler fn,
                 const CommandOpts& opts = {});

  // ─── Variable registry ──────────────────────────────────────────────────────
  // bindVar is the one pattern for scalars: bind a pointer and WebConsole builds
  // the getter/setter for you. persist=true mirrors the value in NVS across
  // reboots (loaded in begin(), written on change). NVS keys are ≤15 chars, so
  // keep persisted names short. Register before begin().
  template <class T>
  void bindVar(const char* name, T* p, const char* help = nullptr, bool persist = false) {
    addVar(name, detail::varTypeOf<T>(), help, persist,
           [p] { return detail::toStr(*p); },
           [p](const String& v) { detail::fromStr(v, *p); });
  }

  // Escape hatch for computed / derived variables (no backing pointer).
  // NOTE ON CONTEXT: unlike setters and command handlers, variable *getters* are
  // read from the async TCP task (a client connecting pulls the manifest; GET
  // /api/vars and /api/manifest read live values there). The deferred-to-loop()
  // contract does NOT cover them — keep getters cheap and pure (no hardware, no
  // shared-state mutation). bindVar's pointer read is fine; a computed getter here
  // must not do work that only loop() context makes safe.
  void addVar(const char* name, VarType type, const char* help, bool persist,
              VarGetter get, VarSetter set);

  // ─── File registry ──────────────────────────────────────────────────────────
  // Expose an existing file on cfg.fs for download (GET /api/file?name=<name>)
  // and, if allowClear, erasure (POST /api/file/clear name=<name>). Appears in
  // the manifest so the UI shows a download (and Erase) control. Use it for
  // domain artifacts the device writes elsewhere — run logs, capture CSVs,
  // exports. Requires cfg.fs. Register before begin().
  void addFile(const char* name, const char* path,
               const char* contentType = "application/octet-stream", bool allowClear = false);

  // ─── Upload targets ─────────────────────────────────────────────────────────
  // Register an upload slot. A multipart POST to /api/upload?name=<name> streams
  // straight to `path` on cfg.fs (replacing it), then `onDone(path)` runs in
  // loop() context to validate/ingest it and returns a status echoed to clients.
  // Appears in the manifest so the UI shows a file picker. Register before begin().
  void onUpload(const char* name, const char* path, UploadDoneFn onDone, const char* help = nullptr);

  // ─── Record collections (structured data entry) ─────────────────────────────
  // Declare a schema-driven list of records backed by a JSON file on cfg.fs. The
  // UI renders a table with add/edit/delete; create and edit share one form. The
  // same backing file is what upload() bulk-replaces and addFile() downloads, so
  // a collection unifies structured entry, bulk import, and export. `fields` must
  // point to storage that outlives the console (a static array). Register before begin().
  void collection(const char* name, const char* path,
                  const Field* fields, size_t fieldCount,
                  const CollectionOpts& opts = {});

  IPAddress ip() const;        // AP IP when in SoftAP mode, else the STA IP
  size_t clientCount() const;  // connected WebSocket clients

  // ─── Escape hatch ───────────────────────────────────────────────────────────
  // Raw server handle for the long tail — bespoke routes the declarative surface
  // can't model (e.g. a rich diagnostic report). Valid after begin(); register
  // routes right after it. Reserve this for genuinely one-off views; foundational
  // needs should be a first-class feature (command/var/collection/file), not this.
  AsyncWebServer& server();
  // True if the request satisfies cfg.authToken (or none is set). Call this in
  // escape-hatch routes to match the built-in endpoints' auth.
  bool authorized(AsyncWebServerRequest* req) const;

  // Register a user-facing page so a button for it appears in every page's top
  // bar (alongside the built-in "Home"). Register before begin().
  void addPage(const char* label, const char* path);
  // Wrap body HTML in the console's shared <head> (theme/CSS) + top-bar nav, so an
  // escape-hatch page matches the home layout. `body` is your content (cards, etc.)
  // without <html>/<head>/<body> or a <header>. Returns a full HTML document.
  String pageShell(const char* title, const String& body) const;

 private:
  struct Cmd {
    const char* name; const char* help; CmdHandler fn;
    const Field* args; size_t argCount; bool pinned; bool confirm;
  };
  struct Var {
    const char* name; const char* help; VarType type; bool persist;
    VarGetter get; VarSetter set;
  };
  struct FileEntry { const char* name; const char* path; const char* contentType; bool allowClear; };
  struct NavPage { const char* label; const char* path; };
  struct UploadEntry { const char* name; const char* path; const char* help; UploadDoneFn onDone; };
  struct Collection {
    const char* name; const char* path; const char* rootKey; const char* help;
    const Field* fields; size_t fieldCount; size_t maxRecords;
    RecordHook validate; std::function<void()> onChanged;
  };

  // A deferred unit of work moved from the async task into loop().
  struct Job {
    enum Kind { Cmd, VarSet, Upload, Collection } kind;
    uint32_t reqId;     // caller correlation id for cmd_result
    int index;          // into cmds_ / vars_ / uploads_ / colls_
    JsonDocument args;  // owned deep copy of command args or an assembled record
    String value;       // new value for VarSet
    uint32_t clientId;  // 0 = not from a WS client (e.g. HTTP)
    int collOp = 0;     // Collection: 0=create 1=edit 2=delete
    int recIndex = -1;  // Collection: target record index for edit/delete
  };
  struct LogLine { uint32_t seq; uint32_t ms; String text; };

  // WS + HTTP plumbing (all run in the async task).
  void handleWsText(AsyncWebSocketClient* c, const uint8_t* data, size_t len);
  bool authOkWs(JsonVariantConst msg) const;
  bool authOkHttp(AsyncWebServerRequest* req) const;
  void enqueue(Job&& j);
  int findCmd(const char* name) const;
  int findVar(const char* name) const;
  int findFile(const char* name) const;
  int findUpload(const char* name) const;
  int findColl(const char* name) const;
  // Assemble a record from schema + request params into `rec` (async ctx, owned storage).
  void assembleRecord(const Collection& c, AsyncWebServerRequest* req, JsonObject rec) const;

  // Message builders.
  String buildManifest() const;
  void   loadPersistedVars();
  void   persistVar(const Var& v, const String& value);

  Config cfg_{};
  AsyncWebServer* server_ = nullptr;
  AsyncWebSocket* ws_ = nullptr;
  bool started_ = false;

  std::vector<Cmd> cmds_;
  std::vector<Var> vars_;
  std::vector<FileEntry> files_;
  std::vector<UploadEntry> uploads_;
  std::vector<Collection> colls_;
  std::vector<NavPage> pages_;
  fs::File uploadFile_;  // single in-flight upload (one AP client assumed)
  int uploadIdx_ = -1;

  // Cross-task state — guarded by the mutexes in WebConsole.cpp.
  std::vector<Job> jobs_;
  std::vector<LogLine> logRing_;
  uint32_t logSeq_ = 0;
  uint32_t lastPushedSeq_ = 0;

  class LineSink;
  LineSink* sink_ = nullptr;
};

}  // namespace jelly::webconsole
