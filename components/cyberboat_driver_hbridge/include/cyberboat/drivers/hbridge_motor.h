#ifndef CYBERBOAT_DRIVER_HBRIDGE_MOTOR_H
#define CYBERBOAT_DRIVER_HBRIDGE_MOTOR_H

/** @file hbridge_motor.h
 *  @brief Configurable LEDC driver for one or two brushed DC H-bridge motors.
 */

#include <stdbool.h>
#include <stdint.h>

#include "cyberboat/hal/motor.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CB_HBRIDGE_PWM_DUTY_MAX 1023U

/** Pin and PWM configuration for a bridge with two logic inputs per motor.
 *
 * The driver uses PWM on one direction input and holds the other input low.
 * Board enable pins (such as L298 ENA/ENB) are outside this interface and must
 * be configured according to the selected module's datasheet.
 */
typedef struct
{
    uint8_t motor_count; /**< Must match CONFIG_CYBERBOAT_MOTOR_COUNT. */
    int32_t input_gpio[CB_MAX_MOTORS][2]; /**< Forward/reverse bridge inputs per motor. */
    bool reversed[CB_MAX_MOTORS]; /**< Invert the logical direction of a motor. */
    uint32_t pwm_frequency_hz; /**< PWM frequency shared by all bridge inputs. */
    uint8_t ledc_timer; /**< LEDC timer used by the bridge. */
    uint8_t ledc_channel_base; /**< First of two LEDC channels per motor. */
} cb_hbridge_motor_config_t;

/** PWM duty values for the two logic inputs of one H-bridge. */
typedef struct
{
    uint32_t input_1_duty; /**< Duty for bridge input 1. */
    uint32_t input_2_duty; /**< Duty for bridge input 2. */
} cb_hbridge_motor_pwm_t;

/** Caller-owned storage for one statically allocated H-bridge driver. */
typedef struct
{
    cb_hbridge_motor_config_t config; /**< Validated configuration copied at bind time. */
    bool initialized; /**< True after every configured LEDC channel is initialized. */
    bool armed; /**< False after initialization and after every emergency stop. */
    uint8_t configured_channels; /**< Number of LEDC channels successfully initialized. */
    cb_axis_t last_output[CB_MAX_MOTORS]; /**< Last accepted software command per motor. */
    esp_err_t last_error; /**< Result of the most recent driver operation. */
} cb_hbridge_motor_driver_t;

/** Snapshot of software-visible bridge state; it is not motor feedback. */
typedef struct
{
    cb_hbridge_motor_config_t config; /**< Active pin and PWM configuration. */
    bool initialized; /**< LEDC initialization status. */
    bool armed; /**< Driver arming state. */
    uint8_t configured_channels; /**< Successfully initialized LEDC channels. */
    cb_axis_t last_output[CB_MAX_MOTORS]; /**< Last accepted commands. */
    esp_err_t last_error; /**< Last returned ESP-IDF status. */
} cb_hbridge_motor_diagnostics_t;

/** Read compile-time Kconfig settings into a driver configuration.
 * @param[out] config Destination configuration.
 * @return ESP_OK on success, or ESP_ERR_INVALID_ARG if config is null.
 */
esp_err_t cb_hbridge_motor_get_default_config(cb_hbridge_motor_config_t *config);

/** Validate motor count, GPIO uniqueness, peripheral conflicts, and LEDC allocation.
 * @param[in] config Configuration to validate.
 * @return ESP_OK when valid, or ESP_ERR_INVALID_ARG otherwise.
 */
esp_err_t cb_hbridge_motor_validate_config(const cb_hbridge_motor_config_t *config);

/** Convert a signed normalized command into mutually exclusive bridge PWM duties.
 * @param[in] output Command in CB_AXIS_MIN..CB_AXIS_MAX.
 * @param[in] reversed True to invert forward and reverse.
 * @param[out] pwm Destination duties; zero command coasts with both inputs low.
 * @return ESP_OK on success, or ESP_ERR_INVALID_ARG for an invalid range/pointer.
 */
esp_err_t cb_hbridge_motor_map_output(
    cb_axis_t output,
    bool reversed,
    cb_hbridge_motor_pwm_t *pwm);

/** Bind caller-owned state to the generic motor HAL.
 * @param[out] driver Static driver storage; it must outlive all HAL callbacks.
 * @param[in] config Configuration copied into driver state.
 * @param[out] motor_hal HAL callback table to initialize.
 * @return ESP_OK on success, or ESP_ERR_INVALID_ARG for invalid input/configuration.
 */
esp_err_t cb_hbridge_motor_bind(
    cb_hbridge_motor_driver_t *driver,
    const cb_hbridge_motor_config_t *config,
    cb_motor_hal_t *motor_hal);

/** Copy current software-visible driver state for diagnostics.
 *
 * This is command/state feedback only; it does not measure motor rotation or
 * bridge output voltage.
 *
 * @param[in] driver Driver state to inspect.
 * @param[out] diagnostics Destination snapshot.
 * @return ESP_OK on success, or ESP_ERR_INVALID_ARG for a null argument.
 */
esp_err_t cb_hbridge_motor_get_diagnostics(
    const cb_hbridge_motor_driver_t *driver,
    cb_hbridge_motor_diagnostics_t *diagnostics);

#if CONFIG_CYBERBOAT_RUN_UNIT_TESTS
/** Run the driver's hardware-independent Unity tests. */
void cb_hbridge_motor_run_unit_tests(void);
#endif

#ifdef __cplusplus
}
#endif

#endif