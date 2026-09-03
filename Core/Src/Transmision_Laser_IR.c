/**
 * @file    Transmision_Laser_IR.c
 * @brief   Driver implementation for IR laser transmitter via Timer PWM.
 *
 * @date    March 12, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#include "Transmsion_Laser_IR.h"
#include "main.h"
#include "Logger.h"
#include "Inicializacion.h"

/* ======================  STATIC FUNCTIONS  ================================ */

/*
 * Pin control strategy for PA2 ("TX_LASER"):
 *
 * During MARK: PA2 is configured as Alternate Function (AF1) so TIM2 CH3
 *              drives the 40 kHz carrier directly on the pin.
 *
 * During SPACE: PA2 is reconfigured as GPIO Output Low, forcing 0 V regardless
 *               of the timer's internal state. This eliminates the residual
 *               voltage that appears when the pin stays in AF mode with CCR = 0.
 *
 * TIM2 keeps running continuously; only the pin's MODER field is toggled.
 * ARR is shared with the buzzer (also on TIM2, CH2) — see the ARR
 * re-enforcement in Tx_IR_Init()/Tx_IR_SendFrame()/Tx_IR_SendCalibration().
 */

/**
 * @brief  Switches PA2 to Alternate Function mode so TIM2 CH3 drives it.
 * @note   Modifies MODER bits directly for minimal latency.
 */
static inline void Tx_IR_Pin_SetAF(void) {
    Tx_IR_GPIO_PORT->MODER &= ~(3U << (Tx_IR_GPIO_PIN_NUM * 2U));
    Tx_IR_GPIO_PORT->MODER |=  (2U << (Tx_IR_GPIO_PIN_NUM * 2U));
}

/**
 * @brief  Switches PA2 to GPIO Output Low, forcing 0 V on the pin.
 * @note   Clears ODR first, then sets MODER to output mode.
 */
static inline void Tx_IR_Pin_SetLow(void) {
    Tx_IR_GPIO_PORT->ODR   &= ~(1U << Tx_IR_GPIO_PIN_NUM);
    Tx_IR_GPIO_PORT->MODER &= ~(3U << (Tx_IR_GPIO_PIN_NUM * 2U));
    Tx_IR_GPIO_PORT->MODER |=  (1U << (Tx_IR_GPIO_PIN_NUM * 2U));
}

/**
 * @brief  Blocking microsecond delay using TIM1 as a free-running 1 MHz counter.
 * @param  us  Microseconds to wait (max 65535).
 * @note   uint16_t subtraction handles the counter wrap-around automatically.
 */
static void Tx_IR_DelayUs(uint16_t us) {
    uint16_t start = (uint16_t)__HAL_TIM_GET_COUNTER(&Tx_IR_DELAY_TIM_HANDLE);
    while ((uint16_t)(__HAL_TIM_GET_COUNTER(&Tx_IR_DELAY_TIM_HANDLE) - start) < us) {}
}

/**
 * @brief  Computes the XOR checksum of a byte buffer.
 * @param  data  Data buffer.
 * @param  len   Number of bytes.
 */
static uint8_t Tx_IR_ComputeChecksum(const uint8_t *data, uint8_t len) {
    uint8_t chk = 0U;
    for (uint8_t i = 0U; i < len; i++) {
        chk ^= data[i];
    }
    return chk;
}

/**
 * @brief  Generates a 40 kHz burst for Tx_IR_MARK_US microseconds.
 * @note   Sets CCR_ACTIVE, switches to AF mode, then waits.
 */
static void Tx_IR_Mark(void) {
    __HAL_TIM_SET_COMPARE(&Tx_IR_TIM_HANDLE, Tx_IR_TIM_CHANNEL, Tx_IR_TIM_CCR_ACTIVE);
    Tx_IR_Pin_SetAF();
    Tx_IR_DelayUs(Tx_IR_MARK_US);
}

/**
 * @brief  Holds the output at 0 V for the given number of microseconds.
 * @param  us  Space duration in microseconds.
 */
static void Tx_IR_Space(uint16_t us) {
    Tx_IR_Pin_SetLow();
    __HAL_TIM_SET_COMPARE(&Tx_IR_TIM_HANDLE, Tx_IR_TIM_CHANNEL, 0U);
    Tx_IR_DelayUs(us);
}

/**
 * @brief  Transmits one symbol: MARK followed by a SPACE of variable duration.
 * @param  space_us  SPACE duration in microseconds.
 */
static void Tx_IR_SendSymbol(uint16_t space_us) {
    Tx_IR_Mark();
    Tx_IR_Space(space_us);
}

/**
 * @brief  Transmits one bit using the protocol timing.
 * @param  bit  0 → Tx_IR_SPACE0_US, 1 → Tx_IR_SPACE1_US.
 */
static void Tx_IR_SendBit(uint8_t bit) {
    Tx_IR_SendSymbol(bit ? Tx_IR_SPACE1_US : Tx_IR_SPACE0_US);
}

/**
 * @brief  Transmits one byte MSB-first, then appends an INTER separator.
 * @param  data  Byte to transmit.
 */
static void Tx_IR_SendByte(uint8_t data) {
    for (int8_t i = 7; i >= 0; i--) {
        Tx_IR_SendBit((data >> i) & 0x01U);
    }
    Tx_IR_SendSymbol(Tx_IR_INTER_US);
}

/* ================================  API  =================================== */

/* Public functions declared in the .h */

/**
 * @brief  Forces PA2 to GPIO Output Low and sets CCR to 0 as the idle state.
 */
void Tx_IR_Init(void) {
    Tx_IR_Pin_SetLow();
    /* ARR is shared with the buzzer on the same timer (TIM2) — enforce the
     * 40 kHz carrier period here in case the buzzer left it at a note's ARR. */
    __HAL_TIM_SET_AUTORELOAD(&Tx_IR_TIM_HANDLE, 24U);
    __HAL_TIM_SET_COMPARE(&Tx_IR_TIM_HANDLE, Tx_IR_TIM_CHANNEL, 0U);
}

/**
 * @brief  Validates parameters, computes the XOR checksum, then transmits:
 *         SYNC → [data bytes with INTER] → [checksum with INTER] → SYNC.
 *         Leaves PA2 low after transmission.
 */
HAL_StatusTypeDef Tx_IR_SendFrame(const uint8_t *pData, uint8_t len) {
    if ((pData == NULL) || (len == 0U) || (len > Tx_IR_MAX_DATA_BYTES)) {
        return HAL_ERROR;
    }

    /* ARR is shared with the buzzer on TIM2 — re-enforce the carrier period
     * in case a melody played since the last transmission. */
    __HAL_TIM_SET_AUTORELOAD(&Tx_IR_TIM_HANDLE, 24U);

    uint8_t checksum = Tx_IR_ComputeChecksum(pData, len);

    Tx_IR_SendSymbol(Tx_IR_SYNC_US);            /* Start SYNC */

    for (uint8_t i = 0U; i < len; i++) {
        Tx_IR_SendByte(pData[i]);
    }

    Tx_IR_SendByte(checksum);                   /* Checksum byte */

    Tx_IR_SendSymbol(Tx_IR_SYNC_US);            /* End SYNC */

    Tx_IR_Pin_SetLow();                         /* Ensure pin is idle-low */

    return HAL_OK;
}

/**
 * @brief  Transmits LASER_CAL_CODE (0xAA55) as two bytes, no CRC.
 * @note   Frame structure: SYNC → 0xAA (INTER) → 0x55 (INTER) → SYNC.
 *         Used during calibration mode to aim the laser without exposing the MAC.
 */
void Tx_IR_SendCalibration(void) {
    uint8_t b0 = (uint8_t)((LASER_CAL_CODE >> 8U) & 0xFFU);  /* 0xAA */
    uint8_t b1 = (uint8_t)(LASER_CAL_CODE & 0xFFU);           /* 0x55 */

    /* ARR is shared with the buzzer on TIM2 — re-enforce the carrier period
     * in case a melody played since the last transmission. */
    __HAL_TIM_SET_AUTORELOAD(&Tx_IR_TIM_HANDLE, 24U);

    Tx_IR_SendSymbol(Tx_IR_SYNC_US);
    Tx_IR_SendByte(b0);
    Tx_IR_SendByte(b1);
    Tx_IR_SendSymbol(Tx_IR_SYNC_US);

    Tx_IR_Pin_SetLow();
}

volatile bool gatillo_disparo_pendiente_log = false;

/* Contadores de diagnostico temporal -- para ver si la ISR entra y si el
 * antirrebote esta descartando disparos legitimos. Se imprimen junto al log. */
volatile uint32_t gatillo_isr_entradas = 0U;
volatile uint32_t gatillo_disparos_enviados = 0U;

/**
 * @brief  Gatillo (PA8, EXTI8) -- dispara al vuelo: codigo de calibracion
 *         (0xAA55) en modo Calibrar, o el frame real (orden+lora) durante
 *         un Ejercicio activo, con su descuento de balas.
 * @note   Corre en ISR de maxima prioridad, bloquea ~15-20ms. No llama
 *         ninguna funcion de FreeRTOS ni nada que use un mutex (Log_Print
 *         incluido) -- eso se difiere via gatillo_disparo_pendiente_log,
 *         que MenuTask/ExerciseTask revisan y limpian en su propio ciclo
 *         (cualquiera de las dos que este activa en ese momento).
 *         Solo dispara si el menu esta en SCREEN_EXERCISE (ejercicio o
 *         calibrar) -- en Configuracion/Test HW/Bluetooth/Programar el
 *         boton no debe mandar nada.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    static volatile uint32_t s_last_tick = 0U;

    if (GPIO_Pin == GATILLO_Pin) {
        if (hmenu.screen != SCREEN_EXERCISE) {
            return;
        }

        /* En Ejercicio real (no Calibrar) no se puede disparar mientras
         * corre la cuenta regresiva -- ExerciseTask pone esta bandera en
         * true justo despues de mostrar "!INICIA!". Calibrar no usa esta
         * bandera, siempre puede disparar. */
        if (!laser_calibration_mode && !ejercicio_disparo_habilitado) {
            return;
        }

        gatillo_isr_entradas++;

        uint32_t now = HAL_GetTick();
        if ((now - s_last_tick) < GATILLO_DEBOUNCE_MS) {
            return;
        }
        s_last_tick = now;

        if (laser_calibration_mode) {
            /* Calibrar: solo apuntar, no gasta balas. */
            Tx_IR_SendCalibration();
        } else {
            /* Ejercicio real: frame con orden+lora (p.ej. orden=1,lora=2
             * -> bytes {0x01,0x02}), y SI descuenta bala (tope en 0). */
            uint8_t frame[2] = { g_exercise_data.orden, g_exercise_data.lora };
            Tx_IR_SendFrame(frame, 2U);

            if (g_exercise_data.ammo > 0U) {
                g_exercise_data.ammo--;
            }
        }

        gatillo_disparos_enviados++;

        gatillo_disparo_pendiente_log = true;
    }
}
