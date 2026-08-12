#!/usr/bin/env bash
# Render a manifest through roost_manifest.h and validate it against the
# generated schema. Two capability shapes, because a device that declares fewer
# record types renders a different files array and that is the half most likely
# to break.
#
# --shape-only: the renderer is not sitting in a session directory, so the check
# for "every declared file is present" does not apply here.
set -uo pipefail

cd "$(dirname "$0")/.."
BUILD="${TMPDIR:-/tmp}/roost_render"
fail=0

FULL='-DROOST_CAP_GNSS=1 -DROOST_CAP_STORAGE=1 -DROOST_CAP_WIFI=1
      -DROOST_CAP_WIFI_PROMISCUOUS=1 -DROOST_CAP_WIFI_SCAN=0 -DROOST_CAP_IE_PARSE=1
      -DROOST_CAP_BLE=1 -DROOST_CAP_BLE_PROMISCUOUS=1 -DROOST_CAP_TARGET_MATCH=1
      -DROOST_CAP_OPERATOR_MARK=1'
SPARSE='-DROOST_CAP_GNSS=0 -DROOST_CAP_STORAGE=1 -DROOST_CAP_WIFI=1
        -DROOST_CAP_WIFI_PROMISCUOUS=0 -DROOST_CAP_WIFI_SCAN=1 -DROOST_CAP_IE_PARSE=0
        -DROOST_CAP_BLE=0 -DROOST_CAP_BLE_PROMISCUOUS=0 -DROOST_CAP_TARGET_MATCH=0
        -DROOST_CAP_OPERATOR_MARK=0'

COMPONENTS='-DROOST_COMPONENTS(X)=X(W0,"wifi0",WIFI,"ESP32-S3",ROOST_BAND_REACH_2_4)X(S,"sys",SYSTEM,NULL,0)'

run() {
  local name="$1"; shift
  if ! g++ -std=c++17 -Wall -Wextra -Werror -Igenerated -Iruntime \
       "$@" "$COMPONENTS" tests/test_render_manifest.cpp -o "$BUILD" 2>"$BUILD.err"; then
    echo "  FAIL  $name did not compile"
    sed 's/^/          /' "$BUILD.err" | head -8
    fail=1
    return
  fi
  if ! "$BUILD" > "$BUILD.json" 2>"$BUILD.err"; then
    echo "  FAIL  $name did not render"
    sed 's/^/          /' "$BUILD.err" | head -5
    fail=1
    return
  fi
  if python3 tests/test_manifest.py --shape-only "$BUILD.json" > "$BUILD.out" 2>&1; then
    echo "  ok    $name renders a manifest the contract accepts"
  else
    echo "  FAIL  $name rendered a manifest the contract rejects"
    sed 's/^/          /' "$BUILD.out" | head -12
    fail=1
  fi
}

# shellcheck disable=SC2086
run "full capability" $FULL
# shellcheck disable=SC2086
run "scan-mode, no gnss" $SPARSE

exit $fail
