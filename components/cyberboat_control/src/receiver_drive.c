/** @file receiver_drive.c
 *  @brief Calibrated four-channel receiver mapping for differential propulsion.
 */

#include "cyberboat/receiver_drive.h"

#include "sdkconfig.h"

static esp_err_t cb_receiver_drive_require_fresh(
    const cb_receiver_raw_frame_t *raw,
    uint8_t channel,
    uint16_t timeout_ms,
    uint32_t *remaining_ms);

static esp_err_t cb_receiver_drive_map_centered(
    uint16_t pulse_width_us,
    const cb_receiver_axis_calibration_t *calibration,
    uint16_t deadband_us,
    bool reversed,
    cb_axis_t *axis);

static int32_t cb_receiver_drive_map_throttle(
    uint16_t pulse_width_us,
    const cb_receiver_axis_calibration_t *calibration,
    uint16_t deadband_us);

esp_err_t cb_receiver_drive_get_default_config(cb_receiver_drive_config_t *config)
{
    if (config == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    config->servo_channel = (uint8_t)CONFIG_CYBERBOAT_RC_DRIVE_SERVO_CHANNEL;
    config->throttle_channel = (uint8_t)CONFIG_CYBERBOAT_RC_DRIVE_THROTTLE_CHANNEL;
    config->direction_channel = (uint8_t)CONFIG_CYBERBOAT_RC_DRIVE_DIRECTION_CHANNEL;
    config->steering_channel = (uint8_t)CONFIG_CYBERBOAT_RC_DRIVE_STEERING_CHANNEL;
    config->left_motor_index = (uint8_t)CONFIG_CYBERBOAT_RC_DRIVE_LEFT_MOTOR_INDEX;
    config->deadband_us = (uint16_t)CONFIG_CYBERBOAT_RC_DRIVE_DEADBAND_US;
    config->frame_timeout_ms = (uint16_t)CONFIG_CYBERBOAT_PWM_RECEIVER_TIMEOUT_MS;
    config->servo_travel_percent = (uint8_t)CONFIG_CYBERBOAT_RC_DRIVE_SERVO_TRAVEL_PERCENT;
#ifdef CONFIG_CYBERBOAT_RC_DRIVE_SERVO_REVERSED
    config->servo_reversed = true;
#else
    config->servo_reversed = false;
#endif
#ifdef CONFIG_CYBERBOAT_RC_DRIVE_DIRECTION_REVERSED
    config->direction_reversed = true;
#else
    config->direction_reversed = false;
#endif
#ifdef CONFIG_CYBERBOAT_RC_DRIVE_STEERING_REVERSED
    config->steering_reversed = true;
#else
    config->steering_reversed = false;
#endif
    config->servo.min_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_DRIVE_SERVO_MIN_US;
    config->servo.center_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_DRIVE_SERVO_CENTER_US;
    config->servo.max_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_DRIVE_SERVO_MAX_US;
    config->throttle.min_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_DRIVE_THROTTLE_MIN_US;
    config->throttle.center_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_DRIVE_THROTTLE_CENTER_US;
    config->throttle.max_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_DRIVE_THROTTLE_MAX_US;
    config->direction.min_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_DRIVE_DIRECTION_MIN_US;
    config->direction.center_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_DRIVE_DIRECTION_CENTER_US;
    config->direction.max_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_DRIVE_DIRECTION_MAX_US;
    config->steering.min_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_DRIVE_STEERING_MIN_US;
    config->steering.center_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_DRIVE_STEERING_CENTER_US;
    config->steering.max_pulse_us = (uint16_t)CONFIG_CYBERBOAT_RC_DRIVE_STEERING_MAX_US;

    return ESP_OK;
}

static bool cb_receiver_drive_calibration_is_valid(
    const cb_receiver_axis_calibration_t *calibration,
    uint16_t deadband_us)
{
    return (calibration->min_pulse_us < calibration->center_pulse_us) &&
           (calibration->center_pulse_us < calibration->max_pulse_us) &&
           (deadband_us < (uint16_t)(calibration->center_pulse_us - calibration->min_pulse_us)) &&
           (deadband_us < (uint16_t)(calibration->max_pulse_us - calibration->center_pulse_us));
}

esp_err_t cb_receiver_drive_validate_config(const cb_receiver_drive_config_t *config)
{
    if (config == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if ((config->servo_channel >= CB_MAX_RECEIVER_CHANNELS) ||
        (config->throttle_channel >= CB_MAX_RECEIVER_CHANNELS) ||
        (config->direction_channel >= CB_MAX_RECEIVER_CHANNELS) ||
        (config->steering_channel >= CB_MAX_RECEIVER_CHANNELS) ||
        (config->left_motor_index >= CB_MAX_MOTORS) ||
        (config->servo_channel == config->throttle_channel) ||
        (config->servo_channel == config->direction_channel) ||
        (config->servo_channel == config->steering_channel) ||
        (config->throttle_channel == config->direction_channel) ||
        (config->throttle_channel == config->steering_channel) ||
        (config->direction_channel == config->steering_channel) ||
        (config->deadband_us > 200U) ||
        (config->frame_timeout_ms < 20U) || (config->frame_timeout_ms > 1000U) ||
        (config->servo_travel_percent == 0U) || (config->servo_travel_percent > 100U) ||
        !cb_receiver_drive_calibration_is_valid(&config->servo, config->deadband_us) ||
        !cb_receiver_drive_calibration_is_valid(&config->direction, config->deadband_us) ||
        !cb_receiver_drive_calibration_is_valid(&config->steering, config->deadband_us) ||
        (config->throttle.min_pulse_us >= config->throttle.center_pulse_us) ||
        (config->throttle.center_pulse_us >= config->throttle.max_pulse_us) ||
        (config->deadband_us >=
         (uint16_t)(config->throttle.max_pulse_us - config->throttle.min_pulse_us)))
    {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t cb_receiver_drive_require_fresh(
    const cb_receiver_raw_frame_t *raw,
    uint8_t channel,
    uint16_t timeout_ms,
    uint32_t *remaining_ms)
{
    const uint16_t channel_mask = (uint16_t)(1U << channel);

    if (channel >= raw->channel_count)
    {
        return ESP_ERR_TIMEOUT;
    }

    if (((raw->valid_channels_mask & channel_mask) == 0U) ||
        (raw->channel_age_ms[channel] >= (uint32_t)timeout_ms))
    {
        return ESP_ERR_TIMEOUT;
    }

    *remaining_ms = (uint32_t)timeout_ms - raw->channel_age_ms[channel];
    return ESP_OK;
}

static esp_err_t cb_receiver_drive_map_centered(
    uint16_t pulse_width_us,
    const cb_receiver_axis_calibration_t *calibration,
    uint16_t deadband_us,
    bool reversed,
    cb_axis_t *axis)
{
    const int32_t minimum = (int32_t)calibration->min_pulse_us;
    const int32_t center = (int32_t)calibration->center_pulse_us;
    const int32_t maximum = (int32_t)calibration->max_pulse_us;
    const int32_t deadband = (int32_t)deadband_us;
    int32_t pulse = (int32_t)pulse_width_us;
    int32_t mapped = 0;

    if (axis == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (pulse < minimum)
    {
        pulse = minimum;
    }
    else if (pulse > maximum)
    {
        pulse = maximum;
    }

    if (pulse < (center - deadband))
    {
        const int32_t active_minimum = center - deadband;
        mapped = -(((active_minimum - pulse) * CB_AXIS_MAX) /
                   (active_minimum - minimum));
    }
    else if (pulse > (center + deadband))
    {
        const int32_t active_maximum = center + deadband;
        mapped = ((pulse - active_maximum) * CB_AXIS_MAX) /
                 (maximum - active_maximum);
    }

    if (reversed)
    {
        mapped = -mapped;
    }
    *axis = (cb_axis_t)mapped;
    return ESP_OK;
}

static int32_t cb_receiver_drive_map_throttle(
    uint16_t pulse_width_us,
    const cb_receiver_axis_calibration_t *calibration,
    uint16_t deadband_us)
{
    const int32_t minimum = (int32_t)calibration->min_pulse_us;
    const int32_t maximum = (int32_t)calibration->max_pulse_us;
    const int32_t active_minimum = minimum + (int32_t)deadband_us;
    int32_t pulse = (int32_t)pulse_width_us;

    if (pulse <= active_minimum)
    {
        return 0;
    }
    if (pulse >= maximum)
    {
        return CB_AXIS_MAX;
    }

    return ((pulse - active_minimum) * CB_AXIS_MAX) / (maximum - active_minimum);
}

esp_err_t cb_receiver_drive_map(
    const cb_receiver_raw_frame_t *raw,
    const cb_receiver_drive_config_t *config,
    cb_receiver_drive_command_t *command)
{
    uint32_t remaining_ms;
    uint32_t minimum_remaining_ms = UINT32_MAX;
    cb_axis_t servo_axis = 0;
    cb_axis_t direction_axis = 0;
    cb_axis_t steering_axis = 0;
    int32_t throttle;
    int32_t direction_sign;
    int32_t direction_magnitude;
    int32_t base_power;
    int32_t steering_magnitude;
    int32_t turn_reduction;
    int32_t left_motor;
    int32_t right_motor;
    esp_err_t result;

    if ((raw == NULL) || (config == NULL) || (command == NULL))
    {
        return ESP_ERR_INVALID_ARG;
    }

    command->motors.motor[0] = 0;
    command->motors.motor[1] = 0;
    command->auxiliary_servo = 0;
    command->rearm_safe = false;
    command->valid_for_ms = 0U;

    result = cb_receiver_drive_validate_config(config);
    if (result != ESP_OK)
    {
        return result;
    }
    if ((raw->channel_count == 0U) || (raw->channel_count > CB_MAX_RECEIVER_CHANNELS))
    {
        return ESP_ERR_INVALID_ARG;
    }

    {
        const uint8_t channels[4] = {
            config->servo_channel,
            config->throttle_channel,
            config->direction_channel,
            config->steering_channel,
        };
        uint8_t channel_index;

        for (channel_index = 0U; channel_index < 4U; ++channel_index)
        {
            result = cb_receiver_drive_require_fresh(
                raw, channels[channel_index], config->frame_timeout_ms, &remaining_ms);
            if (result != ESP_OK)
            {
                return result;
            }
            if (remaining_ms < minimum_remaining_ms)
            {
                minimum_remaining_ms = remaining_ms;
            }
        }
    }

    result = cb_receiver_drive_map_centered(
        raw->pulse_width_us[config->servo_channel], &config->servo, config->deadband_us,
        config->servo_reversed, &servo_axis);
    if (result != ESP_OK)
    {
        return result;
    }
    result = cb_receiver_drive_map_centered(
        raw->pulse_width_us[config->direction_channel], &config->direction, config->deadband_us,
        config->direction_reversed, &direction_axis);
    if (result != ESP_OK)
    {
        return result;
    }
    result = cb_receiver_drive_map_centered(
        raw->pulse_width_us[config->steering_channel], &config->steering, config->deadband_us,
        config->steering_reversed, &steering_axis);
    if (result != ESP_OK)
    {
        return result;
    }

    throttle = cb_receiver_drive_map_throttle(
        raw->pulse_width_us[config->throttle_channel], &config->throttle, config->deadband_us);
    direction_sign = (direction_axis > 0) ? 1 : ((direction_axis < 0) ? -1 : 0);
    direction_magnitude = (direction_axis < 0) ? -(int32_t)direction_axis : (int32_t)direction_axis;
    base_power = (throttle * direction_magnitude) / CB_AXIS_MAX;
    steering_magnitude = (steering_axis < 0) ? -(int32_t)steering_axis : (int32_t)steering_axis;
    turn_reduction = (base_power * steering_magnitude) / CB_AXIS_MAX;
    left_motor = base_power;
    right_motor = base_power;

    if (steering_axis > 0)
    {
        right_motor -= turn_reduction;
    }
    else if (steering_axis < 0)
    {
        left_motor -= turn_reduction;
    }

    left_motor *= direction_sign;
    right_motor *= direction_sign;

    command->motors.motor[config->left_motor_index] = (cb_axis_t)left_motor;
    command->motors.motor[1U - config->left_motor_index] = (cb_axis_t)right_motor;
    command->auxiliary_servo = (cb_axis_t)(
        ((int32_t)servo_axis * (int32_t)config->servo_travel_percent) / 100);
    command->rearm_safe = (throttle == 0) && (direction_axis == 0);
    command->valid_for_ms = minimum_remaining_ms;

    return ESP_OK;
}
