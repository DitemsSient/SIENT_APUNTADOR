/**
 * @file    I2C1_Bus.c
 * @brief   Implementacion del mutex compartido del bus I2C1.
 *
 * @date    September 2, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#include "I2C1_Bus.h"
#include "cmsis_os2.h"
#include <stddef.h>

static osMutexId_t s_mutex = NULL;
static const osMutexAttr_t s_mutex_attr = { .name = "I2C1Mutex" };

void I2C1Bus_InitMutex(void)
{
    if (s_mutex == NULL) {
        s_mutex = osMutexNew(&s_mutex_attr);
    }
}

void I2C1Bus_Lock(void)
{
    if (s_mutex != NULL) {
        osMutexAcquire(s_mutex, osWaitForever);
    }
}

void I2C1Bus_Unlock(void)
{
    if (s_mutex != NULL) {
        osMutexRelease(s_mutex);
    }
}
