/** @file control.c
 *  @brief Pure command shaping with no direct peripheral access.
 */

#include "cyberboat/control.h"

#include "sdkconfig.h"

esp_err_t cb_control_get_vessel_mode(cb_vessel_mode_config_t *mode)
{
    if (mode == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    mode->motor_count = (uint8_t)CONFIG_CYBERBOAT_MOTOR_COUNT;
#ifdef CONFIG_CYBERBOAT_RUDDER_SERVO_ENABLED
    mode->rudder_servo_enabled = true;
#else
    mode->rudder_servo_enabled = false;
#endif
#ifdef CONFIG_CYBERBOAT_AUX_SERVO_ENABLED
    mode->auxiliary_servo_enabled = true;
#else
    mode->auxiliary_servo_enabled = false;
#endif

#if CONFIG_CYBERBOAT_MODE_SINGLE_MOTOR_ONLY
    mode->id = CB_VESSEL_MODE_SINGLE_MOTOR_ONLY;
    mode->name = "single-motor";
#elif CONFIG_CYBERBOAT_MODE_SINGLE_MOTOR_RUDDER
    mode->id = CB_VESSEL_MODE_SINGLE_MOTOR_RUDDER;
    mode->name = "single-motor-rudder";
#elif CONFIG_CYBERBOAT_MODE_SINGLE_MOTOR_AUX_SERVO
    mode->id = CB_VESSEL_MODE_SINGLE_MOTOR_AUX_SERVO;
    mode->name = "single-motor-aux-servo";
#elif CONFIG_CYBERBOAT_MODE_SINGLE_MOTOR_RUDDER_AUX
    mode->id = CB_VESSEL_MODE_SINGLE_MOTOR_RUDDER_AUX;
    mode->name = "single-motor-rudder-aux-servo";
#elif CONFIG_CYBERBOAT_MODE_TWIN_MOTOR_ONLY
    mode->id = CB_VESSEL_MODE_TWIN_MOTOR_ONLY;
    mode->name = "twin-motor-differential";
#elif CONFIG_CYBERBOAT_MODE_TWIN_MOTOR_RUDDER
    mode->id = CB_VESSEL_MODE_TWIN_MOTOR_RUDDER;
    mode->name = "twin-motor-rudder";
#elif CONFIG_CYBERBOAT_MODE_TWIN_MOTOR_AUX_SERVO
    mode->id = CB_VESSEL_MODE_TWIN_MOTOR_AUX_SERVO;
    mode->name = "twin-motor-aux-servo";
#elif CONFIG_CYBERBOAT_MODE_TWIN_MOTOR_RUDDER_AUX
    mode->id = CB_VESSEL_MODE_TWIN_MOTOR_RUDDER_AUX;
    mode->name = "twin-motor-rudder-aux-servo";
#else
#error "Select exactly one CYBERBOAT_VESSEL_MODE"
#endif

    return ESP_OK;
}

static int32_t cb_control_clamp_axis(int32_t value)
{
    if (value > CB_AXIS_MAX)
    {
        return CB_AXIS_MAX;
    }

    if (value < CB_AXIS_MIN)
    {
        return CB_AXIS_MIN;
    }

    return value;
}

#if CONFIG_CYBERBOAT_MOTOR_COUNT == 2 && !defined(CONFIG_CYBERBOAT_RUDDER_SERVO_ENABLED)
static int32_t cb_control_abs(int32_t value)
{
    return (value < 0) ? -value : value;
}
#endif

esp_err_t cb_control_mix_motors(const cb_pilot_command_t *command, cb_motor_outputs_t *outputs)
{
    int32_t throttle;
#if CONFIG_CYBERBOAT_MOTOR_COUNT == 2
#ifndef CONFIG_CYBERBOAT_RUDDER_SERVO_ENABLED
    int32_t steering;
#endif
#endif

    if ((command == NULL) || (outputs == NULL))
    {
        return ESP_ERR_INVALID_ARG;
    }

    throttle = cb_control_clamp_axis(command->throttle);
    outputs->motor[0] = (cb_axis_t)throttle;
    outputs->motor[1] = 0;

#if CONFIG_CYBERBOAT_MOTOR_COUNT == 2
#ifdef CONFIG_CYBERBOAT_RUDDER_SERVO_ENABLED
    outputs->motor[1] = (cb_axis_t)throttle;
#else
    {
        steering = cb_control_clamp_axis(command->steering);
        int32_t left = throttle + steering;
        int32_t right = throttle - steering;
        int32_t magnitude = cb_control_abs(left);
        int32_t right_magnitude = cb_control_abs(right);

        if (right_magnitude > magnitude)
        {
            magnitude = right_magnitude;
        }

        if (magnitude > CB_AXIS_MAX)
        {
            left = (left * CB_AXIS_MAX) / magnitude;
            right = (right * CB_AXIS_MAX) / magnitude;
        }

        outputs->motor[0] = (cb_axis_t)left;
        outputs->motor[1] = (cb_axis_t)right;
    }
#endif
#elif CONFIG_CYBERBOAT_MOTOR_COUNT != 1
#error "CYBERBOAT_MOTOR_COUNT must be 1 or 2"
#endif

    return ESP_OK;
}