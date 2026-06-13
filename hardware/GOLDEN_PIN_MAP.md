# 🔌 Golden Pin Map (STM32F405RGT6, LQFP-64)

This is the **single source of truth** for hardware wiring. Any deviation from this map will result in system failure or component damage.

## ⚠️ CRITICAL HARDWARE RULES (MUST FOLLOW)
1. **Gate Resistors:** EVERY MOSFET gate (PB0-PB11, PC6-PC11) MUST have a **100Ω series resistor** between the STM32 pin and the gate.
2. **Pull-Down Resistors:** EVERY MOSFET gate MUST have a **10kΩ pull-down resistor** to GND to prevent floating states during boot.
3. **Flyback Diodes:** EVERY real injector coil MUST have a **1N5822 Schottky diode** in parallel (Cathode to +12V, Anode to LS pin) to protect MOSFETs from inductive kickback.
4. **Common Ground:** The GND of the STM32, ESP32, TJA1050, and the vehicle chassis MUST be tied together at a single star point.
5. **Decoupling:** A **100nF ceramic capacitor** MUST be placed as close as possible to EVERY VDD pin (9, 22, 38, 54) of the STM32.

---

## 1. Power & System
| Physical Pin | GPIO Name | Function | Connection & Mandatory Conditions |
| :--- | :--- | :--- | :--- |
| **7** | VBAT | Backup Power | Connect to 3.3V (or via diode to CR2032) |
| **8, 21, 37, 53** | VSS | Ground (GND) | **Common Ground** with Vehicle Chassis, ESP32, and Drivers |
| **9, 22, 38, 54** | VDD | 3.3V Power | From AMS1117-3.3. **Mandatory:** 100nF cap near each pin! |
| **4** | NRST | Reset | 10kΩ Pull-up to 3.3V + Push button to GND |
| **1** | VCAP_1 | Core Capacitor | **Mandatory:** 2.2µF capacitor to GND |
| **32** | VCAP_2 | Core Capacitor | **Mandatory:** 2.2µF capacitor to GND |

## 2. Sensors, Communication & Lambda Control
| Physical Pin | GPIO Name | Alt. Function | Project Function | Connection |
| :--- | :--- | :--- | :--- | :--- |
| **18** | **PA6** | TIM3_CH1 | **CKP** (Crankshaft) | From inductive sensor. 10kΩ Pull-Down to GND. |
| **19** | **PA7** | TIM3_CH2 | **CMP** (Camshaft) | From Hall sensor. 10kΩ Pull-Down to GND. |
| **45** | **PA11** | CAN1_RX | **CAN_RX** | To TJA1050 RX pin. |
| **46** | **PA12** | CAN1_TX | **CAN_TX** | To TJA1050 TX pin. |
| **31** | **PA9** | USART1_TX | **Debug TX** | To USB-UART adapter (CH340) for PC monitoring. |
| **32** | **PA10** | USART1_RX | **Debug RX** | To USB-UART adapter (CH340). |
| **34** | **PA2** | USART2_TX | **ESP32 RX** | To ESP32 **TX** (GPIO 17). |
| **35** | **PA3** | USART2_RX | **ESP32 TX** | To ESP32 **RX** (GPIO 16). |
| **37** | **PA8** | GPIO_Output | **Lambda Ctrl** | Controls external Lambda Emulation circuit (Op-Amp/DigPot). |

## 3. Cylinder Drivers (Atomic BSRR Control)
*Each cylinder requires 3 pins. Apply the 100Ω series and 10kΩ pull-down rules to ALL of them.*

| Cylinder | Physical Pin | GPIO | Signal | Connect To |
| :--- | :--- | :--- | :--- | :--- |
| **Cyl 1** | 55 | **PB0** | MUX | MOSFET Gate (IRLZ44N) |
| | 56 | **PB1** | HS | High-Side Driver Gate (VND5T050) |
| | 57 | **PB2** | LS | MOSFET Gate (IRLZ44N) |
| **Cyl 2** | 58 | **PB3** | MUX | MOSFET Gate (IRLZ44N) |
| | 59 | **PB4** | HS | High-Side Driver Gate (VND5T050) |
| | 60 | **PB5** | LS | MOSFET Gate (IRLZ44N) |
| **Cyl 3** | 61 | **PB6** | MUX | MOSFET Gate (IRLZ44N) |
| | 62 | **PB7** | HS | High-Side Driver Gate (VND5T050) |
| | 63 | **PB8** | LS | MOSFET Gate (IRLZ44N) |
| **Cyl 4** | 64 | **PB9** | MUX | MOSFET Gate (IRLZ44N) |
| | 1 | **PB10** | HS | High-Side Driver Gate (VND5T050) |
| | 2 | **PB11** | LS | MOSFET Gate (IRLZ44N) |
| **Cyl 5** | 15 | **PC6** | MUX | MOSFET Gate (IRLZ44N) |
| | 16 | **PC7** | HS | High-Side Driver Gate (VND5T050) |
| | 17 | **PC8** | LS | MOSFET Gate (IRLZ44N) |
| **Cyl 6** | 26 | **PC9** | MUX | MOSFET Gate (IRLZ44N) |
| | 27 | **PC10** | HS | High-Side Driver Gate (VND5T050) |
| | 28 | **PC11** | LS | MOSFET Gate (IRLZ44N) |