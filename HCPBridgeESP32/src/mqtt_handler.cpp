#include "mqtt_handler.h"

// ============================================================================
// Helper
// ============================================================================

const char* MqttHandler::ToHA(bool value) {
    if (value) return HA_ON;
    return HA_OFF;
}

// ============================================================================
// Setup
// ============================================================================

void MqttHandler::setupMqttStrings() {
    String ftopic = "hormann/" + _prefs->getString(preference_gd_id);
    _mqttStrings.st_availability_topic = ftopic + "/availability";
    _mqttStrings.st_state_topic = ftopic + "/state";
    _mqttStrings.st_cmd_topic = ftopic + "/command";
    _mqttStrings.st_cmd_topic_var = _mqttStrings.st_cmd_topic + "/%s";
    _mqttStrings.st_cmd_topic_subs = _mqttStrings.st_cmd_topic + "/#";
    _mqttStrings.st_pos_topic = ftopic + "/position";
    _mqttStrings.st_setpos_topic = _mqttStrings.st_cmd_topic + "/set_position";
    _mqttStrings.st_lamp_topic = _mqttStrings.st_cmd_topic + "/lamp";
    _mqttStrings.st_door_topic = _mqttStrings.st_cmd_topic + "/door";
    _mqttStrings.st_vent_topic = _mqttStrings.st_cmd_topic + "/vent";
    _mqttStrings.st_half_topic = _mqttStrings.st_cmd_topic + "/half";
    _mqttStrings.st_step_topic = _mqttStrings.st_cmd_topic + "/step";
    _mqttStrings.st_sensor_topic = ftopic + "/sensor";
    _mqttStrings.st_debug_topic = ftopic + "/debug";

    strcpy(_mqttStrings.availability_topic, _mqttStrings.st_availability_topic.c_str());
    strcpy(_mqttStrings.state_topic, _mqttStrings.st_state_topic.c_str());
    strcpy(_mqttStrings.cmd_topic, _mqttStrings.st_cmd_topic.c_str());
    strcpy(_mqttStrings.pos_topic, _mqttStrings.st_pos_topic.c_str());
    strcpy(_mqttStrings.setpos_topic, _mqttStrings.st_setpos_topic.c_str());
    strcpy(_mqttStrings.lamp_topic, _mqttStrings.st_lamp_topic.c_str());
    strcpy(_mqttStrings.door_topic, _mqttStrings.st_door_topic.c_str());
    strcpy(_mqttStrings.vent_topic, _mqttStrings.st_vent_topic.c_str());
    strcpy(_mqttStrings.half_topic, _mqttStrings.st_half_topic.c_str());
    strcpy(_mqttStrings.step_topic, _mqttStrings.st_step_topic.c_str());
    strcpy(_mqttStrings.sensor_topic, _mqttStrings.st_sensor_topic.c_str());
    strcpy(_mqttStrings.debug_topic, _mqttStrings.st_debug_topic.c_str());
}

void MqttHandler::begin(Preferences* prefs, PreferenceHandler* prefHandler, SensorManager* sensorMgr) {
    _prefs = prefs;
    _prefHandler = prefHandler;
    _sensorMgr = sensorMgr;

    memset(lastCommandTopic, 0, sizeof(lastCommandTopic));
    memset(lastCommandPayload, 0, sizeof(lastCommandPayload));

    setupMqttStrings();

    // Set up callbacks using lambdas that capture 'this'
    _mqttClient.onConnect([this](bool sessionPresent) { this->onConnect(sessionPresent); });
    _mqttClient.onDisconnect([this](AsyncMqttClientDisconnectReason reason) { this->onDisconnect(reason); });
    _mqttClient.onMessage([this](char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
        this->onMessage(topic, payload, properties, len, index, total);
    });
    _mqttClient.onPublish([this](uint16_t packetId) { this->onPublish(packetId); });

    // Generate unique client ID
    static char uniqueID[17];
    uint64_t chipid = ESP.getEfuseMac();
    snprintf(uniqueID, sizeof(uniqueID), "ESP-%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);

    _mqttClient.setClientId(uniqueID);
    _mqttClient.setServer(prefHandler->getPreferencesCache()->mqtt_server, prefs->getInt(preference_mqtt_server_port));
    _mqttClient.setCredentials(prefHandler->getPreferencesCache()->mqtt_user, prefHandler->getPreferencesCache()->mqtt_password);
    setWill();
}

void MqttHandler::connectToMqtt() {
    if (!_sensorMgr->isReady()) {
        DBG_PRINTLN("MQTT: waiting for sensors...");
        xTimerStart(mqttReconnectTimer, 0);
        return;
    }
    DBG_PRINTLN("Connecting to MQTT...");
    _mqttClient.connect();
}

// ============================================================================
// Callbacks
// ============================================================================

void MqttHandler::onConnect(bool sessionPresent) {
    DBG_PRINTLN("Function on mqtt connect.");
    _mqttConnected = true;
    xTimerStop(mqttReconnectTimer, 0);
    sendOnline();
    _mqttClient.subscribe(_mqttStrings.st_cmd_topic_subs.c_str(), 1);
    updateDoorStatus(true);
    updateSensors(true);
    if (_bootFlag) {
        if (_sensorMgr->isReady()) {
            sendDiscoveryMessage();
        } else {
            _discoveryPending = true;
            DBG_PRINTLN("Discovery deferred: sensors not ready");
        }
        sendDebug();
        _bootFlag = false;
    }
}

void MqttHandler::onDisconnect(AsyncMqttClientDisconnectReason reason) {
    _mqttConnected = false;
    switch (reason) {
        case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED:
            DBG_PRINTLN("Disconnected from MQTT. reason : TCP_DISCONNECTED"); break;
        case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
            DBG_PRINTLN("Disconnected from MQTT. reason : MQTT_UNACCEPTABLE_PROTOCOL_VERSION"); break;
        case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED:
            DBG_PRINTLN("Disconnected from MQTT. reason : MQTT_IDENTIFIER_REJECTED"); break;
        case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE:
            DBG_PRINTLN("Disconnected from MQTT. reason : MQTT_SERVER_UNAVAILABLE"); break;
        case AsyncMqttClientDisconnectReason::ESP8266_NOT_ENOUGH_SPACE:
            DBG_PRINTLN("Disconnected from MQTT. reason : ESP8266_NOT_ENOUGH_SPACE"); break;
        case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS:
            DBG_PRINTLN("Disconnected from MQTT. reason : MQTT_MALFORMED_CREDENTIALS"); break;
        case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED:
            DBG_PRINTLN("Disconnected from MQTT. reason : MQTT_NOT_AUTHORIZED"); break;
        case AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT:
            DBG_PRINTLN("Disconnected from MQTT. reason :TLS_BAD_FINGERPRINT"); break;
        default: break;
    }

    if (WiFi.isConnected()) {
        xTimerStart(mqttReconnectTimer, 0);
    }
}

void MqttHandler::onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    strcpy(lastCommandTopic, topic);
    strncpy(lastCommandPayload, payload, len);
    lastCommandPayload[len] = '\0';

    if (strcmp(topic, _mqttStrings.lamp_topic) == 0) {
        if (strncmp(payload, HA_ON, len) == 0) {
            hoermannEngine->turnLight(true);
        } else if (strncmp(payload, HA_OFF, len) == 0) {
            hoermannEngine->turnLight(false);
        } else {
            hoermannEngine->toogleLight();
        }
    }
    else if (strcmp(_mqttStrings.door_topic, topic) == 0 ||
             strcmp(_mqttStrings.vent_topic, topic) == 0 ||
             strcmp(_mqttStrings.half_topic, topic) == 0 ||
             strcmp(_mqttStrings.step_topic, topic) == 0) {
        if (strncmp(payload, HA_OPEN, len) == 0) {
            hoermannEngine->openDoor();
        } else if (strncmp(payload, HA_CLOSE, len) == 0) {
            hoermannEngine->closeDoor();
        } else if (strncmp(payload, HA_STOP, len) == 0) {
            hoermannEngine->stopDoor();
        } else if (strncmp(payload, HA_HALF, len) == 0) {
            hoermannEngine->halfPositionDoor();
        } else if (strncmp(payload, HA_VENT, len) == 0) {
            hoermannEngine->ventilationPositionDoor();
        } else if (strncmp(payload, HA_STEP, len) == 0) {
            DBG_PRINTLN("STEPPING...");
            HoermannState::State currState = hoermannEngine->state->state;
            DBG_PRINTLN(currState);
            if (currState == HoermannState::State::CLOSED) {
                hoermannEngine->openDoor();
            } else if (currState == HoermannState::State::OPEN) {
                hoermannEngine->closeDoor();
            } else if (currState == HoermannState::State::CLOSING || currState == HoermannState::State::OPENING) {
                lastDoorState = currState;
                hoermannEngine->stopDoor();
            } else if (currState == HoermannState::State::STOPPED && lastDoorState == HoermannState::State::OPENING) {
                hoermannEngine->closeDoor();
            } else if (currState == HoermannState::State::STOPPED && lastDoorState == HoermannState::State::CLOSING) {
                hoermannEngine->openDoor();
            }
        }
    }
    else if (strcmp(_mqttStrings.setpos_topic, topic) == 0) {
        hoermannEngine->setPosition(atoi(lastCommandPayload));
    }
}

void MqttHandler::onPublish(uint16_t packetId) {
    // Nothing to do
}

// ============================================================================
// Status Updates
// ============================================================================

void MqttHandler::updateDoorStatus(bool forceUpdate) {
    if (hoermannEngine->state->changed || forceUpdate) {
        hoermannEngine->state->clearChanged();
        JsonDocument doc;
        char payload[1024];
        const char* venting = HA_CLOSE;
        const char* half = HA_CLOSE;

        doc["valid"] = hoermannEngine->state->valid;
        doc["doorposition"] = (int)(hoermannEngine->state->currentPosition * 100);
        doc["lamp"] = ToHA(hoermannEngine->state->lightOn);
        doc["doorstate"] = hoermannEngine->state->coverState;
        doc["detailedState"] = hoermannEngine->state->translatedState;
        if (hoermannEngine->state->translatedState == HA_VENT) {
            venting = HA_VENT;
        }
        doc["vent"] = venting;

        if (hoermannEngine->state->translatedState == HA_HALFOPEN) {
            half = HA_HALF;
        }
        doc["half"] = half;

        serializeJson(doc, payload);
        _mqttClient.publish(_mqttStrings.state_topic, 1, true, payload);

        sprintf(payload, "%d", (int)(hoermannEngine->state->currentPosition * 100));
        _mqttClient.publish(_mqttStrings.pos_topic, 1, true, payload);
    }
}

void MqttHandler::updateSensors(bool forceUpdate) {
    if (!_sensorMgr->hasAnySensor()) return;

    if (millis() - _sensorLastUpdate >= _sensorForceUpdateInterval) {
        forceUpdate = true;
    }

    if (_sensorMgr->hasNewData() || forceUpdate) {
        _sensorMgr->clearNewData();
        JsonDocument doc;
        char payload[1024];
        _sensorMgr->toJson(doc);
        serializeJson(doc, payload);
        _mqttClient.publish(_mqttStrings.sensor_topic, 0, false, payload);
        _sensorLastUpdate = millis();
    }
}

void MqttHandler::publishMotionState(int state) {
    if (!_mqttConnected) return;
    JsonDocument doc;
    char payload[1024];
    doc["motion"] = state ? HA_ON : HA_OFF;
    serializeJson(doc, payload);
    _mqttClient.publish(_mqttStrings.sensor_topic, 0, true, payload);
}

void MqttHandler::sendOnline() {
    _mqttClient.publish(_mqttStrings.availability_topic, 0, true, HA_ONLINE);
}

void MqttHandler::setWill() {
    _mqttClient.setWill(_mqttStrings.availability_topic, 0, true, HA_OFFLINE);
}

void MqttHandler::sendDebug() {
    JsonDocument doc;
    char payload[1024];
    doc["restart_reason"] = esp_reset_reason();
    doc["debug"] = hoermannEngine->state->debugMessage;
    String sensorErr = _sensorMgr->getLastError();
    if (sensorErr.length() > 0) {
        doc["sensor_error"] = sensorErr;
    }
    serializeJson(doc, payload);
    _mqttClient.publish(_mqttStrings.debug_topic, 0, true, payload);
}

// ============================================================================
// Discovery Messages
// ============================================================================

void MqttHandler::sendDiscoveryMessageForBinarySensor(const char name[], const char topic[], const char key[], const char off[], const char on[], const JsonDocument& device) {
    char full_topic[64];
    sprintf(full_topic, HA_DISCOVERY_BIN_SENSOR, _prefs->getString(preference_gd_id), key);

    char uid[64];
    sprintf(uid, "%s_binary_sensor_%s", _prefs->getString(preference_gd_id), key);

    char vtemp[64];
    sprintf(vtemp, "{{ value_json.%s }}", key);

    JsonDocument doc;
    doc["name"] = name;
    doc["state_topic"] = topic;
    doc["availability_topic"] = _mqttStrings.availability_topic;
    doc["payload_available"] = HA_ONLINE;
    doc["payload_not_available"] = HA_OFFLINE;
    doc["unique_id"] = uid;
    doc["value_template"] = vtemp;
    doc["payload_on"] = on;
    doc["payload_off"] = off;
    doc["device"] = device;

    char payload[1024];
    serializeJson(doc, payload);
    _mqttClient.publish(full_topic, 1, true, payload);
}

void MqttHandler::sendDiscoveryMessageForAVSensor(const JsonDocument& device) {
    char full_topic[64];
    sprintf(full_topic, HA_DISCOVERY_AV_SENSOR, _prefs->getString(preference_gd_id));

    char uid[64];
    sprintf(uid, "%s_sensor_availability", _prefs->getString(preference_gd_id));

    JsonDocument doc;
    doc["name"] = GD_AVAIL;
    doc["state_topic"] = _mqttStrings.availability_topic;
    doc["unique_id"] = uid;
    doc["device"] = device;

    char payload[1024];
    serializeJson(doc, payload);
    _mqttClient.publish(full_topic, 1, true, payload);
}

void MqttHandler::sendDiscoveryMessageForSensor(const char name[], const char topic[], const char key[], const JsonDocument& device, const char device_class[], const char unit[]) {
    char full_topic[64];
    sprintf(full_topic, HA_DISCOVERY_SENSOR, _prefs->getString(preference_gd_id), key);

    char uid[64];
    sprintf(uid, "%s_sensor_%s", _prefs->getString(preference_gd_id), key);

    char vtemp[64];
    if (key == "hum" || key == "temp" || key == "pres") {
        sprintf(vtemp, "{{ value_json.%s | float }}", key);
    } else {
        sprintf(vtemp, "{{ value_json.%s }}", key);
    }

    JsonDocument doc;
    doc["name"] = name;
    doc["state_topic"] = topic;
    doc["availability_topic"] = _mqttStrings.availability_topic;
    doc["payload_available"] = HA_ONLINE;
    doc["payload_not_available"] = HA_OFFLINE;
    doc["value_template"] = vtemp;
    doc["device"] = device;

    if (device_class != "") {
        doc["device_class"] = device_class;
    }

    doc["unit_of_measurement"] = unit;
    doc["unique_id"] = uid;

    if (key == "hum") {
        doc["state_class"] = "measurement";
        doc["unit_of_measurement"] = "%";
        doc["device_class"] = "humidity";
    } else if (key == "temp") {
        doc["state_class"] = "measurement";
        doc["unit_of_measurement"] = "°C";
        doc["device_class"] = "temperature";
    } else if (key == "pres") {
        doc["state_class"] = "measurement";
        doc["unit_of_measurement"] = "hPa";
        doc["device_class"] = "pressure";
    }

    char payload[1024];
    serializeJson(doc, payload);
    _mqttClient.publish(full_topic, 1, true, payload);
}

void MqttHandler::sendDiscoveryMessageForSwitch(const char name[], const char discovery[], const char topic[], const char off[], const char on[], const char icon[], const JsonDocument& device, bool optimistic) {
    char command_topic[64];
    sprintf(command_topic, _mqttStrings.st_cmd_topic_var.c_str(), topic);

    char full_topic[64];
    sprintf(full_topic, discovery, _prefs->getString(preference_gd_id), topic);

    char value_template[64];
    sprintf(value_template, "{{ value_json.%s }}", topic);

    char uid[64];
    if (discovery == HA_DISCOVERY_LIGHT) {
        sprintf(uid, "%s_light_%s", _prefs->getString(preference_gd_id), topic);
    } else {
        sprintf(uid, "%s_switch_%s", _prefs->getString(preference_gd_id), topic);
    }

    JsonDocument doc;
    doc["name"] = name;
    doc["state_topic"] = _mqttStrings.state_topic;
    doc["command_topic"] = command_topic;
    doc["payload_on"] = on;
    doc["payload_off"] = off;
    doc["icon"] = icon;
    doc["availability_topic"] = _mqttStrings.availability_topic;
    doc["payload_available"] = HA_ONLINE;
    doc["payload_not_available"] = HA_OFFLINE;
    doc["unique_id"] = uid;
    doc["value_template"] = value_template;
    doc["optimistic"] = optimistic;
    doc["device"] = device;

    char payload[1024];
    serializeJson(doc, payload);
    _mqttClient.publish(full_topic, 1, true, payload);
}

void MqttHandler::sendDiscoveryMessageForButton(const char name[], const char topic[], const char payload_press[], const char icon[], const JsonDocument& device) {
    char command_topic[64];
    sprintf(command_topic, _mqttStrings.st_cmd_topic_var.c_str(), topic);

    char full_topic[64];
    sprintf(full_topic, HA_DISCOVERY_BUTTON, _prefs->getString(preference_gd_id), topic);

    char uid[64];
    sprintf(uid, "%s_button_%s", _prefs->getString(preference_gd_id), topic);

    JsonDocument doc;
    doc["name"] = name;
    doc["command_topic"] = command_topic;
    doc["payload_press"] = payload_press;
    doc["icon"] = icon;
    doc["availability_topic"] = _mqttStrings.availability_topic;
    doc["payload_available"] = HA_ONLINE;
    doc["payload_not_available"] = HA_OFFLINE;
    doc["unique_id"] = uid;
    doc["device"] = device;

    char payload[1024];
    serializeJson(doc, payload);
    _mqttClient.publish(full_topic, 1, true, payload);
}

void MqttHandler::sendDiscoveryMessageForCover(const char name[], const char topic[], const JsonDocument& device) {
    char command_topic[64];
    sprintf(command_topic, _mqttStrings.st_cmd_topic_var.c_str(), topic);

    char full_topic[64];
    sprintf(full_topic, HA_DISCOVERY_COVER, _prefs->getString(preference_gd_id), topic);

    char uid[64];
    sprintf(uid, "%s_cover_%s", _prefs->getString(preference_gd_id), topic);

    JsonDocument doc;
    doc["name"] = name;
    doc["state_topic"] = _mqttStrings.state_topic;
    doc["command_topic"] = command_topic;
    doc["position_topic"] = _mqttStrings.pos_topic;
    doc["set_position_topic"] = _mqttStrings.setpos_topic;
    doc["position_open"] = 100;
    doc["position_closed"] = 0;

    doc["payload_open"] = HA_OPEN;
    doc["payload_close"] = HA_CLOSE;
    doc["payload_stop"] = HA_STOP;
    doc["payload_step"] = HA_STEP;
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
    doc["availability_topic"] = _mqttStrings.availability_topic;
    doc["payload_available"] = HA_ONLINE;
    doc["payload_not_available"] = HA_OFFLINE;
    doc["unique_id"] = uid;
    doc["device_class"] = "garage";
    doc["device"] = device;

    char payload[1024];
    serializeJson(doc, payload);
    _mqttClient.publish(full_topic, 1, true, payload);
}

void MqttHandler::sendDiscoveryMessage() {
    JsonDocument device;
    device["identifiers"] = _prefs->getString(preference_gd_name);
    device["name"] = _prefs->getString(preference_gd_name);
    device["sw_version"] = HA_VERSION;
    device["model"] = "Garage Door";
    device["manufacturer"] = "Hörmann";
    String configUrl = "http://" + WiFi.localIP().toString();
    device["configuration_url"] = configUrl;

    // Helper to remove retained discovery topics
    auto clearTopic = [&](const char* fmt, const char* key) {
        char t[64]; sprintf(t, fmt, _prefs->getString(preference_gd_id), key);
        _mqttClient.publish(t, 1, true, "");
    };

    sendDiscoveryMessageForAVSensor(device);
    sendDiscoveryMessageForSwitch(GD_LIGHT, HA_DISCOVERY_SWITCH, "lamp", HA_OFF, HA_ON, "mdi:lightbulb", device);
    sendDiscoveryMessageForBinarySensor(GD_LIGHT, _mqttStrings.state_topic, "lamp", HA_OFF, HA_ON, device);
    sendDiscoveryMessageForSwitch(GD_VENT, HA_DISCOVERY_SWITCH, "vent", HA_CLOSE, HA_VENT, "mdi:air-filter", device);
    sendDiscoveryMessageForSwitch(GD_HALF, HA_DISCOVERY_SWITCH, "half", HA_CLOSE, HA_HALF, "mdi:air-filter", device);
    sendDiscoveryMessageForButton(GD_TOGGLE, "step", HA_STEP, "mdi:remote", device);
    sendDiscoveryMessageForCover(_prefs->getString(preference_gd_name).c_str(), "door", device);

    sendDiscoveryMessageForSensor(GD_STATUS, _mqttStrings.state_topic, "doorstate", device, "enum");
    sendDiscoveryMessageForSensor(GD_DET_STATUS, _mqttStrings.state_topic, "detailedState", device, "enum");
    sendDiscoveryMessageForSensor(GD_POSITIOM, _mqttStrings.state_topic, "doorposition", device);

    if (_sensorMgr->hasTempSensor()) {
        sendDiscoveryMessageForSensor(_prefs->getString(preference_gs_temp).c_str(), _mqttStrings.sensor_topic, "temp", device, "temperature", "°C");
    } else { clearTopic(HA_DISCOVERY_SENSOR, "temp"); }
    if (_sensorMgr->hasHumiditySensor()) {
        sendDiscoveryMessageForSensor(_prefs->getString(preference_gs_hum).c_str(), _mqttStrings.sensor_topic, "hum", device, "humidity", "%");
    } else { clearTopic(HA_DISCOVERY_SENSOR, "hum"); }
    if (_sensorMgr->hasPressureSensor()) {
        sendDiscoveryMessageForSensor(_prefs->getString(preference_gs_pres).c_str(), _mqttStrings.sensor_topic, "pres", device, "atmospheric_pressure", "hPa");
    } else { clearTopic(HA_DISCOVERY_SENSOR, "pres"); }
    if (_sensorMgr->hasDistanceSensor()) {
        sendDiscoveryMessageForSensor(_prefs->getString(preference_gs_free_dist).c_str(), _mqttStrings.sensor_topic, "dist", device, "distance", "cm");
        sendDiscoveryMessageForBinarySensor(_prefs->getString(preference_gs_park_avail).c_str(), _mqttStrings.sensor_topic, "free", HA_OFF, HA_ON, device);
    } else { clearTopic(HA_DISCOVERY_SENSOR, "dist"); clearTopic(HA_DISCOVERY_BIN_SENSOR, "free"); }
    if (_sensorMgr->hasMotionSensor()) {
        sendDiscoveryMessageForBinarySensor(_prefs->getString(preference_gs_motion).c_str(), _mqttStrings.sensor_topic, "motion", HA_OFF, HA_ON, device);
    } else { clearTopic(HA_DISCOVERY_BIN_SENSOR, "motion"); }
    if (_sensorMgr->hasGasSensor()) {
        sendDiscoveryMessageForSensor(_prefs->getString(preference_gs_gas).c_str(), _mqttStrings.sensor_topic, "gas", device, "volatile_organic_compounds_parts", "ppm");
        sendDiscoveryMessageForBinarySensor(_prefs->getString(preference_gs_gas_alarm).c_str(), _mqttStrings.sensor_topic, "gas_alarm", HA_OFF, HA_ON, device);
    } else { clearTopic(HA_DISCOVERY_SENSOR, "gas"); clearTopic(HA_DISCOVERY_BIN_SENSOR, "gas_alarm"); }

    // Debug entities are always active (independent of debug_enabled)
    sendDiscoveryMessageForSensor(_prefs->getString(preference_gd_debug).c_str(), _mqttStrings.debug_topic, "debug", device);
    sendDiscoveryMessageForSensor(_prefs->getString(preference_gd_debug_restart).c_str(), _mqttStrings.debug_topic, "restart_reason", device);
}

// ============================================================================
// Task function
// ============================================================================

void MqttHandler::taskFunc() {
    if (_mqttConnected) {
        if (_discoveryPending && _sensorMgr->isReady()) {
            _discoveryPending = false;
            sendDiscoveryMessage();
        }
        updateDoorStatus();
        updateSensors();
        if (hoermannEngine->state->debMessage) {
            hoermannEngine->state->clearDebug();
            sendDebug();
        }
    }
}
