// Copyright (c) 2026 Lone Crow Design, LLC
// SPDX-License-Identifier: MIT
//
// roost_registry.h - GENERATED FILE, DO NOT EDIT.
//
// Generated from registry/*.toml by tools/gen_registry.py.
// Edit the TOML and regenerate; a hand edit here is reverted by the next
// run and caught by the registry test in the meantime.
//
// This header is how independent repositories stay in step without having
// to agree with each other directly. A device declares the fields it can
// populate as a bitmask, and roostHeader() emits the canonical-order
// projection of the record type. The CSV emitter is then one loop over a
// table rather than a per-device format string, which is what keeps the
// column layouts from drifting apart.
//
// Registry fingerprint: c280d1f75e91ee8caf22a72c41a6e6c17f6b327201b8bb5b3d6dd979c624ef98

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define ROOST_REGISTRY_HASH "c280d1f75e91ee8caf22a72c41a6e6c17f6b327201b8bb5b3d6dd979c624ef98"
#define ROOST_REGISTRY_HASH_SHORT "c280d1f75e91"

// Populatable-field bitmask. Bit N corresponds to index N in the record's
// canonical field order.
typedef uint64_t RoostFieldMask;
#define ROOST_F(idx) ((RoostFieldMask)1u << (idx))

// --------------------------------------------------------------------------
// Enum vocabularies. A value absent from these tables cannot be emitted,
// which is what makes 'unknown enum values are an error' enforceable at
// the write path.
// --------------------------------------------------------------------------

typedef enum {
  ROOST_COMPONENT_KIND_WIFI = 0,
  ROOST_COMPONENT_KIND_BLE = 1,
  ROOST_COMPONENT_KIND_GNSS = 2,
  ROOST_COMPONENT_KIND_SYSTEM = 3,
  ROOST_COMPONENT_KIND_COUNT = 4
} RoostComponentKind;

static const char *const kRoostComponentKind[] = {
  "wifi",
  "ble",
  "gnss",
  "system",
};

// Resolves a producer's spelling to the enum. Returns _COUNT when the
// name is not in the vocabulary, so an unresolvable value is a state the
// caller must handle rather than a silent zero, which is a real value.
// Emitted for every enum because a device reaching a typed field with a
// name string is the normal case, not the exception.
static inline RoostComponentKind roostComponentKindByName(const char *name) {
  if (name)
    for (int i = 0; i < ROOST_COMPONENT_KIND_COUNT; i++)
      if (!strcmp(name, kRoostComponentKind[i])) return (RoostComponentKind)i;
  return ROOST_COMPONENT_KIND_COUNT;
}

typedef enum {
  ROOST_OBS_MODE_PROMISCUOUS = 0,
  ROOST_OBS_MODE_SCAN = 1,
  ROOST_OBS_MODE_COUNT = 2
} RoostObsMode;

static const char *const kRoostObsMode[] = {
  "promiscuous",
  "scan",
};

// Resolves a producer's spelling to the enum. Returns _COUNT when the
// name is not in the vocabulary, so an unresolvable value is a state the
// caller must handle rather than a silent zero, which is a real value.
// Emitted for every enum because a device reaching a typed field with a
// name string is the normal case, not the exception.
static inline RoostObsMode roostObsModeByName(const char *name) {
  if (name)
    for (int i = 0; i < ROOST_OBS_MODE_COUNT; i++)
      if (!strcmp(name, kRoostObsMode[i])) return (RoostObsMode)i;
  return ROOST_OBS_MODE_COUNT;
}

typedef enum {
  ROOST_DETECTION_METHOD_OUI_ADDR1 = 0,
  ROOST_DETECTION_METHOD_OUI_ADDR2 = 1,
  ROOST_DETECTION_METHOD_OUI_ADDR3 = 2,
  ROOST_DETECTION_METHOD_SSID_MATCH = 3,
  ROOST_DETECTION_METHOD_WILDCARD_PROBE = 4,
  ROOST_DETECTION_METHOD_DIRECTED_PROBE = 5,
  ROOST_DETECTION_METHOD_IE_MATCH = 6,
  ROOST_DETECTION_METHOD_BLE_OUI = 7,
  ROOST_DETECTION_METHOD_BLE_MFR = 8,
  ROOST_DETECTION_METHOD_UNMATCHED = 9,
  ROOST_DETECTION_METHOD_COUNT = 10
} RoostDetectionMethod;

static const char *const kRoostDetectionMethod[] = {
  "oui_addr1",
  "oui_addr2",
  "oui_addr3",
  "ssid_match",
  "wildcard_probe",
  "directed_probe",
  "ie_match",
  "ble_oui",
  "ble_mfr",
  "unmatched",
};

// Resolves a producer's spelling to the enum. Returns _COUNT when the
// name is not in the vocabulary, so an unresolvable value is a state the
// caller must handle rather than a silent zero, which is a real value.
// Emitted for every enum because a device reaching a typed field with a
// name string is the normal case, not the exception.
static inline RoostDetectionMethod roostDetectionMethodByName(const char *name) {
  if (name)
    for (int i = 0; i < ROOST_DETECTION_METHOD_COUNT; i++)
      if (!strcmp(name, kRoostDetectionMethod[i])) return (RoostDetectionMethod)i;
  return ROOST_DETECTION_METHOD_COUNT;
}

typedef enum {
  ROOST_FRAME_SUBTYPE_ASSOC_REQ = 0,
  ROOST_FRAME_SUBTYPE_ASSOC_RESP = 1,
  ROOST_FRAME_SUBTYPE_REASSOC_REQ = 2,
  ROOST_FRAME_SUBTYPE_REASSOC_RESP = 3,
  ROOST_FRAME_SUBTYPE_PROBE_REQ = 4,
  ROOST_FRAME_SUBTYPE_PROBE_RESP = 5,
  ROOST_FRAME_SUBTYPE_BEACON = 6,
  ROOST_FRAME_SUBTYPE_ATIM = 7,
  ROOST_FRAME_SUBTYPE_DISASSOC = 8,
  ROOST_FRAME_SUBTYPE_AUTH = 9,
  ROOST_FRAME_SUBTYPE_DEAUTH = 10,
  ROOST_FRAME_SUBTYPE_ACTION = 11,
  ROOST_FRAME_SUBTYPE_DATA = 12,
  ROOST_FRAME_SUBTYPE_QOS_DATA = 13,
  ROOST_FRAME_SUBTYPE_NULL_DATA = 14,
  ROOST_FRAME_SUBTYPE_CTRL = 15,
  ROOST_FRAME_SUBTYPE_UNKNOWN = 16,
  ROOST_FRAME_SUBTYPE_COUNT = 17
} RoostFrameSubtype;

static const char *const kRoostFrameSubtype[] = {
  "assoc_req",
  "assoc_resp",
  "reassoc_req",
  "reassoc_resp",
  "probe_req",
  "probe_resp",
  "beacon",
  "atim",
  "disassoc",
  "auth",
  "deauth",
  "action",
  "data",
  "qos_data",
  "null_data",
  "ctrl",
  "unknown",
};

// Resolves a producer's spelling to the enum. Returns _COUNT when the
// name is not in the vocabulary, so an unresolvable value is a state the
// caller must handle rather than a silent zero, which is a real value.
// Emitted for every enum because a device reaching a typed field with a
// name string is the normal case, not the exception.
static inline RoostFrameSubtype roostFrameSubtypeByName(const char *name) {
  if (name)
    for (int i = 0; i < ROOST_FRAME_SUBTYPE_COUNT; i++)
      if (!strcmp(name, kRoostFrameSubtype[i])) return (RoostFrameSubtype)i;
  return ROOST_FRAME_SUBTYPE_COUNT;
}

typedef enum {
  ROOST_BAND_2_4 = 0,
  ROOST_BAND_5 = 1,
  ROOST_BAND_COUNT = 2
} RoostBand;

static const char *const kRoostBand[] = {
  "2.4",
  "5",
};

// Resolves a producer's spelling to the enum. Returns _COUNT when the
// name is not in the vocabulary, so an unresolvable value is a state the
// caller must handle rather than a silent zero, which is a real value.
// Emitted for every enum because a device reaching a typed field with a
// name string is the normal case, not the exception.
static inline RoostBand roostBandByName(const char *name) {
  if (name)
    for (int i = 0; i < ROOST_BAND_COUNT; i++)
      if (!strcmp(name, kRoostBand[i])) return (RoostBand)i;
  return ROOST_BAND_COUNT;
}

typedef enum {
  ROOST_AUTH_MODE_OPEN = 0,
  ROOST_AUTH_MODE_WEP = 1,
  ROOST_AUTH_MODE_WPA_PSK = 2,
  ROOST_AUTH_MODE_WPA2_PSK = 3,
  ROOST_AUTH_MODE_WPA_WPA2_PSK = 4,
  ROOST_AUTH_MODE_WPA2 = 5,
  ROOST_AUTH_MODE_WPA2_ENTERPRISE = 6,
  ROOST_AUTH_MODE_WPA3_PSK = 7,
  ROOST_AUTH_MODE_WPA2_WPA3_PSK = 8,
  ROOST_AUTH_MODE_WAPI_PSK = 9,
  ROOST_AUTH_MODE_UNDEFINED = 10,
  ROOST_AUTH_MODE_COUNT = 11
} RoostAuthMode;

static const char *const kRoostAuthMode[] = {
  "open",
  "wep",
  "wpa_psk",
  "wpa2_psk",
  "wpa_wpa2_psk",
  "wpa2",
  "wpa2_enterprise",
  "wpa3_psk",
  "wpa2_wpa3_psk",
  "wapi_psk",
  "undefined",
};

// Resolves a producer's spelling to the enum. Returns _COUNT when the
// name is not in the vocabulary, so an unresolvable value is a state the
// caller must handle rather than a silent zero, which is a real value.
// Emitted for every enum because a device reaching a typed field with a
// name string is the normal case, not the exception.
static inline RoostAuthMode roostAuthModeByName(const char *name) {
  if (name)
    for (int i = 0; i < ROOST_AUTH_MODE_COUNT; i++)
      if (!strcmp(name, kRoostAuthMode[i])) return (RoostAuthMode)i;
  return ROOST_AUTH_MODE_COUNT;
}

typedef enum {
  ROOST_BB_FORMAT_11B = 0,
  ROOST_BB_FORMAT_11G = 1,
  ROOST_BB_FORMAT_HT = 2,
  ROOST_BB_FORMAT_VHT = 3,
  ROOST_BB_FORMAT_VHT_MU = 4,
  ROOST_BB_FORMAT_HE_SU = 5,
  ROOST_BB_FORMAT_HE_MU = 6,
  ROOST_BB_FORMAT_HE_ERSU = 7,
  ROOST_BB_FORMAT_HE_TB = 8,
  ROOST_BB_FORMAT_COUNT = 9
} RoostBbFormat;

static const char *const kRoostBbFormat[] = {
  "11b",
  "11g",
  "ht",
  "vht",
  "vht_mu",
  "he_su",
  "he_mu",
  "he_ersu",
  "he_tb",
};

// Resolves a producer's spelling to the enum. Returns _COUNT when the
// name is not in the vocabulary, so an unresolvable value is a state the
// caller must handle rather than a silent zero, which is a real value.
// Emitted for every enum because a device reaching a typed field with a
// name string is the normal case, not the exception.
static inline RoostBbFormat roostBbFormatByName(const char *name) {
  if (name)
    for (int i = 0; i < ROOST_BB_FORMAT_COUNT; i++)
      if (!strcmp(name, kRoostBbFormat[i])) return (RoostBbFormat)i;
  return ROOST_BB_FORMAT_COUNT;
}

typedef enum {
  ROOST_BLE_ADDR_TYPE_PUBLIC = 0,
  ROOST_BLE_ADDR_TYPE_RANDOM = 1,
  ROOST_BLE_ADDR_TYPE_COUNT = 2
} RoostBleAddrType;

static const char *const kRoostBleAddrType[] = {
  "public",
  "random",
};

// Resolves a producer's spelling to the enum. Returns _COUNT when the
// name is not in the vocabulary, so an unresolvable value is a state the
// caller must handle rather than a silent zero, which is a real value.
// Emitted for every enum because a device reaching a typed field with a
// name string is the normal case, not the exception.
static inline RoostBleAddrType roostBleAddrTypeByName(const char *name) {
  if (name)
    for (int i = 0; i < ROOST_BLE_ADDR_TYPE_COUNT; i++)
      if (!strcmp(name, kRoostBleAddrType[i])) return (RoostBleAddrType)i;
  return ROOST_BLE_ADDR_TYPE_COUNT;
}

typedef enum {
  ROOST_BLE_PDU_TYPE_ADV_IND = 0,
  ROOST_BLE_PDU_TYPE_ADV_DIRECT_IND = 1,
  ROOST_BLE_PDU_TYPE_ADV_SCAN_IND = 2,
  ROOST_BLE_PDU_TYPE_ADV_NONCONN_IND = 3,
  ROOST_BLE_PDU_TYPE_SCAN_RSP = 4,
  ROOST_BLE_PDU_TYPE_EXT_ADV_IND = 5,
  ROOST_BLE_PDU_TYPE_UNKNOWN = 6,
  ROOST_BLE_PDU_TYPE_COUNT = 7
} RoostBlePduType;

static const char *const kRoostBlePduType[] = {
  "adv_ind",
  "adv_direct_ind",
  "adv_scan_ind",
  "adv_nonconn_ind",
  "scan_rsp",
  "ext_adv_ind",
  "unknown",
};

// Resolves a producer's spelling to the enum. Returns _COUNT when the
// name is not in the vocabulary, so an unresolvable value is a state the
// caller must handle rather than a silent zero, which is a real value.
// Emitted for every enum because a device reaching a typed field with a
// name string is the normal case, not the exception.
static inline RoostBlePduType roostBlePduTypeByName(const char *name) {
  if (name)
    for (int i = 0; i < ROOST_BLE_PDU_TYPE_COUNT; i++)
      if (!strcmp(name, kRoostBlePduType[i])) return (RoostBlePduType)i;
  return ROOST_BLE_PDU_TYPE_COUNT;
}

typedef enum {
  ROOST_BLE_PHY_1M = 0,
  ROOST_BLE_PHY_2M = 1,
  ROOST_BLE_PHY_CODED = 2,
  ROOST_BLE_PHY_COUNT = 3
} RoostBlePhy;

static const char *const kRoostBlePhy[] = {
  "1m",
  "2m",
  "coded",
};

// Resolves a producer's spelling to the enum. Returns _COUNT when the
// name is not in the vocabulary, so an unresolvable value is a state the
// caller must handle rather than a silent zero, which is a real value.
// Emitted for every enum because a device reaching a typed field with a
// name string is the normal case, not the exception.
static inline RoostBlePhy roostBlePhyByName(const char *name) {
  if (name)
    for (int i = 0; i < ROOST_BLE_PHY_COUNT; i++)
      if (!strcmp(name, kRoostBlePhy[i])) return (RoostBlePhy)i;
  return ROOST_BLE_PHY_COUNT;
}

typedef enum {
  ROOST_FIX_TYPE_NONE = 0,
  ROOST_FIX_TYPE_2D = 1,
  ROOST_FIX_TYPE_3D = 2,
  ROOST_FIX_TYPE_DGPS = 3,
  ROOST_FIX_TYPE_COUNT = 4
} RoostFixType;

static const char *const kRoostFixType[] = {
  "none",
  "2d",
  "3d",
  "dgps",
};

// Resolves a producer's spelling to the enum. Returns _COUNT when the
// name is not in the vocabulary, so an unresolvable value is a state the
// caller must handle rather than a silent zero, which is a real value.
// Emitted for every enum because a device reaching a typed field with a
// name string is the normal case, not the exception.
static inline RoostFixType roostFixTypeByName(const char *name) {
  if (name)
    for (int i = 0; i < ROOST_FIX_TYPE_COUNT; i++)
      if (!strcmp(name, kRoostFixType[i])) return (RoostFixType)i;
  return ROOST_FIX_TYPE_COUNT;
}

typedef enum {
  ROOST_POSITION_SOURCE_DEVICE_FIX = 0,
  ROOST_POSITION_SOURCE_DEVICE_STALE = 1,
  ROOST_POSITION_SOURCE_CELL_TOWER = 2,
  ROOST_POSITION_SOURCE_OPERATOR_FORCED = 3,
  ROOST_POSITION_SOURCE_NONE = 4,
  ROOST_POSITION_SOURCE_COUNT = 5
} RoostPositionSource;

static const char *const kRoostPositionSource[] = {
  "device_fix",
  "device_stale",
  "cell_tower",
  "operator_forced",
  "none",
};

// Resolves a producer's spelling to the enum. Returns _COUNT when the
// name is not in the vocabulary, so an unresolvable value is a state the
// caller must handle rather than a silent zero, which is a real value.
// Emitted for every enum because a device reaching a typed field with a
// name string is the normal case, not the exception.
static inline RoostPositionSource roostPositionSourceByName(const char *name) {
  if (name)
    for (int i = 0; i < ROOST_POSITION_SOURCE_COUNT; i++)
      if (!strcmp(name, kRoostPositionSource[i])) return (RoostPositionSource)i;
  return ROOST_POSITION_SOURCE_COUNT;
}

typedef enum {
  ROOST_CLOCK_SOURCE_GPS = 0,
  ROOST_CLOCK_SOURCE_NTP = 1,
  ROOST_CLOCK_SOURCE_NONE = 2,
  ROOST_CLOCK_SOURCE_COUNT = 3
} RoostClockSource;

static const char *const kRoostClockSource[] = {
  "gps",
  "ntp",
  "none",
};

// Resolves a producer's spelling to the enum. Returns _COUNT when the
// name is not in the vocabulary, so an unresolvable value is a state the
// caller must handle rather than a silent zero, which is a real value.
// Emitted for every enum because a device reaching a typed field with a
// name string is the normal case, not the exception.
static inline RoostClockSource roostClockSourceByName(const char *name) {
  if (name)
    for (int i = 0; i < ROOST_CLOCK_SOURCE_COUNT; i++)
      if (!strcmp(name, kRoostClockSource[i])) return (RoostClockSource)i;
  return ROOST_CLOCK_SOURCE_COUNT;
}

typedef enum {
  ROOST_DEVICE_EVENT_KIND_BOOT = 0,
  ROOST_DEVICE_EVENT_KIND_CLOCK_ANCHORED = 1,
  ROOST_DEVICE_EVENT_KIND_STORAGE_ERROR = 2,
  ROOST_DEVICE_EVENT_KIND_STORAGE_RECOVERED = 3,
  ROOST_DEVICE_EVENT_KIND_BUFFER_FULL = 4,
  ROOST_DEVICE_EVENT_KIND_CONFIG_ERROR = 5,
  ROOST_DEVICE_EVENT_KIND_CLOCK_SYNCED = 6,
  ROOST_DEVICE_EVENT_KIND_VOCABULARY_ERROR = 7,
  ROOST_DEVICE_EVENT_KIND_SHUTDOWN = 8,
  ROOST_DEVICE_EVENT_KIND_COUNT = 9
} RoostDeviceEventKind;

static const char *const kRoostDeviceEventKind[] = {
  "boot",
  "clock_anchored",
  "storage_error",
  "storage_recovered",
  "buffer_full",
  "config_error",
  "clock_synced",
  "vocabulary_error",
  "shutdown",
};

// Resolves a producer's spelling to the enum. Returns _COUNT when the
// name is not in the vocabulary, so an unresolvable value is a state the
// caller must handle rather than a silent zero, which is a real value.
// Emitted for every enum because a device reaching a typed field with a
// name string is the normal case, not the exception.
static inline RoostDeviceEventKind roostDeviceEventKindByName(const char *name) {
  if (name)
    for (int i = 0; i < ROOST_DEVICE_EVENT_KIND_COUNT; i++)
      if (!strcmp(name, kRoostDeviceEventKind[i])) return (RoostDeviceEventKind)i;
  return ROOST_DEVICE_EVENT_KIND_COUNT;
}

typedef enum {
  ROOST_CONFIG_SETTING_OBS_MODE = 0,
  ROOST_CONFIG_SETTING_CHANNELS = 1,
  ROOST_CONFIG_SETTING_COUNTRY_CODE = 2,
  ROOST_CONFIG_SETTING_DWELL_MS = 3,
  ROOST_CONFIG_SETTING_SCAN_PERIOD_MS = 4,
  ROOST_CONFIG_SETTING_VENDOR_MASK = 5,
  ROOST_CONFIG_SETTING_FILTERS = 6,
  ROOST_CONFIG_SETTING_COUNT = 7
} RoostConfigSetting;

static const char *const kRoostConfigSetting[] = {
  "obs_mode",
  "channels",
  "country_code",
  "dwell_ms",
  "scan_period_ms",
  "vendor_mask",
  "filters",
};

// Resolves a producer's spelling to the enum. Returns _COUNT when the
// name is not in the vocabulary, so an unresolvable value is a state the
// caller must handle rather than a silent zero, which is a real value.
// Emitted for every enum because a device reaching a typed field with a
// name string is the normal case, not the exception.
static inline RoostConfigSetting roostConfigSettingByName(const char *name) {
  if (name)
    for (int i = 0; i < ROOST_CONFIG_SETTING_COUNT; i++)
      if (!strcmp(name, kRoostConfigSetting[i])) return (RoostConfigSetting)i;
  return ROOST_CONFIG_SETTING_COUNT;
}

// --------------------------------------------------------------------------
// Capabilities: what this build can measure, declared at compile time.
//
// Every board defines each macro below as 0 or 1, in its board_config.h.
// Where the board already declares the fact under its own name, alias it
// (#define ROOST_CAP_GNSS HAS_GPS) rather than restating it: an alias
// cannot disagree with the thing it aliases.
//
// Undefined is an error, never a default of 0. Defaulting would mean that
// adding a capability here silently shrinks every existing board's mask,
// and a silently shrinking mask is a capture quietly recording less than
// it could.
// --------------------------------------------------------------------------

// gnss: The build reads a fitted GNSS receiver. Gates every position field and the gps_track record entirely.
#if !defined(ROOST_CAP_GNSS)
#error "ROOST_CAP_GNSS is undefined. Every board must declare each capability as 0 or 1; see docs/design_spec.md section 4.2."
#endif
// storage: The build writes to persistent storage at all. Note this is the compile-time half only: whether a card is actually inserted is a boot-time fact, and narrows the mask again at runtime. A device writing to internal flash versus the same firmware with a card fitted is exactly this distinction.
#if !defined(ROOST_CAP_STORAGE)
#error "ROOST_CAP_STORAGE is undefined. Every board must declare each capability as 0 or 1; see docs/design_spec.md section 4.2."
#endif
// wifi: The build has a Wi-Fi capture path. Gates the wifi_obs record. Fitted silicon is not sufficient: a board whose Wi-Fi radio is present but which this build never captures from declares 0, or the session carries a wifi_obs file asserting that nothing was heard when the truth is that nothing listened.
#if !defined(ROOST_CAP_WIFI)
#error "ROOST_CAP_WIFI is undefined. Every board must declare each capability as 0 or 1; see docs/design_spec.md section 4.2."
#endif
// wifi_promiscuous: The build can capture raw 802.11 frames rather than parsed scan results. Gates every frame-layer field, because a scan result has already discarded the frame by the time firmware sees it. Not a statement about which mode is active: a build that can also scan declares wifi_scan as well, and obs_mode on each row says which of them produced it.
#if !defined(ROOST_CAP_WIFI_PROMISCUOUS)
#error "ROOST_CAP_WIFI_PROMISCUOUS is undefined. Every board must declare each capability as 0 or 1; see docs/design_spec.md section 4.2."
#endif
// wifi_scan: The build has a driver-scanning path. Gates auth_mode, which a scan returns already parsed. Declare it if the build can ever scan, not only if it is scanning today. A build with no scan path at all must not declare it, or auth_mode becomes a permanently empty column asserting that the radio had nothing to report when the truth is that nothing ever asked it.
#if !defined(ROOST_CAP_WIFI_SCAN)
#error "ROOST_CAP_WIFI_SCAN is undefined. Every board must declare each capability as 0 or 1; see docs/design_spec.md section 4.2."
#endif
// ie_parse: The build parses beacon information elements. The promiscuous route to auth_mode, cap_info, beacon_interval, ie_ids and vendor_ies.
#if !defined(ROOST_CAP_IE_PARSE)
#error "ROOST_CAP_IE_PARSE is undefined. Every board must declare each capability as 0 or 1; see docs/design_spec.md section 4.2."
#endif
// ble: The build has a BLE capture path. Gates the ble_obs record. Fitted silicon is not sufficient, and this is the capability where that bites: most parts carry a BLE radio, so a board with no BLE capture mode implemented declares 0 until one exists. Declaring it early puts an empty ble_obs file in every session, which asserts BLE was reachable and nothing was heard.
#if !defined(ROOST_CAP_BLE)
#error "ROOST_CAP_BLE is undefined. Every board must declare each capability as 0 or 1; see docs/design_spec.md section 4.2."
#endif
// ble_promiscuous: The build can see raw advertising PDUs rather than a stack's parsed device objects. Gates the PDU-layer BLE fields. Reach rather than current mode, as with wifi_promiscuous; BLE's active-versus-passive scan setting is a separate, per-row fact carried by active_scan.
#if !defined(ROOST_CAP_BLE_PROMISCUOUS)
#error "ROOST_CAP_BLE_PROMISCUOUS is undefined. Every board must declare each capability as 0 or 1; see docs/design_spec.md section 4.2."
#endif
// target_match: The build matches observations against a target table. Gates detection_method: a device that logs everything it hears has no matcher and therefore no answer to 'why was this logged', which is different from answering 'unmatched'.
#if !defined(ROOST_CAP_TARGET_MATCH)
#error "ROOST_CAP_TARGET_MATCH is undefined. Every board must declare each capability as 0 or 1; see docs/design_spec.md section 4.2."
#endif
// operator_mark: The build has an operator control that marks a place and time. Gates the operator_mark record.
#if !defined(ROOST_CAP_OPERATOR_MARK)
#error "ROOST_CAP_OPERATOR_MARK is undefined. Every board must declare each capability as 0 or 1; see docs/design_spec.md section 4.2."
#endif

// --------------------------------------------------------------------------
// Record types.
// --------------------------------------------------------------------------

typedef enum {
  ROOST_REC_WIFI_OBS = 0,
  ROOST_REC_BLE_OBS = 1,
  ROOST_REC_GPS_TRACK = 2,
  ROOST_REC_DEVICE_EVENT = 3,
  ROOST_REC_CONFIG_CHANGE = 4,
  ROOST_REC_OPERATOR_MARK = 5,
  ROOST_REC_COUNT = 6
} RoostRecord;

// wifi_obs v2 - canonical field order.
enum {
  ROOST_WIFI_OBS_TIMESTAMP_UTC = 0,
  ROOST_WIFI_OBS_UPTIME_MS = 1,
  ROOST_WIFI_OBS_FIX_SEQ = 2,
  ROOST_WIFI_OBS_CAP_COMPONENT = 3,
  ROOST_WIFI_OBS_OBS_MODE = 4,
  ROOST_WIFI_OBS_MAC = 5,
  ROOST_WIFI_OBS_DETECTION_METHOD = 6,
  ROOST_WIFI_OBS_FRAME_SUBTYPE = 7,
  ROOST_WIFI_OBS_RSSI = 8,
  ROOST_WIFI_OBS_CHANNEL = 9,
  ROOST_WIFI_OBS_BAND = 10,
  ROOST_WIFI_OBS_SSID = 11,
  ROOST_WIFI_OBS_AUTH_MODE = 12,
  ROOST_WIFI_OBS_ADDR1 = 13,
  ROOST_WIFI_OBS_ADDR2 = 14,
  ROOST_WIFI_OBS_ADDR3 = 15,
  ROOST_WIFI_OBS_SEQ = 16,
  ROOST_WIFI_OBS_FC_FLAGS = 17,
  ROOST_WIFI_OBS_FRAME_LEN = 18,
  ROOST_WIFI_OBS_BB_FORMAT = 19,
  ROOST_WIFI_OBS_CAP_INFO = 20,
  ROOST_WIFI_OBS_BEACON_INTERVAL = 21,
  ROOST_WIFI_OBS_IE_IDS = 22,
  ROOST_WIFI_OBS_VENDOR_IES = 23,
  ROOST_WIFI_OBS_FIELD_COUNT = 24
};
#define ROOST_WIFI_OBS_REQUIRED_MASK ((RoostFieldMask)0x000000000000013aull)
// Does this build emit wifi_obs at all?
#define ROOST_EMITS_WIFI_OBS (ROOST_CAP_WIFI && ROOST_CAP_STORAGE)
// Fields this build can populate, derived from the capability macros
// above. Never hand-written: a board with a card and one without
// differ only in board_config.h and this follows.
#define ROOST_WIFI_OBS_CAPABLE_MASK ( \
      ROOST_F(ROOST_WIFI_OBS_TIMESTAMP_UTC) \
  | ROOST_F(ROOST_WIFI_OBS_UPTIME_MS) \
  | ((ROOST_CAP_GNSS) ? ROOST_F(ROOST_WIFI_OBS_FIX_SEQ) : 0) \
  | ROOST_F(ROOST_WIFI_OBS_CAP_COMPONENT) \
  | ROOST_F(ROOST_WIFI_OBS_OBS_MODE) \
  | ROOST_F(ROOST_WIFI_OBS_MAC) \
  | ((ROOST_CAP_TARGET_MATCH) ? ROOST_F(ROOST_WIFI_OBS_DETECTION_METHOD) : 0) \
  | ((ROOST_CAP_WIFI_PROMISCUOUS) ? ROOST_F(ROOST_WIFI_OBS_FRAME_SUBTYPE) : 0) \
  | ROOST_F(ROOST_WIFI_OBS_RSSI) \
  | ROOST_F(ROOST_WIFI_OBS_CHANNEL) \
  | ROOST_F(ROOST_WIFI_OBS_BAND) \
  | ROOST_F(ROOST_WIFI_OBS_SSID) \
  | ((ROOST_CAP_WIFI_SCAN || ROOST_CAP_IE_PARSE) ? ROOST_F(ROOST_WIFI_OBS_AUTH_MODE) : 0) \
  | ((ROOST_CAP_WIFI_PROMISCUOUS) ? ROOST_F(ROOST_WIFI_OBS_ADDR1) : 0) \
  | ((ROOST_CAP_WIFI_PROMISCUOUS) ? ROOST_F(ROOST_WIFI_OBS_ADDR2) : 0) \
  | ((ROOST_CAP_WIFI_PROMISCUOUS) ? ROOST_F(ROOST_WIFI_OBS_ADDR3) : 0) \
  | ((ROOST_CAP_WIFI_PROMISCUOUS) ? ROOST_F(ROOST_WIFI_OBS_SEQ) : 0) \
  | ((ROOST_CAP_WIFI_PROMISCUOUS) ? ROOST_F(ROOST_WIFI_OBS_FC_FLAGS) : 0) \
  | ((ROOST_CAP_WIFI_PROMISCUOUS) ? ROOST_F(ROOST_WIFI_OBS_FRAME_LEN) : 0) \
  | ((ROOST_CAP_WIFI_PROMISCUOUS) ? ROOST_F(ROOST_WIFI_OBS_BB_FORMAT) : 0) \
  | ((ROOST_CAP_IE_PARSE) ? ROOST_F(ROOST_WIFI_OBS_CAP_INFO) : 0) \
  | ((ROOST_CAP_IE_PARSE) ? ROOST_F(ROOST_WIFI_OBS_BEACON_INTERVAL) : 0) \
  | ((ROOST_CAP_IE_PARSE) ? ROOST_F(ROOST_WIFI_OBS_IE_IDS) : 0) \
  | ((ROOST_CAP_IE_PARSE) ? ROOST_F(ROOST_WIFI_OBS_VENDOR_IES) : 0) \
  )
#if !defined(ROOST_WIFI_OBS_EXCLUDE)
#define ROOST_WIFI_OBS_EXCLUDE 0
#endif
#define ROOST_WIFI_OBS_COLUMNS_MASK (ROOST_WIFI_OBS_CAPABLE_MASK & ~(RoostFieldMask)(ROOST_WIFI_OBS_EXCLUDE))
#if ROOST_EMITS_WIFI_OBS
typedef char roost_required_wifi_obs_declared[
    ((ROOST_WIFI_OBS_COLUMNS_MASK & ROOST_WIFI_OBS_REQUIRED_MASK)
     == ROOST_WIFI_OBS_REQUIRED_MASK) ? 1 : -1];
#endif
// detection_method on this record accepts only: directed_probe, ie_match, oui_addr1, oui_addr2, oui_addr3, ssid_match, unmatched, wildcard_probe
#define ROOST_WIFI_OBS_DETECTION_METHOD_ALLOWED ((uint64_t)0x000000000000027full)

// ble_obs v1 - canonical field order.
enum {
  ROOST_BLE_OBS_TIMESTAMP_UTC = 0,
  ROOST_BLE_OBS_UPTIME_MS = 1,
  ROOST_BLE_OBS_FIX_SEQ = 2,
  ROOST_BLE_OBS_CAP_COMPONENT = 3,
  ROOST_BLE_OBS_OBS_MODE = 4,
  ROOST_BLE_OBS_MAC = 5,
  ROOST_BLE_OBS_ADDR_TYPE = 6,
  ROOST_BLE_OBS_DETECTION_METHOD = 7,
  ROOST_BLE_OBS_RSSI = 8,
  ROOST_BLE_OBS_TX_POWER = 9,
  ROOST_BLE_OBS_DEVICE_NAME = 10,
  ROOST_BLE_OBS_PDU_TYPE = 11,
  ROOST_BLE_OBS_PHY_PRIMARY = 12,
  ROOST_BLE_OBS_PHY_SECONDARY = 13,
  ROOST_BLE_OBS_SID = 14,
  ROOST_BLE_OBS_ACTIVE_SCAN = 15,
  ROOST_BLE_OBS_ADV_DATA_HEX = 16,
  ROOST_BLE_OBS_FIELD_COUNT = 17
};
#define ROOST_BLE_OBS_REQUIRED_MASK ((RoostFieldMask)0x000000000000013aull)
// Does this build emit ble_obs at all?
#define ROOST_EMITS_BLE_OBS (ROOST_CAP_BLE && ROOST_CAP_STORAGE)
// Fields this build can populate, derived from the capability macros
// above. Never hand-written: a board with a card and one without
// differ only in board_config.h and this follows.
#define ROOST_BLE_OBS_CAPABLE_MASK ( \
      ROOST_F(ROOST_BLE_OBS_TIMESTAMP_UTC) \
  | ROOST_F(ROOST_BLE_OBS_UPTIME_MS) \
  | ((ROOST_CAP_GNSS) ? ROOST_F(ROOST_BLE_OBS_FIX_SEQ) : 0) \
  | ROOST_F(ROOST_BLE_OBS_CAP_COMPONENT) \
  | ROOST_F(ROOST_BLE_OBS_OBS_MODE) \
  | ROOST_F(ROOST_BLE_OBS_MAC) \
  | ((ROOST_CAP_BLE_PROMISCUOUS) ? ROOST_F(ROOST_BLE_OBS_ADDR_TYPE) : 0) \
  | ((ROOST_CAP_TARGET_MATCH) ? ROOST_F(ROOST_BLE_OBS_DETECTION_METHOD) : 0) \
  | ROOST_F(ROOST_BLE_OBS_RSSI) \
  | ROOST_F(ROOST_BLE_OBS_TX_POWER) \
  | ROOST_F(ROOST_BLE_OBS_DEVICE_NAME) \
  | ((ROOST_CAP_BLE_PROMISCUOUS) ? ROOST_F(ROOST_BLE_OBS_PDU_TYPE) : 0) \
  | ((ROOST_CAP_BLE_PROMISCUOUS) ? ROOST_F(ROOST_BLE_OBS_PHY_PRIMARY) : 0) \
  | ((ROOST_CAP_BLE_PROMISCUOUS) ? ROOST_F(ROOST_BLE_OBS_PHY_SECONDARY) : 0) \
  | ((ROOST_CAP_BLE_PROMISCUOUS) ? ROOST_F(ROOST_BLE_OBS_SID) : 0) \
  | ((ROOST_CAP_BLE_PROMISCUOUS) ? ROOST_F(ROOST_BLE_OBS_ACTIVE_SCAN) : 0) \
  | ((ROOST_CAP_BLE_PROMISCUOUS) ? ROOST_F(ROOST_BLE_OBS_ADV_DATA_HEX) : 0) \
  )
#if !defined(ROOST_BLE_OBS_EXCLUDE)
#define ROOST_BLE_OBS_EXCLUDE 0
#endif
#define ROOST_BLE_OBS_COLUMNS_MASK (ROOST_BLE_OBS_CAPABLE_MASK & ~(RoostFieldMask)(ROOST_BLE_OBS_EXCLUDE))
#if ROOST_EMITS_BLE_OBS
typedef char roost_required_ble_obs_declared[
    ((ROOST_BLE_OBS_COLUMNS_MASK & ROOST_BLE_OBS_REQUIRED_MASK)
     == ROOST_BLE_OBS_REQUIRED_MASK) ? 1 : -1];
#endif
// detection_method on this record accepts only: ble_mfr, ble_oui, unmatched
#define ROOST_BLE_OBS_DETECTION_METHOD_ALLOWED ((uint64_t)0x0000000000000380ull)

// gps_track v1 - canonical field order.
enum {
  ROOST_GPS_TRACK_TIMESTAMP_UTC = 0,
  ROOST_GPS_TRACK_UPTIME_MS = 1,
  ROOST_GPS_TRACK_FIX_SEQ = 2,
  ROOST_GPS_TRACK_CAP_COMPONENT = 3,
  ROOST_GPS_TRACK_POSITION_SOURCE = 4,
  ROOST_GPS_TRACK_LAT = 5,
  ROOST_GPS_TRACK_LON = 6,
  ROOST_GPS_TRACK_ALT_M = 7,
  ROOST_GPS_TRACK_SPEED_MPS = 8,
  ROOST_GPS_TRACK_COURSE_DEG = 9,
  ROOST_GPS_TRACK_HDOP = 10,
  ROOST_GPS_TRACK_SATS = 11,
  ROOST_GPS_TRACK_FIX_TYPE = 12,
  ROOST_GPS_TRACK_FIX_AGE_MS = 13,
  ROOST_GPS_TRACK_FIELD_COUNT = 14
};
#define ROOST_GPS_TRACK_REQUIRED_MASK ((RoostFieldMask)0x000000000000001eull)
// Does this build emit gps_track at all?
#define ROOST_EMITS_GPS_TRACK (ROOST_CAP_GNSS && ROOST_CAP_STORAGE)
// Fields this build can populate, derived from the capability macros
// above. Never hand-written: a board with a card and one without
// differ only in board_config.h and this follows.
#define ROOST_GPS_TRACK_CAPABLE_MASK ( \
      ROOST_F(ROOST_GPS_TRACK_TIMESTAMP_UTC) \
  | ROOST_F(ROOST_GPS_TRACK_UPTIME_MS) \
  | ((ROOST_CAP_GNSS) ? ROOST_F(ROOST_GPS_TRACK_FIX_SEQ) : 0) \
  | ROOST_F(ROOST_GPS_TRACK_CAP_COMPONENT) \
  | ((ROOST_CAP_GNSS) ? ROOST_F(ROOST_GPS_TRACK_POSITION_SOURCE) : 0) \
  | ((ROOST_CAP_GNSS) ? ROOST_F(ROOST_GPS_TRACK_LAT) : 0) \
  | ((ROOST_CAP_GNSS) ? ROOST_F(ROOST_GPS_TRACK_LON) : 0) \
  | ((ROOST_CAP_GNSS) ? ROOST_F(ROOST_GPS_TRACK_ALT_M) : 0) \
  | ((ROOST_CAP_GNSS) ? ROOST_F(ROOST_GPS_TRACK_SPEED_MPS) : 0) \
  | ((ROOST_CAP_GNSS) ? ROOST_F(ROOST_GPS_TRACK_COURSE_DEG) : 0) \
  | ((ROOST_CAP_GNSS) ? ROOST_F(ROOST_GPS_TRACK_HDOP) : 0) \
  | ((ROOST_CAP_GNSS) ? ROOST_F(ROOST_GPS_TRACK_SATS) : 0) \
  | ((ROOST_CAP_GNSS) ? ROOST_F(ROOST_GPS_TRACK_FIX_TYPE) : 0) \
  | ((ROOST_CAP_GNSS) ? ROOST_F(ROOST_GPS_TRACK_FIX_AGE_MS) : 0) \
  )
#if !defined(ROOST_GPS_TRACK_EXCLUDE)
#define ROOST_GPS_TRACK_EXCLUDE 0
#endif
#define ROOST_GPS_TRACK_COLUMNS_MASK (ROOST_GPS_TRACK_CAPABLE_MASK & ~(RoostFieldMask)(ROOST_GPS_TRACK_EXCLUDE))
#if ROOST_EMITS_GPS_TRACK
typedef char roost_required_gps_track_declared[
    ((ROOST_GPS_TRACK_COLUMNS_MASK & ROOST_GPS_TRACK_REQUIRED_MASK)
     == ROOST_GPS_TRACK_REQUIRED_MASK) ? 1 : -1];
#endif

// device_event v1 - canonical field order.
enum {
  ROOST_DEVICE_EVENT_TIMESTAMP_UTC = 0,
  ROOST_DEVICE_EVENT_UPTIME_MS = 1,
  ROOST_DEVICE_EVENT_FIX_SEQ = 2,
  ROOST_DEVICE_EVENT_CAP_COMPONENT = 3,
  ROOST_DEVICE_EVENT_EVENT_KIND = 4,
  ROOST_DEVICE_EVENT_EVENT_COUNT = 5,
  ROOST_DEVICE_EVENT_EVENT_DETAIL = 6,
  ROOST_DEVICE_EVENT_FIELD_COUNT = 7
};
#define ROOST_DEVICE_EVENT_REQUIRED_MASK ((RoostFieldMask)0x000000000000001aull)
// Does this build emit device_event at all?
#define ROOST_EMITS_DEVICE_EVENT (1 && ROOST_CAP_STORAGE)
// Fields this build can populate, derived from the capability macros
// above. Never hand-written: a board with a card and one without
// differ only in board_config.h and this follows.
#define ROOST_DEVICE_EVENT_CAPABLE_MASK ( \
      ROOST_F(ROOST_DEVICE_EVENT_TIMESTAMP_UTC) \
  | ROOST_F(ROOST_DEVICE_EVENT_UPTIME_MS) \
  | ((ROOST_CAP_GNSS) ? ROOST_F(ROOST_DEVICE_EVENT_FIX_SEQ) : 0) \
  | ROOST_F(ROOST_DEVICE_EVENT_CAP_COMPONENT) \
  | ROOST_F(ROOST_DEVICE_EVENT_EVENT_KIND) \
  | ROOST_F(ROOST_DEVICE_EVENT_EVENT_COUNT) \
  | ROOST_F(ROOST_DEVICE_EVENT_EVENT_DETAIL) \
  )
#if !defined(ROOST_DEVICE_EVENT_EXCLUDE)
#define ROOST_DEVICE_EVENT_EXCLUDE 0
#endif
#define ROOST_DEVICE_EVENT_COLUMNS_MASK (ROOST_DEVICE_EVENT_CAPABLE_MASK & ~(RoostFieldMask)(ROOST_DEVICE_EVENT_EXCLUDE))
#if ROOST_EMITS_DEVICE_EVENT
typedef char roost_required_device_event_declared[
    ((ROOST_DEVICE_EVENT_COLUMNS_MASK & ROOST_DEVICE_EVENT_REQUIRED_MASK)
     == ROOST_DEVICE_EVENT_REQUIRED_MASK) ? 1 : -1];
#endif

// config_change v1 - canonical field order.
enum {
  ROOST_CONFIG_CHANGE_TIMESTAMP_UTC = 0,
  ROOST_CONFIG_CHANGE_UPTIME_MS = 1,
  ROOST_CONFIG_CHANGE_FIX_SEQ = 2,
  ROOST_CONFIG_CHANGE_CAP_COMPONENT = 3,
  ROOST_CONFIG_CHANGE_SETTING = 4,
  ROOST_CONFIG_CHANGE_VALUE = 5,
  ROOST_CONFIG_CHANGE_FIELD_COUNT = 6
};
#define ROOST_CONFIG_CHANGE_REQUIRED_MASK ((RoostFieldMask)0x000000000000001aull)
// Does this build emit config_change at all?
#define ROOST_EMITS_CONFIG_CHANGE (1 && ROOST_CAP_STORAGE)
// Fields this build can populate, derived from the capability macros
// above. Never hand-written: a board with a card and one without
// differ only in board_config.h and this follows.
#define ROOST_CONFIG_CHANGE_CAPABLE_MASK ( \
      ROOST_F(ROOST_CONFIG_CHANGE_TIMESTAMP_UTC) \
  | ROOST_F(ROOST_CONFIG_CHANGE_UPTIME_MS) \
  | ((ROOST_CAP_GNSS) ? ROOST_F(ROOST_CONFIG_CHANGE_FIX_SEQ) : 0) \
  | ROOST_F(ROOST_CONFIG_CHANGE_CAP_COMPONENT) \
  | ROOST_F(ROOST_CONFIG_CHANGE_SETTING) \
  | ROOST_F(ROOST_CONFIG_CHANGE_VALUE) \
  )
#if !defined(ROOST_CONFIG_CHANGE_EXCLUDE)
#define ROOST_CONFIG_CHANGE_EXCLUDE 0
#endif
#define ROOST_CONFIG_CHANGE_COLUMNS_MASK (ROOST_CONFIG_CHANGE_CAPABLE_MASK & ~(RoostFieldMask)(ROOST_CONFIG_CHANGE_EXCLUDE))
#if ROOST_EMITS_CONFIG_CHANGE
typedef char roost_required_config_change_declared[
    ((ROOST_CONFIG_CHANGE_COLUMNS_MASK & ROOST_CONFIG_CHANGE_REQUIRED_MASK)
     == ROOST_CONFIG_CHANGE_REQUIRED_MASK) ? 1 : -1];
#endif

// operator_mark v1 - canonical field order.
enum {
  ROOST_OPERATOR_MARK_TIMESTAMP_UTC = 0,
  ROOST_OPERATOR_MARK_UPTIME_MS = 1,
  ROOST_OPERATOR_MARK_FIX_SEQ = 2,
  ROOST_OPERATOR_MARK_CAP_COMPONENT = 3,
  ROOST_OPERATOR_MARK_FIELD_COUNT = 4
};
#define ROOST_OPERATOR_MARK_REQUIRED_MASK ((RoostFieldMask)0x000000000000000aull)
// Does this build emit operator_mark at all?
#define ROOST_EMITS_OPERATOR_MARK (ROOST_CAP_OPERATOR_MARK && ROOST_CAP_STORAGE)
// Fields this build can populate, derived from the capability macros
// above. Never hand-written: a board with a card and one without
// differ only in board_config.h and this follows.
#define ROOST_OPERATOR_MARK_CAPABLE_MASK ( \
      ROOST_F(ROOST_OPERATOR_MARK_TIMESTAMP_UTC) \
  | ROOST_F(ROOST_OPERATOR_MARK_UPTIME_MS) \
  | ((ROOST_CAP_GNSS) ? ROOST_F(ROOST_OPERATOR_MARK_FIX_SEQ) : 0) \
  | ROOST_F(ROOST_OPERATOR_MARK_CAP_COMPONENT) \
  )
#if !defined(ROOST_OPERATOR_MARK_EXCLUDE)
#define ROOST_OPERATOR_MARK_EXCLUDE 0
#endif
#define ROOST_OPERATOR_MARK_COLUMNS_MASK (ROOST_OPERATOR_MARK_CAPABLE_MASK & ~(RoostFieldMask)(ROOST_OPERATOR_MARK_EXCLUDE))
#if ROOST_EMITS_OPERATOR_MARK
typedef char roost_required_operator_mark_declared[
    ((ROOST_OPERATOR_MARK_COLUMNS_MASK & ROOST_OPERATOR_MARK_REQUIRED_MASK)
     == ROOST_OPERATOR_MARK_REQUIRED_MASK) ? 1 : -1];
#endif

static const char *const kRoostFieldsWifiObs[] = {
  "timestamp_utc",
  "uptime_ms",
  "fix_seq",
  "cap_component",
  "obs_mode",
  "mac",
  "detection_method",
  "frame_subtype",
  "rssi",
  "channel",
  "band",
  "ssid",
  "auth_mode",
  "addr1",
  "addr2",
  "addr3",
  "seq",
  "fc_flags",
  "frame_len",
  "bb_format",
  "cap_info",
  "beacon_interval",
  "ie_ids",
  "vendor_ies",
};
static const char *const kRoostFieldsBleObs[] = {
  "timestamp_utc",
  "uptime_ms",
  "fix_seq",
  "cap_component",
  "obs_mode",
  "mac",
  "addr_type",
  "detection_method",
  "rssi",
  "tx_power",
  "device_name",
  "pdu_type",
  "phy_primary",
  "phy_secondary",
  "sid",
  "active_scan",
  "adv_data_hex",
};
static const char *const kRoostFieldsGpsTrack[] = {
  "timestamp_utc",
  "uptime_ms",
  "fix_seq",
  "cap_component",
  "position_source",
  "lat",
  "lon",
  "alt_m",
  "speed_mps",
  "course_deg",
  "hdop",
  "sats",
  "fix_type",
  "fix_age_ms",
};
static const char *const kRoostFieldsDeviceEvent[] = {
  "timestamp_utc",
  "uptime_ms",
  "fix_seq",
  "cap_component",
  "event_kind",
  "event_count",
  "event_detail",
};
static const char *const kRoostFieldsConfigChange[] = {
  "timestamp_utc",
  "uptime_ms",
  "fix_seq",
  "cap_component",
  "setting",
  "value",
};
static const char *const kRoostFieldsOperatorMark[] = {
  "timestamp_utc",
  "uptime_ms",
  "fix_seq",
  "cap_component",
};

static const char *const *const kRoostRecordFields[ROOST_REC_COUNT] = {
  kRoostFieldsWifiObs,
  kRoostFieldsBleObs,
  kRoostFieldsGpsTrack,
  kRoostFieldsDeviceEvent,
  kRoostFieldsConfigChange,
  kRoostFieldsOperatorMark,
};

static const uint8_t kRoostRecordFieldCount[ROOST_REC_COUNT] = {
  24,
  17,
  14,
  7,
  6,
  4,
};

static const char *const kRoostRecordName[ROOST_REC_COUNT] = {
  "wifi_obs",
  "ble_obs",
  "gps_track",
  "device_event",
  "config_change",
  "operator_mark",
};

static const uint16_t kRoostRecordVersion[ROOST_REC_COUNT] = {
  2,
  1,
  1,
  1,
  1,
  1,
};

static const RoostFieldMask kRoostRecordRequired[ROOST_REC_COUNT] = {
  ROOST_WIFI_OBS_REQUIRED_MASK,
  ROOST_BLE_OBS_REQUIRED_MASK,
  ROOST_GPS_TRACK_REQUIRED_MASK,
  ROOST_DEVICE_EVENT_REQUIRED_MASK,
  ROOST_CONFIG_CHANGE_REQUIRED_MASK,
  ROOST_OPERATOR_MARK_REQUIRED_MASK,
};

// --------------------------------------------------------------------------
// Accessors. Header-only so this file drops into a PlatformIO project
// with no build-system change.
// --------------------------------------------------------------------------

static inline const char *roostRecordName(RoostRecord r) {
  return (r < ROOST_REC_COUNT) ? kRoostRecordName[r] : "";
}

static inline uint16_t roostRecordVersion(RoostRecord r) {
  return (r < ROOST_REC_COUNT) ? kRoostRecordVersion[r] : 0;
}

static inline uint8_t roostRecordFieldCount(RoostRecord r) {
  return (r < ROOST_REC_COUNT) ? kRoostRecordFieldCount[r] : 0;
}

// Which record types carry observations. The manifest's
// observations_written counts these and no others, so a device that adds
// a radio gets the right total without also editing its writer.
static inline int roostRecordIsObservation(RoostRecord r) {
  return r == ROOST_REC_WIFI_OBS || r == ROOST_REC_BLE_OBS;
}

static inline const char *roostFieldName(RoostRecord r, uint8_t idx) {
  if (r >= ROOST_REC_COUNT || idx >= kRoostRecordFieldCount[r]) return "";
  return kRoostRecordFields[r][idx];
}

// A device's mask must include every required field. A device that cannot
// populate one cannot emit the record type at all, so this is checked once
// at startup rather than per row.
static inline int roostMaskSatisfiesRequired(RoostRecord r, RoostFieldMask mask) {
  if (r >= ROOST_REC_COUNT) return 0;
  return (mask & kRoostRecordRequired[r]) == kRoostRecordRequired[r];
}

// Writes the CSV header for the caller's declared subset, in canonical
// order. Returns bytes written excluding the terminator, or 0 if it did
// not fit. The caller must not write a partial header,
// since a truncated header is worse than none.
static inline size_t roostHeader(char *out, size_t cap, RoostRecord r,
                                RoostFieldMask mask) {
  if (!out || !cap || r >= ROOST_REC_COUNT) return 0;
  size_t n = 0;
  const uint8_t count = kRoostRecordFieldCount[r];
  for (uint8_t i = 0; i < count; i++) {
    if (!(mask & ROOST_F(i))) continue;
    const char *name = kRoostRecordFields[r][i];
    const size_t len = strlen(name);
    const size_t need = len + (n ? 1u : 0u);
    // Leave the buffer empty rather than partially filled: a caller that
    // ignores the 0 return must not find a plausible-looking short header.
    if (n + need + 1u > cap) { out[0] = '\0'; return 0; }
    if (n) out[n++] = ',';
    memcpy(out + n, name, len);
    n += len;
  }
  out[n] = '\0';
  return n;
}

// A shared vocabulary can be legal on one record type and nonsense on
// another: detection_method covers both radios, but ble_oui on a Wi-Fi
// row means nothing. Check before writing; the shared C enum cannot
// express the restriction on its own.
static inline int roostValueAllowed(RoostRecord r, uint8_t fieldIdx, int value) {
  if (value < 0 || value >= 64) return 0;
  const uint64_t bit = (uint64_t)1u << value;
  switch (r) {
    case ROOST_REC_WIFI_OBS:
      switch (fieldIdx) {
        case ROOST_WIFI_OBS_DETECTION_METHOD:
          return (ROOST_WIFI_OBS_DETECTION_METHOD_ALLOWED & bit) != 0;
        default: return 1;
      }
    case ROOST_REC_BLE_OBS:
      switch (fieldIdx) {
        case ROOST_BLE_OBS_DETECTION_METHOD:
          return (ROOST_BLE_OBS_DETECTION_METHOD_ALLOWED & bit) != 0;
        default: return 1;
      }
    default: return 1;
  }
}

// --------------------------------------------------------------------
// Capture components
//
// cap_component on every row names one of these. The values are
// device-scoped rather than a fleet-wide vocabulary, because a role name
// cannot distinguish two radios serving the same band. Declared in
// board config so the manifest is rendered from it rather than restated.
// --------------------------------------------------------------------

// Band-reach flags. Immutable per component, and not the same thing as
// the channels it is currently tuned to, which are runtime.
#define ROOST_BAND_REACH_2_4 (1u << 0)
#define ROOST_BAND_REACH_5 (1u << 1)

// The board supplies this list, one X() per physical component:
//
//   #define ROOST_COMPONENTS(X)
//     X(ident, id, kind, part, bands)
//     ...
//
// `ident` is a C identifier the row writers use, so a component named on a
// row cannot disagree with the declaration through a typo.
#if !defined(ROOST_COMPONENTS)
#error "ROOST_COMPONENTS is undefined. Declare this board's capture components. "\
       "There is no safe default: a device that cannot say what it is made of "\
       "cannot say which part produced a row."
#endif

typedef enum {
#define ROOST_X(ident, id, kind, part, bands) ROOST_COMP_##ident,
  ROOST_COMPONENTS(ROOST_X)
#undef ROOST_X
  ROOST_COMPONENT_COUNT
} RoostComponent;

static inline const char *roostComponentId(RoostComponent c) {
  static const char *const v[] = {
#define ROOST_X(ident, id, kind, part, bands) id,
    ROOST_COMPONENTS(ROOST_X)
#undef ROOST_X
  };
  return (c < ROOST_COMPONENT_COUNT) ? v[c] : "";
}

static inline RoostComponentKind roostComponentKind(RoostComponent c) {
  static const RoostComponentKind v[] = {
#define ROOST_X(ident, id, kind, part, bands) ROOST_COMPONENT_KIND_##kind,
    ROOST_COMPONENTS(ROOST_X)
#undef ROOST_X
  };
  return (c < ROOST_COMPONENT_COUNT) ? v[c] : ROOST_COMPONENT_KIND_SYSTEM;
}

static inline const char *roostComponentPart(RoostComponent c) {
  static const char *const v[] = {
#define ROOST_X(ident, id, kind, part, bands) part,
    ROOST_COMPONENTS(ROOST_X)
#undef ROOST_X
  };
  return (c < ROOST_COMPONENT_COUNT) ? v[c] : 0;
}

static inline unsigned roostComponentBandMask(RoostComponent c) {
  static const unsigned v[] = {
#define ROOST_X(ident, id, kind, part, bands) (unsigned)(bands),
    ROOST_COMPONENTS(ROOST_X)
#undef ROOST_X
  };
  return (c < ROOST_COMPONENT_COUNT) ? v[c] : 0u;
}

// Ids must be present and distinct. cap_component on a row resolves
// against this list, so two components sharing an id make every row
// naming it ambiguous, which is the question the field exists to answer.
// The preprocessor cannot compare string literals, so this is the one
// component check that happens at boot rather than at build. Call it once;
// a failure is a device_event, not a silent start.
static inline int roostComponentsValid(void) {
  for (int i = 0; i < ROOST_COMPONENT_COUNT; i++) {
    const char *a = roostComponentId((RoostComponent)i);
    if (!a || !a[0]) return 0;
    for (int j = i + 1; j < ROOST_COMPONENT_COUNT; j++) {
      const char *b = roostComponentId((RoostComponent)j);
      if (b && strcmp(a, b) == 0) return 0;
    }
  }
  return 1;
}

// Renders the manifest's "components" array. Nothing hand-written.
static inline size_t roostManifestComponents(char *out, size_t cap) {
  if (!out || !cap) return 0;
  size_t o = 0;
#define ROOST_CPUT(s)                                  \
  do {                                                 \
    const char *s_ = (s);                              \
    const size_t l_ = strlen(s_);                      \
    if (o + l_ + 1 > cap) { out[0] = '\0'; return 0; } \
    memcpy(out + o, s_, l_);                           \
    o += l_;                                           \
  } while (0)
  ROOST_CPUT("[");
  for (int i = 0; i < ROOST_COMPONENT_COUNT; i++) {
    const RoostComponent c = (RoostComponent)i;
    if (i) ROOST_CPUT(",");
    ROOST_CPUT("{\"id\":\"");
    ROOST_CPUT(roostComponentId(c));
    ROOST_CPUT("\",\"kind\":\"");
    ROOST_CPUT(kRoostComponentKind[roostComponentKind(c)]);
    ROOST_CPUT("\",\"part\":");
    if (roostComponentPart(c) && roostComponentPart(c)[0]) {
      ROOST_CPUT("\"");
      ROOST_CPUT(roostComponentPart(c));
      ROOST_CPUT("\"");
    } else {
      ROOST_CPUT("null");
    }
    ROOST_CPUT(",\"bands\":");
    if (!roostComponentBandMask(c)) {
      ROOST_CPUT("null");
    } else {
      int first = 1;
      ROOST_CPUT("[");
      if (roostComponentBandMask(c) & ROOST_BAND_REACH_2_4) {
        if (!first) ROOST_CPUT(",");
        ROOST_CPUT("\"2.4\"");
        first = 0;
      }
      if (roostComponentBandMask(c) & ROOST_BAND_REACH_5) {
        if (!first) ROOST_CPUT(",");
        ROOST_CPUT("\"5\"");
        first = 0;
      }
      ROOST_CPUT("]");
    }
    ROOST_CPUT("}");
  }
  ROOST_CPUT("]");
#undef ROOST_CPUT
  out[o] = '\0';
  return o;
}

// --------------------------------------------------------------------
// Row assembly
//
// The single entry point for writing a row. It owns canonical order,
// RFC 4180 quoting, MAC case, float precision, enum restriction, and the
// required-field check, so none of those is a rule a device is trusted to
// follow. Adding a field to a record type means adding one setter call,
// not editing a format string on every device.
// --------------------------------------------------------------------

typedef enum {
  ROOST_FT_TEXT,
  ROOST_FT_MAC,
  ROOST_FT_HEX,
  ROOST_FT_ENUM,
  ROOST_FT_BOOL,
  ROOST_FT_UINT,
  ROOST_FT_INT,
  ROOST_FT_FLOAT,
} RoostFieldType;

static const uint8_t kRoostWifiObsFieldType[] = {
  ROOST_FT_TEXT, ROOST_FT_UINT, ROOST_FT_UINT, ROOST_FT_TEXT, ROOST_FT_ENUM, ROOST_FT_MAC, ROOST_FT_ENUM, ROOST_FT_ENUM, ROOST_FT_INT, ROOST_FT_UINT, ROOST_FT_ENUM, ROOST_FT_TEXT, ROOST_FT_ENUM, ROOST_FT_MAC, ROOST_FT_MAC, ROOST_FT_MAC, ROOST_FT_UINT, ROOST_FT_HEX, ROOST_FT_UINT, ROOST_FT_ENUM, ROOST_FT_HEX, ROOST_FT_UINT, ROOST_FT_TEXT, ROOST_FT_TEXT};
static const uint8_t kRoostBleObsFieldType[] = {
  ROOST_FT_TEXT, ROOST_FT_UINT, ROOST_FT_UINT, ROOST_FT_TEXT, ROOST_FT_ENUM, ROOST_FT_MAC, ROOST_FT_ENUM, ROOST_FT_ENUM, ROOST_FT_INT, ROOST_FT_INT, ROOST_FT_TEXT, ROOST_FT_ENUM, ROOST_FT_ENUM, ROOST_FT_ENUM, ROOST_FT_UINT, ROOST_FT_BOOL, ROOST_FT_HEX};
static const uint8_t kRoostGpsTrackFieldType[] = {
  ROOST_FT_TEXT, ROOST_FT_UINT, ROOST_FT_UINT, ROOST_FT_TEXT, ROOST_FT_ENUM, ROOST_FT_FLOAT, ROOST_FT_FLOAT, ROOST_FT_FLOAT, ROOST_FT_FLOAT, ROOST_FT_FLOAT, ROOST_FT_FLOAT, ROOST_FT_UINT, ROOST_FT_ENUM, ROOST_FT_UINT};
static const uint8_t kRoostGpsTrackFieldPrec[] = {
  0, 0, 0, 0, 0, 6, 6, 1, 2, 1, 2, 0, 0, 0};
static const uint8_t kRoostDeviceEventFieldType[] = {
  ROOST_FT_TEXT, ROOST_FT_UINT, ROOST_FT_UINT, ROOST_FT_TEXT, ROOST_FT_ENUM, ROOST_FT_UINT, ROOST_FT_TEXT};
static const uint8_t kRoostConfigChangeFieldType[] = {
  ROOST_FT_TEXT, ROOST_FT_UINT, ROOST_FT_UINT, ROOST_FT_TEXT, ROOST_FT_ENUM, ROOST_FT_TEXT};
static const uint8_t kRoostOperatorMarkFieldType[] = {
  ROOST_FT_TEXT, ROOST_FT_UINT, ROOST_FT_UINT, ROOST_FT_TEXT};

static const uint8_t *const kRoostFieldType[] = {
  kRoostWifiObsFieldType,
  kRoostBleObsFieldType,
  kRoostGpsTrackFieldType,
  kRoostDeviceEventFieldType,
  kRoostConfigChangeFieldType,
  kRoostOperatorMarkFieldType,
};
static const uint8_t *const kRoostFieldPrec[] = {
  0,
  0,
  kRoostGpsTrackFieldPrec,
  0,
  0,
  0,
};

static const char *const *const kRoostWifiObsEnumNames[] = {
  0, 0, 0, 0, kRoostObsMode, 0, kRoostDetectionMethod, kRoostFrameSubtype, 0, 0, kRoostBand, 0, kRoostAuthMode, 0, 0, 0, 0, 0, 0, kRoostBbFormat, 0, 0, 0, 0};
static const uint8_t kRoostWifiObsEnumCount[] = {
  0, 0, 0, 0, 2, 0, 10, 17, 0, 0, 2, 0, 11, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0};
static const char *const *const kRoostBleObsEnumNames[] = {
  0, 0, 0, 0, kRoostObsMode, 0, kRoostBleAddrType, kRoostDetectionMethod, 0, 0, 0, kRoostBlePduType, kRoostBlePhy, kRoostBlePhy, 0, 0, 0};
static const uint8_t kRoostBleObsEnumCount[] = {
  0, 0, 0, 0, 2, 0, 2, 10, 0, 0, 0, 7, 3, 3, 0, 0, 0};
static const char *const *const kRoostGpsTrackEnumNames[] = {
  0, 0, 0, 0, kRoostPositionSource, 0, 0, 0, 0, 0, 0, 0, kRoostFixType, 0};
static const uint8_t kRoostGpsTrackEnumCount[] = {
  0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 4, 0};
static const char *const *const kRoostDeviceEventEnumNames[] = {
  0, 0, 0, 0, kRoostDeviceEventKind, 0, 0};
static const uint8_t kRoostDeviceEventEnumCount[] = {
  0, 0, 0, 0, 9, 0, 0};
static const char *const *const kRoostConfigChangeEnumNames[] = {
  0, 0, 0, 0, kRoostConfigSetting, 0};
static const uint8_t kRoostConfigChangeEnumCount[] = {
  0, 0, 0, 0, 7, 0};
static const char *const *const kRoostOperatorMarkEnumNames[] = {
  0, 0, 0, 0};
static const uint8_t kRoostOperatorMarkEnumCount[] = {
  0, 0, 0, 0};

static const char *const *const *const kRoostEnumNames[] = {
  kRoostWifiObsEnumNames,
  kRoostBleObsEnumNames,
  kRoostGpsTrackEnumNames,
  kRoostDeviceEventEnumNames,
  kRoostConfigChangeEnumNames,
  kRoostOperatorMarkEnumNames,
};
static const uint8_t *const kRoostEnumCount[] = {
  kRoostWifiObsEnumCount,
  kRoostBleObsEnumCount,
  kRoostGpsTrackEnumCount,
  kRoostDeviceEventEnumCount,
  kRoostConfigChangeEnumCount,
  kRoostOperatorMarkEnumCount,
};

// The wire spelling of an enum value on a given column, or null if the
// column is not an enum or the value is outside its vocabulary.
static inline const char *roostEnumName(RoostRecord r, uint8_t idx, int v) {
  if (r >= ROOST_REC_COUNT || idx >= roostRecordFieldCount(r)) return 0;
  const char *const *names = kRoostEnumNames[r][idx];
  if (!names || v < 0 || v >= (int)kRoostEnumCount[r][idx]) return 0;
  return names[v];
}

static inline RoostFieldType roostFieldTypeOf(RoostRecord r, uint8_t i) {
  return (RoostFieldType)kRoostFieldType[r][i];
}

// A row under construction. Holds no allocation and copies nothing it
// does not own; the caller supplies the buffer and keeps it alive.
typedef struct {
  char *out;
  size_t cap;
  size_t n;
  RoostRecord record;
  RoostFieldMask columns;  // what this file declared
  RoostFieldMask written;  // what has been set so far
  uint8_t next;            // next declared column awaiting emission
  uint16_t fields;         // declared columns emitted, empty ones included
  uint16_t unknownEnums;   // names roostRowSetEnumByName could not resolve
  int ok;                  // cleared by the first refusal; never recovers
} RoostRow;

static inline void roostRowBegin(RoostRow *w_, char *out, size_t cap,
                                 RoostRecord r, RoostFieldMask columns) {
  w_->out = out; w_->cap = cap; w_->n = 0;
  w_->record = r; w_->columns = columns; w_->written = 0;
  w_->fields = 0; w_->unknownEnums = 0;
  w_->next = 0;
  w_->ok = (out && cap && r < ROOST_REC_COUNT) ? 1 : 0;
  if (w_->ok) out[0] = '\0';
}

static inline int roostRowRaw_(RoostRow *w_, const char *s, size_t len) {
  if (!w_->ok) return 0;
  if (w_->n + len + 1 > w_->cap) { w_->ok = 0; return 0; }
  memcpy(w_->out + w_->n, s, len);
  w_->n += len;
  w_->out[w_->n] = '\0';
  return 1;
}

// Advances to `idx`, emitting a separator per declared column passed and
// leaving each of them empty. Refuses to go backwards: a column the row has
// already emitted cannot be filled in afterwards, which is what makes
// canonical order a property of the API rather than a rule to remember.
static inline int roostRowSeek_(RoostRow *w_, uint8_t idx) {
  if (!w_->ok) return 0;
  if (idx >= roostRecordFieldCount(w_->record)) { w_->ok = 0; return 0; }
  if (!(w_->columns & ROOST_F(idx))) return 0;  // not declared: benign no-op
  if (idx < w_->next) { w_->ok = 0; return 0; }  // out of order
  for (uint8_t i = w_->next; i <= idx; i++) {
    if (!(w_->columns & ROOST_F(i))) continue;
    // Counted, not measured. Testing w_->n here would ask whether any
    // bytes have been written, a different question from whether any field
    // has been emitted precisely when the leading columns are empty, and
    // every later value lands one column left.
    if (w_->fields++ && !roostRowRaw_(w_, ",", 1)) return 0;
  }
  w_->next = (uint8_t)(idx + 1);
  w_->written |= ROOST_F(idx);
  return 1;
}

static inline int roostRowCheck_(RoostRow *w_, uint8_t idx, RoostFieldType t) {
  if (!w_->ok) return 0;
  if (idx >= roostRecordFieldCount(w_->record)) { w_->ok = 0; return 0; }
  // A setter of the wrong class for the field is a coding error, not a
  // value problem: writing an int into an ssid column would produce a row
  // that parses and means nothing.
  if (roostFieldTypeOf(w_->record, idx) != t) { w_->ok = 0; return 0; }
  return 1;
}

// Length in bytes of the well-formed, shortest-form UTF-8 sequence at `p`,
// or 0 if there is not one. Rejecting overlongs, encoded surrogates,
// anything above U+10FFFF and truncation is what makes ill-formedness
// decidable a byte at a time, which is what the text encoding escapes on
// (docs/design_spec.md section 3.4).
static inline size_t roostUtf8SeqLen_(const char *p, size_t avail) {
  const unsigned char a = (unsigned char)p[0];
  size_t need;
  unsigned long cp;
  // 0xC0 and 0xC1 are excluded by starting at 0xC2: they can only ever
  // introduce an overlong two-byte form.
  if (a >= 0xC2 && a <= 0xDF) { need = 2; cp = (unsigned long)(a & 0x1F); }
  else if (a >= 0xE0 && a <= 0xEF) { need = 3; cp = (unsigned long)(a & 0x0F); }
  else if (a >= 0xF0 && a <= 0xF4) { need = 4; cp = (unsigned long)(a & 0x07); }
  else return 0;  // 0x80-0xC1 and 0xF5-0xFF are never a lead byte
  if (avail < need) return 0;
  for (size_t k = 1; k < need; k++) {
    const unsigned char b = (unsigned char)p[k];
    if ((b & 0xC0) != 0x80) return 0;
    cp = (cp << 6) | (unsigned long)(b & 0x3F);
  }
  if (need == 3 && cp < 0x800UL) return 0;
  if (need == 4 && cp < 0x10000UL) return 0;
  if (cp >= 0xD800UL && cp <= 0xDFFFUL) return 0;
  if (cp > 0x10FFFFUL) return 0;
  return need;
}

// Text, RFC 4180. Quotes only when it must, doubles embedded quotes,
// and encodes rather than emits anything that would break a reader, so the
// original bytes survive a round trip.
//
// Takes a length. The NUL-terminated roostRowSetText below cannot express a
// value containing 0x00, and an SSID is 0-32 arbitrary octets while a BLE
// device name comes from a length-delimited AD structure. For anything
// attacker-controlled this is the correct entry point; the short form is a
// silent truncation.
static inline int roostRowSetTextN(RoostRow *w_, uint8_t idx, const char *v,
                                   size_t n) {
  if (!roostRowCheck_(w_, idx, ROOST_FT_TEXT)) return 0;
  if (!roostRowSeek_(w_, idx)) return 0;
  if (!v || !n) return 1;
  static const char kHexDigits[] = "0123456789abcdef";
  int quote = 0;
  for (size_t i = 0; i < n; i++) {
    if (v[i] == ',' || v[i] == '"') { quote = 1; break; }
  }
  if (v[0] == ' ' || v[n - 1] == ' ') quote = 1;
  if (quote && !roostRowRaw_(w_, "\"", 1)) return 0;
  for (size_t i = 0; i < n;) {
    const unsigned char c = (unsigned char)v[i];
    // A control byte is encoded, never emitted raw and never dropped.
    // Reversible, so the original text is recoverable, while
    // the file stays usable by every line-oriented tool that touches it.
    if (c < 0x20 || c == 0x7F) {
      const char esc[4] = {'\\', 'x', kHexDigits[c >> 4], kHexDigits[c & 0x0F]};
      if (!roostRowRaw_(w_, esc, 4)) return 0;
      i++;
      continue;
    }
    // The escape introducer itself, or the encoding would be ambiguous.
    if (c == '\\') {
      if (!roostRowRaw_(w_, "\\\\", 2)) return 0;
      i++;
      continue;
    }
    if (c < 0x80) {
      if (!roostRowRaw_(w_, v + i, 1)) return 0;
      if (c == '"' && !roostRowRaw_(w_, "\"", 1)) return 0;
      i++;
      continue;
    }
    // High bytes are escaped on validity, not on range. Well-formed UTF-8
    // passes through whole, so a real SSID stays legible; anything else is
    // escaped one byte at a time and scanning resumes at the next byte,
    // which is what keeps the encoding byte-exact reversible.
    {
      const size_t seq = roostUtf8SeqLen_(v + i, n - i);
      if (seq) {
        if (!roostRowRaw_(w_, v + i, seq)) return 0;
        i += seq;
        continue;
      }
      const char esc[4] = {'\\', 'x', kHexDigits[c >> 4], kHexDigits[c & 0x0F]};
      if (!roostRowRaw_(w_, esc, 4)) return 0;
      i++;
    }
  }
  if (quote && !roostRowRaw_(w_, "\"", 1)) return 0;
  return 1;
}

// For values that are C strings by construction. Truncates at the first
// 0x00; use roostRowSetTextN where the value came off the air.
static inline int roostRowSetText(RoostRow *w_, uint8_t idx, const char *v) {
  if (!v) return roostRowSetTextN(w_, idx, v, 0);
  return roostRowSetTextN(w_, idx, v, strlen(v));
}

static inline int roostRowSetInt(RoostRow *w_, uint8_t idx, long v) {
  if (!roostRowCheck_(w_, idx, ROOST_FT_INT)) return 0;
  if (!roostRowSeek_(w_, idx)) return 0;
  char b[24]; snprintf(b, sizeof(b), "%ld", v);
  return roostRowRaw_(w_, b, strlen(b));
}

static inline int roostRowSetUInt(RoostRow *w_, uint8_t idx, unsigned long v) {
  if (!roostRowCheck_(w_, idx, ROOST_FT_UINT)) return 0;
  if (!roostRowSeek_(w_, idx)) return 0;
  char b[24]; snprintf(b, sizeof(b), "%lu", v);
  return roostRowRaw_(w_, b, strlen(b));
}

static inline int roostRowSetBool(RoostRow *w_, uint8_t idx, int v) {
  if (!roostRowCheck_(w_, idx, ROOST_FT_BOOL)) return 0;
  if (!roostRowSeek_(w_, idx)) return 0;
  return roostRowRaw_(w_, v ? "1" : "0", 1);
}

// Decimal places come from the field definition, never from the caller, so
// two devices cannot write a coordinate at different precision.
static inline int roostRowSetFloat(RoostRow *w_, uint8_t idx, double v) {
  if (!roostRowCheck_(w_, idx, ROOST_FT_FLOAT)) return 0;
  if (!roostRowSeek_(w_, idx)) return 0;
  const uint8_t *pt = kRoostFieldPrec[w_->record];
  char b[40]; snprintf(b, sizeof(b), "%.*f", pt ? (int)pt[idx] : 6, v);
  return roostRowRaw_(w_, b, strlen(b));
}

// Lowercase, colon-separated, normalized here rather than at ingest.
static inline int roostRowSetMac(RoostRow *w_, uint8_t idx, const uint8_t *m) {
  if (!roostRowCheck_(w_, idx, ROOST_FT_MAC)) return 0;
  if (!roostRowSeek_(w_, idx)) return 0;
  if (!m) return 1;
  char b[18];
  snprintf(b, sizeof(b), "%02x:%02x:%02x:%02x:%02x:%02x",
           m[0], m[1], m[2], m[3], m[4], m[5]);
  return roostRowRaw_(w_, b, 17);
}

static inline int roostRowSetHex(RoostRow *w_, uint8_t idx,
                                 const uint8_t *b, size_t len) {
  if (!roostRowCheck_(w_, idx, ROOST_FT_HEX)) return 0;
  if (!roostRowSeek_(w_, idx)) return 0;
  if (!b || !len) return 1;
  static const char kHex[] = "0123456789abcdef";
  for (size_t i = 0; i < len; i++) {
    const char pair[2] = {kHex[b[i] >> 4], kHex[b[i] & 0x0F]};
    if (!roostRowRaw_(w_, pair, 2)) return 0;
  }
  return 1;
}

// Enum values go through the per-record restriction, so a BLE
// detection method cannot land on a Wi-Fi row even though both share one
// vocabulary.
static inline int roostRowSetEnum(RoostRow *w_, uint8_t idx, int value) {
  if (!roostRowCheck_(w_, idx, ROOST_FT_ENUM)) return 0;
  if (!roostValueAllowed(w_->record, idx, value)) { w_->ok = 0; return 0; }
  if (!roostRowSeek_(w_, idx)) return 0;
  const char *s = roostEnumName(w_->record, idx, value);
  return s ? roostRowRaw_(w_, s, strlen(s)) : 0;
}

// Sets an enum column from the wire spelling of its value.
//
// Devices arrive at this contract with producers that already return name
// strings: a frame subtype, a detection method, a baseband format. Those
// producers usually have other consumers, a display or a console, so
// rewriting them to return roost enums means either duplicating the table
// or breaking something. Resolving the name against the registry's own
// vocabulary keeps one table, and it is the generated one.
//
// This is a first-class setter rather than something each device writes,
// because every device has the same problem, and one hand-written reverse
// lookup per device is one chance per device to differ on what an
// unresolvable name does.
//
// An unresolvable name leaves the column empty and counts itself in
// w_->unknownEnums; it does not void the row. A name is device code and
// the registry cannot check it at compile time, so this is drift that will
// happen, and the proportionate response is to lose the field rather than
// the observation. If the column is required, leaving it unwritten makes
// roostRowFinish() refuse anyway, which is the right escalation and needs
// no special case here.
//
// Read w_->unknownEnums after finishing and surface it. A silent empty
// column is ambiguous: it reads as 'nothing to record' when the truth is
// 'a producer and the registry have drifted'.
static inline int roostRowSetEnumByName(RoostRow *w_, uint8_t idx,
                                        const char *name) {
  if (!roostRowCheck_(w_, idx, ROOST_FT_ENUM)) return 0;
  // Not declared on this build: a benign no-op, and not drift. Counting
  // it would make every device that declines a column look broken.
  if (!(w_->columns & ROOST_F(idx))) return 0;
  // No value is not drift. An empty name is a producer saying the frame
  // carried nothing for this column: a legacy advertisement has no
  // secondary PHY, a fix seen without a GSA sentence has no fix type. That
  // is what an optional enum column is for. Counting it would raise the
  // drift counter on every such row, leaving a warning that is always on
  // and so carries no signal.
  if (!name || !name[0]) return 0;
  for (int v = 0;; v++) {
    const char *candidate = roostEnumName(w_->record, idx, v);
    if (!candidate) break;
    if (strcmp(candidate, name) == 0) return roostRowSetEnum(w_, idx, v);
  }
  // A non-empty name the registry does not carry. This is drift.
  w_->unknownEnums++;
  return 0;
}

// Pads out the declared columns the caller never set, then checks that
// every required one was populated. Returns 0 and empties the buffer on any
// refusal: a partial row is worse than no row, because it parses.
static inline size_t roostRowFinish(RoostRow *w_) {
  if (!w_->ok) { if (w_->out && w_->cap) w_->out[0] = '\0'; return 0; }
  const uint8_t count = roostRecordFieldCount(w_->record);
  for (uint8_t i = w_->next; i < count; i++) {
    if (!(w_->columns & ROOST_F(i))) continue;
    if (w_->fields++ && !roostRowRaw_(w_, ",", 1)) { w_->out[0] = '\0'; return 0; }
  }
  const RoostFieldMask req = kRoostRecordRequired[w_->record] & w_->columns;
  if ((w_->written & req) != req) { w_->out[0] = '\0'; return 0; }
  return w_->n;
}

// What a raw capture's record timestamps mean. A property of the
// file, not a report of whether the session's clock anchored.
typedef enum {
  ROOST_TIMEBASE_BOOT = 0,
  ROOST_TIMEBASE_UTC = 1,
  ROOST_TIMEBASE_COUNT,
} RoostTimebase;

static const char *const kRoostTimebase[ROOST_TIMEBASE_COUNT] = {
  "boot",
  "utc",
};

// The pcap declaration. Not a record type: it has no columns, and its
// shape (linktype, snaplen) is a capture-format fact rather than a
// registry one. Generated anyway so the manifest's files array has one
// producer rather than one generated half and one hand-written half.
static inline size_t roostManifestPcapFile(char *out, size_t cap,
                                           const char *name,
                                           uint16_t linktype,
                                           uint16_t snaplen,
                                           RoostTimebase timebase) {
  if (!out || !cap || !name) return 0;
  if ((unsigned)timebase >= (unsigned)ROOST_TIMEBASE_COUNT) return 0;
  const int n = snprintf(out, cap,
      "{\"record\":\"frames\",\"format\":\"pcap\",\"name\":\"%s\","
      "\"linktype\":%u,\"snaplen\":%u,\"timebase\":\"%s\"}",
      name, (unsigned)linktype, (unsigned)snaplen,
      kRoostTimebase[timebase]);
  if (n <= 0 || (size_t)n >= cap) { out[0] = '\0'; return 0; }
  return (size_t)n;
}

// The filename a record type's file takes inside a session directory.
// Version rides in the name so a file separated from its manifest is
// still interpretable.
//
// Returns the bare name, not a path. The session directory is the caller's
// business, since only it knows the session number and mount point.
static inline size_t roostFileName(char *out, size_t cap, RoostRecord r) {
  if (!out || !cap || r >= ROOST_REC_COUNT) return 0;
  const char *name = kRoostRecordName[r];
  const uint16_t ver = kRoostRecordVersion[r];
  size_t n = 0;
  const size_t len = strlen(name);
  if (len + 8u > cap) return 0;
  memcpy(out, name, len); n = len;
  out[n++] = '.'; out[n++] = 'v';
  if (ver >= 100) out[n++] = (char)('0' + (ver / 100) % 10);
  if (ver >= 10)  out[n++] = (char)('0' + (ver / 10) % 10);
  out[n++] = (char)('0' + ver % 10);
  memcpy(out + n, ".csv", 4); n += 4;
  out[n] = '\0';
  return n;
}

// --------------------------------------------------------------------------
// Session manifest.
//
// The contract half is rendered from the registry and the caller's masks,
// never hand-written. A hand-written copy of a derivable fact is exactly
// the drift this file prevents, so there is no way to state a
// column list here that disagrees with the one roostHeader() emits.
//
// The provenance half is the caller's to fill, but its key names come from
// the constants below so two devices cannot spell one fact two ways.
// --------------------------------------------------------------------------

#define ROOST_MANIFEST_VERSION 2

// Which revision of the text encoding roostRowSetTextN implements. Folded
// into the registry hash, so it is stated here for a test to assert against
// rather than for a device to publish.
#define ROOST_TEXT_ENCODING_REV 2

// identity (build tier)
#define ROOST_MK_REGISTRY_HASH "registry_hash"
#define ROOST_MK_DEVICE_MODEL "device_model"
#define ROOST_MK_DEVICE_SERIAL "device_serial"
#define ROOST_MK_HW_REVISION "hw_revision"
#define ROOST_MK_FW_VERSION "fw_version"
#define ROOST_MK_BUILT_AT "built_at"

// hardware (build tier)
#define ROOST_MK_OWN_MACS "own_macs"
#define ROOST_MK_COMPONENTS "components"
#define ROOST_MK_GNSS_CEP_M "gnss_cep_m"

// session (session tier)
#define ROOST_MK_SESSION_ID "session_id"
#define ROOST_MK_SEQUENCE "sequence"
#define ROOST_MK_BOOT_COUNT "boot_count"
#define ROOST_MK_STARTED_UTC "started_utc"
#define ROOST_MK_ENDED_UTC "ended_utc"
#define ROOST_MK_CLOCK_SOURCE "clock_source"
#define ROOST_MK_CLOCK_ANCHOR_UNIX "clock_anchor_unix"
#define ROOST_MK_CLOCK_ANCHOR_UPTIME_MS "clock_anchor_uptime_ms"

// capture (session tier)
#define ROOST_MK_OUI_TABLE_HASH "oui_table_hash"
#define ROOST_MK_IE_TABLE_HASH "ie_table_hash"
#define ROOST_MK_DEDUP_POLICY "dedup_policy"
#define ROOST_MK_STORAGE_TIER "storage_tier"

// counters (session tier)
#define ROOST_MK_OBSERVATIONS_WRITTEN "observations_written"
#define ROOST_MK_OBSERVATIONS_SUPPRESSED "observations_suppressed"
#define ROOST_MK_OBSERVATIONS_DROPPED "observations_dropped"
#define ROOST_MK_FIXES_WRITTEN "fixes_written"
#define ROOST_MK_STORAGE_ERRORS "storage_errors"
#define ROOST_MK_WORST_FLUSH_MS "worst_flush_ms"

// What a device declares about one file it is writing.
//
// Three states, and telling them apart is what the contract guarantees:
//   in `columns`                 the file carries this column. An empty
//                                value in a row means uncaptured.
//   in `capable`, not `columns`  the hardware can measure it but this
//                                configuration does not record it.
//   in neither                   this hardware cannot measure it at all,
//                                so the field is uncapturable.
//
// `capable` must be a superset of `columns`; roostFileDeclValid() checks it.
typedef struct {
  RoostRecord    record;
  RoostFieldMask columns;
  RoostFieldMask capable;
} RoostFileDecl;

// Fills `out` with one declaration per record type this build emits, in
// registry order, and returns how many. Derived entirely from the
// capability macros: a board declares capabilities, never a file list.
//
// Returns 0 if `cap` is too small, rather than a truncated set that would
// silently omit a record type from the session and its manifest.
static inline size_t roostDeclaredFiles(RoostFileDecl *out, size_t cap) {
  size_t n = 0;
  // A build emitting no record type at all, with no storage fitted, reaches
  // neither parameter, and an unused one is an error under -Werror.
  (void)out; (void)cap;
#if ROOST_EMITS_WIFI_OBS
  if (n >= cap) return 0;
  out[n].record  = ROOST_REC_WIFI_OBS;
  out[n].columns = ROOST_WIFI_OBS_COLUMNS_MASK;
  out[n].capable = ROOST_WIFI_OBS_CAPABLE_MASK;
  n++;
#endif
#if ROOST_EMITS_BLE_OBS
  if (n >= cap) return 0;
  out[n].record  = ROOST_REC_BLE_OBS;
  out[n].columns = ROOST_BLE_OBS_COLUMNS_MASK;
  out[n].capable = ROOST_BLE_OBS_CAPABLE_MASK;
  n++;
#endif
#if ROOST_EMITS_GPS_TRACK
  if (n >= cap) return 0;
  out[n].record  = ROOST_REC_GPS_TRACK;
  out[n].columns = ROOST_GPS_TRACK_COLUMNS_MASK;
  out[n].capable = ROOST_GPS_TRACK_CAPABLE_MASK;
  n++;
#endif
#if ROOST_EMITS_DEVICE_EVENT
  if (n >= cap) return 0;
  out[n].record  = ROOST_REC_DEVICE_EVENT;
  out[n].columns = ROOST_DEVICE_EVENT_COLUMNS_MASK;
  out[n].capable = ROOST_DEVICE_EVENT_CAPABLE_MASK;
  n++;
#endif
#if ROOST_EMITS_CONFIG_CHANGE
  if (n >= cap) return 0;
  out[n].record  = ROOST_REC_CONFIG_CHANGE;
  out[n].columns = ROOST_CONFIG_CHANGE_COLUMNS_MASK;
  out[n].capable = ROOST_CONFIG_CHANGE_CAPABLE_MASK;
  n++;
#endif
#if ROOST_EMITS_OPERATOR_MARK
  if (n >= cap) return 0;
  out[n].record  = ROOST_REC_OPERATOR_MARK;
  out[n].columns = ROOST_OPERATOR_MARK_COLUMNS_MASK;
  out[n].capable = ROOST_OPERATOR_MARK_CAPABLE_MASK;
  n++;
#endif
  return n;
}

// Upper bound for a caller's array, so a device need not count by hand.
#define ROOST_MAX_DECLARED_FILES ROOST_REC_COUNT

static inline int roostFileDeclValid(const RoostFileDecl *d) {
  if (!d || d->record >= ROOST_REC_COUNT) return 0;
  if ((d->columns & d->capable) != d->columns) return 0;
  return roostMaskSatisfiesRequired(d->record, d->columns);
}

// Renders the manifest's "files" array. Returns bytes written excluding
// the terminator, or 0 if it did not fit or a declaration was invalid. In
// which case the buffer is emptied, since a truncated manifest is worse
// than none.
//
// `raw` carries entries this generator cannot render from a record
// declaration, which today means a raw capture file: a pcap has no columns
// and no record version, so roostManifestPcapFile builds it and it is
// appended here. They go in the same array rather than beside it so
// a reader walks one list to find everything the session produced. Pass
// nullptr and 0 when there are none.
static inline size_t roostManifestFiles(char *out, size_t cap,
                                       const RoostFileDecl *files,
                                       size_t n,
                                       const char *const *raw,
                                       size_t nRaw) {
  if (!out || !cap) return 0;
  size_t o = 0;
#define ROOST_PUT(s)                                   \
  do {                                                 \
    const char *s_ = (s);                               \
    const size_t l_ = strlen(s_);                       \
    if (o + l_ + 1u > cap) { out[0] = '\0'; return 0; } \
    memcpy(out + o, s_, l_); o += l_;                   \
  } while (0)
  ROOST_PUT("\"files\":[");
  for (size_t i = 0; i < n; i++) {
    const RoostFileDecl *d = &files[i];
    if (!roostFileDeclValid(d)) { out[0] = '\0'; return 0; }
    if (i) ROOST_PUT(",");
    ROOST_PUT("{\"record\":\"");
    ROOST_PUT(roostRecordName(d->record));
    ROOST_PUT("\",\"version\":");
    {
      char v[8]; size_t vi = 0; uint16_t ver = roostRecordVersion(d->record);
      if (ver >= 100) v[vi++] = (char)('0' + (ver / 100) % 10);
      if (ver >= 10)  v[vi++] = (char)('0' + (ver / 10) % 10);
      v[vi++] = (char)('0' + ver % 10); v[vi] = '\0';
      ROOST_PUT(v);
    }
    ROOST_PUT(",\"format\":\"csv\",\"name\":\"");
    {
      char fn[64];
      if (!roostFileName(fn, sizeof(fn), d->record)) { out[0] = '\0'; return 0; }
      ROOST_PUT(fn);
    }
    ROOST_PUT("\",\"columns\":[");
    {
      int first = 1;
      for (uint8_t f = 0; f < roostRecordFieldCount(d->record); f++) {
        if (!(d->columns & ROOST_F(f))) continue;
        if (!first) ROOST_PUT(",");
        first = 0;
        ROOST_PUT("\"");
        ROOST_PUT(roostFieldName(d->record, f));
        ROOST_PUT("\"");
      }
    }
    // Capable-but-not-recorded. Empty is the common case and means the
    // configuration records everything the hardware can reach.
    ROOST_PUT("],\"capable_unrecorded\":[");
    {
      int first = 1;
      const RoostFieldMask extra = d->capable & ~d->columns;
      for (uint8_t f = 0; f < roostRecordFieldCount(d->record); f++) {
        if (!(extra & ROOST_F(f))) continue;
        if (!first) ROOST_PUT(",");
        first = 0;
        ROOST_PUT("\"");
        ROOST_PUT(roostFieldName(d->record, f));
        ROOST_PUT("\"");
      }
    }
    ROOST_PUT("]}");
  }
  for (size_t i = 0; i < nRaw; i++) {
    if (!raw || !raw[i] || !raw[i][0]) { out[0] = '\0'; return 0; }
    if (n || i) ROOST_PUT(",");
    ROOST_PUT(raw[i]);
  }
  ROOST_PUT("]");
#undef ROOST_PUT
  out[o] = '\0';
  return o;
}
