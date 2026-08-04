#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#include "configuration.h"
#include "preferences_handler.h"

// ============================================================================
// Digital I/O - In1/In2 and Out1/Out2 screw terminals of the prebuilt HCP PCBs
//
// Inputs are debounced, published to MQTT as binary_sensor and can optionally
// trigger a local door/light action so a wall button keeps working when the
// network is down. On the HCP PCBs each input sits behind a 1.2k/2.2k divider
// (~5 V in -> 3.24 V at the pin) with the 2.2k acting as a hard pull-down, so
// the internal pulls must stay off there - hence IO_PULL_NONE as default.
//
// Outputs are exposed as HA switches. On the HCP PCBs the GPIO drives an
// optocoupler that pulls the gate of an N-channel MOSFET to the jumper-selected
// rail (VCC_SEL, 5 V or 3 V): ON sources roughly VCC_SEL minus V_GS(th),
// OFF is high impedance. Note both optocoupler sides share the board GND, so
// this is a level-shifting driver stage, not a galvanic isolation barrier.
// ============================================================================

#define IO_INPUT_COUNT 2
#define IO_OUTPUT_COUNT 2

struct IoInputChannel {
    const char* key;          // JSON/MQTT key ("in1", "in2")
    bool enabled = false;
    int pin = 0;
    uint8_t pull = IO_PULL_NONE;
    bool inverted = false;    // true: pin LOW = active (e.g. button to GND + pullup)
    uint8_t action = IO_ACTION_NONE;
    String name;
    bool state = false;       // debounced logical state
    bool pending = false;     // last sampled logical state
    unsigned long lastEdge = 0;
};

struct IoOutputChannel {
    const char* key;          // JSON/MQTT key ("out1", "out2")
    bool enabled = false;
    int pin = 0;
    bool inverted = false;    // true: swap ON/OFF (isolation stage / wiring dependent)
    bool restore = false;     // restore last state after reboot
    String name;
    bool state = false;
};

// ============================================================================
// IoManager Class
// ============================================================================

class IoManager {
public:
    // Configure and initialize the enabled channels - call in setup()
    void begin(Preferences* prefs);

    // Sample inputs, debounce and run local actions - call from IoTask
    void poll();

    bool hasInputs() const;
    bool hasOutputs() const;
    bool hasAny() const { return hasInputs() || hasOutputs(); }

    // Any input/output changed since the last clearChanged()
    bool hasChanged() const { return _changed; }
    void clearChanged() { _changed = false; }

    const IoInputChannel& input(int idx) const { return _inputs[idx]; }
    const IoOutputChannel& output(int idx) const { return _outputs[idx]; }

    // Drive an output. Returns false if the channel is disabled/unknown.
    bool setOutput(int idx, bool on);
    bool toggleOutput(int idx);

    // MQTT payload for the io topic
    void toJson(JsonDocument& doc) const;
    // /status HTTP endpoint (values only for enabled channels)
    void toStatusJson(JsonObject& io) const;
    // /sysinfo HTTP endpoint (channel status)
    void toDetectionJson(JsonObject& io) const;

    // Last error string (appended to the debug entity)
    String getLastError() const { return _lastError; }

private:
    void applyOutput(IoOutputChannel& ch);
    void runAction(uint8_t action);
    void setError(const char* msg);
    static bool pinUsable(int pin, bool asOutput);

    Preferences* _prefs = nullptr;
    IoInputChannel _inputs[IO_INPUT_COUNT] = {{"in1"}, {"in2"}};
    IoOutputChannel _outputs[IO_OUTPUT_COUNT] = {{"out1"}, {"out2"}};
    volatile bool _changed = false;
    String _lastError = "";
};
