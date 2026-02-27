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
    DBG_PRINT("SENSOR DISABLED: ");
    DBG_PRINT(name);
    DBG_PRINTLN(" - too many consecutive failures. Will not retry until reboot.");
}

// ============================================================================
// Detection functions
// ============================================================================

bool SensorManager::detectBme(Preferences* prefs) {
    _i2cSdaPin = prefs->getInt(preference_sensor_i2c_sda);
    _i2cSclPin = prefs->getInt(preference_sensor_i2c_scl);

    if (_i2cSdaPin == 0 || _i2cSclPin == 0) {
        DBG_PRINTLN("BME280: Pins not configured, skipping");
        return false;
    }

    DBG_PRINT("BME280: Probing on I2C SDA=");
    DBG_PRINT(_i2cSdaPin);
    DBG_PRINT(" SCL=");
    DBG_PRINTLN(_i2cSclPin);

    _i2cBme.begin(_i2cSdaPin, _i2cSclPin);

    // Try address 0x76 first, then 0x77
    bool found = _bme.begin(0x76, &_i2cBme);
    if (!found) {
        found = _bme.begin(0x77, &_i2cBme);
    }

    if (!found) {
        DBG_PRINTLN("BME280: Not found on I2C bus");
        return false;
    }

    // Read first values and validate
    float temp = _bme.readTemperature();
    float hum = _bme.readHumidity();
    float pres = _bme.readPressure() / 100.0f;

    if (!validateBmeReading(temp, hum, pres)) {
        DBG_PRINTLN("BME280: Found but initial reading invalid");
        return false;
    }

    _bmeTemp = temp;
    _bmeHum = hum;
    _bmePres = pres;
    DBG_PRINT("BME280: Active (T=");
    DBG_PRINT(temp);
    DBG_PRINT(" H=");
    DBG_PRINT(hum);
    DBG_PRINT(" P=");
    DBG_PRINT(pres);
    DBG_PRINTLN(")");
    return true;
}

bool SensorManager::detectDs18x20(Preferences* prefs) {
    _ds18x20Pin = prefs->getInt(preference_sensor_ds18x20_pin);

    if (_ds18x20Pin == 0) {
        DBG_PRINTLN("DS18X20: Pin not configured, skipping");
        return false;
    }

    DBG_PRINT("DS18X20: Probing on pin ");
    DBG_PRINTLN(_ds18x20Pin);

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
        DBG_PRINTLN("DS18X20: Not found or invalid reading");
        _ds18x20 = nullptr;
        _oneWire = nullptr;
        return false;
    }

    _ds18x20Temp = temp;
    DBG_PRINT("DS18X20: Active (T=");
    DBG_PRINT(temp);
    DBG_PRINTLN(")");
    return true;
}

bool SensorManager::detectDht22(Preferences* prefs) {
    _dhtPin = prefs->getInt(preference_sensor_dht_data_pin);

    if (_dhtPin == 0) {
        DBG_PRINTLN("DHT22: Pin not configured, skipping");
        return false;
    }

    DBG_PRINT("DHT22: Probing on pin ");
    DBG_PRINTLN(_dhtPin);

    static DHT staticDht(_dhtPin, DHT22);
    _dht = &staticDht;
    _dht->begin();

    // Wait a moment for DHT22 to stabilize
    delay(2000);

    float temp = _dht->readTemperature();
    float hum = _dht->readHumidity();

    if (!validateTemperature(temp) || !validateHumidity(hum)) {
        DBG_PRINTLN("DHT22: Not found or invalid reading");
        _dht = nullptr;
        return false;
    }

    _dht22Temp = temp;
    _dht22Hum = hum;
    DBG_PRINT("DHT22: Active (T=");
    DBG_PRINT(temp);
    DBG_PRINT(" H=");
    DBG_PRINT(hum);
    DBG_PRINTLN(")");
    return true;
}

bool SensorManager::detectHcsr04(Preferences* prefs) {
    _hcsr04TrigPin = prefs->getInt(preference_sensor_sr04_trigpin);
    _hcsr04EchoPin = prefs->getInt(preference_sensor_sr04_echopin);
    _hcsr04MaxDistanceCm = prefs->getInt(preference_sensor_sr04_max_dist);

    if (_hcsr04TrigPin == 0 || _hcsr04EchoPin == 0) {
        DBG_PRINTLN("HC-SR04: Pins not configured, skipping");
        return false;
    }

    DBG_PRINT("HC-SR04: Probing on trig=");
    DBG_PRINT(_hcsr04TrigPin);
    DBG_PRINT(" echo=");
    DBG_PRINTLN(_hcsr04EchoPin);

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
        DBG_PRINTLN("HC-SR04: No valid response");
        return false;
    }

    _hcsr04DistanceCm = distCm;
    _hcsr04MaxMeasuredCm = distCm;
    DBG_PRINT("HC-SR04: Active (dist=");
    DBG_PRINT(distCm);
    DBG_PRINTLN("cm)");
    return true;
}

bool SensorManager::detectHcsr501(Preferences* prefs) {
    _hcsr501Pin = prefs->getInt(preference_sensor_sr501);

    if (_hcsr501Pin == 0) {
        DBG_PRINTLN("HC-SR501: Pin not configured, skipping");
        return false;
    }

    DBG_PRINT("HC-SR501: Configured on pin ");
    DBG_PRINTLN(_hcsr501Pin);

    // HC-SR501 is a simple digital sensor - no way to detect if it's actually connected
    // We assume it's present if pin is configured
    pinMode(_hcsr501Pin, INPUT);
    _hcsr501LastStat = digitalRead(_hcsr501Pin);
    _hcsr501Stat = _hcsr501LastStat;

    DBG_PRINTLN("HC-SR501: Active (assumed present)");
    return true;
}

bool SensorManager::detectMq4(Preferences* prefs) {
    _mq4AnalogPin = prefs->getInt(preference_sensor_mq4_analog);
    _mq4DigitalPin = prefs->getInt(preference_sensor_mq4_digital);

    if (_mq4AnalogPin == 0) {
        DBG_PRINTLN("MQ4: Analog pin not configured, skipping");
        return false;
    }

    DBG_PRINT("MQ4: Probing on analog=");
    DBG_PRINT(_mq4AnalogPin);
    DBG_PRINT(" digital=");
    DBG_PRINTLN(_mq4DigitalPin);

    // Configure pins
    pinMode(_mq4AnalogPin, INPUT);
    if (_mq4DigitalPin > 0) {
        pinMode(_mq4DigitalPin, INPUT);
    }

    // Take multiple readings to distinguish a real sensor from a floating pin.
    // A connected MQ4 produces stable ADC values; a floating pin produces noisy, scattered readings.
    const int NUM_SAMPLES = 10;
    const int SAMPLE_DELAY_MS = 20;
    int samples[NUM_SAMPLES];
    int minVal = 4095, maxVal = 0;
    long sum = 0;

    delay(100);  // Let pin settle

    for (int i = 0; i < NUM_SAMPLES; i++) {
        samples[i] = analogRead(_mq4AnalogPin);
        if (samples[i] < minVal) minVal = samples[i];
        if (samples[i] > maxVal) maxVal = samples[i];
        sum += samples[i];
        delay(SAMPLE_DELAY_MS);
    }

    int avgVal = sum / NUM_SAMPLES;
    int spread = maxVal - minVal;

    DBG_PRINT("MQ4: samples avg=");
    DBG_PRINT(avgVal);
    DBG_PRINT(" spread=");
    DBG_PRINT(spread);
    DBG_PRINT(" (min=");
    DBG_PRINT(minVal);
    DBG_PRINT(" max=");
    DBG_PRINT(maxVal);
    DBG_PRINTLN(")");

    // Validate average value
    if (!validateGasReading(avgVal)) {
        DBG_PRINTLN("MQ4: Not found or invalid reading");
        return false;
    }

    // A floating pin without pullup/pulldown produces high variance (spread > 100 typical).
    // A real MQ4 sensor is stable (spread < 50 under normal conditions).
    const int MAX_SPREAD = 80;
    if (spread > MAX_SPREAD) {
        DBG_PRINT("MQ4: Readings too unstable (spread=");
        DBG_PRINT(spread);
        DBG_PRINTLN("), likely floating pin - no sensor connected");
        return false;
    }

    _mq4AnalogValue = avgVal;
    if (_mq4DigitalPin > 0) {
        _mq4DigitalAlarm = digitalRead(_mq4DigitalPin) == LOW;  // MQ4 digital output is active LOW
    }

    DBG_PRINT("MQ4: Active (analog=");
    DBG_PRINT(avgVal);
    DBG_PRINT(" alarm=");
    DBG_PRINT(_mq4DigitalAlarm ? "YES" : "NO");
    DBG_PRINTLN(")");
    return true;
}

// ============================================================================
// begin() - Initialize and auto-detect sensors
// ============================================================================

void SensorManager::begin(Preferences* prefs) {
    DBG_PRINTLN("=== Sensor Auto-Detection ===");

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
    DBG_PRINTLN("=== Sensor Summary ===");
    DBG_PRINT("BME280:  "); DBG_PRINTLN(_bmeStatus == SensorStatus::ACTIVE ? "ACTIVE" : "not found");
    DBG_PRINT("DS18X20: "); DBG_PRINTLN(_ds18x20Status == SensorStatus::ACTIVE ? "ACTIVE" : "not found");
    DBG_PRINT("DHT22:   "); DBG_PRINTLN(_dht22Status == SensorStatus::ACTIVE ? "ACTIVE" : "not found");
    DBG_PRINT("HC-SR04: "); DBG_PRINTLN(_hcsr04Status == SensorStatus::ACTIVE ? "ACTIVE" : "not found");
    DBG_PRINT("HC-SR501:"); DBG_PRINTLN(_hcsr501Status == SensorStatus::ACTIVE ? "ACTIVE" : "not found");
    DBG_PRINT("MQ4:     "); DBG_PRINTLN(_mq4Status == SensorStatus::ACTIVE ? "ACTIVE" : "not found");
    DBG_PRINTLN("======================");
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
