// Copyright (c) 2026 Lone Crow Design, LLC
// SPDX-License-Identifier: MIT
//
// roost_ie.h - 802.11 management-body and information-element walker.
//
// Any device that captures Wi-Fi promiscuously needs the same four answers off
// the same bytes: the SSID, the capability field, the beacon interval, and the
// element-ID fingerprint. They are derived here once rather than per device.
//
// Platform-free, like roost_sdlog.h and roost_manifest.h beside it: no ESP-IDF
// types, no Arduino. The caller pulls the frame bytes out of whatever its
// driver hands it and passes them in, which makes the parsing, the part that
// can read out of bounds, host-testable.
//
// Contract: docs/design_spec.md. The fields these produce are `ssid`,
// `cap_info`, `beacon_interval`, `ie_ids` and `vendor_ies` on wifi_obs.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// The trailing frame check sequence. Hardware strips it, but a driver's
// reported on-air length still counts it, so a parser that trusts that length
// reads four bytes of FCS as the start of another element and every management
// frame gains a spurious trailing IE. See roostIeParseLen.
#define ROOST_IE_FCS_LEN 4

// Smallest frame carrying three addresses and a sequence number.
#define ROOST_IE_MIN_MGMT_LEN 24

// Sized to hold the element list of a dense beacon. These are the buffer sizes
// a caller should use; below them the value is refused
// rather than truncated, so undersizing loses the densest frames silently.
#define ROOST_IE_IDS_BUF  160
#define ROOST_IE_VEND_BUF 160
#define ROOST_IE_SSID_BUF 33

// How many bytes of a captured frame may be parsed. `sigLen` is the driver's
// on-air length and `dumpLen` is what it actually copied; the smaller bounds
// the buffer, and the FCS comes off that.
//
// The min() is applied before the subtraction on purpose: if a platform ever
// reports dumpLen < sigLen the other way round, this degrades to a short parse
// rather than running past the end of the buffer.
static inline size_t roostIeParseLen(size_t sigLen, size_t dumpLen) {
  const size_t bounded = sigLen < dumpLen ? sigLen : dumpLen;
  return bounded > ROOST_IE_FCS_LEN ? bounded - ROOST_IE_FCS_LEN : 0;
}

// True for the management subtypes whose body carries information elements.
static inline int roostIeSubtypeHasIes(uint8_t type, uint8_t subtype) {
  if (type != 0) return 0;                   // management frames only
  switch (subtype) {
    case 0x0:  // assoc req
    case 0x1:  // assoc resp
    case 0x2:  // reassoc req
    case 0x3:  // reassoc resp
    case 0x4:  // probe req
    case 0x5:  // probe resp
    case 0x8:  // beacon
      return 1;
    default:
      return 0;
  }
}

// Bytes of fixed field ahead of the first element, by subtype. A beacon or
// probe response carries timestamp, beacon interval and capability info; a
// probe request carries none.
static inline size_t roostIeOffset(uint8_t type, uint8_t subtype) {
  if (type != 0) return 0;
  switch (subtype) {
    case 0x8:  // beacon
    case 0x5:  // probe resp
      return 12;
    case 0x3:  // reassoc resp
    case 0x1:  // assoc resp
      return 6;
    case 0x2:  // reassoc req
      return 10;
    case 0x0:  // assoc req
      return 4;
    default:   // probe req
      return 0;
  }
}

typedef struct {
  uint8_t        id;
  uint8_t        len;
  const uint8_t* data;
} RoostIe;

// Walks the element list. Start with *cursor = roostIeOffset(...). Returns 0 at
// the end or on a malformed length, and never reads past `bodyLen`.
static inline int roostIeNext(const uint8_t* body, size_t bodyLen,
                              size_t* cursor, RoostIe* out) {
  if (!body || !cursor || !out) return 0;
  const size_t i = *cursor;
  if (i + 2 > bodyLen) return 0;

  const uint8_t id = body[i];
  const uint8_t len = body[i + 1];
  // A declared length running past the end of the body is a malformed frame,
  // not something to salvage.
  if (i + 2 + len > bodyLen) return 0;

  out->id = id;
  out->len = len;
  out->data = len ? (body + i + 2) : NULL;
  *cursor = i + 2 + len;
  return 1;
}

// The SSID element, captured once. An SSID is 0-32 ARBITRARY OCTETS, so
// `text[0]` answers none of the three questions a caller has: was there an
// element, how long was it, and what were the bytes. A caller
// that tests it silently merges a cloaked network padding with 0x00 into "no
// SSID at all". Everything that wants any of the three reads this struct.
//
//   present == 0             no element: a data frame, a non-IE-bearing
//                            subtype, or a malformed body
//   present == 1, len == 0   an empty element: a hidden network on a beacon,
//                            a wildcard probe on a probe_req
//   present == 1, len  > 0   `len` octets of name in `text`
//
// `text` is NUL-terminated at [len] so it can be printed, but that terminator
// is for display only: the octets may contain 0x00 and the CSV must be written
// with roostRowSetTextN and `len`, never with a strlen-taking setter.
//
// Both empty cases render as an empty ssid column, which is correct under
// design_spec.md 7.1: present-and-empty. `frame_subtype` is what separates
// them at ingest: an empty ssid on a beacon or probe_req means the element was
// there and zero-length, on a data frame it means the field was unreachable.
typedef struct {
  char    text[33];
  uint8_t len;
  uint8_t present;
} RoostSsid;

// The printable form, for a display or a detection table: the name, or NULL
// when there is nothing to show. Lossy by construction, since it stops at the
// first 0x00, so it must never feed a record column. Those take
// roostRowSetTextN with `len`, which keeps the octets byte-exact and reversible.
static inline const char* roostSsidPrintable(const RoostSsid* s) {
  return (s && s->len) ? s->text : NULL;
}

// One walk, filling all three. Bytes are copied verbatim, including
// unprintable ones: the CSV writer is what makes them safe and the raw value
// is the evidence.
static inline void roostIeSsidCapture(const uint8_t* body, size_t bodyLen,
                                      uint8_t type, uint8_t subtype,
                                      RoostSsid* out) {
  if (!out) return;
  out->text[0] = '\0';
  out->len     = 0;
  out->present = 0;
  if (!roostIeSubtypeHasIes(type, subtype)) return;

  size_t cursor = roostIeOffset(type, subtype);
  RoostIe ie;
  while (roostIeNext(body, bodyLen, &cursor, &ie)) {
    if (ie.id != 0) continue;
    size_t n = ie.len;
    if (n > 32) n = 32;              // longer than the standard allows
    if (n && ie.data) memcpy(out->text, ie.data, n);
    out->text[n] = '\0';
    out->len     = (uint8_t)n;
    out->present = 1;
    return;
  }
}

// The ordered element-ID list, "0|1|3|5|221" in frame order. Returns bytes
// written, or 0 if there was nothing to write or it did not fit.
//
// Order and membership are the measurement, so a list that does not fit is
// refused rather than truncated: a prefix reads as a complete list of fewer
// elements and false-matches a frame that genuinely has only those. Give it
// ROOST_IE_IDS_BUF and it does not arise.
static inline size_t roostIeIdList(const uint8_t* body, size_t bodyLen,
                                   uint8_t type, uint8_t subtype,
                                   char* out, size_t cap) {
  if (!out || !cap) return 0;
  out[0] = '\0';
  if (!roostIeSubtypeHasIes(type, subtype)) return 0;

  size_t n = 0, cursor = roostIeOffset(type, subtype);
  RoostIe ie;
  int first = 1;
  while (roostIeNext(body, bodyLen, &cursor, &ie)) {
    char item[6];
    const int w = snprintf(item, sizeof(item), first ? "%u" : "|%u", ie.id);
    if (w < 0 || n + (size_t)w + 1 > cap) { out[0] = '\0'; return 0; }
    memcpy(out + n, item, (size_t)w);
    n += (size_t)w;
    out[n] = '\0';
    first = 0;
  }
  return n;
}

// Vendor elements as `oui:type` pairs in frame order. Same refusal rule and
// same reason: every vendor element shares ID 221, so this list is what
// distinguishes them and a prefix of it is a different fingerprint.
static inline size_t roostIeVendorList(const uint8_t* body, size_t bodyLen,
                                       uint8_t type, uint8_t subtype,
                                       char* out, size_t cap) {
  if (!out || !cap) return 0;
  out[0] = '\0';
  if (!roostIeSubtypeHasIes(type, subtype)) return 0;

  size_t n = 0, cursor = roostIeOffset(type, subtype);
  RoostIe ie;
  int first = 1;
  while (roostIeNext(body, bodyLen, &cursor, &ie)) {
    if (ie.id != 221) continue;
    if (ie.len < 4 || !ie.data) continue;    // 3-byte OUI plus a type octet
    char item[16];
    const int w = snprintf(item, sizeof(item),
                           first ? "%02x:%02x:%02x:%02x" : "|%02x:%02x:%02x:%02x",
                           ie.data[0], ie.data[1], ie.data[2], ie.data[3]);
    if (w < 0 || n + (size_t)w + 1 > cap) { out[0] = '\0'; return 0; }
    memcpy(out + n, item, (size_t)w);
    n += (size_t)w;
    out[n] = '\0';
    first = 0;
  }
  return n;
}

// The two values sitting in the fixed field ahead of the elements, present only
// on a beacon or probe response. Returns 0 and touches nothing when the body is
// too short or the subtype carries no fixed field, so a caller can use the
// return as the "set these columns" test.
static inline int roostIeFixedFields(const uint8_t* body, size_t bodyLen,
                                     uint8_t type, uint8_t subtype,
                                     uint8_t capInfo[2], uint16_t* beaconInterval) {
  if (!body || !capInfo || !beaconInterval) return 0;
  if (roostIeOffset(type, subtype) != 12 || bodyLen < 12) return 0;
  // Layout: timestamp[0..7], beacon interval[8..9], capability info[10..11].
  *beaconInterval = (uint16_t)(body[8] | (body[9] << 8));
  capInfo[0] = body[10];
  capInfo[1] = body[11];
  return 1;
}

#ifdef __cplusplus
}
#endif
