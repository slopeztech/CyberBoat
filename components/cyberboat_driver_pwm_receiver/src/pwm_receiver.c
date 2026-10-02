/** @file pwm_receiver.c
 *  @brief Interrupt-driven pulse capture for parallel PWM RC receiver outputs.
 */

#include "cyberboat/drivers/pwm_receiver.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static void cb_pwm_receiver_gpio_isr(void *argument);
static bool cb_pwm_receiver_gpio_is_reserved(int32_t gpio_num);
static void cb_pwm_receiver_remove_handlers(cb_pwm_receiver_driver_t *driver);
static esp_err_t cb_pwm_receiver_init(void *context);
static esp_err_t cb_pwm_receiver_read_frame(void *context, cb_receiver_raw_frame_t *frame);

esp_err_t cb_pwm_receiver_get_default_config(cb_pwm_receiver_config_t *config)
{
    if (config == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    config->channel_count = (uint8_t)CONFIG_CYBERBOAT_PWM_RECEIVER_CHANNEL_COUNT;
    config->channel_gpio[0] = CONFIG_CYBERBOAT_PWM_RECEIVER_CH1_GPIO;
    config->channel_gpio[1] = CONFIG_CYBERBOAT_PWM_RECEIVER_CH2_GPIO;
    config->channel_gpio[2] = CONFIG_CYBERBOAT_PWM_RECEIVER_CH3_GPIO;
    config->channel_gpio[3] = CONFIG_CYBERBOAT_PWM_RECEIVER_CH4_GPIO;
#if CONFIG_CYBERBOAT_PWM_RECEIVER_CHANNEL_COUNT >= 5
    config->channel_gpio[4] = CONFIG_CYBERBOAT_PWM_RECEIVER_CH5_GPIO;
#else
    config->channel_gpio[4] = 0;
#endif
#if CONFIG_CYBERBOAT_PWM_RECEIVER_CHANNEL_COUNT >= 6
    config->channel_gpio[5] = CONFIG_CYBERBOAT_PWM_RECEIVER_CH6_GPIO;
#else
    config->channel_gpio[5] = 0;
#endif
#if CONFIG_CYBERBOAT_PWM_RECEIVER_CHANNEL_COUNT >= 7
    config->channel_gpio[6] = CONFIG_CYBERBOAT_PWM_RECEIVER_CH7_GPIO;
#else
    config->channel_gpio[6] = 0;
#endif
#if CONFIG_CYBERBOAT_PWM_RECEIVER_CHANNEL_COUNT >= 8
    config->channel_gpio[7] = CONFIG_CYBERBOAT_PWM_RECEIVER_CH8_GPIO;
#else
    config->channel_gpio[7] = 0;
#endif
    config->min_pulse_width_us = CONFIG_CYBERBOAT_PWM_RECEIVER_MIN_PULSE_US;
    config->max_pulse_width_us = CONFIG_CYBERBOAT_PWM_RECEIVER_MAX_PULSE_US;
    config->timeout_ms = CONFIG_CYBERBOAT_PWM_RECEIVER_TIMEOUT_MS;

    return ESP_OK;
}

esp_err_t cb_pwm_receiver_validate_config(const cb_pwm_receiver_config_t *config)
{
    uint8_t channel_index;

    if (config == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if ((config->channel_count == 0U) ||
        (config->channel_count > CB_MAX_RECEIVER_CHANNELS) ||
        (config->channel_count != (uint8_t)CONFIG_CYBERBOAT_PWM_RECEIVER_CHANNEL_COUNT) ||
        (config->min_pulse_width_us >= config->max_pulse_width_us) ||
        (config->timeout_ms < 20U) || (config->timeout_ms > 1000U))
    {
        return ESP_ERR_INVALID_ARG;
    }

    for (channel_index = 0U; channel_index < config->channel_count; ++channel_index)
    {
        uint8_t previous_channel;
        const int32_t gpio_num = config->channel_gpio[channel_index];

        if (!GPIO_IS_VALID_GPIO(gpio_num) || cb_pwm_receiver_gpio_is_reserved(gpio_num))
        {
            return ESP_ERR_INVALID_ARG;
        }

        for (previous_channel = 0U; previous_channel < channel_index; ++previous_channel)
        {
            if (gpio_num == config->channel_gpio[previous_channel])
            {
                return ESP_ERR_INVALID_ARG;
            }
        }
    }

    return ESP_OK;
}

esp_err_t cb_pwm_receiver_bind(
    cb_pwm_receiver_driver_t *driver,
    const cb_pwm_receiver_config_t *config,
    cb_receiver_hal_t *receiver_hal)
{
    esp_err_t result;

    if ((driver == NULL) || (config == NULL) || (receiver_hal == NULL))
    {
        return ESP_ERR_INVALID_ARG;
    }

    result = cb_pwm_receiver_validate_config(config);
    if (result != ESP_OK)
    {
        return result;
    }

    driver->config = *config;
    driver->initialized = false;
    driver->configured_channel_count = 0U;
    driver->valid_pulse_count = 0U;
    driver->invalid_pulse_count = 0U;
    driver->last_error = ESP_OK;
    portMUX_INITIALIZE(&driver->mux);

    for (uint8_t channel_index = 0U; channel_index < CB_MAX_RECEIVER_CHANNELS; ++channel_index)
    {
        driver->channel[channel_index].rising_edge_us = 0;
        driver->channel[channel_index].last_valid_edge_us = 0;
        driver->channel[channel_index].pulse_width_us = 0U;
        driver->isr_context[channel_index].driver = driver;
        driver->isr_context[channel_index].channel_index = channel_index;
    }

    receiver_hal->context = driver;
    receiver_hal->init = cb_pwm_receiver_init;
    receiver_hal->read_frame = cb_pwm_receiver_read_frame;

    return ESP_OK;
}

esp_err_t cb_pwm_receiver_get_diagnostics(
    cb_pwm_receiver_driver_t *driver,
    cb_pwm_receiver_diagnostics_t *diagnostics)
{
    int64_t now_us;

    if ((driver == NULL) || (diagnostics == NULL))
    {
        return ESP_ERR_INVALID_ARG;
    }

    now_us = esp_timer_get_time();
    portENTER_CRITICAL(&driver->mux);
    diagnostics->config = driver->config;
    diagnostics->initialized = driver->initialized;
    diagnostics->valid_channels_mask = 0U;
    diagnostics->valid_pulse_count = driver->valid_pulse_count;
    diagnostics->invalid_pulse_count = driver->invalid_pulse_count;
    diagnostics->last_error = driver->last_error;

    for (uint8_t channel_index = 0U; channel_index < CB_MAX_RECEIVER_CHANNELS; ++channel_index)
    {
        const int64_t last_valid_us = driver->channel[channel_index].last_valid_edge_us;

        diagnostics->pulse_width_us[channel_index] = driver->channel[channel_index].pulse_width_us;
        diagnostics->channel_age_ms[channel_index] = UINT32_MAX;

        if ((channel_index < driver->config.channel_count) &&
            (last_valid_us > 0) && (now_us >= last_valid_us))
        {
            const uint64_t age_ms = (uint64_t)(now_us - last_valid_us) / 1000U;
            diagnostics->channel_age_ms[channel_index] =
                (age_ms > UINT32_MAX) ? UINT32_MAX : (uint32_t)age_ms;
            if (age_ms < driver->config.timeout_ms)
            {
                diagnostics->valid_channels_mask |= (uint16_t)(1U << channel_index);
            }
        }
    }

    portEXIT_CRITICAL(&driver->mux);

    return ESP_OK;
}

static bool cb_pwm_receiver_gpio_is_reserved(int32_t gpio_num)
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

#ifdef CONFIG_CYBERBOAT_RUDDER_SERVO_ENABLED
    if (gpio_num == CONFIG_CYBERBOAT_RUDDER_SERVO_GPIO)
    {
        return true;
    }
#endif
#ifdef CONFIG_CYBERBOAT_AUX_SERVO_ENABLED
    if (gpio_num == CONFIG_CYBERBOAT_AUX_SERVO_GPIO)
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

    return false;
}

static void cb_pwm_receiver_gpio_isr(void *argument)
{
    cb_pwm_receiver_isr_context_t *isr_argument = (cb_pwm_receiver_isr_context_t *)argument;
    cb_pwm_receiver_driver_t *driver;
    cb_pwm_receiver_channel_state_t *channel;
    gpio_num_t gpio_num;
    const int64_t now_us = esp_timer_get_time();
    int level;
    uint8_t channel_index;

    if (isr_argument == NULL)
    {
        return;
    }

    driver = isr_argument->driver;
    channel_index = isr_argument->channel_index;
    if ((driver == NULL) || (channel_index >= driver->config.channel_count))
    {
        return;
    }

    gpio_num = (gpio_num_t)driver->config.channel_gpio[channel_index];
    level = gpio_get_level(gpio_num);
    channel = &driver->channel[channel_index];
    portENTER_CRITICAL_ISR(&driver->mux);
    if (level != 0)
    {
        channel->rising_edge_us = now_us;
    }
    else if ((channel->rising_edge_us > 0) && (now_us > channel->rising_edge_us))
    {
        const uint64_t pulse_width_us = (uint64_t)(now_us - channel->rising_edge_us);
        channel->rising_edge_us = 0;

        if ((pulse_width_us >= driver->config.min_pulse_width_us) &&
            (pulse_width_us <= driver->config.max_pulse_width_us) &&
            (pulse_width_us <= UINT16_MAX))
        {
            channel->pulse_width_us = (uint16_t)pulse_width_us;
            channel->last_valid_edge_us = now_us;
            driver->valid_pulse_count++;
        }
        else
        {
            driver->invalid_pulse_count++;
        }
    }
    portEXIT_CRITICAL_ISR(&driver->mux);
}

static void cb_pwm_receiver_remove_handlers(cb_pwm_receiver_driver_t *driver)
{
    while (driver->configured_channel_count > 0U)
    {
        const uint8_t channel_index = (uint8_t)(driver->configured_channel_count - 1U);
        const gpio_num_t gpio_num = (gpio_num_t)driver->config.channel_gpio[channel_index];

        (void)gpio_isr_handler_remove(gpio_num);
        (void)gpio_reset_pin(gpio_num);
        driver->configured_channel_count--;
    }

    driver->initialized = false;
}

static esp_err_t cb_pwm_receiver_init(void *context)
{
    cb_pwm_receiver_driver_t *driver = (cb_pwm_receiver_driver_t *)context;
    esp_err_t result;

    if (driver == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (driver->initialized || (driver->configured_channel_count != 0U))
    {
        return ESP_ERR_INVALID_STATE;
    }

    result = cb_pwm_receiver_validate_config(&driver->config);
    if (result != ESP_OK)
    {
        driver->last_error = result;
        return result;
    }

    result = gpio_install_isr_service(0);
    if ((result != ESP_OK) && (result != ESP_ERR_INVALID_STATE))
    {
        driver->last_error = result;
        return result;
    }

    for (uint8_t channel_index = 0U; channel_index < driver->config.channel_count; ++channel_index)
    {
        const gpio_num_t gpio_num = (gpio_num_t)driver->config.channel_gpio[channel_index];
        const gpio_config_t pin_config = {
            .pin_bit_mask = (1ULL << (uint32_t)gpio_num),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_ANYEDGE,
        };

        result = gpio_config(&pin_config);
        if (result != ESP_OK)
        {
            (void)gpio_reset_pin(gpio_num);
            driver->last_error = result;
            cb_pwm_receiver_remove_handlers(driver);
            return result;
        }

        result = gpio_isr_handler_add(
            gpio_num,
            cb_pwm_receiver_gpio_isr,
            &driver->isr_context[channel_index]);
        if (result != ESP_OK)
        {
            (void)gpio_reset_pin(gpio_num);
            driver->last_error = result;
            cb_pwm_receiver_remove_handlers(driver);
            return result;
        }

        driver->configured_channel_count++;
    }

    driver->initialized = true;
    driver->last_error = ESP_OK;

    return ESP_OK;
}

static esp_err_t cb_pwm_receiver_read_frame(void *context, cb_receiver_raw_frame_t *frame)
{
    cb_pwm_receiver_driver_t *driver = (cb_pwm_receiver_driver_t *)context;
    const int64_t now_us = esp_timer_get_time();

    if ((driver == NULL) || (frame == NULL))
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (!driver->initialized)
    {
        driver->last_error = ESP_ERR_INVALID_STATE;
        return driver->last_error;
    }

    portENTER_CRITICAL(&driver->mux);
    frame->channel_count = driver->config.channel_count;
    frame->valid_channels_mask = 0U;

    for (uint8_t channel_index = 0U; channel_index < CB_MAX_RECEIVER_CHANNELS; ++channel_index)
    {
        const int64_t last_valid_us = driver->channel[channel_index].last_valid_edge_us;

        frame->pulse_width_us[channel_index] = driver->channel[channel_index].pulse_width_us;
        frame->channel_age_ms[channel_index] = UINT32_MAX;

        if ((channel_index < driver->config.channel_count) &&
            (last_valid_us > 0) && (now_us >= last_valid_us))
        {
            const uint64_t age_ms = (uint64_t)(now_us - last_valid_us) / 1000U;
            frame->channel_age_ms[channel_index] =
                (age_ms > UINT32_MAX) ? UINT32_MAX : (uint32_t)age_ms;
            if (age_ms < driver->config.timeout_ms)
            {
                frame->valid_channels_mask |= (uint16_t)(1U << channel_index);
            }
        }
    }

    portEXIT_CRITICAL(&driver->mux);
    portENTER_CRITICAL(&driver->mux);
    driver->last_error = ESP_OK;
    portEXIT_CRITICAL(&driver->mux);

    return ESP_OK;
}