#ifndef CYBERBOAT_RECEIVER_MAPPING_H
#define CYBERBOAT_RECEIVER_MAPPING_H

/** @file receiver_mapping.h
 *  @brief Configurable mapping from raw RC channels to semantic pilot commands.
 */

#include <stdbool.h>
#include <stdint.h>

#include "cyberboat/types.h"
#include "esp_err.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Calibrated pulse range for one normalized RC axis. */
typedef struct
{
    uint16_t min_pulse_us; /**< Pulse mapped to the negative endpoint or zero-throttle stop. */
    uint16_t center_pulse_us; /**< Neutral pulse used for centered axes and validation. */
    uint16_t max_pulse_us; /**< Pulse mapped to the positive endpoint. */
} cb_receiver_axis_calibration_t;

/** Build-time channel assignment, calibration, and failsafe parameters. */
typedef struct
{
    uint8_t throttle_channel; /**< Zero-based raw input channel for throttle. */
    uint8_t steering_channel; /**< Zero-based raw input channel for steering. */
    uint8_t auxiliary_servo_channel; /**< Zero-based channel used when an auxiliary servo role is selected. */
    uint8_t arm_channel; /**< Zero-based auxiliary channel used when arm_enabled is true. */
    bool steering_enabled; /**< Whether the selected topology consumes steering input. */
    bool arm_enabled; /**< Whether an auxiliary channel may request arming. */
    bool auxiliary_servo_enabled; /**< Whether the selected vessel mode maps an auxiliary servo. */
    bool throttle_reversed; /**< Reverse the mapped throttle direction. */
    bool steering_reversed; /**< Reverse the mapped steering direction. */
    bool auxiliary_servo_reversed; /**< Reverse the mapped auxiliary servo direction. */
    bool throttle_bidirectional; /**< Map around center; false maps throttle from stop to forward only. */
    uint16_t arm_threshold_us; /**< Pulse at or above this value requests arming. */
    uint16_t deadband_us; /**< Neutral deadband around axis center. */
    uint16_t frame_timeout_ms; /**< Maximum accepted age for every mapped channel. */
    cb_receiver_axis_calibration_t throttle; /**< Throttle pulse calibration. */
    cb_receiver_axis_calibration_t steering; /**< Steering pulse calibration. */
    cb_receiver_axis_calibration_t auxiliary_servo; /**< Auxiliary servo pulse calibration. */
} cb_receiver_mapping_config_t;

/** Load mapping settings from ESP-IDF Kconfig. */
esp_err_t cb_receiver_mapping_get_default_config(cb_receiver_mapping_config_t *config);

/** Validate channel assignments, calibration, deadband, and timeout. */
esp_err_t cb_receiver_mapping_validate_config(const cb_receiver_mapping_config_t *config);

/** Convert fresh raw channel pulses into throttle, steering, and arm request.
 *
 * Invalid or stale required channels return an error and leave the output
 * command in its safe zero/disarmed state.
 *
 * @param[in] raw Raw PWM channel snapshot from a receiver HAL.
 * @param[in] config Valid mapping configuration.
 * @param[out] command Destination semantic command.
 * @return ESP_OK on success, ESP_ERR_TIMEOUT for stale/missing mapped channels,
 *         or ESP_ERR_INVALID_ARG for malformed pulses/configuration.
 */
esp_err_t cb_control_map_receiver(
    const cb_receiver_raw_frame_t *raw,
    const cb_receiver_mapping_config_t *config,
    cb_receiver_frame_t *command);

#if CONFIG_CYBERBOAT_RUN_UNIT_TESTS
/** Run hardware-independent receiver mapping tests. */
void cb_receiver_mapping_run_unit_tests(void);
#endif

#ifdef __cplusplus
}
#endif

#endif