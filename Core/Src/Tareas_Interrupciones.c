/**
 * @file    Tareas_Interrupciones.c
 * @brief   Implementacion de tareas RTOS e interrupciones del firmware real.
 *
 * @date    August 28, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#include "Tareas_Interrupciones.h"
#include "cmsis_os2.h"
#include "Logger.h"
#include "Inicializacion.h"

/* ===========================================================================
 *  MenuTask
 * ===========================================================================
 */

#define MENU_TASK_PERIOD_MS   20U

static osThreadId_t s_menuTaskHandle;
static const osThreadAttr_t s_menuTask_attr = {
    .name       = "MenuTask",
    .stack_size = 512U * 4U,
    .priority   = (osPriority_t)osPriorityNormal,
};

/**
 * @brief  Poll + update del menu, sin bloqueos, cada MENU_TASK_PERIOD_MS.
 */
static void MenuTask(void *argument) {
    (void)argument;

    for (;;) {
        Menu_Poll(&hmenu);
        Menu_Update(&hmenu);
        osDelay(MENU_TASK_PERIOD_MS);
    }
}

/* ================================  API  =================================== */

void Tareas_InicializarMutex(void) {
    Log_InitMutex();
}

void Tareas_CrearTareas(void) {
    s_menuTaskHandle = osThreadNew(MenuTask, NULL, &s_menuTask_attr);
}
