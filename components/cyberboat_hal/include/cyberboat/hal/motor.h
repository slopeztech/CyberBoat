#ifndef CYBERBOAT_HAL_MOTOR_H
#define CYBERBOAT_HAL_MOTOR_H

/** @file motor.h
 *  @brief Hardware-independent propulsion driver contract.
 */

#include "cyberboat/types.h"
#include "esp_err.h"

/** Operations supplied by a concrete motor/ESC driver.
 *
 * Outputs use the normalized axis range. Implementations must define a safe
 * unarmed state and make emergency_stop safe to call repeatedly.
 */
typedef struct
{
    void *context; /**< Driver-owned state passed to each operation. */
    esp_err_t (*init)(void *context); /**< Initialize the hardware. */
    esp_err_t (*set_armed)(void *context, bool armed); /**< Request the armed state. */
    esp_err_t (*set_output)(void *context, uint8_t motor_index, cb_axis_t output); /**< Set one motor command. */
    esp_err_t (*emergency_stop)(void *context); /**< Immediately request the driver's safe output. */
} cb_motor_hal_t;

#endif