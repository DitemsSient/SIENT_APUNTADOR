/**
 * @file    Tareas_Interrupciones.h
 * @brief   Tareas RTOS e interrupciones del firmware real.
 *
 * @details Punto central para las tareas de FreeRTOS (CMSIS-RTOS v2) y los
 *          callbacks de interrupcion que ya estan validados y en uso.
 *
 *          Orden de uso desde main.c:
 *          1. osKernelInitialize()
 *          2. Tareas_InicializarMutex()  -- USER CODE BEGIN RTOS_MUTEX
 *          3. Tareas_CrearTareas()       -- USER CODE BEGIN RTOS_THREADS
 *          4. osKernelStart()
 *
 * @date    August 28, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#ifndef TAREAS_INTERRUPCIONES_H
#define TAREAS_INTERRUPCIONES_H

/* ================================  API  =================================== */

/**
 * @brief  Crea los mutex compartidos entre tareas (por ahora, el del Logger).
 * @note   Llamar despues de osKernelInitialize(), antes de osKernelStart().
 */
void Tareas_InicializarMutex(void);

/**
 * @brief  Crea todas las tareas RTOS del firmware.
 * @note   Llamar despues de Tareas_InicializarMutex(), antes de osKernelStart().
 */
void Tareas_CrearTareas(void);

#endif /* TAREAS_INTERRUPCIONES_H */
