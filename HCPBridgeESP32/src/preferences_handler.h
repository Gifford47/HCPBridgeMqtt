#pragma once

#include <Preferences.h>
#include <ArduinoJson.h>
#include "configuration.h"

// ============================================================================
// Preference Key Defines (max 15 chars each) - kept for backward compatibility
// ============================================================================

// System
#define preference_started_before "run"

// RS485 pins
#define preference_rs485_txd "rs485_txd"
#define preference_rs485_rxd "rs485_rxd"

// Device
#define preference_gd_id "device_id"
#define preference_gd_name "device_name"
#define preference_hostname "hostname"

// MQTT connection
#define preference_mqtt_server "mqtt_server"
#define preference_mqtt_server_port "mqtt_port"
#define preference_mqtt_user "mqtt_user"
#define preference_mqtt_password "mqtt_pass"

// WiFi
#define preference_wifi_ap_mode "wifi_ap_enabled"
#define preference_wifi_ssid "wifi_ssid"
#define preference_wifi_password "wifi_pass"
#define preference_www_password "www_pass"
#define preference_wifi_ap_password "wifi_ap_pass"

// MQTT entity names
#define preference_gd_avail "gd_availability"
#define preference_gd_light "gd_light"
#define preference_gd_vent "gd_vent"
#define preference_gd_half "gd_half"
#define preference_gd_step "gd_step"
#define preference_gd_status "gd_status"
#define preference_gd_det_status "gd_det_status"
#define preference_gd_position "gd_position"
#define preference_gd_debug "gd_debug_string"
#define preference_gd_debug_restart "gd_dg_rst_reas"

// Sensor entity names
#define preference_gs_temp "sensor_temp"
#define preference_gs_hum "sensor_humidity"
#define preference_gs_pres "sensor_atmpress"
#define preference_gs_free_dist "sensor_freedist"
#define preference_gs_park_avail "sen_park_avail"
#define preference_gs_motion "sen_motion"
#define preference_gs_gas "sensor_gas"
#define preference_gs_gas_alarm "sen_gas_alarm"

// Sensor thresholds
#define preference_sensor_temp_treshold "sen_temp_thresh"
#define preference_sensor_hum_threshold "sen_hum_thresh"
#define preference_sensor_pres_threshold "sen_pres_thresh"
#define preference_sensor_prox_treshold "sen_prox_thresh"
#define preference_sensor_gas_threshold "sen_gas_thresh"

// Sensor pins
#define preference_sensor_i2c_sda "sen_i2c_sda"
#define preference_sensor_i2c_scl "sen_i2c_scl"
#define preference_sensor_dht_data_pin "sen_dhtdatapin"
#define preference_sensor_ds18x20_pin "sen_ds18x20pin"
#define preference_sensor_sr04_trigpin "sen_sr04trigpin"
#define preference_sensor_sr04_echopin "sen_sr04echopin"
#define preference_sensor_sr04_max_dist "sen_sr04maxdist"
#define preference_sensor_sr501 "sen_sr501pin"
#define preference_sensor_mq4_analog "sen_mq4analog"
#define preference_sensor_mq4_digital "sen_mq4digital"

// Sensor query interval
#define preference_query_interval_sensors "sen_StInterval"

// ============================================================================
// Registry-based Preference System
// ============================================================================

enum class PrefType { STRING, INT, DOUBLE, BOOL };

// Flags for save/load grouping
enum PrefGroup : uint8_t {
    PREF_GROUP_BASIC  = 0x01,   // Basic config (WebUI basic tab)
    PREF_GROUP_EXPERT = 0x02,   // Expert config (WebUI expert tab)
    PREF_GROUP_INTERNAL = 0x04, // Internal (not exposed via config API)
};

struct PrefDef {
    const char* key;
    PrefType type;
    bool redacted;        // Password fields - show as "*" in getConf
    uint8_t group;        // PrefGroup bitmask
    // Default values
    const char* defaultStr;
    int defaultInt;
    double defaultDouble;
    bool defaultBool;
};

// ============================================================================
// Central Registry - ADD/REMOVE preferences here (one line each)
// ============================================================================

static const PrefDef PREF_REGISTRY[] = {
    // === Internal ===
    // key                          type              redact  group               strDefault      intDefault      dblDefault  boolDefault
    {preference_started_before,     PrefType::BOOL,   false,  PREF_GROUP_INTERNAL, "",             0,              0.0,        false},

    // === Basic Config (WebUI basic tab) ===
    {preference_gd_id,              PrefType::STRING, false,  PREF_GROUP_BASIC,    DEVICE_ID,      0,              0.0,        false},
    {preference_gd_name,            PrefType::STRING, false,  PREF_GROUP_BASIC,    DEVICENAME,     0,              0.0,        false},
    {preference_hostname,           PrefType::STRING, false,  PREF_GROUP_BASIC,    HOSTNAME,       0,              0.0,        false},
    {preference_wifi_ap_mode,       PrefType::BOOL,   false,  PREF_GROUP_BASIC,    "",             0,              0.0,        AP_ACTIF},
    {preference_wifi_ssid,          PrefType::STRING, false,  PREF_GROUP_BASIC,    STA_SSID,       0,              0.0,        false},
    {preference_wifi_password,      PrefType::STRING, true,   PREF_GROUP_BASIC,    STA_PASSWD,     0,              0.0,        false},
    {preference_wifi_ap_password,   PrefType::STRING, true,   PREF_GROUP_BASIC,    AP_PASSWD,      0,              0.0,        false},
    {preference_www_password,       PrefType::STRING, true,   PREF_GROUP_BASIC,    WWW_PASSWD,     0,              0.0,        false},
    {preference_mqtt_server,        PrefType::STRING, false,  PREF_GROUP_BASIC,    MQTTSERVER,     0,              0.0,        false},
    {preference_mqtt_server_port,   PrefType::INT,    false,  PREF_GROUP_BASIC,    "",             MQTTPORT,       0.0,        false},
    {preference_mqtt_user,          PrefType::STRING, false,  PREF_GROUP_BASIC,    MQTTUSER,       0,              0.0,        false},
    {preference_mqtt_password,      PrefType::STRING, true,   PREF_GROUP_BASIC,    MQTTPASSWORD,   0,              0.0,        false},

    // === Expert Config (WebUI expert tab) ===
    // RS485
    {preference_rs485_txd,          PrefType::INT,    false,  PREF_GROUP_EXPERT,   "",             PIN_TXD,        0.0,        false},
    {preference_rs485_rxd,          PrefType::INT,    false,  PREF_GROUP_EXPERT,   "",             PIN_RXD,        0.0,        false},

    // MQTT entity names
    {preference_gd_avail,           PrefType::STRING, false,  PREF_GROUP_EXPERT,   GD_AVAIL,       0,              0.0,        false},
    {preference_gd_light,           PrefType::STRING, false,  PREF_GROUP_EXPERT,   GD_LIGHT,       0,              0.0,        false},
    {preference_gd_vent,            PrefType::STRING, false,  PREF_GROUP_EXPERT,   GD_VENT,        0,              0.0,        false},
    {preference_gd_half,            PrefType::STRING, false,  PREF_GROUP_EXPERT,   GD_HALF,        0,              0.0,        false},
    {preference_gd_step,            PrefType::STRING, false,  PREF_GROUP_EXPERT,   GD_STEP,        0,              0.0,        false},
    {preference_gd_status,          PrefType::STRING, false,  PREF_GROUP_EXPERT,   GD_STATUS,      0,              0.0,        false},
    {preference_gd_det_status,      PrefType::STRING, false,  PREF_GROUP_EXPERT,   GD_DET_STATUS,  0,              0.0,        false},
    {preference_gd_position,        PrefType::STRING, false,  PREF_GROUP_EXPERT,   GD_POSITIOM,    0,              0.0,        false},
    {preference_gd_debug,           PrefType::STRING, false,  PREF_GROUP_EXPERT,   GD_DEBUG,       0,              0.0,        false},
    {preference_gd_debug_restart,   PrefType::STRING, false,  PREF_GROUP_EXPERT,   GD_DEBUG_RESTART, 0,            0.0,        false},

    // Sensor entity names
    {preference_gs_temp,            PrefType::STRING, false,  PREF_GROUP_EXPERT,   GS_TEMP,        0,              0.0,        false},
    {preference_gs_hum,             PrefType::STRING, false,  PREF_GROUP_EXPERT,   GS_HUM,         0,              0.0,        false},
    {preference_gs_pres,            PrefType::STRING, false,  PREF_GROUP_EXPERT,   GS_PRES,        0,              0.0,        false},
    {preference_gs_free_dist,       PrefType::STRING, false,  PREF_GROUP_EXPERT,   GS_FREE_DIST,   0,              0.0,        false},
    {preference_gs_park_avail,      PrefType::STRING, false,  PREF_GROUP_EXPERT,   GS_PARK_AVAIL,  0,              0.0,        false},
    {preference_gs_motion,          PrefType::STRING, false,  PREF_GROUP_EXPERT,   GS_MOTION,      0,              0.0,        false},
    {preference_gs_gas,             PrefType::STRING, false,  PREF_GROUP_EXPERT,   GS_GAS,         0,              0.0,        false},
    {preference_gs_gas_alarm,       PrefType::STRING, false,  PREF_GROUP_EXPERT,   GS_GAS_ALARM,   0,              0.0,        false},

    // Sensor thresholds
    {preference_sensor_temp_treshold, PrefType::DOUBLE, false, PREF_GROUP_EXPERT,  "",             0,              temp_threshold, false},
    {preference_sensor_hum_threshold, PrefType::INT,    false, PREF_GROUP_EXPERT,  "",             hum_threshold,  0.0,        false},
    {preference_sensor_pres_threshold,PrefType::INT,    false, PREF_GROUP_EXPERT,  "",             pres_threshold, 0.0,        false},
    {preference_sensor_prox_treshold, PrefType::INT,    false, PREF_GROUP_EXPERT,  "",             prox_treshold,  0.0,        false},
    {preference_sensor_gas_threshold, PrefType::INT,    false, PREF_GROUP_EXPERT,  "",             gas_threshold,  0.0,        false},

    // Sensor pins
    {preference_sensor_i2c_sda,     PrefType::INT,    false,  PREF_GROUP_EXPERT,   "",             I2C_SDA,        0.0,        false},
    {preference_sensor_i2c_scl,     PrefType::INT,    false,  PREF_GROUP_EXPERT,   "",             I2C_SCL,        0.0,        false},
    {preference_sensor_dht_data_pin,PrefType::INT,    false,  PREF_GROUP_EXPERT,   "",             DHTPIN,         0.0,        false},
    {preference_sensor_ds18x20_pin, PrefType::INT,    false,  PREF_GROUP_EXPERT,   "",             oneWireBus,     0.0,        false},
    {preference_sensor_sr04_trigpin,PrefType::INT,    false,  PREF_GROUP_EXPERT,   "",             SR04_TRIGPIN,   0.0,        false},
    {preference_sensor_sr04_echopin,PrefType::INT,    false,  PREF_GROUP_EXPERT,   "",             SR04_ECHOPIN,   0.0,        false},
    {preference_sensor_sr04_max_dist,PrefType::INT,   false,  PREF_GROUP_EXPERT,   "",             SR04_MAXDISTANCECM, 0.0,    false},
    {preference_sensor_sr501,       PrefType::INT,    false,  PREF_GROUP_EXPERT,   "",             SR501PIN,       0.0,        false},
    {preference_sensor_mq4_analog,  PrefType::INT,    false,  PREF_GROUP_EXPERT,   "",             MQ4_ANALOG_PIN, 0.0,        false},
    {preference_sensor_mq4_digital, PrefType::INT,    false,  PREF_GROUP_EXPERT,   "",             MQ4_DIGITAL_PIN,0.0,        false},

    // Query interval
    {preference_query_interval_sensors, PrefType::INT, false, PREF_GROUP_EXPERT,   "",             SENSE_PERIOD,   0.0,        false},
};

static const size_t PREF_REGISTRY_SIZE = sizeof(PREF_REGISTRY) / sizeof(PREF_REGISTRY[0]);

// ============================================================================
// Preferences Cache (for performance-critical MQTT fields)
// ============================================================================

class Preferences_cache {
public:
    char mqtt_server[64];
    char mqtt_user[64];
    char mqtt_password[64];
    char hostname[64];
};

// ============================================================================
// PreferenceHandler Class
// ============================================================================

class PreferenceHandler {
private:
    Preferences* preferences = nullptr;
    bool firstStart = true;
    Preferences_cache* preferencesCache = nullptr;

    // Find a PrefDef by key (returns nullptr if not found)
    const PrefDef* findDef(const char* key) const;

    // Set default value for a single preference
    void setDefault(const PrefDef& def);

    // Load a single preference into a JsonDocument
    void loadToJson(const PrefDef& def, JsonDocument& doc) const;

    // Save a single preference from a JsonDocument
    void saveFromJson(const PrefDef& def, const JsonDocument& doc);

public:
    void initPreferences();
    Preferences* getPreferences();
    Preferences_cache* getPreferencesCache();
    bool getFirstStart();
    void resetPreferences();
    void saveConf(JsonDocument& doc);
    void getConf(JsonDocument& conf);
};
