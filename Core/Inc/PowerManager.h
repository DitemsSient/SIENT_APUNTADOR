/**
 * @file    PowerManager.h
 * @brief   System-level low-power coordinator for all peripheral drivers.
 *
 * @details Provides two high-level functions to suspend or resume every
 *          peripheral that supports a low-power state:
 *
 *          - Flash MX25L6445E  : Deep Power-Down (~1 µA)
 *          - LSM6DSO32TR       : LSM6DSO32TR_PowerDown/PowerOn (ODR=0 → ~5 µA)
 *          - TSL2571           : ALS disabled (PON + AEN cleared)
 *          - SSD1306 OLED      : Display OFF command (0xAE)
 *          - BQ27441 fuel gauge: Hibernate mode
 *          - LED RGB           : All channels LOW
 *          - Buzzer            : PWM duty cycle 0
 *          - BL654 Bluetooth   : Auto-sleep (no command needed)
 *
 *          Handles for drivers that require a context struct must be
 *          registered once with PowerManager_Init() before calling
 *          Suspend or Wake.
 *
 *          STM32 low-power mode (Stop / Standby) is declared here but
 *          not yet implemented — see PowerManager_MCUSleep().
 *
 * @date    May 28, 2026
 * @author  César Pérez
 * @version 1.1.0
 */

#ifndef POWERMANAGER_H
#define POWERMANAGER_H

#include "stm32l4xx_hal.h"
#include "LSM6DSO32TR.h"
#include "SensorLuz_TSL2571.h"
#include <stdbool.h>

/* ========================  ENUMERATIONS  ================================= */

/* PowerManager return codes */

typedef enum {
    PM_OK               = 0,    /**< All operations succeeded                */
    PM_ERR_NOT_INIT     = 1,    /**< PowerManager_Init() was not called       */
    PM_ERR_PARTIAL      = 2,    /**< One or more drivers reported an error    */
    PM_ERR_PARAM        = 3     /**< NULL pointer passed to Init             */
} PM_Status_e;

/* ============================  STRUCTURES  ================================ */

/* Result detail from the last Suspend or Wake call */

typedef struct {
    bool flash_ok;      /**< Flash entered / exited power-down correctly     */
    bool imu_ok;        /**< LSM6DSO32TR entered / exited low-power correctly */
    bool light_ok;      /**< TSL2571 disabled / enabled correctly            */
    bool display_ok;    /**< SSD1306 turned off / on correctly               */
    bool gauge_ok;      /**< BQ27441 entered / exited hibernate correctly    */
} PM_Result_t;

/* ================================  API  =================================== */

/**
 * @brief  Registers the driver handles required by PowerManager.
 * @param  himu    Pointer to the initialised LSM6DSO32TR handle (LSM6DSO32TR_t).
 * @param  hlight  Pointer to the initialised TSL2571 handle (TSL2571_t).
 * @note   Call once after all driver Init() functions have succeeded.
 *         Flash, SSD1306, BQ27441, LedRGB and Buzzer use module-level
 *         handles and do not need to be passed here.
 */
PM_Status_e PowerManager_Init(LSM6DSO32TR_t   *himu,
                               TSL2571_t       *hlight);

/**
 * @brief  Puts every supported peripheral into its lowest-power state.
 * @param  result  Optional pointer to a PM_Result_t struct that receives
 *                 the per-driver outcome. Pass NULL to ignore.
 * @note   Actuators (LED, Buzzer) are turned off unconditionally.
 *         ICs (Flash, IMU, light sensor, display, gauge) are commanded
 *         via their respective low-power APIs.
 *         Returns PM_OK only if every driver succeeded.
 */
PM_Status_e PowerManager_SuspendAll(PM_Result_t *result);

/**
 * @brief  Restores every peripheral to normal operation.
 * @param  result  Optional pointer to a PM_Result_t struct that receives
 *                 the per-driver outcome. Pass NULL to ignore.
 * @note   Mirrors PowerManager_SuspendAll() in reverse order.
 *         Returns PM_OK only if every driver succeeded.
 */
PM_Status_e PowerManager_WakeAll(PM_Result_t *result);

/**
 * @brief  Sends the STM32 into a low-power mode after suspending peripherals.
 * @note   NOT YET IMPLEMENTED — deferred to a future sprint.
 *         Agreed approach: Stop mode with UART wakeup (EXTI on USART1 RX pin).
 *         Requires .ioc change before implementation.
 *         See PowerManager.c stub for full details and pending items.
 */
void PowerManager_MCUSleep(void);

#endif /* POWERMANAGER_H */
