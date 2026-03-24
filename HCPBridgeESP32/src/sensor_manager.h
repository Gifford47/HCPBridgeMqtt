#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>

#include "configuration.h"
#include "preferences_handler.h"

// ============================================================================
// Sensor Status
// ============================================================================

enum class SensorStatus {
    NOT_CONFIGURED,  // Not enabled or pin invalid
    ACTIVE,          // Enabled and initialized
    FAILED_DISABLED  // Too many consecutive errors (until reboot)
};

#define SENSOR_MAX_FAIL_COUNT 5

// ============================================================================
// Unified sensor data - single source of truth for all consumers
// ============================================================================

struct SensorEntry {
    const char* key;     // JSON key (e.g. "temp", "hum")
    char value[16];      // Formatted value string
    const char* unit;    // Unit suffix for WebUI (e.g. " °C")
    bool active;         // Sensor present (active or failed)
    bool failed;         // Sensor failed (shows last known value in red)
};

#define SENSOR_FIELD_COUNT 8

struct SensorData {
    SensorEntry fields[SENSOR_FIELD_COUNT] = {
        {"temp",      "", " \xC2\xB0""C", false, false},
        {"hum",       "", " %",           false, false},
        {"pres",      "", " mbar",        false, false},
        {"dist",      "", " cm",          false, false},
        {"free",      "", "",             false, false},
        {"motion",    "", "",             false, false},
        {"gas",       "", "",             false, false},
        {"gas_alarm", "", "",             false, false}
    };
};

// ============================================================================
// SensorManager Class
// ============================================================================

class SensorManager {
public:
    // Initialize enabled sensors - call in setup()
    void begin(Preferences* prefs);

    // Poll all active sensors - call in SensorCheck FreeRTOS task
    void poll();

    // Check if new data is available since last call
    bool hasNewData();
    void clearNewData();

    // Unified sensor data (updated by poll/begin)
    SensorData data;

    // Build JSON payload for MQTT sensor topic
    void toJson(JsonDocument& doc);

    // Build JSON for /status HTTP endpoint
    void toStatusJson(JsonObject& sensors);

    // Sensor status queries
    SensorStatus getBmeStatus() const { return _bmeStatus; }
    SensorStatus getDs18x20Status() const { return _ds18x20Status; }
    SensorStatus getHcsr04Status() const { return _hcsr04Status; }
    SensorStatus getDht22Status() const { return _dht22Status; }
    SensorStatus getHcsr501Status() const { return _hcsr501Status; }
    SensorStatus getMq4Status() const { return _mq4Status; }

    // Capability queries for MQTT discovery
    bool hasTempSensor() const;
    bool hasHumiditySensor() const;
    bool hasPressureSensor() const;
    bool hasDistanceSensor() const;
    bool hasMotionSensor() const;
    bool hasGasSensor() const;
    bool hasAnySensor() const;

    // Build JSON with sensor status for /sysinfo
    void toDetectionJson(JsonObject& sensors) const;

    // Thresholds
    float tempThreshold = 0;
    int humThreshold = 0;
    int presThreshold = 0;
    int proxThreshold = 0;
    int gasThreshold = 0;
    int forceUpdateInterval = 7200000;  // 2 hours in ms

    // HC-SR501 immediate publish
    bool hcsr501StateChanged() const { return _hcsr501Changed; }
    void clearHcsr501Changed() { _hcsr501Changed = false; }
    int getHcsr501State() const { return _hcsr501Stat; }

    // Sensors fully initialized (begin() complete)
    volatile bool isReady() const { return _ready; }

    // Last error string for debug entity (always active)
    String getLastError() const { return _lastError; }

private:
    // Validation
    bool validateTemperature(float temp);
    bool validateHumidity(float hum);
    bool validatePressure(float pres);
    bool validateBmeReading(float temp, float hum, float pres);
    bool validateDistance(int distCm);
    bool validateGasReading(int analogValue);

    // Init functions (called in begin() for enabled sensors)
    bool initBme(Preferences* prefs);
    bool initDs18x20(Preferences* prefs);
    bool initDht22(Preferences* prefs);
    bool initHcsr04(Preferences* prefs);
    bool initHcsr501(Preferences* prefs);
    bool initMq4(Preferences* prefs);

    // Poll functions
    void pollBme();
    void pollDs18x20();
    void pollDht22();
    void pollHcsr04();
    void pollHcsr501();
    void pollMq4();

    void disableSensor(const char* name, SensorStatus& status);
    void setError(const char* msg);
    void updateDataStrings();

    // Sensor states
    SensorStatus _bmeStatus = SensorStatus::NOT_CONFIGURED;
    SensorStatus _ds18x20Status = SensorStatus::NOT_CONFIGURED;
    SensorStatus _hcsr04Status = SensorStatus::NOT_CONFIGURED;
    SensorStatus _dht22Status = SensorStatus::NOT_CONFIGURED;
    SensorStatus _hcsr501Status = SensorStatus::NOT_CONFIGURED;
    SensorStatus _mq4Status = SensorStatus::NOT_CONFIGURED;

    // Fail counters
    int _bmeFailCount = 0;
    int _ds18x20FailCount = 0;
    int _dht22FailCount = 0;
    int _hcsr04FailCount = 0;
    int _mq4FailCount = 0;

    // Sensor hardware
    TwoWire _i2cBme = TwoWire(0);
    Adafruit_BME280 _bme;
    int _i2cSdaPin = 0;
    int _i2cSclPin = 0;

    OneWire* _oneWire = nullptr;
    DallasTemperature* _ds18x20 = nullptr;
    int _ds18x20Pin = 0;

    DHT* _dht = nullptr;
    int _dhtPin = 0;

    int _hcsr04TrigPin = 0;
    int _hcsr04EchoPin = 0;
    int _hcsr04MaxDistanceCm = SR04_MAXDISTANCECM;

    int _hcsr501Pin = 0;

    int _mq4AnalogPin = 0;
    int _mq4DigitalPin = 0;

    // Sensor values
    float _bmeTemp = -99.99, _bmeLastTemp = -99.99;
    float _bmeHum = -99.99, _bmeLastHum = -99.99;
    float _bmePres = -99.99, _bmeLastPres = -99.99;
    float _ds18x20Temp = -99.99, _ds18x20LastTemp = -99.99;
    float _dht22Temp = -99.99, _dht22LastTemp = -99.99;
    float _dht22Hum = -99.99, _dht22LastHum = -99.99;
    int _hcsr04DistanceCm = 0, _hcsr04LastDistanceCm = 0;
    int _hcsr04MaxMeasuredCm = 0;
    bool _hcsr04ParkAvailable = false, _hcsr04LastParkAvailable = false;
    int _hcsr501Stat = 0;
    bool _hcsr501LastStat = false;
    bool _hcsr501Changed = false;
    int _mq4AnalogValue = 0, _mq4LastAnalogValue = 0;
    bool _mq4DigitalAlarm = false, _mq4LastDigitalAlarm = false;

    bool _newSensorData = false;
    volatile bool _ready = false;
    unsigned long _lastUpdateTime = 0;
    String _lastError = "";
};
