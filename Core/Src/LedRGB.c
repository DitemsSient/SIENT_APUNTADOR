/**
 * @file    LedRGB.c
 * @brief   Driver implementation for common-cathode RGB LED via GPIO.
 *
 * @date    March 28, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#include "LedRGB.h"

/* ======================  STATIC VARIABLES  ================================ */

/* Last color set — used by Toggle to restore the previous state */

static RGBColor_t last_color = { 0U, 0U, 0U };

/* Current on/off state */

static uint8_t is_on = 0U;

/* ======================  STATIC FUNCTIONS  ================================ */

/**
 * @brief  Drives the three GPIO pins to match the given color.
 * @param  color  Color to apply.
 * @note   Common cathode: GPIO_PIN_SET turns the channel on.
 */
static void LedRGB_ApplyColor(RGBColor_t color) {
    HAL_GPIO_WritePin(RGB_R_PORT, RGB_R_PIN,
                      (color.r) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RGB_G_PORT, RGB_G_PIN,
                      (color.g) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RGB_B_PORT, RGB_B_PIN,
                      (color.b) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ================================  API  =================================== */

/* Public functions declared in the .h */

/**
 * @brief  Turns all channels off via LedRGB_Off().
 */
void LedRGB_Init(void) {
    LedRGB_Off();
}

/**
 * @brief  Updates last_color and is_on, then calls LedRGB_ApplyColor().
 */
void LedRGB_SetColor(RGBColor_t color) {
    last_color = color;
    is_on = (color.r || color.g || color.b) ? 1U : 0U;
    LedRGB_ApplyColor(color);
}

/**
 * @brief  Delegates to LedRGB_SetColor() with RGB_WHITE.
 */
void LedRGB_On(void) {
    LedRGB_SetColor(RGB_WHITE);
}

/**
 * @brief  Clears is_on and drives all pins low via LedRGB_ApplyColor().
 */
void LedRGB_Off(void) {
    is_on = 0U;
    LedRGB_ApplyColor(RGB_OFF);
}

/**
 * @brief  If on, turns off. If off, restores last_color (defaults to white
 *         if no color was ever set).
 */
void LedRGB_Toggle(void) {
    if (is_on) {
        LedRGB_Off();
    } else {
        if (last_color.r == 0U && last_color.g == 0U && last_color.b == 0U) {
            last_color = (RGBColor_t){ 1U, 1U, 1U };
        }
        is_on = 1U;
        LedRGB_ApplyColor(last_color);
    }
}

/**
 * @brief  Loops times cycles, applying the color then RGB_OFF with the given
 *         delays. Leaves the LED off and clears is_on when done.
 */
void LedRGB_Blink(RGBColor_t color, uint8_t times, uint16_t on_ms, uint16_t off_ms) {
    for (uint8_t i = 0U; i < times; i++) {
        LedRGB_ApplyColor(color);
        HAL_Delay(on_ms);
        LedRGB_ApplyColor(RGB_OFF);
        HAL_Delay(off_ms);
    }
    is_on = 0U;
}

/**
 * @brief  Iterates over the step array, applying each color for duration_ms,
 *         then turns the LED off when the sequence completes.
 */
void LedRGB_PlayPattern(const RGBStep_t *pattern, uint16_t length) {
    for (uint16_t i = 0U; i < length; i++) {
        LedRGB_ApplyColor(pattern[i].color);
        HAL_Delay(pattern[i].duration_ms);
    }
    LedRGB_ApplyColor(RGB_OFF);
    is_on = 0U;
}

/* ========================  SELF-TEST  ==================================== */

#define RGB_TEST_STEP_MS    1500U
#define RGB_TEST_CYCLES     2U

void LedRGB_Test(void)
{
    for (uint8_t i = 0U; i < RGB_TEST_CYCLES; i++) {
        LedRGB_SetColor(RGB_RED);   HAL_Delay(RGB_TEST_STEP_MS);
        LedRGB_SetColor(RGB_GREEN); HAL_Delay(RGB_TEST_STEP_MS);
        LedRGB_SetColor(RGB_BLUE);  HAL_Delay(RGB_TEST_STEP_MS);
    }
    LedRGB_Off();
}
