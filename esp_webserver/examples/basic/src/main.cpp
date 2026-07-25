// Copyright (C) 2026 Lone Crow Design, LLC
// Licensed under GPLv3 — see LICENSE
//
// Smallest useful WebConsole setup: a SoftAP, one command, two variables (one
// persisted), and a heartbeat that streams to the log. Join "WebConsole-Demo"
// (password below), browse to http://192.168.4.1/ — or curl it:
//   curl http://192.168.4.1/api/manifest
//   curl "http://192.168.4.1/api/log"
//   curl -d "name=ping" http://192.168.4.1/api/cmd
//   curl -d "name=gain" -d "value=2.5" http://192.168.4.1/api/var

#include <Arduino.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>  // for the escape-hatch page route
#include <WebConsole.h>

using jelly::webconsole::WebConsole;

static WebConsole console;

// Variables the console can read/write live.
static float gain = 1.0f;          // persisted across reboots
static bool  ledOn = false;

void setup() {
  Serial.begin(115200);
  SPIFFS.begin(true);  // for the persisted log file (optional)

  // A bare verb: no args, no button — just type `ping` in the console.
  console.onCommand("ping", "reply pong", [](JsonVariantConst) {
    return String("pong");
  });

  // A typed, pinned command. The arg schema lets the console accept `led on=true`
  // (coerced to a real bool) and gives the pinned button a checkbox mini-form.
  static const jelly::webconsole::Field kLedArgs[] = {
    {"on", "On", jelly::webconsole::FieldType::Bool, nullptr, /*required=*/true},
  };
  jelly::webconsole::CommandOpts ledOpts;
  ledOpts.args = kLedArgs;
  ledOpts.argCount = sizeof(kLedArgs) / sizeof(kLedArgs[0]);
  ledOpts.pinned = true;             // critical control → button in the Controls card
  console.onCommand("led", "turn the LED on/off", [](JsonVariantConst a) {
    ledOn = a["on"] | false;
    digitalWrite(LED_BUILTIN, ledOn);
    return String(ledOn ? "on" : "off");
  }, ledOpts);

  // A pinned, destructive control: the browser confirms before it runs.
  jelly::webconsole::CommandOpts rebootOpts;
  rebootOpts.pinned = true;
  rebootOpts.confirm = true;
  console.onCommand("reboot", "restart the device", [](JsonVariantConst) {
    ESP.restart();
    return String("ok");
  }, rebootOpts);

  console.bindVar("gain", &gain, "signal gain multiplier", /*persist=*/true);
  console.bindVar("led", &ledOn, "LED state");

  // Expose the persisted log file for download + erase from the browser/curl.
  console.addFile("log", "/console.log", "text/plain", /*allowClear=*/true);

  // Accept an uploaded notes file; validate/ingest it in loop() context.
  console.onUpload("notes", "/notes.txt", [](const String& path) {
    File f = SPIFFS.open(path, FILE_READ);
    size_t n = f ? f.size() : 0;
    if (f) f.close();
    return String("received ") + n + " bytes";
  }, "any text file");

  // Structured data entry: a record collection backed by /people.json.
  static const jelly::webconsole::Field kPeopleFields[] = {
    {"name", "Name",  jelly::webconsole::FieldType::Text,   nullptr,           true},
    {"age",  "Age",   jelly::webconsole::FieldType::Number, nullptr,           false},
    {"role", "Role",  jelly::webconsole::FieldType::Enum,   "guest,member,vip", false},
    {"vip",  "VIP",   jelly::webconsole::FieldType::Bool,   nullptr,           false},
  };
  jelly::webconsole::CollectionOpts opts;
  opts.rootKey = "people";
  opts.maxRecords = 16;
  opts.help = "example roster";
  opts.validate = [](JsonObject rec) -> String {
    if (!rec["name"].is<const char*>()) return "name is required";
    return "";
  };
  console.collection("people", "/people.json", kPeopleFields,
                     sizeof(kPeopleFields) / sizeof(kPeopleFields[0]), opts);

  console.addPage("About", "/about");  // top-bar button → escape-hatch page below

  WebConsole::Config cfg;
  cfg.apSsid = "WebConsole-Demo";
  cfg.apPassword = "console123";     // 8+ chars → WPA2
  cfg.deviceName = "webconsole";      // also mDNS: webconsole.local
  cfg.fs = &SPIFFS;                   // enable the curl-able persisted log
  console.begin(cfg);

  // Escape-hatch page, wrapped in the console shell so it matches home + gets the
  // top bar. Reachable via the "About" nav button.
  console.server().on("/about", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", console.pageShell("About",
      "<section class=\"card\"><h2>About</h2>"
      "<p>A bespoke page rendered through <code>pageShell()</code> — same theme, "
      "same nav bar as the console home.</p></section>"));
  });

  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  console.tick();  // required every loop

  static uint32_t last = 0;
  if (millis() - last > 2000) {
    last = millis();
    console.logf("heartbeat up=%lus gain=%.2f", millis() / 1000, gain);
  }
}
