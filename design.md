# Jellybeans design notes

For "how to build/adopt a bean," see each bean's own README.
This doc is the reasoning behind the shared conventions.

## What a "jellybean" is

A jellybean is a self-contained, drop-in feature for MCU development
projects. It's the software equivalent of a [jellybean part](https://en.wikipedia.org/wiki/Jellybean_%28integrated_circuit%29):
a generic, interchangeable component that works the same everywhere.

Each bean lives in its own subdirectory as a PlatformIO library
(`library.json` + `src/` + `examples/` + `README.md`) and can be pulled into a
host project two ways:

- copied into the project's `lib/`, or
- referenced in place via `lib_extra_dirs = /path/to/jellybeans`.

## The second bean shape: contract beans

`roost_logging` is the first bean whose source of truth is data rather than
code. A contract bean keeps a machine-readable definition (`registry/*.toml`),
a generator, and checked-in generated artifacts, so it carries `registry/`,
`tools/`, `generated/`, and `tests/` instead of a `src/`.

Three convention differences follow from that shape, and are allowed for beans
of this kind:

- **Verification is a test suite, not just a building example.** The guarantee a
  contract bean sells is that the generated artifact still matches its source.
  That can only be a check that regenerates and diffs, so a contract bean owns a
  `tests/` entry point and states its dependencies in its README.
- **The API may be macro-based, so the `jelly::` namespace does not fully
  apply.** Preprocessor symbols have no scope. A contract bean uses a unique
  symbol prefix instead, which buys the same collision safety the namespace
  does.
- **Each bean carries its own license.** There is no repository-wide license, so
  a bean ships a `LICENSE` beside its `library.json`, declares the same there,
  and repeats it in each source file's header. An umbrella license would force
  every bean to the most restrictive terms present, which would deny an MIT-only
  build to beans meant for free adoption.

One adoption caveat that applies only to this shape: copying a contract bean
into a host's `lib/` pins its definition at copy time while the host keeps
moving, which is the drift the contract exists to prevent. Reference it in place.

## The core principle: consistent contract, not consistent code

The value of a collection like this is not that the beans share
implementation; it's that they share contracts a host project already knows.
A new bean should feel like the last one: same wiring shape, same conventions,
same docs layout. You should never re-learn an interface or rebuild a schema
from scratch to adopt one.

## Naming & namespace convention (and why)

All beans live under one umbrella namespace, `jelly`, with each bean nested in
its own sub-namespace: `jelly::webconsole::…`, `jelly::<nextbean>::…`.

The reasoning for this is to prevent collisions in naming, but also because I don't
want to maintain a bunch of different repos.

Two companion rules that follow the same logic:

- **Feature-specific header names** (`WebConsole.h`, not `Console.h`). The include
  path is a flat global namespace too.
- **Distinct `library.json` names** per bean.

Nested-namespace syntax (`namespace jelly::webconsole { … }`) requires C++17,
which the pioarduino / Arduino-ESP32 3.x (IDF 5.x) toolchain provides. Untested
in the Arduino IDE.

## Concurrency posture (embedded default)

Beans that touch async frameworks should keep host handlers out of framework
task contexts by default. `esp_webserver` is an example: inbound async
callbacks never run user code directly. Work is copied into an owned queue and
executed from the host's `loop()`/`tick()`, so a user's handler may touch
hardware freely without knowing the framework's threading model. New beans that
wrap callback-driven libraries should follow suit rather than exporting the
framework's footguns to every adopter.

## Platform baseline

Beans target the pioarduino `platform-espressif32` fork (Arduino-ESP32 3.x /
IDF 5.x). Where a bean depends on async networking, it pins the maintained
ESP32Async forks (`ESPAsyncWebServer`, `AsyncTCP`), because the original
`me-no-dev` repos do not build consistently. Pin dependency versions in each
bean's `library.json`.
