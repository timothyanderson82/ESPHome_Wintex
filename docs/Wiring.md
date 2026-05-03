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

```yaml
uart:
  - id: uart_s          # StreamServer — Wintex PC software bridge
    tx_pin: GPIO26
    rx_pin: GPIO27
    baud_rate: 19200
    data_bits: 8
    parity: none
    stop_bits: 2

  - id: uart_w          # Wintex component — zone status polling
    tx_pin: GPIO17
    rx_pin: GPIO16
    baud_rate: 19200
    data_bits: 8
    parity: none
    stop_bits: 2
```

> Both `uart_s` and `uart_w` connect to the **same** panel COM1 port (pins 2 & 3). Both share the same level-shifted TX/RX lines.

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
