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
 * @note   La ISR (HAL_GPIO_EXTI_Callback, Transmision_Laser_IR.c) sigue sin
 *         llamar Log_Print directamente -- deja la bandera y quien la
 *         revisa (MenuTask o ExerciseTask, la que este activa) imprime
 *         aqui, ya en contexto de tarea. Nota (7-sep-2026): con el Logger
 *         migrado a cola, Log_Print() ya seria tecnicamente ISR-safe
 *         (osMessageQueuePut con timeout 0 esta permitido desde ISR por el
 *         estandar CMSIS-RTOS2) -- se deja la bandera diferida de todos
 *         modos, funciona bien y no hay necesidad de tocar codigo de la
 *         ISR de maxima prioridad sin una razon concreta.
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
 *  LoggerTask -- consume la cola del Logger y hace la transmision USB CDC
 *  real (bloqueante). Cuerpo publico Log_Task() vive en Logger.c; aqui solo
 *  se crea el hilo, igual que las demas tareas. Ver Logger.h para el porque
 *  del cambio de mutex a cola (7-sep-2026).
 * ===========================================================================
 */

static osThreadId_t s_loggerTaskHandle;
static const osThreadAttr_t s_loggerTask_attr = {
    .name       = "LoggerTask",
    .stack_size = 512U * 4U,
    .priority   = (osPriority_t)osPriorityNormal,
};

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

/* Pausa COOPERATIVA de MenuTask -- NUNCA usar osThreadSuspend(s_menuTaskHandle)
 * desde otra tarea directamente. Si MenuTask esta a mitad de una transaccion
 * I2C (con I2C1Bus_Lock() tomado) justo cuando la suspenden de golpe desde
 * afuera, se queda congelada SIN soltar el mutex -- cualquier otra tarea que
 * despues necesite I2C1 (ExerciseTask dibujando, LuzMuxTask leyendo) se
 * queda esperando ese mutex para siempre. Confirmado en pruebas (7-sep-2026):
 * un $RUN llegando a media escritura del OLED congelaba todo el sistema.
 * En vez de eso, se le PIDE que se suspenda sola en el punto seguro de su
 * propio loop (nunca a medio mutex). */
static volatile bool s_menuTask_pausar  = false;
static volatile bool s_menuTask_pausada = false;

/**
 * @brief  Pide que MenuTask se pause y espera (con timeout) su confirmacion.
 * @return true si confirmo la pausa a tiempo, false si se agoto el timeout
 *         (se sigue de todos modos -- ver nota de uso en cada caller).
 */
static bool MenuTask_PausarYEsperar(uint32_t timeout_ms) {
    s_menuTask_pausar = true;
    uint32_t start = HAL_GetTick();
    while (!s_menuTask_pausada) {
        if ((HAL_GetTick() - start) > timeout_ms) {
            return false;
        }
        osDelay(2U);
    }
    return true;
}

/**
 * @brief  Reanuda MenuTask (previamente pausada con MenuTask_PausarYEsperar).
 */
static void MenuTask_Reanudar(void) {
    s_menuTask_pausar = false;
    osThreadResume(s_menuTaskHandle);
}

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
        if (s_menuTask_pausar) {
            s_menuTask_pausada = true;
            osThreadSuspend(s_menuTaskHandle);  /* se suspende A SI MISMA */
            s_menuTask_pausada = false;         /* al reanudar, limpia */
            continue;
        }

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

#define LUZ_MUX_TASK_PERIOD_MS   20000U

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

/* Idem -- BluetoothTask necesita bajarla al ver el ACK correspondiente, y
 * esta arriba de la seccion de ApuntadorUpdateTask en el archivo.
 *
 * Enum GENERICO para "que ACK estamos esperando" -- un solo valor pendiente
 * a la vez en todo el sistema (no soporta esperar dos acks distintos al
 * mismo tiempo desde dos tareas distintas; hoy no hace falta porque los
 * envios son secuenciales). Agregar un caso nuevo (ej. ACK_END) es solo
 * sumar un valor aqui + su rama en BluetoothTask. */
typedef enum {
    ACK_NINGUNO = 0,  /* nada pendiente */
    ACK_A_AP,         /* esperando $ACKA_AP -- ApuntadorUpdateTask */
} AckEstado_e;

/* La pone quien mande el mensaje que necesita ACK; la baja BluetoothTask
 * al ver la respuesta correspondiente (mismo patron que s_dscon_en_ejercicio). */
static volatile AckEstado_e s_ack_pendiente = ACK_NINGUNO;

/* true mientras ExerciseTask esta corriendo un ejercicio; BluetoothTask lo
 * consulta para saber como reaccionar a un $DSCON. */
static volatile bool s_ejercicio_activo = false;

/* BluetoothTask la pone en true si llega $DSCON mientras s_ejercicio_activo;
 * ExerciseTask la revisa en cada vuelta de su loop corto y aborta. */
static volatile bool s_dscon_en_ejercicio = false;

/* Idem, para $END_S -- el encargado del juego (Sensores) puede detener el
 * ejercicio en cualquier momento para cualquier jugador. */
static volatile bool s_end_admin_en_ejercicio = false;

/** @brief Motivo por el que ExerciseTask salio de su ciclo principal. */
typedef enum {
    EXFIN_NORMAL = 0,  /* tiempo agotado, fin normal */
    EXFIN_DSCON,        /* $DSCON recibido */
    EXFIN_ADMIN,         /* $END_S recibido */
} ExercicioFinMotivo_e;

static void ExerciseTask(void *argument) {
    (void)argument;

    for (;;) {
        Log_Printf("EJERCICIO", "Inicio -- duracion %lu s (tratado como segundos, ver nota)",
                   (unsigned long)g_exercise_data.tiempo);

        /* ---- Cuenta regresiva 10..1 ---- */
        ExercicioFinMotivo_e motivo = EXFIN_NORMAL;
        for (uint8_t n = 10U; n >= 1U; n--) {
            gatillo_log_si_pendiente();
            if (s_dscon_en_ejercicio)     { motivo = EXFIN_DSCON; break; }
            if (s_end_admin_en_ejercicio) { motivo = EXFIN_ADMIN; break; }

            char buf[5];
            snprintf(buf, sizeof(buf), "%u", n);

            ssd1306_clearDisplay();
            ssd1306_setTextSize(2U);
            ssd1306_setTextColor(WHITE);
            ssd1306_printCentered(buf, 8, &Font5x7);
            ssd1306_display();

            HAL_Delay(1000U);
        }

        if (motivo == EXFIN_NORMAL) {
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

                if (s_dscon_en_ejercicio)     { motivo = EXFIN_DSCON; break; }
                if (s_end_admin_en_ejercicio) { motivo = EXFIN_ADMIN; break; }

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

        if (motivo == EXFIN_DSCON) {
            Log_Print("EJERCICIO", "Terminado -- $DSCON recibido");

            ssd1306_clearDisplay();
            ssd1306_setTextSize(1U);
            ssd1306_setTextColor(WHITE);
            ssd1306_printCentered("Bluetooth",  6, &Font5x7);
            ssd1306_printCentered("Desconect.", 18, &Font5x7);
            ssd1306_display();

            SecuenciasLED_FinPorDesconexion();
        } else if (motivo == EXFIN_ADMIN) {
            Log_Print("EJERCICIO", "Terminado -- $END_S recibido (parado por admin)");

            ssd1306_clearDisplay();
            ssd1306_setTextSize(1U);
            ssd1306_setTextColor(WHITE);
            ssd1306_printCentered("FINALIZADO", 6, &Font5x7);
            ssd1306_printCentered("POR ADMIN", 18, &Font5x7);
            ssd1306_display();

            SecuenciasLED_FinPorAdmin();
            osDelay(5000U);
        } else {
            Log_Print("EJERCICIO", "Terminado -- tiempo agotado");

            /* Solo tiene caso mandarlo aqui -- si abortamos por $DSCON/$END_S
             * ya no hay a quien avisarle (o ya nos avisaron ellos). */
            static const uint8_t end_msg[] = "$END_A\r";
            Bt_Transmit(&Bluetooth, end_msg, sizeof(end_msg) - 1U);
            Log_Print("EJERCICIO", "END_A enviado por Bluetooth");

            ssd1306_clearDisplay();
            ssd1306_setTextSize(1U);
            ssd1306_setTextColor(WHITE);
            ssd1306_printCentered("EJERCICIO",  6, &Font5x7);
            ssd1306_printCentered("FINALIZADO", 18, &Font5x7);
            ssd1306_display();

            SecuenciasLED_FinEjercicio();
        }

        s_ejercicio_activo       = false;
        s_dscon_en_ejercicio     = false;
        s_end_admin_en_ejercicio = false;

        Menu_GoTo(&hmenu, SCREEN_MAIN_MENU);
        MenuTask_Reanudar();

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
    s_end_admin_en_ejercicio    = false;
    ejercicio_disparo_habilitado = false;  /* se prende al terminar la cuenta */

    /* Pausa MenuTask de forma segura (nunca a medio mutex I2C) ANTES de
     * tocar hmenu -- asi tambien evitamos que MenuTask lea hmenu.screen a
     * medias mientras lo estamos cambiando desde esta tarea. */
    (void)MenuTask_PausarYEsperar(100U);

    /* El gatillo (HAL_GPIO_EXTI_Callback, Transmision_Laser_IR.c) solo
     * dispara si hmenu.screen == SCREEN_EXERCISE -- forzarlo aqui, ya que
     * ExerciseTask toma control de la pantalla sin pasar por Menu_GoTo(). */
    hmenu.screen = SCREEN_EXERCISE;

    /* Por si el $RUN llega justo mientras estabamos parados en Calibrar
     * (MenuTask se pauso ahi mismo, sin pasar por el OnButton que
     * normalmente lo apaga) -- sin esto, el gatillo seguiria mandando
     * calibracion sin descontar balas durante todo el ejercicio real. */
    laser_calibration_mode = false;

    osThreadResume(s_exerciseTaskHandle);
    osThreadResume(s_apuntadorUpdateTaskHandle);
}

/* ===========================================================================
 *  BluetoothTask -- vigila $DSCON, $END_S y $RUN de forma global (cualquier
 *  pantalla, incluso con MenuTask suspendida durante el Ejercicio) y
 *  responde su ACK correspondiente. El resto de mensajes ($ACKCON,
 *  $NoCON, $CONF<datos>) se dejan intactos para que Menu_Bluetooth.c los
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
                    /* MenuTask sigue viva aqui -- se pausa de forma segura
                     * (nunca a medio mutex I2C) para que ella y BluetoothTask
                     * no toquen el OLED al mismo tiempo (Menu_HandleDisconnect
                     * es bloqueante, ~3s). */
                    (void)MenuTask_PausarYEsperar(100U);
                    Menu_HandleDisconnect(&hmenu);
                    MenuTask_Reanudar();
                }

            } else if (Bluetooth.rx_count >= 5U &&
                       strncmp((char *)Bluetooth.rx_buffer, "END_S", 5U) == 0) {

                /* $END_S -- el encargado del juego para el ejercicio para
                 * cualquier jugador, en cualquier momento. Mismo patron que
                 * $DSCON: si hay ejercicio activo, ExerciseTask reacciona;
                 * si no, no hay nada que detener. */
                Log_Print("BT-TASK", "END_S recibido");
                Bt_ResetRx(&Bluetooth);
                bt_task_send_ack("$ACKEND_S\r");

                if (s_ejercicio_activo) {
                    s_end_admin_en_ejercicio = true;  /* ExerciseTask reacciona */
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

            } else if (Bluetooth.rx_count >= 7U &&
                       strncmp((char *)Bluetooth.rx_buffer, "ACKA_AP", 7U) == 0) {

                Bt_ResetRx(&Bluetooth);
                if (s_ack_pendiente == ACK_A_AP) {
                    s_ack_pendiente = ACK_NINGUNO;
                }
                /* No se manda ACK de vuelta -- este mensaje YA ES un ACK,
                 * ApuntadorUpdateTask lo esta esperando (ver mas abajo). */
            }
            /* Agregar un caso nuevo de ACK (ej. $ACKEND) aqui mismo: otro
             * "else if" que revise s_ack_pendiente == ACK_END antes de
             * bajarla, mismo patron que arriba. */
            /* Cualquier otro mensaje ($ACKCON, $NoCON, $CONF<datos>) se deja
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
 *  direccion), manda $A_AP<balas>,<pct>\r y espera $ACKA_AP\r hasta
 *  A_AP_ACK_TIMEOUT_MS, con hasta A_AP_MAX_REINTENTOS reintentos (7-sep-2026).
 *  Si llega $DSCON a media espera, se aborta de inmediato -- sin conexion no
 *  hay a quien reintentarle. Si algo cambia mientras se espera un ACK, ese
 *  dato intermedio se pierde a proposito: el siguiente intento/ciclo manda
 *  siempre el valor MAS RECIENTE, nunca se encola nada.
 *  El giroscopio queda fuera del mensaje por ahora, se agrega despues si
 *  hace falta.
 *  Solo corre durante el modo Ejercicio -- nace suspendida, la reanuda
 *  Tareas_IniciarEjercicio() junto con ExerciseTask, y se auto-suspende en
 *  cuanto s_ejercicio_activo se apaga (mismo ciclo de vida que ExerciseTask).
 * ===========================================================================
 */

#define APUNTADOR_UPDATE_PERIOD_MS      200U
#define APUNTADOR_BATERIA_UMBRAL_PCT      2U
#define A_AP_ACK_TIMEOUT_MS             1500U
#define A_AP_ACK_POLL_MS                  50U
#define A_AP_MAX_REINTENTOS                2U  /* hasta 3 transmisiones totales */

/* AckEstado_e y s_ack_pendiente se declaran arriba, junto a
 * s_apuntadorUpdateTaskHandle, porque BluetoothTask (definida antes que
 * esta seccion en el archivo) tambien las necesita. */

static const osThreadAttr_t s_apuntadorUpdateTask_attr = {
    .name       = "ApuntadorUpdateTask",
    .stack_size = 512U * 4U,
    .priority   = (osPriority_t)osPriorityNormal,
};

/**
 * @brief  Manda $A_AP<balas>,<pct>\r con el valor MAS RECIENTE de
 *         g_exercise_data (se relee en cada intento, incluidos los
 *         reintentos) y espera su ACK, hasta A_AP_MAX_REINTENTOS veces.
 * @note   Bloqueante (~hasta 1.5s por intento). Se aborta de inmediato si
 *         llega $DSCON durante la espera.
 */
static void ApuntadorUpdateTask_EnviarConAck(void) {
    char msg[32];

    for (uint8_t intento = 0U; intento <= A_AP_MAX_REINTENTOS; intento++) {
        if (!s_ejercicio_activo || s_dscon_en_ejercicio || s_end_admin_en_ejercicio) {
            /* El ejercicio ya termino (tiempo agotado) mientras estabamos
             * a media espera/reintento de un intento anterior -- no seguir
             * mandando/reintentando fuera de Ejercicio (ej. si el usuario
             * ya entro a Calibrar). */
            return;
        }

        uint16_t ammo_actual    = g_exercise_data.ammo;
        uint8_t  bateria_actual = g_exercise_data.lvBatery;

        int len = snprintf(msg, sizeof(msg), "$A_AP%u,%u\r",
                           (unsigned)ammo_actual, (unsigned)bateria_actual);
        if (len <= 0) { return; }

        s_ack_pendiente = ACK_A_AP;
        Bt_Transmit(&Bluetooth, (uint8_t *)msg, (uint16_t)len);
        Log_Printf("A_AP", "Enviado (intento %u/%u) -- balas=%u bateria=%u%%",
                  (unsigned)(intento + 1U), (unsigned)(A_AP_MAX_REINTENTOS + 1U),
                  (unsigned)ammo_actual, (unsigned)bateria_actual);

        uint32_t start = HAL_GetTick();
        while ((s_ack_pendiente == ACK_A_AP) &&
               ((HAL_GetTick() - start) < A_AP_ACK_TIMEOUT_MS)) {
            if (s_dscon_en_ejercicio) {
                Log_Print("A_AP", "Abortado -- DSCON en medio de la espera del ACK");
                return;
            }
            if (s_end_admin_en_ejercicio) {
                Log_Print("A_AP", "Abortado -- END_S en medio de la espera del ACK");
                return;
            }
            if (!s_ejercicio_activo) {
                Log_Print("A_AP", "Abortado -- ejercicio termino en medio de la espera del ACK");
                return;
            }
            osDelay(A_AP_ACK_POLL_MS);
        }

        if (s_ack_pendiente == ACK_NINGUNO) {
            Log_Print("A_AP", "ACK recibido");
            return;
        }
    }

    Log_Print("A_AP", "Sin ACK tras agotar reintentos -- se sigue de todos modos");
}

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

            if (ejercicio_disparo_habilitado &&
                (primera_vuelta || cambio_balas || cambio_bateria)) {
                ApuntadorUpdateTask_EnviarConAck();

                /* Se compara contra el valor MAS RECIENTE, llegue o no el
                 * ACK -- si fallo, el siguiente ciclo de 200ms ya no
                 * reintenta lo mismo para siempre, sigue con lo que haya
                 * cambiado de aqui en adelante. */
                ultimo_ammo    = g_exercise_data.ammo;
                ultima_bateria = g_exercise_data.lvBatery;
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
    Log_InitQueue();
    I2C1Bus_InitMutex();
}

void Tareas_CrearTareas(void) {
    /* Log_Print/Log_Printf YA SON SEGUROS de llamar aqui dentro (7-sep-2026,
     * Logger migrado de mutex a cola -- encolar con timeout 0 nunca bloquea,
     * el mensaje simplemente espera en la cola hasta que LoggerTask arranque
     * con el scheduler). La regla dura vieja ("nunca loguear entre la
     * creacion del mutex y osKernelStart") sigue aplicando SOLO para
     * I2C1Bus_Lock() -- ese si sigue siendo mutex, ver I2C1_Bus.h. */
    s_loggerTaskHandle = osThreadNew(Log_Task, NULL, &s_loggerTask_attr);
    s_menuTaskHandle    = osThreadNew(MenuTask, NULL, &s_menuTask_attr);
    s_luzMuxTaskHandle  = osThreadNew(LuzMuxTask, NULL, &s_luzMuxTask_attr);
    s_btTaskHandle      = osThreadNew(BluetoothTask, NULL, &s_btTask_attr);

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
