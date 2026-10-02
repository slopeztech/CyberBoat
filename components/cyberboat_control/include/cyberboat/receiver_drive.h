#ifndef CYBERBOAT_RECEIVER_DRIVE_H
#define CYBERBOAT_RECEIVER_DRIVE_H

/** @file receiver_drive.h
 *  @brief Calibrated four-channel receiver mapping for differential propulsion.
 */

#include <stdbool.h>
#include <stdint.h>

#include "cyberboat/receiver_mapping.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Channel assignment and pulse calibration for a two-motor auxiliary-servo boat. */
typedef struct
{
    uint8_t servo_channel; /**< Zero-based channel selecting auxiliary-servo position. */
    uint8_t throttle_channel; /**< Zero-based channel setting maximum motor power. */
    uint8_t direction_channel; /**< Centered signed speed channel: reverse, stop, or forward. */
    uint8_t steering_channel; /**< Centered channel controlling differential turn. */
    uint8_t left_motor_index; /**< H-bridge motor index physically installed on the left. */
    uint16_t deadband_us; /**< Pulse deadband around calibrated centers and throttle stop. */
    uint16_t frame_timeout_ms; /**< Maximum age accepted for every required channel. */
    uint8_t servo_travel_percent; /**< Fraction of calibrated servo travel used, 50 means half range. */
    bool servo_reversed; /**< Reverse the auxiliary-servo sweep. */
    bool direction_reversed; /**< Swap forward and reverse positions. */
    bool steering_reversed; /**< Swap left and right steering. */
    cb_receiver_axis_calibration_t servo; /**< Left/center/right pulse calibration. */
    cb_receiver_axis_calibration_t throttle; /**< Stop/nominal-center/full-power calibration. */
    cb_receiver_axis_calibration_t direction; /**< Reverse/neutral/forward speed calibration. */
    cb_receiver_axis_calibration_t steering; /**< Left/center/right calibration. */
} cb_receiver_drive_config_t;

/** Mapped motor and servo targets, assigned by left_motor_index in the config. */
typedef struct
{
    cb_motor_outputs_t motors; /**< Signed normalized commands, limited by throttle. */
    cb_axis_t auxiliary_servo; /**< Position command within the configured travel percentage. */
    bool rearm_safe; /**< True only with throttle at stop and direction in neutral. */
    uint32_t valid_for_ms; /**< Minimum remaining freshness across all four channels. */
} cb_receiver_drive_command_t;

/** Load channel assignments, calibrations, and drive limits from Kconfig. */
esp_err_t cb_receiver_drive_get_default_config(cb_receiver_drive_config_t *config);

/** Validate channel uniqueness, calibrations, and safety ranges. */
esp_err_t cb_receiver_drive_validate_config(const cb_receiver_drive_config_t *config);

/** Map a fresh receiver frame to motor and auxiliary-servo commands.
 *
 * Throttle is unidirectional and sets the maximum absolute motor output. The
 * direction channel scales that limit continuously from reverse through zero at
 * center to forward. The signed motor power is throttle_limit * direction_axis /
 * CB_AXIS_MAX. Steering only reduces the inner motor; it never increases either
 * motor above the resulting power. Missing or stale required channels return an
 * error and leave every output at zero.
 *
 * @param[in] raw Raw PWM frame.
 * @param[in] config Valid channel and calibration configuration.
 * @param[out] command Mapped commands, initialized to safe zero values on error.
 * @return ESP_OK on success, ESP_ERR_TIMEOUT for missing/stale inputs, or
 *         ESP_ERR_INVALID_ARG for invalid pointers, frame, or configuration.
 */
esp_err_t cb_receiver_drive_map(
    const cb_receiver_raw_frame_t *raw,
    const cb_receiver_drive_config_t *config,
    cb_receiver_drive_command_t *command);

#if CONFIG_CYBERBOAT_RUN_UNIT_TESTS
/** Run hardware-independent four-channel drive mapping tests. */
void cb_receiver_drive_run_unit_tests(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
