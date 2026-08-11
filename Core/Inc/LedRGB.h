/**
 * @file    LedRGB.h
 * @brief   Driver for common-cathode RGB LED via GPIO on STM32F4xx.
 *
 * @details Controls a common-cathode RGB LED using three GPIO output pins
 *          (one per channel: R, G, B). Each channel is ON/OFF, giving 8
 *          possible color combinations including off.
 *          Update RGB_R/G/B_PORT and RGB_R/G/B_PIN to match the GPIO
 *          configuration in CubeMX.
 *
 * @date    March 28, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#ifndef LED_RGB_H
#define LED_RGB_H

#include "stm32l4xx_hal.h"

/* ========================  CONFIGURATION  ================================= */

/* GPIO port and pin for each color channel */

#define RGB_R_PORT      GPIOB
#define RGB_R_PIN       GPIO_PIN_0

#define RGB_G_PORT      GPIOB
#define RGB_G_PIN       GPIO_PIN_1

#define RGB_B_PORT      GPIOB
#define RGB_B_PIN       GPIO_PIN_2

/* ============================  STRUCTURES  ================================ */

/* Represents an RGB color as three ON/OFF channel states */

typedef struct {
    uint8_t r;  /**< Red channel   (0 = off, 1 = on) */
    uint8_t g;  /**< Green channel (0 = off, 1 = on) */
    uint8_t b;  /**< Blue channel  (0 = off, 1 = on) */
} RGBColor_t;

/* Step inside an RGB pattern: a color and how long to hold it */

typedef struct {
    RGBColor_t color;       /**< Color to display on this step            */
    uint16_t   duration_ms; /**< Step duration in milliseconds            */
} RGBStep_t;

/* ====================  DEVICE CONSTANTS  ================================== */

/* Predefined 8-color palette */

#define RGB_OFF         ((RGBColor_t){ 0, 0, 0 })
#define RGB_RED         ((RGBColor_t){ 1, 0, 0 })
#define RGB_GREEN       ((RGBColor_t){ 0, 1, 0 })
#define RGB_BLUE        ((RGBColor_t){ 0, 0, 1 })
#define RGB_YELLOW      ((RGBColor_t){ 1, 1, 0 })
#define RGB_CYAN        ((RGBColor_t){ 0, 1, 1 })
#define RGB_MAGENTA     ((RGBColor_t){ 1, 0, 1 })
#define RGB_WHITE       ((RGBColor_t){ 1, 1, 1 })

/* Calculates the number of steps in a static RGBStep_t array */

#define RGB_PATTERN_LEN(arr)    (sizeof(arr) / sizeof((arr)[0]))

/* ====================  PREDEFINED PATTERNS  =============================== */

/* Red blink — error / failure */

static const RGBStep_t RGB_PATTERN_ERROR[] = {
    { { 1, 0, 0 },  150U },
    { { 0, 0, 0 },  100U },
    { { 1, 0, 0 },  150U },
    { { 0, 0, 0 },  100U },
    { { 1, 0, 0 },  150U },
};
#define RGB_PATTERN_ERROR_LEN       5U

/* Green blink — success / confirmation */

static const RGBStep_t RGB_PATTERN_OK[] = {
    { { 0, 1, 0 },  200U },
    { { 0, 0, 0 },  150U },
    { { 0, 1, 0 },  200U },
};
#define RGB_PATTERN_OK_LEN          3U

/* Blue blink — waiting / standby */

static const RGBStep_t RGB_PATTERN_STANDBY[] = {
    { { 0, 0, 1 },  500U },
    { { 0, 0, 0 },  500U },
};
#define RGB_PATTERN_STANDBY_LEN     2U

/* Rainbow cycle — cycles through 6 main colors */

static const RGBStep_t RGB_PATTERN_RAINBOW[] = {
    { { 1, 0, 0 },  250U },     /* Red     */
    { { 1, 1, 0 },  250U },     /* Yellow  */
    { { 0, 1, 0 },  250U },     /* Green   */
    { { 0, 1, 1 },  250U },     /* Cyan    */
    { { 0, 0, 1 },  250U },     /* Blue    */
    { { 1, 0, 1 },  250U },     /* Magenta */
};
#define RGB_PATTERN_RAINBOW_LEN     6U

/* Yellow blink — caution / warning */

static const RGBStep_t RGB_PATTERN_WARNING[] = {
    { { 1, 1, 0 },  200U },
    { { 0, 0, 0 },  150U },
    { { 1, 1, 0 },  200U },
    { { 0, 0, 0 },  150U },
    { { 1, 1, 0 },  200U },
};
#define RGB_PATTERN_WARNING_LEN     5U

/* Fast white blink — trigger / action flash */

static const RGBStep_t RGB_PATTERN_FLASH[] = {
    { { 1, 1, 1 },   50U },
    { { 0, 0, 0 },   50U },
    { { 1, 1, 1 },   50U },
};
#define RGB_PATTERN_FLASH_LEN       3U

/* ================================  API  =================================== */

/**
 * @brief  Configures the RGB GPIO pins and turns the LED off.
 * @note   Call once after MX_GPIO_Init().
 */
void LedRGB_Init(void);

/**
 * @brief  Sets the LED to the given color.
 * @param  color  RGBColor_t with the state of each channel.
 *                Use the predefined colors: RGB_RED, RGB_GREEN, etc.
 */
void LedRGB_SetColor(RGBColor_t color);

/**
 * @brief  Turns the LED on at full white (all three channels ON).
 */
void LedRGB_On(void);

/**
 * @brief  Turns the LED off completely (all three channels OFF).
 */
void LedRGB_Off(void);

/**
 * @brief  Toggles the LED state.
 * @note   If the LED was on, it turns off; if off, it restores the last color.
 */
void LedRGB_Toggle(void);

/**
 * @brief  Blinks a color a given number of times.
 * @param  color    Color to blink.
 * @param  times    Number of blink cycles.
 * @param  on_ms    On-time per cycle in milliseconds.
 * @param  off_ms   Off-time between cycles in milliseconds.
 * @note   Blocking. The LED is left off when finished.
 */
void LedRGB_Blink(RGBColor_t color, uint8_t times, uint16_t on_ms, uint16_t off_ms);

/**
 * @brief  Plays a full RGBStep_t pattern sequence.
 * @param  pattern  Pointer to the step array.
 * @param  length   Number of steps in the array.
 * @note   Blocking. The LED is left off when finished.
 */
void LedRGB_PlayPattern(const RGBStep_t *pattern, uint16_t length);

/* ========================  SELF-TEST  ==================================== */

/**
 * @brief  Cicla Rojo → Verde → Azul dos veces (1.5 s por color = 9 s total).
 * @note   Bloqueante. El resultado lo confirma el usuario visualmente.
 *         LED queda apagado al finalizar.
 */
void LedRGB_Test(void);

#endif /* LED_RGB_H */
