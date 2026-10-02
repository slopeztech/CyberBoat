/** @file servo_pwm.c
 *  @brief LEDC PWM implementation of the generic hobby-servo HAL.
 */

#include "cyberboat/drivers/servo_pwm.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "sdkconfig.h"

#define CB_SERVO_PWM_RESOLUTION_BITS 14U
#define CB_SERVO_PWM_MAX_DUTY (1UL << CB_SERVO_PWM_RESOLUTION_BITS)
#define CB_SERVO_PWM_MICROSECONDS_PER_SECOND 1000000ULL

static esp_err_t cb_servo_pwm_init(void *context);
static esp_err_t cb_servo_pwm_set_position(void *context, cb_axis_t position);
static esp_err_t cb_servo_pwm_set_pulse_width_us(void *context, uint32_t pulse_width_us);
static bool cb_servo_pwm_gpio_is_reserved(int32_t gpio_num, cb_servo_role_t role);
static uint32_t s_ledc_timer_frequency_hz[LEDC_TIMER_MAX];

esp_err_t cb_servo_pwm_get_default_config(cb_servo_role_t role, cb_servo_pwm_config_t *config)
{
    if (config == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    config->role = role;
    config->min_pulse_width_us = CONFIG_CYBERBOAT_RUDDER_SERVO_MIN_PULSE_US;
    config->center_pulse_width_us = CONFIG_CYBERBOAT_RUDDER_SERVO_CENTER_PULSE_US;
    config->max_pulse_width_us = CONFIG_CYBERBOAT_RUDDER_SERVO_MAX_PULSE_US;

    if (role == CB_SERVO_ROLE_RUDDER)
    {
#ifndef CONFIG_CYBERBOAT_RUDDER_SERVO_ENABLED
        return ESP_ERR_NOT_SUPPORTED;
#endif
        config->gpio_num = CONFIG_CYBERBOAT_RUDDER_SERVO_GPIO;
        config->frequency_hz = CONFIG_CYBERBOAT_RUDDER_SERVO_FREQUENCY_HZ;
        config->ledc_timer = CONFIG_CYBERBOAT_RUDDER_SERVO_LEDC_TIMER;
        config->ledc_channel = CONFIG_CYBERBOAT_RUDDER_SERVO_LEDC_CHANNEL;
    }
    else if (role == CB_SERVO_ROLE_AUXILIARY)
    {
#ifndef CONFIG_CYBERBOAT_AUX_SERVO_ENABLED
        return ESP_ERR_NOT_SUPPORTED;
#endif
        config->gpio_num = CONFIG_CYBERBOAT_AUX_SERVO_GPIO;
        config->frequency_hz = CONFIG_CYBERBOAT_AUX_SERVO_FREQUENCY_HZ;
        config->min_pulse_width_us = CONFIG_CYBERBOAT_AUX_SERVO_MIN_PULSE_US;
        config->center_pulse_width_us = CONFIG_CYBERBOAT_AUX_SERVO_CENTER_PULSE_US;
        config->max_pulse_width_us = CONFIG_CYBERBOAT_AUX_SERVO_MAX_PULSE_US;
        config->ledc_timer = CONFIG_CYBERBOAT_AUX_SERVO_LEDC_TIMER;
        config->ledc_channel = CONFIG_CYBERBOAT_AUX_SERVO_LEDC_CHANNEL;
    }
    else
    {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

esp_err_t cb_servo_pwm_validate_config(const cb_servo_pwm_config_t *config)
{
    uint64_t period_us;

    if (config == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if ((config->role != CB_SERVO_ROLE_RUDDER) &&
        (config->role != CB_SERVO_ROLE_AUXILIARY))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (cb_servo_pwm_gpio_is_reserved(config->gpio_num, config->role) ||
        !GPIO_IS_VALID_OUTPUT_GPIO(config->gpio_num) ||
        (config->frequency_hz < 40U) || (config->frequency_hz > 333U) ||
        (config->min_pulse_width_us >= config->center_pulse_width_us) ||
        (config->center_pulse_width_us >= config->max_pulse_width_us) ||
        (config->ledc_timer >= LEDC_TIMER_MAX) ||
        (config->ledc_channel >= LEDC_CHANNEL_MAX))
    {
        return ESP_ERR_INVALID_ARG;
    }

    period_us = CB_SERVO_PWM_MICROSECONDS_PER_SECOND / config->frequency_hz;
    if (config->max_pulse_width_us >= period_us)
    {
        return ESP_ERR_INVALID_ARG;
    }

#if defined(CONFIG_CYBERBOAT_RUDDER_SERVO_ENABLED) && defined(CONFIG_CYBERBOAT_AUX_SERVO_ENABLED)
    if (config->role == CB_SERVO_ROLE_RUDDER)
    {
        if (config->ledc_channel == CONFIG_CYBERBOAT_AUX_SERVO_LEDC_CHANNEL)
        {
            return ESP_ERR_INVALID_ARG;
        }
        if ((config->ledc_timer == CONFIG_CYBERBOAT_AUX_SERVO_LEDC_TIMER) &&
            (config->frequency_hz != CONFIG_CYBERBOAT_AUX_SERVO_FREQUENCY_HZ))
        {
            return ESP_ERR_INVALID_ARG;
        }
    }
    else
    {
        if (config->ledc_channel == CONFIG_CYBERBOAT_RUDDER_SERVO_LEDC_CHANNEL)
        {
            return ESP_ERR_INVALID_ARG;
        }
        if ((config->ledc_timer == CONFIG_CYBERBOAT_RUDDER_SERVO_LEDC_TIMER) &&
            (config->frequency_hz != CONFIG_CYBERBOAT_RUDDER_SERVO_FREQUENCY_HZ))
        {
            return ESP_ERR_INVALID_ARG;
        }
    }
#endif

    return ESP_OK;
}

static bool cb_servo_pwm_gpio_is_reserved(int32_t gpio_num, cb_servo_role_t role)
{
    if (gpio_num == CONFIG_CYBERBOAT_STATUS_LED_GPIO)
    {
        return true;
    }

#ifdef CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    if ((gpio_num == 19) || (gpio_num == 20))
    {
        return true;
    }
#endif

    if ((gpio_num == CONFIG_CYBERBOAT_MOTOR_1_IN1_GPIO) ||
        (gpio_num == CONFIG_CYBERBOAT_MOTOR_1_IN2_GPIO))
    {
        return true;
    }
#if CONFIG_CYBERBOAT_MOTOR_COUNT == 2
    if ((gpio_num == CONFIG_CYBERBOAT_MOTOR_2_IN1_GPIO) ||
        (gpio_num == CONFIG_CYBERBOAT_MOTOR_2_IN2_GPIO))
    {
        return true;
    }
#endif

#ifdef CONFIG_CYBERBOAT_RECEIVER_PWM
    if ((gpio_num == CONFIG_CYBERBOAT_PWM_RECEIVER_CH1_GPIO) ||
        (gpio_num == CONFIG_CYBERBOAT_PWM_RECEIVER_CH2_GPIO) ||
        (gpio_num == CONFIG_CYBERBOAT_PWM_RECEIVER_CH3_GPIO) ||
        (gpio_num == CONFIG_CYBERBOAT_PWM_RECEIVER_CH4_GPIO))
    {
        return true;
    }
#endif
#if CONFIG_CYBERBOAT_PWM_RECEIVER_CHANNEL_COUNT >= 5
#ifdef CONFIG_CYBERBOAT_RECEIVER_PWM
    if (gpio_num == CONFIG_CYBERBOAT_PWM_RECEIVER_CH5_GPIO)
    {
        return true;
    }
#endif
#endif
#if CONFIG_CYBERBOAT_PWM_RECEIVER_CHANNEL_COUNT >= 6
#ifdef CONFIG_CYBERBOAT_RECEIVER_PWM
    if (gpio_num == CONFIG_CYBERBOAT_PWM_RECEIVER_CH6_GPIO)
    {
        return true;
    }
#endif
#endif
#if CONFIG_CYBERBOAT_PWM_RECEIVER_CHANNEL_COUNT >= 7
#ifdef CONFIG_CYBERBOAT_RECEIVER_PWM
    if (gpio_num == CONFIG_CYBERBOAT_PWM_RECEIVER_CH7_GPIO)
    {
        return true;
    }
#endif
#endif
#if CONFIG_CYBERBOAT_PWM_RECEIVER_CHANNEL_COUNT >= 8
#ifdef CONFIG_CYBERBOAT_RECEIVER_PWM
    if (gpio_num == CONFIG_CYBERBOAT_PWM_RECEIVER_CH8_GPIO)
    {
        return true;
    }
#endif
#endif

#ifdef CONFIG_CYBERBOAT_AUX_SERVO_ENABLED
    if ((role == CB_SERVO_ROLE_RUDDER) && (gpio_num == CONFIG_CYBERBOAT_AUX_SERVO_GPIO))
    {
        return true;
    }
#endif
#ifdef CONFIG_CYBERBOAT_RUDDER_SERVO_ENABLED
    if ((role == CB_SERVO_ROLE_AUXILIARY) && (gpio_num == CONFIG_CYBERBOAT_RUDDER_SERVO_GPIO))
    {
        return true;
    }
#endif

    return false;
}

esp_err_t cb_servo_pwm_position_to_pulse_width_us(
    const cb_servo_pwm_config_t *config,
    cb_axis_t position,
    uint32_t *pulse_width_us)
{
    int32_t pulse_delta;
    int64_t scaled_delta;
    esp_err_t result;

    if ((config == NULL) || (pulse_width_us == NULL) ||
        (position < CB_AXIS_MIN) || (position > CB_AXIS_MAX))
    {
        return ESP_ERR_INVALID_ARG;
    }

    result = cb_servo_pwm_validate_config(config);
    if (result != ESP_OK)
    {
        return result;
    }

    if (position < 0)
    {
        pulse_delta = (int32_t)config->center_pulse_width_us -
                      (int32_t)config->min_pulse_width_us;
        scaled_delta = ((int64_t)pulse_delta * position) / CB_AXIS_MIN;
        *pulse_width_us = config->center_pulse_width_us - (uint32_t)scaled_delta;
    }
    else
    {
        pulse_delta = (int32_t)config->max_pulse_width_us -
                      (int32_t)config->center_pulse_width_us;
        scaled_delta = ((int64_t)pulse_delta * position) / CB_AXIS_MAX;
        *pulse_width_us = config->center_pulse_width_us + (uint32_t)scaled_delta;
    }

    return ESP_OK;
}

esp_err_t cb_servo_pwm_bind(
    cb_servo_pwm_driver_t *driver,
    const cb_servo_pwm_config_t *config,
    cb_servo_hal_t *servo_hal)
{
    esp_err_t result;

    if ((driver == NULL) || (config == NULL) || (servo_hal == NULL))
    {
        return ESP_ERR_INVALID_ARG;
    }

    result = cb_servo_pwm_validate_config(config);
    if (result != ESP_OK)
    {
        return result;
    }

    driver->config = *config;
    driver->initialized = false;
    driver->last_pulse_width_us = 0U;
    driver->last_error = ESP_OK;
    servo_hal->context = driver;
    servo_hal->init = cb_servo_pwm_init;
    servo_hal->set_position = cb_servo_pwm_set_position;
    servo_hal->set_pulse_width_us = cb_servo_pwm_set_pulse_width_us;

    return ESP_OK;
}

esp_err_t cb_servo_pwm_get_diagnostics(
    const cb_servo_pwm_driver_t *driver,
    cb_servo_pwm_diagnostics_t *diagnostics)
{
    if ((driver == NULL) || (diagnostics == NULL))
    {
        return ESP_ERR_INVALID_ARG;
    }

    diagnostics->config = driver->config;
    diagnostics->initialized = driver->initialized;
    diagnostics->last_pulse_width_us = driver->last_pulse_width_us;
    diagnostics->last_error = driver->last_error;

    return ESP_OK;
}

static esp_err_t cb_servo_pwm_init(void *context)
{
    cb_servo_pwm_driver_t *driver = (cb_servo_pwm_driver_t *)context;
    const ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_14_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 0U,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false,
    };
    ledc_timer_config_t configured_timer = timer_config;
    ledc_channel_config_t channel_config = {};
    esp_err_t result;

    if (driver == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (driver->initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

#if !CONFIG_CYBERBOAT_ACTUATORS_ENABLED
    driver->last_error = ESP_ERR_NOT_SUPPORTED;
    return ESP_ERR_NOT_SUPPORTED;
#endif

    configured_timer.timer_num = (ledc_timer_t)driver->config.ledc_timer;
    configured_timer.freq_hz = driver->config.frequency_hz;
    {
        const uint32_t existing_frequency = s_ledc_timer_frequency_hz[driver->config.ledc_timer];
        if (existing_frequency == 0U)
        {
            result = ledc_timer_config(&configured_timer);
            if (result == ESP_OK)
            {
                s_ledc_timer_frequency_hz[driver->config.ledc_timer] = driver->config.frequency_hz;
            }
        }
        else
        {
            result = (existing_frequency == driver->config.frequency_hz) ?
                     ESP_OK : ESP_ERR_INVALID_STATE;
        }
    }
    if (result != ESP_OK)
    {
        driver->last_error = result;
        return result;
    }

    channel_config.gpio_num = driver->config.gpio_num;
    channel_config.speed_mode = LEDC_LOW_SPEED_MODE;
    channel_config.channel = (ledc_channel_t)driver->config.ledc_channel;
    channel_config.timer_sel = (ledc_timer_t)driver->config.ledc_timer;
    channel_config.duty = 0U;
    channel_config.hpoint = 0;
    channel_config.sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD;
    channel_config.flags.output_invert = 0U;
    channel_config.deconfigure = false;

    result = ledc_channel_config(&channel_config);
    if (result != ESP_OK)
    {
        driver->last_error = result;
        return result;
    }

    driver->initialized = true;
    driver->last_error = ESP_OK;

    return ESP_OK;
}

static esp_err_t cb_servo_pwm_set_position(void *context, cb_axis_t position)
{
    cb_servo_pwm_driver_t *driver = (cb_servo_pwm_driver_t *)context;
    uint32_t pulse_width_us;
    esp_err_t result;

    if (driver == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    result = cb_servo_pwm_position_to_pulse_width_us(
        &driver->config, position, &pulse_width_us);
    if (result != ESP_OK)
    {
        driver->last_error = result;
        return result;
    }

    return cb_servo_pwm_set_pulse_width_us(driver, pulse_width_us);
}

static esp_err_t cb_servo_pwm_set_pulse_width_us(void *context, uint32_t pulse_width_us)
{
    cb_servo_pwm_driver_t *driver = (cb_servo_pwm_driver_t *)context;
    uint64_t duty_numerator;
    uint32_t duty;
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

    if ((pulse_width_us < driver->config.min_pulse_width_us) ||
        (pulse_width_us > driver->config.max_pulse_width_us))
    {
        driver->last_error = ESP_ERR_INVALID_ARG;
        return driver->last_error;
    }

    duty_numerator = (uint64_t)pulse_width_us * CB_SERVO_PWM_MAX_DUTY *
                     driver->config.frequency_hz;
    duty = (uint32_t)((duty_numerator + (CB_SERVO_PWM_MICROSECONDS_PER_SECOND / 2U)) /
                      CB_SERVO_PWM_MICROSECONDS_PER_SECOND);

    result = ledc_set_duty(
        LEDC_LOW_SPEED_MODE,
        (ledc_channel_t)driver->config.ledc_channel,
        duty);
    if (result == ESP_OK)
    {
        result = ledc_update_duty(
            LEDC_LOW_SPEED_MODE,
            (ledc_channel_t)driver->config.ledc_channel);
    }
    driver->last_error = result;
    if (result == ESP_OK)
    {
        driver->last_pulse_width_us = pulse_width_us;
    }

    return result;
}