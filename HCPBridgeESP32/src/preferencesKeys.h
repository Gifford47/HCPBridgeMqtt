//https://github.com/technyon/nuki_hub/
#pragma once

#include <vector>
#include <Preferences.h>
#include <ArduinoJson.h>

#include "configuration.h"
//max lenght of key is 15char.
#define preference_started_before "run"
//rs485 pins
#define preference_rs485_txd "rs485_txd"
#define preference_rs485_rxd "rs485_rxd"
#define preference_gd_id "device_id"
#define preference_gd_name "device_name"
#define preference_mqtt_server "mqtt_server"
#define preference_mqtt_server_port "mqtt_port"
#define preference_mqtt_user "mqtt_user"
#define preference_mqtt_password "mqtt_pass"

#define preference_wifi_ap_mode "wifi_ap_enabled"
#define preference_wifi_ssid "wifi_ssid"
#define preference_wifi_password "wifi_pass"
#define preference_hostname "hostname"

#define preference_gd_avail "gd_availability"
#define preference_gd_light "gd_light"
#define preference_gd_vent "gd_vent"
#define preference_gd_status "gd_status"
#define preference_gd_det_status "gd_det_status"
#define preference_gd_position "gd_position"
#define preference_gd_debug "gd_debug_string"
#define preference_gd_debug_restart "gd_dg_rst_reas"
#define preference_gs_temp "sensor_temp"
#define preference_gs_hum "sensor_humidity"
#define preference_gs_pres "sensor_atmpreas"
#define preference_gs_free_dist "sensor_freedist"
#define preference_gs_park_avail "sen_park_avail"

#define preference_sensor_sr04_trigpin "sen_sr04trigpin"
#define preference_sensor_sr04_echopin "sen_sr04echopin"




#define preference_query_interval_sensors "sensorsStInterval"

std::vector<const char*> _keys =
{
        preference_started_before, preference_wifi_ap_mode, preference_wifi_ssid, preference_wifi_password, preference_gd_id, 
        preference_gd_name, preference_mqtt_server, preference_mqtt_server_port,
        preference_mqtt_user, preference_mqtt_password, preference_query_interval_sensors, preference_hostname,
        preference_gd_avail, preference_gd_light, preference_gd_vent, preference_gd_status, preference_gd_det_status,
        preference_gd_position,preference_gd_debug, preference_gd_debug_restart, preference_gs_temp, preference_gs_hum,
        preference_gs_pres, preference_gs_free_dist, preference_gs_park_avail, preference_sensor_sr04_trigpin, preference_sensor_sr04_echopin,
        preference_rs485_txd, preference_rs485_rxd
};

std::vector<const char*> _strings =
{
        preference_started_before, preference_wifi_ap_mode, preference_wifi_ssid, preference_wifi_password,
        preference_gd_id, preference_gd_name, preference_mqtt_server,
        preference_mqtt_user, preference_mqtt_password, preference_query_interval_sensors, preference_hostname, 
        preference_gd_avail, preference_gd_light, preference_gd_vent, preference_gd_status, preference_gd_det_status,
        preference_gd_position,preference_gd_debug, preference_gd_debug_restart, preference_gs_temp, preference_gs_hum,
        preference_gs_pres, preference_gs_free_dist, preference_gs_park_avail,
};

std::vector<const char*> _ints =
{
        preference_mqtt_server_port, preference_query_interval_sensors, preference_sensor_sr04_trigpin, preference_sensor_sr04_echopin,
        preference_rs485_txd, preference_rs485_rxd
};

std::vector<const char*> _redact =
{
    preference_wifi_password, preference_mqtt_user, preference_mqtt_password,
};
std::vector<const char*> _boolPrefs =
{
    preference_started_before, 
};

class Preferences_cache {    
  public:         
    char mqtt_server[64];
    char mqtt_user[64];
    char mqtt_password[64];
    char hostname[64];
};

class PreferenceHandler{
 private:
    Preferences* preferences = nullptr;
    bool firstStart = true;
    Preferences_cache* PreferencesCache = nullptr;
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

            preferences->putInt(preference_rs485_txd, PIN_TXD);
            preferences->putInt(preference_rs485_rxd, PIN_RXD);

            preferences->putString(preference_gd_id, DEVICE_ID);
            preferences->putString(preference_gd_name, DEVICENAME);
            preferences->putString(preference_hostname, HOSTNAME);
            preferences->putBool(preference_wifi_ap_mode, AP_ACTIF);
            preferences->putString(preference_wifi_ssid, STA_SSID);
            preferences->putString(preference_wifi_password, STA_PASSWD);
            preferences->putString(preference_mqtt_password, MQTTPASSWORD);
            preferences->putString(preference_mqtt_server, MQTTSERVER);
            preferences->putString(preference_mqtt_user, MQTTUSER);
            preferences->putInt(preference_mqtt_server_port, MQTTPORT);

            preferences->putString(preference_gd_avail, GD_AVAIL);
            preferences->putString(preference_gd_light, GD_LIGHT);
            preferences->putString(preference_gd_vent, GD_VENT);
            preferences->putString(preference_gd_status, GD_STATUS);
            preferences->putString(preference_gd_det_status, GD_DET_STATUS);
            preferences->putString(preference_gd_position, GD_POSITIOM);
            preferences->putString(preference_gd_debug, GD_DEBUG);
            preferences->putString(preference_gd_debug_restart, GD_DEBUG_RESTART);
            preferences->putString(preference_gs_temp, GS_TEMP);
            preferences->putString(preference_gs_hum, GS_HUM);
            preferences->putString(preference_gs_pres, GS_PRES);
            preferences->putString(preference_gs_free_dist, GS_FREE_DIST);
            preferences->putString(preference_gs_park_avail, GS_PARK_AVAIL);

            preferences->putInt(preference_sensor_sr04_trigpin, SR04_TRIGPIN);
            preferences->putInt(preference_sensor_sr04_echopin, SR04_ECHOPIN);
            //that way we could avoid some vectors to know the type of the preferences.
            //And use the gettype function and updated them with a case.
        }
        //setup preferences cache.
            this->PreferencesCache = new Preferences_cache();
            strcpy(this->PreferencesCache->mqtt_server, preferences->getString(preference_mqtt_server).c_str());
            strcpy(this->PreferencesCache->mqtt_user, preferences->getString(preference_mqtt_user).c_str());
            strcpy(this->PreferencesCache->mqtt_password, preferences->getString(preference_mqtt_password).c_str());
            strcpy(this->PreferencesCache->hostname, preferences->getString(preference_hostname).c_str());
    }
    Preferences* getPreferences(){
        return this->preferences;
    }
    Preferences_cache* getPreferencesCache(){
        return this->PreferencesCache;  
    }
    bool getFirstStart(){
        return this->firstStart;
    }
    /*
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
                        this->preferences->putBool(kv.key().c_str(), kv.value().as<bool>());
                        return;
                    }   
                } 
            }
        }
    }
    JsonDocument getPreferencesJson(){
        //see debugpreferences to see how the full preferences can been retrived.      
    }
    */
    // handle Preferences
    void saveConf(StaticJsonDocument<256> doc) {
        String apactif = doc[preference_wifi_ap_mode].as<String>();
        String ssid = doc[preference_wifi_ssid].as<String>();
        String pass = doc[preference_wifi_password].as<String>();
        String mqtt_server = doc[preference_mqtt_server].as<String>();
        int mqtt_port = doc[preference_mqtt_server_port].as<int>();
        String mqtt_user = doc[preference_mqtt_user].as<String>();
        String mqtt_pass = doc[preference_mqtt_password].as<String>();
        int rs485_rxd = doc[preference_rs485_rxd].as<int>();
        int rs485_txd = doc[preference_rs485_txd].as<int>();
        
        if(pass != "*"){
            //* stands for password not changed
            this->preferences->putString(preference_wifi_password, pass);
        }

        if (mqtt_pass != "*"){
            this->preferences->putString(preference_mqtt_password, mqtt_pass);
        }
        // Save Settings in Prefs
        if (apactif == "on"){
           this->preferences->putBool(preference_wifi_ap_mode, true); 
        } else{
            this->preferences->putBool(preference_wifi_ap_mode, false); 
        }
        this->preferences->putString(preference_wifi_ssid, ssid);
        this->preferences->putString(preference_mqtt_server, mqtt_server);
        this->preferences->putInt(preference_mqtt_server_port, mqtt_port);
        this->preferences->putString(preference_mqtt_user, mqtt_user);
        
        this->preferences->putInt(preference_rs485_rxd, rs485_rxd);
        this->preferences->putInt(preference_rs485_txd, rs485_txd);

        ESP.restart();
    }
    void getConf(JsonDocument&  conf){

        char mqtt_server[64];
        char mqtt_user[64];
        char wifi_ssid[64];

        strcpy(mqtt_server, preferences->getString(preference_mqtt_server).c_str());
        strcpy(mqtt_user, preferences->getString(preference_mqtt_user).c_str());
        strcpy(wifi_ssid, preferences->getString(preference_wifi_ssid).c_str());

        conf[preference_wifi_ap_mode] = this->preferences->getBool(preference_wifi_ap_mode);
        conf[preference_wifi_ssid] = wifi_ssid;
        conf[preference_mqtt_server] = mqtt_user;
        conf[preference_mqtt_user] = mqtt_server;
        conf[preference_mqtt_server_port] = this->preferences->getInt(preference_mqtt_server_port);
        conf[preference_rs485_rxd] = this->preferences->getInt(preference_rs485_rxd);
        conf[preference_rs485_txd] = this->preferences->getInt(preference_rs485_txd);
        if (this->preferences->getString(preference_wifi_password).length() != 0){
            //if preferences have been set then return *
            conf[preference_wifi_password] = "*";
        }else{
            conf[preference_wifi_password] = "";
        }
        if (this->preferences->getString(preference_mqtt_password).length() != 0){
            //if preferences have been set then return *
            conf[preference_mqtt_password] = "*";
        }else{
            conf[preference_mqtt_password] = "";
        }
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