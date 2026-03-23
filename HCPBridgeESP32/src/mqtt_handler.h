#pragma once

#include <Arduino.h>
#include <AsyncMqttClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
extern "C" {
    #include "freertos/FreeRTOS.h"
    #include "freertos/timers.h"
}

#include "configuration.h"
#include "preferences_handler.h"
#include "sensor_manager.h"
#include "hoermann.h"

// ============================================================================
// MQTT Strings - Topic management
// ============================================================================

class MqttStrings {
public:
    char availability_topic[64];
    char state_topic[64];
    char cmd_topic[64];
    char pos_topic[64];
    char setpos_topic[64];
    char lamp_topic[64];
    char door_topic[64];
    char vent_topic[64];
    char half_topic[64];
    char step_topic[64];
    char sensor_topic[64];
    char debug_topic[64];
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
    String st_half_topic;
    String st_step_topic;
    String st_sensor_topic;
    String st_debug_topic;
};

// ============================================================================
// MqttHandler Class
// ============================================================================

class MqttHandler {
public:
    // Initialize MQTT - call after preferences and WiFi are set up
    void begin(Preferences* prefs, PreferenceHandler* prefHandler, SensorManager* sensorMgr);

    // Get the MQTT client (for external use like WiFi event handlers)
    AsyncMqttClient& getClient() { return _mqttClient; }

    // Get MQTT strings (for external use)
    MqttStrings& getStrings() { return _mqttStrings; }

    // Connection state
    volatile bool isConnected() const { return _mqttConnected; }

    // Connect/disconnect
    void connectToMqtt();

    // Update door status - call from MQTT task
    void updateDoorStatus(bool forceUpdate = false);

    // Update sensor data - call from MQTT task
    void updateSensors(bool forceUpdate = false);

    // Send online availability
    void sendOnline();

    // Set Last Will Testament
    void setWill();

    // Send debug info
    void sendDebug();

    // Publish HC-SR501 motion state immediately
    void publishMotionState(int state);

    // MQTT task function (FreeRTOS)
    void taskFunc();

    // Reconnect timer
    TimerHandle_t mqttReconnectTimer = nullptr;

    // Last command tracking (for /status endpoint)
    char lastCommandTopic[64];
    char lastCommandPayload[64];

    // Last door state for step command logic
    HoermannState::State lastDoorState;

private:
    void setupMqttStrings();

    // MQTT callbacks
    void onConnect(bool sessionPresent);
    void onDisconnect(AsyncMqttClientDisconnectReason reason);
    void onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
    void onPublish(uint16_t packetId);

    // Discovery messages
    void sendDiscoveryMessage();
    void sendDiscoveryMessageForBinarySensor(const char name[], const char topic[], const char key[], const char off[], const char on[], const JsonDocument& device);
    void sendDiscoveryMessageForAVSensor(const JsonDocument& device);
    void sendDiscoveryMessageForSensor(const char name[], const char topic[], const char key[], const JsonDocument& device, const char device_class[] = "", const char unit[] = "");
    void sendDiscoveryMessageForSwitch(const char name[], const char discovery[], const char topic[], const char off[], const char on[], const char icon[], const JsonDocument& device, bool optimistic = false);
    void sendDiscoveryMessageForCover(const char name[], const char topic[], const JsonDocument& device);

    // Helper
    static const char* ToHA(bool value);

    // Members
    AsyncMqttClient _mqttClient;
    MqttStrings _mqttStrings;
    volatile bool _mqttConnected = false;
    Preferences* _prefs = nullptr;
    PreferenceHandler* _prefHandler = nullptr;
    SensorManager* _sensorMgr = nullptr;

    unsigned long _sensorLastUpdate = 0;
    int _sensorForceUpdateInterval = 7200000;  // 2 hours

    bool _bootFlag = true;
    bool _discoveryPending = false;
};
