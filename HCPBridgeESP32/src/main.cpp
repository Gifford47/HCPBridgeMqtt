#include <Arduino.h>
#include <AsyncElegantOTA.h>
#include <ESPAsyncWebServer.h>
#include "AsyncJson.h"
#include <AsyncMqttClient.h>
#include <Ticker.h>
#include "ArduinoJson.h"
#include "hciemulator.h"
#include "../../WebUI/index_html.h"
#include <sstream>
#include "configuration.h"

#ifdef USE_DS18X20
  #include <OneWire.h>
  #include <DallasTemperature.h>
#endif

// switch relay sync to the lamp
// e.g. the Wifi Relay Board U4648
//#define USERELAY

// use alternative uart pins
//#define SWAPUART

#define RS485 Serial

// Relay Board parameters
#define ESP8266_GPIO2 2 // Blue LED.
#define ESP8266_GPIO4 4 // Relay control.
#define ESP8266_GPIO5 5 // Optocoupler input.
#define LED_PIN ESP8266_GPIO2

// Hörmann HCP2 based on modbus rtu @57.6kB 8E1
HCIEmulator emulator(&RS485);

// webserver on port 80
AsyncWebServer server(80);

#ifdef USE_DS18X20
  // Setup a oneWire instance to communicate with any OneWire devices
  OneWire oneWire(oneWireBus);
  DallasTemperature sensors(&oneWire);
  float ds18x20_temp = -99.99;
  float ds18x20_last_temp = -99.99;
  unsigned long lastMillis = 0L ;
#endif

// mqtt
volatile bool mqttConnected;
AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

const char *HA_ON = "true";
const char *HA_OFF = "false";

const char *HA_OPEN = "open";
const char *HA_CLOSE = "close";
const char *HA_HALF = "half";
const char *HA_STOP = "stop";
const char *HA_OPENING = "opening";
const char *HA_CLOSING = "closing";
const char *HA_CLOSED = "closed";
const char *HA_OPENED = "open";

const char *HA_ONLINE = "online";
const char *HA_OFFLINE = "offline";
const char *AVAILABILITY_TOPIC = "home/garage/door/availability";

const char *HA_VERSION = "0.0.2.0";

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
  // see https://ucexperiment.wordpress.com/2016/12/18/yunshan-esp8266-250v-15a-acdc-network-wifi-relay-module/
  // Setting GPIO4 high, causes the relay to close the NO contact with
  if (state.valid)
  {
    digitalWrite(ESP8266_GPIO4, state.lampOn);
    // digitalWrite(LED_PIN, false);
  }
  else
  {
    digitalWrite(ESP8266_GPIO4, false);
    // digitalWrite(LED_PIN, true);
  }

  if (mqttConnected == true)
  {
    DynamicJsonDocument doc(1024);
    doc["valid"] = ToHA(state.valid);
    doc["doorstate"] = state.doorState;
    doc["doorposition"] = state.doorCurrentPosition;
    doc["doortarget"] = state.doorTargetPosition;
    doc["lamp"] = ToHA(state.lampOn);
    doc["debug"] = ToHA(state.reserved);
    // doc["lastresponse"] = emulator.getMessageAge() / 1000;
    // doc["looptime"] = maxPeriod;
    // doc["lastCommandTopic"] = lastCommandTopic;
    // doc["lastCommandPayload"] = lastCommandPayload;
    switch (state.doorState)
    {
    case DOOR_OPEN_POSITION:
      doc["door"] = HA_OPENED;
      break;
    case DOOR_CLOSE_POSITION:
      doc["door"] = HA_CLOSED;
      break;
    case DOOR_HALF_POSITION:
      doc["door"] = HA_HALF;
      break;
    case DOOR_MOVE_CLOSEPOSITION:
      doc["door"] = HA_CLOSING;
      break;
    case DOOR_MOVE_OPENPOSITION:
      doc["door"] = HA_OPENING;
      break;
    default:
      doc["door"] = "UNKNOWN";
    }

    lastCall = maxPeriod = 0;
    char payload[1024];
    serializeJson(doc, payload);

    mqttClient.publish("home/garage/door/state", 1, true, payload);
  }
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
  // strcpy(lastCommandTopic,topic);
  // strcpy(lastCommandPayload, payload);

  auto payloadEqualsOn = Equals(HA_ON, payload);
  auto payloadEqualsOff = Equals(HA_OFF, payload);

  if (Equals("home/garage/door/command/lamp", topic))
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
  else if (Equals("home/garage/door/command/door", topic))
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
      emulator.stopDoor();
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
  sprintf(full_topic, "homeassistant/binary_sensor/garage/%s/config", topic);

  char uid[64];
  sprintf(uid, "garagedoor_binary_sensor_%s", topic);

  char vtemp[64];
  sprintf(vtemp, "{{ value_json.%s }}", topic);

  DynamicJsonDocument doc(1024);

  doc["name"] = name;
  doc["state_topic"] = "home/garage/door/state";
  doc["availability_topic"] = AVAILABILITY_TOPIC;
  doc["payload_available"] = HA_ONLINE;
  doc["payload_not_available"] = HA_OFFLINE;
  doc["unique_id"] = uid;
  doc["value_template"] = vtemp;
  doc["payload_on"] = on;
  doc["payload_off"] = off;

  JsonObject device = doc.createNestedObject("device");
  device["identifiers"] = "Garagentor";
  device["name"] = "Garagentor";
  device["sw_version"] = HA_VERSION;
  device["model"] = "Garagentor";
  device["manufacturer"] = "Andreas Strauß";

  char payload[1024];
  serializeJson(doc, payload);
  //-//Serial.write(payload);
  mqttClient.publish(full_topic, 1, true, payload);
}

void sendDiscoveryMessageForAVSensor()
{
  DynamicJsonDocument doc(1024);

  doc["name"] = "Available";
  doc["state_topic"] = AVAILABILITY_TOPIC;
  doc["unique_id"] = "garagedoor_sensor_availability";

  JsonObject device = doc.createNestedObject("device");
  device["identifiers"] = "Garagentor";
  device["name"] = "Garagentor";
  device["sw_version"] = HA_VERSION;
  device["model"] = "Garagentor";
  device["manufacturer"] = "Andreas Strauß";

  char payload[1024];
  serializeJson(doc, payload);
  //-//Serial.write(payload);
  mqttClient.publish("homeassistant/sensor/garage/available/config", 1, true, payload);
}

void sendDiscoveryMessageForSensor(const char name[], const char topic[])
{

  char full_topic[64];
  sprintf(full_topic, "homeassistant/sensor/garage/%s/config", topic);

  char uid[64];
  sprintf(uid, "garagedoor_sensor_%s", topic);

  char vtemp[64];
  sprintf(vtemp, "{{ value_json.%s }}", topic);

  DynamicJsonDocument doc(1024);

  doc["name"] = name;
  doc["state_topic"] = "home/garage/door/state";
  doc["availability_topic"] = AVAILABILITY_TOPIC;
  doc["payload_available"] = HA_ONLINE;
  doc["payload_not_available"] = HA_OFFLINE;
  doc["unique_id"] = uid;
  doc["value_template"] = vtemp;

  JsonObject device = doc.createNestedObject("device");
  device["identifiers"] = "Garagentor";
  device["name"] = "Garagentor";
  device["sw_version"] = HA_VERSION;
  device["model"] = "Garagentor";
  device["manufacturer"] = "Andreas Strauß";

  char payload[1024];
  serializeJson(doc, payload);
  //-//Serial.write(payload);
  mqttClient.publish(full_topic, 1, true, payload);
}

void sendDiscoveryMessageForSwitch(const char name[], const char topic[], const char off[], const char on[], bool optimistic = false)
{
  char command_topic[64];
  sprintf(command_topic, "home/garage/door/command/%s", topic);

  char full_topic[64];
  sprintf(full_topic, "homeassistant/switch/garage/%s/config", topic);

  char value_template[64];
  sprintf(value_template, "{{ value_json.%s }}", topic);

  char uid[64];
  sprintf(uid, "garagedoor_switch_%s", topic);

  DynamicJsonDocument doc(1024);

  doc["name"] = name;
  doc["state_topic"] = "home/garage/door/state";
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
  device["identifiers"] = "Garagentor";
  device["name"] = "Garagentor";
  device["sw_version"] = HA_VERSION;
  device["model"] = "Garagentor";
  device["manufacturer"] = "Andreas Strauß";

  char payload[1024];
  serializeJson(doc, payload);
  //-//Serial.write(payload);
  mqttClient.publish(full_topic, 1, true, payload);
}

void sendDiscoveryMessageForCover(const char name[], const char topic[])
{

  char command_topic[64];
  sprintf(command_topic, "home/garage/door/command/%s", topic);

  char full_topic[64];
  sprintf(full_topic, "homeassistant/cover/garage/%s/config", topic);

  char uid[64];
  sprintf(uid, "garagedoor_cover_%s", topic);

  char value_template[64];
  sprintf(value_template, "{{ value_json.%s }}", topic);

  DynamicJsonDocument doc(1024);

  doc["name"] = name;
  doc["state_topic"] = "home/garage/door/state";
  doc["command_topic"] = command_topic;

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

  JsonObject device = doc.createNestedObject("device");
  device["identifiers"] = "Garagentor";
  device["name"] = "Garagentor";
  device["sw_version"] = HA_VERSION;
  device["model"] = "Garagentor";
  device["manufacturer"] = "Andreas Strauß";

  char payload[1024];
  serializeJson(doc, payload);
  //-//Serial.write(payload);
  mqttClient.publish(full_topic, 1, true, payload);
}

void sendDiscoveryMessage()
{
  sendDiscoveryMessageForAVSensor();

  sendDiscoveryMessageForSwitch("Garagentor Lampe", "lamp", HA_OFF, HA_ON);
  sendDiscoveryMessageForBinarySensor("Lampe", "lamp", HA_OFF, HA_ON);

  sendDiscoveryMessageForCover("Garagentor", "door");

  sendDiscoveryMessageForBinarySensor("Debug", "debug", HA_OFF, HA_ON);
  sendDiscoveryMessageForBinarySensor("Zustand", "valid", HA_OFF, HA_ON);

  sendDiscoveryMessageForSensor("Tor Status", "doorstate");
  sendDiscoveryMessageForSensor("Tor Position", "doorposition");
  sendDiscoveryMessageForSensor("Tor Ziel", "doortarget");
  //sendDiscoveryMessageForSensor("Last Response", "lastresponse");
  //sendDiscoveryMessageForSensor("Loop Time", "looptime");
  sendDiscoveryMessageForSensor("Tor", "door");
  //sendDiscoveryMessageForSensor("Last Command Topic", "lastCommandTopic");
  //sendDiscoveryMessageForSensor("Last Command Payload", "lastCommandPayload");
}

void onMqttConnect(bool sessionPresent)
{
  mqttConnected = true;

  if (sessionPresent)
  {
    //-//Serial.write("MQTT connected with sessionPresent=true");
  }
  else
  {
    //-//Serial.write("MQTT connected with sessionPresent=false");
  }

  sendOnline();
  mqttClient.subscribe("home/garage/door/command/#", 1);
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
        setWill();
        connectToMqtt();
      }
      else
      {
        const SHCIState &doorstate = emulator.getState();
        onStatusChanged(doorstate);
        vTaskDelay(6000);
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

// setup mcu
void setup()
{
  // setup modbus
  RS485.begin(57600, SERIAL_8E1);
  #ifdef SWAPUART
    RS485.swap();
  #endif
  #ifdef USE_DS18X20
    // Start the DS18B20 sensor
    sensors.begin();
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
  WiFi.setHostname("Garagentor");
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);

  WiFi.setAutoReconnect(true);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
  }

  xTaskCreatePinnedToCore(
      mqttTaskFunc, /* Function to implement the task */
      "MqttTask",   /* Name of the task */
      10000,        /* Stack size in words */
      NULL,         /* Task input parameter */
      // 1,  /* Priority of the task */
      configMAX_PRIORITIES - 1,
      &mqttTask, /* Task handle. */
      1);        /* Core where the task should run */

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
              root["debug"] = doorstate.reserved;
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

  AsyncElegantOTA.begin(&server);

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
}

// mainloop
void loop()
{
  AsyncElegantOTA.loop();
  #ifdef USE_DS18X20
    if (millis() - lastMillis >= SENSE_PERIOD){
      ds18x20_temp = sensors.getTempCByIndex(0);
      lastMillis = millis();
      if (abs(ds18x20_temp-ds18x20_last_temp) >= temp_threshold){
        DynamicJsonDocument json(1024);
        json["ds18x20"] = ds18x20_temp;
        mqttClient.publish("home/garage/door/temp", 1, true, json);  //uint16_t publish(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr, size_t length = 0)
      }
      ds18x20_last_temp = ds18x20_temp;
    }
  #endif
}
