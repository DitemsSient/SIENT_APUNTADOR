/**
 * @file    Bootloader.c
 * @brief   Driver implementation for the software jump to the USB DFU bootloader.
 *
 * @date    August 23, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#include "Bootloader.h"
#include "LedRGB.h"

/* ======================  STATIC FUNCTIONS  ================================ */

/**
 * @brief  Resets clocks/peripherals to a clean state and jumps to the
 *         System Memory reset vector. Never returns.
 * @note   Mirrors what the hardware boot selector does when BOOT0 is HIGH
 *         at reset, just triggered from running application code instead.
 */
static void Bootloader_JumpToSystemMemory(void) {
    void (*SysMemBootJump)(void);

    /* Bring clocks/peripherals back to their reset state so the bootloader
     * starts from the same conditions it would after a real reset. */
    HAL_RCC_DeInit();
    HAL_DeInit();

    /* Stop SysTick and mask every interrupt before leaving. */
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;

    __disable_irq();
    for (uint8_t i = 0U; i < 8U; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }
    __enable_irq();

    /* Remap System Memory to 0x00000000 and point VTOR at its vector table,
     * so the bootloader sees its own vectors, not the application's. */
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();
    SCB->VTOR = BOOTLOADER_SYSMEM_ADDR;

    /* Read the bootloader's initial stack pointer and reset handler from
     * its vector table, then jump. */
    uint32_t jump_address = *(__IO uint32_t *)(BOOTLOADER_SYSMEM_ADDR + 4U);
    SysMemBootJump = (void (*)(void))jump_address;

    __set_MSP(*(__IO uint32_t *)BOOTLOADER_SYSMEM_ADDR);
    SysMemBootJump();

    /* Never reached. */
    while (1) { }
}

/**
 * @brief  Blinks the RGB LED a fixed number of times in the given color.
 * @param  color  Color to blink (e.g. RGB_RED, RGB_GREEN).
 * @param  times  Number of on/off cycles.
 * @param  ms     Duration of each on/off half-cycle in milliseconds.
 * @note   Leaves the LED off when finished.
 */
static void Bootloader_BlinkLed(RGBColor_t color, uint8_t times, uint32_t ms) {
    for (uint8_t i = 0U; i < times; i++) {
        LedRGB_SetColor(color);
        HAL_Delay(ms);
        LedRGB_Off();
        HAL_Delay(ms);
    }
}

/* ================================  API  =================================== */

/* Public functions declared in the .h */

Bootloader_Status_e Bootloader_CheckAndEnter(void) {
    bool pressed = (HAL_GPIO_ReadPin(BOOTLOADER_BTN_PORT, BOOTLOADER_BTN_PIN) == GPIO_PIN_RESET);

    if (pressed) {
        Bootloader_BlinkLed(RGB_RED, BOOTLOADER_LED_BLINK_COUNT, BOOTLOADER_LED_BLINK_MS);
        Bootloader_JumpToSystemMemory();
        /* Unreachable. */
    }

    Bootloader_BlinkLed(RGB_GREEN, BOOTLOADER_LED_BLINK_COUNT, BOOTLOADER_LED_BLINK_MS);

    return BOOTLOADER_NOT_ENTERED;
}
