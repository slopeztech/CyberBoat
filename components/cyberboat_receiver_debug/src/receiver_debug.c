/** @file receiver_debug.c
 *  @brief Toggleable console logging for raw and mapped receiver data.
 */

#include "cyberboat/receiver_debug.h"

#include "esp_log.h"

#include <inttypes.h>

static const char *TAG = "receiver";

esp_err_t cb_receiver_debug_get_default_config(cb_receiver_debug_config_t *config)
{
    if (config == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

#ifdef CONFIG_CYBERBOAT_RECEIVER_CONSOLE_LOG
    config->enabled = true;
#else
    config->enabled = false;
#endif
    config->interval_ms = (uint32_t)CONFIG_CYBERBOAT_RECEIVER_CONSOLE_LOG_INTERVAL_MS;

    return ESP_OK;
}

esp_err_t cb_receiver_debug_validate_config(const cb_receiver_debug_config_t *config)
{
    if (config == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if ((config->interval_ms == 0U) || (config->interval_ms > 60000U))
    {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

esp_err_t cb_receiver_debug_should_log(
    const cb_receiver_debug_config_t *config,
    cb_receiver_debug_state_t *state,
    uint32_t now_ms,
    bool *out_should_log)
{
    esp_err_t result;

    if ((config == NULL) || (state == NULL) || (out_should_log == NULL))
    {
        return ESP_ERR_INVALID_ARG;
    }

    result = cb_receiver_debug_validate_config(config);
    if (result != ESP_OK)
    {
        return result;
    }

    if (!config->enabled)
    {
        *out_should_log = false;
        return ESP_OK;
    }

    if (!state->logged_once)
    {
        state->logged_once = true;
        state->last_log_ms = now_ms;
        *out_should_log = true;
        return ESP_OK;
    }

    if ((uint32_t)(now_ms - state->last_log_ms) >= config->interval_ms)
    {
        state->last_log_ms = now_ms;
        *out_should_log = true;
    }
    else
    {
        *out_should_log = false;
    }

    return ESP_OK;
}

esp_err_t cb_receiver_debug_log_raw_frame(const cb_pwm_receiver_diagnostics_t *diagnostics)
{
    uint8_t channel_index;

    if (diagnostics == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Raw channels (valid_pulses=%" PRIu32 ", invalid_pulses=%" PRIu32 "):",
             diagnostics->valid_pulse_count, diagnostics->invalid_pulse_count);
    for (channel_index = 0U; channel_index < diagnostics->config.channel_count; ++channel_index)
    {
        const bool channel_valid =
            (diagnostics->valid_channels_mask & (uint16_t)(1U << channel_index)) != 0U;

        ESP_LOGI(TAG, "  CH%u gpio=%" PRId32 " pulse=%u us age=%" PRIu32 " ms %s",
                 (unsigned int)(channel_index + 1U),
                 diagnostics->config.channel_gpio[channel_index],
                 (unsigned int)diagnostics->pulse_width_us[channel_index],
                 diagnostics->channel_age_ms[channel_index],
                 channel_valid ? "valid" : "stale");
    }

    return ESP_OK;
}

esp_err_t cb_receiver_debug_log_mapped_command(const cb_receiver_debug_command_t *command)
{
    if (command == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG,
             "Drive command: motor_left=%d motor_right=%d auxiliary_servo=%d rearm_safe=%s valid_for=%" PRIu32 "ms",
             (int)command->motor_left,
             (int)command->motor_right,
             (int)command->auxiliary_servo,
             command->rearm_safe ? "yes" : "no",
             command->valid_for_ms);

    return ESP_OK;
}
