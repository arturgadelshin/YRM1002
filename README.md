# YRM1002 UHF RFID Reader — ESPHome Component

ESPHome external component for the **YRM1002** UHF RFID reader module. Supports multi-tag inventory scanning, tag detection/removal events, per-tag binary sensors, and is fully configurable from YAML.

## Features

- Multi-tag inventory scanning via UART
- Configurable scan duration, update interval, and repeat count
- `on_tag` / `on_tag_removed` triggers with EPC and RSSI data
- Built-in `tag_present` binary sensor (ON = tag in range, OFF = no tags)
- Per-UID binary sensors for individual tag tracking
- Text sensor with last scanned tag EPC
- Home Assistant `tag_scanned` event integration

## Hardware

| Parameter | Value |
|---|---|
| Interface | UART (115200 baud, 8N1) |
| Operating voltage | 5V (use level converter for 3.3V MCU) |
| Frequency | 840–960 MHz (UHF) |
| Read range | Up to ~30 cm (depends on antenna/tag) |

### Wiring

```
ESP32 GPIO TX  ──→  Level Converter  ──→  YRM1002 RX
ESP32 GPIO RX  ←──  Level Converter  ←──  YRM1002 TX
GND            ──────────────────────────  YRM1002 GND
5V             ──────────────────────────  YRM1002 VCC
```

## Installation

Add this component as an external component in your ESPHome YAML:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/arturgadelshin/YRM1002
    components: [ yrm1002 ]
```

## Configuration

### Minimal Example

```yaml
uart:
  - id: rfid_uart
    tx_pin: GPIO3
    rx_pin: GPIO6
    baud_rate: 115200
    rx_buffer_size: 1024

yrm1002:
  - id: my_rfid
    uart_id: rfid_uart
```

### Full Example

```yaml
uart:
  - id: rfid_uart
    tx_pin: GPIO3
    rx_pin: GPIO6
    baud_rate: 115200
    data_bits: 8
    parity: NONE
    stop_bits: 1
    rx_buffer_size: 1024

yrm1002:
  - id: my_rfid
    uart_id: rfid_uart
    scan_duration: 500ms
    update_interval: 1s
    repeat_count: 10
    power: 26
    tag_present:
      name: "RFID Tag Present"
    on_tag:
      then:
        - homeassistant.tag_scanned: !lambda 'return x;'
    on_tag_removed:
      then:
        - lambda: |-
            ESP_LOGI("yrm1002", "Tag removed: %s", x.c_str());

text_sensor:
  - platform: yrm1002
    yrm1002_id: my_rfid
    name: "Last RFID Tag EPC"

binary_sensor:
  - platform: yrm1002
    yrm1002_id: my_rfid
    uid: "E28068940000401A48B8507E"
    name: "My Tag"
```

## Configuration Variables

### `yrm1002` (Main Component)

| Key | Type | Default | Description |
|---|---|---|---|
| `id` | ID | Required | Component ID for referencing from other platforms |
| `uart_id` | ID | Required | ID of the UART bus connected to the reader |
| `scan_duration` | Time | `500ms` | How long each scan cycle lasts |
| `update_interval` | Time | `1s` | Interval between scan cycles |
| `repeat_count` | int | `10` | Number of inventory attempts per scan cycle (1–255) |
| `power` | int | `26` | RF output power in dBm (0–26) |
| `tag_present` | binary_sensor | Optional | Binary sensor: ON when any tag detected, OFF when none |
| `on_tag` | trigger | Optional | Automation triggered when a new tag is detected |
| `on_tag_removed` | trigger | Optional | Automation triggered when a tag leaves the read range |

### `on_tag` Trigger Variables

| Variable | Type | Description |
|---|---|---|
| `x` | string | Tag EPC (hex string, e.g. `E28068940000401A48B8507E`) |
| `tag` | TagInfo struct | Tag info with `.epc`, `.rssi`, `.pc`, `.crc` fields |

### `on_tag_removed` Trigger Variables

| Variable | Type | Description |
|---|---|---|
| `x` | string | Tag EPC that was removed |

### Text Sensor (`text_sensor` platform)

| Key | Type | Description |
|---|---|---|
| `yrm1002_id` | ID | ID of the yrm1002 component |

Publishes the EPC (hex string) of the last scanned tag.

### Per-Tag Binary Sensor (`binary_sensor` platform)

| Key | Type | Description |
|---|---|---|
| `yrm1002_id` | ID | ID of the yrm1002 component |
| `uid` | string | Tag EPC to match (hex string, e.g. `E28068940000401A48B8507E`) |

Publishes `ON` when the specified tag is detected, `OFF` when it is not found during a scan cycle.

## How It Works

1. Every `update_interval`, the component sends a **multi-read inventory** command (`0x27`) with `repeat_count` rounds
2. During `scan_duration`, the YRM1002 continuously reports discovered tags
3. After `scan_duration` expires, a **stop** command (`0x28`) is sent
4. Received tag data is parsed: EPC, RSSI, PC (Protocol Control), CRC
5. New tags (not seen in previous cycle) trigger `on_tag`
6. Tags from previous cycle not seen in current cycle trigger `on_tag_removed`
7. `tag_present` sensor updates: ON if any tags found, OFF if none
8. Per-UID binary sensors update based on their specific tag

## YRM1002 Protocol

Communication uses a framed protocol over UART (115200 baud, 8N1):

```
Frame: BB [type] [cmd] [length_hi] [length_lo] [data...] [checksum] 7E

Types:  0x00 = Command, 0x01 = Response, 0x02 = Notification
Commands used:
  0x07 - Set Region (data: region byte, e.g. 0x03 = EU)
  0x27 - Multi-Read Inventory (data: sub_cmd=0x22, 0x00, repeat_count)
  0x28 - Stop Multi-Read

Inventory response payload:
  [RSSI(1)] [PC(2)] [EPC(variable)] [CRC(2)]

Checksum: sum of all bytes from type to last data byte, modulo 256
```

## Tuning Guide

| Scenario | scan_duration | update_interval | repeat_count |
|---|---|---|---|
| Fast detection, single tag | 300ms | 500ms | 5 |
| Balanced | 500ms | 1s | 10 |
| Multiple tags, max reliability | 1000ms | 2s | 20 |

- **`repeat_count`**: Higher = more read attempts per cycle = more reliable detection, but more UART traffic
- **`scan_duration`**: Must be long enough for `repeat_count` rounds to complete (~50ms per round)
- **`update_interval`**: Total cycle time = `scan_duration` + processing overhead

## Troubleshooting

### No tags detected
- Check UART wiring (TX↔RX crossover through level converter)
- Verify baud rate is 115200
- Ensure YRM1002 has 5V power supply
- Check `rx_buffer_size: 1024` is set on UART bus (default 256 may overflow)

### Tags detected but events not firing in Home Assistant
- Ensure `on_tag` trigger is configured with `homeassistant.tag_scanned`
- Check ESPHome API connection is established
- Tags must be registered in Home Assistant (Settings → Tags) for `tag_scanned` to work

### 0xFF errors in logs
- `Command execution failed (0x15)` is normal during multi-read — means reader has no more tags to report
- These are logged at VERBOSE level and do not affect functionality

## License

MIT
