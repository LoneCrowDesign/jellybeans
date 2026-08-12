// Copyright (c) 2026 Lone Crow Design, LLC
// SPDX-License-Identifier: MIT
//
// roost_channel.h - the channel-to-band derivation, in one place.
//
// `band` is a computed field, so under design_spec.md 3.1 it has one
// computation and every device calls this rather than open-coding a range test.
// Open-coded range tests are how two columns of one row come to disagree about
// the same channel: a value outside every declared range resolves differently in
// each test that was written independently.
//
// The row carries channel and band, not frequency: frequency is a pure function
// of the two, so it earns no column of its own. `channel` is what the driver
// reports and what config_change's channel plan is expressed in; `band`
// disambiguates a numbering space that overlaps between 2.4 GHz and 6 GHz, where
// channel 1 is a different frequency in each.

#pragma once

#include <stdint.h>

#include "roost_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

// 2.4 GHz occupies 1-14 with no ambiguity.
#define ROOST_CHAN_2G4_MIN 1
#define ROOST_CHAN_2G4_MAX 14

// The 5 GHz numbering floor is a regulatory question, not an arithmetic one.
// 36 is the lowest channel in the FCC domain; 32 and 34 exist in the 802.11
// Annex E tables for other domains at the same 5000 MHz base. This accepts from
// 32 so a capture in one of those domains is not refused. An FCC-only
// deployment can narrow the floor to 36.
#define ROOST_CHAN_5G_MIN 32
#define ROOST_CHAN_5G_MAX 177

// Not covered, deliberately, and each returns known = 0 rather than a guess:
//   - 6 GHz, which restarts at channel 1 on a different base. A device that
//     reaches it needs a `6` in the band vocabulary first, and this function
//     cannot distinguish 6 GHz channel 1 from 2.4 GHz channel 1 without one.
//   - 40/80/160 MHz centre-channel numbering, which is a different question
//     from the primary channel a promiscuous frame arrives on.
typedef struct {
  RoostBand band;
  uint8_t   known;   // 0: the caller writes no band column at all
} RoostChannelBand;

static inline RoostChannelBand roostBandForChannel(uint16_t channel) {
  RoostChannelBand out;
  out.band  = ROOST_BAND_2_4;
  out.known = 0;
  if (channel >= ROOST_CHAN_2G4_MIN && channel <= ROOST_CHAN_2G4_MAX) {
    out.band  = ROOST_BAND_2_4;
    out.known = 1;
  } else if (channel >= ROOST_CHAN_5G_MIN && channel <= ROOST_CHAN_5G_MAX) {
    out.band  = ROOST_BAND_5;
    out.known = 1;
  }
  return out;
}

#ifdef __cplusplus
}
#endif
