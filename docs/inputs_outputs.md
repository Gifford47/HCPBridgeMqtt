# Digital Inputs & Outputs (In1 / In2 / Out1 / Out2)

The prebuilt HCP PCBs carry four screw terminals next to the sensor connectors:
**In1**, **In2**, **Out1** and **Out2**. They are plain ESP32 GPIOs brought out to
terminals — there is no relay, no level shifter and no protection circuitry on
the board.

Since firmware **v1.0.2** they are supported by the firmware: inputs become Home
Assistant `binary_sensor` entities (and can trigger a local door/light action),
outputs become Home Assistant `switch` entities.

> **The HCP mini does not have these terminals.** It runs the same
> `HCP_Giffordv3` firmware, which is why the whole feature is **disabled by
> default** and has to be enabled per channel in the Web UI.

---

## 1. Electrical behaviour — read this first

### Inputs (In1 / In2)

| Property | Value |
|---|---|
| Type | **Voltage input** through a fixed resistive divider (1.2 kΩ series + 2.2 kΩ to GND) |
| Expected signal | **~5 V logic level** against board GND (**not** a potential-free contact) |
| Divider ratio | 2.2 / (1.2 + 2.2) = **0.647** → 5 V in ≈ 3.24 V at the GPIO |
| Minimum HIGH | **≈ 3.9 V** at the terminal (ESP32 needs ≥ 0.75 × 3.3 V = 2.48 V at the pin) |
| **Maximum input voltage** | **≈ 5.1 V** — above that the GPIO exceeds 3.3 V |
| Idle level | LOW — the 2.2 kΩ pulls the GPIO to GND when nothing is applied |
| Input current | ≈ 1.5 mA at 5 V (3.4 kΩ total) |
| Pull resistor | **already on the PCB** (2.2 kΩ pull-down) — leave the firmware pull mode on *none* |
| Debounce | 50 ms in firmware (`IO_DEBOUNCE_MS`) |

**These are 5 V signal inputs, not dry-contact inputs.** The on-board divider is
dimensioned for 5 V. Two consequences:

- **A potential-free contact across the two In1 terminals does nothing** — it
  would merely bridge the 1.2 kΩ series resistor. A push button has to *switch a
  voltage* (≈5 V against board GND) onto the input.
- **The internal pull-up cannot be used.** The ESP32's internal pull-up is
  ~45 kΩ; against the on-board 2.2 kΩ pull-down the pin would sit at ~0.15 V and
  read LOW permanently. Keep *Pull resistor* on **none (external)** for the
  prebuilt PCBs — the pull mode only exists for self-built boards without this
  divider.

**Never feed 12 V or 24 V in** (e.g. straight from the motor): the divider would
put 7.8 V resp. 15.5 V on the GPIO and destroy it. Use an external optocoupler
or an additional divider that brings the signal down to ≈5 V first.

A 3.3 V source is **not** reliably detected either — it only produces 2.13 V at
the pin, below the ESP32's HIGH threshold.

### Outputs (Out1 / Out2)

| Property | Value |
|---|---|
| Type | **High-side switch** — optocoupler-driven N-channel MOSFET (source follower) |
| ON state | OUTx **sources** voltage: ≈ `VCC_SEL` − V_GS(th), i.e. roughly 0.7–1.5 V below the jumper level, load dependent |
| OFF state | High impedance (floating) — the gate is pulled to GND by 2.2 kΩ |
| Output voltage | Set by the `5V \| – \| 3V` jumper (`VCC_SEL`) |
| Reference | Board GND — the load returns to the same GND as the bridge |
| Suitable loads | PLC inputs, relay module inputs, optocoupler inputs, logic inputs |
| **Not** suitable | Driving loads directly (relays, lamps, motors, coils, mains loads) |

**Voltage selection:** the `5V | – | 3V` jumper selects the rail (`VCC_SEL`) the
outputs switch through. Because the MOSFET works as a source follower, the
actual output sits about a threshold voltage below that rail — with the jumper
on **3 V** you only get roughly 2.3 V, which is marginal for 3.3 V logic. Prefer
the **5 V** position unless the connected device explicitly needs 3.3 V.

```
        VCC_SEL (5V | 3V jumper)
              │
   ┌──────────┴─────────┐
   │  optocoupler  ──┐  │ drain
 GPIO ─[360Ω]─ LED   └─ gate ─┤ MOSFET (CJ3400)
              │              │ source
             GND   [2.2kΩ]───┘
              │              │
             GND           OUT1  ──► your device (returns to board GND)
```

- **ON** → optocoupler pulls the gate up to `VCC_SEL` → MOSFET conducts → OUT1
  follows the rail and can source current.
- **OFF** → the 2.2 kΩ pulls the gate to GND → MOSFET blocks → OUT1 floats. If
  the connected device needs a defined LOW, add an external pull-down.

**Do not drive a load directly.** For a relay, lamp or motor use a relay or
solid-state relay module with its own supply and feed only its *input* from
OUTx.

**No galvanic isolation:** the optocoupler's LED and its output transistor both
reference the same board GND, so it acts as a level-shifting driver stage, not
as an isolation barrier. Treat OUT1/OUT2 as GND-referenced to the bridge.

**Active low:** the *Active low* checkbox simply inverts the driving GPIO. Leave
it off — ON in Home Assistant then means "output active". Only tick it if ON/OFF
end up swapped for your particular wiring.

---

## 2. Pin assignment

| Terminal | HCP PCB v2 | HCP PCB v3.x | HCP mini | other boards |
|---|---|---|---|---|
| In1 | GPIO12 | GPIO12 | – | free choice |
| In2 | GPIO14 | GPIO14 | – | free choice |
| Out1 | GPIO37 | GPIO25 | – | free choice |
| Out2 | GPIO35 | GPIO22 | – | free choice |

The GPIO number is configurable in the Web UI, so a self-built board can use any
free pin. The firmware rejects pins that cannot work and reports the reason via
the debug MQTT entity (`io_error`):

- `0` → treated as "not configured"
- SPI flash pins (ESP32: 6–11, ESP32-S3: 26–32)
- GPIO34–39 on the classic ESP32 as an **output** (those pins are input-only)

---

## 3. Configuration in the Web UI

Open the web interface → **I/O Configuration**.

### Per input

| Field | Meaning |
|---|---|
| **Enable Input 1/2** | Activates the channel. Off = pin untouched, no HA entity. |
| **Name** | Entity name in Home Assistant (e.g. "Garage wall button"). |
| **GPIO** | Pin number (see table above). |
| **Pull resistor** | `none (external)` — the **correct setting for the prebuilt PCBs**, which already have a 2.2 kΩ pull-down (default). `internal pull-up` / `internal pull-down` only for self-built boards without that divider. |
| **Active low** | ON = pin LOW means "active". Default OFF, i.e. an applied voltage = active. Only turn it on for a self-built board with a button to GND plus internal pull-up. |
| **Local action** | What the ESP32 does itself on the activating edge — works even when Wi-Fi/MQTT is down. |

Available local actions:

| Action | Effect |
|---|---|
| none (MQTT only) | Only publishes the state — react in Home Assistant |
| impulse / toggle door | Same as the wall button: open ↔ stop ↔ close |
| open door | Drive to open |
| close door | Drive to close |
| stop door | Stop movement |
| **toggle light** | Toggles the SupraMatic courtesy light |
| vent position | Move to ventilation position |
| half position | Move to half-open position |
| toggle Output 1/2 | Switches one of the local outputs |

### Per output

| Field | Meaning |
|---|---|
| **Enable Output 1/2** | Activates the channel. Off = pin untouched, no HA entity. |
| **Name** | Entity name in Home Assistant. |
| **GPIO** | Pin number (see table above). |
| **Active low** | ON = the pin is driven LOW when the switch is ON (typical relay module). |
| **Restore after reboot** | ON = the last state is written to NVS and restored on boot. OFF (default) = the output is always OFF after a power loss — the safer choice for anything that moves. |

Saving the form restarts the device (like every configuration change).

---

## 4. Example: push button on In1 toggles the SupraMatic light

This is the setup from issue #147.

**Wiring:** the input needs a *switched voltage*, not a dry contact, so the
button switches ≈5 V onto In1:

```
  +5V ────o/ o──── In1 signal terminal
          push button
  (device GND tied to the bridge GND)
```

The 2.2 kΩ on the board pulls the input to GND while the button is open, so no
extra resistor is needed. If you only have a potential-free contact and no 5 V
nearby, use the 5 V rail of the bridge (see the `5V | – | 3V` jumper area) or
any other 4–5 V source that shares GND with the bridge.

**Web UI → I/O Configuration → Input 1:**

| Field | Value |
|---|---|
| Enable Input 1 | ✅ |
| Name | `Garage light button` |
| GPIO | `12` |
| Pull resistor | `none (external)` |
| Active low | ❌ |
| Local action | `toggle light` |

Save & restart. Pressing the button now toggles the light directly on the bus —
no Home Assistant, no MQTT, no Wi-Fi required. In addition the button state is
published as a `binary_sensor`, so you can also use the press as a trigger for
Home Assistant automations.

## 5. Example: Out1 drives a relay module

Out1 **sources** the module's input, so the module has to be an **active HIGH**
type (energises when `IN` is pulled high). Set the `5V | – | 3V` jumper to
**5 V** for a 5 V relay board.

**Wiring:** relay module `IN` → Out1 signal terminal, relay module `GND` → the
bridge GND, relay module `VCC` → its own 5 V supply.

For the common **active LOW** relay boards this stage is the wrong polarity —
those need their input pulled to GND. Drive them through a small inverting
transistor stage, or pick an active-high / optocoupler-input module.

**Web UI → I/O Configuration → Output 1:**

| Field | Value |
|---|---|
| Enable Output 1 | ✅ |
| Name | `Garage socket` |
| GPIO | `25` |
| Active low | ❌ (only tick this if ON/OFF end up swapped) |
| Restore after reboot | ❌ |

The switch appears in Home Assistant and in the Web UI status area.

---

## 6. MQTT

State topic (retained, published on every change):

```
hormann/<device_id>/io    {"in1":"true","in2":"false","out1":"false","out2":"true"}
```

Command topics for the outputs:

```
hormann/<device_id>/command/out1    true | false | toggle
hormann/<device_id>/command/out2    true | false | toggle
```

Only enabled channels appear in the payload. Example:

```bash
mosquitto_pub -h broker -t 'hormann/hcpbridge/command/out1' -m 'true'
mosquitto_sub -h broker -t 'hormann/hcpbridge/io'
```

### Home Assistant

Auto-discovery is sent at boot:

- `binary_sensor` for every enabled input, named after the configured name
- `switch` for every enabled output

Disabled channels get their retained discovery topic cleared, so entities
disappear from Home Assistant when you turn a channel off.

Example automation using an input as a trigger:

```yaml
automation:
  - alias: "Garage button also turns on the outside light"
    trigger:
      - platform: state
        entity_id: binary_sensor.garage_light_button
        to: "on"
    action:
      - service: light.turn_on
        target:
          entity_id: light.outside
```

---

## 7. HTTP API

| Route | Purpose |
|---|---|
| `GET /status` | contains an `io` object with the state of all enabled channels |
| `GET /sysinfo` | contains an `io` object with `active` / `off` per channel |
| `GET /io?output=1&state=on` | switch Out1 on (`on`, `off`, `toggle`) |
| `GET /io?output=2&state=toggle` | toggle Out2 |

Returns `404` if the addressed output is not enabled.

---

## 8. ESPHome

If you run the ESPHome firmware instead, `esphome_hcpbridge.yaml` ships
ready-to-uncomment blocks:

```yaml
binary_sensor:
  - platform: gpio
    name: "Garage Door Input 1"
    pin:
      number: GPIO12
      mode:
        input: true
      # no pullup and not inverted: the PCB divider already pulls the pin down
    filters:
      - delayed_on_off: 50ms
    on_press:
      - light.toggle: gd_light

switch:
  - platform: gpio
    name: "Garage Door Output 1"
    pin:
      number: GPIO25
      inverted: false
    restore_mode: ALWAYS_OFF
```

---

## 9. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Entity does not appear in HA | Channel not enabled, or discovery was only sent at boot — restart the device after enabling. |
| Input permanently ON, never changes | *Pull resistor* is set to `internal pull-up`: on the prebuilt PCB the 2.2 kΩ pull-down wins and the pin reads LOW forever, which with *Active low* looks like "always active". Set pull to `none (external)` and *Active low* off. |
| Input never reacts | Signal too low — the divider needs ≈4 V or more at the terminal; a 3.3 V source only produces 2.13 V at the pin. Or wrong GPIO. |
| Input toggles randomly | On a self-built board without the divider the pin is floating — select `internal pull-up` (or pull-down) there. |
| `io_error` in the debug entity | The configured pin is unusable on this chip (see section 2). The channel was auto-disabled. |
| Output measures ~1 V below the jumper level | Expected — the MOSFET is a source follower and drops about one gate threshold. |
| Output floats when off | Expected. Add an external pull-down if the connected device needs a defined LOW. |
| Relay never energises | The module is probably active LOW; this stage sources instead of sinking (see section 5). |
| Output is on right after boot | Turn off **Restore after reboot**, or fix the *Active low* setting. |
| Output does nothing | On the classic ESP32, GPIO34–39 cannot be outputs. Pick another pin. |
