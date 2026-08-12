# Roost Logging: Unified Logging 

- One TOML registry defines field names, types, units, enum values, and record layouts once.
- A generated C header gives every device the same row builder, so no device reimplements quoting,
  column order, or enum restriction.
- Each device declares what it can capture, and a session manifest ships that declaration with the
  capture.
- A consumer can tell an uncaptured value from an uncapturable one, because absence is only
  interpretable when a capture records its own limits.

Header-only. No Python in the firmware build, no build-system change to adopt.

## Why Did You Build This Extremely Complicated Solution to a Simple Issue?

Devices that implement logging independently drift. The same measurement gets two names, one name
means two things, and a device that cannot measure something produces the same empty column as a
device that measured nothing or found nothing. Unified logging is one less thing to keep track of.

The registry removes the guessing by making the vocabulary generated:

```
registry/*.toml  ->  gen_registry.py  ->  roost_registry.h      (firmware)
                                      ->  manifest.schema.json  (pipeline)

board_config.h  ->  firmware  ->  session directory
(capabilities)                    manifest.json + CSV
```

Both generated artifacts carry the registry hash, and `tests/run.sh` fails if either has drifted
from the TOML.

## Drop-in instructions

1. Make the library visible. Reference it from the repo, or in place against a local checkout, so
   the registry and the firmware move together:

   ```ini
   [env:my_board]
   lib_deps = https://github.com/LoneCrowDesign/jellybeans.git
   lib_ldf_mode = deep+
   ```

   Against a local checkout, use `symlink://` rather than a bare path:

   ```ini
   lib_deps = symlink:///path/to/jellybeans/roost_logging
   ```

   A bare path copies the library into `.pio/libdeps` once and never refreshes it, which reintroduces
   the drift the contract exists to remove. `deep+` is needed if the code that builds rows is itself
   a library rather than living in `src/`.

2. Declare the board in one header. Every capability must be defined; undefined is a compile error,
   so a board cannot silently inherit a default it does not have.

   ```c
   // board_config.h
   #define ROOST_CAP_GNSS      1
   #define ROOST_CAP_STORAGE   1
   #define ROOST_CAP_WIFI      1
   #define ROOST_CAP_WIFI_SCAN 1
   // ... every capability in design_spec.md section 4.2

   #define ROOST_COMPONENTS \
       ROOST_COMPONENT(wifi0, "wifi0", "wifi", "ESP32-S3", ROOST_BAND_2G4)
   ```

3. Include the header and build rows through the builder. Do not restate a column list, a header
   string, or a filename: all of them are rendered from the declarations above.

   ```c
   #include "roost_registry.h"

   RoostRow row;
   roostRowBegin(&row, buf, sizeof buf, ROOST_REC_WIFI_OBS, columnsMask);
   roostRowSetMac(&row, ROOST_F_BSSID, bssid);
   roostRowSetI32(&row, ROOST_F_RSSI_DBM, rssi);
   if (!roostRowFinish(&row)) { /* required field missing, or an unknown enum */ }
   ```

`docs/design_spec.md` is the specification and states what an implementation must do, in order.
Start with section 6, the emitter contract.

## Layout

| Path | Contents |
|---|---|
| `registry/enums.toml` | Closed value vocabularies |
| `registry/fields.toml` | Field definitions: name, type, unit, format, null semantics |
| `registry/records.toml` | Record types, their canonical field order, and required fields |
| `registry/capabilities.toml` | Compile-time capabilities that gate fields and record types |
| `registry/manifest.toml` | Session manifest provenance keys. The contract half is derived, not declared |
| `tools/gen_registry.py` | Validates the registry and generates both artifacts |
| `generated/roost_registry.h` | Generated. Checked in; never hand-edited |
| `generated/manifest.schema.json` | Generated. What the pipeline validates a manifest against |
| `docs/design_spec.md` | The model, the design choices, and the generator, emitter and ingest contracts |
| `docs/examples/` | An example manifest, validated by the test suite |
| `tests/run.sh` | The full gate. Run before any commit touching `registry/` |

## Regenerating

The generated files are checked in, so a consumer needs no Python. After editing `registry/`:

```bash
python3 tools/gen_registry.py \
    --out generated/roost_registry.h \
    --schema-out generated/manifest.schema.json
```

Both artifacts must be regenerated together; both carry the registry hash and both are drift-checked.

## Running the tests

```bash
./tests/run.sh
```

Validates the registry, fails on drift between it and the checked-in artifacts, then compiles and
runs the host tests, four board profiles, three negative cases, and the manifest schema test.
Requires `python3` 3.11+ for `tomllib` and a C++17 compiler. No other dependencies.

## Status

All six record types are defined, the manifest schema is generated and validated, and capability
masks derive from compile-time macros. `wifi_obs` is at v2 and the other five record types at v1.
No record type is frozen and no external consumer's compatibility has been promised; see
`docs/design_spec.md` section 3.5 before depending on a column layout.

## License

MIT. See [LICENSE](LICENSE).
