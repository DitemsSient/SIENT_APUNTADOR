---
name: formato-header
description: Template and formatting rules for driver .h files in Core/Inc/. Use when creating or modifying a driver header.
---

# Driver Header File Format (.h)

When creating or modifying a `.h` driver file in `Core/Inc/`, **follow this template** in the order shown. Only include sections that apply to the driver.

## Template

```c
/**
 * @file    DriverName.h
 * @brief   Short one-line description.
 *
 * @details Only relevant information for using the driver:
 *          CubeMX pin configuration, peripheral used,
 *          key .ioc parameters that must match, etc.
 *
 * @date    Month DD, YYYY
 * @author  César Pérez
 * @version 1.0.0
 */

#ifndef DRIVERNAME_H
#define DRIVERNAME_H

#include "stm32l4xx_hal.h"
#include <stdint.h>         /* only if needed */
#include <stdbool.h>        /* only if needed */

/* ========================  CONFIGURATION  ================================= */

/* Handle, ports, pins and timeouts. Must match the .ioc configuration */

#define DRIVER_HANDLE           (&hperipheraln)
#define DRIVER_PORT             GPIOx
#define DRIVER_PIN              GPIO_PIN_n
#define DRIVER_TIMEOUT          100U

/* ====================  DEVICE CONSTANTS  ================================== */

/* Commands, IDs, memory sizes, masks, etc. */

#define DRIVER_CONST_A          0x00    /**< Short description */
#define DRIVER_CONST_B          0x01    /**< Short description */

/* ========================  ENUMERATIONS  ================================== */

/* Return codes for driver functions */

typedef enum {
    DRIVER_OK            = 0,
    DRIVER_ERR_COM       = 1,
    DRIVER_ERR_BUSY      = 2,
    DRIVER_ERR_PARAM     = 3
} DriverStatus_e;

/* ============================  STRUCTURES  ================================ */

/* Structure to hold [component] read data */

typedef struct {
    uint16_t    field_one;      /**< Field description */
    uint8_t     field_two;      /**< Field description */
} DriverData_t;

/* ==========================  PREDEFINED PATTERNS  ========================= */

/* Only if the driver uses sequences/patterns (buzzer, vibrator, LED, etc.) */

static const DriverStep_t DRIVER_PATTERN_EXAMPLE[] = {
    { value1, duration1 },
    { value2, duration2 },
};
#define DRIVER_PATTERN_EXAMPLE_LEN  2U

#define DRIVER_PATTERN_LEN(arr)     (sizeof(arr) / sizeof((arr)[0]))

/* ================================  API  =================================== */

/**
 * @brief  Initializes the driver.
 * @note   Call after MX_xxx_Init().
 */
DriverStatus_e Driver_Init(void);

/**
 * @brief  Function description.
 * @param  param1  Parameter description.
 * @param  param2  Parameter description.
 * @note   Simple note if applicable.
 */
DriverStatus_e Driver_Function(uint32_t param1, const uint8_t *param2);

#endif /* DRIVERNAME_H */
```

## Naming Conventions

| Element | Format | Example |
|---------|--------|---------|
| Macros / defines | `DRIVER_PARAM_NAME` (UPPERCASE) | `FLASH_SPI_TIMEOUT` |
| Functions | `DriverName_Function` (PascalCase) | `Flash_ReadID()` |
| Enums | `_e` suffix | `FlashStatus_e` |
| Structs / typedefs | `_t` suffix | `FlashID_t` |
| Struct fields | `snake_case` | `device_id`, `raw_value` |

## Rules

1. **Include guard**: `#ifndef DRIVERNAME_H` / `#define DRIVERNAME_H` / `#endif /* DRIVERNAME_H */`
2. **Section separators**: `/* ========================  TITLE  ===================== */` (~77 chars wide)
3. **Status enum**: always `OK = 0` as first value
4. **Numeric literals**: always with `U` suffix → `100U`, `4096U`, `0xFFU`
5. **Doxygen**: `@file`, `@brief`, `@details`, `@param`, `@note` — do NOT use `@retval`
6. **Brief comments**: before each zone (enums, structs, constants) a `/* short zone description */`
7. **Everything in English**: comments, documentation, descriptive names

## Canonical Reference

See `Core/Inc/Flash.h` as a complete example.
