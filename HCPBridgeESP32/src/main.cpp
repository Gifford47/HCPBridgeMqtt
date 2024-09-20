#include <Arduino.h>
#include <Esp.h>
#include <ElegantOTA.h>
#include <ESPAsyncWebServer.h>
#include "AsyncJson.h"
#include <AsyncMqttClient.h>
extern "C" {
	#include "freertos/FreeRTOS.h"
	#include "freertos/timers.h"
}
#include "ArduinoJson.h"

#include "hoermann.h"
#include "preferencesKeys.h"
#include "../WebUI/index_html.h"

#ifdef USE_DS18X20
  #include <OneWire.h>
  #include <DallasTemperature.h>
#endif

#ifdef USE_BME
  #include <Wire.h>
  #include <Adafruit_Sensor.h>
  #include <Adafruit_BME280.h>
#endif

#ifdef USE_DHT22
  #include <Adafruit_Sensor.h>
  #include <DHT.h>
#endif

// webserver on port 80
AsyncWebServer server(80);

#ifdef SENSORS
  int     sensor_prox_tresh   = 0;
  float  sensor_temp_thresh  = 0;
  int     sensor_hum_thresh   = 0;
  int     sensor_pres_thresh  = 0;
  int     sensor_last_update  = 0;
  int     sensor_force_update_intervall = 7200000;    // force sending sensor updates after amount of time since last update
#endif


#ifdef USE_DS18X20
  // Setup a oneWire instance to communicate with any OneWire devices
  DallasTemperature *ds18x20 = nullptr;
  float ds18x20_temp = -99.99;
  float ds18x20_last_temp = -99.99;
  int ds18x20_pin = 0;
#endif
#ifdef USE_BME
  TwoWire I2CBME = TwoWire(0);
  Adafruit_BME280 bme;
  unsigned bme_status;
  float bme_temp = -99.99;
  float bme_last_temp = -99.99;
  float bme_hum = -99.99;
  float bme_last_hum = -99.99;
  float bme_pres = -99.99;
  float bme_last_pres = -99.99;
  int i2c_onoffpin = 0;
  int i2c_sdapin = 0;
  int i2c_sclpin = 0;
#endif

#ifdef USE_HCSR04
  long hcsr04_duration = -99.99;
  int hcsr04_distanceCm = 0;
  int hcsr04_lastdistanceCm = 0;
  int hcsr04_maxdistanceCm = 150;
  bool hcsr04_park_available = false;
  bool hcsr04_lastpark_available = false;
  int hcsr04_tgpin = 0;
  int hscr04_ecpin = 0;
#endif

#ifdef USE_DHT22
  DHT *dht = nullptr;
  float dht22_temp = -99.99;
  float dht22_last_temp = -99.99;
  float dht22_hum = -99.99;
  float dht22_last_hum = -99.99;
  int dht_data_pin = 0;
#endif

#ifdef USE_HCSR501
  int hcsr501stat = 0;
  bool hcsr501_laststat = false;
#endif

// sensors
bool new_sensor_data = false;

// mqtt
volatile bool mqttConnected;
AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

char lastCommandTopic[64];
char lastCommandPayload[64];

PreferenceHandler prefHandler;
Preferences *localPrefs = nullptr;

class MqttStrings {    
  public:         
    char availability_topic [64];
    char state_topic [64];
    char cmd_topic [64];
    char pos_topic [64];
    char setpos_topic [64];
    char lamp_topic [64];
    char door_topic [64];
    char vent_topic [64];
    char sensor_topic [64];
    char debug_topic [64];
    String st_availability_topic;
    String st_state_topic;
    String st_cmd_topic;
    String st_cmd_topic_var;
    String st_cmd_topic_subs;
    String st_pos_topic;
    String st_setpos_topic;
    String st_lamp_topic;
    String st_door_topic;
    String st_vent_topic;
    String st_sensor_topic;
    String st_debug_topic;   
};
MqttStrings mqttStrings;

unsigned long resetButtonTimePressed = 0l;
TimerHandle_t resetTimer;

#ifdef DEBUG
  bool boot_Flag = true;
#endif

const char *ToHA(bool value)
{
  if (value == true)
  {
    return HA_ON;
  }
  if (value == false)
  {
    return HA_OFF;
  }
  return "UNKNOWN";
}

void setuptMqttStrings(){
  String ftopic = "hormann/" + localPrefs->getString(preference_gd_id);
  mqttStrings.st_availability_topic = ftopic + "/availability";
  mqttStrings.st_state_topic = ftopic + "/state";
  mqttStrings.st_cmd_topic = ftopic + "/command";
  mqttStrings.st_cmd_topic_var = mqttStrings.st_cmd_topic + "/%s";
  mqttStrings.st_cmd_topic_subs = mqttStrings.st_cmd_topic + "/#";
  mqttStrings.st_pos_topic = ftopic + "/position";
  mqttStrings.st_setpos_topic = mqttStrings.st_cmd_topic  + "/set_position";
  mqttStrings.st_lamp_topic = mqttStrings.st_cmd_topic  + "/lamp";
  mqttStrings.st_door_topic = mqttStrings.st_cmd_topic  + "/door";
  mqttStrings.st_vent_topic = mqttStrings.st_cmd_topic  + "/vent";
  mqttStrings.st_sensor_topic = ftopic + "/sensor";
  mqttStrings.st_debug_topic = ftopic + "/debug";

  strcpy(mqttStrings.availability_topic, mqttStrings.st_availability_topic.c_str());
  strcpy(mqttStrings.state_topic, mqttStrings.st_state_topic.c_str());
  strcpy(mqttStrings.cmd_topic, mqttStrings.st_cmd_topic.c_str());
  strcpy(mqttStrings.pos_topic, mqttStrings.st_pos_topic.c_str()); 
  strcpy(mqttStrings.setpos_topic, mqttStrings.st_setpos_topic.c_str());
  strcpy(mqttStrings.lamp_topic, mqttStrings.st_lamp_topic.c_str());
  strcpy(mqttStrings.door_topic, mqttStrings.st_door_topic.c_str());
  strcpy(mqttStrings.vent_topic, mqttStrings.st_vent_topic.c_str());
  strcpy(mqttStrings.sensor_topic, mqttStrings.st_sensor_topic.c_str());
  strcpy(mqttStrings.debug_topic, mqttStrings.st_debug_topic.c_str());
}
void IRAM_ATTR reset_button_change(){
  if (digitalRead(0) == 0)
  {
    // Pressed
    resetButtonTimePressed = millis();
  } else {
    // unpressed
    unsigned long timeNow = millis();
    int timeInSecs = (timeNow - resetButtonTimePressed) / 1000;
    if (timeInSecs > 5)
    {
      xTimerStart(resetTimer, 0);
    }
    resetButtonTimePressed = 0;
  }
}

void resetPreferences()
{
  xTimerStop(resetTimer, 0);
  Serial.println("Resetting config...");
  prefHandler.resetPreferences();
}

void switchLamp(bool on){
  hoermannEngine->turnLight(on);
}

void connectToWifi() {
  /*if (localPrefs->getBool(preference_wifi_ap_mode))
  {
    Serial.println("WIFI AP mode enabled, set Hostname");
    WiFi.softAP(prefHandler.getPreferencesCache()->hostname);
    return;
  }*/
  if (localPrefs->getString(preference_wifi_ssid) != "")
  {
    Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(localPrefs->getString(preference_wifi_ssid).c_str(), localPrefs->getString(preference_wifi_password).c_str());
  } else
  {
    Serial.println("No WiFi Client enabled");
  }

  //Serial.println("Connecting to Wi-Fi...");
  //this disocnnect should not be necessary as we restart the esp after changing form AP mode to Station mode.
  //WiFi.softAPdisconnect(true);  //stop AP, we now work as a wifi client

}
void connectToMqtt()
{
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  mqttConnected = false;
  #ifdef DEBUG
  switch (reason) {
    case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED: 
      Serial.println("Disconnected from MQTT. reason : TCP_DISCONNECTED");
      break;
    case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION: 
      Serial.println("Disconnected from MQTT. reason : MQTT_UNACCEPTABLE_PROTOCOL_VERSION");
      break;
    case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED: 
      Serial.println("Disconnected from MQTT. reason : MQTT_IDENTIFIER_REJECTED");
      break;
    case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE: 
      Serial.println("Disconnected from MQTT. reason : MQTT_SERVER_UNAVAILABLE");
      break;
    case AsyncMqttClientDisconnectReason::ESP8266_NOT_ENOUGH_SPACE: 
      Serial.println("Disconnected from MQTT. reason : ESP8266_NOT_ENOUGH_SPACE");
      break;
    case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS: 
      Serial.println("Disconnected from MQTT. reason : MQTT_MALFORMED_CREDENTIALS");
      break;
    case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED: 
      Serial.println("Disconnected from MQTT. reason : MQTT_NOT_AUTHORIZED");
      break;
    case AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT: 
      Serial.println("Disconnected from MQTT. reason :TLS_BAD_FINGERPRINT");
      break;
    default: break;
  } 
  #endif


  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttPublish(uint16_t packetId){
}

void updateDoorStatus(bool forceUpate = false)
{
  // onyl send updates when state changed
  if (hoermannEngine->state->changed || forceUpate){
    hoermannEngine->state->clearChanged();
    JsonDocument doc;
    char payload[1024];
    const char *venting = HA_CLOSE;

    doc["valid"] = hoermannEngine->state->valid;
    doc["doorposition"] = (int)(hoermannEngine->state->currentPosition * 100);
    doc["lamp"] = ToHA(hoermannEngine->state->lightOn);
    doc["doorstate"] = hoermannEngine->state->coverState;
    doc["detailedState"] = hoermannEngine->state->translatedState;
    if (hoermannEngine->state->translatedState == HA_VENT){
      venting = HA_VENT;
    }
    doc["vent"] = venting;

    serializeJson(doc, payload);
    mqttClient.publish(mqttStrings.state_topic, 1, true, payload);

    sprintf(payload, "%d", (int)(hoermannEngine->state->currentPosition * 100));
    mqttClient.publish(mqttStrings.pos_topic, 1, true, payload);
  }
}

void updateSensors(bool forceUpate = false){
  #ifdef SENSORS
    if (millis()-sensor_last_update >= sensor_force_update_intervall) {
      forceUpate = true;
    }
    
    if (new_sensor_data || forceUpate) {
      new_sensor_data = false;
      JsonDocument doc;    //2048 needed because of BME280 float values!
      char payload[1024];
      char buf[20];
      #ifdef USE_DS18X20
        dtostrf(ds18x20_temp,2,1,buf);    // convert to string
        //Serial.println("Temp: "+ (String)buf);
        doc["temp"] = buf;
      #endif
      #ifdef USE_BME
        dtostrf(bme_temp,2,1,buf);    // convert to string
        //Serial.println("Temp: "+ (String)buf);
        doc["temp"] = buf;
        dtostrf(bme_hum,2,1,buf);    // convert to string
        //Serial.println("Hum: "+ (String)buf);
        doc["hum"] = buf;
        dtostrf(bme_pres,2,1,buf);    // convert to string
        //Serial.println("Pres: "+ (String)buf);
        doc["pres"] = buf;
      #endif
      #ifdef USE_HCSR04
        sprintf(buf, "%d", hcsr04_distanceCm);
        doc["dist"] = buf;
        doc["free"] = ToHA(hcsr04_park_available);
      #endif
      #ifdef USE_DHT22
        dtostrf(dht22_temp,2,2,buf);
        doc["temp"] = buf;
        dtostrf(dht22_hum,2,2,buf);
        doc["hum"] = buf;
      #endif
      serializeJson(doc, payload);
      mqttClient.publish(mqttStrings.sensor_topic, 0, false, payload);  //uint16_t publish(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr, size_t length = 0)
      sensor_last_update = millis();
    }
  #endif
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
  // Note that payload is NOT a string; it contains raw data.
  // https://github.com/marvinroger/async-mqtt-client/blob/develop/docs/5.-Troubleshooting.md
  strcpy(lastCommandTopic, topic);
  strncpy(lastCommandPayload, payload, len);
  lastCommandPayload[len] = '\0';

  if (strcmp(topic, mqttStrings.lamp_topic) == 0){
    if (strncmp(payload, HA_ON, len) == 0){
      switchLamp(true);
    }
    else if (strncmp(payload, HA_OFF, len) == 0){
      switchLamp(false);
    }
    else{
      hoermannEngine->toogleLight();
    }
  }
  else if (strcmp(mqttStrings.door_topic, topic) == 0 || strcmp(mqttStrings.vent_topic, topic) == 0){
    if (strncmp(payload, HA_OPEN, len) == 0){
      hoermannEngine->openDoor();
    }
    else if (strncmp(payload, HA_CLOSE, len) == 0){
      hoermannEngine->closeDoor();
    }
    else if (strncmp(payload, HA_STOP, len) == 0){
      hoermannEngine->stopDoor();
    }
    else if (strncmp(payload, HA_HALF, len) == 0){
      hoermannEngine->halfPositionDoor();
    }
    else if (strncmp(payload, HA_VENT, len) == 0){
      hoermannEngine->ventilationPositionDoor();
    }
  }
  else if (strcmp(mqttStrings.setpos_topic, topic) == 0){
    hoermannEngine->setPosition(atoi(lastCommandPayload));
  }

}

void sendOnline()
{
  mqttClient.publish(mqttStrings.availability_topic, 0, true, HA_ONLINE);
}

void setWill()
{
  mqttClient.setWill(mqttStrings.availability_topic, 0, true, HA_OFFLINE);
}

void sendDebug(char *key, String value)
{
  JsonDocument doc;
  char payload[1024];
  doc["reset-reason"] = esp_reset_reason();
  doc["debug"] = hoermannEngine->state->debugMessage;
  serializeJson(doc, payload);
  mqttClient.publish(mqttStrings.debug_topic, 0, false, payload);  //uint16_t publish(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr, size_t length = 0)
}

void sendDiscoveryMessageForBinarySensor(const char name[], const char topic[], const char key[], const char off[], const char on[], const JsonDocument& device)
{

  char full_topic[64];
  sprintf(full_topic, HA_DISCOVERY_BIN_SENSOR, localPrefs->getString(preference_gd_id), key);

  char uid[64];
  sprintf(uid, "%s_binary_sensor_%s", localPrefs->getString(preference_gd_id), key);

  char vtemp[64];
  sprintf(vtemp, "{{ value_json.%s }}", key);

  JsonDocument doc;

  doc["name"] = name;
  doc["state_topic"] = topic;
  doc["availability_topic"] = mqttStrings.availability_topic;
  doc["payload_available"] = HA_ONLINE;
  doc["payload_not_available"] = HA_OFFLINE;
  doc["unique_id"] = uid;
  doc["value_template"] = vtemp;
  doc["payload_on"] = on;
  doc["payload_off"] = off;
  doc["device"] = device;

  char payload[1024];
  serializeJson(doc, payload);
  //-//Serial.write(payload);
  mqttClient.publish(full_topic, 1, true, payload);
}

void sendDiscoveryMessageForAVSensor(const JsonDocument& device)
{
  char full_topic[64];
  sprintf(full_topic, HA_DISCOVERY_AV_SENSOR, localPrefs->getString(preference_gd_id));

  char uid[64];
  sprintf(uid, "%s_sensor_availability", localPrefs->getString(preference_gd_id));
  JsonDocument doc;

  doc["name"] = localPrefs->getString(preference_gd_avail);
  doc["state_topic"] = mqttStrings.availability_topic;
  doc["unique_id"] = uid;
  doc["device"] = device;

  char payload[1024];
  serializeJson(doc, payload);
  //-//Serial.write(payload);
  mqttClient.publish(full_topic, 1, true, payload);
}

void sendDiscoveryMessageForSensor(const char name[], const char topic[], const char key[], const JsonDocument& device, const char device_class[] = "", const char unit[] = "")
{

  char full_topic[64];
  sprintf(full_topic, HA_DISCOVERY_SENSOR, localPrefs->getString(preference_gd_id), key);

  char uid[64];
  sprintf(uid, "%s_sensor_%s", localPrefs->getString(preference_gd_id), key);

  char vtemp[64];
  //small workaround to get the value as float
  if (key == "hum" || key == "temp" || key == "pres"){
    sprintf(vtemp, "{{ value_json.%s | float }}", key);
  } else {
    sprintf(vtemp, "{{ value_json.%s }}", key);
  }

  JsonDocument doc;

  doc["name"] = name;
  doc["state_topic"] = topic;
  doc["availability_topic"] = mqttStrings.availability_topic;
  doc["payload_available"] = HA_ONLINE;
  doc["payload_not_available"] = HA_OFFLINE;
  doc["value_template"] = vtemp;
  doc["device"] = device;
  
  //Only set device class if set
  if(device_class != "") {
    doc["device_class"] = device_class;
  }

  doc["unit_of_measurement"] = unit;
  doc["unique_id"] = uid;
  
  // overwrite device class if special key
  if (key == "hum") {
    doc["state_class"] = "measurement";
    doc["unit_of_measurement"] = "%";
    doc["device_class"] = "humidity";
  } else if (key == "temp") {
    doc["state_class"] = "measurement";
    doc["unit_of_measurement"] = "°C";
    doc["device_class"] = "temperature";
  } else if (key == "pres") {
    doc["state_class"] = "measurement";
    doc["unit_of_measurement"] = "hPa";
    doc["device_class"] = "pressure";
  }

  char payload[1024];
  serializeJson(doc, payload);
  //-//Serial.write(payload);
  mqttClient.publish(full_topic, 1, true, payload);
}

void sendDiscoveryMessageForDebug(const char name[], const char key[], const JsonDocument& device)
{

  char command_topic[64];
  sprintf(command_topic, mqttStrings.st_cmd_topic_var.c_str(), mqttStrings.debug_topic);

  char full_topic[64];
  sprintf(full_topic, HA_DISCOVERY_TEXT, localPrefs->getString(preference_gd_id), key);

  char uid[64];
  sprintf(uid, "%s_text_%s", localPrefs->getString(preference_gd_id), key);

  char vtemp[64];
  sprintf(vtemp, "{{ value_json.%s }}", key);

  JsonDocument doc;

  doc["name"] = name;
  doc["state_topic"] = mqttStrings.debug_topic;
  doc["command_topic"] = command_topic;
  doc["availability_topic"] = mqttStrings.availability_topic;
  doc["payload_available"] = HA_ONLINE;
  doc["payload_not_available"] = HA_OFFLINE;
  doc["unique_id"] = uid;
  doc["value_template"] = vtemp;
  doc["device"] = device;

  char payload[1024];
  serializeJson(doc, payload);
  //-//Serial.write(payload);
  mqttClient.publish(full_topic, 1, true, payload);
}

void sendDiscoveryMessageForSwitch(const char name[], const char discovery[], const char topic[], const char off[], const char on[], const char icon[], const JsonDocument& device, bool optimistic = false)
{
  char command_topic[64];
  sprintf(command_topic, mqttStrings.st_cmd_topic_var.c_str(), topic);

  char full_topic[64];
  sprintf(full_topic, discovery, localPrefs->getString(preference_gd_id), topic);

  char value_template[64];
  sprintf(value_template, "{{ value_json.%s }}", topic);

  char uid[64];
  if (discovery == HA_DISCOVERY_LIGHT){
    sprintf(uid, "%s_light_%s",localPrefs->getString(preference_gd_id), topic);
  }
  else{
    sprintf(uid, "%s_switch_%s",localPrefs->getString(preference_gd_id), topic);
  }

  JsonDocument doc;

  doc["name"] = name;
  doc["state_topic"] = mqttStrings.state_topic;
  doc["command_topic"] = command_topic;
  doc["payload_on"] = on;
  doc["payload_off"] = off;
  doc["icon"] = icon;
  doc["availability_topic"] = mqttStrings.availability_topic;
  doc["payload_available"] = HA_ONLINE;
  doc["payload_not_available"] = HA_OFFLINE;
  doc["unique_id"] = uid;
  doc["value_template"] = value_template;
  doc["optimistic"] = optimistic;
  doc["device"] = device;

  char payload[1024];
  serializeJson(doc, payload);
  //-//Serial.write(payload);
  mqttClient.publish(full_topic, 1, true, payload);
}

void sendDiscoveryMessageForCover(const char name[], const char topic[], const JsonDocument& device)
{

  char command_topic[64];
  sprintf(command_topic, mqttStrings.st_cmd_topic_var.c_str(), topic);

  char full_topic[64];
  sprintf(full_topic, HA_DISCOVERY_COVER, localPrefs->getString(preference_gd_id), topic);

  char uid[64];
  sprintf(uid, "%s_cover_%s", localPrefs->getString(preference_gd_id), topic);

  JsonDocument doc;
 //if it didn't work try without state topic.
  doc["name"] = name;
  doc["state_topic"] = mqttStrings.state_topic;
  doc["command_topic"] = command_topic;
  doc["position_topic"] = mqttStrings.pos_topic;
  doc["set_position_topic"] = mqttStrings.setpos_topic;
  doc["position_open"] = 100;
  doc["position_closed"] = 0;

  doc["payload_open"] = HA_OPEN;
  doc["payload_close"] = HA_CLOSE;
  doc["payload_stop"] = HA_STOP;
  #ifdef AlignToOpenHab
    doc["value_template"] = "{{ value_json.doorposition }}";
  #else
    doc["value_template"] = "{{ value_json.doorstate }}";
  #endif
  doc["state_open"] = HA_OPEN;
  doc["state_opening"] = HA_OPENING;
  doc["state_closed"] = HA_CLOSED;
  doc["state_closing"] = HA_CLOSING;
  doc["state_stopped"] = HA_STOP;
  doc["availability_topic"] = mqttStrings.availability_topic;
  doc["payload_available"] = HA_ONLINE;
  doc["payload_not_available"] = HA_OFFLINE;
  doc["unique_id"] = uid;
  doc["device_class"] = "garage";
  doc["device"] = device;

  char payload[1024];
  serializeJson(doc, payload);
  //-//Serial.write(payload);
  mqttClient.publish(full_topic, 1, true, payload);
}

void sendDiscoveryMessage()
{
  //declare json object here for device instead of creating in each methode. 150 bytes should be enough
  JsonDocument device;
  device["identifiers"] = localPrefs->getString(preference_gd_name);
  device["name"] = localPrefs->getString(preference_gd_name);
  device["sw_version"] = HA_VERSION;
  device["model"] = "Garage Door";
  device["manufacturer"] = "Hörmann";
  
  sendDiscoveryMessageForAVSensor(device);
  //not able to get it working sending the discovery message for light.
  sendDiscoveryMessageForSwitch(localPrefs->getString(preference_gd_light).c_str(), HA_DISCOVERY_SWITCH, "lamp", HA_OFF, HA_ON, "mdi:lightbulb", device);
  sendDiscoveryMessageForBinarySensor(localPrefs->getString(preference_gd_light).c_str(), mqttStrings.state_topic, "lamp", HA_OFF, HA_ON, device);
  sendDiscoveryMessageForSwitch(localPrefs->getString(preference_gd_vent).c_str(), HA_DISCOVERY_SWITCH, "vent", HA_CLOSE, HA_VENT, "mdi:air-filter", device);
  sendDiscoveryMessageForCover(localPrefs->getString(preference_gd_name).c_str(), "door", device);

  sendDiscoveryMessageForSensor(localPrefs->getString(preference_gd_status).c_str(), mqttStrings.state_topic, "doorstate", device, "enum");
  sendDiscoveryMessageForSensor(localPrefs->getString(preference_gd_det_status).c_str(), mqttStrings.state_topic, "detailedState", device, "enum");
  sendDiscoveryMessageForSensor(localPrefs->getString(preference_gd_position).c_str(), mqttStrings.state_topic, "doorposition", device);
  #ifdef SENSORS
    #if defined(USE_BME)
      sendDiscoveryMessageForSensor(localPrefs->getString(preference_gs_temp).c_str(), mqttStrings.sensor_topic, "temp", device, "temperature", "°C");
      sendDiscoveryMessageForSensor(localPrefs->getString(preference_gs_hum).c_str(), mqttStrings.sensor_topic, "hum", device, "humidity", "%");
      sendDiscoveryMessageForSensor(localPrefs->getString(preference_gs_pres).c_str(), mqttStrings.sensor_topic, "pres", device, "atmospheric_pressure", "hPa");
    #elif defined(USE_DS18X20)
      sendDiscoveryMessageForSensor(localPrefs->getString(preference_gs_temp).c_str(), mqttStrings.sensor_topic, "temp", device, "temperature", "°C");
    #endif
    #if defined(USE_HCSR04)
      sendDiscoveryMessageForSensor(localPrefs->getString(preference_gs_free_dist).c_str(), mqttStrings.sensor_topic, "dist", device, "distance", "cm");
      sendDiscoveryMessageForBinarySensor(localPrefs->getString(preference_gs_park_avail).c_str(), mqttStrings.sensor_topic, "free", HA_OFF, HA_ON, device);
    #endif
    #if defined(USE_DHT22)
      sendDiscoveryMessageForSensor(localPrefs->getString(preference_gs_temp).c_str(), mqttStrings.sensor_topic, "temp", device, "temperature", "°C");
      sendDiscoveryMessageForSensor(localPrefs->getString(preference_gs_hum).c_str(), mqttStrings.sensor_topic, "hum", device, "humidity", "%");
    #endif
    #if defined(USE_HCSR501)
      sendDiscoveryMessageForBinarySensor(localPrefs->getString(preference_sensor_sr501).c_str(), mqttStrings.sensor_topic, "motion", HA_OFF, HA_ON, device);
    #endif
  #endif
  #ifdef DEBUG
    sendDiscoveryMessageForDebug(localPrefs->getString(preference_gd_debug).c_str(), "debug", device);
    sendDiscoveryMessageForDebug(localPrefs->getString(preference_gd_debug_restart).c_str(), "reset-reason", device);
  #endif
}

void onMqttConnect(bool sessionPresent)
{
  Serial.println("Function on mqtt connect.");
  mqttConnected = true;
  xTimerStop(mqttReconnectTimer, 0); // stop timer as we are connected to Mqtt again
  sendOnline();
  mqttClient.subscribe(mqttStrings.st_cmd_topic_subs.c_str(), 1);
  updateDoorStatus(true);
  updateSensors(true);
  sendDiscoveryMessage();
  #ifdef DEBUG
    if (boot_Flag){
      int i = esp_reset_reason();
      char val[3];
      sprintf(val, "%i", i);
      sendDebug("ResetReason", val);
      boot_Flag = false;
    }
  #endif
}

void mqttTaskFunc(void *parameter)
{
  while (true)
  {
    if (mqttConnected){
      updateDoorStatus();
      updateSensors();
      #ifdef DEBUG
      if (hoermannEngine->state->debMessage){
        hoermannEngine->state->clearDebug();
        sendDebug();
      }
      #endif      
    }
    vTaskDelay(READ_DELAY);     // delay task xxx ms
  }
}

TaskHandle_t mqttTask;

void SensorCheck(void *parameter){
  while(true){
    // handle motion sensor at first and send state immediately. Do not 
    // use updateSensors to avoid unneccessary polling of the other sensors
    #ifdef USE_HCSR501
      hcsr501stat = digitalRead(SR501PIN);
      if (hcsr501stat != hcsr501_laststat) {
        hcsr501_laststat = hcsr501stat;
        JsonDocument doc;
        char payload[1024];
        if (hcsr501stat) {
          doc["motion"] = HA_ON;
        }
        else {
          doc["motion"] = HA_OFF;
        }
        serializeJson(doc, payload);
        mqttClient.publish(mqttStrings.sensor_topic, 1, true, payload);
      }
    #endif
    #ifdef USE_DS18X20
      ds18x20_temp = ds18x20->getTempCByIndex(0);
      if (abs(ds18x20_temp-ds18x20_last_temp) >= sensor_temp_thresh){
        ds18x20_last_temp = ds18x20_temp;
        new_sensor_data = true;
      }
    #endif
    #ifdef USE_BME
      if (digitalRead(i2c_onoffpin) == LOW) {
        digitalWrite(i2c_onoffpin, HIGH);   // activate sensor
        sleep(10);
        I2CBME.begin(i2c_sdapin, i2c_sclpin);   // https://randomnerdtutorials.com/esp32-i2c-communication-arduino-ide/
        bme_status = bme.begin(0x76, &I2CBME);  // check sensor. adreess can be 0x76 or 0x77
      }
      if (!bme_status) {
        bme_status = bme.begin(0x77, &I2CBME);  // check sensor. address can be 0x76 or 0x77
      }
      if (!bme_status) {
        digitalWrite(i2c_onoffpin, LOW);      // deactivate sensor
      }
      else {
        bme_temp = bme.readTemperature();   // round float
        bme_hum = bme.readHumidity();
        bme_pres = bme.readPressure()/100;  // convert from pascal to mbar
        if (bme_hum < 99.9){                   // I2C hung up ...
          if (abs(bme_temp-bme_last_temp) >= sensor_temp_thresh || abs(bme_hum-bme_last_hum) >= sensor_hum_thresh || abs(bme_pres-bme_last_pres) >= sensor_pres_thresh){
            bme_last_temp = bme_temp;
            bme_last_hum = bme_hum;
            bme_last_pres = bme_pres;
            new_sensor_data = true;
          }
        } else {
          digitalWrite(i2c_onoffpin, LOW);      // deactivate sensor
        }
      }
    #endif
    #ifdef USE_HCSR04

        // Clears the trigPin
        digitalWrite(hcsr04_tgpin, LOW);
        delayMicroseconds(2);
        // Sets the trigPin on HIGH state for 10 micro seconds
        digitalWrite(hcsr04_tgpin, HIGH);
        delayMicroseconds(10);
        digitalWrite(hcsr04_tgpin, LOW);
        // Reads the echoPin, returns the sound wave travel time in microseconds
        hcsr04_duration = pulseIn(hscr04_ecpin, HIGH);
        // Calculate the distance
        hcsr04_distanceCm = hcsr04_duration * SOUND_SPEED/2;
        if (hcsr04_distanceCm > hcsr04_maxdistanceCm) {
          // set new Max
          hcsr04_maxdistanceCm = hcsr04_distanceCm;
        }
        if ((hcsr04_distanceCm + sensor_prox_tresh) > hcsr04_maxdistanceCm ){
          hcsr04_park_available = true;
        } else {
          hcsr04_park_available = false;
        }
        if (abs(hcsr04_distanceCm-hcsr04_lastdistanceCm) >= sensor_prox_tresh || hcsr04_park_available != hcsr04_lastpark_available ){
          hcsr04_lastdistanceCm = hcsr04_distanceCm;
          hcsr04_lastpark_available = hcsr04_park_available;
          new_sensor_data = true;
        }
    #endif
    #ifdef USE_DHT22
      dht22_temp = dht->readTemperature();
      dht22_hum = dht->readHumidity();
      if (!isnan(dht22_temp) && abs(dht22_temp) >= sensor_temp_thresh){
        dht22_last_temp = dht22_temp;
        new_sensor_data = true;
      }
      if (!isnan(dht22_hum) && abs(dht22_hum) >= sensor_hum_thresh){
        dht22_last_hum = dht22_hum;
        new_sensor_data = true;
      }
    #endif
    vTaskDelay(localPrefs->getInt(preference_query_interval_sensors)*1000);     // delay task xxx ms if statemachine had nothing to do
    //vTaskDelay(SENSE_PERIOD);     // TODO take from Preferences
  }
}

TaskHandle_t sensorTask;

void WiFiEvent(WiFiEvent_t event) {
    String eventInfo = "No Info";

    switch (event) {
        case ARDUINO_EVENT_WIFI_READY: 
            eventInfo = "WiFi interface ready";
            break;
        case ARDUINO_EVENT_WIFI_SCAN_DONE:
            eventInfo = "Completed scan for access points";
            break;
        case ARDUINO_EVENT_WIFI_STA_START:
            eventInfo = "WiFi client started";
            break;
        case ARDUINO_EVENT_WIFI_STA_STOP:
            eventInfo = "WiFi clients stopped";
            break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            eventInfo = "Connected to access point";
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            eventInfo = "Disconnected from WiFi access point";
            xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
            xTimerStart(wifiReconnectTimer, 0);
            break;
        case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
            eventInfo = "Authentication mode of access point has changed";
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            eventInfo = "Obtained IP address";
            xTimerStop(wifiReconnectTimer, 0); // stop in case it was started
            xTimerStart(mqttReconnectTimer, 0);
            //Serial.println(WiFi.localIP());
            break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
            eventInfo = "Lost IP address and IP address is reset to 0";
            break;
        case ARDUINO_EVENT_WPS_ER_SUCCESS:
            eventInfo = "WiFi Protected Setup (WPS): succeeded in enrollee mode";
            break;
        case ARDUINO_EVENT_WPS_ER_FAILED:
            eventInfo = "WiFi Protected Setup (WPS): failed in enrollee mode";
            break;
        case ARDUINO_EVENT_WPS_ER_TIMEOUT:
            eventInfo = "WiFi Protected Setup (WPS): timeout in enrollee mode";
            break;
        case ARDUINO_EVENT_WPS_ER_PIN:
            eventInfo = "WiFi Protected Setup (WPS): pin code in enrollee mode";
            break;
        case ARDUINO_EVENT_WIFI_AP_START:
            eventInfo = "WiFi access point started";
            break;
        case ARDUINO_EVENT_WIFI_AP_STOP:
            eventInfo = "WiFi access point  stopped";
            break;
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            eventInfo = "Client connected";
            break;
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            eventInfo = "Client disconnected";
            break;
        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
            eventInfo = "Assigned IP address to client";
            break;
        case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
            eventInfo = "Received probe request";
            break;
        case ARDUINO_EVENT_WIFI_AP_GOT_IP6:
            eventInfo = "AP IPv6 is preferred";
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
            eventInfo = "STA IPv6 is preferred";
            break;
        case ARDUINO_EVENT_ETH_GOT_IP6:
            eventInfo = "Ethernet IPv6 is preferred";
            break;
        case ARDUINO_EVENT_ETH_START:
            eventInfo = "Ethernet started";
            break;
        case ARDUINO_EVENT_ETH_STOP:
            eventInfo = "Ethernet stopped";
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            eventInfo = "Ethernet connected";
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            eventInfo = "Ethernet disconnected";
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            eventInfo = "Obtained IP address";
            break;
        default: break;
    }
    Serial.print("WIFI-Event: ");
    Serial.println(eventInfo);
}

// Function to generate a unique ID
const char* generateUniqueID() {
  static char uniqueID[ID_LENGTH + 1];
  uint64_t chipid = ESP.getEfuseMac();
  
  // Format the MAC address into the uniqueID array
  snprintf(uniqueID, sizeof(uniqueID), "ESP-%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
   return uniqueID;
}

// setup mcu
void setup()
{
  // Serial
  Serial.begin(9600);

/*
  while (Serial.available()==0){
    //only continues if an input get received from serial.
    ;
  } 
*/  

  // setup preferences
  prefHandler.initPreferences();
  localPrefs = prefHandler.getPreferences();
  // setup modbus
  hoermannEngine->setup(localPrefs);

  //Add interrupts for Factoryreset over Boot button
  pinMode(0, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(0), reset_button_change, CHANGE);
  resetTimer = xTimerCreate("resetTimer", pdMS_TO_TICKS(10), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(resetPreferences));

  // setup wifi
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));
  WiFi.setHostname(prefHandler.getPreferencesCache()->hostname);
  if (localPrefs->getBool(preference_wifi_ap_mode)){
    Serial.println("WIFI AP enabled");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(prefHandler.getPreferencesCache()->hostname);
    }
  else{
    WiFi.mode(WIFI_STA);  
  }
  

  WiFi.onEvent(WiFiEvent);

  // generate unique ID as mqttclientid
  const char *uniqueId = generateUniqueID();
	
  setuptMqttStrings();
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);

  mqttClient.setClientId(uniqueId);
  mqttClient.setServer(prefHandler.getPreferencesCache()->mqtt_server, localPrefs->getInt(preference_mqtt_server_port));
  mqttClient.setCredentials(prefHandler.getPreferencesCache()->mqtt_user, prefHandler.getPreferencesCache()->mqtt_password);
  setWill();

  delay(1000);
  
  connectToWifi();

  xTaskCreatePinnedToCore(
      mqttTaskFunc, /* Function to implement the task */
      "MqttTask",   /* Name of the task */
      10000,        /* Stack size in words */
      NULL,         /* Task input parameter */
      // 1,  /* Priority of the task */
      configMAX_PRIORITIES - 3,
      &mqttTask, /* Task handle. */
      0);        /* Core where the task should run */


  #ifdef SENSORS
    sensor_prox_tresh = localPrefs->getInt(preference_sensor_prox_treshold);
    sensor_temp_thresh = localPrefs->getDouble(preference_sensor_temp_treshold);
    sensor_hum_thresh = localPrefs->getInt(preference_sensor_hum_threshold);
    sensor_pres_thresh = localPrefs->getInt(preference_sensor_pres_threshold);
    #ifdef USE_DS18X20
      ds18x20_pin = localPrefs->getInt(preference_sensor_ds18x20_pin);
      OneWire oneWire(ds18x20_pin);
      static DallasTemperature static_ds18x20(&oneWire);
      // save its address.
      ds18x20 = &static_ds18x20;
      ds18x20->begin();
    #endif
    #ifdef USE_BME
      i2c_sdapin = localPrefs->getInt(preference_sensor_i2c_sda);
      i2c_sclpin = localPrefs->getInt(preference_sensor_i2c_scl);
      pinMode(i2c_onoffpin, OUTPUT);
      I2CBME.begin(i2c_sdapin, i2c_sclpin);   // https://randomnerdtutorials.com/esp32-i2c-communication-arduino-ide/
      bme_status = bme.begin(0x76, &I2CBME);  // check sensor. adreess can be 0x76 or 0x77
      //bme_status = bme.begin();  // check sensor. adreess can be 0x76 or 0x77
    #endif
    #ifdef USE_HCSR04
      hcsr04_tgpin = localPrefs->getInt(preference_sensor_sr04_trigpin);
      hscr04_ecpin = localPrefs->getInt(preference_sensor_sr04_echopin);
      hcsr04_maxdistanceCm = localPrefs->getInt(preference_sensor_sr04_max_dist);
      pinMode(hcsr04_tgpin, OUTPUT); // Sets the trigPin as an Output
      pinMode(hscr04_ecpin, INPUT); // Sets the echoPin as an Input
    #endif
    #ifdef USE_HCSR501
      pinMode(SR501PIN, INPUT); // Sets the trigPin as an Output
      hcsr501_laststat = digitalRead(SR501PIN); // read first state of sensor
    #endif
    #ifdef USE_DHT22
      dht_data_pin = localPrefs->getInt(preference_sensor_dht_data_pin);
       static DHT static_dht(dht_data_pin, DHTTYPE);
      // save its address.
      dht = &static_dht;
      dht->begin();
    #endif

    xTaskCreatePinnedToCore(
      SensorCheck, /* Function to implement the task */
      "SensorTask",   /* Name of the task */
      10000,        /* Stack size in words */
      NULL,         /* Task input parameter */
      // 1,  /* Priority of the task */
      configMAX_PRIORITIES,
      &sensorTask, /* Task handle. */
      0);        /* Core where the task should run */
  #endif


  // setup http server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html, sizeof(index_html));
              response->addHeader("Content-Encoding", "deflate");
              request->send(response); });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
              //const SHCIState &doorstate = emulator.getState();
              AsyncResponseStream *response = request->beginResponseStream("application/json");
              JsonDocument root;
              //response->print(hoermannEngine->state->toStatusJson());
              root["doorstate"] = hoermannEngine->state->translatedState;
              root["valid"] = hoermannEngine->state->valid;
              root["targetPosition"] = (int)(hoermannEngine->state->targetPosition * 100);
              root["currentPosition"] = (int)(hoermannEngine->state->currentPosition * 100);
              root["light"] = hoermannEngine->state->lightOn;
              root["state"] = hoermannEngine->state->state;
              root["busResponseAge"] = hoermannEngine->state->responseAge();
              root["lastModbusRespone"] = hoermannEngine->state->lastModbusRespone;
              root["swversion"] = HA_VERSION;
              #ifdef SENSORS
                JsonObject sensors  = root.createNestedObject("sensors");
                  char buf[20];
                #ifdef USE_DS18X20
                  dtostrf(ds18x20_temp,2,1,buf);
                  strcat(buf, " °C");
                  sensors["temp"] = buf;
                #endif
                #ifdef USE_BME
                  dtostrf(bme_temp,2,1,buf);
                  strcat(buf, " °C");
                  sensors["temp"] = buf;
                  dtostrf(bme_hum,2,1,buf); 
                  strcat(buf, " %");
                  sensors["hum"] = buf;
                  dtostrf(bme_pres,2,1,buf); 
                  strcat(buf, " mbar");
                  sensors["pres"] = buf;
                #endif
                #ifdef USE_DHT22
                  dtostrf(dht22_temp,2,1,buf);
                  strcat(buf, " °C");
                  sensors["temp"] = buf;
                #endif
                #ifdef USE_HCSR04
                  dtostrf(hcsr04_distanceCm,2,0,buf);
                  strcat(buf, " cm");
                  sensors["dist"] = buf;
                #endif
              #endif
              //root["debug"] = doorstate.reserved;
              root["lastCommandTopic"] = lastCommandTopic;
              root["lastCommandPayload"] = lastCommandPayload;
              serializeJson(root, *response);
              request->send(response); });

  server.on("/statush", HTTP_GET, [](AsyncWebServerRequest *request){
              AsyncResponseStream *response = request->beginResponseStream("application/json");
              response->print(hoermannEngine->state->toStatusJson());
              request->send(response); });

  server.on("/command", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              if (request->hasParam("action"))
              {
                int actionid = request->getParam("action")->value().toInt();
                switch (actionid)
                {
                case 0:
                  hoermannEngine->closeDoor();
                  break;
                case 1:
                  hoermannEngine->openDoor();
                  break;
                case 2:
                  hoermannEngine->stopDoor();
                  break;
                case 3:
                  hoermannEngine->ventilationPositionDoor();
                  break;
                case 4:
                  hoermannEngine->halfPositionDoor();
                  break;
                case 5:
                  hoermannEngine->toogleLight();
                  break;
                case 6:
                  Serial.println("restart...");
                  setWill();
                  ESP.restart();
                  break;
                case 7:
                  if (request->hasParam("position"))
                    hoermannEngine->setPosition(request->getParam("position")->value().toInt());
                  break;
                default:
                  break;
                }
              }
              request->send(200, "text/plain", "OK");
              //const SHCIState &doorstate = emulator.getState();
              //onStatusChanged(doorstate);
              });

  server.on("/sysinfo", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              Serial.println("GET SYSINFO");
              AsyncResponseStream *response = request->beginResponseStream("application/json");
              JsonDocument root;
              root["freemem"] = ESP.getFreeHeap();
              root["hostname"] = WiFi.getHostname();
              root["ip"] = WiFi.localIP().toString();
              root["wifistatus"] = WiFi.status();
              root["mqttstatus"] = mqttClient.connected();
              root["resetreason"] = esp_reset_reason();
              root["swversion"] = HA_VERSION;
              serializeJson(root, *response);

              request->send(response); });
  
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              Serial.println("GET CONFIG");
              AsyncResponseStream *response = request->beginResponseStream("application/json");
              JsonDocument conf;
              prefHandler.getConf(conf);
              serializeJson(conf, *response);
              request->send(response); });

  // load requestbody for json Post requests
  server.onRequestBody([](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total)
          {
          // Handle setting config request
          if (request->url() == "/config")
          {
            JsonDocument doc;
            deserializeJson(doc, data);
            prefHandler.saveConf(doc);

            request->send(200, "text/plain", "OK");
          } });
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request)
        {
          Serial.println("GET reset");
          AsyncResponseStream *response = request->beginResponseStream("application/json");
          JsonDocument root;
          root["reset"] = "OK";
          serializeJson(root, *response);
          request->send(response); 
          prefHandler.resetPreferences();
          });

  ElegantOTA.begin(&server);
  ElegantOTA.setAutoReboot(true);
  ElegantOTA.setAuth(OTA_USERNAME, OTA_PASSWD);

  server.begin();
}

// mainloop
void loop(){
  ElegantOTA.loop();
}
