#!/usr/bin/env python3
"""
test_manifest.py: validate the example manifests against the generated schema.

Closes the loop between the two generated artifacts. The firmware renders a
manifest's contract half from `roost_registry.h`, and the pipeline validates what
it receives against `manifest.schema.json`. Both come from the same TOML, so this
checks that a manifest the firmware could plausibly write is one the pipeline
would accept.

Also checks the part a JSON Schema cannot express: that every column a manifest
declares actually exists in the record type it names, and appears in the
registry's canonical order. A schema can say "columns is an array of strings"; it
cannot say "and each one is a real field of wifi_obs, in the right order".

    python3 tests/test_manifest.py                  # the example manifests
    python3 tests/test_manifest.py path/to/session/manifest.json
    python3 tests/test_manifest.py --shape-only rendered.json

--shape-only skips the check that every declared file is present, for a manifest
that is not sitting in a session directory: a firmware build rendering the
manifest it would write, to find out whether the contract would accept it.
Without the flag, a manifest with no files beside it fails that check, which is
correct for a session directory and wrong for a renderer.

A manifest a device actually wrote is checked by the same code the gate runs, so
a bench result and a gate result cannot disagree.
"""

import json
import sys
import tomllib
from pathlib import Path

_ROOT = Path(__file__).resolve().parent.parent
_SCHEMA = _ROOT / "generated" / "manifest.schema.json"
_EXAMPLES = _ROOT / "docs" / "examples"

_fail = 0


def ok(what):
    print(f"  ok    {what}")


def bad(what, detail=""):
    global _fail
    _fail += 1
    print(f"  FAIL  {what}")
    if detail:
        print(f"          {detail}")


def check_schema(name, doc, schema):
    try:
        import jsonschema
    except ImportError:
        print(f"  skip  {name} schema validation "
              f"(jsonschema not installed)")
        return
    try:
        jsonschema.validate(doc, schema)
        ok(f"{name} validates against the generated schema")
    except jsonschema.ValidationError as exc:
        bad(f"{name} fails schema validation",
            f"{'/'.join(str(p) for p in exc.absolute_path)}: {exc.message}")


def check_columns(name, doc, records):
    """Every declared column is a real field of its record, in canonical order.

    A manifest whose columns disagree with the registry is the drift the
    contract exists to prevent, and it is invisible to a JSON Schema.
    """
    for entry in doc["files"]:
        rec = entry["record"]
        # A raw capture file lives in this array too and has no columns to
        # check against a record type. The schema has already verified it
        # declares linktype, snaplen and timebase, which is everything a reader
        # needs; there is no registry entry to compare it against, because its
        # shape is a capture-format fact rather than a record definition.
        if entry.get("format") == "pcap":
            ok(f"{name}/{rec}: raw capture, linktype {entry['linktype']}, "
               f"{entry['timebase']} timebase")
            continue
        # Dispatch on record AND version, never on name alone. A retired version
        # is still in the registry precisely so a capture written under it stays
        # readable, so a name now resolves to more than one definition and the
        # manifest's own version says which. Keyed by name only, a v1 capture
        # would be validated against the v2 shape and rejected for carrying a
        # column that version legitimately had.
        ver = entry["version"]
        definition = records.get((rec, ver))
        if definition is None:
            known = sorted(v for n, v in records if n == rec)
            bad(f"{name}: no registry definition for record {rec!r} version {ver}",
                f"known versions: {known}" if known else "unknown record type")
            continue
        canonical = definition["fields"]
        cols = entry["columns"]

        unknown = [c for c in cols if c not in canonical]
        if unknown:
            bad(f"{name}/{rec}: columns not in the record type",
                ", ".join(unknown))
            continue

        positions = [canonical.index(c) for c in cols]
        if positions != sorted(positions):
            bad(f"{name}/{rec}: columns are not in canonical order",
                f"declared {cols}")
            continue

        missing_required = [f for f in definition.get("required", [])
                            if f not in cols]
        if missing_required:
            bad(f"{name}/{rec}: required fields absent from columns",
                ", ".join(missing_required))
            continue

        overlap = set(cols) & set(entry.get("capable_unrecorded", []))
        if overlap:
            bad(f"{name}/{rec}: a field is both recorded and capable_unrecorded",
                ", ".join(sorted(overlap)))
            continue

        expected = f"{rec}.v{ver}.csv"
        if entry["name"] != expected:
            bad(f"{name}/{rec}: filename {entry['name']!r}, expected {expected!r}")
            continue

        retired = " (retired version, read-only)" if definition.get("retired") else ""
        ok(f"{name}/{rec}: {len(cols)} columns, canonical order, version and "
           f"filename agree with the registry{retired}")


def check_components(name, doc):
    """Checks the schema cannot express: ids are unique and usable as row values.

    cap_component is a device-scoped id validated against this list rather than
    against a fleet-wide enum, so this list is the vocabulary. Two components
    sharing an id make every row naming it ambiguous, which defeats the purpose
    of the field.
    """
    comps = (doc.get("hardware") or {}).get("components")
    if not comps:
        bad(f"{name}: hardware.components is empty; nothing validates cap_component")
        return

    ids = [c.get("id") for c in comps]
    if any(not i for i in ids):
        bad(f"{name}: a component has no id")
        return
    dupes = sorted({i for i in ids if ids.count(i) > 1})
    if dupes:
        bad(f"{name}: duplicate component ids: {', '.join(dupes)}")
        return
    if any(i != i.strip() or "," in i or '"' in i for i in ids):
        bad(f"{name}: a component id needs CSV quoting; keep ids plain")
        return

    kinds = sorted({c.get("kind") for c in comps})
    ok(f"{name}: {len(comps)} components, unique ids, kinds {', '.join(kinds)}")


def check_declared_files_exist(name, doc, path):
    """Every file the manifest declares is present in the session directory.

    --shape-only is the only way to skip this. Do not add a path-based exemption:
    a real session's manifest used as a fixture would then stop being checked.
    """
    session = path.parent
    files = doc.get("files") or []
    if not files:
        bad(f"{name}: declares no files")
        return

    missing = [f.get("name") for f in files if not (session / f.get("name", "")).exists()]
    if missing:
        bad(f"{name}: declared but not in {session.name}: {', '.join(missing)}",
            "a record type that observed nothing is a header with no rows,"
            " never an absent file")
        return

    # Zero rows is fine; zero bytes is not, since the declared header is missing.
    empty = [f.get("name") for f in files
             if f.get("format") == "csv" and (session / f["name"]).stat().st_size == 0]
    if empty:
        bad(f"{name}: declared and empty, with no header: {', '.join(empty)}")
        return

    rowless = [f["name"] for f in files
               if f.get("format") == "csv"
               and len((session / f["name"]).read_bytes().splitlines()) <= 1]
    detail = f", {len(rowless)} with no rows" if rowless else ""
    ok(f"{name}: all {len(files)} declared files present in {session.name}{detail}")


def main():
    if not _SCHEMA.exists():
        print(f"schema not generated: {_SCHEMA}\n"
              f"run: python3 tools/gen_registry.py --schema-out {_SCHEMA}",
              file=sys.stderr)
        return 1

    schema = json.loads(_SCHEMA.read_text())
    with (_ROOT / "registry" / "records.toml").open("rb") as fh:
        records = {(r["name"], r["version"]): r
                   for r in tomllib.load(fh)["record"]}

    args = [a for a in sys.argv[1:] if a != "--shape-only"]
    # The examples are manifests, not sessions, with no files beside them by
    # construction, so the no-argument gate run is shape-only whether or not
    # the flag was passed.
    shape_only = "--shape-only" in sys.argv[1:] or not args

    if args:
        examples = [Path(a) for a in args]
        missing = [p for p in examples if not p.exists()]
        if missing:
            print(f"no such manifest: {missing[0]}", file=sys.stderr)
            return 1
    else:
        examples = sorted(_EXAMPLES.glob("*.json"))
        if not examples:
            print(f"no example manifests in {_EXAMPLES}", file=sys.stderr)
            return 1

    for path in examples:
        print(f"\n{path.name}:")
        doc = json.loads(path.read_text())
        check_schema(path.stem, doc, schema)
        check_columns(path.stem, doc, records)
        check_components(path.stem, doc)
        if not shape_only:
            check_declared_files_exist(path.stem, doc, path)

    print(f"\n{'FAILED' if _fail else 'all checks passed'}")
    return 1 if _fail else 0


if __name__ == "__main__":
    sys.exit(main())
