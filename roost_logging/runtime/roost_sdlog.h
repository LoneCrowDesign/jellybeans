// Copyright (c) 2026 Lone Crow Design, LLC
// SPDX-License-Identifier: MIT
//
// roost_sdlog.h - buffered CSV writer for a roost session.
//
// Hand-written, unlike generated/roost_registry.h. It lives here because every
// device needs the same buffering behaviour.
//
// The constraint it is built around: on an SD card behind an SPI controller,
// small writes cost far more per byte than block-sized ones, and a per-row sync
// on a high-rate file stalls the capture path long enough to lose observations.
// Hence: accumulate into block-sized buffers, and never sync per row on a
// high-rate file.
//
// Platform-free by construction. Storage arrives as a RoostSdIo of function
// pointers, so the buffering logic compiles and is tested on the host, and each
// device supplies its own backend.
//
// The caller owns every buffer. Nothing here allocates.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "roost_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

// Storage, injected. A handle is an opaque non-negative integer; <0 is failure.
typedef struct {
  void* ctx;
  int      (*mkdir)(void* ctx, const char* path);                  // 0 ok
  // Must append, not truncate: a session reopens its files after the
  // directory is renamed at the clock anchor, and truncating there destroys
  // everything captured before it.
  int      (*open) (void* ctx, const char* path);                  // handle, <0 fail
  size_t   (*size) (void* ctx, int h);                             // current length
  int      (*write)(void* ctx, int h, const void* d, size_t n);    // bytes, <0 fail
  int      (*sync) (void* ctx, int h);                             // 0 ok
  void     (*close)(void* ctx, int h);
  uint32_t (*nowMs)(void* ctx);
} RoostSdIo;

typedef struct {
  uint32_t rowsWritten;
  uint32_t rowsDropped;    // no card, no session, or the write failed
  // Failed write or sync calls, counted separately from the rows lost to them:
  // the manifest's storage_errors and observations_dropped are different
  // questions, and one variable answering both cannot distinguish a card that
  // failed once from a buffer that overflowed with the card healthy.
  uint32_t storageErrors;
  // Rows lost to backpressure with storage working: the buffer had no room and
  // the flush that would have made room did not happen or did not help.
  uint32_t overflowRows;
  uint32_t bytesWritten;
  uint32_t worstFlushMs;   // the number that governs queue sizing
  uint32_t flushCount;
  // Bytes that actually reached a raw stream. A capture side's own retained-byte
  // count is a different figure: it counts what was handed to the writer, not
  // what the writer landed, and the two diverge whenever storage fails.
  uint32_t rawBytesWritten;
} RoostSdStats;

typedef struct {
  uint8_t*       buf;      // NULL means write-through
  size_t         cap;
  size_t         used;
  int            handle;   // <0 when not open
  RoostFieldMask columns;
} RoostSdSlot;

// How many raw streams a session may declare alongside its record files. A
// device that retains no raw capture pays one empty slot; a device retaining a
// pcap uses one. Raised here rather than per device, since the writer is shared.
#ifndef ROOST_SD_MAX_RAW
#define ROOST_SD_MAX_RAW 2
#endif

// A raw stream is bytes, not rows: no header from the registry, no CRLF, no
// row framing. Its opening bytes are supplied by the caller because their
// meaning belongs to the format, not to the contract: a pcap global header is
// 24 bytes the writer has no business interpreting.
typedef struct {
  uint8_t*       buf;
  size_t         cap;
  size_t         used;
  int            handle;
  const uint8_t* fileHeader;   // written once, on create only
  size_t         fileHeaderLen;
  char           name[32];
} RoostSdRaw;

typedef struct {
  const RoostSdIo* io;
  RoostSdSlot      slot[ROOST_REC_COUNT];
  RoostSdRaw       raw[ROOST_SD_MAX_RAW];
  size_t           rawCount;
  RoostSdStats     stats;
  // Per record type, because the manifest's observations_written counts the
  // observation family alone and a session-wide row total answers a different
  // question. Without the split, a manifest can report a healthy row total while
  // the record that carries the observations has written nothing.
  uint32_t         wrote[ROOST_REC_COUNT];
  uint32_t         voided[ROOST_REC_COUNT];
  int              open;
} RoostSdLog;

// --------------------------------------------------------------------------

static inline void roostSdInit(RoostSdLog* g, const RoostSdIo* io) {
  memset(g, 0, sizeof(*g));
  g->io = io;
  for (int i = 0; i < ROOST_REC_COUNT; i++) g->slot[i].handle = -1;
  for (int i = 0; i < ROOST_SD_MAX_RAW; i++) g->raw[i].handle = -1;
}

// Declares a raw stream. `fileHeader` is written only when the file is created,
// never on a reopen: a pcap global header emitted twice is as unreadable as one
// omitted. Call before roostSdOpenSession. Returns the stream index, or -1.
static inline int roostSdAttachRaw(RoostSdLog* g, const char* name,
                                   uint8_t* buf, size_t cap,
                                   const uint8_t* fileHeader, size_t headerLen) {
  if (g->rawCount >= ROOST_SD_MAX_RAW || !name) return -1;
  RoostSdRaw* r = &g->raw[g->rawCount];
  r->buf = cap ? buf : NULL;
  r->cap = cap;
  r->used = 0;
  r->handle = -1;
  r->fileHeader = fileHeader;
  r->fileHeaderLen = headerLen;
  size_t n = 0;
  while (name[n] && n + 1 < sizeof(r->name)) { r->name[n] = name[n]; n++; }
  r->name[n] = 0;
  return (int)g->rawCount++;
}

// Attach before opening the session. A record with no buffer is write-through:
// every row reaches the card before the call returns.
//
// Which records get a buffer is a durability decision, not a performance one.
// device_event, config_change and operator_mark must not have one. A
// device_event reporting that storage just failed is useless sitting in RAM,
// and an operator mark is a press that does not come again. They are rare
// enough that write-through costs nothing.
static inline void roostSdAttachBuffer(RoostSdLog* g, RoostRecord r,
                                       uint8_t* buf, size_t cap) {
  if (r >= ROOST_REC_COUNT) return;
  g->slot[r].buf = cap ? buf : NULL;
  g->slot[r].cap = cap;
  g->slot[r].used = 0;
}

static inline int roostSdFlush_(RoostSdLog* g, RoostSdSlot* s) {
  if (!s->used || s->handle < 0) return 1;
  const uint32_t t0 = g->io->nowMs(g->io->ctx);
  const int n = g->io->write(g->io->ctx, s->handle, s->buf, s->used);
  const int rc = g->io->sync(g->io->ctx, s->handle);
  const uint32_t dt = g->io->nowMs(g->io->ctx) - t0;

  if (dt > g->stats.worstFlushMs) g->stats.worstFlushMs = dt;
  g->stats.flushCount++;

  if (n < 0) { g->stats.storageErrors++; return 0; }
  g->stats.bytesWritten += (uint32_t)n;
  // Consume exactly what reached the card. Retrying the whole buffer would
  // write the landed bytes a second time, leaving a truncated line followed by
  // a complete copy of the same block: corruption in the MIDDLE of the file
  // rather than a partial line at the end, and the one shape a line-oriented
  // reader cannot recover from by dropping the tail.
  if ((size_t)n < s->used) {
    memmove(s->buf, s->buf + n, s->used - (size_t)n);
    s->used -= (size_t)n;
    g->stats.storageErrors++;
    return 0;
  }
  s->used = 0;
  if (rc != 0) { g->stats.storageErrors++; return 0; }
  return 1;
}

static inline int roostSdFlushRaw_(RoostSdLog* g, RoostSdRaw* r) {
  if (!r->used || r->handle < 0) return 1;
  const uint32_t t0 = g->io->nowMs(g->io->ctx);
  const int n = g->io->write(g->io->ctx, r->handle, r->buf, r->used);
  const int rc = g->io->sync(g->io->ctx, r->handle);
  const uint32_t dt = g->io->nowMs(g->io->ctx) - t0;
  if (dt > g->stats.worstFlushMs) g->stats.worstFlushMs = dt;
  g->stats.flushCount++;
  if (n < 0) { g->stats.storageErrors++; return 0; }
  g->stats.rawBytesWritten += (uint32_t)n;
  if ((size_t)n < r->used) {          // same rule as a record stream
    memmove(r->buf, r->buf + n, r->used - (size_t)n);
    r->used -= (size_t)n;
    g->stats.storageErrors++;
    return 0;
  }
  r->used = 0;
  if (rc != 0) { g->stats.storageErrors++; return 0; }
  return 1;
}

static inline int roostSdFlush(RoostSdLog* g, RoostRecord r) {
  if (!g->open || r >= ROOST_REC_COUNT) return 0;
  return roostSdFlush_(g, &g->slot[r]);
}

static inline int roostSdFlushAll(RoostSdLog* g) {
  int ok = 1;
  for (int i = 0; i < ROOST_REC_COUNT; i++)
    if (!roostSdFlush_(g, &g->slot[i])) ok = 0;
  for (size_t i = 0; i < g->rawCount; i++)
    if (!roostSdFlushRaw_(g, &g->raw[i])) ok = 0;
  return ok;
}

// Creates the directory and every declared file, each with its header, whether
// or not the session will ever observe that record type. A declared file is
// never absent: an empty operator_mark.v1.csv says the operator marked nothing,
// where a missing one says that and "something went wrong" at once.
// design_spec.md 6.2.
static inline int roostSdOpenSession(RoostSdLog* g, const char* dir,
                                     const RoostFileDecl* decls, size_t n) {
  if (!g->io || !dir || !decls) return 0;
  if (g->io->mkdir(g->io->ctx, dir) != 0) return 0;

  for (size_t i = 0; i < n; i++) {
    if (!roostFileDeclValid(&decls[i])) return 0;
    const RoostRecord r = decls[i].record;
    RoostSdSlot* s = &g->slot[r];

    char name[48], path[160];
    if (!roostFileName(name, sizeof(name), r)) return 0;
    const size_t dl = strlen(dir);
    if (dl + 1u + strlen(name) + 1u > sizeof(path)) return 0;
    memcpy(path, dir, dl);
    path[dl] = '/';
    memcpy(path + dl + 1u, name, strlen(name) + 1u);

    s->handle = g->io->open(g->io->ctx, path);
    if (s->handle < 0) return 0;
    s->columns = decls[i].columns;
    s->used = 0;

    // Only on a new file. Reopening after the anchor rename must append to
    // what is already there, not stack a second header onto it.
    if (g->io->size(g->io->ctx, s->handle) == 0) {
      // Straight to the card, not through the buffer: a session whose power is
      // pulled before the first block still has readable files.
      char hdr[640];
      const size_t hl = roostHeader(hdr, sizeof(hdr) - 2, r, s->columns);
      if (!hl) return 0;
      hdr[hl] = '\r'; hdr[hl + 1] = '\n';
      if (g->io->write(g->io->ctx, s->handle, hdr, hl + 2) < 0) {
        g->stats.storageErrors++; return 0;
      }
      if (g->io->sync(g->io->ctx, s->handle) != 0) {
        g->stats.storageErrors++; return 0;
      }
      g->stats.bytesWritten += (uint32_t)(hl + 2);
    }
  }
  for (size_t i = 0; i < g->rawCount; i++) {
    RoostSdRaw* r = &g->raw[i];
    char path[160];
    const size_t dl = strlen(dir);
    if (dl + 1u + strlen(r->name) + 1u > sizeof(path)) return 0;
    memcpy(path, dir, dl);
    path[dl] = '/';
    memcpy(path + dl + 1u, r->name, strlen(r->name) + 1u);

    r->handle = g->io->open(g->io->ctx, path);
    if (r->handle < 0) return 0;
    r->used = 0;
    // Create only, exactly as with a record header.
    if (r->fileHeader && r->fileHeaderLen &&
        g->io->size(g->io->ctx, r->handle) == 0) {
      if (g->io->write(g->io->ctx, r->handle, r->fileHeader,
                       r->fileHeaderLen) < 0) return 0;
      if (g->io->sync(g->io->ctx, r->handle) != 0) return 0;
      g->stats.rawBytesWritten += (uint32_t)r->fileHeaderLen;
    }
  }
  g->open = 1;
  return 1;
}

// Appends opaque bytes to a raw stream. No framing is added, so a caller that
// needs record boundaries supplies them.
static inline int roostSdAppendRaw(RoostSdLog* g, int stream,
                                   const void* data, size_t len) {
  if (!g->open || stream < 0 || (size_t)stream >= g->rawCount || !data || !len)
    return 0;
  RoostSdRaw* r = &g->raw[stream];
  if (r->handle < 0) return 0;

  if (!r->buf) {                                   // write-through
    if (g->io->write(g->io->ctx, r->handle, data, len) != (int)len) {
      g->stats.storageErrors++; return 0;
    }
    if (g->io->sync(g->io->ctx, r->handle) != 0) {
      g->stats.storageErrors++; return 0;
    }
    g->stats.rawBytesWritten += (uint32_t)len;
    return 1;
  }
  // Unlike a row, a raw payload may legitimately exceed the buffer, so it is
  // split across blocks rather than refused.
  const uint8_t* p = (const uint8_t*)data;
  while (len) {
    const size_t room = r->cap - r->used;
    const size_t take = (len < room) ? len : room;
    memcpy(r->buf + r->used, p, take);
    r->used += take; p += take; len -= take;
    if (r->used == r->cap && !roostSdFlushRaw_(g, r)) return 0;
  }
  return 1;
}

// Appends one finished row. `row` is what roostRowFinish() produced, with no
// line terminator; a zero-length row is a refusal from the row builder and is
// counted as a drop rather than written as a blank line.
static inline int roostSdAppend(RoostSdLog* g, RoostRecord r, const char* row) {
  if (!g->open || r >= ROOST_REC_COUNT || !row || !row[0]) {
    g->stats.rowsDropped++;
    return 0;
  }
  RoostSdSlot* s = &g->slot[r];
  if (s->handle < 0) { g->stats.rowsDropped++; return 0; }

  const size_t len = strlen(row);

  if (!s->buf) {                                  // write-through
    const uint32_t t0 = g->io->nowMs(g->io->ctx);
    int ok = g->io->write(g->io->ctx, s->handle, row, len) == (int)len
          && g->io->write(g->io->ctx, s->handle, "\r\n", 2) == 2
          && g->io->sync(g->io->ctx, s->handle) == 0;
    const uint32_t dt = g->io->nowMs(g->io->ctx) - t0;
    if (dt > g->stats.worstFlushMs) g->stats.worstFlushMs = dt;
    g->stats.flushCount++;
    if (!ok) { g->stats.rowsDropped++; g->stats.storageErrors++; return 0; }
    g->stats.bytesWritten += (uint32_t)(len + 2);
    g->stats.rowsWritten++;
    g->wrote[r]++;
    return 1;
  }

  // A row longer than the whole buffer would never fit and must not spin.
  if (len + 2u > s->cap) {
    g->stats.rowsDropped++; g->stats.overflowRows++;
    return 0;
  }
  if (s->used + len + 2u > s->cap && !roostSdFlush_(g, s)) {
    // The flush counted its own storage error if the card failed. This counts
    // the row that had nowhere to go, which is the separate fact.
    g->stats.rowsDropped++; g->stats.overflowRows++;
    return 0;
  }
  memcpy(s->buf + s->used, row, len);
  s->used += len;
  s->buf[s->used++] = '\r';
  s->buf[s->used++] = '\n';
  g->stats.rowsWritten++;
  g->wrote[r]++;
  return 1;
}

// Flushes and closes every file. Call before renaming the session directory:
// a renamed object must not be open, and reopening appends rather than
// re-emitting headers.
static inline void roostSdCloseSession(RoostSdLog* g) {
  if (!g->io) return;
  for (int i = 0; i < ROOST_REC_COUNT; i++) {
    RoostSdSlot* s = &g->slot[i];
    if (s->handle < 0) continue;
    roostSdFlush_(g, s);
    g->io->close(g->io->ctx, s->handle);
    s->handle = -1;
  }
  for (size_t i = 0; i < g->rawCount; i++) {
    RoostSdRaw* r = &g->raw[i];
    if (r->handle < 0) continue;
    roostSdFlushRaw_(g, r);
    g->io->close(g->io->ctx, r->handle);
    r->handle = -1;
  }
  g->open = 0;
}

static inline void roostSdGetStats(const RoostSdLog* g, RoostSdStats* out) {
  *out = g->stats;
}

static inline uint32_t roostSdRowsWritten(const RoostSdLog* g, RoostRecord r) {
  return (r < ROOST_REC_COUNT) ? g->wrote[r] : 0;
}

static inline uint32_t roostSdRowsVoided(const RoostSdLog* g, RoostRecord r) {
  return (r < ROOST_REC_COUNT) ? g->voided[r] : 0;
}

// Rows the row builder refused, which never reach roostSdAppend and so cannot
// be counted there. A refusal means a required column was never written, and
// it is silent by construction: the row simply does not exist. Counted per
// record type rather than summed, so a session of refusals is distinguishable
// from a session with nothing to record.
static inline void roostSdCountVoid(RoostSdLog* g, RoostRecord r) {
  if (r < ROOST_REC_COUNT) g->voided[r]++;
}

// Rows written across every record type in the observation family, which is
// what the manifest's observations_written means. Derived from the registry so
// a device adding a radio does not also have to remember to add it here.
static inline uint32_t roostSdObservationsWritten(const RoostSdLog* g) {
  uint32_t n = 0;
  for (int i = 0; i < ROOST_REC_COUNT; i++)
    if (roostRecordIsObservation((RoostRecord)i)) n += g->wrote[i];
  return n;
}

#ifdef __cplusplus
}
#endif
