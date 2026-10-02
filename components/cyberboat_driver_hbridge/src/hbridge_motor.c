/** @file hbridge_motor.c
 *  @brief LEDC implementation for direction-and-duty H-bridge inputs.
 */

#include "cyberboat/drivers/hbridge_motor.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "sdkconfig.h"

#define CB_HBRIDGE_PWM_RESOLUTION LEDC_TIMER_10_BIT
#define CB_HBRIDGE_INPUTS_PER_MOTOR 2U

static esp_err_t cb_hbridge_motor_init(void *context);
static esp_err_t cb_hbridge_motor_set_armed(void *context, bool armed);
static esp_err_t cb_hbridge_motor_set_output(void *context, uint8_t motor_index, cb_axis_t output);
static esp_err_t cb_hbridge_motor_emergency_stop(void *context);
static esp_err_t cb_hbridge_motor_stop_configured_channels(cb_hbridge_motor_driver_t *driver);
static esp_err_t cb_hbridge_motor_set_duty(ledc_channel_t channel, uint32_t duty);
static esp_err_t cb_hbridge_motor_write_pair(
    cb_hbridge_motor_driver_t *driver,
    uint8_t motor_index,
    const cb_hbridge_motor_pwm_t *pwm);

esp_err_t cb_hbridge_motor_get_default_config(cb_hbridge_motor_config_t *config)
{
    if (config == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    config->motor_count = (uint8_t)CONFIG_CYBERBOAT_MOTOR_COUNT;
    config->input_gpio[0][0] = CONFIG_CYBERBOAT_MOTOR_1_IN1_GPIO;
    config->input_gpio[0][1] = CONFIG_CYBERBOAT_MOTOR_1_IN2_GPIO;
#ifdef CONFIG_CYBERBOAT_MOTOR_1_REVERSED
    config->reversed[0] = true;
#else
    config->reversed[0] = false;
#endif
#if CONFIG_CYBERBOAT_MOTOR_COUNT == 2
    config->input_gpio[1][0] = CONFIG_CYBERBOAT_MOTOR_2_IN1_GPIO;
    config->input_gpio[1][1] = CONFIG_CYBERBOAT_MOTOR_2_IN2_GPIO;
#ifdef CONFIG_CYBERBOAT_MOTOR_2_REVERSED
    config->reversed[1] = true;
#else
    config->reversed[1] = false;
#endif
#else
    config->input_gpio[1][0] = 0;
    config->input_gpio[1][1] = 0;
    config->reversed[1] = false;
#endif
    config->pwm_frequency_hz = CONFIG_CYBERBOAT_MOTOR_PWM_FREQUENCY_HZ;
    config->ledc_timer = (uint8_t)CONFIG_CYBERBOAT_MOTOR_LEDC_TIMER;
    config->ledc_channel_base = (uint8_t)CONFIG_CYBERBOAT_MOTOR_LEDC_CHANNEL_BASE;

    return ESP_OK;
}

esp_err_t cb_hbridge_motor_validate_config(const cb_hbridge_motor_config_t *config)
{
    uint8_t motor_index;
    uint8_t input_index;
    uint16_t channel_end;

    if (config == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if ((config->motor_count != (uint8_t)CONFIG_CYBERBOAT_MOTOR_COUNT) ||
        (config->motor_count == 0U) || (config->motor_count > CB_MAX_MOTORS) ||
        (config->pwm_frequency_hz < 100U) || (config->pwm_frequency_hz > 20000U) ||
        (config->ledc_timer >= LEDC_TIMER_MAX))
    {
        return ESP_ERR_INVALID_ARG;
    }

    channel_end = (uint16_t)((uint16_t)config->ledc_channel_base +
                             ((uint16_t)config->motor_count *
                              (uint16_t)CB_HBRIDGE_INPUTS_PER_MOTOR));
    if (channel_end > (uint16_t)LEDC_CHANNEL_MAX)
    {
        return ESP_ERR_INVALID_ARG;
    }

#ifdef CONFIG_CYBERBOAT_RUDDER_SERVO_ENABLED
    if (config->ledc_timer == (uint8_t)CONFIG_CYBERBOAT_RUDDER_SERVO_LEDC_TIMER)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (((uint16_t)CONFIG_CYBERBOAT_RUDDER_SERVO_LEDC_CHANNEL >= (uint16_t)config->ledc_channel_base) &&
        ((uint16_t)CONFIG_CYBERBOAT_RUDDER_SERVO_LEDC_CHANNEL < channel_end))
    {
        return ESP_ERR_INVALID_ARG;
    }
#endif
#ifdef CONFIG_CYBERBOAT_AUX_SERVO_ENABLED
    if (config->ledc_timer == (uint8_t)CONFIG_CYBERBOAT_AUX_SERVO_LEDC_TIMER)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (((uint16_t)CONFIG_CYBERBOAT_AUX_SERVO_LEDC_CHANNEL >= (uint16_t)config->ledc_channel_base) &&
        ((uint16_t)CONFIG_CYBERBOAT_AUX_SERVO_LEDC_CHANNEL < channel_end))
    {
        return ESP_ERR_INVALID_ARG;
    }
#endif

    for (motor_index = 0U; motor_index < config->motor_count; ++motor_index)
    {
        for (input_index = 0U; input_index < CB_HBRIDGE_INPUTS_PER_MOTOR; ++input_index)
        {
            const int32_t gpio_num = config->input_gpio[motor_index][input_index];

            if (!GPIO_IS_VALID_OUTPUT_GPIO(gpio_num) ||
                (gpio_num == CONFIG_CYBERBOAT_STATUS_LED_GPIO))
            {
                return ESP_ERR_INVALID_ARG;
            }

#ifdef CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
            if ((gpio_num == 19) || (gpio_num == 20))
            {
                return ESP_ERR_INVALID_ARG;
            }
#endif

#ifdef CONFIG_CYBERBOAT_RUDDER_SERVO_ENABLED
            if (gpio_num == CONFIG_CYBERBOAT_RUDDER_SERVO_GPIO)
            {
                return ESP_ERR_INVALID_ARG;
            }
#endif
#ifdef CONFIG_CYBERBOAT_AUX_SERVO_ENABLED
            if (gpio_num == CONFIG_CYBERBOAT_AUX_SERVO_GPIO)
            {
                return ESP_ERR_INVALID_ARG;
            }
#endif

            {
                uint8_t prior_motor;
                uint8_t prior_input;

                for (prior_motor = 0U; prior_motor <= motor_index; ++prior_motor)
                {
                    const uint8_t input_limit = (prior_motor == motor_index) ? input_index : CB_HBRIDGE_INPUTS_PER_MOTOR;

                    for (prior_input = 0U; prior_input < input_limit; ++prior_input)
                    {
                        if (gpio_num == config->input_gpio[prior_motor][prior_input])
                        {
                            return ESP_ERR_INVALID_ARG;
                        }
                    }
                }
            }
        }
    }

    return ESP_OK;
}

esp_err_t cb_hbridge_motor_map_output(
    cb_axis_t output,
    bool reversed,
    cb_hbridge_motor_pwm_t *pwm)
{
    int32_t effective_output;
    uint32_t magnitude;
    uint32_t duty;

    if ((pwm == NULL) || (output < CB_AXIS_MIN) || (output > CB_AXIS_MAX))
    {
        return ESP_ERR_INVALID_ARG;
    }

    effective_output = (int32_t)output;
    if (reversed)
    {
        effective_output = -effective_output;
    }

    magnitude = (uint32_t)((effective_output < 0) ? -effective_output : effective_output);
    duty = (magnitude * CB_HBRIDGE_PWM_DUTY_MAX) / (uint32_t)CB_AXIS_MAX;
    pwm->input_1_duty = 0U;
    pwm->input_2_duty = 0U;

    if (effective_output > 0)
    {
        pwm->input_1_duty = duty;
    }
    else if (effective_output < 0)
    {
        pwm->input_2_duty = duty;
    }

    return ESP_OK;
}

esp_err_t cb_hbridge_motor_bind(
    cb_hbridge_motor_driver_t *driver,
    const cb_hbridge_motor_config_t *config,
    cb_motor_hal_t *motor_hal)
{
    esp_err_t result;

    if ((driver == NULL) || (config == NULL) || (motor_hal == NULL))
    {
        return ESP_ERR_INVALID_ARG;
    }

    result = cb_hbridge_motor_validate_config(config);
    if (result != ESP_OK)
    {
        return result;
    }

    driver->config = *config;
    driver->initialized = false;
    driver->armed = false;
    driver->configured_channels = 0U;
    driver->last_output[0] = 0;
    driver->last_output[1] = 0;
    driver->last_error = ESP_OK;
    motor_hal->context = driver;
    motor_hal->init = cb_hbridge_motor_init;
    motor_hal->set_armed = cb_hbridge_motor_set_armed;
    motor_hal->set_output = cb_hbridge_motor_set_output;
    motor_hal->emergency_stop = cb_hbridge_motor_emergency_stop;

    return ESP_OK;
}

esp_err_t cb_hbridge_motor_get_diagnostics(
    const cb_hbridge_motor_driver_t *driver,
    cb_hbridge_motor_diagnostics_t *diagnostics)
{
    if ((driver == NULL) || (diagnostics == NULL))
    {
        return ESP_ERR_INVALID_ARG;
    }

    diagnostics->config = driver->config;
    diagnostics->initialized = driver->initialized;
    diagnostics->armed = driver->armed;
    diagnostics->configured_channels = driver->configured_channels;
    diagnostics->last_output[0] = driver->last_output[0];
    diagnostics->last_output[1] = driver->last_output[1];
    diagnostics->last_error = driver->last_error;

    return ESP_OK;
}

static esp_err_t cb_hbridge_motor_init(void *context)
{
    cb_hbridge_motor_driver_t *driver = (cb_hbridge_motor_driver_t *)context;
    const ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = CB_HBRIDGE_PWM_RESOLUTION,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 0U,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false,
    };
    ledc_timer_config_t configured_timer = timer_config;
    uint8_t motor_index;
    uint8_t input_index;
    esp_err_t result;

    if (driver == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

#ifndef CONFIG_CYBERBOAT_ACTUATORS_ENABLED
    driver->last_error = ESP_ERR_NOT_SUPPORTED;
    return driver->last_error;
#endif

    if (driver->initialized || (driver->configured_channels != 0U))
    {
        return ESP_ERR_INVALID_STATE;
    }

    result = cb_hbridge_motor_validate_config(&driver->config);
    if (result != ESP_OK)
    {
        driver->last_error = result;
        return result;
    }

    configured_timer.timer_num = (ledc_timer_t)driver->config.ledc_timer;
    configured_timer.freq_hz = driver->config.pwm_frequency_hz;
    result = ledc_timer_config(&configured_timer);
    if (result != ESP_OK)
    {
        driver->last_error = result;
        return result;
    }

    for (motor_index = 0U; motor_index < driver->config.motor_count; ++motor_index)
    {
        for (input_index = 0U; input_index < CB_HBRIDGE_INPUTS_PER_MOTOR; ++input_index)
        {
            const uint8_t output_index = (uint8_t)(motor_index * CB_HBRIDGE_INPUTS_PER_MOTOR + input_index);
            const ledc_channel_config_t channel_config = {
                .gpio_num = driver->config.input_gpio[motor_index][input_index],
                .speed_mode = LEDC_LOW_SPEED_MODE,
                .channel = (ledc_channel_t)(driver->config.ledc_channel_base + output_index),
                .timer_sel = (ledc_timer_t)driver->config.ledc_timer,
                .duty = 0U,
                .hpoint = 0,
                .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
                .flags = {
                    .output_invert = 0U,
                },
                .deconfigure = false,
            };

            result = ledc_channel_config(&channel_config);
            if (result != ESP_OK)
            {
                (void)cb_hbridge_motor_stop_configured_channels(driver);
                driver->last_error = result;
                return result;
            }

            driver->configured_channels++;
        }
    }

    driver->initialized = true;
    driver->armed = false;
    driver->last_error = ESP_OK;

    return ESP_OK;
}

static esp_err_t cb_hbridge_motor_set_armed(void *context, bool armed)
{
    cb_hbridge_motor_driver_t *driver = (cb_hbridge_motor_driver_t *)context;
    esp_err_t result;

    if (driver == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!driver->initialized)
    {
        driver->last_error = ESP_ERR_INVALID_STATE;
        return driver->last_error;
    }

    if (!armed)
    {
        return cb_hbridge_motor_emergency_stop(driver);
    }

    driver->armed = true;
    result = ESP_OK;
    driver->last_error = result;

    return result;
}

static esp_err_t cb_hbridge_motor_set_output(void *context, uint8_t motor_index, cb_axis_t output)
{
    cb_hbridge_motor_driver_t *driver = (cb_hbridge_motor_driver_t *)context;
    cb_hbridge_motor_pwm_t pwm;
    esp_err_t result;

    if (driver == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!driver->initialized || !driver->armed)
    {
        driver->last_error = ESP_ERR_INVALID_STATE;
        return driver->last_error;
    }

    if ((motor_index >= driver->config.motor_count) ||
        (output < CB_AXIS_MIN) || (output > CB_AXIS_MAX))
    {
        driver->last_error = ESP_ERR_INVALID_ARG;
        return driver->last_error;
    }

    result = cb_hbridge_motor_map_output(output, driver->config.reversed[motor_index], &pwm);
    if (result == ESP_OK)
    {
        result = cb_hbridge_motor_write_pair(driver, motor_index, &pwm);
    }

    if (result != ESP_OK)
    {
        (void)cb_hbridge_motor_stop_configured_channels(driver);
        driver->armed = false;
        driver->last_output[0] = 0;
        driver->last_output[1] = 0;
        driver->last_error = result;
        return result;
    }

    driver->last_output[motor_index] = output;
    driver->last_error = ESP_OK;

    return ESP_OK;
}

static esp_err_t cb_hbridge_motor_emergency_stop(void *context)
{
    cb_hbridge_motor_driver_t *driver = (cb_hbridge_motor_driver_t *)context;
    esp_err_t result;

    if (driver == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    result = cb_hbridge_motor_stop_configured_channels(driver);
    driver->armed = false;
    driver->last_output[0] = 0;
    driver->last_output[1] = 0;
    driver->last_error = result;

    return result;
}

static esp_err_t cb_hbridge_motor_stop_configured_channels(cb_hbridge_motor_driver_t *driver)
{
    uint8_t channel_offset;
    esp_err_t first_error = ESP_OK;

    for (channel_offset = 0U; channel_offset < driver->configured_channels; ++channel_offset)
    {
        const ledc_channel_t channel = (ledc_channel_t)(driver->config.ledc_channel_base + channel_offset);
        const esp_err_t result = cb_hbridge_motor_set_duty(channel, 0U);

        if ((first_error == ESP_OK) && (result != ESP_OK))
        {
            first_error = result;
        }
    }

    return first_error;
}

static esp_err_t cb_hbridge_motor_write_pair(
    cb_hbridge_motor_driver_t *driver,
    uint8_t motor_index,
    const cb_hbridge_motor_pwm_t *pwm)
{
    const uint8_t first_output_index = (uint8_t)(motor_index * CB_HBRIDGE_INPUTS_PER_MOTOR);
    const ledc_channel_t input_1_channel = (ledc_channel_t)(driver->config.ledc_channel_base + first_output_index);
    const ledc_channel_t input_2_channel = (ledc_channel_t)(driver->config.ledc_channel_base + first_output_index + 1U);
    esp_err_t result;

    result = cb_hbridge_motor_set_duty(input_1_channel, 0U);
    if (result != ESP_OK)
    {
        return result;
    }

    result = cb_hbridge_motor_set_duty(input_2_channel, 0U);
    if (result != ESP_OK)
    {
        return result;
    }

    if (pwm->input_1_duty > 0U)
    {
        return cb_hbridge_motor_set_duty(input_1_channel, pwm->input_1_duty);
    }

    if (pwm->input_2_duty > 0U)
    {
        return cb_hbridge_motor_set_duty(input_2_channel, pwm->input_2_duty);
    }

    return ESP_OK;
}

static esp_err_t cb_hbridge_motor_set_duty(ledc_channel_t channel, uint32_t duty)
{
    esp_err_t result = ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty);

    if (result == ESP_OK)
    {
        result = ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
    }

    return result;
}