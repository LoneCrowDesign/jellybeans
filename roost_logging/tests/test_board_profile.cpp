// test_board_profile.cpp: the capability mask is derived, not written.
//
// Compiled once per capability profile, with that profile's capability macros
// supplied on the command line the way a board_config.h would supply them, and
// prints what the build declares. tests/board_profiles.sh drives it and fails
// when a profile that should compile does not, or one that should not does.
//
// A board with an SD card fitted and one without differ only in their
// board_config.h, and everything the manifest says about them follows from that
// difference alone.

#ifndef ROOST_SKIP_COMPONENT_DECL
#define ROOST_COMPONENTS(X)                                                   \
  X(WIFI0, "wifi0", WIFI, "ESP32-WROOM-32", ROOST_BAND_REACH_2_4)                   \
  X(GNSS0, "gnss0", GNSS, "ATGM336", 0)                                       \
  X(SYS,   "sys",   SYSTEM, 0, 0)
#endif

#include "roost_registry.h"

#include <cstdio>

static void dump(const char *label, RoostRecord r, int emits,
                 RoostFieldMask capable, RoostFieldMask columns) {
  std::printf("%s: ", label);
  if (!emits) {
    std::printf("not emitted\n");
    return;
  }
  char buf[512];
  if (!roostHeader(buf, sizeof(buf), r, columns)) {
    std::printf("HEADER DID NOT FIT\n");
    return;
  }
  std::printf("%s\n", buf);

  const RoostFieldMask unrecorded = capable & ~columns;
  std::printf("%s.capable_unrecorded:", label);
  if (!unrecorded) {
    std::printf(" (none)");
  } else {
    for (uint8_t i = 0; i < roostRecordFieldCount(r); i++) {
      if (unrecorded & ROOST_F(i)) std::printf(" %s", roostFieldName(r, i));
    }
  }
  std::printf("\n");
}

int main() {
  std::printf("profile: %s\n", ROOST_PROFILE_NAME);
  dump("wifi_obs", ROOST_REC_WIFI_OBS, ROOST_EMITS_WIFI_OBS,
       ROOST_WIFI_OBS_CAPABLE_MASK, ROOST_WIFI_OBS_COLUMNS_MASK);
  dump("ble_obs", ROOST_REC_BLE_OBS, ROOST_EMITS_BLE_OBS,
       ROOST_BLE_OBS_CAPABLE_MASK, ROOST_BLE_OBS_COLUMNS_MASK);
  dump("gps_track", ROOST_REC_GPS_TRACK, ROOST_EMITS_GPS_TRACK,
       ROOST_GPS_TRACK_CAPABLE_MASK, ROOST_GPS_TRACK_COLUMNS_MASK);
  dump("config_change", ROOST_REC_CONFIG_CHANGE, ROOST_EMITS_CONFIG_CHANGE,
       ROOST_CONFIG_CHANGE_CAPABLE_MASK, ROOST_CONFIG_CHANGE_COLUMNS_MASK);
  dump("operator_mark", ROOST_REC_OPERATOR_MARK, ROOST_EMITS_OPERATOR_MARK,
       ROOST_OPERATOR_MARK_CAPABLE_MASK, ROOST_OPERATOR_MARK_COLUMNS_MASK);
  dump("device_event", ROOST_REC_DEVICE_EVENT, ROOST_EMITS_DEVICE_EVENT,
       ROOST_DEVICE_EVENT_CAPABLE_MASK, ROOST_DEVICE_EVENT_COLUMNS_MASK);
  return 0;
}
