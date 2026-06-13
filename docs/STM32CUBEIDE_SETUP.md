# ⚙️ STM32CubeIDE Setup Guide (Guaranteed 0 Errors)

Follow these exact steps to configure your `.ioc` file. Deviating from these settings will cause compilation errors or runtime failures.

## 1. System Core
- **SYS** → Debug: **Serial Wire**
- **RCC** → High Speed Clock (HSE): **Crystal/Ceramic Resonator**
- **RCC** → Low Speed Clock (LSI): **Enabled** *(Critical for Watchdog!)*

## 2. Clock Configuration
- HSE → PLL → SYSCLK: **168 MHz**
- HCLK (AHB): **168 MHz**
- APB1 Prescaler: **Divide by 4** *(Results in 42 MHz)*
- APB2 Prescaler: **Divide by 2** *(Results in 84 MHz)*

## 3. TIM3 (For CKP/CMP)
- **Mode:** Input Capture direct mode (for Channel 1 and Channel 2)
- **Parameter Settings:**
  - Prescaler: **83** *(84MHz / (83+1) = 1 MHz → 1 µs/tick resolution)*
  - Counter Period: **65535** *(0xFFFF)*
  - Input Capture → Filter: **15** *(🔥 CRITICAL: Hardware filter ignores coil ringing < 15 µs)*
- **NVIC Settings:**
  - TIM3 global interrupt: **Enabled**
  - Preemption Priority: **1**

## 4. CAN1
- **Mode:** **Normal**
- **Parameter Settings:**
  - Prescaler: **6**
  - Time Seg 1 (BS1): **10**
  - Time Seg 2 (BS2): **3**
  - *(This yields exactly 500 kbps with a 78.6% Sample Point, optimal for automotive)*
- **NVIC Settings:**
  - CAN1 RX0 interrupt: **Enabled**
  - Preemption Priority: **2**

## 5. USART1 & USART2
- **Mode:** **Asynchronous**
- **Parameter Settings:**
  - Baud Rate: **115200 Bits/s**
  - Word Length: **8 Bits**
  - Parity: **None**
  - Stop Bits: **1**

## 6. IWDG (Independent Watchdog)
- **Parameter Settings:**
  - Prescaler: **32** *(LSI ~32kHz / 32 = 1 kHz)*
  - Counter: **1000** *(Yields exactly a 1-second timeout)*

## 7. GPIO Configuration
- Find **PA6** and **PA7**. Ensure they are labeled `TIM3_CH1` and `TIM3_CH2` (green tag).
- Find **PA8**. Set: GPIO mode: **Output Push Pull**, Speed: **Very High**. *(User Label: `LAMBDA_CTRL`)*
- For ALL pins **PB0-PB11** and **PC6-PC11**, set:
  - GPIO mode: **Output Push Pull**
  - Maximum output speed: **Very High**

## 8. Final Generation
1. Press `Ctrl+S` to save. CubeIDE will generate the base code.
2. Open `Core/Src/main.c` and `Core/Inc/main.h`.
3. **COMPLETELY REPLACE** their contents with the provided clean code files.
4. Click **Project → Build All** (or `Ctrl+B`).
5. **Expected Result:** `0 Errors, 0 Warnings`.