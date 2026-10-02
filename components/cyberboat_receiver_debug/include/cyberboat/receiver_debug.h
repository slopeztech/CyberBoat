#ifndef CYBERBOAT_RECEIVER_DEBUG_H
#define CYBERBOAT_RECEIVER_DEBUG_H

/** @file receiver_debug.h
 *  @brief Toggleable console logging for raw and mapped receiver data.
 */

#include <stdbool.h>
#include <stdint.h>

#include "cyberboat/drivers/pwm_receiver.h"
#include "cyberboat/types.h"
#include "esp_err.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Build-time console logging behavior, loaded from Kconfig. */
typedef struct
{
    bool enabled; /**< Master on/off switch for console logging. */
    uint32_t interval_ms; /**< Minimum time between log lines. */
} cb_receiver_debug_config_t;

/** Caller-owned rate-limiting state for one logging stream. */
typedef struct
{
    uint32_t last_log_ms; /**< Timestamp of the last emitted log line. */
    bool logged_once; /**< True after the first log line has been emitted. */
} cb_receiver_debug_state_t;

/** Minimal command snapshot used only for console output. */
typedef struct
{
    cb_axis_t motor_left; /**< Signed left motor target. */
    cb_axis_t motor_right; /**< Signed right motor target. */
    cb_axis_t auxiliary_servo; /**< Auxiliary-servo target. */
    bool rearm_safe; /**< Whether neutral controls permit safe rearming. */
    uint32_t valid_for_ms; /**< Remaining accepted frame freshness. */
} cb_receiver_debug_command_t;

/** Load the console logging toggle and interval from Kconfig.
 * @param[out] config Destination configuration.
 * @return ESP_OK on success, or ESP_ERR_INVALID_ARG for a null pointer.
 */
esp_err_t cb_receiver_debug_get_default_config(cb_receiver_debug_config_t *config);

/** Validate the logging interval range.
 * @param[in] config Configuration to validate.
 * @return ESP_OK when valid, or ESP_ERR_INVALID_ARG otherwise.
 */
esp_err_t cb_receiver_debug_validate_config(const cb_receiver_debug_config_t *config);

/** Decide whether enough time has elapsed to emit another log line.
 *
 * The first call after state is zero-initialized always logs immediately,
 * regardless of interval_ms, so bring-up data appears as soon as logging
 * starts. Subsequent calls respect interval_ms and tolerate 32-bit
 * millisecond tick rollover through unsigned subtraction.
 *
 * @param[in] config Logging configuration.
 * @param[in,out] state Rate-limiting state; persists across calls.
 * @param[in] now_ms Caller-supplied monotonic time in milliseconds.
 * @param[out] out_should_log Destination decision.
 * @return ESP_OK on success, or ESP_ERR_INVALID_ARG for invalid input.
 */
esp_err_t cb_receiver_debug_should_log(
    const cb_receiver_debug_config_t *config,
    cb_receiver_debug_state_t *state,
    uint32_t now_ms,
    bool *out_should_log);

/** Print one line per configured channel: GPIO, pulse width, age, and freshness.
 * @param[in] diagnostics Receiver driver diagnostics snapshot.
 * @return ESP_OK on success, or ESP_ERR_INVALID_ARG for a null pointer.
 */
esp_err_t cb_receiver_debug_log_raw_frame(const cb_pwm_receiver_diagnostics_t *diagnostics);

/** Print the mapped motor outputs, servo target, and safe-rearm status.
 * @param[in] command Mapped drive command.
 * @return ESP_OK on success, or ESP_ERR_INVALID_ARG for a null pointer.
 */
esp_err_t cb_receiver_debug_log_mapped_command(const cb_receiver_debug_command_t *command);

#if CONFIG_CYBERBOAT_RUN_UNIT_TESTS
/** Run hardware-independent tests for the logging rate limiter and formatters. */
void cb_receiver_debug_run_unit_tests(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
