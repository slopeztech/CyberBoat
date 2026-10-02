/** @file receiver_mapping.c
 *  @brief Convert fresh receiver pulses into transport-independent controls.
 */

#include "cyberboat/receiver_mapping.h"

#include "sdkconfig.h"

static esp_err_t cb_receiver_map_axis(
    uint16_t pulse_width_us,
    const cb_receiver_axis_calibration_t *calibration,
    uint16_t deadband_us,
    bool bidirectional,
    bool reversed,
    cb_axis_t *axis);

static esp_err_t cb_receiver_require_fresh_channel(
    const cb_receiver_raw_frame_t *raw,
    uint8_t channel,
    uint16_t timeout_ms,
    uint32_t *remaining_ms);

esp_err_t cb_receiver_mapping_get_default_config(cb_receiver_mapping_config_t *config)
{
    if (config == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    config->throttle_channel = (uint8_t)CONFIG_CYBERBOAT_RC_THROTTLE_CHANNEL;
    config->steering_channel = (uint8_t)CONFIG_CYBERBOAT_RC_STEERING_CHANNEL;
    config->auxiliary_servo_channel = (uint8_t)CONFIG_CYBERBOAT_RC_AUX_SERVO_CHANNEL;
    config->arm_channel = (uint8_t)CONFIG_CYBERBOAT_RC_ARM_CHANNEL;
#if CONFIG_CYBERBOAT_MOTOR_COUNT == 2 || defined(CONFIG_CYBERBOAT_RUDDER_SERVO_ENABLED)
    config->steering_enabled = true;
#else
    config->steering_enabled = false;
#endif
#ifdef CONFIG_CYBERBOAT_AUX_SERVO_ENABLED
    config->auxiliary_servo_enabled = true;
#else
    config->auxiliary_servo_enabled = false;
#endif
#ifdef CONFIG_CYBERBOAT_RC_ARM_ENABLED
    config->arm_enabled = true;
#else
    config->arm_enabled = false;
#endif
#ifdef CONFIG_CYBERBOAT_RC_THROTTLE_REVERSED
    config->throttle_reversed = true;
#else
    config->throttle_reversed = false;
#endif
#ifdef CONFIG_CYBERBOAT_RC_STEERING_REVERSED
    config->steering_reversed = true;
#else
    config->steering_reversed = false;
#endif
#ifdef CONFIG_CYBERBOAT_RC_AUX_SERVO_REVERSED
    config->auxiliary_servo_reversed = true;
#else
    config->auxiliary_servo_reversed = false;
#endif
#ifdef CONFIG_CYBERBOAT_RC_THROTTLE_BIDIRECTIONAL
    config->throttle_bidirectional = true;
#else
    config->throttle_bidirectional = false;
#endif
    config->arm_threshold_us = (uint16_t)CONFIG_CYBERBOAT_RC_ARM_THRESHOLD_US;
    config->deadband_us = (uint16_t)CONFIG_CYBERBOAT_RC_DEADBAND_US;
    config->frame_timeout_ms = (uint16_t)CONFIG_CYBERBOAT_PWM_RECEIVER_TIMEOUT_MS;
    config->throttle.min_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_THROTTLE_MIN_US;
    config->throttle.center_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_THROTTLE_CENTER_US;
    config->throttle.max_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_THROTTLE_MAX_US;
    config->steering.min_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_STEERING_MIN_US;
    config->steering.center_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_STEERING_CENTER_US;
    config->steering.max_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_STEERING_MAX_US;
    config->auxiliary_servo.min_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_AUX_SERVO_MIN_US;
    config->auxiliary_servo.center_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_AUX_SERVO_CENTER_US;
    config->auxiliary_servo.max_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_AUX_SERVO_MAX_US;

    return ESP_OK;
}

esp_err_t cb_receiver_mapping_validate_config(const cb_receiver_mapping_config_t *config)
{
    if (config == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if ((config->throttle_channel >= CB_MAX_RECEIVER_CHANNELS) ||
        (config->steering_enabled &&
         ((config->steering_channel >= CB_MAX_RECEIVER_CHANNELS) ||
          (config->throttle_channel == config->steering_channel))) ||
        (config->frame_timeout_ms < 20U) || (config->frame_timeout_ms > 1000U) ||
        (config->arm_enabled &&
         ((config->arm_channel >= CB_MAX_RECEIVER_CHANNELS) ||
          (config->arm_channel == config->throttle_channel) ||
          (config->steering_enabled &&
           (config->arm_channel == config->steering_channel)) ||
          (config->auxiliary_servo_enabled &&
           (config->arm_channel == config->auxiliary_servo_channel)))) ||
        (config->auxiliary_servo_enabled &&
         ((config->auxiliary_servo_channel >= CB_MAX_RECEIVER_CHANNELS) ||
          (config->auxiliary_servo_channel == config->throttle_channel) ||
          (config->steering_enabled &&
           (config->auxiliary_servo_channel == config->steering_channel)))) ||
        (config->throttle.min_pulse_us >= config->throttle.center_pulse_us) ||
        (config->throttle.center_pulse_us >= config->throttle.max_pulse_us) ||
        (config->steering_enabled &&
         ((config->steering.min_pulse_us >= config->steering.center_pulse_us) ||
          (config->steering.center_pulse_us >= config->steering.max_pulse_us) ||
          (config->deadband_us >=
           (uint16_t)(config->steering.center_pulse_us - config->steering.min_pulse_us)) ||
          (config->deadband_us >=
           (uint16_t)(config->steering.max_pulse_us - config->steering.center_pulse_us)))))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!config->throttle_bidirectional &&
        (config->deadband_us >=
         (uint16_t)(config->throttle.max_pulse_us - config->throttle.min_pulse_us)))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (config->throttle_bidirectional &&
        ((config->deadband_us >=
          (uint16_t)(config->throttle.center_pulse_us - config->throttle.min_pulse_us)) ||
         (config->deadband_us >=
          (uint16_t)(config->throttle.max_pulse_us - config->throttle.center_pulse_us))))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (config->auxiliary_servo_enabled &&
        ((config->auxiliary_servo.min_pulse_us >= config->auxiliary_servo.center_pulse_us) ||
         (config->auxiliary_servo.center_pulse_us >= config->auxiliary_servo.max_pulse_us) ||
         (config->deadband_us >=
          (uint16_t)(config->auxiliary_servo.center_pulse_us - config->auxiliary_servo.min_pulse_us)) ||
         (config->deadband_us >=
          (uint16_t)(config->auxiliary_servo.max_pulse_us - config->auxiliary_servo.center_pulse_us))))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (config->arm_enabled &&
        ((config->arm_threshold_us < CONFIG_CYBERBOAT_PWM_RECEIVER_MIN_PULSE_US) ||
         (config->arm_threshold_us > CONFIG_CYBERBOAT_PWM_RECEIVER_MAX_PULSE_US)))
    {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

esp_err_t cb_control_map_receiver(
    const cb_receiver_raw_frame_t *raw,
    const cb_receiver_mapping_config_t *config,
    cb_receiver_frame_t *command)
{
    uint32_t remaining_ms;
    uint32_t minimum_remaining_ms;
    uint16_t arm_pulse_width_us = 0U;
    esp_err_t result;

    if ((raw == NULL) || (config == NULL) || (command == NULL))
    {
        return ESP_ERR_INVALID_ARG;
    }

    command->command.throttle = 0;
    command->command.steering = 0;
    command->auxiliary_servo = 0;
    command->arm_requested = false;
    command->valid_for_ms = 0U;

    result = cb_receiver_mapping_validate_config(config);
    if (result != ESP_OK)
    {
        return result;
    }

    if ((raw->channel_count == 0U) || (raw->channel_count > CB_MAX_RECEIVER_CHANNELS) ||
        (config->throttle_channel >= raw->channel_count) ||
        (config->steering_enabled && (config->steering_channel >= raw->channel_count)) ||
        (config->auxiliary_servo_enabled &&
         (config->auxiliary_servo_channel >= raw->channel_count)) ||
        (config->arm_enabled && (config->arm_channel >= raw->channel_count)))
    {
        return ESP_ERR_INVALID_ARG;
    }

    result = cb_receiver_require_fresh_channel(
        raw, config->throttle_channel, config->frame_timeout_ms, &minimum_remaining_ms);
    if (result != ESP_OK)
    {
        return result;
    }

    result = cb_receiver_map_axis(
        raw->pulse_width_us[config->throttle_channel],
        &config->throttle,
        config->deadband_us,
        config->throttle_bidirectional,
        config->throttle_reversed,
        &command->command.throttle);
    if (result != ESP_OK)
    {
        return result;
    }

    if (config->steering_enabled)
    {
        result = cb_receiver_require_fresh_channel(
            raw, config->steering_channel, config->frame_timeout_ms, &remaining_ms);
        if (result != ESP_OK)
        {
            command->command.throttle = 0;
            return result;
        }
        if (remaining_ms < minimum_remaining_ms)
        {
            minimum_remaining_ms = remaining_ms;
        }

        result = cb_receiver_map_axis(
            raw->pulse_width_us[config->steering_channel],
            &config->steering,
            config->deadband_us,
            true,
            config->steering_reversed,
            &command->command.steering);
        if (result != ESP_OK)
        {
            command->command.throttle = 0;
            return result;
        }
    }

    if (config->auxiliary_servo_enabled)
    {
        result = cb_receiver_require_fresh_channel(
            raw,
            config->auxiliary_servo_channel,
            config->frame_timeout_ms,
            &remaining_ms);
        if (result != ESP_OK)
        {
            command->command.throttle = 0;
            command->command.steering = 0;
            return result;
        }
        if (remaining_ms < minimum_remaining_ms)
        {
            minimum_remaining_ms = remaining_ms;
        }

        result = cb_receiver_map_axis(
            raw->pulse_width_us[config->auxiliary_servo_channel],
            &config->auxiliary_servo,
            config->deadband_us,
            true,
            config->auxiliary_servo_reversed,
            &command->auxiliary_servo);
        if (result != ESP_OK)
        {
            command->command.throttle = 0;
            command->command.steering = 0;
            return result;
        }
    }

    if (config->arm_enabled)
    {
        result = cb_receiver_require_fresh_channel(
            raw, config->arm_channel, config->frame_timeout_ms, &remaining_ms);
        if (result != ESP_OK)
        {
            command->command.throttle = 0;
            command->command.steering = 0;
            command->auxiliary_servo = 0;
            return result;
        }
        if (remaining_ms < minimum_remaining_ms)
        {
            minimum_remaining_ms = remaining_ms;
        }

        arm_pulse_width_us = raw->pulse_width_us[config->arm_channel];
        if ((arm_pulse_width_us < CONFIG_CYBERBOAT_PWM_RECEIVER_MIN_PULSE_US) ||
            (arm_pulse_width_us > CONFIG_CYBERBOAT_PWM_RECEIVER_MAX_PULSE_US))
        {
            command->command.throttle = 0;
            command->command.steering = 0;
            command->auxiliary_servo = 0;
            return ESP_ERR_INVALID_ARG;
        }
        command->arm_requested = (arm_pulse_width_us >= config->arm_threshold_us);
    }

    command->valid_for_ms = minimum_remaining_ms;

    return ESP_OK;
}

static esp_err_t cb_receiver_map_axis(
    uint16_t pulse_width_us,
    const cb_receiver_axis_calibration_t *calibration,
    uint16_t deadband_us,
    bool bidirectional,
    bool reversed,
    cb_axis_t *axis)
{
    int32_t mapped_value = 0;

    if ((calibration == NULL) || (axis == NULL) ||
        (pulse_width_us < calibration->min_pulse_us) ||
        (pulse_width_us > calibration->max_pulse_us))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!bidirectional)
    {
        const uint32_t start_pulse_us = (uint32_t)calibration->min_pulse_us + deadband_us;
        const uint32_t available_range_us = (uint32_t)calibration->max_pulse_us - start_pulse_us;

        if ((uint32_t)pulse_width_us > start_pulse_us)
        {
            const uint32_t pulse_delta_us = (uint32_t)pulse_width_us - start_pulse_us;
            mapped_value = (int32_t)((pulse_delta_us * (uint32_t)CB_AXIS_MAX) / available_range_us);
        }
    }
    else if (pulse_width_us < calibration->center_pulse_us)
    {
        const uint16_t lower_deadband_us =
            (uint16_t)(calibration->center_pulse_us - deadband_us);
        if (pulse_width_us < lower_deadband_us)
        {
            const uint32_t available_range_us =
                (uint32_t)lower_deadband_us - calibration->min_pulse_us;
            const uint32_t pulse_delta_us = (uint32_t)lower_deadband_us - pulse_width_us;
            mapped_value = -(int32_t)((pulse_delta_us * (uint32_t)CB_AXIS_MAX) / available_range_us);
        }
    }
    else
    {
        const uint16_t upper_deadband_us =
            (uint16_t)(calibration->center_pulse_us + deadband_us);
        if (pulse_width_us > upper_deadband_us)
        {
            const uint32_t available_range_us =
                (uint32_t)calibration->max_pulse_us - upper_deadband_us;
            const uint32_t pulse_delta_us = (uint32_t)pulse_width_us - upper_deadband_us;
            mapped_value = (int32_t)((pulse_delta_us * (uint32_t)CB_AXIS_MAX) / available_range_us);
        }
    }

    if (reversed)
    {
        mapped_value = -mapped_value;
    }

    *axis = (cb_axis_t)mapped_value;

    return ESP_OK;
}

static esp_err_t cb_receiver_require_fresh_channel(
    const cb_receiver_raw_frame_t *raw,
    uint8_t channel,
    uint16_t timeout_ms,
    uint32_t *remaining_ms)
{
    uint32_t age_ms;

    if ((raw == NULL) || (remaining_ms == NULL) || (channel >= raw->channel_count))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if ((raw->valid_channels_mask & (uint16_t)(1U << channel)) == 0U)
    {
        return ESP_ERR_TIMEOUT;
    }

    age_ms = raw->channel_age_ms[channel];
    if (age_ms >= timeout_ms)
    {
        return ESP_ERR_TIMEOUT;
    }

    *remaining_ms = (uint32_t)timeout_ms - age_ms;

    return ESP_OK;
}