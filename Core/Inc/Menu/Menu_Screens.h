/**
 * @file    Menu_Screens.h
 * @brief   Internal screen function declarations for the menu system.
 *
 * @details Each screen exposes two functions:
 *          - Draw: renders the screen based on handle state.
 *          - OnButton: reacts to Navigate / Enter presses.
 *
 *          This header is internal — only included by Menu.c and
 *          the individual screen .c files. Do NOT include in main.c.
 *
 * @date    April 13, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#ifndef MENU_SCREENS_H
#define MENU_SCREENS_H

#include "Menu/Menu.h"

/* ========================  MAIN MENU  ==================================== */
/* Implemented in Menu.c */

void Screen_MainMenu_Draw(Menu_Handle_t *h);
void Screen_MainMenu_OnButton(Menu_Handle_t *h, MenuButton_e btn);

/* ========================  TEST HARDWARE  ================================ */

void Screen_TestHW_Draw(Menu_Handle_t *h);
void Screen_TestHW_OnButton(Menu_Handle_t *h, MenuButton_e btn);

/* ========================  BLUETOOTH  ==================================== */

/** @brief Sub-states for the Bluetooth screen. */
typedef enum {
    BT_MAIN          = 0,   /**< Anunciar / Salir  (o Id + Salir)         */
    BT_INICIANDO     = 1,   /**< Sends $CON\r                             */
    BT_BUSCANDO      = 2,   /**< Espera $ACKCON\r (conectado) o $NoCON\r (timeout), LED azul parpadea 1s */
    BT_ESPERANDO     = 3,   /**< Espera $1<MAC>\r tras el $OK\r           */
    BT_SPLASH_OK     = 4,   /**< "Conectado" (3 s) → MAIN                */
    BT_SPLASH_ERR    = 5    /**< "Falla de conexion" (3 s) → MAIN        */
} BtSubState_e;

void Screen_Bluetooth_Draw(Menu_Handle_t *h);
void Screen_Bluetooth_OnButton(Menu_Handle_t *h, MenuButton_e btn);
void Screen_Bluetooth_ResetLink(void);

/* ========================  PROGRAMMING  ================================== */

/** @brief Sub-states for the Programming screen. */
typedef enum {
    PROG_MCU         = 0,   /**< Header: "Mode: MCU"  · opts: Bluetooth / Salir  */
    PROG_SPLASH_BT   = 1,   /**< Splash "Modo BT activo" (3 s) → PROG_BT         */
    PROG_BT          = 2,   /**< Header: "Mode: BT"   · opts: MCU / Salir         */
    PROG_SPLASH_MCU  = 3    /**< Splash "Modo MCU activo" (3 s) → PROG_MCU        */
} ProgSubState_e;

void Screen_Programming_Draw(Menu_Handle_t *h);
void Screen_Programming_OnButton(Menu_Handle_t *h, MenuButton_e btn);

/* ========================  EXERCISE  ===================================== */

/** @brief Sub-states for the Exercise screen. */
typedef enum {
    EX_NO_BT         = 0,   /**< Bt status + Preview/Calibrar/Salir    */
    EX_PREVIEW       = 1,   /**< Bitmap preview (auto-return)          */
    EX_CALIBRAR      = 2,   /**< Modo calibración activo               */
    EX_WAITING       = 3,   /**< Bitmap + "Esperando..."               */
    EX_ACTIVE        = 4    /**< HUD: lives, ammo, battery             */
} ExSubState_e;

void Screen_Exercise_Draw(Menu_Handle_t *h);
void Screen_Exercise_OnButton(Menu_Handle_t *h, MenuButton_e btn);
void Screen_Exercise_ResetScroll(void);

/**
 * @brief  Dibuja la pagina de balas/vidas (misma vista que EX_PREVIEW),
 *         usada tambien por ExerciseTask (Tareas_Interrupciones.c) para
 *         alternar pantallas durante un ejercicio activo.
 */
void Exercise_DrawStatsPage(void);

/**
 * @brief  Dibuja la pagina de equipo/jugador (misma vista que EX_PREVIEW),
 *         usada tambien por ExerciseTask.
 */
void Exercise_DrawTeamPage(void);

#endif /* MENU_SCREENS_H */
