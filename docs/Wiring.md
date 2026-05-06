# Wiring Guide: ESP32 DevKit + Fermion 4-Channel Level Shifter → Texecom Premier 412

## Components

| Component | Notes |
|-----------|-------|
| ESP32 DevKit (ESP32-WROOM-32) | `esp32dev` board target |
| 4-Channel Fermion Level Converter (YF04E/74LVCH4T245) | Bidirectional, 1.65V–5.5V |
| Texecom Premier 412 | COM1 (wintex polling) + COM2 (stream server) |

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

## Texecom COM Port Pinout

Both COM1 and COM2 use the same 4-pin header pinout. COM1 is near the PSU section; COM2 location varies by board revision.

| Pin | Signal       |
|-----|--------------|
| 1   | GND          |
| 2   | TX (panel → ESP32) |
| 3   | RX (ESP32 → panel) |
| 4   | +12V (do not connect) |

> ⚠️ Verify pinout against your specific board revision in the Texecom Premier 412 installation manual before connecting.

---

## Level Shifter Wiring

Each level shifter channel handles one direction of one UART. Channels 1–2 go to panel COM1, channels 3–4 go to panel COM2.

```
Level Shifter
─────────────────────────────────────────────────────────
A-side (3.3V / ESP32)          B-side (5V / Panel)
─────────────────────────────────────────────────────────
VA  ←── ESP32 3V3              VB  ←── ESP32 5V
GND ←── ESP32 GND              GND ←── Panel GND (COM1 Pin 1)
OE  ←── ESP32 3V3 (enable)

A1  ←── ESP32 GPIO26 (uart_s TX)   B1  ───→ COM1 RX (Pin 3)
A2  ───→ ESP32 GPIO27 (uart_s RX)  B2  ←── COM1 TX (Pin 2)
A3  ←── ESP32 GPIO17 (uart_w TX)   B3  ───→ COM2 RX (Pin 3)
A4  ───→ ESP32 GPIO16 (uart_w RX)  B4  ←── COM2 TX (Pin 2)
```

> Connect COM1 GND and COM2 GND to the same shared GND on the level shifter.

---

## ESP32 UART Pin Assignments

| UART | GPIO | Connected to | Purpose |
|------|------|--------------|---------|
| `uart_s` | GPIO26 (TX) / GPIO27 (RX) | Panel **COM1** | `wintex` component — zone status polling for HA |
| `uart_w` | GPIO17 (TX) / GPIO16 (RX) | Panel **COM2** | `stream_server` — TCP bridge for Wintex PC software |

Both run simultaneously — no bus conflicts since they are on separate COM ports.

```yaml
uart:
  - id: uart_s          # wintex component — zone polling on COM1
    tx_pin: GPIO26
    rx_pin: GPIO27
    baud_rate: 19200
    data_bits: 8
    parity: none
    stop_bits: 2

  - id: uart_w          # stream_server — Wintex PC software on COM2
    tx_pin: GPIO17
    rx_pin: GPIO16
    baud_rate: 19200
    data_bits: 8
    parity: none
    stop_bits: 2
```

---

## Voltage Check (recommended before first connection)

With the panel powered and the ESP32 disconnected, use a multimeter to measure the panel COM TX pin (Pin 2) to GND (Pin 1) on each port:

- **≤ 3.3V** → Level shifter still recommended but risk of damage without it is very low
- **~5V** → Level shifter is required
- **> 5V** → Do not connect directly; investigate further

---

## Summary Diagram

```
Texecom Premier 412                      Level Shifter            ESP32
                                         ─────────────
COM1 Header                              A-side  B-side
┌─────────┐                              ──────  ──────
│ 1  GND  │──────────────────────────────────────────── GND
│ 2  TX   │──→ B2 ──────────────────── A2 ──→ GPIO27 (uart_s RX)
│ 3  RX   │←── B1 ──────────────────── A1 ←── GPIO26 (uart_s TX)
│ 4  +12V │  (not connected)
└─────────┘

COM2 Header
┌─────────┐
│ 1  GND  │──────────────────────────────────────────── GND (shared)
│ 2  TX   │──→ B4 ──────────────────── A4 ──→ GPIO16 (uart_w RX)
│ 3  RX   │←── B3 ──────────────────── A3 ←── GPIO17 (uart_w TX)
│ 4  +12V │  (not connected)
└─────────┘

Level Shifter power:
  VA ←── ESP32 3V3
  VB ←── ESP32 5V (USB) or buck converter 5V
  OE ←── ESP32 3V3
```

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

A1  ←── ESP32 GPIO26 (uart_s TX)   B1  ───→ Panel RX (Pin 3)
A2  ───→ ESP32 GPIO27 (uart_s RX)  B2  ←── Panel TX (Pin 2)
A3  ←── ESP32 GPIO17 (uart_w TX)   B3  ───→ Panel RX (Pin 3) ← same wire as B1
A4  ───→ ESP32 GPIO16 (uart_w RX)  B4  ←── Panel TX (Pin 2) ← same wire as B2
```

---

## ESP32 UART Pin Assignments

The original design connects **both UARTs to the same panel COM1 port**, each serving a different purpose:

- `uart_s` (GPIO26/27) → **stream_server** — TCP bridge for Wintex PC software (passive, only active when a PC connects)
- `uart_w` (GPIO17/16) → **wintex component** — zone status polling for Home Assistant

```yaml
uart:
  - id: uart_s          # stream_server — Wintex PC software TCP bridge
    tx_pin: GPIO26
    rx_pin: GPIO27
    baud_rate: 19200
    data_bits: 8
    parity: none
    stop_bits: 2

  - id: uart_w          # wintex component — zone status polling
    tx_pin: GPIO17
    rx_pin: GPIO16
    baud_rate: 19200
    data_bits: 8
    parity: none
    stop_bits: 2
```

> ⚠️ Both UARTs connect to panel COM1 (same pins 2 & 3). This means both level shifter channel pairs (A1/B1 and A2/B2 for one, A3/B3 and A4/B4 for the other) wire to the same panel TX and RX lines.  
> **Do not use both simultaneously** — if Wintex PC software is actively connected via the stream_server, disconnect the wintex component to avoid bus conflicts.

---

## Operating Modes

### Mode 1 — Wintex component only (normal HA operation)

Comment out `stream_server`, use `wintex` on `uart_w`:

```yaml
wintex:
  uart_id: uart_w
  udl: !secret udl
```

### Mode 2 — Stream server only (protocol debugging)

Comment out `wintex`, use `stream_server` on `uart_s`. Connect Wintex PC software via a virtual COM port (e.g. HW VSP3 → `<device-ip>:10000`):

```yaml
stream_server:
  - uart_id: uart_s
    port: 10000
```

### Mode 3 — Both (original design intent)

Both components active on separate UARTs, both wired to panel COM1. Works in practice because stream_server is passive when no PC is connected:

```yaml
stream_server:
  - uart_id: uart_s
    port: 10000

wintex:
  uart_id: uart_w
  udl: !secret udl
```

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
│ 2  TX   │──→ B2 [Level Shifter] A2 ──→ GPIO27 (uart_s RX)
│         │──→ B4 [Level Shifter] A4 ──→ GPIO16 (uart_w RX)
│ 3  RX   │←── B1 [Level Shifter] A1 ←── GPIO26 (uart_s TX)
│         │←── B3 [Level Shifter] A3 ←── GPIO17 (uart_w TX)
│ 4  +12V │  (not connected)
└─────────┘

Level Shifter power:
  VA ←── ESP32 3V3
  VB ←── ESP32 5V (USB) or buck converter 5V
  OE ←── ESP32 3V3
```
