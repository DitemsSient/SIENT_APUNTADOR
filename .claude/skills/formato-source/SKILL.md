---
name: formato-source
description: Template and formatting rules for driver .c files in Core/Src/. Use when creating or modifying a driver source file.
---

# Driver Source File Format (.c)

When creating or modifying a `.c` driver file in `Core/Src/`, **follow this template** in the order shown. Only include sections that apply to the driver.

## Template

```c
/**
 * @file    DriverName.c
 * @brief   Driver implementation for [component].
 *
 * @date    Month DD, YYYY
 * @author  César Pérez
 * @version 1.0.0
 */

#include "DriverName.h"
#include <string.h>         /* only if needed */

/* ======================  EXTERNAL HAL HANDLES  ============================ */

extern SPI_HandleTypeDef hspi1;     /* or whichever peripheral applies */

/* ======================  STATIC VARIABLES  ================================ */

/* Internal buffers and driver state */

static uint8_t internal_buffer[SIZE];
static uint8_t current_state = 0U;

/* ======================  STATIC FUNCTIONS  ================================ */

/* Internal helper functions */

/**
 * @brief  Helper function description.
 * @param  data    Parameter description.
 * @param  length  Parameter description.
 * @note   Note if applicable.
 */
static DriverStatus_e Driver_InternalFunction(uint8_t *data, uint16_t length) {
    if (HAL_SPI_Transmit(DRIVER_HANDLE, data, length, DRIVER_TIMEOUT) != HAL_OK) {
        return DRIVER_ERR_COM;
    }
    return DRIVER_OK;
}

/* ================================  API  =================================== */

/* Public functions declared in the .h */

/**
 * @brief  Initializes the peripheral and sets hardware to a safe state.
 */
DriverStatus_e Driver_Init(void) {
    /* Initialize HAL peripheral */
    /* Set to safe state */
    Driver_Off();
    return DRIVER_OK;
}

/**
 * @brief  More detailed description of how this function works
 *         internally (more than in the .h).
 */
DriverStatus_e Driver_Function(uint32_t param1, const uint8_t *param2) {
    if (param2 == NULL || param1 == 0U) {
        return DRIVER_ERR_PARAM;
    }

    DriverStatus_e status = Driver_InternalFunction((uint8_t *)param2, param1);
    if (status != DRIVER_OK) return status;

    return DRIVER_OK;
}
```

## Function Documentation

- **Static (private) functions**: full documentation with `@brief`, `@param`, `@note`
- **Public (API) functions**: only `@brief` describing in more detail than the `.h` how the function works internally. No `@param` or `@retval` (already in the `.h`)

## Mandatory Patterns

1. **Own include first**: `#include "DriverName.h"` always goes before any other include
2. **Header macros for handles**: use `DRIVER_HANDLE`, `DRIVER_PORT`, etc. — never raw `&hspi1` in the code
3. **Early return on error**: `if (status != OK) return status;`
4. **Parameter validation**: check NULL pointers and ranges in public functions
5. **Const pointers for input**: `const uint8_t *data` when the buffer is not modified
6. **Safe init**: every `_Init()` function must leave hardware in a safe state (off/stopped)

## Rules

1. **Section separators**: `/* ========================  TITLE  ===================== */` (~77 chars wide)
2. **Brief comments**: before each zone a `/* short zone description */`
3. **Numeric literals**: always with `U` suffix → `100U`, `0xFFU`
4. **Everything in English**: comments, documentation

## Canonical Reference

See `Core/Src/Flash.c` as a complete example.
