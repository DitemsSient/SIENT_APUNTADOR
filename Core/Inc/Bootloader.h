/**
 * @file    Bootloader.h
 * @brief   Software jump to the STM32 factory USB DFU bootloader, gated by a button held at boot.
 *
 * @details No BOOT0 pin is exposed on this board, so entering the ST system
 *          bootloader (System Memory, address 0x1FFF0000 on STM32L433) is
 *          done entirely in software: if the configured button is held when
 *          Bootloader_CheckAndEnter() runs (call it as early as possible in
 *          main(), right after MX_GPIO_Init()), the MCU resets its clocks/
 *          peripherals to a clean state and jumps to the bootloader's reset
 *          vector — the CPU ends up running ST's code exactly as if BOOT0
 *          had been HIGH at reset. From there the bootloader brings up USB
 *          on its own (PA11/PA12) and enumerates as a DFU device; flash with
 *          STM32CubeProgrammer selecting "USB" instead of "ST-LINK".
 *
 *          LED feedback (via LedRGB, must be initialized before calling):
 *          RED blinking x3 (500 ms each)   = button held, about to jump.
 *          GREEN blinking x3 (500 ms each) = button not held, normal boot.
 *
 *          This driver intentionally has no Driver_Test() — the only way to
 *          "test" it is to actually jump, which ends normal execution.
 *
 *          CubeMX / .ioc requirements:
 *          - BOOTLOADER_BTN_PORT/PIN must already be configured as GPIO
 *            input (reuses an existing button, e.g. BOTON_A).
 *          - LedRGB must be initialized before this call for the visual
 *            feedback to work.
 *
 * @date    August 23, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include "stm32l4xx_hal.h"
#include <stdbool.h>

/* ========================  CONFIGURATION  ================================= */

/* Button read at boot to decide whether to enter the bootloader.
 * Reuses BOTON_A (PB11) — active-low, same convention as the rest of the
 * board's buttons. Change here if the redesign moves this to another pin. */

#define BOOTLOADER_BTN_PORT         GPIOB
#define BOOTLOADER_BTN_PIN          GPIO_PIN_11

/* System Memory base address for STM32L433 (AN2606, STM32L43xxx/L44xxx table).
 * Confirmed via ST documentation — do not reuse this value on a different
 * STM32 family/line without checking AN2606 again. */

#define BOOTLOADER_SYSMEM_ADDR      0x1FFF0000UL

/* Blink pattern shown before deciding: N blinks, each half-cycle (on or off)
 * held for BOOTLOADER_LED_BLINK_MS. */

#define BOOTLOADER_LED_BLINK_MS     500U
#define BOOTLOADER_LED_BLINK_COUNT  3U

/* ========================  ENUMERATIONS  ================================== */

/* Return code — only the "not entered" path actually returns; the "enter"
 * path jumps away and never comes back to the caller. */

typedef enum {
    BOOTLOADER_NOT_ENTERED = 0    /**< Button not held, continue normal boot */
} Bootloader_Status_e;

/* ================================  API  =================================== */

/**
 * @brief  Checks the boot button and, if held, jumps to the USB DFU bootloader.
 * @note   Call as early as possible in main(), right after MX_GPIO_Init()
 *         and LedRGB_Init() — before any other peripheral/driver init that
 *         you would not want left half-configured if this jumps away.
 *         Blinks the RGB LED red x3 and jumps (never returns) if the button
 *         is held; blinks it green x3, turns it off, and returns
 *         BOOTLOADER_NOT_ENTERED otherwise.
 */
Bootloader_Status_e Bootloader_CheckAndEnter(void);

#endif /* BOOTLOADER_H */
