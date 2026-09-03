/**
 * @file    I2C1_Bus.h
 * @brief   Mutex compartido para el bus I2C1, usado por todos los drivers
 *          que lo comparten (Display, TSL2571, IMU, Magnetometro, BatteryMonitor).
 *
 * @details Sin esto, dos tareas RTOS distintas pueden intentar una
 *          transaccion I2C1 al mismo tiempo (ej. MenuTask dibujando en el
 *          OLED mientras LuzMuxTask lee el TSL2571) y una de las dos falla
 *          porque el driver HAL de I2C no es reentrante entre tareas.
 *          Mismo patron que el mutex del Logger (Log_InitMutex()).
 *
 * @date    September 2, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#ifndef I2C1_BUS_H
#define I2C1_BUS_H

/* ================================  API  =================================== */

/**
 * @brief  Crea el mutex compartido del bus I2C1.
 * @note   Llamar una sola vez, junto con Log_InitMutex() en
 *         Tareas_InicializarMutex() (despues de osKernelInitialize(),
 *         antes de osKernelStart() -- crear el mutex ahi es seguro, lo que
 *         NO es seguro es *usarlo* -- I2C1Bus_Lock()/Unlock() -- antes de
 *         que el scheduler arranque).
 */
void I2C1Bus_InitMutex(void);

/**
 * @brief  Toma el mutex del bus I2C1 (bloqueante). Llamar antes de
 *         cualquier transaccion I2C1.
 */
void I2C1Bus_Lock(void);

/**
 * @brief  Libera el mutex del bus I2C1.
 */
void I2C1Bus_Unlock(void);

#endif /* I2C1_BUS_H */
