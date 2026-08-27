/**
 * @file    Inicializacion.c
 * @brief   Implementación de la secuencia de arranque de la tarjeta.
 *
 * @date    August 27, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#include "Inicializacion.h"
#include "Bootloader.h"
#include "LedRGB.h"
#include "Logger.h"
#include "usb_device.h"

/* ================================  API  =================================== */

/* Función pública declarada en el .h */

void Inicializacion_Run(void) {
    /* Debe ir primero: si el botón de bootloader está presionado, esta
     * llamada nunca regresa. El USB tiene que seguir libre para que el
     * bootloader lo inicialice el solo, por eso MX_USB_DEVICE_Init() va
     * después, no antes. */
    LedRGB_Init();
    Bootloader_CheckAndEnter();

    MX_USB_DEVICE_Init();
    Log_Init();
}
