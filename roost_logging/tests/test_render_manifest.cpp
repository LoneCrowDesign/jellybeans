// Renders a manifest through the shared renderer and writes it to stdout, so
// tests/run.sh can validate the real output against the generated schema
// instead of a hand-written fixture.
//
// Capabilities come from -D flags the way a board_config.h would supply them,
// on the same reasoning as board_profiles.sh: this checks the mechanism, and a
// profile named after a device would be a second copy of a declaration this
// repo does not own.
//
// One renderer, validated here: a per-device renderer is validated only when
// someone checks a card, so a manifest key that fails validation can reach a
// whole session's output undetected.

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "roost_registry.h"
#include "roost_sdlog.h"
#include "roost_manifest.h"

int main(void) {
  RoostFileDecl decls[ROOST_MAX_DECLARED_FILES];
  const size_t n = roostDeclaredFiles(decls, ROOST_MAX_DECLARED_FILES);

  static const char *const kMacs[] = {"aa:bb:cc:dd:ee:ff"};

  RoostSessionInfo info;
  memset(&info, 0, sizeof(info));
  info.deviceModel = "profile_device";
  info.deviceSerial = "aabbccddeeff";
  info.hwRevision = "HWr0.1";
  info.fwVersion = "0.0.0 test";
  info.builtAt = "2026-08-08T00:00Z";
  info.ownMacs = kMacs;
  info.numOwnMacs = 1;
  info.gnssCepM = 2.5f;
  info.sessionId = "profile-1";
  info.sequence = 1;
  info.bootCount = 1;
  info.clockAnchored = 1;
  info.clockSource = roostClockSourceByName("gps");
  info.clockAnchorUnix = 1786222471u;
  info.clockAnchorUptimeMs = 3705u;
  info.ouiTableHash = 0x9f8b6ccfu;
  info.storageTier = "sd";
  info.files = decls;
  info.numFiles = n;
  info.deviceDiagnostics = "\"queue_drops\":0";

  RoostSdLog g;
  memset(&g, 0, sizeof(g));
  roostSessionCounters(&g, &info.counters, 0, 0);

  static char out[4096];
  const size_t w = roostSessionJson(out, sizeof(out), &info);
  if (!w) {
    fprintf(stderr, "renderer returned 0\n");
    return 1;
  }
  fwrite(out, 1, w, stdout);
  return 0;
}
