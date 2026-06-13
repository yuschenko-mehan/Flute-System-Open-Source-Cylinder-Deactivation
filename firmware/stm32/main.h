/*
 * ============================================================================
 * FLUTE SYSTEM – MASTER HEADER
 * Target: STM32F405RGT6 @ 168 MHz
 * ============================================================================
 */
#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f4xx_hal.h"

/* ---- External HAL handles ---- */
extern UART_HandleTypeDef huart1;      // Debug UART (PA9, PA10)
extern UART_HandleTypeDef huart2;      // ESP32 communication (PA2, PA3)
extern IWDG_HandleTypeDef hiwdg;       // Independent Watchdog (1s timeout)
extern TIM_HandleTypeDef  htim3;       // CKP/CMP capture timer (1µs resolution)
extern CAN_HandleTypeDef  hcan;        // CAN1 for OBD-II (500 kbps)

/* ---- Public functions ---- */
void Error_Handler(void);

#endif /* __MAIN_H */
