# 📦 Bill of Materials (BOM)

Estimated total cost: **$100 - $150 USD** (depending on local vs. global sourcing).

| # | Component | Specification | Qty | Est. Price (UAH) | Est. Price (USD) | Sourcing Tips |
|:--|:---|:---|:--:|:---|:---|:---|
| 1 | **STM32F405RGT6** | Dev Board (LQFP-64, 168MHz) | 1 | 450 - 650 | $12 - $18 | Prom.ua, Electronika.in.ua |
| 2 | **ESP32-WROOM-32** | Dev Kit V1 (30 pin) | 1 | 250 - 350 | $5 - $8 | Rozetka, Prom.ua |
| 3 | **TJA1050** | CAN Transceiver (SOIC-8 or module) | 1 | 60 - 100 | $1.5 - $3 | Electronika.in.ua |
| 4 | **IRLZ44N** | N-Channel MOSFET, **Logic Level**, TO-220 | 12 | 25 - 40 /pc | $0.8 - $1.5 /pc | Electronika.in.ua |
| 5 | **VND5T050AK-E** | High-Side Smart Driver, MultiSO-8 | 6 | 80 - 120 /pc | $2 - $4 /pc | Prom.ua, ChipDip |
| 6 | **1N5822** | Schottky Diode, 3A, 40V, DO-201AD | 6 | 15 - 25 /pc | $0.3 - $0.6 /pc | Electronika.in.ua |
| 7 | **Resistors 100Ω** | 0.25W, 0805 SMD or Through-hole | 18 | 1 - 2 /pc | $0.01 /pc (kit) | Buy resistor kits |
| 8 | **Resistors 10kΩ** | 0.25W, 0805 SMD or Through-hole | 25 | 1 - 2 /pc | $0.01 /pc (kit) | Buy resistor kits |
| 9 | **Capacitors 100nF** | Ceramic, 50V, 0805 SMD | 25 | 2 - 3 /pc | $0.02 /pc (kit) | Buy capacitor kits |
| 10 | **Capacitor 2.2µF** | Ceramic, 10V+ (for VCAP1/2) | 2 | 5 - 10 /pc | $0.1 /pc | Electronika.in.ua |
| 11 | **AMS1117-3.3** | Voltage Regulator, SOT-223 | 1 | 15 - 25 | $0.3 - $0.6 | Electronika.in.ua |
| 12 | **Used Fuel Injectors** | Bosch/Siemens, **MUST BE 12-16Ω** | 6 | 100 - 200 /pc | $3 - $6 /pc | Local scrapyards, OLX |
| 13 | **Conical Adapter** | M12x1.25, 37° cone, steel/stainless | 6 | 60 - 100 /pc | $2 - $4 /pc | Custom lathe order |
| 14 | **Silicone Tubing** | ID 8mm, OD 14mm, 200°C rated | 6 m | 40 - 60 /m | $1 - $2 /m | Prom.ua, Auto stores |
| 15 | **Check Valve** | 10mm, low cracking pressure (<0.1 bar) | 1 | 80 - 150 | $3 - $6 | Prom.ua, Pneumatic stores |
| 16 | **Torque Wrench** | Range 10-50 N·m | 1 | 800 - 1500 | $20 - $40 | Epicentrk, Auto stores |

### 💡 Pro Tips for Sourcing:
- **Injectors:** Do NOT buy new injectors. Buy used ones from a scrapyard, but **you must verify their coil resistance with a multimeter**. It must read between 12Ω and 16Ω. The mechanical nozzle will be blocked, but the coil must be electrically perfect.
- **Resistors/Capacitors:** Always buy assortment kits on AliExpress or local electronics stores. It is vastly cheaper than buying individual values.