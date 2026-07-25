# Jellybeans

Self-contained, drop-in features for ESP32 / embedded projects. The software
equivalent of [jellybean parts](https://en.wikipedia.org/wiki/Jellybean_%28integrated_circuit%29):
generic, interchangeable components that work the same everywhere. Each bean is a
PlatformIO library you copy into a project's `lib/` or reference in place with
`lib_extra_dirs`.

The reasoning behind the shared conventions (self-describing-contract
principle, the namespace scheme, the concurrency posture, and the platform
baseline) lives in [design.md](design.md). Read it before adding a bean.

## Conventions (see [design.md](design.md) for the why)

- **Namespace:** one umbrella `jelly`, each bean in its own sub-namespace
  `jelly::<bean>::…` (e.g. `jelly::webconsole::WebConsole`). Keeps beans from
  colliding when a project adopts several at once.
- **Names are feature-specific:** unique header names (`WebConsole.h`) and unique
  `library.json` names.
- **Platform:** pioarduino `platform-espressif32` (Arduino-ESP32 3.x / IDF 5.x);
  pin async deps to the ESP32Async forks. C++17 is required, since beans use
  nested-namespace syntax.
- **Concurrency:** beans that wrap callback-driven libraries keep host handlers
  out of the framework's task context. Inbound work is deferred into the host's
  `loop()`, so your handlers may touch hardware freely.
- **Examples:** each bean contains a buildable `examples/` sketch that compiles the
  library in place, plus a README covering its API and wire contract.

## Beans

| Bean | What it is |
|---|---|
| [`esp_webserver`](esp_webserver) | Schema-driven async web console with standard functions builtin |

## Building

There is no top-level build. The repo is a container, not an application. Each
bean builds through its own example:

```bash
cd esp_webserver/examples/basic
pio run
```

The example references the library in place, so it compiles the working tree.
Each example targets one board. `esp_webserver`'s is an ESP32-S3 with PSRAM, so
a clean build covers that configuration; on boards without PSRAM, RAM is the
binding constraint.

Adopting a bean requires the pioarduino `platform-espressif32` fork and the
ESP32Async forks of `ESPAsyncWebServer` / `AsyncTCP`; the original `me-no-dev`
repos do not build against it. Each bean pins its dependency versions in
`library.json`.
