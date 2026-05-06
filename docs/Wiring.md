# Wiring Guide: ESP32 DevKit + Fermion 4-Channel Level Shifter → Texecom Premier 412

## Components

| Component | Notes |
|-----------|-------|
| ESP32 DevKit (ESP32-WROOM-32) | `esp32dev` board target |
| 4-Channel Fermion Level Converter (YF04E/74LVCH4T245) | Bidirectional, 1.65V–5.5V |
| Texecom Premier 412 | COM1 / UDL port (4-pin header on PCB) |

---

## Voltage Reference

| Pin | Source |
|-----|--------|
| Level shifter **VA** (3.3V side) | ESP32 DevKit **3V3** pin |
| Level shifter **VB** (5V side)   | ESP32 DevKit **5V/VIN** pin (USB-powered) |
| Level shifter **OE**             | ESP32 DevKit **3V3** pin (tie high to enable) |
| Level shifter **GND**            | ESP32 DevKit **GND** (shared with panel GND) |

> ⚠️ The ESP32 DevKit 5V pin is only live when powered via USB. If you later power from the panel's 12V via a buck converter, use the buck converter's 5V output for VB instead.

---

## Texecom COM1 / UDL Port Pinout

Located on the Premier 412 PCB — 4-pin header near the PSU section.

| Pin | Signal       |
|-----|--------------|
| 1   | GND          |
| 2   | TX (panel → ESP32) |
| 3   | RX (ESP32 → panel) |
| 4   | +12V (do not connect) |

> ⚠️ Verify pinout against your specific board revision in the Texecom Premier 412 installation manual before connecting.

---

## Level Shifter Wiring

```
Level Shifter
─────────────────────────────────────────────────────────
A-side (3.3V / ESP32)          B-side (5V / Panel)
─────────────────────────────────────────────────────────
VA  ←── ESP32 3V3              VB  ←── ESP32 5V
GND ←── ESP32 GND              GND ←── Panel GND (Pin 1)
OE  ←── ESP32 3V3 (enable)

A1  ←── ESP32 GPIO26 (TX1)     B1  ───→ Panel RX (Pin 3)
A2  ───→ ESP32 GPIO27 (RX1)    B2  ←── Panel TX (Pin 2)
A3, A4  (unused)               B3, B4  (unused)
```

---

## ESP32 UART Pin Assignments

Only **one UART** is physically wired to the panel at a time — `uart_s` on GPIO26/27 via the level shifter. `uart_w` (GPIO17/16) is declared in the YAML but left unconnected; it is reserved for future use.

```yaml
uart:
  - id: uart_s          # Panel UART — used by wintex component OR stream_server (not both)
    tx_pin: GPIO26
    rx_pin: GPIO27
    baud_rate: 19200
    data_bits: 8
    parity: none
    stop_bits: 2

  - id: uart_w          # Unused — reserved, no physical connection required
    tx_pin: GPIO17
    rx_pin: GPIO16
    baud_rate: 19200
    data_bits: 8
    parity: none
    stop_bits: 2
```

---

## Operating Modes

The physical wiring is **identical** for both modes. Only the firmware changes.

### Mode 1 — Wintex component (normal operation)

The `wintex` component owns `uart_s` and polls the panel for zone status, reporting to Home Assistant.

```yaml
wintex:
  uart_id: uart_s
  udl: !secret udl
  zones:
    - zone: 1
      name: "Front Door"
      device_class: door
    # ... more zones
```

### Mode 2 — Stream Server (protocol debugging)

The `stream_server` component owns `uart_s` and bridges raw UART bytes to a TCP socket on port 10000. Connect Wintex PC software via a virtual COM port (e.g. HW VSP3 → `10.0.8.184:10000`) to interact with the panel directly and observe the protocol in ESPHome logs.

```yaml
stream_server:
  - uart_id: uart_s
    port: 10000
```

> ⚠️ `stream_server` and `wintex` **cannot share a UART** — only one may be active at a time. Comment out the other before flashing.

---

## Voltage Check (recommended before first connection)

With the panel powered and the Pi/ESP32 disconnected, use a multimeter to measure the panel COM1 TX pin (Pin 2) to GND (Pin 1):

- **≤ 3.3V** → Level shifter still recommended but risk of damage without it is very low
- **~5V** → Level shifter is required
- **> 5V** → Do not connect directly; investigate further

---

## Summary Diagram

```
Texecom Premier 412
COM1 Header
┌─────────┐
│ 1  GND  │────────────────────────────── GND (ESP32 + Level Shifter)
│ 2  TX   │──→ B2 [Level Shifter] A2 ──→ GPIO27 (ESP32 RX1)
│ 3  RX   │←── B1 [Level Shifter] A1 ←── GPIO26 (ESP32 TX1)
│ 4  +12V │  (not connected)
└─────────┘

Level Shifter power:
  VA ←── ESP32 3V3
  VB ←── ESP32 5V (USB)
  OE ←── ESP32 3V3
```
