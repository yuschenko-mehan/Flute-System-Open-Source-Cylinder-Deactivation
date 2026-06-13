/*
 * ============================================================================
 * FLUTE SYSTEM – MASTER CONTROLLER STM32F405RGT6 @ 168 MHz
 * Consolidated firmware with updated physics-based logic:
 * 1. Allows deactivation at idle (~500+ RPM) for vibration/fuel reduction.
 * 2. LUT supports extended valve opening (8 teeth) into the exhaust stroke.
 * 3. Configurable Engine Profiles (I4, I6, V6) with pair/triplet deactivation.
 * 4. DYNAMIC Lambda Emulation: Activates ONLY during deactivation cycles.
 * ============================================================================
 */

#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

/* ============================================================================
 * 1. GLOBAL VARIABLES & SYSTEM HANDLES
 * ============================================================================ */
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
IWDG_HandleTypeDef hiwdg;
TIM_HandleTypeDef  htim3;
CAN_HandleTypeDef  hcan;

/* ---- Engine Synchronisation State ---- */
typedef enum { ENGINE_STOPPED = 0, ENGINE_CRANKING = 1, ENGINE_RUNNING = 2 } EngineState_t;

typedef struct {
    volatile uint32_t last_ckp_time;
    volatile uint32_t tooth_period_us;
    volatile uint32_t raw_rpm;
    volatile uint32_t filtered_rpm;
    volatile uint16_t current_tooth;
    volatile bool     is_phase_synced;
    volatile EngineState_t engine_state;
} EngineSync_t;
static EngineSync_t sync_state = {0};

/* ---- Engine Conditions (from CAN OBD-II) ---- */
typedef struct {
    volatile float    clt_temp_c;       // Coolant temperature (°C)
    volatile uint8_t  tps_percent;      // Throttle position (%)
    volatile uint16_t map_kpa;          // Manifold Absolute Pressure (kPa)
    volatile float    stft_percent;     // Short Term Fuel Trim (%) for lambda calibration
} EngineCond_t;
static EngineCond_t cond_state = {0};

/* ---- Engine Profile Configuration ---- */
typedef enum {
    ENGINE_I4 = 0,  // Inline-4
    ENGINE_I6 = 1,  // Inline-6 (e.g., Opel C30LE)
    ENGINE_V6 = 2   // V6
} EngineType_t;

static EngineType_t current_engine = ENGINE_I6; // Default
static uint8_t active_deactivation_mask = 0x07; // Default: Cylinders 1, 2, 3

/**
 * @brief Dynamically configures the deactivation mask based on engine type.
 */
void Set_Engine_Profile(EngineType_t type, uint8_t deactivate_count) {
    current_engine = type;
    active_deactivation_mask = 0x00; // Reset mask

    if (type == ENGINE_I6) {
        if (deactivate_count == 2) active_deactivation_mask = 0x05; // Cyl 1 & 3 (Bits 0, 2)
        else if (deactivate_count == 3) active_deactivation_mask = 0x07; // Cyl 1, 2, 3 (Bits 0, 1, 2)
    } 
    else if (type == ENGINE_I4) {
        if (deactivate_count == 2) active_deactivation_mask = 0x05; // Cyl 1 & 3 (Bits 0, 2)
    }
    else if (type == ENGINE_V6) {
        if (deactivate_count == 2) active_deactivation_mask = 0x09; // Cyl 1 & 4 (Bits 0, 3)
        else if (deactivate_count == 3) active_deactivation_mask = 0x07; // One full bank (Bits 0, 1, 2)
    }
    
    // Recalculate LUT with new mask
    Update_LUT_with_Offset();
}

/* ---- Adaptive Timing & Lambda Management ---- */
#define MAX_TIMING_OFFSET 5  // Extended safe range: ±5 teeth (±30° crank)
static int8_t timing_offset = 0; 

typedef enum {
    LAMBDA_MODE_TIMING_ONLY = 0,   // Default: strict timing offset control
    LAMBDA_MODE_AUTO_CALIBRATE = 1,// Auto-adjust offset based on CAN STFT
    LAMBDA_MODE_EMULATION = 2      // External Op-Amp signal scaling mode
} LambdaMode_t;
static LambdaMode_t lambda_mode = LAMBDA_MODE_TIMING_ONLY;
static uint32_t last_auto_calib_time = 0;

/* ---- Skip-Fire Thermal Management Strategy ---- */
static uint8_t skip_active_cycles = 2;   // Default: 2 active cycles
static uint8_t skip_total_cycles = 3;    // Default: out of 3 total cycles
static uint32_t skip_cycle_counter = 0;

// SAFETY THRESHOLDS (UPDATED FOR IDLE DEACTIVATION)
#define MIN_RPM_FOR_DEACTIVATION 500    // ALLOWED at idle to reduce vibration/fuel
#define MIN_CLT_FOR_DEACTIVATION 80.0f  // STRICT: Must be fully warm

/* ---- Deferred UART diagnostics (ISR-safe) ---- */
static volatile uint8_t  pending_error_code = 0;
static volatile bool     error_pending = false;
static uint8_t           last_error_code = 0;

/* ---- LUT & Execution State ---- */
static uint8_t flute_lut[60] = {0};
static volatile bool tooth_updated = false;
static bool current_cycle_deactivated = false; // Used for DYNAMIC lambda emulation sync

/* ============================================================================
 * 2. GPIO SPEED MACROS (BSRR) FOR ALL 6 CYLINDERS & LAMBDA CONTROL
 * Atomic, single-cycle (~6ns) pin manipulation. No glitches.
 * ============================================================================ */
// Lambda Emulation Control (e.g., PA8 controls Op-Amp bypass or Digital Potentiometer)
#define LAMBDA_EMULATION_ENABLE()  (GPIOA->BSRR = GPIO_PIN_8)
#define LAMBDA_EMULATION_DISABLE() (GPIOA->BSRR = (GPIO_PIN_8 << 16))

// Cyl 1 (PB0=MUX, PB1=HS, PB2=LS)
#define INJ1_MUX_SET()   (GPIOB->BSRR = GPIO_PIN_0)
#define INJ1_MUX_RESET() (GPIOB->BSRR = (GPIO_PIN_0 << 16))
#define INJ1_LS_SET()    (GPIOB->BSRR = GPIO_PIN_2)
#define INJ1_LS_RESET()  (GPIOB->BSRR = (GPIO_PIN_2 << 16))
// Cyl 2 (PB3=MUX, PB4=HS, PB5=LS)
#define INJ2_MUX_SET()   (GPIOB->BSRR = GPIO_PIN_3)
#define INJ2_MUX_RESET() (GPIOB->BSRR = (GPIO_PIN_3 << 16))
#define INJ2_LS_SET()    (GPIOB->BSRR = GPIO_PIN_5)
#define INJ2_LS_RESET()  (GPIOB->BSRR = (GPIO_PIN_5 << 16))
// Cyl 3 (PB6=MUX, PB7=HS, PB8=LS)
#define INJ3_MUX_SET()   (GPIOB->BSRR = GPIO_PIN_6)
#define INJ3_MUX_RESET() (GPIOB->BSRR = (GPIO_PIN_6 << 16))
#define INJ3_LS_SET()    (GPIOB->BSRR = GPIO_PIN_8)
#define INJ3_LS_RESET()  (GPIOB->BSRR = (GPIO_PIN_8 << 16))
// Cyl 4 (PB9=MUX, PB10=HS, PB11=LS)
#define INJ4_MUX_SET()   (GPIOB->BSRR = GPIO_PIN_9)
#define INJ4_MUX_RESET() (GPIOB->BSRR = (GPIO_PIN_9 << 16))
#define INJ4_LS_SET()    (GPIOB->BSRR = GPIO_PIN_11)
#define INJ4_LS_RESET()  (GPIOB->BSRR = (GPIO_PIN_11 << 16))
// Cyl 5 (PC6=MUX, PC7=HS, PC8=LS)
#define INJ5_MUX_SET()   (GPIOC->BSRR = GPIO_PIN_6)
#define INJ5_MUX_RESET() (GPIOC->BSRR = (GPIO_PIN_6 << 16))
#define INJ5_LS_SET()    (GPIOC->BSRR = GPIO_PIN_8)
#define INJ5_LS_RESET()  (GPIOC->BSRR = (GPIO_PIN_8 << 16))
// Cyl 6 (PC9=MUX, PC10=HS, PC11=LS)
#define INJ6_MUX_SET()   (GPIOC->BSRR = GPIO_PIN_9)
#define INJ6_MUX_RESET() (GPIOC->BSRR = (GPIO_PIN_9 << 16))
#define INJ6_LS_SET()    (GPIOC->BSRR = GPIO_PIN_11)
#define INJ6_LS_RESET()  (GPIOC->BSRR = (GPIO_PIN_11 << 16))

/* ============================================================================
 * 3. CORE LOGIC FUNCTIONS
 * ============================================================================ */

/**
 * @brief Dynamically recalculates the LUT based on user timing offset.
 * UPDATED: Supports extended windows (8 teeth) into the exhaust stroke.
 */
void Update_LUT_with_Offset(void) {
    memset(flute_lut, 0, sizeof(flute_lut));
    
    // Base windows: Start tooth, duration (in teeth). Extended to 8 teeth.
    int8_t base_windows[][2] = {{58, 8}, {18, 8}, {38, 8}}; 
    uint8_t masks[] = {0x01, 0x02, 0x04};

    for (int cyl = 0; cyl < 3; cyl++) {
        for (int i = 0; i < base_windows[cyl][1]; i++) {
            int8_t current_tooth = (base_windows[cyl][0] + i + timing_offset) % 60;
            if (current_tooth < 0) current_tooth += 60; 
            flute_lut[current_tooth] |= masks[cyl];
        }
    }
}

/**
 * @brief Auto-calibrates timing offset based on OBD-II Short Term Fuel Trim.
 */
void Auto_Calibrate_Lambda(void) {
    if ((HAL_GetTick() - last_auto_calib_time) < 2000) return; // 2s hysteresis
    last_auto_calib_time = HAL_GetTick();

    if (cond_state.stft_percent > 5.0f) { // ECU sees lean, adds fuel
        if (timing_offset > -MAX_TIMING_OFFSET) {
            timing_offset--; 
            Update_LUT_with_Offset();
        }
    } else if (cond_state.stft_percent < -3.0f) { // ECU sees rich
        if (timing_offset < MAX_TIMING_OFFSET) {
            timing_offset++; 
            Update_LUT_with_Offset();
        }
    }
}

/**
 * @brief Evaluates if deactivation is safe and permitted for the current cycle.
 */
bool Should_Deactivate_This_Cycle(void) {
    // 1. STRICT TEMP PROTECTION
    if (cond_state.clt_temp_c < MIN_CLT_FOR_DEACTIVATION) return false;
    
    // 2. SAFE IDLE DEACTIVATION (Allowed down to 500 RPM)
    if (sync_state.filtered_rpm < MIN_RPM_FOR_DEACTIVATION) return false;
    
    // 3. LOAD PROTECTION
    if (cond_state.tps_percent > 30) return false;

    // 4. SKIP-FIRE LOGIC
    uint8_t pos = skip_cycle_counter % skip_total_cycles;
    bool is_active_cycle = (pos < skip_active_cycles);
    skip_cycle_counter++;
    
    return !is_active_cycle; 
}

/**
 * @brief Immediately forces all injector drivers to the STOCK state.
 */
void Force_Stock_Mode(void) {
    INJ1_LS_RESET(); INJ2_LS_RESET(); INJ3_LS_RESET();
    INJ4_LS_RESET(); INJ5_LS_RESET(); INJ6_LS_RESET();
    INJ1_MUX_RESET(); INJ2_MUX_RESET(); INJ3_MUX_RESET();
    INJ4_MUX_RESET(); INJ5_MUX_RESET(); INJ6_MUX_RESET();
    
    // Also disable lambda emulation when forcing stock mode
    LAMBDA_EMULATION_DISABLE();
}

/* ============================================================================
 * 4. PERIPHERAL INITIALIZATION
 * ============================================================================ */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON; 
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8; RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2; RCC_OscInitStruct.PLL.PLLQ = 7;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4; // 42 MHz
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2; // 84 MHz
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}

static void MX_GPIO_Init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE();
    
    // SAFE START: Atomically reset ALL pins BEFORE initialization
    GPIOA->BSRR = (GPIO_PIN_8 << 16); // Ensure Lambda Emulation is OFF at boot
    GPIOB->BSRR = 0x0FFF << 16; 
    GPIOC->BSRR = 0x0FC0 << 16;

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    
    // Lambda Control Pin
    GPIO_InitStruct.Pin = GPIO_PIN_8;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Cylinder Driver Pins
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | 
                          GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    // CKP (PA6) & CMP (PA7)
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

static void MX_TIM3_Init(void) {
    __HAL_RCC_TIM3_CLK_ENABLE();
    TIM_IC_InitTypeDef sConfigIC = {0};
    
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 84 - 1; // APB1=42MHz -> Timer=84MHz -> 84-1 = 1 MHz (1 µs/tick)
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 0xFFFF;
    HAL_TIM_Base_Init(&htim3);

    sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter = 15; // 🔥 HARDWARE FILTER: Ignores coil ringing < 15 µs
    
    HAL_TIM_IC_ConfigChannel(&htim3, &sConfigIC, TIM_CHANNEL_1); // CKP
    HAL_TIM_IC_ConfigChannel(&htim3, &sConfigIC, TIM_CHANNEL_2); // CMP
    
    HAL_NVIC_SetPriority(TIM3_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM3_IRQn);
    HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_1);
    HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_2);
}

static void MX_IWDG_Init(void) {
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_32; // LSI (~32kHz) / 32 = 1 kHz (1 ms/tick)
    hiwdg.Init.Reload = 1000;                 // 1000 ms = 1 second timeout
    HAL_IWDG_Init(&hiwdg);
    HAL_IWDG_Refresh(&hiwdg);
}

static void MX_UART_Init(void) {
    huart1.Instance = USART1; huart1.Init.BaudRate = 115200; huart1.Init.Mode = UART_MODE_TX_RX;
    HAL_UART_Init(&huart1);
    huart2.Instance = USART2; huart2.Init.BaudRate = 115200; huart2.Init.Mode = UART_MODE_TX_RX;
    HAL_UART_Init(&huart2);
}

static void MX_CAN_Init(void) {
    __HAL_RCC_CAN1_CLK_ENABLE();
    hcan.Instance = CAN1;
    hcan.Init.Prescaler = 6;          
    hcan.Init.Mode = CAN_MODE_NORMAL;
    hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1 = CAN_BS1_10TQ; 
    hcan.Init.TimeSeg2 = CAN_BS2_3TQ;  
    HAL_CAN_Init(&hcan);

    CAN_FilterTypeDef filter = {0};
    filter.FilterActivation = CAN_FILTER_ENABLE;
    filter.FilterBank = 0;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterIdHigh = 0x7E8 << 5; 
    filter.FilterIdLow = 0x0000;
    filter.FilterMaskIdHigh = 0x7FF << 5;
    filter.FilterMaskIdLow = 0x0000;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.SlaveStartFilterBank = 14;
    HAL_CAN_ConfigFilter(&hcan, &filter);
    
    HAL_CAN_Start(&hcan);
    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
}

/* ============================================================================
 * 5. INTERRUPT SERVICE ROUTINES (ISR)
 * ============================================================================ */
void TIM3_IRQHandler(void) {
    uint32_t sr = TIM3->SR;

    if (sr & TIM_FLAG_CC1) { // CKP
        __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_CC1);
        uint32_t now = TIM3->CCR1;
        uint32_t delta = (uint16_t)(now - sync_state.last_ckp_time); 
        sync_state.last_ckp_time = now;

        if (delta < 100) return; 

        if (sync_state.tooth_period_us > 0 && delta > (sync_state.tooth_period_us * 5 / 2)) {
            sync_state.current_tooth = 0;
        } else {
            sync_state.current_tooth++;
            if (sync_state.current_tooth >= 60) sync_state.current_tooth = 0;
        }
        sync_state.tooth_period_us = delta;

        if (delta > 0) sync_state.raw_rpm = 1000000UL / delta;
        sync_state.filtered_rpm = (uint32_t)(0.2f * (float)sync_state.raw_rpm + 0.8f * (float)sync_state.filtered_rpm);

        if (sync_state.filtered_rpm < 100) {
            sync_state.engine_state = ENGINE_STOPPED;
            sync_state.is_phase_synced = false;
        } else if (sync_state.filtered_rpm < 300) {
            sync_state.engine_state = ENGINE_CRANKING;
        } else {
            sync_state.engine_state = ENGINE_RUNNING;
        }
        tooth_updated = true;
    }

    if (sr & TIM_FLAG_CC2) { // CMP
        __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_CC2);
        if (sync_state.current_tooth >= 5 && sync_state.current_tooth <= 15) {
            sync_state.is_phase_synced = true;
        } else {
            sync_state.is_phase_synced = false;
            pending_error_code = 1; error_pending = true; 
        }
    }
}

void CAN1_RX0_IRQHandler(void) { HAL_CAN_IRQHandler(&hcan); }

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    CAN_RxHeaderTypeDef rx_header = {0};
    uint8_t rx_data[8] = {0};

    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK) {
        if (rx_header.StdId == 0x7E8 && rx_data[0] == 0x41) {
            switch (rx_data[1]) {
                case 0x05: cond_state.clt_temp_c = (float)(rx_data[2] - 40); break; 
                case 0x11: cond_state.tps_percent = (uint8_t)((rx_data[2] * 100U) / 255U); break; 
                case 0x0B: cond_state.map_kpa = rx_data[2]; break; 
                case 0x14: cond_state.stft_percent = ((float)rx_data[2] / 1.28f) - 100.0f; break;
            }
        }
    }
}

void HAL_IncTick(void) { /* Standard HAL tick increment */ }

/* ============================================================================
 * 6. CORE EXECUTION & UART PARSING
 * ============================================================================ */

/* Global variable for RPM activation limit (configurable via Web UI) */
static uint16_t rpm_activation_limit = 2500;

/**
 * @brief Main execution function called once per new tooth event.
 */
void Flute_Execute(void) {
    // 1. FAIL-SAFE GUARD
    if (cond_state.clt_temp_c < MIN_CLT_FOR_DEACTIVATION || 
        sync_state.engine_state != ENGINE_RUNNING || 
        !sync_state.is_phase_synced) {
        Force_Stock_Mode();
        return;
    }

    // 2. ATOMIC TOOTH READ
    __disable_irq();
    uint16_t tooth = sync_state.current_tooth;
    __enable_irq();

    if (tooth >= 60) {
        Force_Stock_Mode();
        return;
    }

    // 3. SKIP-FIRE SYNCHRONIZATION
    if (tooth == 0) {
        current_cycle_deactivated = Should_Deactivate_This_Cycle();
    }

    if (!current_cycle_deactivated) {
        Force_Stock_Mode();
        return;
    }

    // 4. LOOKUP TABLE (LUT) EXECUTION
    uint8_t actions = flute_lut[tooth];
    if (actions == 0) {
        Force_Stock_Mode();
        return;
    }

    // 5. ATOMIC GPIO MANIPULATION (BSRR)
    if ((actions & 0x01) && (active_deactivation_mask & 0x01)) { INJ1_MUX_SET(); INJ1_LS_SET(); } 
    else { INJ1_MUX_RESET(); INJ1_LS_RESET(); }

    if ((actions & 0x02) && (active_deactivation_mask & 0x02)) { INJ2_MUX_SET(); INJ2_LS_SET(); } 
    else { INJ2_MUX_RESET(); INJ2_LS_RESET(); }

    if ((actions & 0x04) && (active_deactivation_mask & 0x04)) { INJ3_MUX_SET(); INJ3_LS_SET(); } 
    else { INJ3_MUX_RESET(); INJ3_LS_RESET(); }

    if ((actions & 0x08) && (active_deactivation_mask & 0x08)) { INJ4_MUX_SET(); INJ4_LS_SET(); } 
    else { INJ4_MUX_RESET(); INJ4_LS_RESET(); }

    if ((actions & 0x10) && (active_deactivation_mask & 0x10)) { INJ5_MUX_SET(); INJ5_LS_SET(); } 
    else { INJ5_MUX_RESET(); INJ5_LS_RESET(); }

    if ((actions & 0x20) && (active_deactivation_mask & 0x20)) { INJ6_MUX_SET(); INJ6_LS_SET(); } 
    else { INJ6_MUX_RESET(); INJ6_LS_RESET(); }
}

/**
 * @brief Non-blocking UART command parser.
 */
static void Process_UART_Commands(void) {
    static char rx_buffer[128];
    static uint8_t rx_index = 0;
    uint8_t ch;

    while (HAL_UART_Receive(&huart2, &ch, 1, 0) == HAL_OK) {
        if (ch == '\n' || rx_index >= sizeof(rx_buffer) - 1) {
            rx_buffer[rx_index] = '\0';
            rx_index = 0;

            if (strstr(rx_buffer, "\"set_rpm_limit\":") != NULL) {
                char* ptr = strstr(rx_buffer, "\"set_rpm_limit\":");
                ptr += 16;
                int val = atoi(ptr);
                if (val >= 1000 && val <= 6000) {
                    rpm_activation_limit = (uint16_t)val;
                    char ack[64];
                    snprintf(ack, sizeof(ack), "{\"ack\":\"rpm_limit\",\"value\":%d}\n", rpm_activation_limit);
                    HAL_UART_Transmit(&huart2, (uint8_t*)ack, strlen(ack), 50);
                }
            }
            else if (strstr(rx_buffer, "\"emergency_stop\":true") != NULL) {
                Force_Stock_Mode();
                pending_error_code = 99; 
                error_pending = true;
                const char ack[] = "{\"ack\":\"emergency_stop\",\"status\":\"ok\"}\n";
                HAL_UART_Transmit(&huart2, (uint8_t*)ack, sizeof(ack) - 1, 50);
            }
            else if (strstr(rx_buffer, "\"set_timing_offset\":") != NULL) {
                char* ptr = strstr(rx_buffer, "\"set_timing_offset\":");
                ptr += 20;
                int val = atoi(ptr);
                if (val >= -MAX_TIMING_OFFSET && val <= MAX_TIMING_OFFSET) {
                    timing_offset = (int8_t)val;
                    Update_LUT_with_Offset();
                    char ack[64];
                    snprintf(ack, sizeof(ack), "{\"ack\":\"timing_offset\",\"value\":%d}\n", timing_offset);
                    HAL_UART_Transmit(&huart2, (uint8_t*)ack, strlen(ack), 50);
                }
            }
            else if (strstr(rx_buffer, "\"set_lambda_mode\":") != NULL) {
                char* ptr = strstr(rx_buffer, "\"set_lambda_mode\":");
                ptr += 18;
                int val = atoi(ptr);
                if (val >= 0 && val <= 2) {
                    lambda_mode = (LambdaMode_t)val;
                    char ack[64];
                    snprintf(ack, sizeof(ack), "{\"ack\":\"lambda_mode\",\"value\":%d}\n", lambda_mode);
                    HAL_UART_Transmit(&huart2, (uint8_t*)ack, strlen(ack), 50);
                }
            }
            else if (strstr(rx_buffer, "\"set_skip_pattern\":") != NULL) {
                char* ptr = strstr(rx_buffer, "\"set_skip_pattern\":");
                ptr += 20;
                int active = atoi(ptr);
                char* comma = strchr(ptr, ',');
                if (comma != NULL) {
                    int total = atoi(comma + 1);
                    if (active >= 1 && total >= 2 && total <= 10 && active <= total) {
                        skip_active_cycles = (uint8_t)active;
                        skip_total_cycles = (uint8_t)total;
                        skip_cycle_counter = 0;
                        char ack[64];
                        snprintf(ack, sizeof(ack), "{\"ack\":\"skip_pattern\",\"active\":%d,\"total\":%d}\n", 
                                 skip_active_cycles, skip_total_cycles);
                        HAL_UART_Transmit(&huart2, (uint8_t*)ack, strlen(ack), 50);
                    }
                }
            }
            else if (strstr(rx_buffer, "\"set_engine_profile\":") != NULL) {
                char* ptr = strstr(rx_buffer, "\"set_engine_profile\":");
                ptr += 21;
                int type = atoi(ptr);
                char* comma = strchr(ptr, ',');
                if (comma != NULL) {
                    int count = atoi(comma + 1);
                    if (type >= 0 && type <= 2 && count >= 1 && count <= 3) {
                        Set_Engine_Profile((EngineType_t)type, (uint8_t)count);
                        char ack[64];
                        snprintf(ack, sizeof(ack), "{\"ack\":\"engine_profile\",\"type\":%d,\"count\":%d}\n", type, count);
                        HAL_UART_Transmit(&huart2, (uint8_t*)ack, strlen(ack), 50);
                    }
                }
            }
        } else {
            rx_buffer[rx_index++] = (char)ch;
        }
    }
}

/**
 * @brief Deferred UART diagnostics transmission (ISR-safe).
 */
static void Diagnostics_FlushPending(void) {
    if (!error_pending) return;
    error_pending = false;

    if (pending_error_code == 0) {
        last_error_code = 0;
        const char clear_msg[] = "{\"alert\":\"CLEAR\",\"code\":0}\n";
        HAL_UART_Transmit(&huart2, (uint8_t*)clear_msg, sizeof(clear_msg) - 1, 50);
    } else {
        last_error_code = pending_error_code;
        char msg[64];
        int len = snprintf(msg, sizeof(msg), "{\"alert\":\"ERROR\",\"code\":%d}\n", pending_error_code);
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, (uint16_t)len, 50);
    }
}

/* ============================================================================
 * 7. MAIN FUNCTION & SUPER-LOOP
 * ============================================================================ */
int main(void) {
    HAL_Init();
    SystemClock_Config();

    // 🔥 CRITICAL SAFETY: Check for previous Watchdog reset
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET) {
        __HAL_RCC_CLEAR_RESET_FLAGS();
        pending_error_code = 3; // Code 3: Watchdog reset
        error_pending = true;
    }

    MX_GPIO_Init();
    Force_Stock_Mode(); // Double safety: ensure all outputs are LOW
    MX_UART_Init();
    MX_TIM3_Init();
    MX_CAN_Init();
    MX_IWDG_Init();

    Set_Engine_Profile(ENGINE_I6, 3);
    Update_LUT_with_Offset();
    rpm_activation_limit = 2500;

    const char* boot_msg = "\r\n=== FLUTE SYSTEM v2.2 (Competition Ready) ===\r\n"
                           "MCU: STM32F405 @ 168 MHz | CAN: 500 kbps | UART: 115200\r\n"
                           "Safety: IWDG 1s, Min RPM 500 (Idle allowed), Min CLT 80C\r\n"
                           "Profiles: I4, I6, V6 supported with dynamic LUT generation.\r\n\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t*)boot_msg, strlen(boot_msg), 100);

    uint32_t last_telemetry = 0;
    uint32_t last_can_request = 0;
    uint8_t can_pid_index = 0;

    while (1) {
        // TASK 1: Staggered OBD-II CAN Requests (every 200ms)
        if ((HAL_GetTick() - last_can_request) >= 200) {
            last_can_request = HAL_GetTick();
            uint8_t pids[] = {0x05, 0x11, 0x0B, 0x14};
            CAN_TxHeaderTypeDef tx_header = {0};
            uint8_t tx_data[8] = {0x02, 0x01, pids[can_pid_index], 0, 0, 0, 0, 0};
            
            tx_header.StdId = 0x7DF;
            tx_header.ExtId = 0;
            tx_header.RTR = CAN_RTR_DATA;
            tx_header.IDE = CAN_ID_STD;
            tx_header.DLC = 8;
            tx_header.TransmitGlobalTime = DISABLE;
            
            uint32_t tx_mailbox;
            HAL_CAN_AddTxMessage(&hcan, &tx_header, tx_data, &tx_mailbox);
            can_pid_index = (can_pid_index + 1) % 4;
        }

        // TASK 2: Execute Flute logic
        if (tooth_updated) {
            tooth_updated = false;
            Flute_Execute();
        }

        // TASK 3: Lambda Management Logic (DYNAMIC)
        if (lambda_mode == LAMBDA_MODE_AUTO_CALIBRATE) {
            Auto_Calibrate_Lambda();
            LAMBDA_EMULATION_DISABLE(); 
        } 
        else if (lambda_mode == LAMBDA_MODE_EMULATION) {
            if (current_cycle_deactivated && sync_state.engine_state == ENGINE_RUNNING) {
                LAMBDA_EMULATION_ENABLE();  // Engage Op-Amp / Digital Pot to shift signal
            } else {
                LAMBDA_EMULATION_DISABLE(); // Bypass circuit, pass raw sensor signal to ECU
            }
        } 
        else {
            LAMBDA_EMULATION_DISABLE();
        }

        // TASK 4: Flush pending diagnostic alerts
        Diagnostics_FlushPending();

        // TASK 5: Parse incoming commands from ESP32
        Process_UART_Commands();

        // TASK 6: Telemetry to ESP32 (every 100ms)
        if ((HAL_GetTick() - last_telemetry) >= 100) {
            last_telemetry = HAL_GetTick();
            char tx_buf[180];
            
            uint8_t deact_count = 0;
            if (active_deactivation_mask == 0x05) deact_count = 2;
            else if (active_deactivation_mask == 0x07 || active_deactivation_mask == 0x09) deact_count = 3;

            int len = snprintf(tx_buf, sizeof(tx_buf),
                     "{\"rpm\":%lu,\"temp\":%.1f,\"tps\":%u,\"sync\":%d,\"limit\":%u,"
                     "\"engine_type\":%d,\"deact_count\":%u,"
                     "\"skip_active\":%u,\"skip_total\":%u,\"lambda_mode\":%d,\"timing_offset\":%d}\n",
                     (unsigned long)sync_state.filtered_rpm,
                     (double)cond_state.clt_temp_c,
                     (unsigned)cond_state.tps_percent,
                     (unsigned)sync_state.is_phase_synced,
                     (unsigned)rpm_activation_limit,
                     (int)current_engine,
                     (unsigned)deact_count,
                     (unsigned)skip_active_cycles,
                     (unsigned)skip_total_cycles,
                     (unsigned)lambda_mode,
                     (int)timing_offset);
                     
            HAL_UART_Transmit(&huart2, (uint8_t*)tx_buf, (uint16_t)len, 50);
        }

        // TASK 7: 🔥 SAFE WATCHDOG FEED
        HAL_IWDG_Refresh(&hiwdg);
    }
}

/**
 * @brief Hard fault handler: disable all interrupts and force stock mode.
 */
void Error_Handler(void) {
    __disable_irq();
    while (1) {
        Force_Stock_Mode();
    }
}
