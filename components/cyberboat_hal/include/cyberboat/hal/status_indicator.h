#ifndef CYBERBOAT_HAL_STATUS_INDICATOR_H
#define CYBERBOAT_HAL_STATUS_INDICATOR_H

/** @file status_indicator.h
 *  @brief Hardware-independent RGB status indicator contract.
 */

#include <stdint.h>

#include "esp_err.h"

/** Operations supplied by a concrete status indicator driver. */
typedef struct
{
    void *context; /**< Driver-owned state passed to each operation. */
    esp_err_t (*set_rgb)(void *context, uint8_t red, uint8_t green, uint8_t blue); /**< Set an RGB color. */
    esp_err_t (*clear)(void *context); /**< Turn the indicator off. */
} cb_status_indicator_hal_t;

#endif