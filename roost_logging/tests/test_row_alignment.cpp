// test_row_alignment.cpp: a finished row always has as many fields as its
// header has columns.
//
// One invariant, checked exhaustively: for every record type and every pattern
// of which declared columns get a value, a row that finishes must have exactly
// as many fields as roostHeader() renders columns. A row that has fewer is not
// a broken row: it is a row whose every later value is in the wrong column,
// and it parses, so nothing downstream can catch it.
//
// The case that needs the coverage is an empty leading column. A separator rule
// that keys on "bytes written" rather than "field emitted" agrees with itself
// everywhere except there, and timestamp_utc is empty by design on every row
// taken before the clock anchors.
//
// test_registry_header.cpp covers that one case directly; this covers the whole
// space, because a later variant of the same mistake need not be in the same
// place.
//
//     g++ -std=c++17 -Wall -Wextra -Werror -Igenerated
//         tests/test_row_alignment.cpp -o /tmp/t && /tmp/t

#include <cstdio>
#include <cstring>
#define ROOST_CAP_GNSS 1
#define ROOST_CAP_STORAGE 1
#define ROOST_CAP_WIFI 1
#define ROOST_CAP_WIFI_PROMISCUOUS 1
#define ROOST_CAP_WIFI_SCAN 1
#define ROOST_CAP_IE_PARSE 1
#define ROOST_CAP_BLE 1
#define ROOST_CAP_BLE_PROMISCUOUS 1
#define ROOST_CAP_TARGET_MATCH 1
#define ROOST_CAP_OPERATOR_MARK 1
#define ROOST_COMPONENTS(X) X(W, "w", WIFI, "c5", ROOST_BAND_REACH_5)
#include "roost_registry.h"

// Counts CSV fields, respecting quotes: a quoted SSID may legally contain a
// comma, and counting those as separators would make this test agree with a
// misaligned row.
static size_t fieldsIn(const char *s) {
  size_t n = 1; int q = 0;
  for (const char *p = s; *p; p++) {
    if (*p == '"') q = !q;
    else if (*p == ',' && !q) n++;
  }
  return n;
}

static RoostFieldMask maskFor(RoostRecord r) {
  switch (r) {
    case ROOST_REC_WIFI_OBS: return ROOST_WIFI_OBS_COLUMNS_MASK;
    case ROOST_REC_BLE_OBS: return ROOST_BLE_OBS_COLUMNS_MASK;
    case ROOST_REC_GPS_TRACK: return ROOST_GPS_TRACK_COLUMNS_MASK;
    case ROOST_REC_DEVICE_EVENT: return ROOST_DEVICE_EVENT_COLUMNS_MASK;
    case ROOST_REC_CONFIG_CHANGE: return ROOST_CONFIG_CHANGE_COLUMNS_MASK;
    default: return ROOST_OPERATOR_MARK_COLUMNS_MASK;
  }
}

// Sets field i with a value appropriate to its type. Empty text/mac/hex are
// the interesting cases: they write nothing, which is what exposed the bug.
static void setOne(RoostRow *r, RoostRecord rec, uint8_t i, bool empty) {
  const uint8_t mac[6] = {1,2,3,4,5,6};
  const uint8_t hex[2] = {0xab, 0xcd};
  switch (roostFieldTypeOf(rec, i)) {
    case ROOST_FT_TEXT:  roostRowSetText(r, i, empty ? "" : "v"); break;
    case ROOST_FT_MAC:   roostRowSetMac(r, i, empty ? nullptr : mac); break;
    case ROOST_FT_HEX:   roostRowSetHex(r, i, empty ? nullptr : hex, empty ? 0 : 2); break;
    case ROOST_FT_ENUM:  roostRowSetEnum(r, i, 0); break;
    case ROOST_FT_BOOL:  roostRowSetBool(r, i, 1); break;
    case ROOST_FT_UINT:  roostRowSetUInt(r, i, 7); break;
    case ROOST_FT_INT:   roostRowSetInt(r, i, -7); break;
    case ROOST_FT_FLOAT: roostRowSetFloat(r, i, 1.5); break;
  }
}

int main() {
  int checked = 0, bad = 0, finished = 0;
  for (int rec = 0; rec < ROOST_REC_COUNT; rec++) {
    const RoostRecord R = (RoostRecord)rec;
    const RoostFieldMask M = maskFor(R);
    const uint8_t count = roostRecordFieldCount(R);

    char hdr[2048];
    roostHeader(hdr, sizeof(hdr), R, M);
    const size_t cols = fieldsIn(hdr);

    // Every subset of "set vs skip" for up to 16 fields; beyond that, every
    // subset of the first 16 with the rest always set.
    const int span = count < 16 ? count : 16;
    for (uint32_t pattern = 0; pattern < (1u << span); pattern++) {
      for (int emptyMode = 0; emptyMode < 2; emptyMode++) {
        char b[4096];
        RoostRow r;
        roostRowBegin(&r, b, sizeof(b), R, M);
        for (uint8_t i = 0; i < count; i++) {
          if (!(M & ROOST_F(i))) continue;
          const bool skip = (i < span) && !((pattern >> i) & 1u);
          if (skip) continue;
          setOne(&r, R, i, emptyMode == 1);
        }
        const size_t n = roostRowFinish(&r);
        checked++;
        if (!n) continue;  // refused: required field missing, which is correct
        finished++;
        if (fieldsIn(b) != cols) {
          if (bad < 3) {
            printf("MISALIGNED %s pattern=%u empty=%d: %zu fields, header has %zu\n",
                   roostRecordName(R), pattern, emptyMode, fieldsIn(b), cols);
          }
          bad++;
        }
      }
    }
  }
  if (bad) {
    printf("\n  FAIL  %d of %d finished rows misaligned\n", bad, finished);
    return 1;
  }
  printf("  ok    %d rows built, %d finished, every one aligned with its header\n",
         checked, finished);
  return 0;
}
