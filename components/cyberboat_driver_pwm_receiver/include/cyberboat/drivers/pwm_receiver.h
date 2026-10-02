#ifndef CYBERBOAT_DRIVER_PWM_RECEIVER_H
#define CYBERBOAT_DRIVER_PWM_RECEIVER_H

/** @file pwm_receiver.h
 *  @brief Generic GPIO pulse-capture driver for hobby RC PWM receivers.
 */

#include <stdbool.h>
#include <stdint.h>

#include "cyberboat/hal/receiver.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Build-time GPIO and pulse validation settings for a parallel PWM receiver. */
typedef struct
{
    uint8_t channel_count; /**< Number of independent PWM output pins. */
    int32_t channel_gpio[CB_MAX_RECEIVER_CHANNELS]; /**< GPIO assigned to each zero-based channel. */
    uint16_t min_pulse_width_us; /**< Smallest accepted high pulse. */
    uint16_t max_pulse_width_us; /**< Largest accepted high pulse. */
    uint16_t timeout_ms; /**< Maximum age for a channel to remain valid. */
} cb_pwm_receiver_config_t;

/** Per-channel edge capture state shared with the GPIO ISR. */
typedef struct
{
    int64_t rising_edge_us; /**< Timestamp of the most recent rising edge. */
    int64_t last_valid_edge_us; /**< Timestamp of the most recent valid pulse. */
    uint16_t pulse_width_us; /**< Most recent valid pulse width. */
} cb_pwm_receiver_channel_state_t;

/** Per-pin context passed to the GPIO interrupt handler. */
typedef struct
{
    void *driver; /**< Owning driver instance. */
    uint8_t channel_index; /**< Zero-based receiver channel index. */
} cb_pwm_receiver_isr_context_t;

/** Caller-owned storage for one statically allocated receiver driver. */
typedef struct
{
    cb_pwm_receiver_config_t config; /**< Validated pin and pulse configuration. */
    bool initialized; /**< True after all configured GPIO handlers are installed. */
    uint8_t configured_channel_count; /**< Number of successfully installed handlers. */
    uint32_t valid_pulse_count; /**< Total accepted pulses, modulo uint32_t range. */
    uint32_t invalid_pulse_count; /**< Pulses rejected for width or edge-order errors. */
    esp_err_t last_error; /**< Result of the most recent driver operation. */
    portMUX_TYPE mux; /**< Protects shared channel and diagnostic state. */
    cb_pwm_receiver_channel_state_t channel[CB_MAX_RECEIVER_CHANNELS]; /**< ISR capture state. */
    cb_pwm_receiver_isr_context_t isr_context[CB_MAX_RECEIVER_CHANNELS]; /**< Stable ISR arguments. */
} cb_pwm_receiver_driver_t;

/** Snapshot of receiver diagnostics and most recent channel samples. */
typedef struct
{
    cb_pwm_receiver_config_t config; /**< Active pin and timing configuration. */
    bool initialized; /**< GPIO capture initialization status. */
    uint16_t valid_channels_mask; /**< Fresh channel bit mask at snapshot time. */
    uint16_t pulse_width_us[CB_MAX_RECEIVER_CHANNELS]; /**< Last pulse per channel. */
    uint32_t channel_age_ms[CB_MAX_RECEIVER_CHANNELS]; /**< Age of each last valid pulse. */
    uint32_t valid_pulse_count; /**< Accepted pulse count. */
    uint32_t invalid_pulse_count; /**< Rejected pulse count. */
    esp_err_t last_error; /**< Last returned ESP-IDF status. */
} cb_pwm_receiver_diagnostics_t;

/** Load compile-time Kconfig values.
 * @param[out] config Destination configuration.
 * @return ESP_OK on success, or ESP_ERR_INVALID_ARG for a null pointer.
 */
esp_err_t cb_pwm_receiver_get_default_config(cb_pwm_receiver_config_t *config);

/** Validate channel count, pulse limits, GPIO capability, and pin conflicts.
 * @param[in] config Receiver configuration to validate.
 * @return ESP_OK when valid, ESP_ERR_INVALID_ARG otherwise.
 */
esp_err_t cb_pwm_receiver_validate_config(const cb_pwm_receiver_config_t *config);

/** Bind caller-owned driver state to the generic raw receiver HAL.
 * @param[out] driver Driver state; it must outlive HAL callbacks.
 * @param[in] config Configuration copied into driver state.
 * @param[out] receiver_hal HAL callback table to initialize.
 * @return ESP_OK on success, or ESP_ERR_INVALID_ARG for invalid input/configuration.
 */
esp_err_t cb_pwm_receiver_bind(
    cb_pwm_receiver_driver_t *driver,
    const cb_pwm_receiver_config_t *config,
    cb_receiver_hal_t *receiver_hal);

/** Copy current channel values, ages, and counters into a diagnostic snapshot.
 * @param[in] driver Initialized or bound driver state.
 * @param[out] diagnostics Destination snapshot.
 * @return ESP_OK on success, or ESP_ERR_INVALID_ARG for a null argument.
 */
esp_err_t cb_pwm_receiver_get_diagnostics(
    cb_pwm_receiver_driver_t *driver,
    cb_pwm_receiver_diagnostics_t *diagnostics);

#if CONFIG_CYBERBOAT_RUN_UNIT_TESTS
/** Run hardware-independent and ESP32 GPIO validation tests. */
void cb_pwm_receiver_run_unit_tests(void);
#endif

#ifdef __cplusplus
}
#endif

#endif