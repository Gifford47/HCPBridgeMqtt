#include "sensor_manager.h"

// ============================================================================
// Validation functions
// ============================================================================

bool SensorManager::validateTemperature(float temp) {
    if (isnan(temp) || isinf(temp)) return false;
    if (temp < -50.0f || temp > 80.0f) return false;
    return true;
}

bool SensorManager::validateHumidity(float hum) {
    if (isnan(hum) || isinf(hum)) return false;
    if (hum < 0.0f || hum > 100.0f) return false;
    return true;
}

bool SensorManager::validatePressure(float pres) {
    if (isnan(pres) || isinf(pres)) return false;
    if (pres < 300.0f || pres > 1100.0f) return false;
    return true;
}

bool SensorManager::validateBmeReading(float temp, float hum, float pres) {
    if (hum >= 99.9f) return false;
    return validateTemperature(temp) && validateHumidity(hum) && validatePressure(pres);
}

bool SensorManager::validateDistance(int distCm) {
    if (distCm <= 0) return false;
    if (distCm > _hcsr04MaxDistanceCm * 2) return false;
    return true;
}

bool SensorManager::validateGasReading(int analogValue) {
    if (analogValue <= 0 || analogValue >= 4095) return false;
    return true;
}

// ============================================================================
// Helpers
// ============================================================================

void SensorManager::disableSensor(const char* name, SensorStatus& status) {
    status = SensorStatus::FAILED_DISABLED;
    char msg[48];
    snprintf(msg, sizeof(msg), "%s: too many failures", name);
    setError(msg);
    DBG_PRINT("SENSOR DISABLED: ");
    DBG_PRINTLN(name);
}

void SensorManager::setError(const char* msg) {
    if (_lastError.length() > 0) {
        _lastError += "; ";
    }
    _lastError += msg;
    DBG_PRINTLN(msg);
}

// ============================================================================
// Init functions (called from begin() for enabled sensors)
// ============================================================================

bool SensorManager::initBme(Preferences* prefs) {
    _i2cSdaPin = prefs->getInt(preference_sensor_i2c_sda);
    _i2cSclPin = prefs->getInt(preference_sensor_i2c_scl);

    if (_i2cSdaPin == 0 || _i2cSclPin == 0) {
        setError("BME280: pins not set");
        return false;
    }

    _i2cBme.begin(_i2cSdaPin, _i2cSclPin);

    if (!_bme.begin(0x76, &_i2cBme) && !_bme.begin(0x77, &_i2cBme)) {
        setError("BME280: not found on I2C");
        return false;
    }

    float temp = _bme.readTemperature();
    float hum = _bme.readHumidity();
    float pres = _bme.readPressure() / 100.0f;

    if (!validateBmeReading(temp, hum, pres)) {
        setError("BME280: initial reading invalid");
        return false;
    }

    _bmeTemp = temp;
    _bmeHum = hum;
    _bmePres = pres;
    DBG_PRINTLN("BME280: Active");
    return true;
}

bool SensorManager::initDs18x20(Preferences* prefs) {
    _ds18x20Pin = prefs->getInt(preference_sensor_ds18x20_pin);

    if (_ds18x20Pin == 0) {
        setError("DS18X20: pin not set");
        return false;
    }

    static OneWire staticOneWire(_ds18x20Pin);
    static DallasTemperature staticDs18x20(&staticOneWire);

    _oneWire = &staticOneWire;
    _ds18x20 = &staticDs18x20;
    _ds18x20->begin();
    _ds18x20->requestTemperatures();
    float temp = _ds18x20->getTempCByIndex(0);

    if (!validateTemperature(temp)) {
        setError("DS18X20: no valid reading");
        _ds18x20 = nullptr;
        _oneWire = nullptr;
        return false;
    }

    _ds18x20Temp = temp;
    DBG_PRINTLN("DS18X20: Active");
    return true;
}

bool SensorManager::initDht22(Preferences* prefs) {
    _dhtPin = prefs->getInt(preference_sensor_dht_data_pin);

    if (_dhtPin == 0) {
        setError("DHT22: pin not set");
        return false;
    }

    static DHT staticDht(_dhtPin, DHT22);
    _dht = &staticDht;
    _dht->begin();
    delay(2000);

    float temp = _dht->readTemperature();
    float hum = _dht->readHumidity();

    if (!validateTemperature(temp) || !validateHumidity(hum)) {
        setError("DHT22: no valid reading");
        _dht = nullptr;
        return false;
    }

    _dht22Temp = temp;
    _dht22Hum = hum;
    DBG_PRINTLN("DHT22: Active");
    return true;
}

bool SensorManager::initHcsr04(Preferences* prefs) {
    _hcsr04TrigPin = prefs->getInt(preference_sensor_sr04_trigpin);
    _hcsr04EchoPin = prefs->getInt(preference_sensor_sr04_echopin);
    _hcsr04MaxDistanceCm = prefs->getInt(preference_sensor_sr04_max_dist);

    if (_hcsr04TrigPin == 0 || _hcsr04EchoPin == 0) {
        setError("HC-SR04: pins not set");
        return false;
    }

    pinMode(_hcsr04TrigPin, OUTPUT);
    pinMode(_hcsr04EchoPin, INPUT);

    digitalWrite(_hcsr04TrigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(_hcsr04TrigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(_hcsr04TrigPin, LOW);

    long duration = pulseIn(_hcsr04EchoPin, HIGH, 30000);
    int distCm = duration * SOUND_SPEED / 2;

    if (!validateDistance(distCm)) {
        setError("HC-SR04: no valid response");
        return false;
    }

    _hcsr04DistanceCm = distCm;
    _hcsr04MaxMeasuredCm = distCm;
    DBG_PRINTLN("HC-SR04: Active");
    return true;
}

bool SensorManager::initHcsr501(Preferences* prefs) {
    _hcsr501Pin = prefs->getInt(preference_sensor_sr501);

    if (_hcsr501Pin == 0) {
        setError("HC-SR501: pin not set");
        return false;
    }

    pinMode(_hcsr501Pin, INPUT);
    _hcsr501LastStat = digitalRead(_hcsr501Pin);
    _hcsr501Stat = _hcsr501LastStat;
    DBG_PRINTLN("HC-SR501: Active");
    return true;
}

bool SensorManager::initMq4(Preferences* prefs) {
    _mq4AnalogPin = prefs->getInt(preference_sensor_mq4_analog);
    _mq4DigitalPin = prefs->getInt(preference_sensor_mq4_digital);

    if (_mq4AnalogPin == 0) {
        setError("MQ4: analog pin not set");
        return false;
    }

    pinMode(_mq4AnalogPin, INPUT);
    if (_mq4DigitalPin > 0) {
        pinMode(_mq4DigitalPin, INPUT);
    }

    int analogVal = analogRead(_mq4AnalogPin);
    if (!validateGasReading(analogVal)) {
        setError("MQ4: no valid reading");
        return false;
    }

    _mq4AnalogValue = analogVal;
    if (_mq4DigitalPin > 0) {
        _mq4DigitalAlarm = digitalRead(_mq4DigitalPin) == LOW;
    }
    DBG_PRINTLN("MQ4: Active");
    return true;
}

// ============================================================================
// begin() - Initialize enabled sensors (manual selection via WebUI)
// ============================================================================

void SensorManager::begin(Preferences* prefs) {
    DBG_PRINTLN("=== Sensor Init ===");

    // Load thresholds
    tempThreshold = prefs->getDouble(preference_sensor_temp_treshold);
    humThreshold = prefs->getInt(preference_sensor_hum_threshold);
    presThreshold = prefs->getInt(preference_sensor_pres_threshold);
    proxThreshold = prefs->getInt(preference_sensor_prox_treshold);
    gasThreshold = prefs->getInt(preference_sensor_gas_threshold);

    // Only init sensors that are enabled in preferences
    if (prefs->getBool(preference_sensor_bme_enabled)) {
        _bmeStatus = initBme(prefs) ? SensorStatus::ACTIVE : SensorStatus::NOT_CONFIGURED;
    }

    if (prefs->getBool(preference_sensor_ds18x20_enabled)) {
        _ds18x20Status = initDs18x20(prefs) ? SensorStatus::ACTIVE : SensorStatus::NOT_CONFIGURED;
    }

    if (prefs->getBool(preference_sensor_dht22_enabled)) {
        _dht22Status = initDht22(prefs) ? SensorStatus::ACTIVE : SensorStatus::NOT_CONFIGURED;
    }

    if (prefs->getBool(preference_sensor_hcsr04_enabled)) {
        _hcsr04Status = initHcsr04(prefs) ? SensorStatus::ACTIVE : SensorStatus::NOT_CONFIGURED;
    }

    if (prefs->getBool(preference_sensor_hcsr501_enabled)) {
        _hcsr501Status = initHcsr501(prefs) ? SensorStatus::ACTIVE : SensorStatus::NOT_CONFIGURED;
    }

    if (prefs->getBool(preference_sensor_mq4_enabled)) {
        _mq4Status = initMq4(prefs) ? SensorStatus::ACTIVE : SensorStatus::NOT_CONFIGURED;
    }

    DBG_PRINTLN("=== Sensor Summary ===");
    DBG_PRINT("BME280:  "); DBG_PRINTLN(_bmeStatus == SensorStatus::ACTIVE ? "ACTIVE" : "off");
    DBG_PRINT("DS18X20: "); DBG_PRINTLN(_ds18x20Status == SensorStatus::ACTIVE ? "ACTIVE" : "off");
    DBG_PRINT("DHT22:   "); DBG_PRINTLN(_dht22Status == SensorStatus::ACTIVE ? "ACTIVE" : "off");
    DBG_PRINT("HC-SR04: "); DBG_PRINTLN(_hcsr04Status == SensorStatus::ACTIVE ? "ACTIVE" : "off");
    DBG_PRINT("HC-SR501:"); DBG_PRINTLN(_hcsr501Status == SensorStatus::ACTIVE ? "ACTIVE" : "off");
    DBG_PRINT("MQ4:     "); DBG_PRINTLN(_mq4Status == SensorStatus::ACTIVE ? "ACTIVE" : "off");
    DBG_PRINTLN("======================");
    _ready = true;
}

// ============================================================================
// Poll functions
// ============================================================================

void SensorManager::pollBme() {
    _i2cBme.begin(_i2cSdaPin, _i2cSclPin);
    bool status = _bme.begin(0x76, &_i2cBme);
    if (!status) {
        status = _bme.begin(0x77, &_i2cBme);
    }

    float temp = _bme.readTemperature();
    float hum = _bme.readHumidity();
    float pres = _bme.readPressure() / 100.0f;

    if (!validateBmeReading(temp, hum, pres)) {
        _bmeFailCount++;
        if (_bmeFailCount >= SENSOR_MAX_FAIL_COUNT) {
            disableSensor("BME280", _bmeStatus);
        }
        return;
    }

    _bmeFailCount = 0;
    _bmeTemp = temp;
    _bmeHum = hum;
    _bmePres = pres;

    if (abs(_bmeTemp - _bmeLastTemp) >= tempThreshold ||
        abs(_bmeHum - _bmeLastHum) >= humThreshold ||
        abs(_bmePres - _bmeLastPres) >= presThreshold) {
        _bmeLastTemp = _bmeTemp;
        _bmeLastHum = _bmeHum;
        _bmeLastPres = _bmePres;
        _newSensorData = true;
    }
}

void SensorManager::pollDs18x20() {
    _ds18x20->requestTemperatures();
    float temp = _ds18x20->getTempCByIndex(0);

    if (!validateTemperature(temp)) {
        _ds18x20FailCount++;
        if (_ds18x20FailCount >= SENSOR_MAX_FAIL_COUNT) {
            disableSensor("DS18X20", _ds18x20Status);
        }
        return;
    }

    _ds18x20FailCount = 0;
    _ds18x20Temp = temp;

    if (abs(_ds18x20Temp - _ds18x20LastTemp) >= tempThreshold) {
        _ds18x20LastTemp = _ds18x20Temp;
        _newSensorData = true;
    }
}

void SensorManager::pollDht22() {
    float temp = _dht->readTemperature();
    float hum = _dht->readHumidity();

    bool tempValid = validateTemperature(temp);
    bool humValid = validateHumidity(hum);

    if (!tempValid && !humValid) {
        _dht22FailCount++;
        if (_dht22FailCount >= SENSOR_MAX_FAIL_COUNT) {
            disableSensor("DHT22", _dht22Status);
        }
        return;
    }

    _dht22FailCount = 0;

    if (tempValid) {
        _dht22Temp = temp;
        if (abs(_dht22Temp - _dht22LastTemp) >= tempThreshold) {
            _dht22LastTemp = _dht22Temp;
            _newSensorData = true;
        }
    }

    if (humValid) {
        _dht22Hum = hum;
        if (abs(_dht22Hum - _dht22LastHum) >= humThreshold) {
            _dht22LastHum = _dht22Hum;
            _newSensorData = true;
        }
    }
}

void SensorManager::pollHcsr04() {
    digitalWrite(_hcsr04TrigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(_hcsr04TrigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(_hcsr04TrigPin, LOW);

    long duration = pulseIn(_hcsr04EchoPin, HIGH, 30000);
    int distCm = duration * SOUND_SPEED / 2;

    if (!validateDistance(distCm)) {
        _hcsr04FailCount++;
        if (_hcsr04FailCount >= SENSOR_MAX_FAIL_COUNT) {
            disableSensor("HC-SR04", _hcsr04Status);
        }
        return;
    }

    _hcsr04FailCount = 0;
    _hcsr04DistanceCm = distCm;

    if (_hcsr04DistanceCm > _hcsr04MaxMeasuredCm) {
        _hcsr04MaxMeasuredCm = _hcsr04DistanceCm;
    }

    if ((_hcsr04DistanceCm + proxThreshold) > _hcsr04MaxMeasuredCm) {
        _hcsr04ParkAvailable = true;
    } else {
        _hcsr04ParkAvailable = false;
    }

    if (abs(_hcsr04DistanceCm - _hcsr04LastDistanceCm) >= proxThreshold ||
        _hcsr04ParkAvailable != _hcsr04LastParkAvailable) {
        _hcsr04LastDistanceCm = _hcsr04DistanceCm;
        _hcsr04LastParkAvailable = _hcsr04ParkAvailable;
        _newSensorData = true;
    }
}

void SensorManager::pollHcsr501() {
    _hcsr501Stat = digitalRead(_hcsr501Pin);
    if (_hcsr501Stat != _hcsr501LastStat) {
        _hcsr501LastStat = _hcsr501Stat;
        _hcsr501Changed = true;
    }
}

void SensorManager::pollMq4() {
    int analogVal = analogRead(_mq4AnalogPin);

    if (!validateGasReading(analogVal)) {
        _mq4FailCount++;
        if (_mq4FailCount >= SENSOR_MAX_FAIL_COUNT) {
            disableSensor("MQ4", _mq4Status);
        }
        return;
    }

    _mq4FailCount = 0;
    _mq4AnalogValue = analogVal;

    bool digitalAlarm = false;
    if (_mq4DigitalPin > 0) {
        digitalAlarm = digitalRead(_mq4DigitalPin) == LOW;
    }
    _mq4DigitalAlarm = digitalAlarm;

    if (abs(_mq4AnalogValue - _mq4LastAnalogValue) >= gasThreshold ||
        _mq4DigitalAlarm != _mq4LastDigitalAlarm) {
        _mq4LastAnalogValue = _mq4AnalogValue;
        _mq4LastDigitalAlarm = _mq4DigitalAlarm;
        _newSensorData = true;
    }
}

// ============================================================================
// poll() - Main polling entry point
// ============================================================================

void SensorManager::poll() {
    if (_hcsr501Status == SensorStatus::ACTIVE) {
        pollHcsr501();
    }
    if (_ds18x20Status == SensorStatus::ACTIVE) {
        pollDs18x20();
    }
    if (_bmeStatus == SensorStatus::ACTIVE) {
        pollBme();
    }
    if (_hcsr04Status == SensorStatus::ACTIVE) {
        pollHcsr04();
    }
    if (_dht22Status == SensorStatus::ACTIVE) {
        pollDht22();
    }
    if (_mq4Status == SensorStatus::ACTIVE) {
        pollMq4();
    }
}

// ============================================================================
// Data access
// ============================================================================

bool SensorManager::hasNewData() {
    return _newSensorData;
}

void SensorManager::clearNewData() {
    _newSensorData = false;
}

bool SensorManager::hasTempSensor() const {
    return _bmeStatus == SensorStatus::ACTIVE ||
           _ds18x20Status == SensorStatus::ACTIVE ||
           _dht22Status == SensorStatus::ACTIVE;
}

bool SensorManager::hasHumiditySensor() const {
    return _bmeStatus == SensorStatus::ACTIVE ||
           _dht22Status == SensorStatus::ACTIVE;
}

bool SensorManager::hasPressureSensor() const {
    return _bmeStatus == SensorStatus::ACTIVE;
}

bool SensorManager::hasDistanceSensor() const {
    return _hcsr04Status == SensorStatus::ACTIVE;
}

bool SensorManager::hasMotionSensor() const {
    return _hcsr501Status == SensorStatus::ACTIVE;
}

bool SensorManager::hasGasSensor() const {
    return _mq4Status == SensorStatus::ACTIVE;
}

bool SensorManager::hasAnySensor() const {
    return hasTempSensor() || hasHumiditySensor() || hasPressureSensor() ||
           hasDistanceSensor() || hasMotionSensor() || hasGasSensor();
}

static const char* sensorStatusStr(SensorStatus s) {
    switch (s) {
        case SensorStatus::ACTIVE:          return "active";
        case SensorStatus::FAILED_DISABLED: return "disabled";
        case SensorStatus::NOT_CONFIGURED:  return "off";
        default:                            return "off";
    }
}

void SensorManager::toDetectionJson(JsonObject& sensors) const {
    sensors["BME280"]  = sensorStatusStr(_bmeStatus);
    sensors["DS18X20"] = sensorStatusStr(_ds18x20Status);
    sensors["DHT22"]   = sensorStatusStr(_dht22Status);
    sensors["HC-SR04"] = sensorStatusStr(_hcsr04Status);
    sensors["HC-SR501"]= sensorStatusStr(_hcsr501Status);
    sensors["MQ4"]     = sensorStatusStr(_mq4Status);
}

void SensorManager::toJson(JsonDocument& doc) {
    char buf[20];

    if (_bmeStatus == SensorStatus::ACTIVE) {
        dtostrf(_bmeTemp, 2, 1, buf);
        doc["temp"] = buf;
        dtostrf(_bmeHum, 2, 1, buf);
        doc["hum"] = buf;
        dtostrf(_bmePres, 2, 1, buf);
        doc["pres"] = buf;
    } else if (_ds18x20Status == SensorStatus::ACTIVE) {
        dtostrf(_ds18x20Temp, 2, 1, buf);
        doc["temp"] = buf;
    } else if (_dht22Status == SensorStatus::ACTIVE) {
        dtostrf(_dht22Temp, 2, 2, buf);
        doc["temp"] = buf;
        dtostrf(_dht22Hum, 2, 2, buf);
        doc["hum"] = buf;
    }

    if (_hcsr04Status == SensorStatus::ACTIVE) {
        sprintf(buf, "%d", _hcsr04DistanceCm);
        doc["dist"] = buf;
        doc["free"] = _hcsr04ParkAvailable ? "true" : "false";
    }

    if (_hcsr501Status == SensorStatus::ACTIVE) {
        doc["motion"] = _hcsr501Stat ? "true" : "false";
    }

    if (_mq4Status == SensorStatus::ACTIVE) {
        sprintf(buf, "%d", _mq4AnalogValue);
        doc["gas"] = buf;
        doc["gas_alarm"] = _mq4DigitalAlarm ? "true" : "false";
    }
}

void SensorManager::toStatusJson(JsonObject& sensors) {
    char buf[20];

    if (_bmeStatus == SensorStatus::ACTIVE) {
        dtostrf(_bmeTemp, 2, 1, buf);
        strcat(buf, " °C");
        sensors["temp"] = buf;
        dtostrf(_bmeHum, 2, 1, buf);
        strcat(buf, " %");
        sensors["hum"] = buf;
        dtostrf(_bmePres, 2, 1, buf);
        strcat(buf, " mbar");
        sensors["pres"] = buf;
    } else if (_ds18x20Status == SensorStatus::ACTIVE) {
        dtostrf(_ds18x20Temp, 2, 1, buf);
        strcat(buf, " °C");
        sensors["temp"] = buf;
    } else if (_dht22Status == SensorStatus::ACTIVE) {
        dtostrf(_dht22Temp, 2, 1, buf);
        strcat(buf, " °C");
        sensors["temp"] = buf;
        dtostrf(_dht22Hum, 2, 1, buf);
        strcat(buf, " %");
        sensors["hum"] = buf;
    }

    if (_hcsr04Status == SensorStatus::ACTIVE) {
        dtostrf(_hcsr04DistanceCm, 2, 0, buf);
        strcat(buf, " cm");
        sensors["dist"] = buf;
    }

    if (_mq4Status == SensorStatus::ACTIVE) {
        sprintf(buf, "%d", _mq4AnalogValue);
        sensors["gas"] = buf;
        sensors["gas_alarm"] = _mq4DigitalAlarm ? "ALARM" : "OK";
    }
}
