/**
 * @file    PowerManager.c
 * @brief   Driver implementation for the system-level low-power coordinator.
 *
 * @date    May 28, 2026
 * @author  César Pérez
 * @version 1.1.0
 */

#include "PowerManager.h"
#include "Flash.h"
#include "LSM6DSO32TR.h"
#include "SensorLuz_TSL2571.h"
#include "Display_Oled/Display_Comands.h"
#include "BatteryMonitor.h"
#include "LedRGB.h"
#include "Buzzer.h"

/* ======================  STATIC VARIABLES  ================================ */

/* Registered driver handles */

static LSM6DSO32TR_t *pm_imu   = NULL;
static TSL2571_t     *pm_light = NULL;
static bool           pm_ready = false;

/* ================================  API  =================================== */

PM_Status_e PowerManager_Init(LSM6DSO32TR_t   *himu,
                               TSL2571_t       *hlight)
{
    if (himu == NULL || hlight == NULL) return PM_ERR_PARAM;

    pm_imu   = himu;
    pm_light = hlight;
    pm_ready = true;

    return PM_OK;
}

PM_Status_e PowerManager_SuspendAll(PM_Result_t *result)
{
    if (!pm_ready) return PM_ERR_NOT_INIT;

    PM_Result_t res = { true, true, true, true, true };
    bool all_ok = true;

    /* --- Actuators off (no error possible) --- */
    LedRGB_Off();
    Buzzer_Stop();

    /* --- ICs with low-power commands --- */

    res.flash_ok   = (Flash_PowerDown()                        == FLASH_OK);
    res.imu_ok     = (LSM6DSO32TR_PowerDown(pm_imu) == LSM_OK);
    res.light_ok   = (TSL2571_Disable(pm_light)                == HAL_OK);
    res.gauge_ok   = (BatGauge_Hibernate()                     == HAL_OK);

    /* Display off */
    ssd1306_sleep();

    /* BL654 Bluetooth enters auto-sleep when UART is idle — no command needed */

    if (!res.flash_ok || !res.imu_ok || !res.light_ok || !res.gauge_ok) {
        all_ok = false;
    }

    if (result != NULL) *result = res;
    return all_ok ? PM_OK : PM_ERR_PARTIAL;
}

PM_Status_e PowerManager_WakeAll(PM_Result_t *result)
{
    if (!pm_ready) return PM_ERR_NOT_INIT;

    PM_Result_t res = { true, true, true, true, true };
    bool all_ok = true;

    /* --- ICs: restore from low-power --- */

    res.flash_ok  = (Flash_WakeUp()               == FLASH_OK);
    res.gauge_ok  = (BatGauge_WakeUp()             == HAL_OK);
    res.imu_ok    = (LSM6DSO32TR_PowerOn(pm_imu) == LSM_OK);
    res.light_ok  = (TSL2571_Enable(pm_light)      == HAL_OK);

    /* Display back on */
    ssd1306_wakeup();

    /* BL654 Bluetooth wakes automatically when UART traffic resumes */

    if (!res.flash_ok || !res.imu_ok || !res.light_ok || !res.gauge_ok) {
        all_ok = false;
    }

    if (result != NULL) *result = res;
    return all_ok ? PM_OK : PM_ERR_PARTIAL;
}

void PowerManager_MCUSleep(void)
{
    /* NOT YET IMPLEMENTED — deferred to a future sprint.
     *
     * Agreed implementation approach: STM32 Stop mode with UART wakeup.
     * The MCU wakes when any byte arrives on huart1 (EXTI on the RX pin).
     *
     * Pending before implementing:
     *   - Open SIENT_APUNTADOR_DITEMS.ioc in CubeMX, configure the USART1 RX
     *     pin (PA10 / MCU_RX) as GPIO_EXTI10 with falling-edge detection and
     *     enable the EXTI15_10 interrupt in NVIC.
     *
     * Implementation steps once .ioc is updated:
     *   1. Call PowerManager_SuspendAll()
     *   2. Enable EXTI wakeup on USART1 RX pin
     *   3. HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFI)
     *   4. On return: call SystemClock_Config() to restore PLL
     *   5. Re-arm UART interrupt: HAL_UART_Receive_IT(...)
     *   6. Call PowerManager_WakeAll()
     */
}
