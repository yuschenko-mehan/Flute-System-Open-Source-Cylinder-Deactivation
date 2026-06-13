# 🎵 Flute System: Open-Source Cylinder Deactivation
[![Platform: STM32](https://img.shields.io/badge/MCU-STM32F405_@168MHz-orange.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32f405.html)
[![Connectivity: ESP32](https://img.shields.io/badge/IoT-ESP32_WiFi--BT-green.svg)](https://www.espressif.com/en/products/socs/esp32)

**An advanced, open-source cylinder deactivation system that eliminates pumping losses via compression-release valves, featuring dynamic lambda emulation, multi-engine profile support, and real-time IoT monitoring.**

---

## 📋 Table of Contents
- [Overview](#overview)
- [Key Features](#key-features)
- [How It Works](#how-it-works)
- [System Architecture](#system-architecture)
- [Safety & Fail-Safes](#safety--fail-safes)
- [Hardware Requirements](#hardware-requirements)
- [Installation & Usage](#installation--usage)
- [Performance & Ecology](#performance--ecology)
- [Contributing](#contributing)
- [License & Disclaimer](#license--disclaimer)

---

## 🎯 Overview
Traditional cylinder deactivation systems (like GM AFM or Honda VCM) reduce fuel consumption by 15–20%, but they still suffer from **pumping losses** because the piston continues to compress trapped air. They also require complex, expensive valvetrain modifications.

The **Flute System** takes a radically different approach: it vents compressed air directly to the atmosphere during the compression stroke of deactivated cylinders. This eliminates pumping work almost entirely, offering a theoretical fuel savings of **30–40%** under light-load conditions, all while maintaining 100% compatibility with the stock Engine Control Unit (ECU) through innovative "stealth" load emulation.

---

## ✨ Key Features

### 🧠 Intelligent Real-Time Control
- **STM32F405 @ 168 MHz:** Deterministic engine synchronization using hardware Input Capture (TIM3) with 1µs resolution and 15µs hardware noise filtering.
- **Dynamic Look-Up Table (LUT):** O(1) complexity valve timing, dynamically adjustable via a `timing_offset` parameter (±5 teeth) to perfectly match any engine's cam profile and exhaust stroke dynamics.
- **Multi-Engine Profiles:** Supports Inline-4, Inline-6, and V6 configurations with selectable pair or triplet deactivation patterns, dynamically generating the correct bit-masks without reflashing.

### 🕵️ "Stealth" ECU Emulation
- **Zero Check Engine Lights:** Uses recycled, electrically functional fuel injectors as "dummy loads". A MOSFET multiplexer seamlessly switches the stock ECU's driver to the dummy load during deactivation, perfectly mimicking the 12–16Ω resistance and 2–3mH inductance, preventing OBD-II P020x errors.

### 🛡️ Automotive-Grade Safety
- **Independent Watchdog (IWDG):** 1-second hardware timeout. Any software stall triggers an immediate, atomic reset to Stock Mode.
- **Multi-Layer Fail-Safes:** Deactivation is strictly blocked if:
  - Coolant temperature < 80°C (prevents oil film washout and fuel condensation).
  - Throttle Position > 30% (instant power restoration).
- **Idle Deactivation Support:** Safely allows deactivation down to ~500 RPM to smooth out idle vibrations and save fuel in traffic, provided the engine is fully warmed up.
- **Atomic GPIO Control:** Direct BSRR register manipulation ensures glitch-free, single-cycle (<100ns) state transitions.

### 📱 Modern IoT Connectivity
- **ESP32 Wi-Fi Bridge:** Creates a local "FluteSystem" access point with a mobile-friendly Single Page Application (SPA).
- **Dynamic Lambda Management:** Three user-selectable modes:
  1. *Timing Offset Only:* Minimizes exhaust stroke interference.
  2. *Auto-Calibrate:* Dynamically adjusts timing based on CAN bus Short Term Fuel Trim (STFT).
  3. *Dynamic Signal Emulation:* Synchronously engages an external Op-Amp circuit to scale the lambda signal *only* during deactivation cycles, preventing lean spikes when the system is inactive.
- **Telegram Alerts:** Instant, anti-spam-protected notifications for critical faults (e.g., sync loss, watchdog reset).

---

## 🔬 How It Works
1. **Intake Stroke:** Fresh air enters the deactivated cylinder normally.
2. **Compression Stroke:** As the piston rises, the Flute solenoid valve opens precisely near Top Dead Center (TDC).
3. **Pressure Relief:** Compressed air escapes through the valve to the atmosphere, reducing cylinder pressure to ~1 bar. **Zero compression work is performed.**
4. **Power & Exhaust Strokes:** The valve remains open slightly into the early exhaust stroke (configurable) to reduce backpressure and shield the lambda sensor, then closes firmly. The piston moves with minimal resistance.
5. **ECU Deception:** Simultaneously, the STM32 disconnects the real injector from the ECU and connects a dummy injector. The ECU "thinks" it is firing normally, maintaining perfect closed-loop fuel trim.

---

## 🏗️ System Architecture

```text
[Engine Sensors] ──(CKP/CMP)──> [STM32F405 Real-Time Controller] <──(CAN 500kbps)──> [Stock ECU / OBD-II]
                                      │
                                      ├──(UART 115200)──> [ESP32 Wi-Fi Bridge] ──(WebSocket)──> [Smartphone Web UI]
                                      │                        │
                                      │                        └──(HTTPS)──> [Telegram Bot Alerts]
                                      │
                                      └──(GPIO BSRR)──> [Stealth Driver Modules (MUX + HS/LS MOSFETs)]
                                                              │
                                                              ├──> [Dummy Injectors (ECU Load)]
                                                              └──> [Real Injectors (Flute Valves)] ──> [Atmosphere]
# 🎵 Flute System: Open-Source Cylinder Deactivation with Compression Release

## The Problem
Traditional cylinder deactivation systems (GM AFM, Honda VCM) save only 15-20% fuel because they still suffer from **pumping losses** — the piston continues compressing trapped air in deactivated cylinders. They also require expensive valvetrain modifications and are unavailable for retrofit on older vehicles.

## The Solution
The **Flute System** takes a radically different approach: it vents compressed air directly to the atmosphere during the compression stroke of deactivated cylinders through modified fuel injectors. This eliminates pumping work almost entirely, offering a theoretical **30-40% fuel savings** under light-load conditions.

## Key Innovations

### 1. Dynamic Lambda Emulation (The Killer Feature)
The stock ECU monitors oxygen sensors and will throw a Check Engine Light if it detects "extra air" from deactivated cylinders. Our solution:
- **Mode 0 (Timing Offset):** Precise valve timing to minimize exhaust interference.
- **Mode 1 (Auto-Calibrate):** Real-time adjustment based on CAN bus Short Term Fuel Trim.
- **Mode 2 (Dynamic Signal Emulation):** An external Op-Amp circuit scales the lambda signal **ONLY during deactivation cycles**, then bypasses during normal operation. This prevents dangerous lean spikes — a feature absent even in commercial systems.

### 2. Multi-Engine Profile Support
Instead of "guessing" the engine type, the system uses configurable profiles:
- **Inline-4 (I4):** Pair deactivation (1 & 3)
- **Inline-6 (I6):** Pair or triplet deactivation
- **V6:** Cross-bank or full-bank deactivation

The STM32 dynamically generates the correct bit-masks without reflashing.

### 3. Seasonal Skip-Fire Strategy
Continuous deactivation causes cylinder cooling and catalytic converter damage. Our adaptive strategy alternates active/deactivated cycles:
- **Winter:** 9/10 pattern (10% deactivation, max heat retention)
- **Moderate:** 2/3 pattern (33% deactivation)
- **Summer/Highway:** 1/4 pattern (75% deactivation, max economy)

### 4. Idle Deactivation Support
Unlike conservative systems that block deactivation below 1200 RPM, Flute safely allows it down to **500 RPM** (idle) when fully warmed up, smoothing vibrations in traffic and saving fuel.

## Technical Architecture

### Hardware
- **Main MCU:** STM32F405RGT6 @ 168 MHz (ARM Cortex-M4)
- **IoT Bridge:** ESP32-WROOM-32 (Wi-Fi + Telegram)
- **Communication:** CAN 500 kbps (OBD-II), UART 115200
- **Actuators:** 6x IRLZ44N MOSFETs + 6x VND5T050AK-E high-side drivers
- **Sensors:** CKP (inductive), CMP (Hall), OBD-II PIDs via CAN

### Software Highlights
- **Atomic GPIO (BSRR):** Single-cycle (~6ns) pin manipulation, zero glitches
- **Hardware Noise Filtering:** TIM3 ICFilter = 15 (ignores coil ringing < 15 µs)
- **Independent Watchdog (IWDG):** 1-second hardware timeout
- **O(1) Look-Up Table:** Deterministic valve timing
- **Non-blocking UART:** ISR-safe command parsing

## Safety & Fail-Safes
- **Cold Engine Protection:** Deactivation blocked if coolant < 80°C
- **Load Protection:** Instant revert to Stock Mode if TPS > 30%
- **Watchdog Reset:** Automatic safe mode if firmware hangs
- **Atomic State Transitions:** No "half-open" valve states

## Environmental Impact
- **CO₂ Reduction:** ~300-400 kg/year per vehicle (based on 15,000 km/year)
- **Circular Economy:** Upcycles electrically functional but mechanically dead fuel injectors
- **Democratization:** Makes advanced efficiency technology accessible for retrofit on older vehicles

## About the Author
This project was developed by **Mykola Yushchenko**, a researcher and educator from the **Kyiv National Automobile and Highway University (KNAHU, formerly KADI)** — Ukraine's leading automotive engineering university since 1944. Leveraging deep expertise in internal combustion engine thermodynamics and embedded systems, the Flute System represents a novel approach to making existing vehicles more efficient without requiring expensive OEM-level modifications.

## Goal
If awarded, 100% of prize money will be reinvested into:
1. Building a commercial-grade test bench with P-V diagram measurement
2. Adapting the controller for 5 popular car models (VAG, Ford, Toyota, Renault, VAZ)
3. Creating a video tutorial series for automotive students in Ukraine
4. Publishing peer-reviewed research in Scopus-indexed journals

---

**Built with ❤️ for the open-source automotive engineering community.**
🛡️ Safety & Fail-Safes
Safety is the cornerstone of this design. The system is engineered to default to a safe state under any anomaly:
Boot Safety: On power-up or Watchdog reset, all GPIO pins are atomically forced to LOW before any logic runs.
Catalyst Protection: The Skip-Fire strategy ensures periodic firing, maintaining exhaust gas velocity and temperature, preventing the catalytic converter from dropping below its 300°C light-off threshold.
Dynamic Lambda Sync: The hardware lambda emulation circuit is engaged only when cylinders are actively deactivated, ensuring the ECU receives authentic sensor data during normal operation to prevent dangerous lean conditions.
## 🧰 Hardware Requirements

**Estimated total cost: $100 – $180 USD**
----------------------------------------------------------------------------------------------------------
|Component         |Specification                                                            |Qty        |
| ---------------- | ------------------------------------------------------------------------| --------- |
| Main MCU         | STM32F405RGT6 (LQFP-64) Dev Board                                       | 1         |
| IoT Module       | ESP32-WROOM-32 Dev Kit                                                  | 1         |
| CAN Transceiver  | TJA1050 (500 kbps)                                                      | 1         |
| Power MOSFETs    | IRLZ44N (Logic-Level, TO-220)                                           |  12       |
| High-Side Driver | VND5T050AK-E (Smart Power Switch)                                       | 6         |
| Flyback Diodes   | 1N5822 (Schottky, 3A)                                                   | 6         |
| Dummy Loads      | Recycled                                                                |           |
| ---------------- | ----------------------------------------------------------------------- | --------- |
| Flyback Diodes   | 1N5822 (Schottky, 3A)                                                   | 6         |
| Dummy Loads      | Recycled Fuel Injectors (12-16Ω)                                        | 6         |
| Mechanical       |  M12x1.25 Conical Adapters (37°), High-temp Silicone Tubing, Check Valve| 1 set     |
----------------------------------------------------------------------------------------------------------
> 📄 See [`hardware/BOM.md`](hardware/BOM.md) for a complete, linked parts list with global sourcing options.

🚀 Installation & Usage
Mechanical Prep: Perform a "water test" on the cylinder head to identify safe drilling zones. Drill tangential holes (3mm → 5mm → 8mm → 10mm), tap M12x1.25, and install conical adapters.
Valve Mod: Block the nozzle of recycled injectors (epoxy or welding) while preserving the electromagnetic coil. Remove the internal filter basket.
Wiring: Connect CKP/CMP sensors, CAN bus (OBD-II pins 6/14), and wire the driver modules to the injectors according to the hardware/GOLDEN_PIN_MAP.md.
Flash Firmware:
STM32: Compile and flash via STM32CubeIDE (ST-Link).
ESP32: Flash via Arduino IDE.
Calibrate: Connect to the "FluteSystem" Wi-Fi, open 192.168.4.1, select your Engine Profile, and use the "Timing Offset" slider to find the optimal valve closure point for your specific engine.
 🌍 Performance & Ecology
Fuel Economy: Theoretical reduction of pumping losses by up to 100% in deactivated cylinders, translating to 30-40% fuel savings during steady-state cruising and idle.
CO₂ Reduction: Directly proportional to fuel saved. A typical 3.0L engine can reduce emissions by ~1,200 kg of CO₂ per year (based on 15,000 km/year).
Circular Economy: Upcycles end-of-life fuel injectors into high-performance pneumatic valves, reducing electronic waste.
🤝 Contributing
We welcome contributions! Whether you want to:
Add a new Look-Up Table (LUT) configuration for a different engine firing order.
Improve the ESP32 web interface or add new OBD-II PID monitoring.
Share your bench-testing data or P-V diagrams.
Please read our CONTRIBUTING.md for guidelines on code style and pull requests.
⚖️ License & Disclaimer
Software: Licensed under the MIT License.
Hardware: Schematics and mechanical designs are licensed under the CERN Open Hardware Licence Version 2 (CERN OHL v2).
⚠️ DISCLAIMER: This project involves modifying critical engine components and interfacing with vehicle electronics. Improper installation or use may result in engine damage, voided warranties, or failed emissions testing. The authors and contributors are not liable for any damage, injury, or loss resulting from the use of this system. Always consult a qualified mechanic and comply with local vehicle modification laws.
Made with ❤️ for the open-source automotive engineering community.

# 🚗 Adaptive Robotized Manual Transmission Controller for STM32

**An open-source, aerospace-grade adaptive controller for robotized manual transmissions. Developed by a researcher from the National Aviation University (former KAI), Ukraine.**

---

## 💖 Support This Research
If you find this project useful for your studies, car tuning, or engineering work, please consider supporting my research. Your contributions help me buy hardware for new prototypes and keep this project open-source.

- **Direct Bank Transfer (IBAN):** UA183052990000026209884907769 (PrivatBank) - *Contact me for details <img width="792" height="792" alt="image" src="https://github.com/user-attachments/assets/1d2b0829-075b-4440-a3ae-8282c67788aa" />*

**Goal:** Raise $10,000 to build a commercial-grade test bench and adapt this controller for 10 different car models. 100% of funds go to hardware and open-source development.

#### 
📄 Файл 2: `LICENSE`
```text
MIT License
Copyright (c) 2024 Mykola M. Yushchenko

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## 👨‍🔬 About the Author
As a researcher. In **National Transport University (NTU, formerly KADI)** in Kyiv — Ukraine's leading automotive engineering university since 1944 — I've spent years studying internal combustion engine efficiency. One problem kept coming up in my lectures and research: **pumping losses**.
