/**
 * @file    Inicializacion.h
 * @brief   Secuencia de arranque del firmware real: inicializa todos los
 *          periféricos/drivers de la tarjeta Mira, en orden, un módulo a
 *          la vez.
 *
 * @details Punto único de entrada para todo lo que main() necesita antes de
 *          arrancar el juego. Cada módulo se activa/desactiva con su propio
 *          INIT_xxx_ENABLE (1U/0U) — al quedar en 0U, tanto su #include como
 *          su llamada de init se excluyen del build (ahorro real de flash/
 *          RAM, no solo un "if" en tiempo de ejecución).
 *
 *          Orden de arranque:
 *          1. Bootloader — SIEMPRE primero. Si el botón está presionado,
 *             Inicializacion_Run() nunca regresa (salta al DFU de fábrica).
 *          2. USB CDC + Logger — para poder ver mensajes de las siguientes
 *             etapas de inicialización.
 *          3. Resto de periféricos (se van agregando aquí conforme se
 *             integran; por ahora todos deshabilitados).
 *
 * @date    August 27, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#ifndef INICIALIZACION_H
#define INICIALIZACION_H

#include "stm32l4xx_hal.h"

/* ========================  CONFIGURATION  ================================= */

/* Bootloader + USB CDC + Logger — infraestructura base, siempre activa.
 * No se apagan con un flag porque sin ellas no hay forma de ver nada del
 * resto de la inicialización. */

#define INIT_BOOTLOADER_ENABLE      1U
#define INIT_USB_LOGGER_ENABLE      1U

/* Resto de periféricos/drivers — 1U para incluir su init en el build,
 * 0U para excluirlo por completo (ahorra flash/RAM). Se van prendiendo
 * conforme se integran a esta librería. */

#define INIT_MODOPROGRAMACION_ENABLE 1U
#define INIT_MULTIPLEXOR_ENABLE      1U
#define INIT_FLASH_ENABLE            1U
#define INIT_BUZZER_ENABLE           1U  /* Reactivado 30-sep-2026: Buzzer se movio a TIM15 CH2/PA3 (timer propio, ya no comparte ARR con el laser en TIM2) -- ver Pendientes.md */
#define INIT_BLUETOOTH_ENABLE        1U
#define INIT_IMU_ENABLE              1U
#define INIT_MAGNETOMETRO_ENABLE     1U
#define INIT_SENSORLUZ_ENABLE        1U
#define INIT_BATTERYMONITOR_ENABLE   1U
#define INIT_LASERIR_ENABLE          1U
#define INIT_SENSORHALL_ENABLE       1U
#define INIT_DISPLAY_ENABLE          1U

/* ========================  GLOBAL STATE  =================================== */

#if INIT_MULTIPLEXOR_ENABLE
#include "Multiplexor_CD4051B.h"
extern mux_handle_t Mux_Laser;
#endif

#if INIT_SENSORLUZ_ENABLE
#include "SensorLuz_TSL2571.h"
extern TSL2571_t SensorLuz;
extern TSL2571_RawData_t SensorLuz_UltimaLectura;
extern float SensorLuz_UltimoLux;
#endif

#if INIT_IMU_ENABLE
#include "LSM6DSO32TR.h"
extern LSM6DSO32TR_t Imu;
extern LSM_Data_t Imu_UltimaLectura;
#endif

#if INIT_MAGNETOMETRO_ENABLE
#include "MMC5983MA.h"
extern MMC_Data_t Magnetometro_UltimaLectura;
#endif

/* ========================  MENU / ESTADO DE JUEGO  ========================= */

#include "Menu/Menu.h"
extern Menu_Handle_t hmenu;

#include "HWTest_Status.h"
extern HWTest_Status_t hw_status;

#if INIT_BLUETOOTH_ENABLE
#include "Bluetooth.h"
extern Bt_Handle_t Bluetooth;
#endif

#if INIT_BATTERYMONITOR_ENABLE
#include "BatteryMonitor.h"
extern BatGauge_Data_t Bateria;
#endif

/* ========================  DIAGNOSTICO  ===================================== */

/* Un bool por componente: true si su init/self-test paso en el arranque.
 * Queda en false (default) si el componente esta deshabilitado por su
 * INIT_xxx_ENABLE o si fallo. i2c_completo: true solo si TODOS los
 * dispositivos I2C esperados respondieron en el escaneo. */

#include <stdbool.h>

typedef struct {
    bool modoprogramacion;
    bool multiplexor;
    bool flash;
    bool buzzer;
    bool bluetooth;
    bool imu;
    bool magnetometro;
    bool sensorluz;
    bool batterymonitor;
    bool laserir;
    bool sensorhall;
    bool display;
    bool i2c_completo;
} Diagnostico_t;

extern Diagnostico_t Diagnostico;

/* ================================  API  =================================== */

/**
 * @brief  Corre la secuencia de arranque completa, en orden, según los
 *         INIT_xxx_ENABLE definidos arriba.
 * @note   Llamar una sola vez en main(), dentro de USER CODE 2, antes de
 *         cualquier otro código. Si el botón de bootloader está presionado,
 *         esta función no regresa.
 */
void Inicializacion_Run(void);

/**
 * @brief  Imprime el mensaje inicial por Logger (título del proyecto +
 *         confirmación de que el USB/Logger quedó configurado).
 * @note   Se llama una vez dentro de Inicializacion_Run(); expuesta también
 *         para poder reimprimirla periódicamente desde main() mientras se
 *         diagnostica si el terminal USB está enumerando a tiempo.
 */
void Inicializacion_PrintBanner(void);

/**
 * @brief  Imprime por Logger el estado de Diagnostico completo (un OK/FALLO
 *         por componente). Util como snapshot rapido de que sigue vivo.
 */
void Inicializacion_PrintDiagnostico(void);

/**
 * @brief  Imprime por Logger el contenido actual de g_exercise_data.
 * @note   Llamar tras parsear un frame de datos de juego valido ($*...\r).
 */
void Inicializacion_PrintExerciseData(void);

#endif /* INICIALIZACION_H */
