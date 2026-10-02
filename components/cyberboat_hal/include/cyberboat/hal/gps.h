#ifndef CYBERBOAT_HAL_GPS_H
#define CYBERBOAT_HAL_GPS_H

/** @file gps.h
 *  @brief Hardware-independent GPS receiver contract.
 */

#include "cyberboat/types.h"
#include "esp_err.h"

/** Operations supplied by a concrete GPS driver. */
typedef struct
{
    void *context; /**< Driver-owned state passed to each operation. */
    esp_err_t (*init)(void *context); /**< Initialize the receiver. */
    esp_err_t (*read_fix)(void *context, cb_gps_fix_t *fix); /**< Read the latest decoded fix. */
} cb_gps_hal_t;

#endif