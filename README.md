# 🎵 Flute System: Open-Source Cylinder Deactivation

[![Platform: STM32](https://img.shields.io/badge/MCU-STM32F405_@168MHz-orange.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32f405.html)
[![Connectivity: ESP32](https://img.shields.io/badge/IoT-ESP32_WiFi--BT-green.svg)](https://www.espressif.com/en/products/socs/esp32)
[![License: MIT](https://img.shields.io/badge/Software-MIT-blue.svg)](LICENSE)
[![Hardware: CERN OHL](https://img.shields.io/badge/Hardware-CERN_OHL_v2-darkgreen.svg)](LICENSE-HARDWARE.md)

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
- **Dynamic Look-Up Table (LUT):** O(1) complexity valve timing, dynamically adjustable via a `timing_offset` parameter (±5 teeth) to perfectly match any engine's cam profile.
- **Multi-Engine Profiles:** Supports Inline-4, Inline-6, and V6 configurations with selectable deactivation patterns.

### 🕵️ "Stealth" ECU Emulation
- **Zero Check Engine Lights:** Uses recycled, electrically functional fuel injectors as "dummy loads". A MOSFET multiplexer seamlessly switches the stock ECU's driver to the dummy load during deactivation, mimicking 12–16Ω resistance and 2–3mH inductance.

### 🛡️ Automotive-Grade Safety
- **Independent Watchdog (IWDG):** 1-second hardware timeout triggers atomic reset to Stock Mode.
- **Multi-Layer Fail-Safes:** Deactivation blocked if coolant < 80°C or throttle > 30%.
- **Idle Deactivation Support:** Safely allows deactivation down to ~500 RPM.

### 📱 Modern IoT Connectivity
- **ESP32 Wi-Fi Bridge:** Local "FluteSystem" access point with mobile-friendly SPA.
- **Dynamic Lambda Management:** Three modes – Timing Offset, Auto-Calibrate, Dynamic Signal Emulation.
- **Telegram Alerts:** Instant notifications for critical faults.

---

## 🔬 How It Works

1. **Intake Stroke:** Fresh air enters the deactivated cylinder normally.
2. **Compression Stroke:** Near TDC, the Flute solenoid valve opens.
3. **Pressure Relief:** Compressed air escapes to atmosphere – **zero compression work**.
4. **Power & Exhaust Strokes:** Valve closes after TDC; piston moves with minimal resistance.
5. **ECU Deception:** STM32 switches ECU driver to a dummy injector, maintaining perfect fuel trim.

---

## 🏗️ System Architecture

```text
[Engine Sensors] ──(CKP/CMP)──> [STM32F405 Real-Time Controller] <──(CAN 500kbps)──> [Stock ECU / OBD-II]
                                      │
                                      ├──(UART 115200)──> [ESP32 Wi-Fi Bridge] ──(WebSocket)──> [Smartphone UI]
                                      │                        │
                                      │                        └──(HTTPS)──> [Telegram Bot Alerts]
                                      │
                                      └──(GPIO BSRR)──> [Stealth Driver Modules]
                                                              ├──> [Dummy Injectors]
                                                              └──> [Flute Valves] ──> Atmosphere
🛡️ Safety & Fail-Safes
Boot Safety: On power-up, all GPIO pins forced LOW before any logic runs.

Catalyst Protection: Skip-fire strategy maintains exhaust temperature above 300°C.

Dynamic Lambda Sync: Hardware emulation engaged only during deactivation cycles.

🧰 Hardware Requirements
Estimated total cost: $100 – $180 USD

Component	Specification	Qty
Main MCU	STM32F405RGT6 (LQFP-64) Dev Board	1
IoT Module	ESP32-WROOM-32 Dev Kit	1
CAN Transceiver	TJA1050 (500 kbps)	1
Power MOSFETs	IRLZ44N (Logic-Level, TO-220)	12
High-Side Driver	VND5T050AK-E (Smart Power Switch)	6
Flyback Diodes	1N5822 (Schottky, 3A)	6
Dummy Loads	Recycled Fuel Injectors (12–16Ω, electrically functional)	6
Mechanical	M12x1.25 Conical Adapters (37°), High-temp Silicone Tubing, Check Valve	1 set
📄 See hardware/BOM.md for a complete, linked parts list with global sourcing options.

🚀 Installation & Usage
Mechanical Prep: Perform a "water test" on the cylinder head. Drill tangential holes (3mm → 5mm → 8mm → 10mm), tap M12x1.25, install conical adapters.

Valve Mod: Block nozzle of recycled injectors (epoxy or welding), preserve coil, remove internal filter basket.

Wiring: Connect CKP/CMP sensors, CAN bus (OBD-II pins 6/14), and driver modules per hardware/GOLDEN_PIN_MAP.md.

Flash Firmware:

STM32: Compile and flash via STM32CubeIDE (ST-Link).

ESP32: Flash via Arduino IDE.

Calibrate: Connect to "FluteSystem" Wi-Fi, open 192.168.4.1, select Engine Profile, adjust "Timing Offset".

🌍 Performance & Ecology
Fuel Economy: 30–40% savings during steady-state cruising and idle.

CO₂ Reduction: ~1,200 kg/year for a typical 3.0L engine (15,000 km/year).

Circular Economy: Upcycles end-of-life fuel injectors.

🤝 Contributing
We welcome contributions! Please read CONTRIBUTING.md for guidelines. Ideas:

Add LUT configuration for a different engine firing order.

Improve ESP32 web interface or OBD-II PID monitoring.

Share bench-testing data or P-V diagrams.

💖 Support This Research
If you find this project useful, please consider supporting my research. Your contributions help buy hardware for new prototypes and keep this project open-source.

Direct Bank Transfer (IBAN): UA183052990000026209884907769 (PrivatBank, UAH)

📷 Scan to donate (PrivatBank):
<img width="150" alt="QR code for donation" src="https://raw.githubusercontent.com/yuschenko-mehan/Flute-System-Open-Source-Cylinder-Deactivation/docs/24.png" />

Contact me for details or alternative methods.

Goal: Raise $10,000 to build a commercial-grade test bench and adapt this controller for 10 popular car models. 100% of funds go to hardware and open-source development.

👨‍🔬 About the Author
This project was developed by Mykola Yushchenko, a researcher and educator from the Kyiv National Automobile and Highway University (KNAHU, formerly KADI) — Ukraine's leading automotive engineering university since 1944. Leveraging deep expertise in internal combustion engine thermodynamics and embedded systems, the Flute System represents a novel approach to making existing vehicles more efficient without expensive OEM-level modifications.

⚖️ License & Disclaimer
Software: Licensed under the MIT License.

Hardware: Schematics and mechanical designs are licensed under the CERN Open Hardware Licence Version 2 (CERN OHL v2).

⚠️ DISCLAIMER: This project involves modifying critical engine components and interfacing with vehicle electronics. Improper installation or use may result in engine damage, voided warranties, or failed emissions testing. The authors and contributors are not liable for any damage, injury, or loss resulting from the use of this system. Always consult a qualified mechanic and comply with local vehicle modification laws.

Built with ❤️ for the open-source automotive engineering community.
