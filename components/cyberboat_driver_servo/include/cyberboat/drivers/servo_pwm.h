#ifndef CYBERBOAT_DRIVER_SERVO_PWM_H
#define CYBERBOAT_DRIVER_SERVO_PWM_H

/** @file servo_pwm.h
 *  @brief Configurable LEDC PWM driver for hobby servos.
 */

#include <stdbool.h>
#include <stdint.h>

#include "cyberboat/hal/servo.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Servo role selected by the compile-time vessel mode. */
typedef enum
{
    CB_SERVO_ROLE_RUDDER = 0,
    CB_SERVO_ROLE_AUXILIARY,
} cb_servo_role_t;

/** Build-time or application-supplied PWM and calibration parameters. */
typedef struct
{
    cb_servo_role_t role; /**< Rudder or independent auxiliary actuator role. */
    int32_t gpio_num; /**< GPIO used for the signal output. */
    uint32_t frequency_hz; /**< PWM frequency in hertz. */
    uint32_t min_pulse_width_us; /**< Pulse at CB_AXIS_MIN. */
    uint32_t center_pulse_width_us; /**< Pulse at neutral position zero. */
    uint32_t max_pulse_width_us; /**< Pulse at CB_AXIS_MAX. */
    uint8_t ledc_timer; /**< LEDC timer index. */
    uint8_t ledc_channel; /**< LEDC channel index. */
} cb_servo_pwm_config_t;

/** Snapshot of software-visible driver state for diagnostics. */
typedef struct
{
    cb_servo_pwm_config_t config; /**< Active configuration. */
    bool initialized; /**< True after LEDC timer and channel initialization succeeds. */
    uint32_t last_pulse_width_us; /**< Last pulse accepted by LEDC. */
    esp_err_t last_error; /**< Last operation result; ESP_OK means the latest call succeeded. */
} cb_servo_pwm_diagnostics_t;

/** Caller-owned storage for one statically allocated servo driver instance. */
typedef struct
{
    cb_servo_pwm_config_t config; /**< Validated configuration copied at bind time. */
    bool initialized; /**< LEDC initialization state. */
    uint32_t last_pulse_width_us; /**< Last pulse accepted by LEDC. */
    esp_err_t last_error; /**< Last operation result. */
} cb_servo_pwm_driver_t;

/** Fill a role-specific configuration from ESP-IDF Kconfig.
 * @param[in] role Servo role selected by the vessel mode.
 * @param[out] config Destination configuration.
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED if the role is not selected,
 *         or ESP_ERR_INVALID_ARG for an invalid pointer/role.
 */
esp_err_t cb_servo_pwm_get_default_config(
    cb_servo_role_t role,
    cb_servo_pwm_config_t *config);

/** Validate GPIO, LEDC resources, PWM frequency, and calibration ordering.
 * @param[in] config Configuration to validate.
 * @return ESP_OK when valid, ESP_ERR_INVALID_ARG otherwise.
 */
esp_err_t cb_servo_pwm_validate_config(const cb_servo_pwm_config_t *config);

/** Convert a normalized position to its calibrated PWM pulse width.
 * @param[in] config Valid servo configuration.
 * @param[in] position Position in CB_AXIS_MIN..CB_AXIS_MAX.
 * @param[out] pulse_width_us Destination pulse width in microseconds.
 * @return ESP_OK on success, or ESP_ERR_INVALID_ARG for invalid input/configuration.
 */
esp_err_t cb_servo_pwm_position_to_pulse_width_us(
    const cb_servo_pwm_config_t *config,
    cb_axis_t position,
    uint32_t *pulse_width_us);

/** Bind caller-owned driver state to the generic servo HAL.
 * @param[out] driver Driver state storage; must outlive the HAL callbacks.
 * @param[in] config Configuration copied into driver state.
 * @param[out] servo_hal HAL operations to initialize.
 * @return ESP_OK on success, or ESP_ERR_INVALID_ARG for invalid input/configuration.
 */
esp_err_t cb_servo_pwm_bind(
    cb_servo_pwm_driver_t *driver,
    const cb_servo_pwm_config_t *config,
    cb_servo_hal_t *servo_hal);

/** Copy the current software-visible state into a diagnostic snapshot.
 *
 * This reports commanded state, not measured shaft position or electrical
 * waveform feedback.
 *
 * @param[in] driver Driver state to inspect.
 * @param[out] diagnostics Destination snapshot.
 * @return ESP_OK on success, or ESP_ERR_INVALID_ARG for a null argument.
 */
esp_err_t cb_servo_pwm_get_diagnostics(
    const cb_servo_pwm_driver_t *driver,
    cb_servo_pwm_diagnostics_t *diagnostics);

#if CONFIG_CYBERBOAT_RUN_UNIT_TESTS
/** Run hardware-independent and ESP32 GPIO validation tests for the servo driver. */
void cb_servo_pwm_run_unit_tests(void);
#endif

#ifdef __cplusplus
}
#endif

#endif