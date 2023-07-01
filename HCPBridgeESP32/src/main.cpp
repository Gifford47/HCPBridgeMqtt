#include <Arduino.h>
#include <Esp.h>
#include <AsyncElegantOTA.h>
#include <ESPAsyncWebServer.h>
#include "AsyncJson.h"
#include <AsyncMqttClient.h>
extern "C" {
	#include "freertos/FreeRTOS.h"
	#include "freertos/timers.h"
}
#include "ArduinoJson.h"

#include "configuration.h"
#include "hoermann.h"
#include "../../WebUI/index_html.h"

#ifdef USE_DS18X20
  #include <OneWire.h>
  #include <DallasTemperature.h>
#endif

#ifdef USE_BME
  #include <Wire.h>
  #include <Adafruit_Sensor.h>
  #include <Adafruit_BME280.h>
#endif

// webserver on port 80
AsyncWebServer server(80);

#ifdef USE_DS18X20
  // Setup a oneWire instance to communicate with any OneWire devices
  OneWire oneWire(oneWireBus);
  DallasTemperature sensors(&oneWire);
  bool new_sensor_data = false;
  float ds18x20_temp = -99.99;
  float ds18x20_last_temp = -99.99;
#endif
#ifdef USE_BME
  TwoWire I2CBME = TwoWire(0);
  Adafruit_BME280 bme;
  bool new_sensor_data = false;
  unsigned bme_status;
  float bme_temp = -99.99;
  float bme_last_temp = -99.99;
  float bme_hum = -99.99;
  float bme_last_hum = -99.99;
  float bme_pres = -99.99;
  float bme_last_pres = -99.99;
#endif

#ifdef USE_HCSR04
  long hcsr04_duration = -99.99;
  int hcsr04_distanceCm = 0;
  int hcsr04_lastdistanceCm = 0;
  int hcsr04_maxdistanceCm = 150;
  bool hcsr04_park_available = false;
  bool hcsr04_lastpark_available = false;
#endif

// mqtt
volatile bool mqttConnected;
AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

char lastCommandTopic[64];
char lastCommandPayload[64];

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

void switchLamp(bool on){
  hoermannEngine->turnLight(on);
}

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(STA_SSID, STA_PASSWD);
}
void connectToMqtt()
{
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  mqttConnected = false;
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttPublish(uint16_t packetId){
}

void updateDoorStatus()
{
  // onyl send updates when state changed
  if (mqttConnected == true && hoermannEngine->state->changed ){
    hoermannEngine->state->clearChanged();
    DynamicJsonDocument doc(1024);
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
    mqttClient.publish(STATE_TOPIC, 1, true, payload);

    sprintf(payload, "%d", (int)(hoermannEngine->state->currentPosition * 100));
    mqttClient.publish(POS_TOPIC, 1, true, payload);
  }
}


void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{

  // Note that payload is NOT a string; it contains raw data.
  // https://github.com/marvinroger/async-mqtt-client/blob/develop/docs/5.-Troubleshooting.md

  strcpy(lastCommandTopic, topic);
  strncpy(lastCommandPayload, payload, len);
  lastCommandPayload[len] = '\0';

  if (strcmp(topic, LAMP_TOPIC) == 0)
  {
    if (strncmp(payload, HA_ON, len) == 0)
    {
      switchLamp(true);
    }
    else if (strncmp(payload, HA_OFF, len) == 0)
    {
      switchLamp(false);
    }
    else
    {
      hoermannEngine->toogleLight();
    }
  }

  else if (strcmp(DOOR_TOPIC, topic) == 0 || strcmp(VENT_TOPIC, topic) == 0)
  {
    if (strncmp(payload, HA_OPEN, len) == 0)
    {
      hoermannEngine->openDoor();
    }
    else if (strncmp(payload, HA_CLOSE, len) == 0)
    {
      hoermannEngine->closeDoor();
    }
    else if (strncmp(payload, HA_STOP, len) == 0)
    {
      hoermannEngine->stopDoor();
    }
    else if (strncmp(payload, HA_HALF, len) == 0)
    {
      hoermannEngine->halfPositionDoor();
    }
    else if (strncmp(payload, HA_VENT, len) == 0)
    {
      hoermannEngine->ventilationPositionDoor();
    }
  }

  else if (strcmp(SETPOS_TOPIC, topic) == 0)
  {
    hoermannEngine->setPosition(atoi(lastCommandPayload));
  }

}

void sendOnline()
{
  mqttClient.publish(AVAILABILITY_TOPIC, 0, true, HA_ONLINE);
}

void setWill()
{
  mqttClient.setWill(AVAILABILITY_TOPIC, 0, true, HA_OFFLINE);
}

void sendDebug(char *key, String value)
{
  DynamicJsonDocument doc(1024);
  char payload[1024];
  doc["reset-reason"] = esp_reset_reason();
  doc["debug"] = hoermannEngine->state->debugMessage;
  serializeJson(doc, payload);
  mqttClient.publish(DEBUGTOPIC, 0, false, payload);  //uint16_t publish(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr, size_t length = 0)
}

void sendDiscoveryMessageForBinarySensor(const char name[], const char topic[], const char key[], const char off[], const char on[], const JsonDocument& device)
{

  char full_topic[64];
  sprintf(full_topic, HA_DISCOVERY_BIN_SENSOR, key);

  char uid[64];
  sprintf(uid, "garagedoor_binary_sensor_%s", key);

  char vtemp[64];
  sprintf(vtemp, "{{ value_json.%s }}", key);

  DynamicJsonDocument doc(1024);

  doc["name"] = name;
  doc["state_topic"] = topic;
  doc["availability_topic"] = AVAILABILITY_TOPIC;
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
  DynamicJsonDocument doc(1024);

  doc["name"] = "Garage Door Available";
  doc["state_topic"] = AVAILABILITY_TOPIC;
  doc["unique_id"] = "garagedoor_sensor_availability";
  doc["device"] = device;

  char payload[1024];
  serializeJson(doc, payload);
  //-//Serial.write(payload);
  mqttClient.publish(HA_DISCOVERY_AV_SENSOR, 1, true, payload);
}

void sendDiscoveryMessageForSensor(const char name[], const char topic[], const char key[], const JsonDocument& device)
{

  char full_topic[64];
  sprintf(full_topic, HA_DISCOVERY_SENSOR, key);

  char uid[64];
  sprintf(uid, "garagedoor_sensor_%s", key);

  char vtemp[64];
  sprintf(vtemp, "{{ value_json.%s }}", key);

  DynamicJsonDocument doc(1024);

  doc["name"] = name;
  doc["state_topic"] = topic;
  doc["availability_topic"] = AVAILABILITY_TOPIC;
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

void sendDiscoveryMessageForDebug(const char name[], const char key[], const JsonDocument& device)
{

  char command_topic[64];
  sprintf(command_topic, CMD_TOPIC "/%s", DEBUGTOPIC);

  char full_topic[64];
  sprintf(full_topic, "homeassistant/text/garage_door/%s/config", key);

  char uid[64];
  sprintf(uid, "garagedoor_text_%s", key);

  char vtemp[64];
  sprintf(vtemp, "{{ value_json.%s }}", key);

  DynamicJsonDocument doc(1024);

  doc["name"] = name;
  doc["state_topic"] = DEBUGTOPIC;
  doc["command_topic"] = command_topic;
  doc["availability_topic"] = AVAILABILITY_TOPIC;
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
  sprintf(command_topic, CMD_TOPIC "/%s", topic);

  char full_topic[64];
  sprintf(full_topic, discovery, topic);

  char value_template[64];
  sprintf(value_template, "{{ value_json.%s }}", topic);

  char uid[64];
  if (discovery == HA_DISCOVERY_LIGHT){
    sprintf(uid, "garagedoor_light_%s", topic);
  }
  else{
    sprintf(uid, "garagedoor_switch_%s", topic);
  }

  DynamicJsonDocument doc(1024);

  doc["name"] = name;
  doc["state_topic"] = STATE_TOPIC;
  doc["command_topic"] = command_topic;
  doc["payload_on"] = on;
  doc["payload_off"] = off;
  doc["icon"] = icon;
  doc["availability_topic"] = AVAILABILITY_TOPIC;
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
  sprintf(command_topic, CMD_TOPIC "/%s", topic);

  char full_topic[64];
  sprintf(full_topic, HA_DISCOVERY_COVER, topic);

  char uid[64];
  sprintf(uid, "garagedoor_cover_%s", topic);

  DynamicJsonDocument doc(1024);
 //if it didn't work try without state topic.
  doc["name"] = name;
  doc["state_topic"] = STATE_TOPIC;
  doc["command_topic"] = command_topic;
  doc["position_topic"] = POS_TOPIC;
  doc["set_position_topic"] = SETPOS_TOPIC;
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
  doc["availability_topic"] = AVAILABILITY_TOPIC;
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
  const int capacity = JSON_OBJECT_SIZE(5);
  StaticJsonDocument<capacity> device;
  device["identifiers"] = "Garage Door";
  device["name"] = "Garage Door";
  device["sw_version"] = HA_VERSION;
  device["model"] = "Garage Door";
  device["manufacturer"] = "Hörmann";
  
  sendDiscoveryMessageForAVSensor(device);
  //not able to get it working sending the discovery message for light.
  sendDiscoveryMessageForSwitch("Garage Door Light", HA_DISCOVERY_SWITCH, "lamp", HA_OFF, HA_ON, "mdi:lightbulb", device);
  sendDiscoveryMessageForBinarySensor("Garage Door Light", STATE_TOPIC, "lamp", HA_OFF, HA_ON, device);
  sendDiscoveryMessageForSwitch("Garage Door Vent", HA_DISCOVERY_SWITCH, "vent", HA_CLOSE, HA_VENT, "mdi:air-filter", device);
  sendDiscoveryMessageForCover("Garage Door", "door", device);

  sendDiscoveryMessageForSensor("Garage Door Status", STATE_TOPIC, "doorstate", device);
  sendDiscoveryMessageForSensor("Garage Door detailed Status", STATE_TOPIC, "detailedState", device);
  sendDiscoveryMessageForSensor("Garage Door Position", STATE_TOPIC, "doorposition", device);
  #ifdef SENSORS
    #if defined(USE_BME)
      sendDiscoveryMessageForSensor("Garage Temperature", SENSOR_TOPIC, "temp", device);
      sendDiscoveryMessageForSensor("Garage Humidity", SENSOR_TOPIC, "hum", device);
      sendDiscoveryMessageForSensor("Garage ambient pressure", SENSOR_TOPIC, "pres", device);
    #elif defined(USE_DS18X20)
      sendDiscoveryMessageForSensor("Garage Temperature", SENSOR_TOPIC, "temp", device);
    #endif
    #if defined(USE_HCSR04)
      sendDiscoveryMessageForSensor("Garage Free distance", SENSOR_TOPIC, "dist", device);
      sendDiscoveryMessageForBinarySensor("Garage park available", SENSOR_TOPIC, "free", HA_OFF, HA_ON, device);
    #endif
  #endif
  #ifdef DEBUG
    sendDiscoveryMessageForDebug("garage Door Debug", "debug", device);
    sendDiscoveryMessageForDebug("garage Restart Reason", "reset-reason", device);
  #endif
}

void onMqttConnect(bool sessionPresent)
{
  mqttConnected = true;

  sendOnline();
  mqttClient.subscribe(CMD_TOPIC "/#", 1);
  updateDoorStatus();
  sendDiscoveryMessage();
  #ifdef DEBUG
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
      #ifdef DEBUG
      if (hoermannEngine->state->debMessage){
        hoermannEngine->state->clearDebug();
        sendDebug();
      }
      #endif
      #ifdef SENSORS
        if (new_sensor_data) {
          #ifdef USE_DS18X20
            DynamicJsonDocument doc(1024);    //2048 needed because of BME280 float values!
            char payload[1024];
            char buf[20];
            dtostrf(ds18x20_temp,2,2,buf);    // convert to string
            //Serial.println("Temp: "+ (String)buf);
            doc["temp"] = buf;
            #ifdef USE_HCSR04
              sprintf(buf, "%d", hcsr04_distanceCm);
              doc["dist"] = buf;
              doc["free"] = ToHA(hcsr04_park_available);
              serializeJson(doc, payload);
              mqttClient.publish(SENSOR_TOPIC, 0, false, payload);  //uint16_t publish(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr, size_t length = 0)
              new_sensor_data = false;
            #endif
            serializeJson(doc, payload);
            mqttClient.publish(SENSOR_TOPIC, 0, false, payload);  //uint16_t publish(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr, size_t length = 0)
            new_sensor_data = false;
          #endif
          #ifdef USE_BME
            DynamicJsonDocument doc(1024);    //2048 needed because of BME280 float values!
            char payload[1024];
            char buf[20];
            dtostrf(bme_temp,2,2,buf);    // convert to string
            //Serial.println("Temp: "+ (String)buf);
            doc["temp"] = buf;
            dtostrf(bme_hum,2,2,buf);    // convert to string
            //Serial.println("Hum: "+ (String)buf);
            doc["hum"] = buf;
            dtostrf(bme_pres,2,1,buf);    // convert to string
            //Serial.println("Pres: "+ (String)buf);
            doc["pres"] = buf;
              #ifdef USE_HCSR04
                sprintf(buf, "%d", hcsr04_distanceCm);
                doc["dist"] = buf;
                doc["free"] = ToHA(hcsr04_park_available);
                serializeJson(doc, payload);
                mqttClient.publish(SENSOR_TOPIC, 0, false, payload);  //uint16_t publish(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr, size_t length = 0)
                new_sensor_data = false;
              #endif
            serializeJson(doc, payload);
            mqttClient.publish(SENSOR_TOPIC, 0, false, payload);  //uint16_t publish(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr, size_t length = 0)
            new_sensor_data = false;
          #endif

        }
      #endif
      
    }
    vTaskDelay(READ_DELAY);     // delay task xxx ms
  }
}

TaskHandle_t mqttTask;

void SensorCheck(void *parameter){
  while(true){
    #ifdef USE_DS18X20
      ds18x20_temp = sensors.getTempCByIndex(0);
      if (abs(ds18x20_temp-ds18x20_last_temp) >= temp_threshold){
        ds18x20_last_temp = ds18x20_temp;
        new_sensor_data = true;
      }
    #endif
    #ifdef USE_BME
      if (digitalRead(I2C_ON_OFF) == LOW) {
        digitalWrite(I2C_ON_OFF, HIGH);   // activate sensor
        sleep(10);
        I2CBME.begin(I2C_SDA, I2C_SCL);   // https://randomnerdtutorials.com/esp32-i2c-communication-arduino-ide/
        bme_status = bme.begin(0x76, &I2CBME);  // check sensor. adreess can be 0x76 or 0x77
        //bme_status = bme.begin();  // check sensor. adreess can be 0x76 or 0x77
      }
      if (!bme_status) {
        DynamicJsonDocument doc(1024);    //2048 needed because of BME280 float values!
        // char payload[1024];
        // doc["bme_status"] = "Could not find a valid BME280 sensor!";   // see: https://github.com/adafruit/Adafruit_BME280_Library/blob/master/examples/bme280test/bme280test.ino#L49
        // serializeJson(doc, payload);
        // mqttClient.publish(SENSOR_TOPIC, 0, false, payload);  //uint16_t publish(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr, size_t length = 0)
        digitalWrite(I2C_ON_OFF, LOW);      // deactivate sensor
      } else {
        bme_temp = bme.readTemperature();   // round float
        bme_hum = bme.readHumidity();
        bme_pres = bme.readPressure()/100;  // convert from pascal to mbar
        if (bme_hum < 99.9){                   // I2C hung up ...
          if (abs(bme_temp-bme_last_temp) >= temp_threshold || abs(bme_hum-bme_last_hum) >= hum_threshold || abs(bme_pres-bme_last_pres) >= pres_threshold){
            bme_last_temp = bme_temp;
            bme_last_hum = bme_hum;
            bme_last_pres = bme_pres;
            new_sensor_data = true;
          }
        } else {
          digitalWrite(I2C_ON_OFF, LOW);      // deactivate sensor
        }
      }
    #endif
    #ifdef USE_HCSR04
        // Clears the trigPin
        digitalWrite(SR04_TRIGPIN, LOW);
        delayMicroseconds(2);
        // Sets the trigPin on HIGH state for 10 micro seconds
        digitalWrite(SR04_TRIGPIN, HIGH);
        delayMicroseconds(10);
        digitalWrite(SR04_TRIGPIN, LOW);
        // Reads the echoPin, returns the sound wave travel time in microseconds
        hcsr04_duration = pulseIn(SR04_ECHOPIN, HIGH);
        // Calculate the distance
        hcsr04_distanceCm = hcsr04_duration * SOUND_SPEED/2;
        if (hcsr04_distanceCm > hcsr04_maxdistanceCm) {
          // set new Max
          hcsr04_maxdistanceCm = hcsr04_distanceCm;
        }
        if ((hcsr04_distanceCm + prox_treshold) > hcsr04_maxdistanceCm ){
          hcsr04_park_available = true;
        } else {
          hcsr04_park_available = false;
        }
        if (abs(hcsr04_distanceCm-hcsr04_lastdistanceCm) >= prox_treshold || hcsr04_park_available != hcsr04_lastpark_available ){
          hcsr04_lastdistanceCm = hcsr04_distanceCm;
          hcsr04_lastpark_available = hcsr04_park_available;
          new_sensor_data = true;
        }
    #endif
    vTaskDelay(SENSE_PERIOD);     // delay task xxx ms if statemachine had nothing to do
  }
}

TaskHandle_t sensorTask;

void WiFiEvent(WiFiEvent_t event) {
    Serial.printf("[WiFi-event] event: %d\n", event);
    switch(event) {
    case SYSTEM_EVENT_STA_GOT_IP:
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        connectToMqtt();
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        Serial.println("WiFi lost connection");
        xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
        xTimerStart(wifiReconnectTimer, 0);
        break;
    }
}
// setup mcu
void setup()
{
  // Serial
  Serial.begin(9600);

  // setup modbus
  hoermannEngine->setup();

  // setup wifi
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));
  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTTSERVER, MQTTPORT);
  mqttClient.setCredentials(MQTTUSER, MQTTPASSWORD);
  setWill();
  
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
    #ifdef USE_DS18X20
      // Start the DS18B20 sensor
      sensors.begin();
    #endif
    #ifdef USE_BME
      pinMode(I2C_ON_OFF, OUTPUT);
      I2CBME.begin(I2C_SDA, I2C_SCL);   // https://randomnerdtutorials.com/esp32-i2c-communication-arduino-ide/
      bme_status = bme.begin(0x76, &I2CBME);  // check sensor. adreess can be 0x76 or 0x77
      //bme_status = bme.begin();  // check sensor. adreess can be 0x76 or 0x77
    #endif
    #ifdef USE_HCSR04
      pinMode(SR04_TRIGPIN, OUTPUT); // Sets the trigPin as an Output
      pinMode(SR04_ECHOPIN, INPUT); // Sets the echoPin as an Input
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
              DynamicJsonDocument root(1024);
              //response->print(hoermannEngine->state->toStatusJson());
              root["doorstate"] = hoermannEngine->state->translatedState;
              root["valid"] = hoermannEngine->state->valid;
              root["targetPosition"] = (int)(hoermannEngine->state->targetPosition * 100);
              root["currentPosition"] = (int)(hoermannEngine->state->currentPosition * 100);
              root["light"] = hoermannEngine->state->lightOn;
              root["state"] = hoermannEngine->state->state;
              root["busResponseAge"] = hoermannEngine->state->responseAge();
              root["lastModbusRespone"] = hoermannEngine->state->lastModbusRespone;
              #ifdef SENSORS
                #ifdef USE_DS18X20
                  root["temp"] = ds18x20_temp;
                #elif defined(USE_BME)
                  root["temp"] = bme_temp;
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
              AsyncResponseStream *response = request->beginResponseStream("application/json");
              DynamicJsonDocument root(1024);
              root["freemem"] = ESP.getFreeHeap();
              root["hostname"] = WiFi.getHostname();
              root["ip"] = WiFi.localIP().toString();
              root["wifistatus"] = WiFi.status();
              root["mqttstatus"] = mqttClient.connected();
              root["resetreason"] = esp_reset_reason();
              serializeJson(root, *response);

              request->send(response); });

  AsyncElegantOTA.begin(&server, OTA_USERNAME, OTA_PASSWD);

  server.begin();
}

// mainloop
void loop(){
}
