#!/usr/bin/env python3
# Copyright (c) 2026 Lone Crow Design, LLC
# SPDX-License-Identifier: MIT
"""
gen_registry.py - validate the log registry and generate the firmware header.

The registry (registry/*.toml) is the single source of truth for field names,
types, units, enum vocabularies, and record layouts across every device in the
birdoscope ecosystem. This script is how that truth reaches firmware: it emits a
header-only C file that each firmware repo checks in, with a test asserting the
checked-in copy still matches (docs/design_spec.md section 9).

Header-only is deliberate. A generated .h with static inline functions can be
dropped into a PlatformIO project with no build-system change and no Python
dependency in the build.

    python3 tools/gen_registry.py --check          # validate only, exit 1 on drift
    python3 tools/gen_registry.py --out <path.h>   # write the header

The analysis pipeline does not consume this header. It reads the TOML directly
and validates a manifest against the generated JSON Schema.
"""

import argparse
import hashlib
import json
import sys
import tomllib
from pathlib import Path

_ROOT = Path(__file__).resolve().parent.parent
_REGISTRY = _ROOT / "registry"

# A record's populatable-field set is passed to firmware as a bitmask, so a
# record cannot carry more fields than the mask has bits. Raising this means
# widening the mask type in the generated header, so it fails loudly instead.
MAX_FIELDS_PER_RECORD = 64

# Wire types a field may declare. Closed on purpose: an unrecognised type would
# otherwise reach the generated header as an unvalidated comment.
VALID_TYPES = {
    "text", "mac", "hex", "timestamp", "enum", "bool",
    "u8", "u16", "u32", "i8", "i16", "i32", "f32", "f64",
}


# Types a manifest key may declare. Narrower than the field types: a manifest is
# JSON, so it has containers a CSV column cannot, and no fixed-width integers.
MANIFEST_TYPES = {"text", "list", "map", "u32", "f32", "bool", "enum", "struct_list"}
# Types a struct_list item may take. Scalars, or a list of scalars constrained by
# a vocabulary. Not objects: a manifest that needs a tree is a manifest
# describing something that should have been its own record type.
ITEM_TYPES = {"text", "u32", "f32", "bool", "enum", "list"}

# What a raw capture's record timestamps mean. A manifest key rather than a
# field vocabulary, so it is here and not in enums.toml; both the C enum and the
# JSON Schema's enum are generated from this tuple. "boot" is index 0 so a
# zeroed declaration makes the claim that needs the anchor rather than the one
# that reads as 1970.
TIMEBASE = ("boot", "utc")

# Wire type -> the setter class the row builder validates against. Several
# registry types share one class: a timestamp is text on the wire, and every
# integer width writes the same way.
TYPE_CLASS = {
    "text": "TEXT", "timestamp": "TEXT", "mac": "MAC", "hex": "HEX",
    "enum": "ENUM", "bool": "BOOL", "f32": "FLOAT", "f64": "FLOAT",
    "u8": "UINT", "u16": "UINT", "u32": "UINT",
    "i8": "INT", "i16": "INT", "i32": "INT",
}

_JSON_TYPE = {
    "text": "string", "list": "array", "map": "object", "struct_list": "array",
    "u32": "integer", "f32": "number", "bool": "boolean", "enum": "string",
}


class RegistryError(ValueError):
    """The registry is internally inconsistent. Never a warning."""


def _load():
    out = {}
    for name in ("enums", "fields", "records", "manifest", "capabilities"):
        path = _REGISTRY / f"{name}.toml"
        if not path.exists():
            raise RegistryError(f"missing registry file: {path}")
        with path.open("rb") as fh:
            out[name] = tomllib.load(fh)
    return out


def _validate(reg):
    """Every check here is a hard error. A registry that is silently accepted
    while internally inconsistent produces captures no consumer can trust."""
    enums = {e["name"]: e for e in reg["enums"].get("enum", [])}
    caps = {}
    for c in reg["capabilities"].get("capability", []):
        if c["name"] in caps:
            raise RegistryError(f"capability {c['name']!r} is defined twice")
        caps[c["name"]] = c
    # Runtime-tier keys are not manifest keys: they become config_change's
    # `setting` vocabulary. Synthesized rather than declared in enums.toml so
    # there is one definition of each setting, not two that can disagree.
    runtime_keys = [k for sec in reg["manifest"].get("section", [])
                    if sec["tier"] == "runtime" for k in sec["keys"]]
    if runtime_keys:
        if "config_setting" in enums:
            raise RegistryError(
                "enums.toml defines 'config_setting', which the generator "
                "synthesizes from runtime-tier manifest keys. Remove it; two "
                "definitions of one vocabulary is what the tier split avoids."
            )
        enums["config_setting"] = {
            "name": "config_setting",
            "description": "Runtime-mutable settings, from the runtime tier of "
                           "manifest.toml. Synthesized, not hand-written.",
            "values": [{"name": k["name"],
                        "description": k.get("description", "")}
                       for k in runtime_keys],
        }

    # Retired fields and records describe captures that already exist, so they
    # are read-only: the generator never emits one, the hash never sees one, and
    # no device can name one. Validation still applies in full: a definition a
    # consumer cannot trust is worse than no definition at all.
    fields = {}
    retired_fields = set()
    for f in reg["fields"].get("field", []):
        name = f["name"]
        if name in fields:
            raise RegistryError(
                f"field {name!r} is defined twice. A field name means one thing "
                f"fleet-wide; redefining it is how two devices come to disagree."
            )
        if f.get("retired"):
            retired_fields.add(name)
        if f["type"] not in VALID_TYPES:
            raise RegistryError(
                f"field {name!r} has unknown type {f['type']!r}. "
                f"Known: {', '.join(sorted(VALID_TYPES))}"
            )
        if f["type"] == "enum":
            if "enum" not in f:
                raise RegistryError(f"field {name!r} is type enum but names no vocabulary")
            if f["enum"] not in enums:
                raise RegistryError(
                    f"field {name!r} references enum {f['enum']!r}, which is not "
                    f"defined in enums.toml"
                )
        elif "enum" in f:
            raise RegistryError(f"field {name!r} names an enum but is type {f['type']!r}")
        for cap in f.get("requires", []):
            if cap not in caps:
                raise RegistryError(
                    f"field {name!r} requires capability {cap!r}, which is not "
                    f"defined in capabilities.toml"
                )
        fields[name] = f

    for e in enums.values():
        seen = set()
        for v in e["values"]:
            if v["name"] in seen:
                raise RegistryError(f"enum {e['name']!r} lists value {v['name']!r} twice")
            seen.add(v["name"])

    records = {}
    retired_records = {}
    for r in reg["records"].get("record", []):
        name, version = r["name"], r["version"]
        is_retired = bool(r.get("retired"))
        target = retired_records if is_retired else records
        if (name, version) in {**records, **retired_records}:
            raise RegistryError(
                f"record type {name!r} version {version} is defined twice"
            )
        if not is_retired and any(n == name for n, _ in records):
            live = next(v for n, v in records if n == name)
            raise RegistryError(
                f"record type {name!r} has two live versions, {live} and {version}. "
                f"Exactly one is the contract devices compile against; mark the "
                f"older one 'retired = true' so consumers can still read its "
                f"captures."
            )
        if len(r["fields"]) > MAX_FIELDS_PER_RECORD:
            raise RegistryError(
                f"record {name!r} has {len(r['fields'])} fields, over the "
                f"{MAX_FIELDS_PER_RECORD} the populatable-field bitmask can carry"
            )
        seen = set()
        for fname in r["fields"]:
            if fname not in fields:
                raise RegistryError(
                    f"record {name!r} lists field {fname!r}, which is not defined "
                    f"in fields.toml"
                )
            if fname in seen:
                raise RegistryError(f"record {name!r} lists field {fname!r} twice")
            # A retired field exists only to type columns already on a card. A
            # live record naming one would put it back in the generated header
            # and in the hash, which is exactly the removal being undone.
            if fname in retired_fields and not is_retired:
                raise RegistryError(
                    f"record {name!r} v{version} is live but lists retired field "
                    f"{fname!r}. Un-retire the field deliberately, or drop it from "
                    f"the record."
                )
            seen.add(fname)
        for fname in r.get("required", []):
            if fname not in seen:
                raise RegistryError(
                    f"record {name!r} requires field {fname!r}, which it does not list"
                )
        for fname, allowed in r.get("restrict", {}).items():
            if fname not in seen:
                raise RegistryError(
                    f"record {name!r} restricts field {fname!r}, which it does not list"
                )
            if fields[fname]["type"] != "enum":
                raise RegistryError(
                    f"record {name!r} restricts field {fname!r}, which is type "
                    f"{fields[fname]['type']!r}. Only enum fields can be restricted."
                )
            vocab = {v["name"] for v in enums[fields[fname]["enum"]]["values"]}
            for value in allowed:
                if value not in vocab:
                    raise RegistryError(
                        f"record {name!r} restricts {fname!r} to {value!r}, which is "
                        f"not in enum {fields[fname]['enum']!r}"
                    )
            if not allowed:
                raise RegistryError(
                    f"record {name!r} restricts {fname!r} to nothing, which makes the "
                    f"field unwritable. Drop the field instead."
                )
        for cap in r.get("requires", []):
            if cap not in caps:
                raise RegistryError(
                    f"record {name!r} requires capability {cap!r}, which is not "
                    f"defined in capabilities.toml"
                )
        # A required field gated on a capability the record does not itself
        # require would make the record unemittable on some builds for a reason
        # the record never states.
        for fname in r.get("required", []):
            gates = fields[fname].get("requires", [])
            if gates and not set(gates) & set(r.get("requires", [])):
                raise RegistryError(
                    f"record {name!r} requires field {fname!r}, which is gated on "
                    f"{gates}. A required field must be reachable whenever the "
                    f"record is, so the record must require one of those too."
                )
        target[(name, version)] = r

    # A field no record references is dead weight or a reference that was typo'd
    # and fell through. Not fatal, since a placeholder for a record type not yet
    # written is legitimate, but never silent. Retired records count as
    # references: a retired field is referenced only by the version that still
    # carries it, and reporting it as an orphan would train the reader to ignore
    # this warning.
    referenced = {f for r in (*records.values(), *retired_records.values())
                  for f in r["fields"]}
    orphans = sorted(set(fields) - referenced)

    # Manifest provenance keys. The contract half is derived from records +
    # masks and is deliberately not declared here, so there is nothing to keep
    # in step with it.
    sections = reg["manifest"].get("section", [])
    seen_keys = {}
    for s in sections:
        for k in s["keys"]:
            kn = k["name"]
            if kn in seen_keys:
                raise RegistryError(
                    f"manifest key {kn!r} appears in both section "
                    f"{seen_keys[kn]!r} and {s['name']!r}. Keys are flat within a "
                    f"manifest, so a duplicate name is two facts under one label."
                )
            seen_keys[kn] = s["name"]
            if k["type"] not in MANIFEST_TYPES:
                raise RegistryError(
                    f"manifest key {kn!r} has unknown type {k['type']!r}. "
                    f"Known: {', '.join(sorted(MANIFEST_TYPES))}"
                )
            if k["type"] == "enum":
                if k.get("enum") not in enums:
                    raise RegistryError(
                        f"manifest key {kn!r} references enum {k.get('enum')!r}, "
                        f"which is not defined in enums.toml"
                    )
            elif "enum" in k:
                raise RegistryError(
                    f"manifest key {kn!r} names an enum but is type {k['type']!r}"
                )

            if k["type"] == "struct_list":
                items = k.get("items")
                if not items:
                    raise RegistryError(
                        f"manifest key {kn!r} is a struct_list but declares no "
                        f"items. An untyped list of objects is exactly the "
                        f"unvalidated blob this type exists to replace."
                    )
                seen_items = set()
                for it in items:
                    iname = it.get("name")
                    if not iname:
                        raise RegistryError(f"manifest key {kn!r} has an unnamed item")
                    if iname in seen_items:
                        raise RegistryError(
                            f"manifest key {kn!r} declares item {iname!r} twice"
                        )
                    seen_items.add(iname)
                    if it.get("type") not in ITEM_TYPES:
                        raise RegistryError(
                            f"manifest key {kn!r} item {iname!r} has type "
                            f"{it.get('type')!r}. Known: {', '.join(sorted(ITEM_TYPES))}"
                        )
                    if it["type"] in ("enum", "list"):
                        if it.get("enum") is not None and it["enum"] not in enums:
                            raise RegistryError(
                                f"manifest key {kn!r} item {iname!r} references enum "
                                f"{it.get('enum')!r}, which is not defined in enums.toml"
                            )
                        if it["type"] == "enum" and it.get("enum") is None:
                            raise RegistryError(
                                f"manifest key {kn!r} item {iname!r} is type enum but "
                                f"names no vocabulary"
                            )
                    elif "enum" in it:
                        raise RegistryError(
                            f"manifest key {kn!r} item {iname!r} names an enum but is "
                            f"type {it['type']!r}"
                        )
            elif "items" in k:
                raise RegistryError(
                    f"manifest key {kn!r} declares items but is type {k['type']!r}"
                )
        if s["tier"] not in ("build", "session", "runtime"):
            raise RegistryError(
                f"manifest section {s['name']!r} declares tier {s['tier']!r}. "
                f"Only 'build', 'session' and 'runtime' exist; registry-tier "
                f"constants live in the registry and never travel in a capture."
            )

    # Runtime-tier sections are not part of the manifest.
    sections = [sec for sec in sections if sec["tier"] != "runtime"]

    return enums, fields, records, orphans, sections, caps


def _canonical(reg):
    """Semantic fingerprint of the registry.

    Hashes the parsed structure rather than the file bytes, so reformatting or
    editing a description does not churn the hash and force a needless reflash.
    A change that alters meaning does change it.

    Retired fields and record versions are excluded. The hash answers "what
    contract does this build emit", and a retired entry is emitted by nothing.
    Including one would move the hash, and so force every device to be reflashed,
    to record only that an old capture stays readable.
    """
    live_fields = [f for f in reg["fields"].get("field", [])
                   if not f.get("retired")]
    live_records = [r for r in reg["records"].get("record", [])
                    if not r.get("retired")]
    payload = {
        "enums": [
            {"name": e["name"], "values": [v["name"] for v in e["values"]]}
            for e in sorted(reg["enums"].get("enum", []), key=lambda x: x["name"])
        ],
        "fields": [
            {k: f.get(k) for k in ("name", "type", "enum", "unit", "precision",
                                   "requires")}
            for f in sorted(live_fields, key=lambda x: x["name"])
        ],
        "records": [
            {
                "name": r["name"], "version": r["version"],
                "fields": r["fields"], "required": r.get("required", []),
                "restrict": {k: sorted(v) for k, v in sorted(r.get("restrict", {}).items())},
                "requires": r.get("requires", []),
            }
            for r in sorted(live_records, key=lambda x: x["name"])
        ],
        "capabilities": sorted(
            (c["name"], c["macro"]) for c in reg["capabilities"].get("capability", [])
        ),
        "manifest_version": reg["manifest"].get("manifest_version"),
        # Encoder behaviour, which lives in this generator rather than in any
        # TOML and so would otherwise be the one part of the contract the hash
        # cannot see. Two builds that put a byte on the wire differently must
        # not both claim the same registry.
        "text_encoding_rev": reg["manifest"].get("text_encoding_rev"),
        # Name, type and item shape, not just the name. A key that changes type,
        # or a struct_list that gains or retypes an item, changes what a manifest
        # means, so it has to move the hash the way any other meaning change does.
        "manifest_keys": sorted(
            (
                k["name"], k["type"], k.get("enum"),
                tuple(sorted((i["name"], i["type"], i.get("enum"))
                             for i in k.get("items", []))),
            )
            for s in reg["manifest"].get("section", []) for k in s["keys"]
        ),
    }
    blob = json.dumps(payload, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(blob).hexdigest()


def _ident(s):
    """A C identifier fragment. Enum values like "2.4" are legal on the wire and
    illegal in C, so the wire value and the identifier are allowed to differ."""
    out = "".join(c if c.isalnum() else "_" for c in s).upper()
    while "__" in out:
        out = out.replace("__", "_")
    return out.strip("_")


def _emit(reg, enums, fields, records, sections, caps):
    h = _canonical(reg)
    L = []
    w = L.append

    w("// Copyright (c) 2026 Lone Crow Design, LLC")
    w("// SPDX-License-Identifier: MIT")
    w("//")
    w("// roost_registry.h - GENERATED FILE, DO NOT EDIT.")
    w("//")
    w("// Generated from registry/*.toml by tools/gen_registry.py.")
    w("// Edit the TOML and regenerate; a hand edit here is reverted by the next")
    w("// run and caught by the registry test in the meantime.")
    w("//")
    w("// This header is how independent repositories stay in step without having")
    w("// to agree with each other directly. A device declares the fields it can")
    w("// populate as a bitmask, and roostHeader() emits the canonical-order")
    w("// projection of the record type. The CSV emitter is then one loop over a")
    w("// table rather than a per-device format string, which is what keeps the")
    w("// column layouts from drifting apart.")
    w("//")
    w(f"// Registry fingerprint: {h}")
    w("")
    w("#pragma once")
    w("")
    w("#include <stddef.h>")
    w("#include <stdint.h>")
    w("#include <string.h>")
    w("#include <stdio.h>")
    w("")
    w(f'#define ROOST_REGISTRY_HASH "{h}"')
    w(f'#define ROOST_REGISTRY_HASH_SHORT "{h[:12]}"')
    w("")
    w("// Populatable-field bitmask. Bit N corresponds to index N in the record's")
    w("// canonical field order.")
    w("typedef uint64_t RoostFieldMask;")
    w("#define ROOST_F(idx) ((RoostFieldMask)1u << (idx))")
    w("")

    # ---- enums -----------------------------------------------------------
    w("// " + "-" * 74)
    w("// Enum vocabularies. A value absent from these tables cannot be emitted,")
    w("// which is what makes 'unknown enum values are an error' enforceable at")
    w("// the write path.")
    w("// " + "-" * 74)
    w("")
    for e in enums.values():
        en = _ident(e["name"])
        w(f"typedef enum {{")
        for i, v in enumerate(e["values"]):
            w(f"  ROOST_{en}_{_ident(v['name'])} = {i},")
        w(f"  ROOST_{en}_COUNT = {len(e['values'])}")
        w(f"}} Roost{''.join(p.capitalize() for p in e['name'].split('_'))};")
        w("")
        camel = ''.join(p.capitalize() for p in e['name'].split('_'))
        w(f"static const char *const kRoost{camel}[] = {{")
        for v in e["values"]:
            w(f'  "{v["name"]}",')
        w("};")
        w("")
        w("// Resolves a producer's spelling to the enum. Returns _COUNT when the")
        w("// name is not in the vocabulary, so an unresolvable value is a state the")
        w("// caller must handle rather than a silent zero, which is a real value.")
        w("// Emitted for every enum because a device reaching a typed field with a")
        w("// name string is the normal case, not the exception.")
        w(f"static inline Roost{camel} roost{camel}ByName(const char *name) {{")
        w(f"  if (name)")
        w(f"    for (int i = 0; i < ROOST_{en}_COUNT; i++)")
        w(f"      if (!strcmp(name, kRoost{camel}[i])) return (Roost{camel})i;")
        w(f"  return ROOST_{en}_COUNT;")
        w("}")
        w("")

    # ---- capabilities ----------------------------------------------------
    w("// " + "-" * 74)
    w("// Capabilities: what this build can measure, declared at compile time.")
    w("//")
    w("// Every board defines each macro below as 0 or 1, in its board_config.h.")
    w("// Where the board already declares the fact under its own name, alias it")
    w("// (#define ROOST_CAP_GNSS HAS_GPS) rather than restating it: an alias")
    w("// cannot disagree with the thing it aliases.")
    w("//")
    w("// Undefined is an error, never a default of 0. Defaulting would mean that")
    w("// adding a capability here silently shrinks every existing board's mask,")
    w("// and a silently shrinking mask is a capture quietly recording less than")
    w("// it could.")
    w("// " + "-" * 74)
    w("")
    for c in caps.values():
        w(f"// {c['name']}: {c['description'].strip().splitlines()[0]}")
        w(f"#if !defined({c['macro']})")
        w(f'#error "{c["macro"]} is undefined. Every board must declare each '
          f'capability as 0 or 1; see docs/design_spec.md section 4.2."')
        w("#endif")
    w("")

    # ---- records ---------------------------------------------------------
    w("// " + "-" * 74)
    w("// Record types.")
    w("// " + "-" * 74)
    w("")
    w("typedef enum {")
    for i, r in enumerate(records.values()):
        w(f"  ROOST_REC_{_ident(r['name'])} = {i},")
    w(f"  ROOST_REC_COUNT = {len(records)}")
    w("} RoostRecord;")
    w("")

    for r in records.values():
        rn = _ident(r["name"])
        w(f"// {r['name']} v{r['version']} - canonical field order.")
        w("enum {")
        for i, fname in enumerate(r["fields"]):
            w(f"  ROOST_{rn}_{_ident(fname)} = {i},")
        w(f"  ROOST_{rn}_FIELD_COUNT = {len(r['fields'])}")
        w("};")
        mask = 0
        for fname in r.get("required", []):
            mask |= 1 << r["fields"].index(fname)
        w(f"#define ROOST_{rn}_REQUIRED_MASK ((RoostFieldMask)0x{mask:016x}ull)")
        # Whether this build emits the record at all.
        rgates = r.get("requires", [])
        gate = " && ".join(caps[c]["macro"] for c in rgates) if rgates else "1"
        w(f"// Does this build emit {r['name']} at all?")
        w(f"#define ROOST_EMITS_{rn} ({gate} && ROOST_CAP_STORAGE)")
        # Capable mask, computed by the preprocessor from the capability macros.
        w(f"// Fields this build can populate, derived from the capability macros")
        w(f"// above. Never hand-written: a board with a card and one without")
        w(f"// differ only in board_config.h and this follows.")
        w(f"#define ROOST_{rn}_CAPABLE_MASK ( \\")
        parts = []
        for i, fname in enumerate(r["fields"]):
            fgates = fields[fname].get("requires", [])
            bit = f"ROOST_F(ROOST_{rn}_{_ident(fname)})"
            if not fgates:
                parts.append(f"    {bit}")
            else:
                cond = " || ".join(caps[c]["macro"] for c in fgates)
                parts.append(f"    (({cond}) ? {bit} : 0)")
        w(" \\\n".join(f"  {p}" if i == 0 else f"  | {p.lstrip()}"
                          for i, p in enumerate(parts)) + " \\")
        w("  )")
        # Boards may decline a field they can reach; the exclusion is declared in
        # the same compile-time place as the capabilities.
        w(f"#if !defined(ROOST_{rn}_EXCLUDE)")
        w(f"#define ROOST_{rn}_EXCLUDE 0")
        w("#endif")
        w(f"#define ROOST_{rn}_COLUMNS_MASK "
          f"(ROOST_{rn}_CAPABLE_MASK & ~(RoostFieldMask)(ROOST_{rn}_EXCLUDE))")
        # A board may decline a field it can reach. It may never decline a
        # REQUIRED one: that produces a file the record type declares unreadable,
        # and roostMaskSatisfiesRequired() would only catch it at runtime, on a
        # device in the field, after the capture. Both operands are preprocessor
        # constants, so the contradiction is a build failure instead.
        # Guarded on EMITS: a record this build does not emit has required fields
        # gated on capabilities it lacks, and that is not an error.
        w(f"#if ROOST_EMITS_{rn}")
        w(f"typedef char roost_required_{r['name']}_declared[")
        w(f"    ((ROOST_{rn}_COLUMNS_MASK & ROOST_{rn}_REQUIRED_MASK)")
        w(f"     == ROOST_{rn}_REQUIRED_MASK) ? 1 : -1];")
        w("#endif")
        for fname, allowed in sorted(r.get("restrict", {}).items()):
            vocab = [v["name"] for v in enums[fields[fname]["enum"]]["values"]]
            vmask = 0
            for value in allowed:
                vmask |= 1 << vocab.index(value)
            w(f"// {fname} on this record accepts only: {', '.join(sorted(allowed))}")
            w(f"#define ROOST_{rn}_{_ident(fname)}_ALLOWED "
              f"((uint64_t)0x{vmask:016x}ull)")
        w("")

    # ---- tables ----------------------------------------------------------
    for r in records.values():
        rn = _ident(r["name"])
        w(f"static const char *const kRoostFields{''.join(p.capitalize() for p in r['name'].split('_'))}[] = {{")
        for fname in r["fields"]:
            w(f'  "{fname}",')
        w("};")
    w("")
    w("static const char *const *const kRoostRecordFields[ROOST_REC_COUNT] = {")
    for r in records.values():
        w(f"  kRoostFields{''.join(p.capitalize() for p in r['name'].split('_'))},")
    w("};")
    w("")
    w("static const uint8_t kRoostRecordFieldCount[ROOST_REC_COUNT] = {")
    for r in records.values():
        w(f"  {len(r['fields'])},")
    w("};")
    w("")
    w("static const char *const kRoostRecordName[ROOST_REC_COUNT] = {")
    for r in records.values():
        w(f'  "{r["name"]}",')
    w("};")
    w("")
    w("static const uint16_t kRoostRecordVersion[ROOST_REC_COUNT] = {")
    for r in records.values():
        w(f"  {r['version']},")
    w("};")
    w("")
    w("static const RoostFieldMask kRoostRecordRequired[ROOST_REC_COUNT] = {")
    for r in records.values():
        w(f"  ROOST_{_ident(r['name'])}_REQUIRED_MASK,")
    w("};")
    w("")

    # ---- accessors -------------------------------------------------------
    w("// " + "-" * 74)
    w("// Accessors. Header-only so this file drops into a PlatformIO project")
    w("// with no build-system change.")
    w("// " + "-" * 74)
    w("")
    w("static inline const char *roostRecordName(RoostRecord r) {")
    w("  return (r < ROOST_REC_COUNT) ? kRoostRecordName[r] : \"\";")
    w("}")
    w("")
    w("static inline uint16_t roostRecordVersion(RoostRecord r) {")
    w("  return (r < ROOST_REC_COUNT) ? kRoostRecordVersion[r] : 0;")
    w("}")
    w("")
    w("static inline uint8_t roostRecordFieldCount(RoostRecord r) {")
    w("  return (r < ROOST_REC_COUNT) ? kRoostRecordFieldCount[r] : 0;")
    w("}")
    w("")
    w("// Which record types carry observations. The manifest's")
    w("// observations_written counts these and no others, so a device that adds")
    w("// a radio gets the right total without also editing its writer.")
    obs = [r["name"].upper() for r in records.values()
           if r.get("family") == "observation"]
    w("static inline int roostRecordIsObservation(RoostRecord r) {")
    if obs:
        w("  return " + " || ".join(f"r == ROOST_REC_{n}" for n in obs) + ";")
    else:
        w("  (void)r; return 0;")
    w("}")
    w("")
    w("static inline const char *roostFieldName(RoostRecord r, uint8_t idx) {")
    w("  if (r >= ROOST_REC_COUNT || idx >= kRoostRecordFieldCount[r]) return \"\";")
    w("  return kRoostRecordFields[r][idx];")
    w("}")
    w("")
    w("// A device's mask must include every required field. A device that cannot")
    w("// populate one cannot emit the record type at all, so this is checked once")
    w("// at startup rather than per row.")
    w("static inline int roostMaskSatisfiesRequired(RoostRecord r, RoostFieldMask mask) {")
    w("  if (r >= ROOST_REC_COUNT) return 0;")
    w("  return (mask & kRoostRecordRequired[r]) == kRoostRecordRequired[r];")
    w("}")
    w("")
    w("// Writes the CSV header for the caller's declared subset, in canonical")
    w("// order. Returns bytes written excluding the terminator, or 0 if it did")
    w("// not fit. The caller must not write a partial header,")
    w("// since a truncated header is worse than none.")
    w("static inline size_t roostHeader(char *out, size_t cap, RoostRecord r,")
    w("                                RoostFieldMask mask) {")
    w("  if (!out || !cap || r >= ROOST_REC_COUNT) return 0;")
    w("  size_t n = 0;")
    w("  const uint8_t count = kRoostRecordFieldCount[r];")
    w("  for (uint8_t i = 0; i < count; i++) {")
    w("    if (!(mask & ROOST_F(i))) continue;")
    w("    const char *name = kRoostRecordFields[r][i];")
    w("    const size_t len = strlen(name);")
    w("    const size_t need = len + (n ? 1u : 0u);")
    w("    // Leave the buffer empty rather than partially filled: a caller that")
    w("    // ignores the 0 return must not find a plausible-looking short header.")
    w("    if (n + need + 1u > cap) { out[0] = '\\0'; return 0; }")
    w("    if (n) out[n++] = ',';")
    w("    memcpy(out + n, name, len);")
    w("    n += len;")
    w("  }")
    w("  out[n] = '\\0';")
    w("  return n;")
    w("}")
    w("")
    # Enum restrictions, if any record declares them.
    restricted = [(r, f) for r in records.values() for f in sorted(r.get("restrict", {}))]
    if restricted:
        w("// A shared vocabulary can be legal on one record type and nonsense on")
        w("// another: detection_method covers both radios, but ble_oui on a Wi-Fi")
        w("// row means nothing. Check before writing; the shared C enum cannot")
        w("// express the restriction on its own.")
        w("static inline int roostValueAllowed(RoostRecord r, uint8_t fieldIdx, int value) {")
        w("  if (value < 0 || value >= 64) return 0;")
        w("  const uint64_t bit = (uint64_t)1u << value;")
        w("  switch (r) {")
        for rec in records.values():
            rr = rec.get("restrict", {})
            if not rr:
                continue
            rn = _ident(rec["name"])
            w(f"    case ROOST_REC_{rn}:")
            w("      switch (fieldIdx) {")
            for fname in sorted(rr):
                w(f"        case ROOST_{rn}_{_ident(fname)}:")
                w(f"          return (ROOST_{rn}_{_ident(fname)}_ALLOWED & bit) != 0;")
            w("        default: return 1;")
            w("      }")
        w("    default: return 1;")
        w("  }")
        w("}")
        w("")

    # ---- Capture components ------------------------------------------------
    # The board declares its components the way it declares its capabilities:
    # once, at compile time, in board config. Everything the manifest says about
    # them is rendered from that declaration, so "this radio cannot reach 5 GHz"
    # is stated once and checked rather than hand-typed into a JSON writer.
    comp_key = next((k for sec in sections for k in sec["keys"]
                     if k["type"] == "struct_list"), None)
    if comp_key is not None:
        items = comp_key["items"]
        names = [i["name"] for i in items]
        xargs = ", ".join(["ident"] + names)
        band_enum = next((i["enum"] for i in items
                          if i["type"] == "list" and i.get("enum")), None)
        bands = enums[band_enum]["values"] if band_enum else []

        w("// --------------------------------------------------------------------")
        w("// Capture components")
        w("//")
        w("// cap_component on every row names one of these. The values are")
        w("// device-scoped rather than a fleet-wide vocabulary, because a role name")
        w("// cannot distinguish two radios serving the same band. Declared in")
        w("// board config so the manifest is rendered from it rather than restated.")
        w("// --------------------------------------------------------------------")
        w("")
        if bands:
            w("// Band-reach flags. Immutable per component, and not the same thing as")
            w("// the channels it is currently tuned to, which are runtime.")
            for i, v in enumerate(bands):
                w(f"#define ROOST_BAND_REACH_{_ident(v['name'])} (1u << {i})")
            w("")
        w("// The board supplies this list, one X() per physical component:")
        w("//")
        w("//   #define ROOST_COMPONENTS(X)")
        w(f"//     X({xargs})")
        w("//     ...")
        w("//")
        w("// `ident` is a C identifier the row writers use, so a component named on a")
        w("// row cannot disagree with the declaration through a typo.")
        w("#if !defined(ROOST_COMPONENTS)")
        w("#error \"ROOST_COMPONENTS is undefined. Declare this board's capture components. \"\\")
        w("       \"There is no safe default: a device that cannot say what it is made of \"\\")
        w("       \"cannot say which part produced a row.\"")
        w("#endif")
        w("")
        w("typedef enum {")
        w(f"#define ROOST_X({xargs}) ROOST_COMP_##ident,")
        w("  ROOST_COMPONENTS(ROOST_X)")
        w("#undef ROOST_X")
        w("  ROOST_COMPONENT_COUNT")
        w("} RoostComponent;")
        w("")

        accessors = [
            ("roostComponentId", "const char *", "id", '""'),
            ("roostComponentKind", "RoostComponentKind", "kind",
             "ROOST_COMPONENT_KIND_SYSTEM"),
            ("roostComponentPart", "const char *", "part", "0"),
        ]
        for fn, ctype, item, fallback in accessors:
            if item not in names:
                continue
            cell = (f"ROOST_COMPONENT_KIND_##{item}" if item == "kind" else item)
            ptr = ctype.endswith("*")
            decl = "static const char *const v[] = {" if ptr else f"  static const {ctype} v[] = {{"
            w(f"static inline {ctype}{'' if ptr else ' '}{fn}(RoostComponent c) {{")
            w("  " + decl if ptr else decl)
            w(f"#define ROOST_X({xargs}) {cell},")
            w("    ROOST_COMPONENTS(ROOST_X)")
            w("#undef ROOST_X")
            w("  };")
            w(f"  return (c < ROOST_COMPONENT_COUNT) ? v[c] : {fallback};")
            w("}")
            w("")
        if bands:
            w("static inline unsigned roostComponentBandMask(RoostComponent c) {")
            w("  static const unsigned v[] = {")
            w(f"#define ROOST_X({xargs}) (unsigned)(bands),")
            w("    ROOST_COMPONENTS(ROOST_X)")
            w("#undef ROOST_X")
            w("  };")
            w("  return (c < ROOST_COMPONENT_COUNT) ? v[c] : 0u;")
            w("}")
            w("")

        w("// Ids must be present and distinct. cap_component on a row resolves")
        w("// against this list, so two components sharing an id make every row")
        w("// naming it ambiguous, which is the question the field exists to answer.")
        w("// The preprocessor cannot compare string literals, so this is the one")
        w("// component check that happens at boot rather than at build. Call it once;")
        w("// a failure is a device_event, not a silent start.")
        w("static inline int roostComponentsValid(void) {")
        w("  for (int i = 0; i < ROOST_COMPONENT_COUNT; i++) {")
        w("    const char *a = roostComponentId((RoostComponent)i);")
        w("    if (!a || !a[0]) return 0;")
        w("    for (int j = i + 1; j < ROOST_COMPONENT_COUNT; j++) {")
        w("      const char *b = roostComponentId((RoostComponent)j);")
        w("      if (b && strcmp(a, b) == 0) return 0;")
        w("    }")
        w("  }")
        w("  return 1;")
        w("}")
        w("")
        w(f"// Renders the manifest's \"{comp_key['name']}\" array. Nothing hand-written.")
        w("static inline size_t roostManifestComponents(char *out, size_t cap) {")
        w("  if (!out || !cap) return 0;")
        w("  size_t o = 0;")
        w("#define ROOST_CPUT(s)                                  \\")
        w("  do {                                                 \\")
        w("    const char *s_ = (s);                              \\")
        w("    const size_t l_ = strlen(s_);                      \\")
        w("    if (o + l_ + 1 > cap) { out[0] = '\\0'; return 0; } \\")
        w("    memcpy(out + o, s_, l_);                           \\")
        w("    o += l_;                                           \\")
        w("  } while (0)")
        w("  ROOST_CPUT(\"[\");")
        w("  for (int i = 0; i < ROOST_COMPONENT_COUNT; i++) {")
        w("    const RoostComponent c = (RoostComponent)i;")
        w("    if (i) ROOST_CPUT(\",\");")
        w("    ROOST_CPUT(\"{\\\"id\\\":\\\"\");")
        w("    ROOST_CPUT(roostComponentId(c));")
        w("    ROOST_CPUT(\"\\\",\\\"kind\\\":\\\"\");")
        w("    ROOST_CPUT(kRoostComponentKind[roostComponentKind(c)]);")
        w("    ROOST_CPUT(\"\\\",\\\"part\\\":\");")
        w("    if (roostComponentPart(c) && roostComponentPart(c)[0]) {")
        w("      ROOST_CPUT(\"\\\"\");")
        w("      ROOST_CPUT(roostComponentPart(c));")
        w("      ROOST_CPUT(\"\\\"\");")
        w("    } else {")
        w("      ROOST_CPUT(\"null\");")
        w("    }")
        if bands:
            w("    ROOST_CPUT(\",\\\"bands\\\":\");")
            w("    if (!roostComponentBandMask(c)) {")
            w("      ROOST_CPUT(\"null\");")
            w("    } else {")
            w("      int first = 1;")
            w("      ROOST_CPUT(\"[\");")
            for v in bands:
                w(f"      if (roostComponentBandMask(c) & ROOST_BAND_REACH_{_ident(v['name'])}) {{")
                w("        if (!first) ROOST_CPUT(\",\");")
                w(f"        ROOST_CPUT(\"\\\"{v['name']}\\\"\");")
                w("        first = 0;")
                w("      }")
            w("      ROOST_CPUT(\"]\");")
            w("    }")
        w("    ROOST_CPUT(\"}\");")
        w("  }")
        w("  ROOST_CPUT(\"]\");")
        w("#undef ROOST_CPUT")
        w("  out[o] = '\\0';")
        w("  return o;")
        w("}")
        w("")

    # ---- Row assembly ------------------------------------------------------
    # One entry point for building a row. Ordering is not a convention the writer
    # is asked to honour: a setter advances a cursor and emits empties for the
    # declared columns it skips, so writing a column the row has already passed
    # is refused rather than misplaced (docs/design_spec.md section 6.3).
    w("// --------------------------------------------------------------------")
    w("// Row assembly")
    w("//")
    w("// The single entry point for writing a row. It owns canonical order,")
    w("// RFC 4180 quoting, MAC case, float precision, enum restriction, and the")
    w("// required-field check, so none of those is a rule a device is trusted to")
    w("// follow. Adding a field to a record type means adding one setter call,")
    w("// not editing a format string on every device.")
    w("// --------------------------------------------------------------------")
    w("")
    w("typedef enum {")
    for cls in ("TEXT", "MAC", "HEX", "ENUM", "BOOL", "UINT", "INT", "FLOAT"):
        w(f"  ROOST_FT_{cls},")
    w("} RoostFieldType;")
    w("")
    for r in records.values():
        rn = _ident(r["name"])
        types = [TYPE_CLASS[fields[f]["type"]] for f in r["fields"]]
        precs = [int(fields[f].get("precision", 0)) for f in r["fields"]]
        w(f"static const uint8_t kRoost{rn.title().replace('_','')}FieldType[] = {{")
        w("  " + ", ".join(f"ROOST_FT_{t}" for t in types) + "};")
        if any(precs):
            w(f"static const uint8_t kRoost{rn.title().replace('_','')}FieldPrec[] = {{")
            w("  " + ", ".join(str(x) for x in precs) + "};")
    w("")
    # Dispatch tables so the row functions are generic rather than per record.
    w("static const uint8_t *const kRoostFieldType[] = {")
    for r in records.values():
        w(f"  kRoost{_ident(r['name']).title().replace('_','')}FieldType,")
    w("};")
    w("static const uint8_t *const kRoostFieldPrec[] = {")
    for r in records.values():
        rn = _ident(r["name"]).title().replace("_", "")
        has = any(int(fields[f].get("precision", 0)) for f in r["fields"])
        w(f"  kRoost{rn}FieldPrec," if has else "  0,")
    w("};")
    w("")
    # Enum-value name lookup by (record, field). The per-vocabulary tables above
    # are indexed by vocabulary; the row builder needs to go the other way, from
    # a column to the strings that column may carry.
    for r in records.values():
        rn = _ident(r["name"]).title().replace("_", "")
        cells = []
        for fname in r["fields"]:
            f = fields[fname]
            cells.append(f"kRoost{_ident(f['enum']).title().replace('_','')}"
                         if f["type"] == "enum" else "0")
        w(f"static const char *const *const kRoost{rn}EnumNames[] = {{")
        w("  " + ", ".join(cells) + "};")
        counts = []
        for fname in r["fields"]:
            f = fields[fname]
            counts.append(str(len(enums[f["enum"]]["values"])) if f["type"] == "enum" else "0")
        w(f"static const uint8_t kRoost{rn}EnumCount[] = {{")
        w("  " + ", ".join(counts) + "};")
    w("")
    w("static const char *const *const *const kRoostEnumNames[] = {")
    for r in records.values():
        w(f"  kRoost{_ident(r['name']).title().replace('_','')}EnumNames,")
    w("};")
    w("static const uint8_t *const kRoostEnumCount[] = {")
    for r in records.values():
        w(f"  kRoost{_ident(r['name']).title().replace('_','')}EnumCount,")
    w("};")
    w("")
    w("// The wire spelling of an enum value on a given column, or null if the")
    w("// column is not an enum or the value is outside its vocabulary.")
    w("static inline const char *roostEnumName(RoostRecord r, uint8_t idx, int v) {")
    w("  if (r >= ROOST_REC_COUNT || idx >= roostRecordFieldCount(r)) return 0;")
    w("  const char *const *names = kRoostEnumNames[r][idx];")
    w("  if (!names || v < 0 || v >= (int)kRoostEnumCount[r][idx]) return 0;")
    w("  return names[v];")
    w("}")
    w("")

    w("static inline RoostFieldType roostFieldTypeOf(RoostRecord r, uint8_t i) {")
    w("  return (RoostFieldType)kRoostFieldType[r][i];")
    w("}")
    w("")
    w("// A row under construction. Holds no allocation and copies nothing it")
    w("// does not own; the caller supplies the buffer and keeps it alive.")
    w("typedef struct {")
    w("  char *out;")
    w("  size_t cap;")
    w("  size_t n;")
    w("  RoostRecord record;")
    w("  RoostFieldMask columns;  // what this file declared")
    w("  RoostFieldMask written;  // what has been set so far")
    w("  uint8_t next;            // next declared column awaiting emission")
    w("  uint16_t fields;         // declared columns emitted, empty ones included")
    w("  uint16_t unknownEnums;   // names roostRowSetEnumByName could not resolve")
    w("  int ok;                  // cleared by the first refusal; never recovers")
    w("} RoostRow;")
    w("")
    w("static inline void roostRowBegin(RoostRow *w_, char *out, size_t cap,")
    w("                                 RoostRecord r, RoostFieldMask columns) {")
    w("  w_->out = out; w_->cap = cap; w_->n = 0;")
    w("  w_->record = r; w_->columns = columns; w_->written = 0;")
    w("  w_->fields = 0; w_->unknownEnums = 0;")
    w("  w_->next = 0;")
    w("  w_->ok = (out && cap && r < ROOST_REC_COUNT) ? 1 : 0;")
    w("  if (w_->ok) out[0] = '\\0';")
    w("}")
    w("")
    w("static inline int roostRowRaw_(RoostRow *w_, const char *s, size_t len) {")
    w("  if (!w_->ok) return 0;")
    w("  if (w_->n + len + 1 > w_->cap) { w_->ok = 0; return 0; }")
    w("  memcpy(w_->out + w_->n, s, len);")
    w("  w_->n += len;")
    w("  w_->out[w_->n] = '\\0';")
    w("  return 1;")
    w("}")
    w("")
    w("// Advances to `idx`, emitting a separator per declared column passed and")
    w("// leaving each of them empty. Refuses to go backwards: a column the row has")
    w("// already emitted cannot be filled in afterwards, which is what makes")
    w("// canonical order a property of the API rather than a rule to remember.")
    w("static inline int roostRowSeek_(RoostRow *w_, uint8_t idx) {")
    w("  if (!w_->ok) return 0;")
    w("  if (idx >= roostRecordFieldCount(w_->record)) { w_->ok = 0; return 0; }")
    w("  if (!(w_->columns & ROOST_F(idx))) return 0;  // not declared: benign no-op")
    w("  if (idx < w_->next) { w_->ok = 0; return 0; }  // out of order")
    w("  for (uint8_t i = w_->next; i <= idx; i++) {")
    w("    if (!(w_->columns & ROOST_F(i))) continue;")
    w("    // Counted, not measured. Testing w_->n here would ask whether any")
    w("    // bytes have been written, a different question from whether any field")
    w("    // has been emitted precisely when the leading columns are empty, and")
    w("    // every later value lands one column left.")
    w("    if (w_->fields++ && !roostRowRaw_(w_, \",\", 1)) return 0;")
    w("  }")
    w("  w_->next = (uint8_t)(idx + 1);")
    w("  w_->written |= ROOST_F(idx);")
    w("  return 1;")
    w("}")
    w("")
    w("static inline int roostRowCheck_(RoostRow *w_, uint8_t idx, RoostFieldType t) {")
    w("  if (!w_->ok) return 0;")
    w("  if (idx >= roostRecordFieldCount(w_->record)) { w_->ok = 0; return 0; }")
    w("  // A setter of the wrong class for the field is a coding error, not a")
    w("  // value problem: writing an int into an ssid column would produce a row")
    w("  // that parses and means nothing.")
    w("  if (roostFieldTypeOf(w_->record, idx) != t) { w_->ok = 0; return 0; }")
    w("  return 1;")
    w("}")
    w("")
    w("// Length in bytes of the well-formed, shortest-form UTF-8 sequence at `p`,")
    w("// or 0 if there is not one. Rejecting overlongs, encoded surrogates,")
    w("// anything above U+10FFFF and truncation is what makes ill-formedness")
    w("// decidable a byte at a time, which is what the text encoding escapes on")
    w("// (docs/design_spec.md section 3.4).")
    w("static inline size_t roostUtf8SeqLen_(const char *p, size_t avail) {")
    w("  const unsigned char a = (unsigned char)p[0];")
    w("  size_t need;")
    w("  unsigned long cp;")
    w("  // 0xC0 and 0xC1 are excluded by starting at 0xC2: they can only ever")
    w("  // introduce an overlong two-byte form.")
    w("  if (a >= 0xC2 && a <= 0xDF) { need = 2; cp = (unsigned long)(a & 0x1F); }")
    w("  else if (a >= 0xE0 && a <= 0xEF) { need = 3; cp = (unsigned long)(a & 0x0F); }")
    w("  else if (a >= 0xF0 && a <= 0xF4) { need = 4; cp = (unsigned long)(a & 0x07); }")
    w("  else return 0;  // 0x80-0xC1 and 0xF5-0xFF are never a lead byte")
    w("  if (avail < need) return 0;")
    w("  for (size_t k = 1; k < need; k++) {")
    w("    const unsigned char b = (unsigned char)p[k];")
    w("    if ((b & 0xC0) != 0x80) return 0;")
    w("    cp = (cp << 6) | (unsigned long)(b & 0x3F);")
    w("  }")
    w("  if (need == 3 && cp < 0x800UL) return 0;")
    w("  if (need == 4 && cp < 0x10000UL) return 0;")
    w("  if (cp >= 0xD800UL && cp <= 0xDFFFUL) return 0;")
    w("  if (cp > 0x10FFFFUL) return 0;")
    w("  return need;")
    w("}")
    w("")
    w("// Text, RFC 4180. Quotes only when it must, doubles embedded quotes,")
    w("// and encodes rather than emits anything that would break a reader, so the")
    w("// original bytes survive a round trip.")
    w("//")
    w("// Takes a length. The NUL-terminated roostRowSetText below cannot express a")
    w("// value containing 0x00, and an SSID is 0-32 arbitrary octets while a BLE")
    w("// device name comes from a length-delimited AD structure. For anything")
    w("// attacker-controlled this is the correct entry point; the short form is a")
    w("// silent truncation.")
    w("static inline int roostRowSetTextN(RoostRow *w_, uint8_t idx, const char *v,")
    w("                                   size_t n) {")
    w("  if (!roostRowCheck_(w_, idx, ROOST_FT_TEXT)) return 0;")
    w("  if (!roostRowSeek_(w_, idx)) return 0;")
    w("  if (!v || !n) return 1;")
    w("  static const char kHexDigits[] = \"0123456789abcdef\";")
    w("  int quote = 0;")
    w("  for (size_t i = 0; i < n; i++) {")
    w("    if (v[i] == ',' || v[i] == '\"') { quote = 1; break; }")
    w("  }")
    w("  if (v[0] == ' ' || v[n - 1] == ' ') quote = 1;")
    w("  if (quote && !roostRowRaw_(w_, \"\\\"\", 1)) return 0;")
    w("  for (size_t i = 0; i < n;) {")
    w("    const unsigned char c = (unsigned char)v[i];")
    w("    // A control byte is encoded, never emitted raw and never dropped.")
    w("    // Reversible, so the original text is recoverable, while")
    w("    // the file stays usable by every line-oriented tool that touches it.")
    w("    if (c < 0x20 || c == 0x7F) {")
    w("      const char esc[4] = {'\\\\', 'x', kHexDigits[c >> 4], kHexDigits[c & 0x0F]};")
    w("      if (!roostRowRaw_(w_, esc, 4)) return 0;")
    w("      i++;")
    w("      continue;")
    w("    }")
    w("    // The escape introducer itself, or the encoding would be ambiguous.")
    w("    if (c == '\\\\') {")
    w("      if (!roostRowRaw_(w_, \"\\\\\\\\\", 2)) return 0;")
    w("      i++;")
    w("      continue;")
    w("    }")
    w("    if (c < 0x80) {")
    w("      if (!roostRowRaw_(w_, v + i, 1)) return 0;")
    w("      if (c == '\"' && !roostRowRaw_(w_, \"\\\"\", 1)) return 0;")
    w("      i++;")
    w("      continue;")
    w("    }")
    w("    // High bytes are escaped on validity, not on range. Well-formed UTF-8")
    w("    // passes through whole, so a real SSID stays legible; anything else is")
    w("    // escaped one byte at a time and scanning resumes at the next byte,")
    w("    // which is what keeps the encoding byte-exact reversible.")
    w("    {")
    w("      const size_t seq = roostUtf8SeqLen_(v + i, n - i);")
    w("      if (seq) {")
    w("        if (!roostRowRaw_(w_, v + i, seq)) return 0;")
    w("        i += seq;")
    w("        continue;")
    w("      }")
    w("      const char esc[4] = {'\\\\', 'x', kHexDigits[c >> 4], kHexDigits[c & 0x0F]};")
    w("      if (!roostRowRaw_(w_, esc, 4)) return 0;")
    w("      i++;")
    w("    }")
    w("  }")
    w("  if (quote && !roostRowRaw_(w_, \"\\\"\", 1)) return 0;")
    w("  return 1;")
    w("}")
    w("")
    w("// For values that are C strings by construction. Truncates at the first")
    w("// 0x00; use roostRowSetTextN where the value came off the air.")
    w("static inline int roostRowSetText(RoostRow *w_, uint8_t idx, const char *v) {")
    w("  if (!v) return roostRowSetTextN(w_, idx, v, 0);")
    w("  return roostRowSetTextN(w_, idx, v, strlen(v));")
    w("}")
    w("")
    w("static inline int roostRowSetInt(RoostRow *w_, uint8_t idx, long v) {")
    w("  if (!roostRowCheck_(w_, idx, ROOST_FT_INT)) return 0;")
    w("  if (!roostRowSeek_(w_, idx)) return 0;")
    w("  char b[24]; snprintf(b, sizeof(b), \"%ld\", v);")
    w("  return roostRowRaw_(w_, b, strlen(b));")
    w("}")
    w("")
    w("static inline int roostRowSetUInt(RoostRow *w_, uint8_t idx, unsigned long v) {")
    w("  if (!roostRowCheck_(w_, idx, ROOST_FT_UINT)) return 0;")
    w("  if (!roostRowSeek_(w_, idx)) return 0;")
    w("  char b[24]; snprintf(b, sizeof(b), \"%lu\", v);")
    w("  return roostRowRaw_(w_, b, strlen(b));")
    w("}")
    w("")
    w("static inline int roostRowSetBool(RoostRow *w_, uint8_t idx, int v) {")
    w("  if (!roostRowCheck_(w_, idx, ROOST_FT_BOOL)) return 0;")
    w("  if (!roostRowSeek_(w_, idx)) return 0;")
    w("  return roostRowRaw_(w_, v ? \"1\" : \"0\", 1);")
    w("}")
    w("")
    w("// Decimal places come from the field definition, never from the caller, so")
    w("// two devices cannot write a coordinate at different precision.")
    w("static inline int roostRowSetFloat(RoostRow *w_, uint8_t idx, double v) {")
    w("  if (!roostRowCheck_(w_, idx, ROOST_FT_FLOAT)) return 0;")
    w("  if (!roostRowSeek_(w_, idx)) return 0;")
    w("  const uint8_t *pt = kRoostFieldPrec[w_->record];")
    w("  char b[40]; snprintf(b, sizeof(b), \"%.*f\", pt ? (int)pt[idx] : 6, v);")
    w("  return roostRowRaw_(w_, b, strlen(b));")
    w("}")
    w("")
    w("// Lowercase, colon-separated, normalized here rather than at ingest.")
    w("static inline int roostRowSetMac(RoostRow *w_, uint8_t idx, const uint8_t *m) {")
    w("  if (!roostRowCheck_(w_, idx, ROOST_FT_MAC)) return 0;")
    w("  if (!roostRowSeek_(w_, idx)) return 0;")
    w("  if (!m) return 1;")
    w("  char b[18];")
    w("  snprintf(b, sizeof(b), \"%02x:%02x:%02x:%02x:%02x:%02x\",")
    w("           m[0], m[1], m[2], m[3], m[4], m[5]);")
    w("  return roostRowRaw_(w_, b, 17);")
    w("}")
    w("")
    w("static inline int roostRowSetHex(RoostRow *w_, uint8_t idx,")
    w("                                 const uint8_t *b, size_t len) {")
    w("  if (!roostRowCheck_(w_, idx, ROOST_FT_HEX)) return 0;")
    w("  if (!roostRowSeek_(w_, idx)) return 0;")
    w("  if (!b || !len) return 1;")
    w("  static const char kHex[] = \"0123456789abcdef\";")
    w("  for (size_t i = 0; i < len; i++) {")
    w("    const char pair[2] = {kHex[b[i] >> 4], kHex[b[i] & 0x0F]};")
    w("    if (!roostRowRaw_(w_, pair, 2)) return 0;")
    w("  }")
    w("  return 1;")
    w("}")
    w("")
    w("// Enum values go through the per-record restriction, so a BLE")
    w("// detection method cannot land on a Wi-Fi row even though both share one")
    w("// vocabulary.")
    w("static inline int roostRowSetEnum(RoostRow *w_, uint8_t idx, int value) {")
    w("  if (!roostRowCheck_(w_, idx, ROOST_FT_ENUM)) return 0;")
    w("  if (!roostValueAllowed(w_->record, idx, value)) { w_->ok = 0; return 0; }")
    w("  if (!roostRowSeek_(w_, idx)) return 0;")
    w("  const char *s = roostEnumName(w_->record, idx, value);")
    w("  return s ? roostRowRaw_(w_, s, strlen(s)) : 0;")
    w("}")
    w("")
    w("// Sets an enum column from the wire spelling of its value.")
    w("//")
    w("// Devices arrive at this contract with producers that already return name")
    w("// strings: a frame subtype, a detection method, a baseband format. Those")
    w("// producers usually have other consumers, a display or a console, so")
    w("// rewriting them to return roost enums means either duplicating the table")
    w("// or breaking something. Resolving the name against the registry\'s own")
    w("// vocabulary keeps one table, and it is the generated one.")
    w("//")
    w("// This is a first-class setter rather than something each device writes,")
    w("// because every device has the same problem, and one hand-written reverse")
    w("// lookup per device is one chance per device to differ on what an")
    w("// unresolvable name does.")
    w("//")
    w("// An unresolvable name leaves the column empty and counts itself in")
    w("// w_->unknownEnums; it does not void the row. A name is device code and")
    w("// the registry cannot check it at compile time, so this is drift that will")
    w("// happen, and the proportionate response is to lose the field rather than")
    w("// the observation. If the column is required, leaving it unwritten makes")
    w("// roostRowFinish() refuse anyway, which is the right escalation and needs")
    w("// no special case here.")
    w("//")
    w("// Read w_->unknownEnums after finishing and surface it. A silent empty")
    w("// column is ambiguous: it reads as 'nothing to record' when the truth is")
    w("// 'a producer and the registry have drifted'.")
    w("static inline int roostRowSetEnumByName(RoostRow *w_, uint8_t idx,")
    w("                                        const char *name) {")
    w("  if (!roostRowCheck_(w_, idx, ROOST_FT_ENUM)) return 0;")
    w("  // Not declared on this build: a benign no-op, and not drift. Counting")
    w("  // it would make every device that declines a column look broken.")
    w("  if (!(w_->columns & ROOST_F(idx))) return 0;")
    w("  // No value is not drift. An empty name is a producer saying the frame")
    w("  // carried nothing for this column: a legacy advertisement has no")
    w("  // secondary PHY, a fix seen without a GSA sentence has no fix type. That")
    w("  // is what an optional enum column is for. Counting it would raise the")
    w("  // drift counter on every such row, leaving a warning that is always on")
    w("  // and so carries no signal.")
    w("  if (!name || !name[0]) return 0;")
    w("  for (int v = 0;; v++) {")
    w("    const char *candidate = roostEnumName(w_->record, idx, v);")
    w("    if (!candidate) break;")
    w("    if (strcmp(candidate, name) == 0) return roostRowSetEnum(w_, idx, v);")
    w("  }")
    w("  // A non-empty name the registry does not carry. This is drift.")
    w("  w_->unknownEnums++;")
    w("  return 0;")
    w("}")
    w("")
    w("// Pads out the declared columns the caller never set, then checks that")
    w("// every required one was populated. Returns 0 and empties the buffer on any")
    w("// refusal: a partial row is worse than no row, because it parses.")
    w("static inline size_t roostRowFinish(RoostRow *w_) {")
    w("  if (!w_->ok) { if (w_->out && w_->cap) w_->out[0] = '\\0'; return 0; }")
    w("  const uint8_t count = roostRecordFieldCount(w_->record);")
    w("  for (uint8_t i = w_->next; i < count; i++) {")
    w("    if (!(w_->columns & ROOST_F(i))) continue;")
    w("    if (w_->fields++ && !roostRowRaw_(w_, \",\", 1)) { w_->out[0] = '\\0'; return 0; }")
    w("  }")
    w("  const RoostFieldMask req = kRoostRecordRequired[w_->record] & w_->columns;")
    w("  if ((w_->written & req) != req) { w_->out[0] = '\\0'; return 0; }")
    w("  return w_->n;")
    w("}")
    w("")

    w("// What a raw capture's record timestamps mean. A property of the")
    w("// file, not a report of whether the session's clock anchored.")
    w("typedef enum {")
    for i, name in enumerate(TIMEBASE):
        w(f"  ROOST_TIMEBASE_{name.upper()} = {i},")
    w("  ROOST_TIMEBASE_COUNT,")
    w("} RoostTimebase;")
    w("")
    w("static const char *const kRoostTimebase[ROOST_TIMEBASE_COUNT] = {")
    for name in TIMEBASE:
        w(f"  \"{name}\",")
    w("};")
    w("")

    w("// The pcap declaration. Not a record type: it has no columns, and its")
    w("// shape (linktype, snaplen) is a capture-format fact rather than a")
    w("// registry one. Generated anyway so the manifest's files array has one")
    w("// producer rather than one generated half and one hand-written half.")
    w("static inline size_t roostManifestPcapFile(char *out, size_t cap,")
    w("                                           const char *name,")
    w("                                           uint16_t linktype,")
    w("                                           uint16_t snaplen,")
    w("                                           RoostTimebase timebase) {")
    w("  if (!out || !cap || !name) return 0;")
    w("  if ((unsigned)timebase >= (unsigned)ROOST_TIMEBASE_COUNT) return 0;")
    w("  const int n = snprintf(out, cap,")
    w("      \"{\\\"record\\\":\\\"frames\\\",\\\"format\\\":\\\"pcap\\\",\\\"name\\\":\\\"%s\\\",\"")
    w("      \"\\\"linktype\\\":%u,\\\"snaplen\\\":%u,\\\"timebase\\\":\\\"%s\\\"}\",")
    w("      name, (unsigned)linktype, (unsigned)snaplen,")
    w("      kRoostTimebase[timebase]);")
    w("  if (n <= 0 || (size_t)n >= cap) { out[0] = '\\0'; return 0; }")
    w("  return (size_t)n;")
    w("}")
    w("")
    w("// The filename a record type's file takes inside a session directory.")
    w("// Version rides in the name so a file separated from its manifest is")
    w("// still interpretable.")
    w("//")
    w("// Returns the bare name, not a path. The session directory is the caller's")
    w("// business, since only it knows the session number and mount point.")
    w("static inline size_t roostFileName(char *out, size_t cap, RoostRecord r) {")
    w("  if (!out || !cap || r >= ROOST_REC_COUNT) return 0;")
    w("  const char *name = kRoostRecordName[r];")
    w("  const uint16_t ver = kRoostRecordVersion[r];")
    w("  size_t n = 0;")
    w("  const size_t len = strlen(name);")
    w("  if (len + 8u > cap) return 0;")
    w("  memcpy(out, name, len); n = len;")
    w("  out[n++] = '.'; out[n++] = 'v';")
    w("  if (ver >= 100) out[n++] = (char)('0' + (ver / 100) % 10);")
    w("  if (ver >= 10)  out[n++] = (char)('0' + (ver / 10) % 10);")
    w("  out[n++] = (char)('0' + ver % 10);")
    w("  memcpy(out + n, \".csv\", 4); n += 4;")
    w("  out[n] = '\\0';")
    w("  return n;")
    w("}")
    w("")

    # ---- manifest --------------------------------------------------------
    w("// " + "-" * 74)
    w("// Session manifest.")
    w("//")
    w("// The contract half is rendered from the registry and the caller's masks,")
    w("// never hand-written. A hand-written copy of a derivable fact is exactly")
    w("// the drift this file prevents, so there is no way to state a")
    w("// column list here that disagrees with the one roostHeader() emits.")
    w("//")
    w("// The provenance half is the caller's to fill, but its key names come from")
    w("// the constants below so two devices cannot spell one fact two ways.")
    w("// " + "-" * 74)
    w("")
    w(f"#define ROOST_MANIFEST_VERSION {reg['manifest'].get('manifest_version', 1)}")
    w("")
    w("// Which revision of the text encoding roostRowSetTextN implements. Folded")
    w("// into the registry hash, so it is stated here for a test to assert against")
    w("// rather than for a device to publish.")
    w(f"#define ROOST_TEXT_ENCODING_REV {reg['manifest'].get('text_encoding_rev', 1)}")
    w("")
    for s in sections:
        w(f"// {s['name']} ({s['tier']} tier)")
        for k in s["keys"]:
            w(f'#define ROOST_MK_{_ident(k["name"])} "{k["name"]}"')
        w("")

    w("// What a device declares about one file it is writing.")
    w("//")
    w("// Three states, and telling them apart is what the contract guarantees:")
    w("//   in `columns`                 the file carries this column. An empty")
    w("//                                value in a row means uncaptured.")
    w("//   in `capable`, not `columns`  the hardware can measure it but this")
    w("//                                configuration does not record it.")
    w("//   in neither                   this hardware cannot measure it at all,")
    w("//                                so the field is uncapturable.")
    w("//")
    w("// `capable` must be a superset of `columns`; roostFileDeclValid() checks it.")
    w("typedef struct {")
    w("  RoostRecord    record;")
    w("  RoostFieldMask columns;")
    w("  RoostFieldMask capable;")
    w("} RoostFileDecl;")
    w("")
    # The set of files a build emits is a pure function of its capability
    # macros, so it is generated rather than restated per device. Otherwise every
    # device writes and maintains the same #ifdef ladder.
    w("// Fills `out` with one declaration per record type this build emits, in")
    w("// registry order, and returns how many. Derived entirely from the")
    w("// capability macros: a board declares capabilities, never a file list.")
    w("//")
    w("// Returns 0 if `cap` is too small, rather than a truncated set that would")
    w("// silently omit a record type from the session and its manifest.")
    w("static inline size_t roostDeclaredFiles(RoostFileDecl *out, size_t cap) {")
    w("  size_t n = 0;")
    w("  // A build emitting no record type at all, with no storage fitted, reaches")
    w("  // neither parameter, and an unused one is an error under -Werror.")
    w("  (void)out; (void)cap;")
    for r in records.values():
        rn = r["name"].upper()
        w(f"#if ROOST_EMITS_{rn}")
        w("  if (n >= cap) return 0;")
        w(f"  out[n].record  = ROOST_REC_{rn};")
        w(f"  out[n].columns = ROOST_{rn}_COLUMNS_MASK;")
        w(f"  out[n].capable = ROOST_{rn}_CAPABLE_MASK;")
        w("  n++;")
        w("#endif")
    w("  return n;")
    w("}")
    w("")
    w("// Upper bound for a caller's array, so a device need not count by hand.")
    w("#define ROOST_MAX_DECLARED_FILES ROOST_REC_COUNT")
    w("")
    w("static inline int roostFileDeclValid(const RoostFileDecl *d) {")
    w("  if (!d || d->record >= ROOST_REC_COUNT) return 0;")
    w("  if ((d->columns & d->capable) != d->columns) return 0;")
    w("  return roostMaskSatisfiesRequired(d->record, d->columns);")
    w("}")
    w("")
    w("// Renders the manifest's \"files\" array. Returns bytes written excluding")
    w("// the terminator, or 0 if it did not fit or a declaration was invalid. In")
    w("// which case the buffer is emptied, since a truncated manifest is worse")
    w("// than none.")
    w("//")
    w("// `raw` carries entries this generator cannot render from a record")
    w("// declaration, which today means a raw capture file: a pcap has no columns")
    w("// and no record version, so roostManifestPcapFile builds it and it is")
    w("// appended here. They go in the same array rather than beside it so")
    w("// a reader walks one list to find everything the session produced. Pass")
    w("// nullptr and 0 when there are none.")
    w("static inline size_t roostManifestFiles(char *out, size_t cap,")
    w("                                       const RoostFileDecl *files,")
    w("                                       size_t n,")
    w("                                       const char *const *raw,")
    w("                                       size_t nRaw) {")
    w("  if (!out || !cap) return 0;")
    w("  size_t o = 0;")
    w("#define ROOST_PUT(s)                                   \\")
    w("  do {                                                 \\")
    w("    const char *s_ = (s);                               \\")
    w("    const size_t l_ = strlen(s_);                       \\")
    w("    if (o + l_ + 1u > cap) { out[0] = '\\0'; return 0; } \\")
    w("    memcpy(out + o, s_, l_); o += l_;                   \\")
    w("  } while (0)")
    w("  ROOST_PUT(\"\\\"files\\\":[\");")
    w("  for (size_t i = 0; i < n; i++) {")
    w("    const RoostFileDecl *d = &files[i];")
    w("    if (!roostFileDeclValid(d)) { out[0] = '\\0'; return 0; }")
    w("    if (i) ROOST_PUT(\",\");")
    w("    ROOST_PUT(\"{\\\"record\\\":\\\"\");")
    w("    ROOST_PUT(roostRecordName(d->record));")
    w("    ROOST_PUT(\"\\\",\\\"version\\\":\");")
    w("    {")
    w("      char v[8]; size_t vi = 0; uint16_t ver = roostRecordVersion(d->record);")
    w("      if (ver >= 100) v[vi++] = (char)('0' + (ver / 100) % 10);")
    w("      if (ver >= 10)  v[vi++] = (char)('0' + (ver / 10) % 10);")
    w("      v[vi++] = (char)('0' + ver % 10); v[vi] = '\\0';")
    w("      ROOST_PUT(v);")
    w("    }")
    w("    ROOST_PUT(\",\\\"format\\\":\\\"csv\\\",\\\"name\\\":\\\"\");")
    w("    {")
    w("      char fn[64];")
    w("      if (!roostFileName(fn, sizeof(fn), d->record)) { out[0] = '\\0'; return 0; }")
    w("      ROOST_PUT(fn);")
    w("    }")
    w("    ROOST_PUT(\"\\\",\\\"columns\\\":[\");")
    w("    {")
    w("      int first = 1;")
    w("      for (uint8_t f = 0; f < roostRecordFieldCount(d->record); f++) {")
    w("        if (!(d->columns & ROOST_F(f))) continue;")
    w("        if (!first) ROOST_PUT(\",\");")
    w("        first = 0;")
    w("        ROOST_PUT(\"\\\"\");")
    w("        ROOST_PUT(roostFieldName(d->record, f));")
    w("        ROOST_PUT(\"\\\"\");")
    w("      }")
    w("    }")
    w("    // Capable-but-not-recorded. Empty is the common case and means the")
    w("    // configuration records everything the hardware can reach.")
    w("    ROOST_PUT(\"],\\\"capable_unrecorded\\\":[\");")
    w("    {")
    w("      int first = 1;")
    w("      const RoostFieldMask extra = d->capable & ~d->columns;")
    w("      for (uint8_t f = 0; f < roostRecordFieldCount(d->record); f++) {")
    w("        if (!(extra & ROOST_F(f))) continue;")
    w("        if (!first) ROOST_PUT(\",\");")
    w("        first = 0;")
    w("        ROOST_PUT(\"\\\"\");")
    w("        ROOST_PUT(roostFieldName(d->record, f));")
    w("        ROOST_PUT(\"\\\"\");")
    w("      }")
    w("    }")
    w("    ROOST_PUT(\"]}\");")
    w("  }")
    w("  for (size_t i = 0; i < nRaw; i++) {")
    w("    if (!raw || !raw[i] || !raw[i][0]) { out[0] = '\\0'; return 0; }")
    w("    if (n || i) ROOST_PUT(\",\");")
    w("    ROOST_PUT(raw[i]);")
    w("  }")
    w("  ROOST_PUT(\"]\");")
    w("#undef ROOST_PUT")
    w("  out[o] = '\\0';")
    w("  return o;")
    w("}")
    w("")
    return "\n".join(L)


def _emit_schema(reg, enums, sections):
    """JSON Schema for the pipeline to validate a manifest against.

    Generated rather than written, for the same reason the C header is: a
    hand-maintained schema is a second copy of the registry that drifts.
    """
    props = {
        "manifest_version": {"type": "integer"},
        "files": {
            "type": "array",
            "description": "Contract half. Derived from the registry and the "
                           "device's declared masks, never hand-written. Holds "
                           "every file the session produced, record-bearing or "
                           "not, so a reader has one list to walk rather than "
                           "one list plus a set of special cases.",
            "items": {
                # Two shapes, discriminated by `format`. A pcap has no columns
                # and no record version because it is not a record type: its
                # shape is a capture-format fact (linktype, snaplen), and
                # requiring the CSV keys of it would force a device to invent
                # values that mean nothing, or to declare the file somewhere the
                # schema does not accept.
                "oneOf": [
                    {
                        "type": "object",
                        "description": "A record-bearing CSV.",
                        "required": ["record", "version", "format", "name",
                                     "columns"],
                        "properties": {
                            "record": {"type": "string"},
                            "version": {"type": "integer"},
                            "format": {"type": "string", "enum": ["csv"]},
                            "name": {"type": "string"},
                            "columns": {
                                "type": "array", "items": {"type": "string"},
                                "description": "Columns this file carries. An "
                                               "empty value in a row means "
                                               "uncaptured.",
                            },
                            "capable_unrecorded": {
                                "type": "array", "items": {"type": "string"},
                                "description": "Fields the hardware can measure "
                                               "that this configuration does not "
                                               "record. A field in neither list "
                                               "is uncapturable.",
                            },
                        },
                        "additionalProperties": False,
                    },
                    {
                        "type": "object",
                        "description": "A raw capture file. Carries no rows, so "
                                       "it declares how to read the bytes "
                                       "instead of what the columns are.",
                        "required": ["record", "format", "name", "linktype",
                                     "snaplen", "timebase"],
                        "properties": {
                            "record": {"type": "string"},
                            "format": {"type": "string", "enum": ["pcap"]},
                            "name": {"type": "string"},
                            "linktype": {"type": "integer"},
                            "snaplen": {"type": "integer"},
                            "timebase": {
                                "type": "string", "enum": list(TIMEBASE),
                                "description": "What the record timestamps mean. "
                                               "'boot' is monotonic from power-on "
                                               "and is placed by the manifest's "
                                               "clock anchor, exactly as a row's "
                                               "uptime_ms is. 'utc' is "
                                               "seconds since the epoch for the "
                                               "whole file. A reader must not "
                                               "infer from this whether the "
                                               "session's clock anchored.",
                            },
                        },
                        "additionalProperties": False,
                    },
                ],
            },
        },
        # The one sanctioned escape hatch, and deliberately the only place the
        # schema stops being closed. A device may carry counters meaningful to
        # its own firmware and to no consumer, such as a per-radio error delta
        # that shows a correction is holding. The alternative to a namespaced
        # home for them is either a registry that grows a key per device or a
        # manifest that fails its own validator.
        #
        # NOT part of the contract. A pipeline may surface these but must never
        # depend on one: any key here may vanish on the next firmware build, and
        # nothing outside the device that wrote it defines what they mean.
        "device_diagnostics": {
            "type": "object",
            "description": "Device-specific counters. Outside the contract: "
                           "readers may display these but must not depend on "
                           "any key being present or keeping its meaning.",
            "additionalProperties": True,
        },
    }
    required = ["manifest_version", "files"]
    for s in sections:
        sub_props, sub_req = {}, []
        for k in s["keys"]:
            entry = {"description": k.get("description", "")}
            jt = _JSON_TYPE[k["type"]]
            # Every key is present in every manifest; a device that cannot supply
            # one writes null. That keeps a missing key unambiguously an error.
            entry["type"] = [jt, "null"]
            if k["type"] == "enum":
                entry["enum"] = [v["name"] for v in enums[k["enum"]]["values"]] + [None]
            elif k["type"] == "struct_list":
                item_props, item_req = {}, []
                for it in k["items"]:
                    sub = {"description": it.get("description", ""),
                           "type": [_JSON_TYPE[it["type"]], "null"]}
                    if it["type"] == "enum":
                        sub["enum"] = [v["name"]
                                       for v in enums[it["enum"]]["values"]] + [None]
                    elif it["type"] == "list" and it.get("enum"):
                        sub["items"] = {
                            "enum": [v["name"] for v in enums[it["enum"]]["values"]]
                        }
                    item_props[it["name"]] = sub
                    if it.get("required"):
                        item_req.append(it["name"])
                entry["items"] = {
                    "type": "object",
                    "required": item_req,
                    "properties": item_props,
                    "additionalProperties": False,
                }
            sub_props[k["name"]] = entry
            if k.get("required"):
                sub_req.append(k["name"])
        props[s["name"]] = {
            "type": "object",
            "description": f"{s.get('description', '').strip()} (tier: {s['tier']})",
            "required": sub_req,
            "properties": sub_props,
            "additionalProperties": False,
        }
        required.append(s["name"])

    return json.dumps({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "title": "Birdoscope ecosystem session manifest",
        "description": "GENERATED from registry/*.toml. Do not edit.",
        "x-registry-hash": _canonical(reg),
        "type": "object",
        "required": required,
        "properties": props,
        "additionalProperties": False,
    }, indent=2) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", type=Path, help="write the generated header here")
    ap.add_argument("--schema-out", type=Path,
                    help="write the manifest JSON Schema here")
    ap.add_argument("--check", action="store_true",
                    help="validate, and verify any named output matches")
    args = ap.parse_args()

    try:
        reg = _load()
        enums, fields, records, orphans, sections, caps = _validate(reg)
    except RegistryError as exc:
        print(f"registry error: {exc}", file=sys.stderr)
        return 1

    outputs = []
    if args.out:
        outputs.append((args.out, _emit(reg, enums, fields, records, sections, caps)))
    if args.schema_out:
        outputs.append((args.schema_out, _emit_schema(reg, enums, sections)))

    if orphans:
        print(f"note: {len(orphans)} field(s) defined but referenced by no record type: "
              f"{', '.join(orphans)}", file=sys.stderr)

    if args.check:
        # Live counts, with retired reported separately: a retired entry is not
        # part of what any device emits, so folding it into the totals would
        # overstate the contract surface.
        n_retired_f = sum(1 for f in reg["fields"].get("field", []) if f.get("retired"))
        n_retired_r = sum(1 for r in reg["records"].get("record", []) if r.get("retired"))
        retired = ""
        if n_retired_f or n_retired_r:
            retired = (f" (plus {n_retired_r} retired record version(s), "
                       f"{n_retired_f} retired field(s), read-only)")
        print(f"registry ok: {len(enums)} enums, {len(fields) - n_retired_f} fields, "
              f"{len(records)} record types, "
              f"{sum(len(s['keys']) for s in sections)} manifest keys, "
              f"hash {_canonical(reg)[:12]}{retired}")
        drift = False
        for path, text in outputs:
            if not path.exists():
                print(f"MISSING: {path} has never been generated.", file=sys.stderr)
                drift = True
            elif path.read_text() != text:
                print(f"DRIFT: {path} does not match the registry. Regenerate it.",
                      file=sys.stderr)
                drift = True
            else:
                print(f"matches registry: {path}")
        return 1 if drift else 0

    if not outputs:
        print(_emit(reg, enums, fields, records, sections, caps))
        return 0

    for path, text in outputs:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        print(f"wrote {path} ({len(text)} bytes, hash {_canonical(reg)[:12]})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
