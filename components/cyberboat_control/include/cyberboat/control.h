#ifndef CYBERBOAT_CONTROL_H
#define CYBERBOAT_CONTROL_H

/** @file control.h
 *  @brief Public API for deterministic pilot-command shaping.
 */

#include "cyberboat/types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Compile-time vessel behavior profile selected by CYBERBOAT_VESSEL_MODE. */
typedef enum
{
	CB_VESSEL_MODE_SINGLE_MOTOR_ONLY = 0,
	CB_VESSEL_MODE_SINGLE_MOTOR_RUDDER,
	CB_VESSEL_MODE_SINGLE_MOTOR_AUX_SERVO,
	CB_VESSEL_MODE_SINGLE_MOTOR_RUDDER_AUX,
	CB_VESSEL_MODE_TWIN_MOTOR_ONLY,
	CB_VESSEL_MODE_TWIN_MOTOR_RUDDER,
	CB_VESSEL_MODE_TWIN_MOTOR_AUX_SERVO,
	CB_VESSEL_MODE_TWIN_MOTOR_RUDDER_AUX,
} cb_vessel_mode_id_t;

/** Resolved immutable capabilities for the selected compile-time mode. */
typedef struct
{
	cb_vessel_mode_id_t id; /**< Selected named Kconfig mode. */
	const char *name; /**< Human-readable mode name. */
	uint8_t motor_count; /**< Number of configured propulsion motors. */
	bool rudder_servo_enabled; /**< Steering is routed to a rudder servo. */
	bool auxiliary_servo_enabled; /**< An independent auxiliary servo role is selected. */
} cb_vessel_mode_config_t;

/** Resolve the selected Kconfig behavior mode into its capabilities.
 * @param[out] mode Destination mode descriptor.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for a null pointer.
 */
esp_err_t cb_control_get_vessel_mode(cb_vessel_mode_config_t *mode);

#if CONFIG_CYBERBOAT_RUN_UNIT_TESTS
/** Run tests for the selected vessel-mode descriptor. */
void cb_control_run_unit_tests(void);
#endif

/** Mix a pilot command for the motor count selected in Kconfig.
 *
 * For one motor, throttle is copied to motor zero and motor one is set to zero.
 * For two motors, throttle and steering are mixed and proportionally scaled
 * to preserve the output range.
 *
 * @param[in] command Non-null normalized pilot command.
 * @param[out] outputs Non-null destination for normalized motor commands.
 * @return ESP_OK on success, or ESP_ERR_INVALID_ARG for a null pointer.
 */
esp_err_t cb_control_mix_motors(
	const cb_pilot_command_t *command,
	cb_motor_outputs_t *outputs);

#ifdef __cplusplus
}
#endif

#endif