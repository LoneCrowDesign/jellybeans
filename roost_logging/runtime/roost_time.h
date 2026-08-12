// Copyright (c) 2026 Lone Crow Design, LLC
// SPDX-License-Identifier: MIT
//
// roost_time.h - the clock anchor arithmetic, in one place.
//
// Anchor rule in code: a row always carries uptime_ms but timestamp_utc only
// when the actual time is known. The manifest's anchor triple places the rest
// retroactively as anchor_unix + (uptime_ms - anchor_uptime_ms).
//
// That subtraction is unsigned, so it is done here once rather than per device.
// A row stamped before the anchor underflows to roughly 49.7 days, which renders
// as a plausible date weeks in the future rather than as an error.
//
// Platform-free, no time.h: the toolchains differ and the conversion is twenty
// lines. See docs/design_spec.md section 6.3.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Widest ISO-8601 second-resolution stamp plus its NUL: 1970-01-01T00:00:00Z.
#define ROOST_ISO_BUF 21

// Civil date from a unix second, by the usual days-from-epoch inversion.
static inline void roostUnixToIso(uint32_t t, char* out, size_t cap) {
  if (!out || cap < ROOST_ISO_BUF) { if (out && cap) out[0] = '\0'; return; }
  const uint32_t secsOfDay = t % 86400u;
  int32_t z = (int32_t)(t / 86400u) + 719468;
  const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  const uint32_t doe = (uint32_t)(z - era * 146097);
  const uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const uint32_t mp = (5 * doy + 2) / 153;
  const uint32_t d = doy - (153 * mp + 2) / 5 + 1;
  const uint32_t m = mp < 10 ? mp + 3 : mp - 9;
  const int32_t y = (int32_t)yoe + era * 400 + (m <= 2 ? 1 : 0);
  snprintf(out, cap, "%04ld-%02lu-%02luT%02lu:%02lu:%02luZ",
           (long)y, (unsigned long)m, (unsigned long)d,
           (unsigned long)(secsOfDay / 3600u),
           (unsigned long)((secsOfDay % 3600u) / 60u),
           (unsigned long)(secsOfDay % 60u));
}

// Places a row on the wall clock from the anchor triple. Returns 0 and empties
// `out` when the row cannot be placed, which the caller must render as an empty
// timestamp_utc rather than as any substitute value (design_spec.md 6.3).
//
// Two cases return 0, and neither is an error:
//   - the clock never anchored, so no row in the session has a wall-clock time
//   - this row precedes the anchor, so its time was not known when it was
//     written and the anchor cannot reach backwards to it
//
// The second is the one that must never be computed anyway. Pre-anchor rows are
// normal on any device whose time arrives from a GNSS fix, and the anchor triple
// in the manifest is what places them at ingest, where the arithmetic can be
// done in a signed type that expresses "before".
static inline int roostTimestampAt(uint32_t anchorUnix, uint32_t anchorUptimeMs,
                                   uint32_t uptimeMs, char* out, size_t cap) {
  if (!out || !cap) return 0;
  out[0] = '\0';
  if (!anchorUnix) return 0;
  if (uptimeMs < anchorUptimeMs) return 0;
  roostUnixToIso(anchorUnix + (uptimeMs - anchorUptimeMs) / 1000u, out, cap);
  return out[0] != '\0';
}

#ifdef __cplusplus
}
#endif
