#ifndef CYBERBOAT_HAL_SERVO_H
#define CYBERBOAT_HAL_SERVO_H

/** @file servo.h
 *  @brief Hardware-independent position-servo driver contract.
 */

#include <stdint.h>

#include "cyberboat/types.h"
#include "esp_err.h"

/** Operations supplied by a concrete servo driver.
 *
 * Normalized positions use CB_AXIS_MIN..CB_AXIS_MAX. Pulse-width control is
 * optional for implementations that do not expose PWM calibration directly.
 */
typedef struct
{
    void *context; /**< Driver-owned state passed to each operation. */
    esp_err_t (*init)(void *context); /**< Initialize the servo output. */
    esp_err_t (*set_position)(void *context, cb_axis_t position); /**< Set a normalized position. */
    esp_err_t (*set_pulse_width_us)(void *context, uint32_t pulse_width_us); /**< Set a calibrated pulse width. */
} cb_servo_hal_t;

#endif