#include <Arduino.h>
#include <Esp.h>
#include <AsyncElegantOTA.h>
#include <ESPAsyncWebServer.h>
#include "AsyncJson.h"
#include <AsyncMqttClient.h>
#include <Ticker.h>
#include "ArduinoJson.h"
#include <sstream>
#include <ESPAsyncWiFiManager.h>

#include "configuration.h"
#include "hciemulator.h"
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

DNSServer dns;

// Hörmann HCP2 based on modbus rtu @57.6kB 8E1
HCIEmulator emulator(&RS485);
SHCIState doorstate = emulator.getState();

// webserver on port 80
AsyncWebServer server(80);

#ifdef USE_DS18X20
  // Setup a oneWire instance to communicate with any OneWire devices
  OneWire oneWire(oneWireBus);
  DallasTemperature sensors(&oneWire);
  float ds18x20_temp = -99.99;
  float ds18x20_last_temp = -99.99;
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
#endif

// mqtt
volatile bool mqttConnected;
AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

char lastCommandTopic[64];
char lastCommandPayload[64];

bool Equals(const char *first, const char *second)
{
  strcpy(lastCommandTopic, first);
  strcpy(lastCommandPayload, second);
  return strcmp(first, second) == 0;
}

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

void switchLamp(bool on)
{
  bool toggle = (on && !emulator.getState().lampOn) || (!on && emulator.getState().lampOn);
  if (toggle)
  {
    emulator.toggleLamp();
  }
}

void connectToMqtt()
{
  mqttClient.connect();
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  mqttConnected = false;
  while (mqttConnected == false)
  {
    if (WiFi.isConnected())
    {
      mqttReconnectTimer.once(10, connectToMqtt);
    }
  }
}

void onMqttPublish(uint16_t packetId)
{
}

void DelayHandler(void)
{
  emulator.poll();
}

volatile unsigned long lastCall = 0;
volatile unsigned long maxPeriod = 0;

void onStatusChanged(const SHCIState &state)
{
  if (mqttConnected == true)
  {
    DynamicJsonDocument doc(1024);
    char payload[1024];

    doc["valid"] = ToHA(state.valid);
    doc["doorposition"] = (int)state.doorCurrentPosition / 2;
    doc["lamp"] = ToHA(state.lampOn);

    switch (state.doorState)
    {
    case DOOR_OPEN_POSITION:
      doc["doorstate"] = HA_OPENED;
      break;
    case DOOR_CLOSE_POSITION:
      doc["doorstate"] = HA_CLOSED;
      break;
    case DOOR_HALF_POSITION:
      doc["doorstate"] = HA_HALF;
      break;
    case DOOR_MOVE_CLOSEPOSITION:
      doc["doorstate"] = HA_CLOSING;
      break;
    case DOOR_MOVE_OPENPOSITION:
      doc["doorstate"] = HA_OPENING;
      break;
    default:
      doc["doorstate"] = "UNKNOWN";
    }

    lastCall = maxPeriod = 0;

    serializeJson(doc, payload);
    mqttClient.publish(STATE_TOPIC, 1, true, payload);

    sprintf(payload, "%d", (int)doorstate.doorCurrentPosition/2 );
    mqttClient.publish(POS_TOPIC, 1, true, payload);
  }
}


void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
  // strcpy(lastCommandTopic,topic);
  // strcpy(lastCommandPayload, payload);

  auto payloadEqualsOn = Equals(HA_ON, payload);
  auto payloadEqualsOff = Equals(HA_OFF, payload);

  if (Equals(LAMP_TOPIC, topic))
  {
    if (payloadEqualsOn)
    {
      switchLamp(true);
    }
    else if (payloadEqualsOff)
    {
      switchLamp(false);
    }
    else
    {
      emulator.toggleLamp();
    }
  }

  else if (Equals(DOOR_TOPIC, topic))
  {

    auto payloadEqualsOpen = Equals(HA_OPEN, payload);
    auto payloadEqualsClose = Equals(HA_CLOSE, payload);
    auto payloadEqualsStop = Equals(HA_STOP, payload);
    auto payloadEqualsHalf = Equals(HA_HALF, payload);

    if (payloadEqualsOpen)
    {
      emulator.openDoor();
    }
    else if (payloadEqualsClose)
    {
      emulator.closeDoor();
    }
    else if (payloadEqualsStop)
    {
      emulator.stopDoor();
    }
    else if (payloadEqualsHalf)
    {
      emulator.openDoorHalf();
    }

    else
    {
      strcpy(lastCommandTopic, "UNKNOWN DOOR COMMAND");
      strcpy(lastCommandPayload, payload);
    }
  }

  const SHCIState &doorstate = emulator.getState();
  onStatusChanged(doorstate);
}

void sendOnline()
{
  mqttClient.publish(AVAILABILITY_TOPIC, 0, true, HA_ONLINE);
}

void setWill()
{
  mqttClient.setWill(AVAILABILITY_TOPIC, 0, true, HA_OFFLINE);
}

void sendDiscoveryMessageForBinarySensor(const char name[], const char topic[], const char off[], const char on[])
{

  char full_topic[64];
  sprintf(full_topic, HA_DISCOVERY_BIN_SENSOR, topic);

  char uid[64];
  sprintf(uid, "garagedoor_binary_sensor_%s", topic);

  char vtemp[64];
  sprintf(vtemp, "{{ value_json.%s }}", topic);

  DynamicJsonDocument doc(1024);

  doc["name"] = name;
  doc["state_topic"] = STATE_TOPIC;
  doc["availability_topic"] = AVAILABILITY_TOPIC;
  doc["payload_available"] = HA_ONLINE;
  doc["payload_not_available"] = HA_OFFLINE;
  doc["unique_id"] = uid;
  doc["value_template"] = vtemp;
  doc["payload_on"] = on;
  doc["payload_off"] = off;

  JsonObject device = doc.createNestedObject("device");
  device["identifiers"] = "Garage Door";
  device["name"] = "Garage Door";
  device["sw_version"] = HA_VERSION;
  device["model"] = "Garage Door";
  device["manufacturer"] = "Hörmann";

  char payload[1024];
  serializeJson(doc, payload);
  //-//Serial.write(payload);
  mqttClient.publish(full_topic, 1, true, payload);
}

void sendDiscoveryMessageForAVSensor()
{
  DynamicJsonDocument doc(1024);

  doc["name"] = "Garage Door Available";
  doc["state_topic"] = AVAILABILITY_TOPIC;
  doc["unique_id"] = "garagedoor_sensor_availability";

  JsonObject device = doc.createNestedObject("device");
  device["identifiers"] = "Garage Door";
  device["name"] = "Garage Door";
  device["sw_version"] = HA_VERSION;
  device["model"] = "Garage Door";
  device["manufacturer"] = "Hörmann";

  char payload[1024];
  serializeJson(doc, payload);
  //-//Serial.write(payload);
  mqttClient.publish(HA_DISCOVERY_AV_SENSOR, 1, true, payload);
}

void sendDiscoveryMessageForSensor(const char name[], const char topic[])
{

  char full_topic[64];
  sprintf(full_topic, HA_DISCOVERY_SENSOR, topic);

  char uid[64];
  sprintf(uid, "garagedoor_sensor_%s", topic);

  char vtemp[64];
  sprintf(vtemp, "{{ value_json.%s }}", topic);

  DynamicJsonDocument doc(1024);

  doc["name"] = name;
  doc["state_topic"] = STATE_TOPIC;
  doc["availability_topic"] = AVAILABILITY_TOPIC;
  doc["payload_available"] = HA_ONLINE;
  doc["payload_not_available"] = HA_OFFLINE;
  doc["unique_id"] = uid;
  doc["value_template"] = vtemp;

  JsonObject device = doc.createNestedObject("device");
  device["identifiers"] = "Garage Door";
  device["name"] = "Garage Door";
  device["sw_version"] = HA_VERSION;
  device["model"] = "Garage Door";
  device["manufacturer"] = "Hörmann";

  char payload[1024];
  serializeJson(doc, payload);
  //-//Serial.write(payload);
  mqttClient.publish(full_topic, 1, true, payload);
}

void sendDiscoveryMessageForSwitch(const char name[], const char topic[], const char off[], const char on[], bool optimistic = false)
{
  char command_topic[64];
  sprintf(command_topic, CMD_TOPIC "/%s", topic);

  char full_topic[64];
  sprintf(full_topic, HA_DISCOVERY_SWITCH, topic);

  char value_template[64];
  sprintf(value_template, "{{ value_json.%s }}", topic);

  char uid[64];
  sprintf(uid, "garagedoor_switch_%s", topic);

  DynamicJsonDocument doc(1024);

  doc["name"] = name;
  doc["state_topic"] = STATE_TOPIC;
  doc["command_topic"] = command_topic;
  doc["payload_on"] = on;
  doc["payload_off"] = off;
  doc["icon"] = "mdi:lightbulb";
  doc["availability_topic"] = AVAILABILITY_TOPIC;
  doc["payload_available"] = HA_ONLINE;
  doc["payload_not_available"] = HA_OFFLINE;
  doc["unique_id"] = uid;
  doc["value_template"] = value_template;
  doc["optimistic"] = optimistic;

  JsonObject device = doc.createNestedObject("device");
  device["identifiers"] = "Garage Door";
  device["name"] = "Garage Door";
  device["sw_version"] = HA_VERSION;
  device["model"] = "Garage Door";
  device["manufacturer"] = "Hörmann";

  char payload[1024];
  serializeJson(doc, payload);
  //-//Serial.write(payload);
  mqttClient.publish(full_topic, 1, true, payload);
}

void sendDiscoveryMessageForCover(const char name[], const char topic[])
{

  char command_topic[64];
  sprintf(command_topic, CMD_TOPIC "/%s", topic);

  char full_topic[64];
  sprintf(full_topic, HA_DISCOVERY_COVER, topic);

  char uid[64];
  sprintf(uid, "garagedoor_cover_%s", topic);

  char value_template[64];
  sprintf(value_template, "{{ value_json.%s }}", topic);

  DynamicJsonDocument doc(1024);

  doc["name"] = name;
  doc["state_topic"] = STATE_TOPIC;
  doc["command_topic"] = command_topic;
  doc["position_topic"] = POS_TOPIC;

  doc["payload_open"] = HA_OPEN;
  doc["payload_close"] = HA_CLOSE;
  doc["payload_stop"] = HA_STOP;
  doc["value_template"] = value_template;
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

  JsonObject device = doc.createNestedObject("device");
  device["identifiers"] = "Garage Door";
  device["name"] = "Garage Door";
  device["sw_version"] = HA_VERSION;
  device["model"] = "Garage Door";
  device["manufacturer"] = "Hörmann";

  char payload[1024];
  serializeJson(doc, payload);
  //-//Serial.write(payload);
  mqttClient.publish(full_topic, 1, true, payload);
}

void sendDiscoveryMessage()
{
  sendDiscoveryMessageForAVSensor();

  sendDiscoveryMessageForSwitch("Garage Door Light", "lamp", HA_OFF, HA_ON);
  sendDiscoveryMessageForBinarySensor("Garage Door Light", "lamp", HA_OFF, HA_ON);

  sendDiscoveryMessageForCover("Garage Door", "door");

  sendDiscoveryMessageForSensor("Garage Door Status", "doorstate");
  sendDiscoveryMessageForSensor("Garage Door Position", "doorposition");
}

void onMqttConnect(bool sessionPresent)
{
  mqttConnected = true;

  sendOnline();
  mqttClient.subscribe(CMD_TOPIC "/#", 1);
  const SHCIState &doorstate = emulator.getState();
  onStatusChanged(doorstate);
  sendDiscoveryMessage();
}

void mqttTaskFunc(void *parameter)
{
  while (true)
  {
    if (WiFi.isConnected())
    {
      if (!mqttConnected)
      {
        vTaskDelay(5000);

        mqttClient.onConnect(onMqttConnect);
        mqttClient.onDisconnect(onMqttDisconnect);
        mqttClient.onMessage(onMqttMessage);
        mqttClient.onPublish(onMqttPublish);
        mqttClient.setServer(MQTTSERVER, MQTTPORT);
        mqttClient.setCredentials(MQTTUSER, MQTTPASSWORD);
        setWill();
        connectToMqtt();
      }
      else
      {
        const SHCIState &new_doorstate = emulator.getState();
        // onyl send updates when state changed
        if(new_doorstate.doorState != doorstate.doorState || new_doorstate.lampOn != doorstate.lampOn || new_doorstate.doorCurrentPosition != doorstate.doorCurrentPosition || new_doorstate.doorTargetPosition != doorstate.doorTargetPosition){
          //const SHCIState &doorstate = emulator.getState();
          doorstate = new_doorstate;  //copy new states
          onStatusChanged(doorstate);
        }
        vTaskDelay(READ_DELAY);     // delay task xxx ms
      }
    }
  }
}

TaskHandle_t mqttTask;

void modBusPolling(void *parameter)
{
  while (true)
  {
    if (lastCall > 0)
    {
      maxPeriod = _max(micros() - lastCall, maxPeriod);
    }
    lastCall = micros();
    emulator.poll();
    vTaskDelay(1);
  }
  vTaskDelete(NULL);
}

TaskHandle_t modBusTask;

void SensorCheck(){
    DynamicJsonDocument doc(1024);    //2048 needed because of BME280 float values!
    char payload[1024];
    bool changed = false;
    #ifdef USE_DS18X20
      ds18x20_temp = sensors.getTempCByIndex(0);
      if (abs(ds18x20_temp-ds18x20_last_temp) >= temp_threshold){
        doc["ds18x20"] = ds18x20_temp;
        ds18x20_last_temp = ds18x20_temp; 
        changed = true;
      }
    #endif
    #ifdef USE_BME
      if (!bme_status) {
        doc["bme_status"] = "Could not find a valid BME280 sensor!";   // see: https://github.com/adafruit/Adafruit_BME280_Library/blob/master/examples/bme280test/bme280test.ino#L49
        //bme_status = bme.begin(0x76, &I2CBME);  // check sensor. adreess can be 0x76 or 0x77
      }
      bme_temp = bme.readTemperature();   // round float
      bme_hum = bme.readHumidity();
      bme_pres = bme.readPressure()/100;  // convert from pascal to mbar
      if (abs(bme_temp-bme_last_temp) >= temp_threshold || abs(bme_hum-bme_last_hum) >= hum_threshold || abs(bme_pres-bme_last_pres) >= pres_threshold){
        char buf[20];
        dtostrf(bme_temp,2,2,buf);    // convert to string
        doc["bme_temp"] = buf;
        dtostrf(bme_hum,2,2,buf);    // convert to string
        doc["bme_hum"] = buf;
        dtostrf(bme_pres,2,1,buf);    // convert to string
        doc["bme_pres"] = buf;
        bme_last_temp = bme_temp;
        bme_last_hum = bme_hum;
        bme_last_pres = bme_pres;
        changed = true;
      }
    #endif
    if (changed){
      serializeJson(doc, payload);
      mqttClient.publish(SENSOR_TOPIC, 1, false, payload);  //uint16_t publish(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr, size_t length = 0)
    }
  vTaskDelay(SENSE_PERIOD);     // delay task xxx ms
}

TaskHandle_t sensorTask;

// setup mcu
void setup()
{
  // setup modbus
  RS485.begin(57600, SERIAL_8E1, PIN_RXD, PIN_TXD);
  #ifdef SWAPUART
    RS485.swap();
  #endif

  strcpy(lastCommandTopic, "topic");
  strcpy(lastCommandPayload, "payload");
  xTaskCreatePinnedToCore(
      modBusPolling, /* Function to implement the task */
      "ModBusTask",  /* Name of the task */
      10000,         /* Stack size in words */
      NULL,          /* Task input parameter */
      // 1,  /* Priority of the task */
      configMAX_PRIORITIES - 1,
      &modBusTask, /* Task handle. */
      1);          /* Core where the task should run */

  // setup wifi
  WiFi.setHostname(HOSTNAME);
  AsyncWiFiManager wifiManager(&server,&dns);
  wifiManager.autoConnect("HCPBridge",AP_PASSWD); // password protected ap

  xTaskCreatePinnedToCore(
      mqttTaskFunc, /* Function to implement the task */
      "MqttTask",   /* Name of the task */
      10000,        /* Stack size in words */
      NULL,         /* Task input parameter */
      // 1,  /* Priority of the task */
      configMAX_PRIORITIES - 2,
      &mqttTask, /* Task handle. */
      1);        /* Core where the task should run */


  #ifdef SENSORS
    #ifdef USE_DS18X20
      // Start the DS18B20 sensor
      sensors.begin();
    #endif
    #ifdef USE_BME
      I2CBME.begin(I2C_SDA, I2C_SCL, 400000);   // https://randomnerdtutorials.com/esp32-i2c-communication-arduino-ide/
      bme_status = bme.begin(0x76, &I2CBME);  // check sensor. adreess can be 0x76 or 0x77
    #endif
      xTaskCreatePinnedToCore(
      SensorCheck, /* Function to implement the task */
      "SensorTask",   /* Name of the task */
      10000,        /* Stack size in words */
      NULL,         /* Task input parameter */
      // 1,  /* Priority of the task */
      configMAX_PRIORITIES - 3,
      &sensorTask, /* Task handle. */
      1);        /* Core where the task should run */
  #endif


  // setup http server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html, sizeof(index_html));
              response->addHeader("Content-Encoding", "deflate");
              request->send(response); });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              const SHCIState &doorstate = emulator.getState();
              AsyncResponseStream *response = request->beginResponseStream("application/json");
              DynamicJsonDocument root(1024);
              root["valid"] = doorstate.valid;
              root["doorstate"] = doorstate.doorState;
              root["doorposition"] = doorstate.doorCurrentPosition;
              root["doortarget"] = doorstate.doorTargetPosition;
              root["lamp"] = doorstate.lampOn;
              //root["debug"] = doorstate.reserved;
              root["lastresponse"] = emulator.getMessageAge() / 1000;
              root["looptime"] = maxPeriod;

              lastCall = maxPeriod = 0;

              serializeJson(root, *response);
              request->send(response); });

  server.on("/command", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              if (request->hasParam("action"))
              {
                int actionid = request->getParam("action")->value().toInt();
                switch (actionid)
                {
                case 0:
                  emulator.closeDoor();
                  break;
                case 1:
                  emulator.openDoor();
                  break;
                case 2:
                  emulator.stopDoor();
                  break;
                case 3:
                  emulator.ventilationPosition();
                  break;
                case 4:
                  emulator.openDoorHalf();
                  break;
                case 5:
                  emulator.toggleLamp();
                  break;
                case 6:
                  setWill();
                  ESP.restart();
                  break;
                default:
                  break;
                }
              }
              request->send(200, "text/plain", "OK");
              const SHCIState &doorstate = emulator.getState();
              onStatusChanged(doorstate); });

  server.on("/sysinfo", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              AsyncResponseStream *response = request->beginResponseStream("application/json");
              DynamicJsonDocument root(1024);
              root["freemem"] = ESP.getFreeHeap();
              root["hostname"] = WiFi.getHostname();
              root["ip"] = WiFi.localIP().toString();
              root["wifistatus"] = WiFi.status();
              root["resetreason"] = esp_reset_reason();
              serializeJson(root, *response);

              request->send(response); });

  AsyncElegantOTA.begin(&server, OTA_USERNAME, OTA_PASSWD);

  server.begin();

  // setup relay board
  #ifdef USERELAY
    pinMode(ESP8266_GPIO4, OUTPUT);       // Relay control pin.
    pinMode(ESP8266_GPIO5, INPUT_PULLUP); // Input pin.
    pinMode(LED_PIN, OUTPUT);             // ESP8266 module blue L
    digitalWrite(ESP8266_GPIO4, 0);
    digitalWrite(LED_PIN, 0);
    emulator.onStatusChanged(onStatusChanged);
  #endif

  // read sensors initially
  #ifdef SENSORS
    #if defined(USE_DS18X20) || defined(USE_BME)
      SensorCheck();
    #endif
  #endif
}

// mainloop
void loop()
{
}
