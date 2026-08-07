#include "io_manager.h"
#include "hoermann.h"

// ============================================================================
// Helpers
// ============================================================================

void IoManager::setError(const char* msg) {
    if (_lastError.length() > 0) {
        _lastError += "; ";
    }
    _lastError += msg;
    DBG_PRINTLN(msg);
}

// Reject pins that cannot be used at all on the current chip.
// Pin 0 is treated as "not configured" - it is the reset button and the
// placeholder default on boards without In/Out terminals.
bool IoManager::pinUsable(int pin, bool asOutput) {
    if (pin <= 0) return false;
    #ifdef CONFIG_IDF_TARGET_ESP32S3
        if (pin > 48) return false;
        if (pin >= 26 && pin <= 32) return false;   // SPI flash / PSRAM
    #else
        if (pin > 39) return false;
        if (pin >= 6 && pin <= 11) return false;    // SPI flash
        if (asOutput && pin >= 34) return false;    // GPIO34-39 are input only
    #endif
    return true;
}

// ============================================================================
// Setup
// ============================================================================

void IoManager::begin(Preferences* prefs) {
    _prefs = prefs;

    static const char* IN_EN[]   = {preference_io_in1_enabled,  preference_io_in2_enabled};
    static const char* IN_PIN[]  = {preference_io_in1_pin,      preference_io_in2_pin};
    static const char* IN_PULL[] = {preference_io_in1_pull,     preference_io_in2_pull};
    static const char* IN_INV[]  = {preference_io_in1_inverted, preference_io_in2_inverted};
    static const char* IN_ACT[]  = {preference_io_in1_action,   preference_io_in2_action};
    static const char* IN_NAME[] = {preference_io_in1_name,     preference_io_in2_name};
    static const int   IN_DEF[]  = {INPUT1, INPUT2};
    static const char* IN_LBL[]  = {GIO_IN1, GIO_IN2};

    static const char* OUT_EN[]   = {preference_io_out1_enabled,  preference_io_out2_enabled};
    static const char* OUT_PIN[]  = {preference_io_out1_pin,      preference_io_out2_pin};
    static const char* OUT_INV[]  = {preference_io_out1_inverted, preference_io_out2_inverted};
    static const char* OUT_RST[]  = {preference_io_out1_restore,  preference_io_out2_restore};
    static const char* OUT_ST[]   = {preference_io_out1_state,    preference_io_out2_state};
    static const char* OUT_NAME[] = {preference_io_out1_name,     preference_io_out2_name};
    static const int   OUT_DEF[]  = {OUTPUT1, OUTPUT2};
    static const char* OUT_LBL[]  = {GIO_OUT1, GIO_OUT2};

    DBG_PRINTLN("=== Digital I/O Init ===");

    for (int i = 0; i < IO_INPUT_COUNT; i++) {
        IoInputChannel& ch = _inputs[i];
        ch.enabled  = prefs->getBool(IN_EN[i], false);
        ch.pin      = prefs->getInt(IN_PIN[i], IN_DEF[i]);
        ch.pull     = (uint8_t)prefs->getInt(IN_PULL[i], IO_PULL_NONE);
        ch.inverted = prefs->getBool(IN_INV[i], false);
        ch.action   = (uint8_t)prefs->getInt(IN_ACT[i], IO_ACTION_NONE);
        ch.name     = prefs->getString(IN_NAME[i], IN_LBL[i]);

        if (!ch.enabled) continue;

        if (!pinUsable(ch.pin, false)) {
            char msg[48];
            snprintf(msg, sizeof(msg), "%s: invalid pin %d", ch.key, ch.pin);
            setError(msg);
            ch.enabled = false;
            continue;
        }

        if (ch.pull == IO_PULL_UP) {
            pinMode(ch.pin, INPUT_PULLUP);
        } else if (ch.pull == IO_PULL_DOWN) {
            pinMode(ch.pin, INPUT_PULLDOWN);
        } else {
            pinMode(ch.pin, INPUT);
        }

        // Seed the debounce state so a held-low input does not fire on boot
        bool raw = digitalRead(ch.pin);
        ch.state = ch.pending = ch.inverted ? !raw : raw;
        ch.lastEdge = millis();

        DBG_PRINTF("%s: GPIO%d pull=%d inv=%d action=%d\n", ch.key, ch.pin, ch.pull, ch.inverted, ch.action);
    }

    for (int i = 0; i < IO_OUTPUT_COUNT; i++) {
        IoOutputChannel& ch = _outputs[i];
        ch.enabled  = prefs->getBool(OUT_EN[i], false);
        ch.pin      = prefs->getInt(OUT_PIN[i], OUT_DEF[i]);
        ch.inverted = prefs->getBool(OUT_INV[i], false);
        ch.restore  = prefs->getBool(OUT_RST[i], false);
        ch.name     = prefs->getString(OUT_NAME[i], OUT_LBL[i]);

        if (!ch.enabled) continue;

        if (!pinUsable(ch.pin, true)) {
            char msg[48];
            snprintf(msg, sizeof(msg), "%s: invalid pin %d", ch.key, ch.pin);
            setError(msg);
            ch.enabled = false;
            continue;
        }

        ch.state = ch.restore ? prefs->getBool(OUT_ST[i], false) : false;
        pinMode(ch.pin, OUTPUT);
        applyOutput(ch);

        DBG_PRINTF("%s: GPIO%d inv=%d restore=%d state=%d\n", ch.key, ch.pin, ch.inverted, ch.restore, ch.state);
    }

    DBG_PRINTLN("========================");
}

// ============================================================================
// Outputs
// ============================================================================

void IoManager::applyOutput(IoOutputChannel& ch) {
    digitalWrite(ch.pin, (ch.inverted ? !ch.state : ch.state) ? HIGH : LOW);
}

bool IoManager::setOutput(int idx, bool on) {
    if (idx < 0 || idx >= IO_OUTPUT_COUNT) return false;
    IoOutputChannel& ch = _outputs[idx];
    if (!ch.enabled) return false;

    if (ch.state != on) {
        ch.state = on;
        applyOutput(ch);
        _changed = true;
        if (ch.restore && _prefs) {
            _prefs->putBool(idx == 0 ? preference_io_out1_state : preference_io_out2_state, on);
        }
        DBG_PRINTF("%s -> %d\n", ch.key, on);
    }
    return true;
}

bool IoManager::toggleOutput(int idx) {
    if (idx < 0 || idx >= IO_OUTPUT_COUNT) return false;
    return setOutput(idx, !_outputs[idx].state);
}

// ============================================================================
// Inputs
// ============================================================================

void IoManager::runAction(uint8_t action) {
    switch (action) {
        case IO_ACTION_TOGGLE_DOOR:  hoermannEngine->toogleDoor(); break;
        case IO_ACTION_OPEN:         hoermannEngine->openDoor(); break;
        case IO_ACTION_CLOSE:        hoermannEngine->closeDoor(); break;
        case IO_ACTION_STOP:         hoermannEngine->stopDoor(); break;
        case IO_ACTION_TOGGLE_LIGHT: hoermannEngine->toogleLight(); break;
        case IO_ACTION_VENT:         hoermannEngine->ventilationPositionDoor(); break;
        case IO_ACTION_HALF:         hoermannEngine->halfPositionDoor(); break;
        case IO_ACTION_TOGGLE_OUT1:  toggleOutput(0); break;
        case IO_ACTION_TOGGLE_OUT2:  toggleOutput(1); break;
        default: break;
    }
}

void IoManager::poll() {
    unsigned long now = millis();

    for (int i = 0; i < IO_INPUT_COUNT; i++) {
        IoInputChannel& ch = _inputs[i];
        if (!ch.enabled) continue;

        bool raw = digitalRead(ch.pin);
        bool logical = ch.inverted ? !raw : raw;

        if (logical != ch.pending) {
            ch.pending = logical;
            ch.lastEdge = now;
            continue;
        }

        if (logical != ch.state && (now - ch.lastEdge) >= IO_DEBOUNCE_MS) {
            ch.state = logical;
            _changed = true;
            DBG_PRINTF("%s = %d\n", ch.key, ch.state);
            // Local action fires on the activating edge only
            if (ch.state) runAction(ch.action);
        }
    }
}

// ============================================================================
// Queries / Serialization
// ============================================================================

bool IoManager::hasInputs() const {
    for (int i = 0; i < IO_INPUT_COUNT; i++) {
        if (_inputs[i].enabled) return true;
    }
    return false;
}

bool IoManager::hasOutputs() const {
    for (int i = 0; i < IO_OUTPUT_COUNT; i++) {
        if (_outputs[i].enabled) return true;
    }
    return false;
}

void IoManager::toJson(JsonDocument& doc) const {
    for (int i = 0; i < IO_INPUT_COUNT; i++) {
        if (_inputs[i].enabled) doc[_inputs[i].key] = _inputs[i].state ? HA_ON : HA_OFF;
    }
    for (int i = 0; i < IO_OUTPUT_COUNT; i++) {
        if (_outputs[i].enabled) doc[_outputs[i].key] = _outputs[i].state ? HA_ON : HA_OFF;
    }
}

void IoManager::toStatusJson(JsonObject& io) const {
    for (int i = 0; i < IO_INPUT_COUNT; i++) {
        if (_inputs[i].enabled) io[_inputs[i].key] = _inputs[i].state;
    }
    for (int i = 0; i < IO_OUTPUT_COUNT; i++) {
        if (_outputs[i].enabled) io[_outputs[i].key] = _outputs[i].state;
    }
}

void IoManager::toDetectionJson(JsonObject& io) const {
    for (int i = 0; i < IO_INPUT_COUNT; i++) {
        io[_inputs[i].key] = _inputs[i].enabled ? "active" : "off";
    }
    for (int i = 0; i < IO_OUTPUT_COUNT; i++) {
        io[_outputs[i].key] = _outputs[i].enabled ? "active" : "off";
    }
}
