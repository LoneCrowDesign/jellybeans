#!/usr/bin/env bash
# Compile test_board_profile.cpp once per capability shape, supplying macros the
# way a board_config.h would, and print what each build declares. A profile
# fails when it does not compile, and the negative cases below fail when they
# do. The printed declarations are for reading; nothing diffs them.
#
# The same source and the same registry produce four different declarations, and
# the only input that differs is a handful of compile-time defines. Nothing
# about a device's capability mask is written down twice.
#
# Profiles are named after capability shapes, not devices. A device declares its
# own capabilities in its own board header, which is the single place that fact
# lives; a profile named after a device would be a second copy of that
# declaration in a repo that does not own it. Each device asserts its own
# declaration in its own host tests.
#
# The last profile is promiscuous hardware with no SD card: identical to the
# profile above it apart from storage, and every record type drops out on its
# own.
set -uo pipefail

cd "$(dirname "$0")/.."
BUILD="${TMPDIR:-/tmp}/roost_profile"
fail=0

# A build that records fewer fields than its promiscuous radio can reach: no
# frame layer. That is a configuration choice, declared in the same compile-time
# place as the capabilities, and it is what puts those fields in
# capable_unrecorded rather than dropping them silently.
NO_FRAME_LAYER='-DROOST_WIFI_OBS_EXCLUDE=(ROOST_F(ROOST_WIFI_OBS_ADDR3)|ROOST_F(ROOST_WIFI_OBS_SEQ)|ROOST_F(ROOST_WIFI_OBS_FC_FLAGS)|ROOST_F(ROOST_WIFI_OBS_FRAME_LEN)|ROOST_F(ROOST_WIFI_OBS_BB_FORMAT))'

run() {
  local name="$1"; shift
  if ! g++ -std=c++17 -Wall -Wextra -Werror -Igenerated \
       -DROOST_PROFILE_NAME="\"$name\"" "$@" \
       tests/test_board_profile.cpp -o "$BUILD" 2>"$BUILD.err"; then
    echo "  FAIL  $name did not compile"
    sed 's/^/          /' "$BUILD.err" | head -5
    fail=1
    return
  fi
  "$BUILD"
  echo
}

echo "== scan mode, GNSS, BLE, no matcher"
run scan_gnss_ble \
  -DROOST_CAP_GNSS=1 -DROOST_CAP_STORAGE=1 \
  -DROOST_CAP_WIFI=1  -DROOST_CAP_WIFI_PROMISCUOUS=0 -DROOST_CAP_WIFI_SCAN=1 \
  -DROOST_CAP_IE_PARSE=0 \
  -DROOST_CAP_BLE=1 -DROOST_CAP_BLE_PROMISCUOUS=0 \
  -DROOST_CAP_TARGET_MATCH=0 -DROOST_CAP_OPERATOR_MARK=0

echo "== promiscuous Wi-Fi, GNSS, matcher, no BLE"
run promisc_wifi_matcher \
  -DROOST_CAP_GNSS=1 -DROOST_CAP_STORAGE=1 \
  -DROOST_CAP_WIFI=1  -DROOST_CAP_WIFI_PROMISCUOUS=1 -DROOST_CAP_WIFI_SCAN=0 \
  -DROOST_CAP_IE_PARSE=0 \
  -DROOST_CAP_BLE=0 -DROOST_CAP_BLE_PROMISCUOUS=0 \
  -DROOST_CAP_TARGET_MATCH=1 -DROOST_CAP_OPERATOR_MARK=1 \
  "$NO_FRAME_LAYER"

echo "== promiscuous Wi-Fi and BLE, IE parsing, matcher"
run promisc_wifi_ble_ie \
  -DROOST_CAP_GNSS=1 -DROOST_CAP_STORAGE=1 \
  -DROOST_CAP_WIFI=1  -DROOST_CAP_WIFI_PROMISCUOUS=1 -DROOST_CAP_WIFI_SCAN=0 \
  -DROOST_CAP_IE_PARSE=1 \
  -DROOST_CAP_BLE=1 -DROOST_CAP_BLE_PROMISCUOUS=1 \
  -DROOST_CAP_TARGET_MATCH=1 -DROOST_CAP_OPERATOR_MARK=0

echo "== the same shape with no SD card fitted"
run promisc_wifi_matcher_no_sd \
  -DROOST_CAP_GNSS=1 -DROOST_CAP_STORAGE=0 \
  -DROOST_CAP_WIFI=1  -DROOST_CAP_WIFI_PROMISCUOUS=1 -DROOST_CAP_WIFI_SCAN=0 \
  -DROOST_CAP_IE_PARSE=0 \
  -DROOST_CAP_BLE=0 -DROOST_CAP_BLE_PROMISCUOUS=0 \
  -DROOST_CAP_TARGET_MATCH=1 -DROOST_CAP_OPERATOR_MARK=1 \
  "$NO_FRAME_LAYER"

echo "== an undeclared capability must not silently default"
if g++ -std=c++17 -Igenerated -DROOST_PROFILE_NAME='"broken"' \
     -DROOST_CAP_GNSS=1 -DROOST_CAP_STORAGE=1 -DROOST_CAP_WIFI=1 \
     tests/test_board_profile.cpp -o "$BUILD" 2>/dev/null; then
  echo "  FAIL  a board missing capability declarations compiled anyway"
  fail=1
else
  echo "  ok    refused to compile with capabilities undeclared"
fi

echo
echo "== a board must not exclude a required field"
# A board may decline a field it can reach. Declining a required one would
# produce a file the record type declares unreadable, and nothing downstream
# could detect it. Both operands of COLUMNS_MASK are preprocessor constants, so
# this is a build failure instead.
if g++ -std=c++17 -Igenerated -DROOST_PROFILE_NAME='"excludes_required"' \
     -DROOST_CAP_GNSS=1 -DROOST_CAP_STORAGE=1 -DROOST_CAP_WIFI=1 \
     -DROOST_CAP_WIFI_PROMISCUOUS=1 \
     -DROOST_CAP_WIFI_SCAN=0 -DROOST_CAP_IE_PARSE=1 \
     -DROOST_CAP_BLE=0 -DROOST_CAP_BLE_PROMISCUOUS=0 \
     -DROOST_CAP_TARGET_MATCH=1 -DROOST_CAP_OPERATOR_MARK=0 \
     -DROOST_WIFI_OBS_EXCLUDE='(ROOST_F(ROOST_WIFI_OBS_CAP_COMPONENT))' \
     tests/test_board_profile.cpp -o "$BUILD" 2>/dev/null; then
  echo "  FAIL  a board excluding a required field compiled anyway"
  fail=1
else
  echo "  ok    refused to compile with a required field excluded"
fi

echo
echo "== a board must declare its capture components"
# Same rule as capabilities: there is no safe default. A device that cannot say
# what it is made of cannot say which part produced a row, and cap_component is
# required on every record type.
if g++ -std=c++17 -Igenerated -DROOST_PROFILE_NAME='"no_components"' \
     -DROOST_CAP_GNSS=1 -DROOST_CAP_STORAGE=1 -DROOST_CAP_WIFI=1 \
     -DROOST_CAP_WIFI_PROMISCUOUS=1 -DROOST_CAP_WIFI_SCAN=0 -DROOST_CAP_IE_PARSE=1 \
     -DROOST_CAP_BLE=0 -DROOST_CAP_BLE_PROMISCUOUS=0 \
     -DROOST_CAP_TARGET_MATCH=1 -DROOST_CAP_OPERATOR_MARK=0 \
     -DROOST_SKIP_COMPONENT_DECL=1 \
     tests/test_board_profile.cpp -o "$BUILD" 2>/dev/null; then
  echo "  FAIL  a board with no component declaration compiled anyway"
  fail=1
else
  echo "  ok    refused to compile with components undeclared"
fi

exit $fail
