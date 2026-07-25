# WebConsole: a drop-in async web console for ESP32

A reusable library you drop into any ESP32 project to get, over WiFi:

- a live log stream (view in a browser or `curl` it down, no serial cable),
- a typed command console, a web serial line you type commands into, with
  optional buttons pinned for the critical controls,
- a variable registry for reading/writing runtime tunables (with optional
  persistence across reboots),

…behind one consistent interface and wire schema you never rebuild per
project. Built on [ESP32Async](https://github.com/ESP32Async)
(ESPAsyncWebServer + AsyncTCP) so it also shares a base with the wider async
ecosystem (ElegantOTA, file managers, etc.).

## Why this exists (the one idea)

The thing that normally forces you to rebuild a web UI for every firmware is a
hardcoded page. WebConsole avoids that by making the device self-describing: it
advertises a manifest of its commands and variables over a WebSocket, and a
single embedded browser page builds itself from that manifest: log pane, typed
command console, type-aware variable table.

> Adding a feature to a project is "register a command or a variable." The
> transport and the UI never change. That is the whole point.

```
   firmware                         one generic client (embedded HTML)
 ┌───────────────┐   manifest      ┌──────────────────────────────┐
 │ onCommand(…)  │ ───────────────▶│ typed console + pinned buttons│
 │ bindVar(…)    │   (WebSocket)   │ renders typed variable table │
 │ log(…)        │ ───log stream──▶│ live log pane                │
 └───────────────┘ ◀──cmd/var_set──│ user actions                 │
                                    └──────────────────────────────┘
```

## Footprint

Measured against the sync `WebServer` baseline by compiling the included
example. On 8/16 MB parts this is rounding error; RAM is the real constraint,
since each WebSocket client holds heap for TCP + framing. Cap concurrent
clients on no-PSRAM boards.

| | Flash | RAM |
|---|---|---|
| ESPAsyncWebServer + AsyncTCP + WebConsole + UI | ~55–70 KB | a few KB per WS client + your log ring |

WiFi is almost certainly already linked in your project (scanning, STA, etc.),
so the SoftAP itself costs ~nothing on top.

## Drop-in instructions

**1. Add the dependencies** to your project's `platformio.ini`. The
[pioarduino](https://github.com/pioarduino/platform-espressif32) platform (Arduino-ESP32
3.x / IDF 5.x) is required; the original `me-no-dev` async repos do not
build on it. Use the `ESP32Async` forks, pinned:

```ini
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = your-esp32-board
framework = arduino
lib_deps =
  ESP32Async/ESPAsyncWebServer @ ^3.11.2
  ESP32Async/AsyncTCP @ ^3.4.10
  bblanchon/ArduinoJson @ ^7
```

**2. Make the library visible.** Either copy this whole `esp_webserver/` folder
into your project's `lib/`, or point at it in place:

```ini
lib_extra_dirs = /path/to/jellybeans        ; the dir that contains esp_webserver/
```

**3. Wire it up** (register before `begin()`; call `tick()` every loop):

```cpp
#include <WebConsole.h>
using jelly::webconsole::WebConsole;

WebConsole console;
float gain = 1.0f;

void setup() {
  console.onCommand("reboot", "restart the device",
                    [](JsonVariantConst){ ESP.restart(); return String("ok"); });
  console.bindVar("gain", &gain, "signal gain multiplier", /*persist=*/true);

  WebConsole::Config cfg;
  cfg.apSsid = "MyDevice-Setup";   // SoftAP. Omit to run on your existing STA WiFi.
  cfg.deviceName = "mydevice";     // also mDNS → http://mydevice.local/
  console.begin(cfg);
}

void loop() {
  console.tick();
  console.logf("temp=%d", readSensor());   // streams live to the page
}
```

Browse to `http://192.168.4.1/` (SoftAP) or `http://mydevice.local/` (STA). A
complete, compilable sketch is in [`examples/basic/`](examples/basic).

## API

Register everything before `begin()`.

| Call | Purpose |
|---|---|
| `onCommand(name, help, fn [, opts])` | Register a command. `fn` is `String(JsonVariantConst args)`, runs in `loop()` context, returns a short result echoed to clients. `opts` (a `CommandOpts`) optionally adds a typed arg schema, pins it as a button, and/or requires a confirm. See [The command console](#the-command-console). |
| `bindVar<T>(name, &var, help, persist)` | Expose a scalar (`bool`/`int`/`long`/`float`/`double`/`String`) for live read/write. `persist=true` mirrors it in NVS across reboots. |
| `addVar(name, type, help, persist, getter, setter)` | Escape hatch for computed/derived variables (no backing pointer). |
| `addFile(name, path, contentType, allowClear)` | Expose a file on `cfg.fs` for download (and optional erase). For domain artifacts written elsewhere: logs, CSVs, exports. Appears in the manifest. |
| `onUpload(name, path, onDone, help)` | Accept a multipart upload into `path` on `cfg.fs`; `onDone(path)` validates/ingests it in `loop()` and returns a status. Appears in the manifest as a file picker. |
| `collection(name, path, fields, count, opts)` | A schema-driven list of structured records backed by a JSON file on `cfg.fs`. Renders a table + add/edit/delete form; validates in `loop()`. The untethered structured-data-entry primitive. |
| `log(str)` / `logf(fmt, …)` | Emit a log line. Thread-safe; buffered and flushed from `tick()`. |
| `io()` | A `Print&` sink you use like `Serial`; each `\n`-terminated line becomes a log entry. |
| `begin(cfg)` | Start the server (loads persisted vars, publishes the manifest). |
| `tick()` | Call every `loop()`: runs queued work, flushes logs, prunes clients. |
| `stop()` | Tear down. |

### `Config`

| Field | Default | Notes |
|---|---|---|
| `apSsid` / `apPassword` | `nullptr` | Non-null `apSsid` starts a SoftAP (WPA2 if password ≥ 8 chars). Omit to attach to WiFi you already connected (STA). Restores `WIFI_STA` on `stop()` so host scanning resumes. |
| `apIp` / `apNetmask` | `0.0.0.0` / `255.255.255.0` | SoftAP IP + gateway. Default → ESP's `192.168.4.1`; set a distinctive subnet (e.g. `10.99.7.1`) to avoid clashing when a client is also on a 192.168/10.0.0 network. |
| `port` | `80` | |
| `deviceName` | `"esp-device"` | Also the mDNS hostname. |
| `authToken` | `nullptr` | Non-null → `cmd`/`var_set` require this shared token. Read-only endpoints stay open. Set this whenever the console is reachable off a trusted SoftAP. |
| `fs` | `nullptr` | A filesystem you already mount (`SPIFFS`/`LittleFS`/`SD`) → the log is also written to a rotating, `curl`-able file. Null → live RAM tail only. WebConsole never mounts an FS for you. |
| `logPath` / `logMaxBytes` | `/console.log` / 128 KB | Rotates to `<logPath>.1` past the cap. |
| `ramLines` | `200` | Live tail depth held in RAM. |
| `nvsNamespace` | `"webcon"` | Preferences namespace for persisted vars. |

## Wire protocol (schema v1)

Everything is a JSON object with a `t` (type) field. This is the contract a
custom client (or a script) codes against.

Device → client:

| `t` | Payload |
|---|---|
| `hello` | `{schema, name, auth, commands:[{name,help,pinned,confirm,args?:[{name,label,type,options,required}]}], vars:[…], files:[{name,clear}], uploads:[{name,help}], collections:[{name,help,max,fields:[{name,label,type,options,required}]}], pages:[{label,path}]}`. Sent on connect and on request. |
| `log` | `{seq, ms, line}` |
| `cmd_result` | `{id, name, ok, result}` |
| `upload_result` | `{name, ok, result}`. Sent after an upload is validated in `loop()`. |
| `collection_changed` | `{name, ok, result}`. Sent after a create/edit/delete; clients re-fetch that collection. |
| `var` | `{name, value}`. Broadcast when a variable changes. |
| `event` | `{name, data}`. Reserved for device-initiated notifications. |

Client → device:

| `t` | Payload |
|---|---|
| `hello` | (request the manifest) |
| `cmd` | `{id, name, args:{…}, token?}` |
| `var_set` | `{name, value, token?}` |

### HTTP mirror (for `curl`/scripts)

| Method + path | Does |
|---|---|
| `GET /` | The self-contained UI page. |
| `GET /api/manifest` | The manifest as JSON. |
| `GET /api/log[?since=N][?file=1]` | Live RAM tail as text; `since` filters by seq; `file=1` streams the persisted log file. |
| `GET /api/vars` | All variables + current values. |
| `GET /api/file?name=<n>` | Download a registered file (attachment). |
| `POST /api/file/clear` | `name` (+ `token`). Erases a clearable registered file. |
| `POST /api/upload?name=<n>` | Multipart upload into the slot's file (+ `token`); validated in `loop()`, result over WS. |
| `GET /api/collection?name=<n>` | The collection's records as a JSON array. |
| `POST /api/collection/create` · `/edit` · `/delete` | `?name=<coll>` (and `&index=` for edit/delete) in the query; record fields in the POST body. Separate namespaces, so a field named `name`/`index` doesn't collide with the identifier. Applied in `loop()`, result over WS. |
| `POST /api/var` | `name`, `value` (+ `token`). Applied in `loop()`; returns `202`. |
| `POST /api/cmd` | `name`, optional `args` (JSON string), (+ `token`). Fire-and-forget; the result comes back over the WebSocket, not the HTTP response. |

```bash
curl http://mydevice.local/api/manifest
curl "http://mydevice.local/api/log?since=0"
curl -d "name=reboot" -H "X-Auth-Token: secret" http://mydevice.local/api/cmd
curl -d "name=gain" -d "value=2.5" http://mydevice.local/api/var
```

## The command console

The home page's Console card is a web serial line: type a command name and
press Enter. Commands are not enumerated as buttons by default, which keeps
the page usable when a project registers dozens of them. Discoverability lives in
the console itself:

- `help` lists every registered command (and its arg schema); `help <name>`
  details one. `clear` wipes the log pane.
- Tab completes the command name; ↑/↓ walk your input history.

> `help`, `?`, and `clear` are handled in the browser, so a device command with
> one of those exact names can't be reached by typing it (pin it as a button if
> you need it). Any other name is fine.

A bare command sends no args (`ping`). To pass args, either:

- Typed, human-readable: declare an arg schema and type `led on=true` (or
  positional `led true`). The console coerces each token to the field's real type
  and sends a structured object. Without a schema the console has no types to
  coerce to, so this form is rejected (see why below).
- Raw JSON: `led {"on":true}` is always accepted, schema or not.

Either way the wire message is identical: `{"t":"cmd","name":"led","args":{"on":true}}`.
Handlers keep reading typed fields (`a["on"] | false`) exactly as before.

> Why a schema is required for `key=val` args: the manifest carries no
> per-argument types on its own, so `led on=true` could only be sent as the
> string `"true"`, and a handler reading `a["on"] | false` would silently see a
> non-bool and treat it as false, doing the opposite of what was typed. The
> schema is what makes untethered typed entry safe.

To pin critical controls, set `opts.pinned = true` to render a command as a
button in the Controls card (with a typed mini-form when it has an arg
schema). Set `opts.confirm = true` to make the browser confirm first, for
destructive controls like `reboot`. Everything else stays a typed verb.

```cpp
static const jelly::webconsole::Field kLedArgs[] = {
  {"on", "On", jelly::webconsole::FieldType::Bool, nullptr, /*required=*/true},
};
jelly::webconsole::CommandOpts led;
led.args = kLedArgs;
led.argCount = sizeof(kLedArgs) / sizeof(kLedArgs[0]);
led.pinned = true;                 // → a button with an On checkbox
console.onCommand("led", "turn the LED on/off", [](JsonVariantConst a) {
  digitalWrite(LED_BUILTIN, a["on"] | false);
  return String(a["on"] | false ? "on" : "off");
}, led);
```

The arg schema reuses the same `Field`/`FieldType` machinery as record
collections (`Text`, `Number`, `Bool`, `Enum`), and the `Field` array must be
static (stored by pointer), same as collections.

> Default-UI note: earlier versions rendered every command as a button.
> The console + pinned model is now the default: nothing is a button unless you
> pin it. Add `opts.pinned = true` to restore a button for the commands that want
> one.

## Record collections

Structured data entry: the foundational untethered function for devices with a
limited on-board control set. Declare a field schema; the bean renders a table
with a shared create/edit form, validates each change in `loop()`, and persists
records as a JSON file on `cfg.fs`.

```cpp
static const jelly::webconsole::Field kPeopleFields[] = {
  {"name", "Name", FieldType::Text,   nullptr,            true},
  {"age",  "Age",  FieldType::Number, nullptr,            false},
  {"role", "Role", FieldType::Enum,   "guest,member,vip", false},
  {"vip",  "VIP",  FieldType::Bool,   nullptr,            false},
};
jelly::webconsole::CollectionOpts opts;
opts.rootKey = "people";       // file is {"people":[ … ]}
opts.maxRecords = 32;
opts.validate = [](JsonObject rec) -> String {   // runs in loop(); "" accepts
  if (!rec["name"].is<const char*>()) return "name required";
  return "";                                       // mutate rec to normalize
};
console.collection("people", "/people.json", kPeopleFields,
                   sizeof(kPeopleFields)/sizeof(kPeopleFields[0]), opts);
```

Field types: `Text`, `Number`, `Bool`, `Enum` (comma-separated `options`). The
`fields` array must be static (stored by pointer). Records are index-addressed
(matching table row order) for edit/delete. `opts.onChanged` fires in `loop()`
after a successful write. Use it to refresh in-memory state derived from the file
(`validate` runs before the write, so it can't).

One backing file, three access paths: point `addFile()` and `onUpload()` at
the same JSON file a collection backs to get export (download), bulk import
(replace-all upload), and record-level edit (the collection) over one consistent
schema.

## Concurrency contract (read before extending)

ESPAsyncWebServer callbacks run on the async TCP task, not `loop()`. To keep
your handlers safe by default, WebConsole never runs your code on that task:
inbound commands/var-sets are deep-copied into an owned queue and executed from
`tick()` in the loop context. So:

- Your command handlers and variable setters may touch hardware freely; they
  run in `loop()`.
- All WebSocket sends (log, results, var broadcasts) also happen from `tick()`,
  so there are no stale-client-pointer hazards; results are broadcast, not aimed
  at a cached pointer.
- Command args are deep-copied out of the transient frame buffer before
  queuing, so they're safe to read later.
- The log ring is mutex-guarded, so `log()`/`logf()` are safe from any task.

If you extend the library, preserve this: never call a user handler from a WS
or HTTP callback. Enqueue it.

> One asymmetry: variable getters are the exception; they run in the async
> task (a connecting client pulls the manifest; `GET /api/vars` and `/api/manifest`
> read live values there). `bindVar`'s pointer read is safe; if you use the
> `addVar` escape hatch, keep the getter cheap and pure, with no hardware access
> or shared-state mutation that only `loop()` context makes safe.

## Extending

- **New command / variable:** just `onCommand` / `bindVar`. The UI picks it up
  from the manifest with zero front-end changes.
- **New scalar type:** add three lines to the `detail::` block in `WebConsole.h`
  (`varTypeOf`, `toStr`, `fromStr`) and a case in `typeName()`.
- **Device-initiated events:** the `event` message type is reserved and rendered
  by the UI; add a small `emitEvent(name, data)` helper the same way `log()`
  broadcasts.
- **Editing the UI:** `WebConsoleUI.h` is a plain editable raw-string page with
  no build/minify step. Keep it self-contained (no external assets) so it works
  offline on a SoftAP.
- **Escape hatch:** for a genuinely one-off view the schema can't model (a bespoke
  diagnostic report, say), grab the raw handle with `server()` after `begin()` and
  add your own route; gate it with `authorized(req)` to match the built-in auth.
  Wrap your content in `pageShell(title, body)` so it inherits the console's theme +
  top-bar, and call `addPage(label, path)` (before `begin()`) to put a button for it
  in every page's nav. Reserve this for the long tail; foundational needs belong in
  a first-class feature (command/var/collection/file), not here.

## Gotchas

- **NVS keys:** ≤ 15 chars, so keep persisted variable names short.
- **Name and help strings:** stored by pointer, not copied. Pass string
  literals or other storage that outlives the console; a `String`'s `c_str()`
  or a stack buffer will dangle.
- **Persistence:** uses `Preferences` (NVS), not the filesystem. Only the log
  file uses `cfg.fs`.
- **Single instance per sketch.** The cross-task queues are guarded by
  file-static mutexes; two instances still work but serialize against each other.
- **`POST /api/cmd`:** doesn't return the command result (single HTTP round-trip,
  loop-context execution). Use the WebSocket for interactive results.
- **mDNS:** `<deviceName>.local` needs an mDNS client. macOS/iOS/Windows 10+/Linux-with-avahi
  resolve it; Android does not resolve `.local` in the browser, so those clients
  must connect by IP.

## Building the example

`examples/basic` is a complete sketch wired to the library in place
(`lib_extra_dirs = ${PROJECT_DIR}/../..`), so it builds the working tree rather
than an installed copy:

```bash
cd examples/basic
pio run                        # compile
pio run -t upload -t monitor   # flash + serial monitor
```

It targets an ESP32-S3 with PSRAM (`esp32-s3-devkitc1-n16r8`). On boards without
PSRAM, RAM is the binding constraint, so cap concurrent WebSocket clients.
