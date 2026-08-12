// Copyright (c) 2026 Lone Crow Design, LLC
// SPDX-License-Identifier: MIT
//
// roost_manifest.h - the session manifest renderer.
//
// The manifest is what tells the pipeline how to read a capture instead of
// making it guess. Rendered here rather than per device, and rendered into a
// buffer rather than printed straight at the card: a card that fills mid-write
// would otherwise leave a truncated manifest, which is worse than none.
//
// So it renders here, once. A device supplies facts; it never spells a key,
// formats a timestamp, decides what null means, or wires a counter.
//
// Platform-free by construction, like roost_sdlog.h beside it: no Arduino, no
// filesystem, no time.h. The caller receives bytes and decides where they go.
//
// Contract: docs/design_spec.md section 7. The two halves are maintained
// differently: the `files` and `components` halves are rendered from the
// registry and this build's masks, so a declared column cannot disagree with the
// header the device wrote.

#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "roost_registry.h"
#include "roost_sdlog.h"
#include "roost_time.h"

#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------------
// COUNTERS
//
// Taken from the writer rather than assembled per device, so observations_written
// cannot become a session-wide row total and storage_errors cannot become a
// count of dropped rows. The only two figures a device passes are the two the
// writer cannot know.
// --------------------------------------------------------------------------

typedef struct {
  uint32_t observationsWritten;
  uint32_t observationsSuppressed;
  uint32_t observationsDropped;
  uint32_t fixesWritten;
  uint32_t storageErrors;
  uint32_t worstFlushMs;
} RoostSessionCounters;

// `suppressed` is observations a declared filter or dedup policy discarded, and
// `droppedBeforeWriter` is those lost upstream of the writer entirely: a full
// alert queue, a ring that desynced. Everything else the writer already knows.
static inline void roostSessionCounters(const RoostSdLog* g,
                                        RoostSessionCounters* c,
                                        uint32_t suppressed,
                                        uint32_t droppedBeforeWriter) {
  RoostSdStats st;
  roostSdGetStats(g, &st);
  memset(c, 0, sizeof(*c));

  c->observationsWritten = roostSdObservationsWritten(g);
  c->observationsSuppressed = suppressed;
  c->fixesWritten = roostSdRowsWritten(g, ROOST_REC_GPS_TRACK);
  c->storageErrors = st.storageErrors;
  c->worstFlushMs = st.worstFlushMs;

  // Rows the builder refused count as dropped: the observation was made and did
  // not reach the card, which is what this counter means. They are invisible in
  // the file by definition, so omitting them here makes a session of refusals
  // indistinguishable from a quiet capture.
  uint32_t voided = 0;
  for (int i = 0; i < ROOST_REC_COUNT; i++)
    voided += roostSdRowsVoided(g, (RoostRecord)i);
  c->observationsDropped = st.rowsDropped + voided + droppedBeforeWriter;
}

// --------------------------------------------------------------------------
// SESSION FACTS
// --------------------------------------------------------------------------

typedef struct {
  // identity
  const char* deviceModel;      // matches the `devices` list in records.toml
  const char* deviceSerial;     // NULL where the hardware has no stable one
  const char* hwRevision;
  const char* fwVersion;
  const char* builtAt;

  // hardware
  const char* const* ownMacs;   // every MAC this device transmits from
  size_t numOwnMacs;
  float gnssCepM;

  // session
  const char* sessionId;
  uint32_t sequence;
  uint32_t bootCount;
  int clockAnchored;
  RoostClockSource clockSource;
  uint32_t clockAnchorUnix;
  uint32_t clockAnchorUptimeMs;
  // Uptime at an orderly shutdown, or 0 while the session is still running.
  // Absent ended_utc is the signal that a session lost power, so this is never
  // filled in from the most recent snapshot.
  uint32_t endedUptimeMs;

  // capture
  uint32_t ouiTableHash;
  const char* ieTableHash;      // NULL renders null: no IE matcher in this build
  const char* dedupPolicy;      // NULL renders null: every repeat is in the file
  const char* storageTier;

  RoostSessionCounters counters;

  // The contract half. Rendered from these, never hand-written.
  const RoostFileDecl* files;
  size_t numFiles;
  const char* const* rawFiles;  // pre-rendered by roostManifestPcapFile()
  size_t numRawFiles;

  // Counters this device keeps beyond the contract, as a rendered JSON object
  // body without the braces, or NULL. The one place the schema stops being
  // closed, so a device with something worth recording has somewhere to put it
  // that does not fail validation.
  const char* deviceDiagnostics;
} RoostSessionInfo;

// --------------------------------------------------------------------------
// RENDERING
// --------------------------------------------------------------------------

typedef struct {
  char* p;
  size_t cap;
  size_t n;
  int ok;
} RoostJsonBuf;

// Every write goes through this, so one overflow check covers the whole
// document and a partial manifest is never emitted.
static inline void roostJsonPut(RoostJsonBuf* b, const char* s) {
  if (!b->ok) return;
  const size_t len = strlen(s);
  if (b->n + len + 1 > b->cap) { b->ok = 0; return; }
  memcpy(b->p + b->n, s, len);
  b->n += len;
  b->p[b->n] = '\0';
}

static inline void roostJsonFmt(RoostJsonBuf* b, const char* f, ...) {
  if (!b->ok) return;
  va_list ap;
  va_start(ap, f);
  const int w = vsnprintf(b->p + b->n, b->cap - b->n, f, ap);
  va_end(ap);
  if (w < 0 || (size_t)w >= b->cap - b->n) { b->ok = 0; return; }
  b->n += (size_t)w;
}

// The only values that could carry anything awkward are version strings, but
// escaping all of them is cheaper than reasoning about which.
static inline void roostJsonStr(RoostJsonBuf* b, const char* s) {
  roostJsonPut(b, "\"");
  for (const char* c = s; c && *c; c++) {
    switch (*c) {
      case '"':  roostJsonPut(b, "\\\""); break;
      case '\\': roostJsonPut(b, "\\\\"); break;
      case '\n': roostJsonPut(b, "\\n");  break;
      case '\r': roostJsonPut(b, "\\r");  break;
      case '\t': roostJsonPut(b, "\\t");  break;
      default: {
        char one[2] = {*c, '\0'};
        roostJsonPut(b, one);
      }
    }
  }
  roostJsonPut(b, "\"");
}

// A key whose value is a string, or null when there is nothing to say. Null and
// "" are different claims and the contract leans on the difference.
static inline void roostJsonKeyStr(RoostJsonBuf* b, const char* k,
                                   const char* v, int comma) {
  roostJsonFmt(b, "\"%s\":", k);
  if (v) roostJsonStr(b, v);
  else   roostJsonPut(b, "null");
  if (comma) roostJsonPut(b, ",");
}

static inline void roostJsonKeyU(RoostJsonBuf* b, const char* k, uint32_t v,
                                 int comma) {
  roostJsonFmt(b, "\"%s\":%lu%s", k, (unsigned long)v, comma ? "," : "");
}

// Renders the whole manifest, or returns 0 and writes nothing usable. A caller
// that gets 0 must not write the buffer: a truncated manifest asserts a file
// set and a column list that the session does not have.
//
// Needs roughly 4 KB for a device declaring six record types.
static inline size_t roostSessionJson(char* out, size_t cap,
                                      const RoostSessionInfo* info) {
  if (!out || !cap || !info) return 0;
  RoostJsonBuf b;
  b.p = out; b.cap = cap; b.n = 0; b.ok = 1;
  out[0] = '\0';

  roostJsonPut(&b, "{");
  roostJsonKeyU(&b, "manifest_version", ROOST_MANIFEST_VERSION, 1);

  // --- identity ---
  // registry_hash first: it says exactly which fields existed when this capture
  // was taken, which is what makes additive registry growth traceable.
  roostJsonPut(&b, "\"identity\":{");
  roostJsonKeyStr(&b, ROOST_MK_REGISTRY_HASH, ROOST_REGISTRY_HASH, 1);
  roostJsonKeyStr(&b, ROOST_MK_DEVICE_MODEL, info->deviceModel, 1);
  roostJsonKeyStr(&b, ROOST_MK_DEVICE_SERIAL, info->deviceSerial, 1);
  roostJsonKeyStr(&b, ROOST_MK_HW_REVISION, info->hwRevision, 1);
  roostJsonKeyStr(&b, ROOST_MK_FW_VERSION, info->fwVersion, 1);
  roostJsonKeyStr(&b, ROOST_MK_BUILT_AT, info->builtAt, 0);
  roostJsonPut(&b, "},");

  // --- hardware ---
  roostJsonFmt(&b, "\"%s\":{\"%s\":[", "hardware", ROOST_MK_OWN_MACS);
  for (size_t i = 0; i < info->numOwnMacs; i++) {
    if (i) roostJsonPut(&b, ",");
    roostJsonStr(&b, info->ownMacs[i]);
  }
  roostJsonPut(&b, "],");
  roostJsonFmt(&b, "\"%s\":", ROOST_MK_COMPONENTS);
  {
    char comps[512];
    // Never a manifest with a half-rendered component list: cap_component on
    // every row is validated against it.
    if (!roostManifestComponents(comps, sizeof(comps))) return 0;
    roostJsonPut(&b, comps);
  }
  roostJsonPut(&b, ",");
  roostJsonFmt(&b, "\"%s\":%.1f", ROOST_MK_GNSS_CEP_M, (double)info->gnssCepM);
  roostJsonPut(&b, "},");

  // --- session ---
  roostJsonPut(&b, "\"session\":{");
  roostJsonKeyStr(&b, ROOST_MK_SESSION_ID, info->sessionId, 1);
  roostJsonKeyU(&b, ROOST_MK_SEQUENCE, info->sequence, 1);
  roostJsonKeyU(&b, ROOST_MK_BOOT_COUNT, info->bootCount, 1);

  // Derived, never supplied. The anchor pair says "unix time U was uptime M",
  // so the session began at U - M/1000. Taken from the caller it could disagree
  // with the anchor it restates.
  char stamp[ROOST_ISO_BUF];
  if (info->clockAnchored) {
    roostUnixToIso(info->clockAnchorUnix - info->clockAnchorUptimeMs / 1000u,
                   stamp, sizeof(stamp));
    roostJsonKeyStr(&b, ROOST_MK_STARTED_UTC, stamp, 1);
  } else {
    roostJsonKeyStr(&b, ROOST_MK_STARTED_UTC, NULL, 1);
  }
  // The end must not precede the anchor. A session that shuts down before the
  // clock anchors has no wall-clock end, and an unsigned subtraction the wrong
  // way round renders a timestamp 136 years out rather than failing.
  if (info->clockAnchored && info->endedUptimeMs &&
      roostTimestampAt(info->clockAnchorUnix, info->clockAnchorUptimeMs,
                       info->endedUptimeMs, stamp, sizeof(stamp))) {
    roostJsonKeyStr(&b, ROOST_MK_ENDED_UTC, stamp, 1);
  } else {
    roostJsonKeyStr(&b, ROOST_MK_ENDED_UTC, NULL, 1);
  }

  // An unresolvable source reads as none rather than indexing past the table.
  // roost<Enum>ByName returns _COUNT for a name the vocabulary does not carry,
  // and that value must not reach an array subscript.
  RoostClockSource src = info->clockSource;
  if (!info->clockAnchored || src >= ROOST_CLOCK_SOURCE_COUNT)
    src = ROOST_CLOCK_SOURCE_NONE;
  roostJsonKeyStr(&b, ROOST_MK_CLOCK_SOURCE, kRoostClockSource[src], 1);
  if (info->clockAnchored) {
    roostJsonKeyU(&b, ROOST_MK_CLOCK_ANCHOR_UNIX, info->clockAnchorUnix, 1);
    roostJsonKeyU(&b, ROOST_MK_CLOCK_ANCHOR_UPTIME_MS,
                  info->clockAnchorUptimeMs, 0);
  } else {
    // Null rather than 0. Zero is a real uptime and a real unix time, so it
    // cannot stand for absence.
    roostJsonFmt(&b, "\"%s\":null,", ROOST_MK_CLOCK_ANCHOR_UNIX);
    roostJsonFmt(&b, "\"%s\":null", ROOST_MK_CLOCK_ANCHOR_UPTIME_MS);
  }
  roostJsonPut(&b, "},");

  // --- capture ---
  roostJsonPut(&b, "\"capture\":{");
  roostJsonFmt(&b, "\"%s\":\"%08lx\",", ROOST_MK_OUI_TABLE_HASH,
               (unsigned long)info->ouiTableHash);
  roostJsonKeyStr(&b, ROOST_MK_IE_TABLE_HASH, info->ieTableHash, 1);
  roostJsonKeyStr(&b, ROOST_MK_DEDUP_POLICY, info->dedupPolicy, 1);
  roostJsonKeyStr(&b, ROOST_MK_STORAGE_TIER, info->storageTier, 0);
  roostJsonPut(&b, "},");

  // --- files, the contract half ---
  {
    char files[1792];
    if (!roostManifestFiles(files, sizeof(files), info->files, info->numFiles,
                            info->rawFiles, info->numRawFiles)) {
      return 0;
    }
    roostJsonPut(&b, files);
  }
  roostJsonPut(&b, ",");

  // --- counters ---
  roostJsonPut(&b, "\"counters\":{");
  roostJsonKeyU(&b, ROOST_MK_OBSERVATIONS_WRITTEN,
                info->counters.observationsWritten, 1);
  roostJsonKeyU(&b, ROOST_MK_OBSERVATIONS_SUPPRESSED,
                info->counters.observationsSuppressed, 1);
  roostJsonKeyU(&b, ROOST_MK_OBSERVATIONS_DROPPED,
                info->counters.observationsDropped, 1);
  roostJsonKeyU(&b, ROOST_MK_FIXES_WRITTEN, info->counters.fixesWritten, 1);
  roostJsonKeyU(&b, ROOST_MK_STORAGE_ERRORS, info->counters.storageErrors, 1);
  roostJsonKeyU(&b, ROOST_MK_WORST_FLUSH_MS, info->counters.worstFlushMs, 0);
  roostJsonPut(&b, "}");

  if (info->deviceDiagnostics) {
    roostJsonPut(&b, ",\"device_diagnostics\":{");
    roostJsonPut(&b, info->deviceDiagnostics);
    roostJsonPut(&b, "}");
  }

  roostJsonPut(&b, "}");
  return b.ok ? b.n : 0;
}

#ifdef __cplusplus
}
#endif
