#ifndef CYBERBOAT_HARDWARE_TEST_H
#define CYBERBOAT_HARDWARE_TEST_H

/** @file hardware_test.h
 *  @brief Explicit, non-propulsive diagnostics for physical peripherals.
 */

#include <stdint.h>

#include "cyberboat/drivers/servo_pwm.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Servo output diagnostic behavior. */
typedef enum
{
    CB_SERVO_TEST_MODE_CENTER = 0, /**< Hold a stable center pulse for scope measurement. */
    CB_SERVO_TEST_MODE_SMALL_STEPS, /**< Step through center and +/-5% of calibrated travel. */
} cb_servo_test_mode_t;

/** Return a bounded normalized position for a step in a servo diagnostic mode.
 * @param[in] mode Selected diagnostic mode.
 * @param[in] step_index Zero-based position within the mode sequence.
 * @param[out] position Normalized servo position.
 * @return ESP_OK on success or ESP_ERR_INVALID_ARG for an invalid mode/index/output.
 */
esp_err_t cb_hardware_test_get_servo_step(
    cb_servo_test_mode_t mode,
    uint8_t step_index,
    cb_axis_t *position);

/** Initialize the selected servo role and start its diagnostic behavior.
 *
 * Requires CONFIG_CYBERBOAT_ACTUATORS_ENABLED and a vessel mode that includes
 * the requested servo role. This test never initializes propulsion outputs.
 *
 * @param[in] role Rudder or auxiliary servo role.
 * @param[in] mode Stable center pulse or small position steps.
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED when output/role is disabled,
 *         or another ESP-IDF error if configuration or initialization fails.
 */
esp_err_t cb_hardware_test_start_servo_test(
    cb_servo_role_t role,
    cb_servo_test_mode_t mode);

/** Poll and advance an active servo test; call from a periodic application loop. */
esp_err_t cb_hardware_test_poll_servo_test(void);

/** Initialize the configured PWM receiver for live pulse/freshness reporting. */
esp_err_t cb_hardware_test_start_pwm_receiver_test(void);

/** Poll and log PWM receiver samples; call from a periodic application loop. */
esp_err_t cb_hardware_test_poll_pwm_receiver_test(void);

/** Initialize a two-motor H-bridge and begin a bounded D0-D3 input test.
 *
 * Requires the hardware-test profile, two configured motors, and
 * CONFIG_CYBERBOAT_ACTUATORS_ENABLED. Applies 10% duty for 300 ms per direction,
 * with 1 s stops between pulses; disarms after one complete sequence.
 *
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED if the profile or motor count
 *         is incompatible, or an ESP-IDF error if initialization fails.
 */
esp_err_t cb_hardware_test_start_hbridge_test(void);

/** Advance the bounded H-bridge test sequence; call from a periodic loop. */
esp_err_t cb_hardware_test_poll_hbridge_test(void);

#if CONFIG_CYBERBOAT_RUN_UNIT_TESTS
/** Run hardware-independent tests for diagnostic mode sequencing. */
void cb_hardware_test_run_unit_tests(void);
#endif

#ifdef __cplusplus
}
#endif

#endif