/**
 * @file    Test.h
 * @brief   Banco de pruebas de hardware de la tarjeta Mira — una función por
 *          módulo, cada una llamable de forma independiente desde main().
 *
 * @details Concentra todo el código de prueba que se usó para validar cada
 *          driver en hardware real (ver Core/Doc/Pruebas_HW.md para el
 *          detalle de qué se confirmó y cómo). Este archivo NO forma parte
 *          del firmware final del juego — es un banco de pruebas manual que
 *          se deja disponible para volver a diagnosticar un componente
 *          individual sin tener que rearmar la prueba desde cero.
 *
 *          Uso: incluir este header en main.c e invocar la función Test_xxx()
 *          del módulo que se quiera probar dentro de USER CODE 2 / WHILE.
 *          Cada función es bloqueante y reporta resultados por Logger (USB
 *          CDC) — abrir una terminal serial para ver la salida.
 *
 *          Todos los handles de driver que antes vivían en main.c (tsl, mux,
 *          hbt, lsm, laser_calibration_mode) ahora se definen en Test.c.
 *
 * @date    August 27, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#ifndef TEST_H
#define TEST_H

#include "stm32l4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ================================  API  =================================== */

/* ===========================================================================
 *  PRUEBA 1 — UART (Logger) + Modo Programación
 *  Incluye: "Logger.h", "ModoProgramacion.h"
 * ===========================================================================
 */

/**
 * @brief  Fija el mux de programación en modo MCU o BT y manda mensajes de
 *         prueba por Log_Print() cada segundo, 10 veces.
 * @param  modo_bt  true = fuerza modo Bluetooth, false = modo MCU (default).
 */
void Test_UART_ModoProgramacion(bool modo_bt);

/* ===========================================================================
 *  PRUEBA 2 — LedRGB
 *  Incluye: "LedRGB.h"
 * ===========================================================================
 */

/**
 * @brief  Corre el self-test del driver (ciclo R->G->B, 2 veces).
 */
void Test_LedRGB(void);

/* ===========================================================================
 *  PRUEBA 3 — Buzzer
 *  Incluye: "Buzzer_Melodias.h"
 * ===========================================================================
 */

/**
 * @brief  Toca la melodía "ode_to_joy" completa una vez (TIM2 CH2 / PB3).
 */
void Test_Buzzer(void);

/* ===========================================================================
 *  PRUEBA 4/15 — Transmision_Laser_IR (CONFIRMADO: alcance >= 40 m)
 *  Incluye: "Transmsion_Laser_IR.h", "Multiplexor_CD4051B.h"
 * ===========================================================================
 */

/**
 * @brief  Arranca TIM1 (delay us) y TIM2 CH3 (portadora), fija el mux en el
 *         canal 6 (potencia IR) y transmite una trama de prueba (0xA5) cada
 *         segundo, N veces.
 * @param  n_disparos  Número de tramas a transmitir.
 */
void Test_LaserIR(uint8_t n_disparos);

/* ===========================================================================
 *  PRUEBA 5 — SensorHall (Gatillo)
 *  Incluye: "SensorHall.h"
 * ===========================================================================
 */

/**
 * @brief  Corre el self-test del driver y además hace polling de
 *         HallSensor_IsPressed() por unos segundos, reportando cambios.
 */
void Test_SensorHall(void);

/* ===========================================================================
 *  PRUEBA 6 — Botones A / B
 *  Sin driver dedicado — lectura directa por HAL_GPIO_ReadPin, requiere
 *  Incluye: "LedRGB.h"
 * ===========================================================================
 */

/**
 * @brief  Hace polling de BOTON_A (PB11) / BOTON_B (PB10) durante
 *         duracion_ms: rojo si A presionado, azul si B, apagado si ninguno.
 * @param  duracion_ms  Duración total de la prueba en milisegundos.
 */
void Test_Botones(uint32_t duracion_ms);

/* ===========================================================================
 *  PRUEBA 7 — Bluetooth (BL654) - AT
 *  Incluye: "Bluetooth.h"
 *  NOTA: bloqueada mientras el módulo no tenga firmware cargado.
 * ===========================================================================
 */

/**
 * @brief  Corre el self-test del driver (Bt_Test: manda "AT\r\n", espera "OK").
 */
void Test_Bluetooth(void);

/* ===========================================================================
 *  PRUEBA 8 — Flash (MX25L6445E)
 *  Incluye: "Flash.h"
 * ===========================================================================
 */

/**
 * @brief  Corre el self-test del driver (Flash_Test: escribe/lee/compara en
 *         el último sector).
 */
void Test_Flash(void);

/* ===========================================================================
 *  PRUEBA 9 — LSM6DSO32TR (IMU accel+gyro, chip real LSM6DS3)
 *  Incluye: "LSM6DSO32TR.h"
 * ===========================================================================
 */

/**
 * @brief  Inicializa el IMU y lee accel/gyro/temperatura N veces, imprimiendo
 *         cada muestra por Logger.
 * @param  n_muestras  Número de lecturas a tomar (1 s entre cada una).
 */
void Test_IMU(uint8_t n_muestras);

/* ===========================================================================
 *  PRUEBA 10 — MMC5983MA (Magnetómetro)
 *  Incluye: "MMC5983MA.h"
 * ===========================================================================
 */

/**
 * @brief  Inicializa el magnetómetro y lee X/Y/Z en uT N veces.
 * @param  n_muestras  Número de lecturas a tomar (1 s entre cada una).
 */
void Test_Magnetometro(uint8_t n_muestras);

/* ===========================================================================
 *  PRUEBA 11 — SensorLuz_TSL2571
 *  Incluye: "SensorLuz_TSL2571.h"
 * ===========================================================================
 */

/**
 * @brief  Inicializa el sensor de luz y lee CH0/CH1/Lux N veces.
 * @param  n_muestras  Número de lecturas a tomar (1 s entre cada una).
 */
void Test_SensorLuz(uint8_t n_muestras);

/* ===========================================================================
 *  PRUEBA 12 — BatteryMonitor (BQ27441)
 *  Incluye: "BatteryMonitor.h"
 * ===========================================================================
 */

/**
 * @brief  Inicializa el fuel gauge y reporta voltaje/corriente/SOC/SOH N
 *         veces (3 s entre cada una).
 * @param  n_muestras  Número de lecturas a tomar.
 */
void Test_BatteryMonitor(uint8_t n_muestras);

/* ===========================================================================
 *  PRUEBA 13 — Display OLED (SSD1306)
 *  Incluye: "Display_Oled/Display_Comands.h"
 * ===========================================================================
 */

/**
 * @brief  Inicializa el display y muestra un contador centrado 1->10.
 */
void Test_DisplayOled(void);

/* ===========================================================================
 *  PRUEBA 14 — Multiplexor_CD4051B
 *  Incluye: "Multiplexor_CD4051B.h"
 * ===========================================================================
 */

/**
 * @brief  Cicla los 8 canales del mux, uno cada segundo, imprimiendo el
 *         canal activo por Logger (confirmar con multímetro en el canal
 *         común).
 */
void Test_Multiplexor(void);

/* ===========================================================================
 *  PRUEBA 17 — Escaneo de bus I2C1
 *  Sin includes adicionales — usa hi2c1 directo.
 * ===========================================================================
 */

/**
 * @brief  Recorre direcciones 1-126 con HAL_I2C_IsDeviceReady() y reporta
 *         cuáles responden ACK. Dispositivos esperados: 0x30 (MMC5983MA),
 *         0x39 (TSL2571), 0x55 (BQ27441), 0x6A (LSM6DSO32TR), 0x3C (SSD1306).
 */
void Test_I2CScan(void);

/* ===========================================================================
 *  PRUEBA 18 — Programar el BT
 *  Incluye: "ModoProgramacion.h", "LedRGB.h"
 *  NOTA: no retorna — modo manual para que otra persona cargue firmware al
 *  BL654 mientras el LED rojo parpadea como testigo de que la tarjeta vive.
 * ===========================================================================
 */

/**
 * @brief  Fija el mux en modo BT y parpadea el LED en rojo cada 2 s
 *         indefinidamente. Bloqueante para siempre — última prueba a llamar.
 */
void Test_ProgramarBT(void);

#endif /* TEST_H */
