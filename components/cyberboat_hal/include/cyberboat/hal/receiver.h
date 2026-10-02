#ifndef CYBERBOAT_HAL_RECEIVER_H
#define CYBERBOAT_HAL_RECEIVER_H

/** @file receiver.h
 *  @brief Hardware-independent pilot receiver contract.
 */

#include "cyberboat/types.h"
#include "esp_err.h"

/** Operations supplied by a PWM, SBUS, radio, or network receiver driver.
 *
 * Drivers expose raw channel data. Semantic mapping to throttle, steering,
 * servo, and arming belongs to the control layer.
 */
typedef struct
{
    void *context; /**< Driver-owned state passed to each operation. */
    esp_err_t (*init)(void *context); /**< Initialize the transport. */
    esp_err_t (*read_frame)(void *context, cb_receiver_raw_frame_t *frame); /**< Snapshot raw channels and freshness. */
} cb_receiver_hal_t;

#endif