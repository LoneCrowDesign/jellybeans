// test_registry_header.cpp: host test for the generated registry header.
//
// Covers the mechanism the contract rests on: several devices declaring
// different populatable-field masks against one record type, each getting a
// header that is an order-preserving projection of the canonical field order.
//
// Compiles with no Arduino, no SD, and no PlatformIO: the header is pure data
// plus static inline accessors, so it is testable on a host.
//
//     g++ -std=c++17 -Wall -Wextra -Werror -Igenerated
//         tests/test_registry_header.cpp -o /tmp/t && /tmp/t

// Stands in for a board_config.h. Firmware always compiles as some board, so
// the header requires every capability to be declared; this file exercises the
// record and field tables rather than any board's derived mask, so it declares
// everything present and lets the tests drive masks directly.
//
// tests/board_profiles.sh is where realistic capability sets are exercised.
#define ROOST_CAP_GNSS             1
#define ROOST_CAP_STORAGE          1
#define ROOST_CAP_WIFI             1
#define ROOST_CAP_WIFI_PROMISCUOUS 1
#define ROOST_CAP_WIFI_SCAN        1
#define ROOST_CAP_IE_PARSE         1
#define ROOST_CAP_BLE              1
#define ROOST_CAP_BLE_PROMISCUOUS  1
#define ROOST_CAP_TARGET_MATCH     1
#define ROOST_CAP_OPERATOR_MARK    1

#define ROOST_COMPONENTS(X)                                                   \
  X(WIFI0, "wifi0", WIFI, "ESP32-WROOM-32", ROOST_BAND_REACH_2_4)                   \
  X(GNSS0, "gnss0", GNSS, "ATGM336", 0)                                       \
  X(SYS,   "sys",   SYSTEM, 0, 0)

#include "roost_registry.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

static int g_fail = 0;

static void check(const char *what, const std::string &got, const std::string &want) {
  if (got == want) {
    std::printf("  ok    %s\n", what);
  } else {
    std::printf("  FAIL  %s\n          got  %s\n          want %s\n",
                what, got.c_str(), want.c_str());
    g_fail++;
  }
}

static void checkTrue(const char *what, bool cond) {
  if (cond) {
    std::printf("  ok    %s\n", what);
  } else {
    std::printf("  FAIL  %s\n", what);
    g_fail++;
  }
}

static std::string header(RoostRecord r, RoostFieldMask mask) {
  char buf[512];
  const size_t n = roostHeader(buf, sizeof(buf), r, mask);
  if (n == 0) return "<empty>";
  return std::string(buf, n);
}

// Two illustrative masks over the single shared wifi_obs record type, used to
// exercise projection. These are hand-written on purpose, to test roostHeader()
// against arbitrary subsets; the masks a real board uses are derived from its
// capability macros and are exercised by tests/board_profiles.sh instead.
//
// Neither is a subset of the other, which is the property that matters: scan
// mode gets auth_mode free and promiscuous mode does not, while promiscuous mode
// gets the whole frame layer and scan mode never sees a frame.
static const RoostFieldMask kScanMode =
    ROOST_F(ROOST_WIFI_OBS_TIMESTAMP_UTC) | ROOST_F(ROOST_WIFI_OBS_UPTIME_MS) |
    ROOST_F(ROOST_WIFI_OBS_FIX_SEQ) | ROOST_F(ROOST_WIFI_OBS_CAP_COMPONENT) |
    ROOST_F(ROOST_WIFI_OBS_OBS_MODE) |
    ROOST_F(ROOST_WIFI_OBS_MAC) | ROOST_F(ROOST_WIFI_OBS_RSSI) |
    ROOST_F(ROOST_WIFI_OBS_CHANNEL) | ROOST_F(ROOST_WIFI_OBS_SSID) |
    ROOST_F(ROOST_WIFI_OBS_AUTH_MODE);

static const RoostFieldMask kPromiscMode =
    ROOST_F(ROOST_WIFI_OBS_TIMESTAMP_UTC) | ROOST_F(ROOST_WIFI_OBS_UPTIME_MS) |
    ROOST_F(ROOST_WIFI_OBS_FIX_SEQ) | ROOST_F(ROOST_WIFI_OBS_CAP_COMPONENT) |
    ROOST_F(ROOST_WIFI_OBS_OBS_MODE) |
    ROOST_F(ROOST_WIFI_OBS_MAC) | ROOST_F(ROOST_WIFI_OBS_DETECTION_METHOD) |
    ROOST_F(ROOST_WIFI_OBS_FRAME_SUBTYPE) | ROOST_F(ROOST_WIFI_OBS_RSSI) |
    ROOST_F(ROOST_WIFI_OBS_CHANNEL) | ROOST_F(ROOST_WIFI_OBS_SSID) |
    ROOST_F(ROOST_WIFI_OBS_ADDR2);

static RoostFieldMask allFields(RoostRecord r) {
  RoostFieldMask m = 0;
  for (uint8_t i = 0; i < roostRecordFieldCount(r); i++) m |= ROOST_F(i);
  return m;
}

int main() {
  std::printf("registry hash %s\n\n", ROOST_REGISTRY_HASH_SHORT);

  std::printf("one record type, arbitrary subsets, correct headers:\n");
  check("scan-style subset",
        header(ROOST_REC_WIFI_OBS, kScanMode),
        "timestamp_utc,uptime_ms,fix_seq,cap_component,obs_mode,mac,rssi,channel,ssid,"
        "auth_mode");
  check("promiscuous-style subset",
        header(ROOST_REC_WIFI_OBS, kPromiscMode),
        "timestamp_utc,uptime_ms,fix_seq,cap_component,obs_mode,mac,detection_method,"
        "frame_subtype,rssi,channel,ssid,addr2");
  check("full field set",
        header(ROOST_REC_WIFI_OBS, allFields(ROOST_REC_WIFI_OBS)),
        "timestamp_utc,uptime_ms,fix_seq,cap_component,obs_mode,mac,detection_method,"
        "frame_subtype,rssi,channel,band,ssid,auth_mode,addr1,addr2,addr3,"
        "seq,fc_flags,frame_len,bb_format,cap_info,beacon_interval,ie_ids,"
        "vendor_ies");

  std::printf("\nprojections preserve canonical order:\n");
  // A mask is a set, so the bit order cannot express a permutation. Verify the
  // emitted order tracks the record's field table rather than the mask literal.
  {
    const std::string h = header(ROOST_REC_WIFI_OBS, kPromiscMode);
    size_t prev = 0;
    bool ordered = true;
    for (uint8_t i = 0; i < roostRecordFieldCount(ROOST_REC_WIFI_OBS); i++) {
      if (!(kPromiscMode & ROOST_F(i))) continue;
      const size_t at = h.find(roostFieldName(ROOST_REC_WIFI_OBS, i));
      if (at == std::string::npos || at < prev) { ordered = false; break; }
      prev = at;
    }
    checkTrue("promiscuous subset is an order-preserving projection", ordered);
  }

  std::printf("\ngps_track and device_event:\n");
  check("gps_track header (full)",
        header(ROOST_REC_GPS_TRACK, allFields(ROOST_REC_GPS_TRACK)),
        "timestamp_utc,uptime_ms,fix_seq,cap_component,position_source,lat,lon,alt_m,"
        "speed_mps,course_deg,hdop,sats,fix_type,fix_age_ms");
  check("device_event header (full)",
        header(ROOST_REC_DEVICE_EVENT, allFields(ROOST_REC_DEVICE_EVENT)),
        "timestamp_utc,uptime_ms,fix_seq,cap_component,event_kind,event_count,event_detail");

  std::printf("\nrequired fields gate emission:\n");
  checkTrue("scan-style mask satisfies wifi_obs required",
            roostMaskSatisfiesRequired(ROOST_REC_WIFI_OBS, kScanMode));
  checkTrue("promiscuous-style mask satisfies wifi_obs required",
            roostMaskSatisfiesRequired(ROOST_REC_WIFI_OBS, kPromiscMode));
  checkTrue("a mask without mac is rejected",
            !roostMaskSatisfiesRequired(ROOST_REC_WIFI_OBS,
                                       kPromiscMode & ~ROOST_F(ROOST_WIFI_OBS_MAC)));
  checkTrue("a mask without uptime_ms is rejected",
            !roostMaskSatisfiesRequired(ROOST_REC_WIFI_OBS,
                                       kPromiscMode & ~ROOST_F(ROOST_WIFI_OBS_UPTIME_MS)));
  checkTrue("gps_track requires fix_seq",
            !roostMaskSatisfiesRequired(ROOST_REC_GPS_TRACK,
                                       allFields(ROOST_REC_GPS_TRACK) &
                                           ~ROOST_F(ROOST_GPS_TRACK_FIX_SEQ)));

  std::printf("\ntruncation leaves nothing plausible behind:\n");
  {
    char small[16];
    std::memset(small, 'X', sizeof(small));
    const size_t n = roostHeader(small, sizeof(small), ROOST_REC_WIFI_OBS, kPromiscMode);
    checkTrue("overflow returns 0", n == 0);
    checkTrue("overflow empties the buffer rather than truncating", small[0] == '\0');
  }

  std::printf("\nfilenames carry the version:\n");
  {
    char name[64];
    roostFileName(name, sizeof(name), ROOST_REC_WIFI_OBS);
    check("wifi_obs filename", name, "wifi_obs.v2.csv");
    roostFileName(name, sizeof(name), ROOST_REC_GPS_TRACK);
    check("gps_track filename", name, "gps_track.v1.csv");
  }

  std::printf("\nble_obs carries the full union, and a scan device pays nothing:\n");
  check("ble_obs header (promiscuous, full)",
        header(ROOST_REC_BLE_OBS, allFields(ROOST_REC_BLE_OBS)),
        "timestamp_utc,uptime_ms,fix_seq,cap_component,obs_mode,mac,addr_type,"
        "detection_method,rssi,tx_power,device_name,pdu_type,phy_primary,"
        "phy_secondary,sid,active_scan,adv_data_hex");
  {
    // Declared subsets are what let ble_obs carry promiscuous-only detail
    // without changing the header a scan-mode device writes.
    const RoostFieldMask kScanModeBle =
        ROOST_F(ROOST_BLE_OBS_TIMESTAMP_UTC) | ROOST_F(ROOST_BLE_OBS_UPTIME_MS) |
        ROOST_F(ROOST_BLE_OBS_FIX_SEQ) | ROOST_F(ROOST_BLE_OBS_CAP_COMPONENT) |
        ROOST_F(ROOST_BLE_OBS_OBS_MODE) |
        ROOST_F(ROOST_BLE_OBS_MAC) | ROOST_F(ROOST_BLE_OBS_RSSI) |
        ROOST_F(ROOST_BLE_OBS_DEVICE_NAME);
    check("ble_obs header (scan, subset)",
          header(ROOST_REC_BLE_OBS, kScanModeBle),
          "timestamp_utc,uptime_ms,fix_seq,cap_component,obs_mode,mac,rssi,device_name");
    checkTrue("the scan-mode ble subset still satisfies required",
              roostMaskSatisfiesRequired(ROOST_REC_BLE_OBS, kScanModeBle));
  }
  {
    // A BLE local name is not a network name and a BLE advertising channel is
    // not an 802.11 channel. Record types never borrow each other's field
    // names, so one column header never covers two different measurements.
    bool leaked = false;
    for (uint8_t i = 0; i < roostRecordFieldCount(ROOST_REC_BLE_OBS); i++) {
      const char *f = roostFieldName(ROOST_REC_BLE_OBS, i);
      if (!std::strcmp(f, "ssid") || !std::strcmp(f, "channel")) leaked = true;
    }
    checkTrue("ble_obs carries neither ssid nor channel", !leaked);
  }

  std::printf("\nshared vocabularies are restricted per record type:\n");
  checkTrue("wifi_obs accepts wildcard_probe",
            roostValueAllowed(ROOST_REC_WIFI_OBS, ROOST_WIFI_OBS_DETECTION_METHOD,
                             ROOST_DETECTION_METHOD_WILDCARD_PROBE));
  checkTrue("wifi_obs rejects ble_oui",
            !roostValueAllowed(ROOST_REC_WIFI_OBS, ROOST_WIFI_OBS_DETECTION_METHOD,
                              ROOST_DETECTION_METHOD_BLE_OUI));
  checkTrue("ble_obs accepts ble_mfr",
            roostValueAllowed(ROOST_REC_BLE_OBS, ROOST_BLE_OBS_DETECTION_METHOD,
                             ROOST_DETECTION_METHOD_BLE_MFR));
  checkTrue("ble_obs rejects oui_addr1",
            !roostValueAllowed(ROOST_REC_BLE_OBS, ROOST_BLE_OBS_DETECTION_METHOD,
                              ROOST_DETECTION_METHOD_OUI_ADDR1));
  checkTrue("both accept unmatched",
            roostValueAllowed(ROOST_REC_WIFI_OBS, ROOST_WIFI_OBS_DETECTION_METHOD,
                             ROOST_DETECTION_METHOD_UNMATCHED) &&
                roostValueAllowed(ROOST_REC_BLE_OBS, ROOST_BLE_OBS_DETECTION_METHOD,
                                 ROOST_DETECTION_METHOD_UNMATCHED));
  checkTrue("an unrestricted field accepts anything",
            roostValueAllowed(ROOST_REC_GPS_TRACK, ROOST_GPS_TRACK_POSITION_SOURCE,
                             ROOST_POSITION_SOURCE_DEVICE_STALE));

  std::printf("\nenum vocabularies are closed:\n");
  check("obs_mode promiscuous", kRoostObsMode[ROOST_OBS_MODE_PROMISCUOUS], "promiscuous");
  check("obs_mode scan", kRoostObsMode[ROOST_OBS_MODE_SCAN], "scan");
  check("position_source device_stale",
        kRoostPositionSource[ROOST_POSITION_SOURCE_DEVICE_STALE], "device_stale");
  check("band 2.4 wire value differs from its C identifier",
        kRoostBand[ROOST_BAND_2_4], "2.4");
  checkTrue("position_source has exactly 5 values", ROOST_POSITION_SOURCE_COUNT == 5);

  std::printf("\nthe row builder owns the rules rather than trusting the writer:\n");
  {
    char b[1024];
    RoostRow r;
    const RoostFieldMask M = ROOST_WIFI_OBS_COLUMNS_MASK;
    const uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0x11, 0x22, 0x33};

    roostRowBegin(&r, b, sizeof(b), ROOST_REC_WIFI_OBS, M);
    roostRowSetText(&r, ROOST_WIFI_OBS_TIMESTAMP_UTC, "2026-08-04T10:00:00Z");
    roostRowSetUInt(&r, ROOST_WIFI_OBS_UPTIME_MS, 123456);
    roostRowSetText(&r, ROOST_WIFI_OBS_CAP_COMPONENT, roostComponentId(ROOST_COMP_WIFI0));
    roostRowSetEnum(&r, ROOST_WIFI_OBS_OBS_MODE, ROOST_OBS_MODE_SCAN);
    roostRowSetMac(&r, ROOST_WIFI_OBS_MAC, mac);
    roostRowSetInt(&r, ROOST_WIFI_OBS_RSSI, -61);
    roostRowSetText(&r, ROOST_WIFI_OBS_SSID, "my,\"odd\"\r\nssid");
    checkTrue("a complete row finishes", roostRowFinish(&r) > 0);
    checkTrue("MACs are lowercased at the write path",
              std::string(b).find("aa:bb:cc:11:22:33") != std::string::npos);
    checkTrue("embedded quotes are doubled and the field is quoted",
              std::string(b).find("\"my,\"\"odd\"\"") != std::string::npos);
    // Encoded, not stripped and not left raw. Raw would be legal RFC 4180 and
    // would make every line-oriented tool that touches the file wrong.
    checkTrue("control bytes are encoded rather than emitted raw",
              std::string(b).find("\\x0d\\x0assid") != std::string::npos);
    checkTrue("and no raw control byte reaches the row",
              std::string(b).find('\r') == std::string::npos &&
              std::string(b).find('\n') == std::string::npos);

    // A row is interpretable only if it has exactly as many fields as the
    // header has columns. Empty leading columns are the case that separates
    // "bytes written" from "field emitted": a row taken before the clock
    // anchors leaves timestamp_utc empty, and a row short one field still
    // parses. Counting fields against the header is what catches it.
    {
      char hdr[1024];
      roostHeader(hdr, sizeof(hdr), ROOST_REC_WIFI_OBS, M);
      const size_t cols = std::count(hdr, hdr + std::strlen(hdr), ',') + 1;

      RoostRow e;
      char eb[1024];
      // No timestamp_utc and no fix_seq: the shape of a row taken before the
      // GPS reports a fix.
      roostRowBegin(&e, eb, sizeof(eb), ROOST_REC_WIFI_OBS, M);
      roostRowSetUInt(&e, ROOST_WIFI_OBS_UPTIME_MS, 1234);
      roostRowSetText(&e, ROOST_WIFI_OBS_CAP_COMPONENT,
                      roostComponentId(ROOST_COMP_WIFI0));
      roostRowSetEnum(&e, ROOST_WIFI_OBS_OBS_MODE, ROOST_OBS_MODE_SCAN);
      roostRowSetMac(&e, ROOST_WIFI_OBS_MAC, mac);
      roostRowSetInt(&e, ROOST_WIFI_OBS_RSSI, -61);
      checkTrue("a row with empty leading columns finishes",
                roostRowFinish(&e) > 0);
      const size_t got = std::count(eb, eb + std::strlen(eb), ',') + 1;
      checkTrue("and has exactly as many fields as the header has columns",
                got == cols);
      checkTrue("with the empty leading column still occupying its place",
                eb[0] == ',');
    }

    // Producers commonly return name strings. Resolving a name against the
    // registry is a first-class setter rather than something each device
    // reimplements, so every device treats an unresolvable name the same way.
    {
      RoostRow n;
      char nb[1024];
      roostRowBegin(&n, nb, sizeof(nb), ROOST_REC_WIFI_OBS, M);
      roostRowSetUInt(&n, ROOST_WIFI_OBS_UPTIME_MS, 1);
      roostRowSetText(&n, ROOST_WIFI_OBS_CAP_COMPONENT,
                      roostComponentId(ROOST_COMP_WIFI0));
      roostRowSetEnum(&n, ROOST_WIFI_OBS_OBS_MODE, ROOST_OBS_MODE_SCAN);
      roostRowSetMac(&n, ROOST_WIFI_OBS_MAC, mac);
      checkTrue("a wire spelling resolves to its enum value",
                roostRowSetEnumByName(&n, ROOST_WIFI_OBS_FRAME_SUBTYPE,
                                      "beacon") != 0);
      roostRowSetInt(&n, ROOST_WIFI_OBS_RSSI, -61);
      // An invented name: the shape of a producer drifting from the registry.
      checkTrue("an unresolvable name does not set the column",
                roostRowSetEnumByName(&n, ROOST_WIFI_OBS_BB_FORMAT,
                                      "mgmt_other") == 0);
      checkTrue("and counts itself rather than passing silently",
                n.unknownEnums == 1);

      // No value is not drift. A producer with nothing to report for an
      // optional enum passes an empty name, which is common and expected; the
      // counter would be useless for finding real drift if those were counted.
      checkTrue("an empty name does not set the column either",
                roostRowSetEnumByName(&n, ROOST_WIFI_OBS_BAND, "") == 0);
      checkTrue("but is NOT counted as drift", n.unknownEnums == 1);
      checkTrue("nor is a null name",
                roostRowSetEnumByName(&n, ROOST_WIFI_OBS_BAND, nullptr) == 0);
      checkTrue("still not counted as drift", n.unknownEnums == 1);

      checkTrue("and costs the field, not the observation",
                roostRowFinish(&n) > 0);
      checkTrue("leaving that column empty",
                std::string(nb).find("beacon") != std::string::npos);
    }

    // A required enum column is the one case where an unresolvable name must
    // cost the whole row. No special case does this: the column is simply
    // never written, and the required check refuses on its own.
    {
      RoostRow n;
      char nb[1024];
      roostRowBegin(&n, nb, sizeof(nb), ROOST_REC_WIFI_OBS, M);
      roostRowSetUInt(&n, ROOST_WIFI_OBS_UPTIME_MS, 1);
      roostRowSetText(&n, ROOST_WIFI_OBS_CAP_COMPONENT,
                      roostComponentId(ROOST_COMP_WIFI0));
      roostRowSetEnumByName(&n, ROOST_WIFI_OBS_OBS_MODE, "not_a_mode");
      roostRowSetMac(&n, ROOST_WIFI_OBS_MAC, mac);
      roostRowSetInt(&n, ROOST_WIFI_OBS_RSSI, -61);
      checkTrue("an unresolvable REQUIRED enum voids the row",
                roostRowFinish(&n) == 0);
    }

    {
      RoostRow bs;
      char bb[1024];
      roostRowBegin(&bs, bb, sizeof(bb), ROOST_REC_WIFI_OBS, M);
      roostRowSetText(&bs, ROOST_WIFI_OBS_SSID, "back\\slash");
      checkTrue("a literal backslash is doubled, so decoding is unambiguous",
                std::string(bb).find("back\\\\slash") != std::string::npos);
    }

    // Text encoding revision 2 escapes on UTF-8 validity, not on byte range.
    // Both directions are checked: an ill-formed byte must not reach the file,
    // and a well-formed one must not be escaped.
    std::printf("\ntext encoding escapes what is ill-formed and nothing else:\n");
    {
      checkTrue("the header states which revision it implements",
                ROOST_TEXT_ENCODING_REV == 2);

      struct Case {
        const char *what;
        const char *in;
        size_t len;
        const char *want;
      };
      static const Case kCases[] = {
          {"a lone 0xff is escaped", "\xff", 1, "\\xff"},
          {"a bare continuation byte is escaped", "\x80", 1, "\\x80"},
          {"an overlong NUL cannot smuggle past the C0 escape", "\xc0\x80", 2,
           "\\xc0\\x80"},
          {"an encoded surrogate is escaped", "\xed\xa0\x80", 3,
           "\\xed\\xa0\\x80"},
          {"a code point above U+10FFFF is escaped", "\xf5\x80\x80\x80", 4,
           "\\xf5\\x80\\x80\\x80"},
          {"a sequence truncated by the field limit is escaped", "ab\xf0\x9f", 4,
           "ab\\xf0\\x9f"},
          {"only the ill-formed byte is escaped, scanning resumes at the next",
           "\xff" "A", 2, "\\xffA"},
          // Correctly encoded text passes through byte for byte.
          {"two-byte UTF-8 passes through untouched", "caf\xc3\xa9", 5,
           "caf\xc3\xa9"},
          {"three-byte UTF-8 passes through untouched", "\xe6\x97\xa5", 3,
           "\xe6\x97\xa5"},
          {"four-byte UTF-8 passes through untouched", "\xf0\x9f\x93\xb6", 4,
           "\xf0\x9f\x93\xb6"},
      };
      for (const Case &c : kCases) {
        RoostRow u;
        char ub[1024];
        roostRowBegin(&u, ub, sizeof(ub), ROOST_REC_WIFI_OBS, M);
        roostRowSetTextN(&u, ROOST_WIFI_OBS_SSID, c.in, c.len);
        checkTrue(c.what, std::string(ub).find(c.want) != std::string::npos);
      }

      // The length-taking form exists because the NUL-terminated one cannot
      // express this value at all, and an SSID is 0-32 arbitrary octets.
      RoostRow z;
      char zb[1024];
      roostRowBegin(&z, zb, sizeof(zb), ROOST_REC_WIFI_OBS, M);
      roostRowSetTextN(&z, ROOST_WIFI_OBS_SSID, "AB\x00" "CD", 5);
      checkTrue("an embedded NUL survives roostRowSetTextN",
                std::string(zb).find("AB\\x00CD") != std::string::npos);
    }

    // Canonical order is enforced by the API rather than by convention: a
    // column already passed cannot be filled in afterwards.
    roostRowBegin(&r, b, sizeof(b), ROOST_REC_WIFI_OBS, M);
    roostRowSetInt(&r, ROOST_WIFI_OBS_RSSI, -50);
    checkTrue("a column already passed cannot be filled in afterwards",
              roostRowSetUInt(&r, ROOST_WIFI_OBS_UPTIME_MS, 1) == 0);
    checkTrue("and the attempt voids the row rather than misplacing the value",
              roostRowFinish(&r) == 0 && b[0] == '\0');

    roostRowBegin(&r, b, sizeof(b), ROOST_REC_WIFI_OBS, M);
    checkTrue("a setter of the wrong class for the field is refused",
              roostRowSetInt(&r, ROOST_WIFI_OBS_SSID, 5) == 0);

    roostRowBegin(&r, b, sizeof(b), ROOST_REC_WIFI_OBS, M);
    checkTrue("a BLE detection method cannot reach a Wi-Fi row",
              roostRowSetEnum(&r, ROOST_WIFI_OBS_DETECTION_METHOD,
                              ROOST_DETECTION_METHOD_BLE_OUI) == 0);

    roostRowBegin(&r, b, sizeof(b), ROOST_REC_WIFI_OBS, M);
    roostRowSetText(&r, ROOST_WIFI_OBS_TIMESTAMP_UTC, "2026-08-04T10:00:00Z");
    checkTrue("a row missing a required field does not finish",
              roostRowFinish(&r) == 0);
    checkTrue("and leaves nothing rather than a partial row, which would parse",
              b[0] == '\0');

    // Precision belongs to the field, so two devices cannot disagree on it.
    roostRowBegin(&r, b, sizeof(b), ROOST_REC_GPS_TRACK, ROOST_GPS_TRACK_COLUMNS_MASK);
    roostRowSetUInt(&r, ROOST_GPS_TRACK_UPTIME_MS, 1);
    roostRowSetUInt(&r, ROOST_GPS_TRACK_FIX_SEQ, 1);
    roostRowSetText(&r, ROOST_GPS_TRACK_CAP_COMPONENT, roostComponentId(ROOST_COMP_GNSS0));
    roostRowSetEnum(&r, ROOST_GPS_TRACK_POSITION_SOURCE, ROOST_POSITION_SOURCE_DEVICE_FIX);
    roostRowSetFloat(&r, ROOST_GPS_TRACK_LAT, 42.3456789);
    roostRowSetFloat(&r, ROOST_GPS_TRACK_HDOP, 1.2345);
    checkTrue("gps_track finishes", roostRowFinish(&r) > 0);
    checkTrue("lat takes the registry's 6 places, hdop its own 2",
              std::string(b).find("42.345679") != std::string::npos &&
              std::string(b).find(",1.23,") != std::string::npos);

    // One writer serves every device: setting a field this device does not
    // declare is a no-op, not an error and not a stray column.
    const RoostFieldMask narrow =
        ROOST_F(ROOST_WIFI_OBS_UPTIME_MS) | ROOST_F(ROOST_WIFI_OBS_CAP_COMPONENT) |
        ROOST_F(ROOST_WIFI_OBS_OBS_MODE) | ROOST_F(ROOST_WIFI_OBS_MAC) |
        ROOST_F(ROOST_WIFI_OBS_RSSI);
    roostRowBegin(&r, b, sizeof(b), ROOST_REC_WIFI_OBS, narrow);
    roostRowSetUInt(&r, ROOST_WIFI_OBS_UPTIME_MS, 1);
    roostRowSetText(&r, ROOST_WIFI_OBS_CAP_COMPONENT, "wifi0");
    roostRowSetEnum(&r, ROOST_WIFI_OBS_OBS_MODE, ROOST_OBS_MODE_SCAN);
    roostRowSetMac(&r, ROOST_WIFI_OBS_MAC, mac);
    roostRowSetInt(&r, ROOST_WIFI_OBS_RSSI, -70);
    roostRowSetText(&r, ROOST_WIFI_OBS_SSID, "not declared on this device");
    check("a scan device writing promiscuous fields emits its own columns only",
          std::string(b, roostRowFinish(&r)), "1,wifi0,scan,aa:bb:cc:11:22:33,-70");
  }

  std::printf("\ncomponents are declared once in board config and rendered:\n");
  {
    char buf[512];
    const size_t n = roostManifestComponents(buf, sizeof(buf));
    checkTrue("renders", n > 0);
    check("from the board's ROOST_COMPONENTS list, not a hand-written string",
          std::string(buf, n),
          "[{\"id\":\"wifi0\",\"kind\":\"wifi\",\"part\":\"ESP32-WROOM-32\","
          "\"bands\":[\"2.4\"]},"
          "{\"id\":\"gnss0\",\"kind\":\"gnss\",\"part\":\"ATGM336\",\"bands\":null},"
          "{\"id\":\"sys\",\"kind\":\"system\",\"part\":null,\"bands\":null}]");
    // Band reach distinguishes a radio that could never have heard 5 GHz from
    // one that was tuned elsewhere, so it is declared per component rather than
    // per device.
    checkTrue("a 2.4-only radio declares it",
              roostComponentBandMask(ROOST_COMP_WIFI0) == ROOST_BAND_REACH_2_4);
    checkTrue("a non-radio component declares no bands",
              roostComponentBandMask(ROOST_COMP_GNSS0) == 0);
    checkTrue("ids are usable as row values", roostComponentsValid());
    check("and the row writer names one by identifier, never by string",
          roostComponentId(ROOST_COMP_GNSS0), "gnss0");
    // Overflow empties rather than truncating: half a component list is worse
    // than none, for the same reason a truncated header is.
    char tiny[8];
    checkTrue("overflow empties rather than truncating",
              roostManifestComponents(tiny, sizeof(tiny)) == 0 && tiny[0] == '\0');
  }

  std::printf("\nconfig_change is key/value, and its vocabulary is synthesized:\n");
  check("config_change header",
        header(ROOST_REC_CONFIG_CHANGE, allFields(ROOST_REC_CONFIG_CHANGE)),
        "timestamp_utc,uptime_ms,fix_seq,cap_component,setting,value");
  {
    char name[64];
    roostFileName(name, sizeof(name), ROOST_REC_CONFIG_CHANGE);
    check("config_change filename", name, "config_change.v1.csv");
  }
  // The setting vocabulary comes from the runtime tier of manifest.toml, so a
  // knob is declared once. Nothing here is hand-written in enums.toml.
  check("a runtime-tier key became a setting value",
        kRoostConfigSetting[ROOST_CONFIG_SETTING_COUNTRY_CODE], "country_code");
  check("and so did the mode",
        kRoostConfigSetting[ROOST_CONFIG_SETTING_OBS_MODE], "obs_mode");
  checkTrue("value is not restated as a column per setting",
            roostRecordFieldCount(ROOST_REC_CONFIG_CHANGE) == 6);
  // No old_value: the previous row for the setting carries it, and boot rows
  // guarantee a previous row exists.
  {
    bool hasOld = false;
    for (uint8_t i = 0; i < roostRecordFieldCount(ROOST_REC_CONFIG_CHANGE); i++)
      if (!std::strcmp(roostFieldName(ROOST_REC_CONFIG_CHANGE, i), "old_value"))
        hasOld = true;
    checkTrue("no old_value column", !hasOld);
  }

  std::printf("\noperator_mark records a place, not a device:\n");
  check("operator_mark header",
        header(ROOST_REC_OPERATOR_MARK, allFields(ROOST_REC_OPERATOR_MARK)),
        "timestamp_utc,uptime_ms,fix_seq,cap_component");
  {
    // A mark that carried a mac would aggregate every press in a session into
    // one device that does not exist, and an rssi would be a fabricated
    // measurement. Excluding both from the record type is what prevents it.
    bool borrowed = false;
    for (uint8_t i = 0; i < roostRecordFieldCount(ROOST_REC_OPERATOR_MARK); i++) {
      const char *f = roostFieldName(ROOST_REC_OPERATOR_MARK, i);
      if (!std::strcmp(f, "mac") || !std::strcmp(f, "rssi") ||
          !std::strcmp(f, "detection_method"))
        borrowed = true;
    }
    checkTrue("carries no mac, rssi, or detection_method", !borrowed);
  }
  // Position is the point, but requiring it would bar a GNSS-less device from
  // marking at all; a co-located device's track resolves a time-only mark.
  checkTrue("fix_seq is not required",
            roostMaskSatisfiesRequired(
                ROOST_REC_OPERATOR_MARK,
                allFields(ROOST_REC_OPERATOR_MARK) &
                    ~ROOST_F(ROOST_OPERATOR_MARK_FIX_SEQ)));

  std::printf("\nmanifest contract half is rendered, not written:\n");
  {
    // A build with no BLE radio declares no ble_obs file at all. Its wifi_obs
    // columns are its mask, and capable == columns because nothing it can
    // measure is being withheld.
    const RoostFileDecl decls[] = {
        {ROOST_REC_WIFI_OBS, kPromiscMode, kPromiscMode},
        {ROOST_REC_GPS_TRACK, allFields(ROOST_REC_GPS_TRACK),
         allFields(ROOST_REC_GPS_TRACK)},
    };
    char buf[2048];
    const size_t n = roostManifestFiles(buf, sizeof(buf), decls, 2, nullptr, 0);
    checkTrue("renders", n > 0);
    const std::string j(buf, n);
    checkTrue("declares the record type", j.find("\"record\":\"wifi_obs\"") != std::string::npos);
    checkTrue("declares the version", j.find("\"version\":1") != std::string::npos);
    checkTrue("declares the filename", j.find("\"name\":\"wifi_obs.v2.csv\"") != std::string::npos);
    checkTrue("columns match the header the device will write",
              j.find("\"columns\":[\"timestamp_utc\",\"uptime_ms\",\"fix_seq\","
                     "\"cap_component\",\"obs_mode\",\"mac\",\"detection_method\",\"frame_subtype\","
                     "\"rssi\",\"channel\",\"ssid\",\"addr2\"]") != std::string::npos);
    checkTrue("nothing capable is left unrecorded",
              j.find("\"capable_unrecorded\":[]") != std::string::npos);
    if (g_fail) std::printf("      rendered: %s\n", j.c_str());
  }
  {
    // The three-state case. A build whose hardware can reach auth_mode but whose
    // configuration does not record it must be distinguishable from one whose
    // hardware cannot reach it at all.
    const RoostFieldMask capable = kPromiscMode | ROOST_F(ROOST_WIFI_OBS_AUTH_MODE);
    const RoostFileDecl d = {ROOST_REC_WIFI_OBS, kPromiscMode, capable};
    char buf[2048];
    const size_t n = roostManifestFiles(buf, sizeof(buf), &d, 1, nullptr, 0);
    const std::string j(buf, n);
    checkTrue("capable-but-unrecorded is stated explicitly",
              j.find("\"capable_unrecorded\":[\"auth_mode\"]") != std::string::npos);
    checkTrue("and it is absent from columns",
              j.find("\"auth_mode\"]}") != std::string::npos &&
                  j.find("\"ssid\",\"auth_mode\"") == std::string::npos);
  }
  {
    // A declaration that claims to emit a column the hardware cannot fill is
    // incoherent, and is refused rather than rendered.
    const RoostFileDecl bad = {ROOST_REC_WIFI_OBS, kPromiscMode,
                               kPromiscMode & ~ROOST_F(ROOST_WIFI_OBS_MAC)};
    checkTrue("columns exceeding capable is invalid", !roostFileDeclValid(&bad));
    char buf[2048];
    std::memset(buf, 'X', sizeof(buf));
    checkTrue("and renders nothing at all",
              roostManifestFiles(buf, sizeof(buf), &bad, 1, nullptr, 0) == 0 && buf[0] == '\0');

    const RoostFileDecl missing = {ROOST_REC_WIFI_OBS,
                                   kPromiscMode & ~ROOST_F(ROOST_WIFI_OBS_RSSI),
                                   kPromiscMode};
    checkTrue("a mask missing a required field is invalid",
              !roostFileDeclValid(&missing));
  }

  std::printf("\nthe pcap declares itself through the same producer:\n");
  {
    char buf[256];
    const size_t n = roostManifestPcapFile(buf, sizeof(buf), "frames.pcap", 127,
                                           256, ROOST_TIMEBASE_UTC);
    const std::string j(buf, n);
    checkTrue("renders", n > 0);
    checkTrue("declares linktype and snaplen",
              j.find("\"linktype\":127") != std::string::npos &&
                  j.find("\"snaplen\":256") != std::string::npos);
    checkTrue("timebase is the caller's, rendered verbatim",
              j.find("\"timebase\":\"utc\"") != std::string::npos);
    const size_t m = roostManifestPcapFile(buf, sizeof(buf), "frames.pcap", 127,
                                           256, ROOST_TIMEBASE_BOOT);
    checkTrue("and boot when that is what the file holds",
              std::string(buf, m).find("\"timebase\":\"boot\"") != std::string::npos);

    // Zeroed memory must claim the timebase that needs the anchor, not utc.
    checkTrue("boot is the zero value", ROOST_TIMEBASE_BOOT == 0);

    // Renders nothing rather than indexing the name table off the end.
    checkTrue("an out-of-range timebase is refused",
              roostManifestPcapFile(buf, sizeof(buf), "frames.pcap", 127, 256,
                                    (RoostTimebase)ROOST_TIMEBASE_COUNT) == 0);

    char small[8];
    std::memset(small, 'X', sizeof(small));
    checkTrue("overflow empties rather than truncating",
              roostManifestPcapFile(small, sizeof(small), "frames.pcap", 127,
                                    256, ROOST_TIMEBASE_UTC) == 0 &&
                  small[0] == '\0');
  }

  std::printf("\n%s\n", g_fail ? "FAILED" : "all checks passed");
  return g_fail ? 1 : 0;
}
