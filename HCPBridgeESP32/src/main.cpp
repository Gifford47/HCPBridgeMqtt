#include <Arduino.h>
#include <WiFi.h>
#include <Esp.h>
#include <ElegantOTA.h>
#include <ESPAsyncWebServer.h>
#include "AsyncJson.h"
extern "C" {
	#include "freertos/FreeRTOS.h"
	#include "freertos/timers.h"
}
#include "ArduinoJson.h"

#include "hoermann.h"
#include "preferences_handler.h"
#include "sensor_manager.h"
#include "mqtt_handler.h"
#include "../WebUI/index_html.h"

// ============================================================================
// Global objects
// ============================================================================

AsyncWebServer server(80);
PreferenceHandler prefHandler;
Preferences *localPrefs = nullptr;
SensorManager sensorManager;
MqttHandler mqttHandler;

TimerHandle_t wifiReconnectTimer;

// ============================================================================
// Reset Button
// ============================================================================

TimerHandle_t resetTimer;

#define RESET_PIN 0
#define RESET_PRESS_COUNT 5
#define RESET_TIME_WINDOW 6000  // 6 seconds window for 5 presses
#define DEBOUNCE_DELAY 200      // 200ms debounce

volatile int pressCount = 0;
volatile unsigned long firstPressTime = 0;
volatile unsigned long lastPressTime = 0;
volatile bool resetTriggered = false;

void IRAM_ATTR reset_button_change() {
    unsigned long now = millis();

    if (now - lastPressTime < DEBOUNCE_DELAY) {
        return;
    }
    lastPressTime = now;

    if (pressCount == 0) {
        firstPressTime = now;
        pressCount = 1;
    } else {
        if (now - firstPressTime <= RESET_TIME_WINDOW) {
            pressCount++;
        } else {
            firstPressTime = now;
            pressCount = 1;
        }
    }

    if (pressCount >= RESET_PRESS_COUNT) {
        xTimerStartFromISR(resetTimer, NULL);
        resetTriggered = true;
        pressCount = 0;
        firstPressTime = 0;
        lastPressTime = 0;
    }
}

void resetPreferences() {
    xTimerStop(resetTimer, 0);
    Serial.println("Resetting config...");
    prefHandler.resetPreferences();
}

// ============================================================================
// WiFi
// ============================================================================

void connectToWifi() {
    if (localPrefs->getString(preference_wifi_ssid) != "") {
        Serial.println("Connecting to Wi-Fi...");
        WiFi.begin(localPrefs->getString(preference_wifi_ssid).c_str(), localPrefs->getString(preference_wifi_password).c_str(), 0, nullptr, true);
    } else {
        Serial.println("No WiFi Client enabled");
    }
}

void WiFiEvent(WiFiEvent_t event) {
    String eventInfo = "No Info";

    switch (event) {
        case ARDUINO_EVENT_WIFI_READY:
            eventInfo = "WiFi interface ready"; break;
        case ARDUINO_EVENT_WIFI_SCAN_DONE:
            eventInfo = "Completed scan for access points"; break;
        case ARDUINO_EVENT_WIFI_STA_START:
            eventInfo = "WiFi client started"; break;
        case ARDUINO_EVENT_WIFI_STA_STOP:
            eventInfo = "WiFi clients stopped"; break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            eventInfo = "Connected to access point"; break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            eventInfo = "Disconnected from WiFi access point";
            xTimerStop(mqttHandler.mqttReconnectTimer, 0);
            xTimerStart(wifiReconnectTimer, 0);
            break;
        case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
            eventInfo = "Authentication mode of access point has changed"; break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            eventInfo = "Obtained IP address";
            xTimerStop(wifiReconnectTimer, 0);
            xTimerStart(mqttHandler.mqttReconnectTimer, 0);
            break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
            eventInfo = "Lost IP address and IP address is reset to 0"; break;
        case ARDUINO_EVENT_WPS_ER_SUCCESS:
            eventInfo = "WiFi Protected Setup (WPS): succeeded in enrollee mode"; break;
        case ARDUINO_EVENT_WPS_ER_FAILED:
            eventInfo = "WiFi Protected Setup (WPS): failed in enrollee mode"; break;
        case ARDUINO_EVENT_WPS_ER_TIMEOUT:
            eventInfo = "WiFi Protected Setup (WPS): timeout in enrollee mode"; break;
        case ARDUINO_EVENT_WPS_ER_PIN:
            eventInfo = "WiFi Protected Setup (WPS): pin code in enrollee mode"; break;
        case ARDUINO_EVENT_WIFI_AP_START:
            eventInfo = "WiFi access point started"; break;
        case ARDUINO_EVENT_WIFI_AP_STOP:
            eventInfo = "WiFi access point  stopped"; break;
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            eventInfo = "Client connected"; break;
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            eventInfo = "Client disconnected"; break;
        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
            eventInfo = "Assigned IP address to client"; break;
        case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
            eventInfo = "Received probe request"; break;
        case ARDUINO_EVENT_WIFI_AP_GOT_IP6:
            eventInfo = "AP IPv6 is preferred"; break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
            eventInfo = "STA IPv6 is preferred"; break;
        case ARDUINO_EVENT_ETH_GOT_IP6:
            eventInfo = "Ethernet IPv6 is preferred"; break;
        case ARDUINO_EVENT_ETH_START:
            eventInfo = "Ethernet started"; break;
        case ARDUINO_EVENT_ETH_STOP:
            eventInfo = "Ethernet stopped"; break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            eventInfo = "Ethernet connected"; break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            eventInfo = "Ethernet disconnected"; break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            eventInfo = "Obtained IP address"; break;
        default: break;
    }
    Serial.print("WIFI-Event: ");
    Serial.println(eventInfo);
}

// ============================================================================
// Web Server Auth
// ============================================================================

bool requireAuth(AsyncWebServerRequest* request) {
    String webpass = localPrefs->getString(preference_www_password);
    if (webpass != "") {
        if (request->authenticate(WWW_USER, webpass.c_str())) return true;
        request->requestAuthentication();
        return false;
    } else {
        return true;
    }
}

// ============================================================================
// FreeRTOS Tasks
// ============================================================================

TaskHandle_t mqttTask;
TaskHandle_t sensorTask;

void mqttTaskFunc(void *parameter) {
    while (true) {
        mqttHandler.taskFunc();
        vTaskDelay(READ_DELAY);
    }
}

void sensorCheckTask(void *parameter) {
    while (true) {
        sensorManager.poll();

        // Handle HC-SR501 immediate publish
        if (sensorManager.hcsr501StateChanged()) {
            mqttHandler.publishMotionState(sensorManager.getHcsr501State());
            sensorManager.clearHcsr501Changed();
        }

        vTaskDelay(localPrefs->getInt(preference_query_interval_sensors) * 1000);
    }
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
    Serial.begin(9600);

    #ifdef IS_HCP_BOARD
    pinMode(LED1, OUTPUT);
    digitalWrite(LED1, HIGH);
    #endif

    // Setup preferences
    prefHandler.initPreferences();
    localPrefs = prefHandler.getPreferences();

    // Setup modbus
    hoermannEngine->setup(localPrefs);

    // Reset button
    pinMode(RESET_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(RESET_PIN), reset_button_change, FALLING);
    resetTimer = xTimerCreate(
        "resetTimer",
        pdMS_TO_TICKS(10),
        pdFALSE,
        (void*)0,
        reinterpret_cast<TimerCallbackFunction_t>(resetPreferences)
    );

    // Setup WiFi timers
    mqttHandler.mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0,
        reinterpret_cast<TimerCallbackFunction_t>(+[](){ mqttHandler.connectToMqtt(); }));
    wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0,
        reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

    WiFi.setHostname(prefHandler.getPreferencesCache()->hostname);
    if (localPrefs->getBool(preference_wifi_ap_mode)) {
        String apPass = localPrefs->getString(preference_wifi_ap_password);
        if (apPass.length() < 8) {
            apPass = AP_PASSWD;
        }
        WiFi.disconnect(true);
        delay(300);
        Serial.println("WIFI AP enabled");
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(prefHandler.getPreferencesCache()->hostname, apPass.c_str(), 1, false, 4);
    } else {
        WiFi.mode(WIFI_STA);
    }

    WiFi.onEvent(WiFiEvent);

    // Setup MQTT
    mqttHandler.begin(localPrefs, &prefHandler, &sensorManager);

    delay(1000);
    connectToWifi();

    // MQTT FreeRTOS task
    xTaskCreatePinnedToCore(
        mqttTaskFunc,
        "MqttTask",
        10000,
        NULL,
        configMAX_PRIORITIES - 3,
        &mqttTask,
        0);

    // Sensor auto-detection and initialization
    sensorManager.begin(localPrefs);

    // Sensor polling task (only if any sensor is active)
    if (sensorManager.hasAnySensor()) {
        xTaskCreatePinnedToCore(
            sensorCheckTask,
            "SensorTask",
            10000,
            NULL,
            configMAX_PRIORITIES,
            &sensorTask,
            0);
    }

    // ========================================================================
    // HTTP Server Routes
    // ========================================================================

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!requireAuth(request)) return;
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html, sizeof(index_html));
        response->addHeader("Content-Encoding", "deflate");
        request->send(response);
    });

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!requireAuth(request)) return;
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument root;
        root["doorstate"] = hoermannEngine->state->translatedState;
        root["valid"] = hoermannEngine->state->valid;
        root["targetPosition"] = (int)(hoermannEngine->state->targetPosition * 100);
        root["currentPosition"] = (int)(hoermannEngine->state->currentPosition * 100);
        root["light"] = hoermannEngine->state->lightOn;
        root["state"] = hoermannEngine->state->state;
        root["busResponseAge"] = hoermannEngine->state->responseAge();
        root["lastModbusRespone"] = hoermannEngine->state->lastModbusRespone;
        root["swversion"] = HA_VERSION;

        if (sensorManager.hasAnySensor()) {
            JsonObject sensors = root["sensors"].to<JsonObject>();
            sensorManager.toStatusJson(sensors);
        }

        root["lastCommandTopic"] = mqttHandler.lastCommandTopic;
        root["lastCommandPayload"] = mqttHandler.lastCommandPayload;
        serializeJson(root, *response);
        request->send(response);
    });

    server.on("/statush", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!requireAuth(request)) return;
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        response->print(hoermannEngine->state->toStatusJson());
        request->send(response);
    });

    server.on("/command", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!requireAuth(request)) return;
        if (request->hasParam("action")) {
            int actionid = request->getParam("action")->value().toInt();
            switch (actionid) {
                case 0: hoermannEngine->closeDoor(); break;
                case 1: hoermannEngine->openDoor(); break;
                case 2: hoermannEngine->stopDoor(); break;
                case 3: hoermannEngine->ventilationPositionDoor(); break;
                case 4: hoermannEngine->halfPositionDoor(); break;
                case 5: hoermannEngine->toogleLight(); break;
                case 6:
                    Serial.println("restart...");
                    mqttHandler.setWill();
                    ESP.restart();
                    break;
                case 7:
                    if (request->hasParam("position"))
                        hoermannEngine->setPosition(request->getParam("position")->value().toInt());
                    break;
                case 8: hoermannEngine->toogleDoor(); break;
                default: break;
            }
        }
        request->send(200, "text/plain", "OK");
    });

    server.on("/sysinfo", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!requireAuth(request)) return;
        Serial.println("GET SYSINFO");
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument root;
        root["freemem"] = ESP.getFreeHeap();
        root["hostname"] = WiFi.getHostname();
        root["ip"] = WiFi.localIP().toString();
        root["wifistatus"] = WiFi.status();
        root["mqttstatus"] = mqttHandler.getClient().connected();
        root["resetreason"] = esp_reset_reason();
        root["swversion"] = HA_VERSION;
        serializeJson(root, *response);
        request->send(response);
    });

    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!requireAuth(request)) return;
        Serial.println("GET CONFIG");
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument conf;
        prefHandler.getConf(conf);
        serializeJson(conf, *response);
        request->send(response);
    });

    server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (!requireAuth(request)) return;
        if (request->url() == "/config") {
            JsonDocument doc;
            deserializeJson(doc, data);
            prefHandler.saveConf(doc);
            request->send(200, "text/plain", "OK");
        }
    });

    server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!requireAuth(request)) return;
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
    #ifdef IS_HCP_BOARD
    digitalWrite(LED1, LOW);
    #endif
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
    ElegantOTA.loop();

    if (resetTriggered) {
        resetTriggered = false;
        #ifdef IS_HCP_BOARD
        for (int i = 0; i < 10; i++) {
            digitalWrite(LED1, !digitalRead(LED1));
            delay(100);
        }
        #endif
    }
}
