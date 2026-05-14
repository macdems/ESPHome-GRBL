# ESPHome GRBL Component

ESPHome external component for integrating **GRBL-based CNC controllers** with Home Assistant. It provides a seamless way to send commands, monitor state, and control your CNC machine from Home Assistant using an ESP32/ESP8266 device. Furthermore it acts as telnet-UART bridge, allowing you to connect to GRBL via telnet for remote command execution and monitoring.

This component allows you to:
- Send GRBL commands
- Control machine movement (jogging)
- Monitor GRBL state and settings
- Expose GRBL as sensors, switches, numbers, and buttons in Home Assistant
- Connect to your GRBL device via telnet for remote access

It is designed for ESP32/ESP8266 devices connected to a GRBL controller via UART.

## ✨ Features

- 📡 Remote communication with GRBL via telnet protocol
- 🎮 Send arbitrary G-code / GRBL commands
- 🕹️ Jog control (X/Y/Z movement)
- 🔘 Home Assistant buttons for common actions
- 🔄 Event-driven updates (no polling)
- 📊 Exposes GRBL settings as:
  - `sensor`
  - `binary_sensor`
  - `switch`
  - `number`
- ⚡ Native ESPHome automations support

### Remote Access via Telnet

The component includes a built-in telnet server that allows you to connect to your GRBL controller remotely. This is especially useful for sending commands or monitoring the machine state without needing direct physical access. By default, the telnet server listens on port 23, but you can configure it to use a different port if needed.

Due to the nature of the telnet protocol, only one client can be connected at a time. When a client is connected, the component will not allow sending commands from Home Assistant to avoid conflicts. However, you can configure this behavior to allow commands even when a telnet client is connected.

## 📦 Installation

Add this to your ESPHome YAML configuration:

```yaml
external_components:
  - source: github://macdems/ESPHome-GRBL
```

## 🔌 Hardware Setup

- ESP32 / ESP8266
- GRBL controller (e.g. Arduino Uno + CNC Shield)
- UART connection:

| ESP | GRBL |
| --- | ---- |
| TX  | RX   |
| RX  | TX   |
| GND | GND  |

> ⚠️ Ensure voltage compatibility (3.3V vs 5V)! Use level shifters if needed.

## ⚙️ Configuration

```yaml
uart:
  tx_pin: <your_esp_tx_pin>
  rx_pin: <your_esp_rx_pin>
  baud_rate: 115200

grbl:
  port: 23 # Optional: Default telnet port is 23
  allow_commands_when_connected: false # Optional: allow commands when telnet client is connected, default is false

```

### Sensors

The component exposes sensors for status reports and GRBL settings. You can create sensors for any GRBL parameter by specifying the `param`. It can be either a numeric value for a GRBL setting (e.g. `110` for `$110`) or a one of the following status parameters:

| Parameter          | Description                          |
| ------------------ | ------------------------------------ |
| `mpos`             | Machine position (MPos)              |
| `wpos`             | Work position (WPos)                 |
| `wco`              | Work coordinates offset (WCO)        |

In case of `mpos`, `wpos`, and `wco`, you need to specify the `axis` to select the X/Y/Z axis or the state value:

```yaml
sensor:
  - platform: grbl
    name: "X Machine Position"
    entity_param: mpos
    axis: x
```

To expose eg. the maximum feed rate for the X axis (`$110`):

```yaml
sensor:
  - platform: grbl
    name: "X Max Rate"
    entity_param: $110
```

### Binary Sensors

Binary GRBL configurations can be exposed as binary sensors. For example, you can create a binary sensor for the `$21` setting (hard limits enabled):

```yaml
binary_sensor:
  - platform: grbl
    name: "Hard Limits Enabled"
    param: $21
```

You can also expose the status of the the telnet connection, limit switches, or the probe as binary sensors:

```yaml
binary_sensor:
  - platform: grbl
    name: "X Limit Switch"
    param: limit
    axis: x
  - platform: grbl
    name: "Probe Triggered"
    param: probe
  - platform: grbl
    name: "GRBL Connected"
    param: client_connected
```

### Text Sensor

The current GRBL state (*Idle*, *Run*, *Alarm*, etc.) can be exposed as a text sensor:

```yaml
text_sensor:
  - platform: grbl
    name: "GRBL State"
    param: state
```

The following states are reported:

- `idle`
- `run`
- `hold`
- `jog`
- `alarm`
- `door`
- `check`
- `home`
- `sleep`
- `unknown`

### Switches

Binary GRBL settings can also be exposed as switches to allow toggling them from Home Assistant. For example, to toggle hard limits:

```yaml
switch:
  - platform: grbl
    name: "Hard Limits"
    param: $21
```

### Numbers

Similarly, numeric GRBL settings can be exposed as `number` entities to allow changing them from Home Assistant. For example, to change the maximum feed rate for the X axis (`$110`):

```yaml
number:
  - platform: grbl
    name: "X Max Rate"
    param: $110
    min_value: 0
    max_value: 10000
    step: 1
```

## 🎮 Actions

The component provides several actions to control GRBL from Home Assistant automations or scripts or eg. ESPHome button entities.

### Reset GRBL

To cancel any running command and reset GRBL:

```yaml
- grbl.send_reset
```

### Send command

Send any GRBL command or G-code line:

```yaml
- grbl.send_command: "G28"
```

or:

```yaml
- grbl.send_command:
    command: "G0 X10"
```

### Jog

Run jog commands to move the machine. You can specify relative movement (`dx`/`dy`/`dz`) and `speed`. This command uses the units set in your machine GRBL configuration (mm and mm/min or inches and in/min).

```yaml
- grbl.jog:
    dx: 10
    speed: 2000
```

To stop jogging:

```yaml
- grbl.cancel_jog
```

### Run Homing Cycle

If you have limit switches set up, you can run the homing cycle:

```yaml
- grbl.run_homing_cycle
```

### Release state

If GRBL is in an alarm or error state, you repeat the homing cycle or release the machine state:

```yaml
- grbl.release_state
```

### Set current position as home

You may set the current position as work coordinates home for X/Y/Z axes:

```yaml
- grbl.set_home:
    xy: true # Set X and Y as home
    z: true  # Set Z as home
    coords: G54 # Optional: select coordinate system (G54-G59 or G92), default is G54
```

### Probe Z
Run a Z probe cycle to measure the height of the tool. You can specify the `distance` to probe, `seek_rate` for the initial probing move, `feed_rate` for the final probing move, `offset` to apply to the measured height, and `retract` distance to move back after probing. You can also specify the coordinate system to use for the probe results.

```yaml
- grbl.probe_z:
    distance: auto
    seek_rate: 100.0
    feed_rate: 10.0
    offset: 0.0
    retract: 5.0
    coords: G54 # Optional: select coordinate system (G54-G59 or G92), default is G54
```

`distance: auto` will automatically calculate the probing distance based on the current Z position, which is useful to avoid crashing the tool into the bed. It will only work if homing is enabled and the machine has a valid position.


### Update status

By default, the component listens for GRBL status reports and updates entities automatically. However, it does not send status queries (`?`) on its own. If you want to trigger a status update (e.g. after sending a command), you can use the `grbl.update_status` action:

```yaml
- grbl.update_status
```

### Example

See [`example.yaml`](example.yaml) for a complete example configuration.

## 🧠 How It Works

- Parses GRBL responses (`$`, `?`, status reports)
- Maintains internal state of settings
- Uses **event-driven listeners** to update entities instantly
- Avoids polling → low latency & efficient

## ⚠️ Limitations

- Assumes standard GRBL serial protocol
- Not all GRBL features are implemented yet
- No G-code streaming (yet)

## 🛣️ Roadmap

- [ ] Streaming G-code support
- [ ] Status parsing improvements
- [ ] Alarm & error codes sensors

## 🤝 Contributing

Contributions are welcome!

- Open issues for bugs / ideas
- PRs with improvements
- Feature requests encouraged

## 📄 Copyright

Copyright (C) 2026 Maciek Dems <https://github.com/macdems>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
[GNU General Public License](LICENSE.md) for more details.

## 🙌 Acknowledgements

- GRBL project – open-source CNC firmware ([GitHub](https://github.com/grbl/grbl))
- ESPHome – amazing IoT framework
- Home Assistant community

## ⭐ Support

If you find this useful:

- ⭐ Star the repo
- Share your setups
- Contribute improvements
