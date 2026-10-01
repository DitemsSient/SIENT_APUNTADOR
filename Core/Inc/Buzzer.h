/**
 * @file    Buzzer.h
 * @brief   Driver for passive buzzer via PWM on STM32F4xx.
 *
 * @details Controls a passive buzzer using a general-purpose timer's PWM channel.
 *          CubeMX configuration (tarjeta nueva, 30-sep-2026):
 *          - TIM15 CH2, pin PA3, en modo PWM Generation. Timer propio, YA NO
 *            comparte ARR con el laser IR (ese sigue en TIM2 CH3/PA2) -- antes
 *            el Buzzer vivia en TIM2 CH2/PB3 y romper el ARR del laser era
 *            justo el bug que forzo este cambio de hardware (ver Pendientes.md).
 *          - TIM15 esta en APB2, igual que TIM2 en APB1 ambos corren a
 *            80 MHz de reloj de timer en este proyecto (RCC.APB2TimFreq_Value),
 *            asi que el mismo Prescaler=79 usado en TIM2 aplica aqui tambien
 *            para llegar a 1 000 000 Hz. CONFIRMAR en CubeMX que TIM15.Prescaler
 *            quede en 79 -- al momento de este cambio el .ioc todavia no lo
 *            traia seteado explicitamente.
 *          - Set BUZZER_TIMER_CLK to APB2_clock_timer / (prescaler + 1).
 *            80 000 000 / (79 + 1) = 1 000 000 Hz.
 *
 * @date    March 27, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#ifndef BUZZER_H
#define BUZZER_H

#include "stm32l4xx_hal.h"

/* ========================  CONFIGURATION  ================================= */

/* Timer and PWM channel assigned to the buzzer */

#define BUZZER_TIMER            (&htim15)
#define BUZZER_CHANNEL          TIM_CHANNEL_2

/**
 * @brief Effective timer clock in Hz after the prescaler.
 * @note  Must match APB1_clock / (prescaler + 1) in CubeMX.
 */
#define BUZZER_TIMER_CLK        1000000U

/**
 * @brief Gap inserted between consecutive notes (ms).
 * @note  Prevents adjacent notes from blending. 20 ms is imperceptible
 *        but keeps the melody clean.
 */
#define BUZZER_NOTE_GAP_MS      20U

/* ====================  DEVICE CONSTANTS  ================================== */

/* Musical note frequencies — Octave 4 */

#define C4      262U
#define Cs4     277U    /**< C#4 / Db4           */
#define D4      294U
#define Ds4     311U    /**< D#4 / Eb4           */
#define E4      330U
#define F4      349U
#define Fs4     370U    /**< F#4 / Gb4           */
#define G4      392U
#define Gs4     415U    /**< G#4 / Ab4           */
#define A4      440U    /**< A440 — standard tuning */
#define As4     466U    /**< A#4 / Bb4           */
#define B4      494U

/* Musical note frequencies — Octave 5 */

#define C5      523U
#define Cs5     554U
#define D5      587U
#define Ds5     622U
#define E5      659U
#define F5      698U
#define Fs5     740U
#define G5      784U
#define Gs5     831U
#define A5      880U
#define As5     932U
#define B5      988U

/* Musical note frequencies — Octave 6 */

#define C6      1047U
#define D6      1175U
#define E6      1319U

/* Rest / silence */

#define SILENCE     0U

/* Rhythmic duration divisors (fractions of a whole note) */

#define WHOLE       1
#define HALF        2
#define QUARTER     4
#define EIGHTH      8
#define SIXTEENTH   16

/* Calculates the number of notes in a static BuzzerNote_t array */

#define MELODY_LEN(arr)     (sizeof(arr) / sizeof((arr)[0]))

/* ============================  STRUCTURES  ================================ */

/* Represents a single musical note: frequency and rhythmic duration */

typedef struct {
    int16_t note;       /**< Frequency in Hz (0 = silence)                 */
    int16_t duration;   /**< Rhythmic value (QUARTER, EIGHTH, etc.).
                             Negative value = dotted note                   */
} BuzzerNote_t;

/* ================================  API  =================================== */

/**
 * @brief  Starts the timer PWM channel for the buzzer.
 * @note   Call once after MX_TIMx_Init().
 */
void Buzzer_Init(void);

/**
 * @brief  Outputs a continuous tone at the given frequency.
 * @param  freq  Tone frequency in Hz. Pass 0 to silence the buzzer.
 */
void Buzzer_PlayTone(uint16_t freq);

/**
 * @brief  Silences the buzzer immediately by setting the duty cycle to 0.
 */
void Buzzer_Stop(void);

/**
 * @brief  Plays a complete melody defined by a BuzzerNote_t array.
 * @param  melody   Pointer to the note array.
 * @param  length   Number of notes in the array.
 * @param  bpm      Tempo in beats per minute.
 * @note   Blocking — uses HAL_Delay() internally.
 */
void Buzzer_PlayMelody(const BuzzerNote_t *melody, uint16_t length, uint16_t bpm);

/* ========================  SELF-TEST  ==================================== */

/**
 * @brief  Emite un tono continuo en A4 durante 5 segundos.
 * @note   Bloqueante. El resultado lo confirma el usuario visualmente.
 */
void Buzzer_Test(void);

#endif /* BUZZER_H */
