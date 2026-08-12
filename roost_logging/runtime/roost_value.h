// Copyright (c) 2026 Lone Crow Design, LLC
// SPDX-License-Identifier: MIT
//
// roost_value.h - the `list` and `map` value encodings, in one place.
//
// A registry key's declared type fixes how its value is rendered, so under
// design_spec.md 3.1 the rendering is one implementation and not a device
// choice. Every device that writes a config_change value builds it here.
//
// A type declaration alone does not fix a rendering. Left to each device, a
// `list` gets written comma-separated on one and pipe-separated on another, and
// a `map` gets a different pair separator again. More than one rendering of one
// type cannot be parsed without knowing which device wrote the row, which is
// what the declared type exists to remove.
//
// Overflow refuses rather than truncates. A shortened channel list reads as a
// narrower plan that was deliberately chosen, which is a different and entirely
// plausible capture; there is nothing in the artifact to mark it as damage.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ROOST_VALUE_SEP '|'
#define ROOST_VALUE_KV  '='

typedef struct {
  char*  buf;
  size_t cap;     // including the terminator
  size_t len;
  int    count;   // entries appended so far, so the separator is not led
  int    ok;      // cleared by the first entry that did not fit
} RoostValue;

static inline void roostValueBegin(RoostValue* v, char* buf, size_t cap) {
  v->buf   = buf;
  v->cap   = cap;
  v->len   = 0;
  v->count = 0;
  v->ok    = (buf && cap) ? 1 : 0;
  if (buf && cap) buf[0] = '\0';
}

// Appends `text`, preceded by the separator when it is not the first entry.
static inline int roostValueRaw(RoostValue* v, const char* text) {
  if (!v->ok || !text) { v->ok = 0; return 0; }
  const size_t lead = v->count ? 1u : 0u;
  size_t n = 0;
  while (text[n]) n++;
  if (v->len + lead + n + 1 > v->cap) { v->ok = 0; return 0; }
  if (lead) v->buf[v->len++] = ROOST_VALUE_SEP;
  for (size_t i = 0; i < n; i++) v->buf[v->len++] = text[i];
  v->buf[v->len] = '\0';
  v->count++;
  return 1;
}

static inline int roostValueAddText(RoostValue* v, const char* text) {
  return roostValueRaw(v, text);
}

static inline int roostValueAddInt(RoostValue* v, int32_t n) {
  char num[12];
  snprintf(num, sizeof(num), "%ld", (long)n);
  return roostValueRaw(v, num);
}

static inline int roostValueAddUInt(RoostValue* v, uint32_t n) {
  char num[12];
  snprintf(num, sizeof(num), "%lu", (unsigned long)n);
  return roostValueRaw(v, num);
}

// One `map` entry. A key or value that would itself need '|' or '=' escaped
// means the setting should be split rather than nested, so neither is escaped
// here: such a value is refused.
static inline int roostValueAddKeyText(RoostValue* v, const char* key,
                                       const char* text) {
  if (!v->ok || !key || !text) { v->ok = 0; return 0; }
  for (const char* p = key; *p; p++)
    if (*p == ROOST_VALUE_SEP || *p == ROOST_VALUE_KV) { v->ok = 0; return 0; }
  for (const char* p = text; *p; p++)
    if (*p == ROOST_VALUE_SEP || *p == ROOST_VALUE_KV) { v->ok = 0; return 0; }

  char pair[64];
  const int w = snprintf(pair, sizeof(pair), "%s%c%s", key, ROOST_VALUE_KV, text);
  if (w < 0 || (size_t)w >= sizeof(pair)) { v->ok = 0; return 0; }
  return roostValueRaw(v, pair);
}

static inline int roostValueAddKeyInt(RoostValue* v, const char* key, int32_t n) {
  char num[12];
  snprintf(num, sizeof(num), "%ld", (long)n);
  return roostValueAddKeyText(v, key, num);
}

static inline int roostValueAddKeyUInt(RoostValue* v, const char* key, uint32_t n) {
  char num[12];
  snprintf(num, sizeof(num), "%lu", (unsigned long)n);
  return roostValueAddKeyText(v, key, num);
}

// True when every entry fitted. On false the buffer is emptied, because an
// empty value already means "does not apply on this device" and a partial one
// would be indistinguishable from a complete short list.
static inline int roostValueDone(RoostValue* v) {
  if (v->ok) return 1;
  if (v->buf && v->cap) v->buf[0] = '\0';
  v->len = 0;
  return 0;
}

#ifdef __cplusplus
}
#endif
