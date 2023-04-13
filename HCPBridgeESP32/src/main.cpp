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

// mqtt
volatile bool mqttConnected;
AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

char lastCommandTopic[64];
char lastCommandPayload[64];

#ifdef DEBUG_REBOOT
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
      emulator.toggleLamp();
    }
  }

  else if (strcmp(DOOR_TOPIC, topic) == 0)
  {
    if (strncmp(payload, HA_OPEN, len) == 0)
    {
      emulator.openDoor();
    }
    else if (strncmp(payload, HA_CLOSE, len) == 0)
    {
      emulator.closeDoor();
    }
    else if (strncmp(payload, HA_STOP, len) == 0)
    {
      emulator.stopDoor();
    }
    else if (strncmp(payload, HA_HALF, len) == 0)
    {
      emulator.openDoorHalf();
    }
  }

  else if (strcmp(SETPOS_TOPIC, topic) == 0)
  {
    emulator.setPosition(atoi(lastCommandPayload));
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

void sendDebug()
{
  DynamicJsonDocument doc(1024);
  char payload[1024];
  doc["reset-reason"] = esp_reset_reason();
  serializeJson(doc, payload);
  mqttClient.publish(DEBUGTOPIC, 0, false, payload);  //uint16_t publish(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr, size_t length = 0)
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

void sendDiscoveryMessageForSensor(const char name[], const char topic[], const char key[])
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

  DynamicJsonDocument doc(1024);

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

  sendDiscoveryMessageForSensor("Garage Door Status", STATE_TOPIC, "doorstate");
  sendDiscoveryMessageForSensor("Garage Door Position", STATE_TOPIC, "doorposition");
  #ifdef SENSORS
    #if defined(USE_BME)
      sendDiscoveryMessageForSensor("Garage Temperature", SENSOR_TOPIC, "temp");
      sendDiscoveryMessageForSensor("Garage Humidity", SENSOR_TOPIC, "hum");
      sendDiscoveryMessageForSensor("Garage ambient pressure", SENSOR_TOPIC, "pres");
    #elif defined(USE_DS18X20)
      sendDiscoveryMessageForSensor("Garage Temperature", SENSOR_TOPIC, "temp");
    #endif
  #endif
}

void onMqttConnect(bool sessionPresent)
{
  mqttConnected = true;

  sendOnline();
  mqttClient.subscribe(CMD_TOPIC "/#", 1);
  const SHCIState &doorstate = emulator.getState();
  onStatusChanged(doorstate);
  sendDiscoveryMessage();
  #ifdef DEBUG_REBOOT
    if (boot_Flag){
      sendDebug();
      boot_Flag = false;
    }
  #endif
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
        #ifdef SENSORS
          if (new_sensor_data) {
            #ifdef USE_DS18X20
              DynamicJsonDocument doc(1024);    //2048 needed because of BME280 float values!
              char payload[1024];
              char buf[20];
              dtostrf(ds18x20_temp,2,2,buf);    // convert to string
              //Serial.println("Temp: "+ (String)buf);
              doc["temp"] = buf;
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
            serializeJson(doc, payload);
            mqttClient.publish(SENSOR_TOPIC, 0, false, payload);  //uint16_t publish(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr, size_t length = 0)
            new_sensor_data = false;
          #endif
          }
        #endif
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
    vTaskDelay(SENSE_PERIOD);     // delay task xxx ms if statemachine had nothing to do
  }
}

TaskHandle_t sensorTask;

// setup mcu
void setup()
{
  // Serial
  Serial.begin(9600);

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
  wifiManager.setDebugOutput(false);    // disable serial debug output
  wifiManager.autoConnect("HCPBridge",AP_PASSWD); // password protected ap

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

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              //const SHCIState &doorstate = emulator.getState();
              AsyncResponseStream *response = request->beginResponseStream("application/json");
              DynamicJsonDocument root(1024);
              root["valid"] = doorstate.valid;
              root["doorstate"] = doorstate.doorState;
              root["doorposition"] = doorstate.doorCurrentPosition;
              root["doortarget"] = doorstate.doorTargetPosition;
              root["lamp"] = doorstate.lampOn;
              #ifdef SENSORS
                #ifdef USE_DS18X20
                  root["temp"] = ds18x20_temp;
                #elif defined(USE_BME)
                  root["temp"] = bme_temp;
                #endif
              #endif
              //root["debug"] = doorstate.reserved;
              root["lastresponse"] = emulator.getMessageAge() / 1000;
              root["looptime"] = maxPeriod;
              root["lastCommandTopic"] = lastCommandTopic;
              root["lastCommandPayload"] = lastCommandPayload;
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
                  Serial.println("Starte neu...");
                  setWill();
                  ESP.restart();
                  break;
                case 7:
                  if (request->hasParam("position"))
                    emulator.setPosition(request->getParam("position")->value().toInt());
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
void loop()
{
}
