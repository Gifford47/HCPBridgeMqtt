#include "preferences_handler.h"
#include <Arduino.h>

// ============================================================================
// Private helpers
// ============================================================================

const PrefDef* PreferenceHandler::findDef(const char* key) const {
    for (size_t i = 0; i < PREF_REGISTRY_SIZE; i++) {
        if (strcmp(PREF_REGISTRY[i].key, key) == 0) {
            return &PREF_REGISTRY[i];
        }
    }
    return nullptr;
}

void PreferenceHandler::setDefault(const PrefDef& def) {
    switch (def.type) {
        case PrefType::STRING:
            preferences->putString(def.key, def.defaultStr);
            break;
        case PrefType::INT:
            preferences->putInt(def.key, def.defaultInt);
            break;
        case PrefType::DOUBLE:
            preferences->putDouble(def.key, def.defaultDouble);
            break;
        case PrefType::BOOL:
            preferences->putBool(def.key, def.defaultBool);
            break;
    }
}

void PreferenceHandler::loadToJson(const PrefDef& def, JsonDocument& doc) const {
    if (def.redacted) {
        switch (def.type) {
            case PrefType::STRING:
                doc[def.key] = preferences->getString(def.key).length() != 0 ? "*" : "";
                break;
            default:
                doc[def.key] = "*";
                break;
        }
        return;
    }

    switch (def.type) {
        case PrefType::STRING: {
            String val = preferences->getString(def.key);
            doc[def.key] = val;
            break;
        }
        case PrefType::INT:
            doc[def.key] = preferences->getInt(def.key, def.defaultInt);
            break;
        case PrefType::DOUBLE:
            doc[def.key] = preferences->getDouble(def.key, def.defaultDouble);
            break;
        case PrefType::BOOL:
            doc[def.key] = preferences->getBool(def.key, def.defaultBool);
            break;
    }
}

void PreferenceHandler::saveFromJson(const PrefDef& def, const JsonDocument& doc) {
    if (doc[def.key].isNull()) return;

    if (def.redacted) {
        String val = doc[def.key].as<String>();
        if (val == "*") return;
        preferences->putString(def.key, val);
        return;
    }

    switch (def.type) {
        case PrefType::STRING:
            preferences->putString(def.key, doc[def.key].as<String>());
            break;
        case PrefType::INT:
            preferences->putInt(def.key, doc[def.key].as<int>());
            break;
        case PrefType::DOUBLE:
            preferences->putDouble(def.key, doc[def.key].as<double>());
            break;
        case PrefType::BOOL:
            preferences->putBool(def.key, doc[def.key].as<bool>());
            break;
    }
}

// ============================================================================
// Public methods
// ============================================================================

void PreferenceHandler::initPreferences() {
    this->preferences = new Preferences();
    this->preferences->begin("hcpbridgeesp32", false);
    this->firstStart = !preferences->getBool(preference_started_before);

    preferences->putString(preference_wifi_ap_password, AP_PASSWD);

    if (this->firstStart) {
        preferences->putBool(preference_started_before, true);
        for (size_t i = 0; i < PREF_REGISTRY_SIZE; i++) {
            if (strcmp(PREF_REGISTRY[i].key, preference_started_before) == 0) continue;
            setDefault(PREF_REGISTRY[i]);
        }
    }

    this->preferencesCache = new Preferences_cache();
    strcpy(this->preferencesCache->mqtt_server, preferences->getString(preference_mqtt_server).c_str());
    strcpy(this->preferencesCache->mqtt_user, preferences->getString(preference_mqtt_user).c_str());
    strcpy(this->preferencesCache->mqtt_password, preferences->getString(preference_mqtt_password).c_str());
    strcpy(this->preferencesCache->hostname, preferences->getString(preference_hostname).c_str());
}

Preferences* PreferenceHandler::getPreferences() {
    return this->preferences;
}

Preferences_cache* PreferenceHandler::getPreferencesCache() {
    return this->preferencesCache;
}

bool PreferenceHandler::getFirstStart() {
    return this->firstStart;
}

void PreferenceHandler::resetPreferences() {
    preferences->clear();
    ESP.restart();
}

void PreferenceHandler::saveConf(JsonDocument& doc) {
    // Detect which form was submitted by presence of sentinel keys
    bool saveBasic  = !doc[preference_gd_id].isNull();
    bool saveSensor = !doc[preference_query_interval_sensors].isNull();
    bool saveExpert = !doc[preference_rs485_txd].isNull();

    // Special handling for checkboxes (come as "on" when checked, absent when unchecked)
    if (saveBasic) {
        preferences->putBool(preference_wifi_ap_mode, !doc[preference_wifi_ap_mode].isNull() && doc[preference_wifi_ap_mode].as<String>() == "on");
        preferences->putBool(preference_debug_enabled, !doc[preference_debug_enabled].isNull() && doc[preference_debug_enabled].as<String>() == "on");
    }

    if (saveSensor) {
        preferences->putBool(preference_sensor_bme_enabled, !doc[preference_sensor_bme_enabled].isNull() && doc[preference_sensor_bme_enabled].as<String>() == "on");
        preferences->putBool(preference_sensor_ds18x20_enabled, !doc[preference_sensor_ds18x20_enabled].isNull() && doc[preference_sensor_ds18x20_enabled].as<String>() == "on");
        preferences->putBool(preference_sensor_dht22_enabled, !doc[preference_sensor_dht22_enabled].isNull() && doc[preference_sensor_dht22_enabled].as<String>() == "on");
        preferences->putBool(preference_sensor_hcsr04_enabled, !doc[preference_sensor_hcsr04_enabled].isNull() && doc[preference_sensor_hcsr04_enabled].as<String>() == "on");
        preferences->putBool(preference_sensor_hcsr501_enabled, !doc[preference_sensor_hcsr501_enabled].isNull() && doc[preference_sensor_hcsr501_enabled].as<String>() == "on");
        preferences->putBool(preference_sensor_mq4_enabled, !doc[preference_sensor_mq4_enabled].isNull() && doc[preference_sensor_mq4_enabled].as<String>() == "on");
    }

    for (size_t i = 0; i < PREF_REGISTRY_SIZE; i++) {
        const PrefDef& def = PREF_REGISTRY[i];

        if (def.group & PREF_GROUP_INTERNAL) continue;

        // Skip all checkboxes - handled specially above
        if (def.type == PrefType::BOOL) continue;

        if ((def.group & PREF_GROUP_BASIC) && saveBasic) {
            saveFromJson(def, doc);
        } else if ((def.group & PREF_GROUP_SENSOR) && saveSensor) {
            saveFromJson(def, doc);
        } else if ((def.group & PREF_GROUP_EXPERT) && saveExpert) {
            saveFromJson(def, doc);
        }
    }

    ESP.restart();
}

void PreferenceHandler::getConf(JsonDocument& conf) {
    for (size_t i = 0; i < PREF_REGISTRY_SIZE; i++) {
        const PrefDef& def = PREF_REGISTRY[i];
        if (def.group & PREF_GROUP_INTERNAL) continue;
        loadToJson(def, conf);
    }
}
