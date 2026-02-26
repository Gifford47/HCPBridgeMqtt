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
    // Redacted fields show "*" if non-empty, "" if empty
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
            // Need to copy to buffer to avoid dangling pointer
            String val = preferences->getString(def.key);
            doc[def.key] = val;
            break;
        }
        case PrefType::INT:
            doc[def.key] = preferences->getInt(def.key);
            break;
        case PrefType::DOUBLE:
            doc[def.key] = preferences->getDouble(def.key);
            break;
        case PrefType::BOOL:
            doc[def.key] = preferences->getBool(def.key);
            break;
    }
}

void PreferenceHandler::saveFromJson(const PrefDef& def, const JsonDocument& doc) {
    // Skip if key not present in JSON
    if (doc[def.key].isNull()) return;

    // Redacted fields: "*" means "unchanged", skip saving
    if (def.redacted) {
        String val = doc[def.key].as<String>();
        if (val == "*") return;
        // For redacted fields, only STRING type is used
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

    // Always overwrite AP password with compiled default
    preferences->putString(preference_wifi_ap_password, AP_PASSWD);

    if (this->firstStart) {
        // Mark as started
        preferences->putBool(preference_started_before, true);

        // Set all defaults from registry
        for (size_t i = 0; i < PREF_REGISTRY_SIZE; i++) {
            // Skip the "started_before" flag itself
            if (strcmp(PREF_REGISTRY[i].key, preference_started_before) == 0) continue;
            setDefault(PREF_REGISTRY[i]);
        }
    }

    // Setup preferences cache for performance-critical fields
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
    // Determine which groups to save based on what's present in the JSON
    // Basic config is identified by presence of preference_gd_id
    // Expert config is identified by presence of preference_gd_avail
    String gd_id = doc[preference_gd_id].as<String>();
    String gd_avail = doc[preference_gd_avail].as<String>();

    bool saveBasic = (gd_id != "null");
    bool saveExpert = (gd_avail != "null");

    // Special handling for wifi_ap_mode (comes as "on"/"off" string from WebUI)
    if (saveBasic && !doc[preference_wifi_ap_mode].isNull()) {
        String apactif = doc[preference_wifi_ap_mode].as<String>();
        preferences->putBool(preference_wifi_ap_mode, apactif == "on");
    }

    for (size_t i = 0; i < PREF_REGISTRY_SIZE; i++) {
        const PrefDef& def = PREF_REGISTRY[i];

        // Skip internal preferences
        if (def.group & PREF_GROUP_INTERNAL) continue;

        // Skip wifi_ap_mode - handled specially above
        if (strcmp(def.key, preference_wifi_ap_mode) == 0) continue;

        // Only save if the group matches
        if ((def.group & PREF_GROUP_BASIC) && saveBasic) {
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

        // Skip internal preferences (like "started_before")
        if (def.group & PREF_GROUP_INTERNAL) continue;

        loadToJson(def, conf);
    }
}
