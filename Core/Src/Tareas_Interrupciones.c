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
#include "FreeRTOS.h"
#include "task.h"
#include "Logger.h"
#include "I2C1_Bus.h"
#include "Inicializacion.h"
#include "SensorLuz_TSL2571.h"
#include "Multiplexor_CD4051B.h"
#include "BatteryMonitor.h"
#include "Transmsion_Laser_IR.h"
#include "Bluetooth.h"
#include "Menu/Menu_Screens.h"
#include "Secuencias_LED.h"
#include "Display_Oled/Display_Comands.h"
#include "Display_Oled/Display_Fonts.h"
#include <string.h>
#include <stdio.h>

/**
 * @brief  Imprime el log del ultimo disparo del gatillo si quedo pendiente.
 * @note   La ISR (HAL_GPIO_EXTI_Callback, Transmision_Laser_IR.c) no puede
 *         llamar Log_Print directamente (usa mutex, no es ISR-safe) -- deja
 *         la bandera y quien la revisa (MenuTask o ExerciseTask, la que
 *         este activa) imprime aqui, ya en contexto de tarea seguro.
 */
static void gatillo_log_si_pendiente(void) {
    if (gatillo_disparo_pendiente_log) {
        gatillo_disparo_pendiente_log = false;
        if (laser_calibration_mode) {
            Log_Print("GATILLO", "Calibracion enviada (0xAA55)");
        } else {
            Log_Printf("GATILLO", "Disparo enviado (orden=%u lora=%u) -- balas restantes=%u",
                       g_exercise_data.orden, g_exercise_data.lora, g_exercise_data.ammo);
        }
    }
}

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

    /* Reporte de heap libre de FreeRTOS, ya con el scheduler corriendo --
     * util para dimensionar configTOTAL_HEAP_SIZE (FreeRTOSConfig.h) con
     * margen real en vez de adivinar. NUNCA mover esto a antes de
     * osKernelStart() (ver nota en Tareas_CrearTareas). */
    Log_Printf("RTOS", "Heap libre tras crear tareas: %u bytes (de %u)",
               (unsigned)xPortGetFreeHeapSize(), (unsigned)configTOTAL_HEAP_SIZE);

    for (;;) {
        Menu_Poll(&hmenu);
        Menu_Update(&hmenu);
        gatillo_log_si_pendiente();
        osDelay(MENU_TASK_PERIOD_MS);
    }
}

/* ===========================================================================
 *  LuzMuxTask -- ajusta la resistencia del driver de potencia del laser
 *  (Mux_Laser, CD4051B) segun la luz ambiental leida por el TSL2571.
 * ===========================================================================
 */

#define LUZ_MUX_TASK_PERIOD_MS   10000U

/* Umbrales de lux para los escalones intermedios (ajustables por prueba).
 * Rango ampliado a todo lo que reporta el sensor en esta tarjeta (0-4000
 * lux aprox). CONFIRMADO con datos reales: < 40 lux -> 810k. El resto de
 * la escalera (100/250/600/1500) es provisional, pendiente de afinar. */
#define LUZ_UMBRAL_1500_LUX      1500.0f
#define LUZ_UMBRAL_600_LUX        600.0f
#define LUZ_UMBRAL_250_LUX        250.0f
#define LUZ_UMBRAL_100_LUX        100.0f
#define LUZ_UMBRAL_40_LUX          40.0f  /* CONFIRMADO: < 40 lux -> 810k */

/* Tabla canal->resistencia, ordenada de MAYOR a MENOR potencia (resistencia
 * ascendente), segun medicion real en la tarjeta -- NO coincide con el
 * comentario generico de Multiplexor_CD4051B.h (ese es del proyecto de
 * referencia). MUX_CHANNEL_3 deberia ser 45k pero quedo soldada una de
 * 810k por error de fabricacion (ver Pendientes.md); se reutiliza aqui
 * como el escalon de "casi sin luz" ya que de facto es la resistencia
 * mas alta disponible en la tarjeta. */
typedef struct {
    mux_channel_t channel;
    uint32_t      resistencia_ohm;
} MuxLuzEntry_t;

static const MuxLuzEntry_t s_mux_luz_tabla[] = {
    { MUX_CHANNEL_7,  10000U },  /* ambos canales saturados (sol directo) */
    { MUX_CHANNEL_6,  15000U },  /* un canal saturado                     */
    { MUX_CHANNEL_5,  20000U },  /* lux >= 1500                           */
    { MUX_CHANNEL_4,  30000U },  /* lux >= 600                            */
    { MUX_CHANNEL_2,  51000U },  /* lux >= 250                            */
    { MUX_CHANNEL_1, 100000U },  /* lux >= 100                            */
    { MUX_CHANNEL_0, 150000U },  /* lux >= 40                             */
    { MUX_CHANNEL_3, 810000U },  /* lux < 40 -- deberia ser 45k, ver arriba */
};

static osThreadId_t s_luzMuxTaskHandle;
static const osThreadAttr_t s_luzMuxTask_attr = {
    .name       = "LuzMuxTask",
    .stack_size = 512U * 4U,
    .priority   = (osPriority_t)osPriorityNormal,
};

/**
 * @brief  Decide el indice de s_mux_luz_tabla segun saturacion y lux.
 */
static uint8_t LuzMux_SeleccionarIndice(bool ch0_sat, bool ch1_sat, float lux) {
    if (ch0_sat && ch1_sat)          { return 0U; }
    if (ch0_sat || ch1_sat)          { return 1U; }
    if (lux >= LUZ_UMBRAL_1500_LUX)  { return 2U; }
    if (lux >= LUZ_UMBRAL_600_LUX)   { return 3U; }
    if (lux >= LUZ_UMBRAL_250_LUX)   { return 4U; }
    if (lux >= LUZ_UMBRAL_100_LUX)   { return 5U; }
    if (lux >= LUZ_UMBRAL_40_LUX)    { return 6U; }
    return 7U;
}

/**
 * @brief  Lee el TSL2571, elige el canal del Mux, lee el nivel de bateria y
 *         actualiza g_exercise_data.lvBatery -- todo cada LUZ_MUX_TASK_PERIOD_MS.
 *         Bloqueante con osDelay, sin prisa. Se aprovecha este ciclo para la
 *         bateria y asi no crear una tarea aparte solo para eso.
 */
static void LuzMuxTask(void *argument) {
    (void)argument;

    for (;;) {
        float lux = 0.0f;
        TSL2571_RawData_t raw = {0};

        if (TSL2571_ReadLux(&SensorLuz, 1U, 50U, &lux, &raw) == HAL_OK) {
            const uint16_t maxCnt = TSL2571_MaxCount(SensorLuz.atime);
            bool ch0_sat = raw.ch0 >= (maxCnt - 2U);
            bool ch1_sat = raw.ch1 >= (maxCnt - 2U);

            uint8_t idx = LuzMux_SeleccionarIndice(ch0_sat, ch1_sat, lux);
            MUX_SelectChannel(&Mux_Laser, s_mux_luz_tabla[idx].channel);

            Log_Printf("MUXLUZ", "CH0=%u CH1=%u Lux=%.1f -> canal=%u R=%luOhm",
                       raw.ch0, raw.ch1, lux,
                       (unsigned)s_mux_luz_tabla[idx].channel,
                       (unsigned long)s_mux_luz_tabla[idx].resistencia_ohm);
        } else {
            Log_Print("MUXLUZ", "Error leyendo TSL2571");
        }

        /* Bateria: sin modulo conectado (banco de pruebas) is_ready sale en
         * 0 y se deja g_exercise_data.lvBatery como estaba (100 por defecto,
         * ver Inicializacion.c) en vez de pisarlo con basura. */
        BatGauge_Update(&Bateria);
        if (Bateria.is_ready) {
            g_exercise_data.lvBatery = (uint8_t)Bateria.soc_pct;
            Log_Printf("MUXLUZ", "Bateria=%u%%", (unsigned)Bateria.soc_pct);
        }

        osDelay(LUZ_MUX_TASK_PERIOD_MS);
    }
}

/* ===========================================================================
 *  ExerciseTask -- cuenta regresiva, alterna pantallas y controla el
 *  temporizador general del modo Ejercicio. Arrancada/reanudada por
 *  BluetoothTask al recibir $RUN\r; se suspende a si misma al terminar.
 * ===========================================================================
 */

#define EXERCISE_TASK_LOOP_MS       150U
#define EXERCISE_PANTALLA_MS        5000U
#define EXERCISE_REFRESH_STATS_MS   1000U

/* NOTA: g_exercise_data.tiempo se trata como SEGUNDOS mientras se prueba
 * en banco -- en el proyecto real es en MINUTOS. Cuando se confirme,
 * cambiar EXERCISE_TIEMPO_MULTIPLICADOR_MS de 1000U a 60000U.
 * Ver Pendientes.md. */
#define EXERCISE_TIEMPO_MULTIPLICADOR_MS   1000U

static osThreadId_t s_exerciseTaskHandle = NULL;
static const osThreadAttr_t s_exerciseTask_attr = {
    .name       = "ExerciseTask",
    .stack_size = 512U * 4U,
    .priority   = (osPriority_t)osPriorityNormal,
};

/* Declarada aqui (definida mas abajo, junto a ApuntadorUpdateTask) para que
 * Tareas_IniciarEjercicio() la pueda reanudar. */
static osThreadId_t s_apuntadorUpdateTaskHandle;

/* true mientras ExerciseTask esta corriendo un ejercicio; BluetoothTask lo
 * consulta para saber como reaccionar a un $DSCON. */
static volatile bool s_ejercicio_activo = false;

/* BluetoothTask la pone en true si llega $DSCON mientras s_ejercicio_activo;
 * ExerciseTask la revisa en cada vuelta de su loop corto y aborta. */
static volatile bool s_dscon_en_ejercicio = false;

static void ExerciseTask(void *argument) {
    (void)argument;

    for (;;) {
        Log_Printf("EJERCICIO", "Inicio -- duracion %lu s (tratado como segundos, ver nota)",
                   (unsigned long)g_exercise_data.tiempo);

        /* ---- Cuenta regresiva 10..1 ---- */
        bool abortado = false;
        for (uint8_t n = 10U; n >= 1U; n--) {
            gatillo_log_si_pendiente();
            if (s_dscon_en_ejercicio) { abortado = true; break; }

            char buf[5];
            snprintf(buf, sizeof(buf), "%u", n);

            ssd1306_clearDisplay();
            ssd1306_setTextSize(2U);
            ssd1306_setTextColor(WHITE);
            ssd1306_printCentered(buf, 8, &Font5x7);
            ssd1306_display();

            HAL_Delay(1000U);
        }

        if (!abortado) {
            ssd1306_clearDisplay();
            ssd1306_setTextSize(1U);
            ssd1306_setTextColor(WHITE);
            ssd1306_printCentered("!INICIA!", 12, &Font5x7);
            ssd1306_display();
            HAL_Delay(1000U);

            /* Recien aqui se puede disparar de verdad -- antes de esto el
             * gatillo esta bloqueado (ver HAL_GPIO_EXTI_Callback). */
            ejercicio_disparo_habilitado = true;

            /* ---- Ciclo principal: alterna pantallas cada 10s, revisa
             * tiempo total y DSCON en cada vuelta corta del loop. ---- */
            Log_Print("EJERCICIO", "Cuenta regresiva terminada, arrancando");

            uint32_t duracion_ms = g_exercise_data.tiempo * EXERCISE_TIEMPO_MULTIPLICADOR_MS;
            uint32_t tick_inicio_ejercicio = HAL_GetTick();
            uint32_t tick_inicio_pantalla  = HAL_GetTick();
            uint32_t tick_refresh_stats    = HAL_GetTick();
            bool     pantalla_stats        = true;

            Exercise_DrawStatsPage();

            for (;;) {
                osDelay(EXERCISE_TASK_LOOP_MS);

                gatillo_log_si_pendiente();

                if (s_dscon_en_ejercicio) { abortado = true; break; }

                if ((HAL_GetTick() - tick_inicio_ejercicio) >= duracion_ms) {
                    break;  /* fin normal, tiempo agotado */
                }

                if ((HAL_GetTick() - tick_inicio_pantalla) >= EXERCISE_PANTALLA_MS) {
                    tick_inicio_pantalla = HAL_GetTick();
                    pantalla_stats = !pantalla_stats;
                    if (pantalla_stats) {
                        Exercise_DrawStatsPage();
                        tick_refresh_stats = HAL_GetTick();
                    } else {
                        Exercise_DrawTeamPage();
                    }
                } else if (pantalla_stats &&
                           (HAL_GetTick() - tick_refresh_stats) >= EXERCISE_REFRESH_STATS_MS) {
                    /* Refresca balas/vidas/bateria cada 1s mientras esta
                     * visible -- las balas cambian con cada disparo del
                     * gatillo (ISR), y la pantalla de equipo/jugador no lo
                     * necesita porque esos datos nunca cambian en vivo. */
                    tick_refresh_stats = HAL_GetTick();
                    Exercise_DrawStatsPage();
                }
            }
        }

        /* ---- Fin del ejercicio ---- */
        ejercicio_disparo_habilitado = false;

        if (abortado) {
            Log_Print("EJERCICIO", "Terminado -- $DSCON recibido");

            ssd1306_clearDisplay();
            ssd1306_setTextSize(1U);
            ssd1306_setTextColor(WHITE);
            ssd1306_printCentered("Bluetooth",  6, &Font5x7);
            ssd1306_printCentered("Desconect.", 18, &Font5x7);
            ssd1306_display();

            SecuenciasLED_FinPorDesconexion();
        } else {
            Log_Print("EJERCICIO", "Terminado -- tiempo agotado");

            /* Solo tiene caso mandarlo aqui -- si abortamos por $DSCON ya
             * no hay a quien avisarle, el enlace se cayo. */
            static const uint8_t end_msg[] = "$END\r";
            Bt_Transmit(&Bluetooth, end_msg, sizeof(end_msg) - 1U);
            Log_Print("EJERCICIO", "END enviado por Bluetooth");

            ssd1306_clearDisplay();
            ssd1306_setTextSize(1U);
            ssd1306_setTextColor(WHITE);
            ssd1306_printCentered("EJERCICIO",  6, &Font5x7);
            ssd1306_printCentered("FINALIZADO", 18, &Font5x7);
            ssd1306_display();

            SecuenciasLED_FinEjercicio();
        }

        s_ejercicio_activo   = false;
        s_dscon_en_ejercicio = false;

        Menu_GoTo(&hmenu, SCREEN_MAIN_MENU);
        osThreadResume(s_menuTaskHandle);

        osThreadSuspend(s_exerciseTaskHandle);
        /* Al reanudar (osThreadResume desde BluetoothTask con el siguiente
         * $RUN), la ejecucion continua justo aqui y el for(;;) externo
         * vuelve a arrancar un ejercicio nuevo desde cero. */
    }
}

/**
 * @brief  Reanuda ExerciseTask (ya creada y suspendida desde Tareas_CrearTareas)
 *         y suspende MenuTask.
 * @note   Llamada por BluetoothTask al recibir $RUN\r, solo si no hay ya
 *         un ejercicio activo.
 */
static void Tareas_IniciarEjercicio(void) {
    s_ejercicio_activo          = true;
    s_dscon_en_ejercicio        = false;
    ejercicio_disparo_habilitado = false;  /* se prende al terminar la cuenta */

    /* El gatillo (HAL_GPIO_EXTI_Callback, Transmision_Laser_IR.c) solo
     * dispara si hmenu.screen == SCREEN_EXERCISE -- forzarlo aqui, ya que
     * ExerciseTask toma control de la pantalla sin pasar por Menu_GoTo(). */
    hmenu.screen = SCREEN_EXERCISE;

    /* Por si el $RUN llega justo mientras estabamos parados en Calibrar
     * (MenuTask se suspende ahi mismo, sin pasar por el OnButton que
     * normalmente lo apaga) -- sin esto, el gatillo seguiria mandando
     * calibracion sin descontar balas durante todo el ejercicio real. */
    laser_calibration_mode = false;

    osThreadSuspend(s_menuTaskHandle);
    osThreadResume(s_exerciseTaskHandle);
    osThreadResume(s_apuntadorUpdateTaskHandle);
}

/* ===========================================================================
 *  BluetoothTask -- vigila $DSCON y $RUN de forma global (cualquier
 *  pantalla, incluso con MenuTask suspendida durante el Ejercicio) y
 *  responde su ACK correspondiente. El resto de mensajes ($ACKCON,
 *  $NoCON, $*<datos>) se dejan intactos para que Menu_Bluetooth.c los
 *  siga consumiendo igual que antes.
 * ===========================================================================
 */

#define BT_TASK_PERIOD_MS   20U

static osThreadId_t s_btTaskHandle;
static const osThreadAttr_t s_btTask_attr = {
    .name       = "BluetoothTask",
    .stack_size = 512U * 4U,
    .priority   = (osPriority_t)osPriorityNormal,
};

static void bt_task_send_ack(const char *ack) {
    Bt_Transmit(&Bluetooth, (const uint8_t *)ack, (uint16_t)strlen(ack));
}

static void BluetoothTask(void *argument) {
    (void)argument;

    for (;;) {
        if (Bluetooth.rx_ready) {
            if (Bluetooth.rx_count >= 5U &&
                strncmp((char *)Bluetooth.rx_buffer, "DSCON", 5U) == 0) {

                Log_Print("BT-TASK", "DSCON recibido");
                Bt_ResetRx(&Bluetooth);
                bt_task_send_ack("$ACKDSCON\r");

                if (s_ejercicio_activo) {
                    s_dscon_en_ejercicio = true;  /* ExerciseTask reacciona */
                } else {
                    /* MenuTask sigue viva aqui -- se suspende brevemente
                     * para que ella y BluetoothTask no toquen el OLED al
                     * mismo tiempo (Menu_HandleDisconnect es bloqueante,
                     * ~3s). */
                    osThreadSuspend(s_menuTaskHandle);
                    Menu_HandleDisconnect(&hmenu);
                    osThreadResume(s_menuTaskHandle);
                }

            } else if (Bluetooth.rx_count >= 3U &&
                       strncmp((char *)Bluetooth.rx_buffer, "RUN", 3U) == 0) {

                Log_Print("BT-TASK", "RUN recibido");
                Bt_ResetRx(&Bluetooth);
                bt_task_send_ack("$ACKRUN\r");

                if (!s_ejercicio_activo) {
                    Tareas_IniciarEjercicio();
                } else {
                    Log_Print("BT-TASK", "RUN ignorado -- ejercicio ya activo");
                }
            }
            /* Cualquier otro mensaje ($ACKCON, $NoCON, $*<datos>) se deja
             * sin tocar -- lo consume Menu_Bluetooth.c como antes. */
        }

        osDelay(BT_TASK_PERIOD_MS);
    }
}

/* ===========================================================================
 *  ApuntadorUpdateTask -- manda a la otra tarjeta (Sensores) los datos que
 *  cambian de este lado: balas y % de bateria. Cada APUNTADOR_UPDATE_PERIOD_MS
 *  compara contra el ultimo valor mandado; si las balas cambiaron o la
 *  bateria se movio APUNTADOR_BATERIA_UMBRAL_PCT o mas (en cualquier
 *  direccion), manda $A_AP<balas>,<pct>\r. No espera $ACKA_AP todavia --
 *  se deja el nombre listo para cuando se implemente reintento (ver
 *  Pendientes.md). El giroscopio queda fuera por ahora, se agrega despues
 *  si hace falta.
 *  Solo corre durante el modo Ejercicio -- nace suspendida, la reanuda
 *  Tareas_IniciarEjercicio() junto con ExerciseTask, y se auto-suspende en
 *  cuanto s_ejercicio_activo se apaga (mismo ciclo de vida que ExerciseTask).
 * ===========================================================================
 */

#define APUNTADOR_UPDATE_PERIOD_MS      200U
#define APUNTADOR_BATERIA_UMBRAL_PCT      2U

static const osThreadAttr_t s_apuntadorUpdateTask_attr = {
    .name       = "ApuntadorUpdateTask",
    .stack_size = 512U * 4U,
    .priority   = (osPriority_t)osPriorityNormal,
};

static void ApuntadorUpdateTask(void *argument) {
    (void)argument;

    for (;;) {
        uint16_t ultimo_ammo    = g_exercise_data.ammo;
        uint8_t  ultima_bateria = g_exercise_data.lvBatery;
        bool     primera_vuelta = true;

        while (s_ejercicio_activo) {
            uint16_t ammo_actual    = g_exercise_data.ammo;
            uint8_t  bateria_actual = g_exercise_data.lvBatery;

            int16_t delta_bateria = (int16_t)bateria_actual - (int16_t)ultima_bateria;
            bool cambio_balas    = (ammo_actual != ultimo_ammo);
            bool cambio_bateria  = (delta_bateria >= (int16_t)APUNTADOR_BATERIA_UMBRAL_PCT) ||
                                   (delta_bateria <= -(int16_t)APUNTADOR_BATERIA_UMBRAL_PCT);

            if (primera_vuelta || cambio_balas || cambio_bateria) {
                char msg[32];
                int len = snprintf(msg, sizeof(msg), "$A_AP%u,%u\r",
                                   (unsigned)ammo_actual, (unsigned)bateria_actual);
                if (len > 0) {
                    Bt_Transmit(&Bluetooth, (uint8_t *)msg, (uint16_t)len);
                    Log_Printf("A_AP", "Enviado -- balas=%u bateria=%u%%",
                              (unsigned)ammo_actual, (unsigned)bateria_actual);
                }

                ultimo_ammo    = ammo_actual;
                ultima_bateria = bateria_actual;
                primera_vuelta = false;
            }

            osDelay(APUNTADOR_UPDATE_PERIOD_MS);
        }

        osThreadSuspend(s_apuntadorUpdateTaskHandle);
        /* Al reanudar (Tareas_IniciarEjercicio, con el siguiente $RUN), el
         * for(;;) externo reinicia la comparacion desde cero. */
    }
}

/* ================================  API  =================================== */

void Tareas_InicializarMutex(void) {
    Log_InitMutex();
    I2C1Bus_InitMutex();
}

void Tareas_CrearTareas(void) {
    /* NUNCA llamar Log_Print/Log_Printf aqui dentro -- esta funcion corre
     * despues de Tareas_InicializarMutex() (el mutex del Logger ya existe)
     * pero antes de osKernelStart(). Log_Print intentaria osMutexAcquire()
     * sin que el scheduler este corriendo todavia -- se queda colgado para
     * siempre (confirmado en pruebas, pantalla y todo se congela). El
     * reporte de heap libre se hace desde MenuTask ya con el scheduler
     * arriba, ver su primera vuelta mas abajo. */
    s_menuTaskHandle   = osThreadNew(MenuTask, NULL, &s_menuTask_attr);
    s_luzMuxTaskHandle = osThreadNew(LuzMuxTask, NULL, &s_luzMuxTask_attr);
    s_btTaskHandle     = osThreadNew(BluetoothTask, NULL, &s_btTask_attr);

    /* ExerciseTask y ApuntadorUpdateTask se crean desde ya (para que el
     * heap libre reportado por MenuTask ya incluya su stack), pero nacen
     * suspendidas -- no hacen nada hasta el primer $RUN (Tareas_IniciarEjercicio
     * las reanuda a ambas). Solo deben correr durante el modo Ejercicio. */
    s_exerciseTaskHandle = osThreadNew(ExerciseTask, NULL, &s_exerciseTask_attr);
    if (s_exerciseTaskHandle != NULL) {
        osThreadSuspend(s_exerciseTaskHandle);
    }

    s_apuntadorUpdateTaskHandle = osThreadNew(ApuntadorUpdateTask, NULL, &s_apuntadorUpdateTask_attr);
    if (s_apuntadorUpdateTaskHandle != NULL) {
        osThreadSuspend(s_apuntadorUpdateTaskHandle);
    }
}
