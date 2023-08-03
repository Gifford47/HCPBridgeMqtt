//https://github.com/technyon/nuki_hub/
#pragma once

#include <vector>
#include <Preferences.h>
#include <ArduinoJson.h>

#define preference_started_before "run"
#define preference_device_id_garade_door "deviceId"
#define preference_mqtt_broker "mqttbroker"
#define preference_mqtt_broker_port "mqttport"
#define preference_mqtt_user "mqttuser"
#define preference_mqtt_password "mqttpass"
#define preference_mqtt_log_enabled "mqttlog"

#define preference_hostname "hostname"

#define preference_query_interval_sensors "sensorsStInterval"

std::vector<char*> _keys =
{
        preference_started_before, preference_device_id_garade_door, preference_mqtt_broker, preference_mqtt_broker_port,
        preference_mqtt_user, preference_mqtt_password, preference_mqtt_log_enabled, preference_query_interval_sensors,
        preference_hostname,
};

std::vector<char*> _strings =
{
        preference_started_before, preference_device_id_garade_door, preference_mqtt_broker, preference_mqtt_broker_port,
        preference_mqtt_user, preference_mqtt_password, preference_mqtt_log_enabled, preference_query_interval_sensors,
        preference_hostname,
};

std::vector<char*> _ints =
{
        preference_mqtt_broker_port, preference_query_interval_sensors,
};

std::vector<char*> _redact =
{
    preference_mqtt_user, preference_mqtt_password,
};
std::vector<char*> _boolPrefs =
{
    preference_started_before, preference_mqtt_log_enabled, 
};

class PreferenceHandler{
 private:
    Preferences* preferences = nullptr;
    bool firstStart = true;
    const bool isKey(const char* key) const{
        return std::find(_keys.begin(), _keys.end(), key) != _keys.end();
    }
    const bool isString(const char* key) const{
        return std::find(_strings.begin(), _strings.end(), key) != _strings.end();;
    }
    const bool isInt(const char* key) const{
        return std::find(_ints.begin(), _ints.end(), key) != _ints.end();;
    }
    const bool isBool(const char* key) const{
        return std::find(_boolPrefs.begin(), _boolPrefs.end(), key) != _boolPrefs.end();;
    }
    const bool isRedacted(const char* key) const{
        return std::find(_redact.begin(), _redact.end(), key) != _redact.end();
    }
 public:
    //Should be converted to constructor.
    //has to be called during setup of main.
    void initPreferences(){
        this->preferences = new Preferences();
        this->preferences->begin("hcpbridgeesp32", false);
        this->firstStart = !preferences->getBool(preference_started_before);

        if(this->firstStart)
        {
            preferences->putBool(preference_started_before, true);
            //maybe all preferences should be defaulted here 
            //that way we could avoid some vectors to know the type of the preferences.
            //And use the gettype function and updated them with a case.
            //set preferences from config file as default value??
        }
    }
    Preferences* getPreferences(){
        return this->preferences;
    }
    bool getFirstStart(){
        return this->firstStart;
    }
    void setPreferencesJson(const JsonDocument& preferences){
        //check if If we can access to preferences array like this.
        JsonArray arr = preferences.as<JsonArray>();

        for (auto value : arr) {
            JsonObject root = value.as<JsonObject>();
            for (JsonPair kv : root) {
                if (this->isKey(kv.key().c_str())){
                    //this should be optmised to as it's quite ugly written this way.
                    if(this->isRedacted(kv.key().c_str())){
                        //redacted value coming as "*" haven't been changed and therefore didn't get updated
                        if (!(kv.value().as<const char*>() == "*")){
                            this->preferences->putString(kv.key().c_str(), kv.value().as<const char*>());
                            return;
                        }
                    }
                    if(this->isString(kv.key().c_str())){
                        this->preferences->putString(kv.key().c_str(), kv.value().as<const char*>());
                        return;
                    }
                    if(this->isInt(kv.key().c_str())){
                        //does better way exit to cast to int?
                        this->preferences->putInt(kv.key().c_str(), atoi(kv.value().as<const char*>()));
                        return;
                    }
                    if(this->isBool(kv.key().c_str())){
                        this->preferences->putBool(kv.key().c_str(), kv.value().as<const bool*>());
                        return;
                    }   
                } 
            }
        }
    }
    JsonDocument getPreferencesJson(){
 
        //see debugpreferences to see how the full preferences can been retrived.


        
    }
};




class DebugPreferences
{
private:
    const bool isRedacted(const char* key) const
    {
        return std::find(_redact.begin(), _redact.end(), key) != _redact.end();
    }
    const String redact(const String s) const
    {
        return s == "" ? "" : "***";
    }
    const String redact(const int i) const
    {
        return i == 0 ? "" : "***";
    }
    const String redact(const uint i) const
    {
        return i == 0 ? "" : "***";
    }
    const String redact(const int64_t i) const
    {
        return i == 0 ? "" : "***";
    }
    const String redact(const uint64_t i) const
    {
        return i == 0 ? "" : "***";
    }

    const void appendPreferenceInt8(Preferences *preferences, String& s, const char* description, const char* key)
    {
        s.concat(description);
        s.concat(": ");
        s.concat(isRedacted(key) ? redact(preferences->getChar(key)) : String(preferences->getChar(key)));
        s.concat("\n");
    }
    const void appendPreferenceUInt8(Preferences *preferences, String& s, const char* description, const char* key)
    {
        s.concat(description);
        s.concat(": ");
        s.concat(isRedacted(key) ? redact(preferences->getUChar(key)) : String(preferences->getUChar(key)));
        s.concat("\n");
    }
    const void appendPreferenceInt16(Preferences *preferences, String& s, const char* description, const char* key)
    {
        s.concat(description);
        s.concat(": ");
        s.concat(isRedacted(key) ? redact(preferences->getShort(key)) : String(preferences->getShort(key)));
        s.concat("\n");
    }
    const void appendPreferenceUInt16(Preferences *preferences, String& s, const char* description, const char* key)
    {
        s.concat(description);
        s.concat(": ");
        s.concat(isRedacted(key) ? redact(preferences->getUShort(key)) : String(preferences->getUShort(key)));
        s.concat("\n");
    }
    const void appendPreferenceInt32(Preferences *preferences, String& s, const char* description, const char* key)
    {
        s.concat(description);
        s.concat(": ");
        s.concat(isRedacted(key) ? redact(preferences->getInt(key)) : String(preferences->getInt(key)));
        s.concat("\n");
    }
    const void appendPreferenceUInt32(Preferences *preferences, String& s, const char* description, const char* key)
    {
        s.concat(description);
        s.concat(": ");
        s.concat(isRedacted(key) ? redact(preferences->getUInt(key)) : String(preferences->getUInt(key)));
        s.concat("\n");
    }
    const void appendPreferenceInt64(Preferences *preferences, String& s, const char* description, const char* key)
    {
        s.concat(description);
        s.concat(": ");
        s.concat(isRedacted(key) ? redact(preferences->getLong64(key)) : String(preferences->getLong64(key)));
        s.concat("\n");
    }
    const void appendPreferenceUInt64(Preferences *preferences, String& s, const char* description, const char* key)
    {
        s.concat(description);
        s.concat(": ");
        s.concat(isRedacted(key) ? redact(preferences->getULong64(key)) : String(preferences->getULong64(key)));
        s.concat("\n");
    }
    const void appendPreferenceBool(Preferences *preferences, String& s, const char* description, const char* key)
    {
        s.concat(description);
        s.concat(": ");
        s.concat(preferences->getBool(key) ? "true" : "false");
        s.concat("\n");
    }
    const void appendPreferenceString(Preferences *preferences, String& s, const char* description, const char* key)
    {
        s.concat(description);
        s.concat(": ");
        s.concat(isRedacted(key) ? redact(preferences->getString(key)) : preferences->getString(key));
        s.concat("\n");
    }

    const void appendPreference(Preferences *preferences, String& s, const char* key)
    {
        if(std::find(_boolPrefs.begin(), _boolPrefs.end(), key) != _boolPrefs.end())
        {
            appendPreferenceBool(preferences, s, key, key);
            return;
        }

        switch(preferences->getType(key))
        {
            case PT_I8:
                appendPreferenceInt8(preferences, s, key, key);
                break;
            case PT_I16:
                appendPreferenceInt16(preferences, s, key, key);
                break;
            case PT_I32:
                appendPreferenceInt32(preferences, s, key, key);
                break;
            case PT_I64:
                appendPreferenceInt64(preferences, s, key, key);
                break;
            case PT_U8:
                appendPreferenceUInt8(preferences, s, key, key);
                break;
            case PT_U16:
                appendPreferenceUInt16(preferences, s, key, key);
                break;
            case PT_U32:
                appendPreferenceUInt32(preferences, s, key, key);
                break;
            case PT_U64:
                appendPreferenceUInt64(preferences, s, key, key);
                break;
            case PT_STR:
                appendPreferenceString(preferences, s, key, key);
                break;
            default:
                appendPreferenceString(preferences, s, key, key);
                break;
        }
    }

public:
    const String preferencesToString(Preferences *preferences)
    {
        String s = "";

        for(const auto& key : _keys)
        {
            appendPreference(preferences, s, key);
        }

        return s;
    }

};