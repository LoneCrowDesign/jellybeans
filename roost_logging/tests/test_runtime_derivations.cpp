// Host test for the shared derivations in runtime/.
//
// design_spec.md section 3.1 states that a computed field has one computation,
// implemented once in runtime/ and called by every device. These assertions are
// where that rule becomes enforceable rather than a matter of prose: they belong
// to the shared runtime, not to whichever device implements a field first.

#include <cstdio>
#include <cstring>

// A full-capability profile: these assertions are about the derivations, not
// about any one board's mask.
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
#include "roost_channel.h"
#include "roost_value.h"

static int g_fail = 0;

static void check(const char *what, long got, long want) {
  if (got == want) { std::printf("  ok    %s\n", what); return; }
  std::printf("  FAIL  %s\n          got %ld want %ld\n", what, got, want);
  g_fail = 1;
}

static void checkStr(const char *what, const char *got, const char *want) {
  if (std::strcmp(got, want) == 0) { std::printf("  ok    %s\n", what); return; }
  std::printf("  FAIL  %s\n          got \"%s\" want \"%s\"\n", what, got, want);
  g_fail = 1;
}

// A channel this build cannot place leaves the band column empty. Guessing a
// band for an unplaceable channel writes a value nothing measured, which is
// indistinguishable downstream from one that was.
static void unknownChannelsAreRefused() {
  static const uint16_t kUnplaceable[] = { 0, 15, 20, 31, 178, 200, 1000 };
  for (size_t i = 0; i < sizeof(kUnplaceable) / sizeof(kUnplaceable[0]); i++) {
    const RoostChannelBand cb = roostBandForChannel(kUnplaceable[i]);
    char label[64];
    std::snprintf(label, sizeof(label), "channel %u is refused, not guessed",
                  (unsigned)kUnplaceable[i]);
    check(label, cb.known, 0);
  }
}

static void bandsAreDerivedNotDeclared() {
  // Every 2.4 GHz channel, including 14, which a linear frequency formula
  // places outside the band it belongs to.
  for (uint16_t ch = 1; ch <= 14; ch++) {
    const RoostChannelBand cb = roostBandForChannel(ch);
    char label[64];
    std::snprintf(label, sizeof(label), "channel %u is 2.4 GHz", (unsigned)ch);
    check(label, cb.known && cb.band == ROOST_BAND_2_4, 1);
  }

  // U-NII channels spanning both boundaries of the 5 GHz range.
  static const uint16_t k5g[] = { 32, 36, 48, 52, 100, 149, 153, 157, 161, 165, 177 };
  for (size_t i = 0; i < sizeof(k5g) / sizeof(k5g[0]); i++) {
    const RoostChannelBand cb = roostBandForChannel(k5g[i]);
    char label[64];
    std::snprintf(label, sizeof(label), "channel %u is 5 GHz", (unsigned)k5g[i]);
    check(label, cb.known && cb.band == ROOST_BAND_5, 1);
  }
}

// The derivation must land on values the registry actually carries, or a device
// resolves a band the vocabulary does not have and empties the column.
static void derivedBandsAreInTheVocabulary() {
  check("2.4 resolves against the registry",
        roostBandByName("2.4"), ROOST_BAND_2_4);
  check("5 resolves against the registry",
        roostBandByName("5"), ROOST_BAND_5);
  check("the vocabulary has exactly the two bands the derivation can return",
        ROOST_BAND_COUNT, 2);
}

// freq_mhz is not a column: channel and band determine it, and wifi_obs v2
// dropped it for that reason.
static void frequencyIsNotAColumn() {
  char buf[64];
  const size_t n = roostFileName(buf, sizeof(buf), ROOST_REC_WIFI_OBS);
  check("wifi_obs is at v2", n > 0 && std::strcmp(buf, "wifi_obs.v2.csv") == 0, 1);

  int found = 0;
  for (uint8_t i = 0; i < ROOST_WIFI_OBS_FIELD_COUNT; i++)
    if (std::strcmp(roostFieldName(ROOST_REC_WIFI_OBS, i), "freq_mhz") == 0) found = 1;
  check("freq_mhz is not a wifi_obs column", found, 0);
}

// A declared type fixes how a value is rendered only if the rendering is shared
// code. `list` and `map` are rendered here, once, because two devices can
// satisfy the same declared type and still write different bytes.
static void listsJoinOnThePipe() {
  char buf[64];
  RoostValue v;

  roostValueBegin(&v, buf, sizeof(buf));
  roostValueAddUInt(&v, 11);
  roostValueAddUInt(&v, 6);
  roostValueAddUInt(&v, 1);
  check("a numeric list fits", roostValueDone(&v), 1);
  checkStr("a numeric list joins on '|'", buf, "11|6|1");

  roostValueBegin(&v, buf, sizeof(buf));
  roostValueAddText(&v, "flock");
  roostValueAddText(&v, "axon");
  check("a name list fits", roostValueDone(&v), 1);
  checkStr("a name list joins on '|'", buf, "flock|axon");

  // Empty means "does not apply on this device", never the word none.
  roostValueBegin(&v, buf, sizeof(buf));
  check("a list with no entries is legal", roostValueDone(&v), 1);
  checkStr("a list with no entries is empty", buf, "");

  roostValueBegin(&v, buf, sizeof(buf));
  roostValueAddUInt(&v, 1);
  check("a single entry leads no separator", roostValueDone(&v), 1);
  checkStr("a single entry stands alone", buf, "1");
}

static void mapsJoinKeyEqualsValue() {
  char buf[64];
  RoostValue v;
  roostValueBegin(&v, buf, sizeof(buf));
  roostValueAddKeyInt(&v, "rssi_min", -100);
  roostValueAddKeyUInt(&v, "cooldown_ms", 5000);
  check("a map fits", roostValueDone(&v), 1);
  checkStr("a map is key=value joined on '|'", buf,
           "rssi_min=-100|cooldown_ms=5000");
}

// A short list reads as a narrower plan deliberately chosen. Refusing loses the
// row, which is visible; truncating produces a different plausible capture.
static void overflowRefusesRatherThanTruncates() {
  char small[8];
  RoostValue v;
  roostValueBegin(&v, small, sizeof(small));
  roostValueAddUInt(&v, 1);
  roostValueAddUInt(&v, 6);
  roostValueAddUInt(&v, 11);
  roostValueAddUInt(&v, 149);
  check("an overflowing list is refused", roostValueDone(&v), 0);
  checkStr("a refused list leaves nothing behind", small, "");

  // A separator inside an entry would make the value unparseable against its
  // own type, so it is refused rather than escaped: split the setting instead.
  char buf[64];
  roostValueBegin(&v, buf, sizeof(buf));
  roostValueAddKeyText(&v, "note", "a|b");
  check("a map value carrying the separator is refused", roostValueDone(&v), 0);
  roostValueBegin(&v, buf, sizeof(buf));
  roostValueAddKeyText(&v, "a=b", "1");
  check("a map key carrying '=' is refused", roostValueDone(&v), 0);
}

int main() {
  std::printf("== runtime derivations (one computation per computed field)\n");
  bandsAreDerivedNotDeclared();
  unknownChannelsAreRefused();
  derivedBandsAreInTheVocabulary();
  frequencyIsNotAColumn();
  listsJoinOnThePipe();
  mapsJoinKeyEqualsValue();
  overflowRefusesRatherThanTruncates();
  if (g_fail) { std::printf("FAILED\n"); return 1; }
  std::printf("  all runtime derivation checks passed\n");
  return 0;
}
