# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

A collection of "jellybeans": self-contained, drop-in PlatformIO libraries for ESP32 / embedded projects. Each bean lives in its own top-level subdirectory and is independently adoptable. There is no top-level build; the repo is a container, not an application.

Beans are consumed two ways: copied into a host project's `lib/`, or referenced in place with `lib_extra_dirs = /path/to/jellybeans`.

`design.md` holds the reasoning behind the shared conventions. Read it before adding a bean.

## Build and verify

There is no test suite and no top-level build. Each bean builds through its own example:

```bash
cd esp_webserver/examples/basic
pio run                        # compile
pio run -t upload -t monitor   # flash + serial monitor
```

The example's `platformio.ini` points at the bean via `lib_extra_dirs = ${PROJECT_DIR}/../..`, so it compiles the working tree, not an installed copy. It defines a single environment (`esp32-s3`, board `esp32-s3-devkitc1-n16r8`, built with `-DBOARD_HAS_PSRAM=1`). RAM is the binding constraint on no-PSRAM boards, where concurrent WebSocket clients need capping.

## Platform constraints

- The **pioarduino** `platform-espressif32` fork is required (Arduino-ESP32 3.x / IDF 5.x). Not the mainline platform.
- Async dependencies must be the **ESP32Async** forks (`ESPAsyncWebServer`, `AsyncTCP`). The original `me-no-dev` repos do not build here.
- C++17 is assumed (nested-namespace syntax). Untested under the Arduino IDE.
- Pin dependency versions in each bean's `library.json`.

## Shared conventions across beans

- **Namespace:** one umbrella `jelly`, each bean nested under it: `jelly::<bean>::…` (e.g. `jelly::webconsole::WebConsole`).
- **Globally unique names:** header filenames (`WebConsole.h`, not `Console.h`) and `library.json` names. The include path is a flat global namespace, so beans must not collide when a project adopts several.
- **Each bean ships** a buildable `examples/` sketch that compiles the library in place, and a README covering its API and wire contract.
- **Concurrency posture:** beans wrapping callback-driven libraries keep host handlers out of framework task contexts by default, rather than exporting the framework's threading model to every adopter. See below for how `esp_webserver` implements this.

## esp_webserver (WebConsole)

A schema-driven async web console. The architectural idea: the device is **self-describing**. It advertises a manifest of its commands, variables, files, uploads, and record collections over one WebSocket, and a single embedded browser page builds its entire UI from that manifest. Adding a feature is "register a command or a variable" — the transport and the UI never change. Nothing about the page is per-project.

Three files, each with a distinct role:

- `src/WebConsole.h` — the public API, the `Field`/`FieldType` schema types, and the `detail::` type-plumbing block. Forward-declares the ESPAsync types so those headers stay out of a host's build.
- `src/WebConsole.cpp` — server setup, WS/HTTP route handlers, the deferred-job queue, manifest construction, NVS persistence.
- `src/WebConsoleUI.h` — the entire browser client as two `PROGMEM` raw string literals (`WEBCONSOLE_CSS`, `WEBCONSOLE_BODY`). Editable in place, no build or minify step.

### The concurrency contract (the central invariant)

ESPAsyncWebServer callbacks run on the async TCP task, not `loop()`. WebConsole never runs host code on that task. Inbound commands, variable writes, uploads, and collection edits are deep-copied into an owned `Job` and pushed onto `jobs_`; `tick()` (called from the host's `loop()`) swaps the queue out under a mutex and executes them in loop context. All WebSocket sends happen from `tick()` too, so there are no stale-client-pointer hazards and results are broadcast rather than aimed at a cached pointer.

**If you extend this library, preserve it: never invoke a host handler from a WS or HTTP callback — enqueue it.**

One deliberate exception: variable *getters* run in the async task, because a connecting client pulls the manifest and `GET /api/vars` reads live values there. `bindVar`'s pointer read is safe; a computed `addVar` getter must stay cheap and pure.

### Other invariants

- **Registration happens before `begin()`.** `begin()` loads persisted variables and publishes the manifest.
- **`Field` arrays are stored by pointer** and must have static storage duration. This applies to both command argument schemas and collection field schemas — they share the same machinery.
- **`const char*` name/help arguments are stored by pointer too**, not copied.
- **Single instance per sketch.** `gJobsMx` and `gLogMx` in `WebConsole.cpp` are file-static, so multiple instances work but serialize against each other.
- **NVS keys are ≤ 15 chars**, so persisted variable names must stay short. Persistence uses `Preferences` (NVS); only the log file uses `cfg.fs`.
- **WebConsole never mounts a filesystem.** `cfg.fs` is whatever the host already mounted, and null is valid (RAM log tail only).
- **Command argument schemas exist for type safety, not decoration.** Without one, `led on=true` could only reach the wire as the string `"true"`, and a handler reading `a["on"] | false` would silently see a non-bool and do the opposite of what was typed. That is why untyped `key=val` input is rejected.

### Extension points

- New command or variable: `onCommand` / `bindVar`. The UI picks it up from the manifest with no front-end change.
- New scalar type: three lines in the `detail::` block of `WebConsole.h` (`varTypeOf`, `toStr`, `fromStr`) plus a case in `typeName()` in the `.cpp`.
- Bespoke routes the schema can't model: `server()` after `begin()`, gated with `authorized(req)`, wrapped in `pageShell()`, registered for nav with `addPage()`. This is the long-tail escape hatch — foundational needs should become a first-class feature (command, variable, collection, or file) instead.
- Editing the UI: keep it self-contained with no external assets, so it works offline on a SoftAP.

### Wire protocol

Every message is a JSON object with a `t` (type) field. The schema is versioned (`schema v1` in the `hello` manifest) and is the contract custom clients code against. Every WebSocket capability has an HTTP mirror under `/api/` for `curl` and scripts, with one asymmetry: `POST /api/cmd` is fire-and-forget, because execution is deferred to `loop()` and the result arrives over the WebSocket rather than in the HTTP response. Both surfaces are documented in full in `esp_webserver/README.md`.

Auth is opt-in via `cfg.authToken`. When set, it gates `cmd` and `var_set` (and the write-side HTTP endpoints); read-only endpoints stay open.
