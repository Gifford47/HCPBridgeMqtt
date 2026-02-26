#include "sensor_manager.h"

// ============================================================================
// Validation functions (used by both detect and poll)
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
    // BME280 specific: humidity >= 99.9 indicates I2C hung
    if (hum >= 99.9f) return false;
    return validateTemperature(temp) && validateHumidity(hum) && validatePressure(pres);
}

bool SensorManager::validateDistance(int distCm) {
    if (distCm <= 0) return false;
    if (distCm > _hcsr04MaxDistanceCm * 2) return false;  // Allow some headroom over max
    return true;
}

bool SensorManager::validateGasReading(int analogValue) {
    // ADC reading should be in valid range (not 0 = disconnected, not 4095 = stuck high)
    if (analogValue <= 0 || analogValue >= 4095) return false;
    return true;
}

// ============================================================================
// Disable helper
// ============================================================================

void SensorManager::disableSensor(const char* name, SensorStatus& status) {
    status = SensorStatus::FAILED_DISABLED;
    Serial.print("SENSOR DISABLED: ");
    Serial.print(name);
    Serial.println(" - too many consecutive failures. Will not retry until reboot.");
}

// ============================================================================
// Detection functions
// ============================================================================

bool SensorManager::detectBme(Preferences* prefs) {
    _i2cSdaPin = prefs->getInt(preference_sensor_i2c_sda);
    _i2cSclPin = prefs->getInt(preference_sensor_i2c_scl);

    if (_i2cSdaPin == 0 || _i2cSclPin == 0) {
        Serial.println("BME280: Pins not configured, skipping");
        return false;
    }

    Serial.print("BME280: Probing on I2C SDA=");
    Serial.print(_i2cSdaPin);
    Serial.print(" SCL=");
    Serial.println(_i2cSclPin);

    _i2cBme.begin(_i2cSdaPin, _i2cSclPin);

    // Try address 0x76 first, then 0x77
    bool found = _bme.begin(0x76, &_i2cBme);
    if (!found) {
        found = _bme.begin(0x77, &_i2cBme);
    }

    if (!found) {
        Serial.println("BME280: Not found on I2C bus");
        return false;
    }

    // Read first values and validate
    float temp = _bme.readTemperature();
    float hum = _bme.readHumidity();
    float pres = _bme.readPressure() / 100.0f;

    if (!validateBmeReading(temp, hum, pres)) {
        Serial.println("BME280: Found but initial reading invalid");
        return false;
    }

    _bmeTemp = temp;
    _bmeHum = hum;
    _bmePres = pres;
    Serial.print("BME280: Active (T=");
    Serial.print(temp);
    Serial.print(" H=");
    Serial.print(hum);
    Serial.print(" P=");
    Serial.print(pres);
    Serial.println(")");
    return true;
}

bool SensorManager::detectDs18x20(Preferences* prefs) {
    _ds18x20Pin = prefs->getInt(preference_sensor_ds18x20_pin);

    if (_ds18x20Pin == 0) {
        Serial.println("DS18X20: Pin not configured, skipping");
        return false;
    }

    Serial.print("DS18X20: Probing on pin ");
    Serial.println(_ds18x20Pin);

    // Create static instances that persist
    static OneWire staticOneWire(_ds18x20Pin);
    static DallasTemperature staticDs18x20(&staticOneWire);

    _oneWire = &staticOneWire;
    _ds18x20 = &staticDs18x20;
    _ds18x20->begin();

    // Request temperature and read
    _ds18x20->requestTemperatures();
    float temp = _ds18x20->getTempCByIndex(0);

    if (!validateTemperature(temp)) {
        Serial.println("DS18X20: Not found or invalid reading");
        _ds18x20 = nullptr;
        _oneWire = nullptr;
        return false;
    }

    _ds18x20Temp = temp;
    Serial.print("DS18X20: Active (T=");
    Serial.print(temp);
    Serial.println(")");
    return true;
}

bool SensorManager::detectDht22(Preferences* prefs) {
    _dhtPin = prefs->getInt(preference_sensor_dht_data_pin);

    if (_dhtPin == 0) {
        Serial.println("DHT22: Pin not configured, skipping");
        return false;
    }

    Serial.print("DHT22: Probing on pin ");
    Serial.println(_dhtPin);

    static DHT staticDht(_dhtPin, DHT22);
    _dht = &staticDht;
    _dht->begin();

    // Wait a moment for DHT22 to stabilize
    delay(2000);

    float temp = _dht->readTemperature();
    float hum = _dht->readHumidity();

    if (!validateTemperature(temp) || !validateHumidity(hum)) {
        Serial.println("DHT22: Not found or invalid reading");
        _dht = nullptr;
        return false;
    }

    _dht22Temp = temp;
    _dht22Hum = hum;
    Serial.print("DHT22: Active (T=");
    Serial.print(temp);
    Serial.print(" H=");
    Serial.print(hum);
    Serial.println(")");
    return true;
}

bool SensorManager::detectHcsr04(Preferences* prefs) {
    _hcsr04TrigPin = prefs->getInt(preference_sensor_sr04_trigpin);
    _hcsr04EchoPin = prefs->getInt(preference_sensor_sr04_echopin);
    _hcsr04MaxDistanceCm = prefs->getInt(preference_sensor_sr04_max_dist);

    if (_hcsr04TrigPin == 0 || _hcsr04EchoPin == 0) {
        Serial.println("HC-SR04: Pins not configured, skipping");
        return false;
    }

    Serial.print("HC-SR04: Probing on trig=");
    Serial.print(_hcsr04TrigPin);
    Serial.print(" echo=");
    Serial.println(_hcsr04EchoPin);

    pinMode(_hcsr04TrigPin, OUTPUT);
    pinMode(_hcsr04EchoPin, INPUT);

    // Take a test measurement
    digitalWrite(_hcsr04TrigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(_hcsr04TrigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(_hcsr04TrigPin, LOW);

    long duration = pulseIn(_hcsr04EchoPin, HIGH, 30000);  // 30ms timeout
    int distCm = duration * SOUND_SPEED / 2;

    if (!validateDistance(distCm)) {
        Serial.println("HC-SR04: No valid response");
        return false;
    }

    _hcsr04DistanceCm = distCm;
    _hcsr04MaxMeasuredCm = distCm;
    Serial.print("HC-SR04: Active (dist=");
    Serial.print(distCm);
    Serial.println("cm)");
    return true;
}

bool SensorManager::detectHcsr501(Preferences* prefs) {
    _hcsr501Pin = prefs->getInt(preference_sensor_sr501);

    if (_hcsr501Pin == 0) {
        Serial.println("HC-SR501: Pin not configured, skipping");
        return false;
    }

    Serial.print("HC-SR501: Configured on pin ");
    Serial.println(_hcsr501Pin);

    // HC-SR501 is a simple digital sensor - no way to detect if it's actually connected
    // We assume it's present if pin is configured
    pinMode(_hcsr501Pin, INPUT);
    _hcsr501LastStat = digitalRead(_hcsr501Pin);
    _hcsr501Stat = _hcsr501LastStat;

    Serial.println("HC-SR501: Active (assumed present)");
    return true;
}

bool SensorManager::detectMq4(Preferences* prefs) {
    _mq4AnalogPin = prefs->getInt(preference_sensor_mq4_analog);
    _mq4DigitalPin = prefs->getInt(preference_sensor_mq4_digital);

    if (_mq4AnalogPin == 0) {
        Serial.println("MQ4: Analog pin not configured, skipping");
        return false;
    }

    Serial.print("MQ4: Probing on analog=");
    Serial.print(_mq4AnalogPin);
    Serial.print(" digital=");
    Serial.println(_mq4DigitalPin);

    // Configure pins
    pinMode(_mq4AnalogPin, INPUT);
    if (_mq4DigitalPin > 0) {
        pinMode(_mq4DigitalPin, INPUT);
    }

    // Take a test reading
    delay(100);
    int analogVal = analogRead(_mq4AnalogPin);

    if (!validateGasReading(analogVal)) {
        Serial.println("MQ4: Not found or invalid reading");
        return false;
    }

    _mq4AnalogValue = analogVal;
    if (_mq4DigitalPin > 0) {
        _mq4DigitalAlarm = digitalRead(_mq4DigitalPin) == LOW;  // MQ4 digital output is active LOW
    }

    Serial.print("MQ4: Active (analog=");
    Serial.print(analogVal);
    Serial.print(" alarm=");
    Serial.print(_mq4DigitalAlarm ? "YES" : "NO");
    Serial.println(")");
    return true;
}

// ============================================================================
// begin() - Initialize and auto-detect sensors
// ============================================================================

void SensorManager::begin(Preferences* prefs) {
    Serial.println("=== Sensor Auto-Detection ===");

    // Load thresholds
    tempThreshold = prefs->getDouble(preference_sensor_temp_treshold);
    humThreshold = prefs->getInt(preference_sensor_hum_threshold);
    presThreshold = prefs->getInt(preference_sensor_pres_threshold);
    proxThreshold = prefs->getInt(preference_sensor_prox_treshold);
    gasThreshold = prefs->getInt(preference_sensor_gas_threshold);

    // Try each sensor type
    _bmeStatus = SensorStatus::DETECTING;
    if (detectBme(prefs)) {
        _bmeStatus = SensorStatus::ACTIVE;
    } else {
        _bmeStatus = SensorStatus::NOT_CONFIGURED;
    }

    _ds18x20Status = SensorStatus::DETECTING;
    if (detectDs18x20(prefs)) {
        _ds18x20Status = SensorStatus::ACTIVE;
    } else {
        _ds18x20Status = SensorStatus::NOT_CONFIGURED;
    }

    _dht22Status = SensorStatus::DETECTING;
    if (detectDht22(prefs)) {
        _dht22Status = SensorStatus::ACTIVE;
    } else {
        _dht22Status = SensorStatus::NOT_CONFIGURED;
    }

    _hcsr04Status = SensorStatus::DETECTING;
    if (detectHcsr04(prefs)) {
        _hcsr04Status = SensorStatus::ACTIVE;
    } else {
        _hcsr04Status = SensorStatus::NOT_CONFIGURED;
    }

    _hcsr501Status = SensorStatus::DETECTING;
    if (detectHcsr501(prefs)) {
        _hcsr501Status = SensorStatus::ACTIVE;
    } else {
        _hcsr501Status = SensorStatus::NOT_CONFIGURED;
    }

    _mq4Status = SensorStatus::DETECTING;
    if (detectMq4(prefs)) {
        _mq4Status = SensorStatus::ACTIVE;
    } else {
        _mq4Status = SensorStatus::NOT_CONFIGURED;
    }

    // Summary
    Serial.println("=== Sensor Summary ===");
    Serial.print("BME280:  "); Serial.println(_bmeStatus == SensorStatus::ACTIVE ? "ACTIVE" : "not found");
    Serial.print("DS18X20: "); Serial.println(_ds18x20Status == SensorStatus::ACTIVE ? "ACTIVE" : "not found");
    Serial.print("DHT22:   "); Serial.println(_dht22Status == SensorStatus::ACTIVE ? "ACTIVE" : "not found");
    Serial.print("HC-SR04: "); Serial.println(_hcsr04Status == SensorStatus::ACTIVE ? "ACTIVE" : "not found");
    Serial.print("HC-SR501:"); Serial.println(_hcsr501Status == SensorStatus::ACTIVE ? "ACTIVE" : "not found");
    Serial.print("MQ4:     "); Serial.println(_mq4Status == SensorStatus::ACTIVE ? "ACTIVE" : "not found");
    Serial.println("======================");
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
    // Trigger measurement
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

    // Track max distance
    if (_hcsr04DistanceCm > _hcsr04MaxMeasuredCm) {
        _hcsr04MaxMeasuredCm = _hcsr04DistanceCm;
    }

    // Determine park availability
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
        // Motion sensor triggers immediate publish (not through _newSensorData)
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
        digitalAlarm = digitalRead(_mq4DigitalPin) == LOW;  // Active LOW
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
    // HC-SR501 is polled first for immediate state change detection
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

void SensorManager::toJson(JsonDocument& doc) {
    char buf[20];

    // Temperature: prefer BME280 > DS18X20 > DHT22
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
