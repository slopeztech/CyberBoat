/** @file ws2812.c
 *  @brief WS2812 RMT driver for the board's RGB status LED.
 */

#include "cyberboat/drivers/ws2812.h"

#include "led_strip.h"
#include "led_strip_rmt.h"
#include "sdkconfig.h"

typedef struct
{
    led_strip_handle_t strip;
} cb_ws2812_context_t;

static cb_ws2812_context_t cb_ws2812_context;

static esp_err_t cb_ws2812_set_rgb(void *context, uint8_t red, uint8_t green, uint8_t blue)
{
    cb_ws2812_context_t *driver = (cb_ws2812_context_t *)context;
    esp_err_t result;

    if ((driver == NULL) || (driver->strip == NULL))
    {
        return ESP_ERR_INVALID_STATE;
    }

    result = led_strip_set_pixel(driver->strip, 0U, red, green, blue);
    if (result != ESP_OK)
    {
        return result;
    }

    return led_strip_refresh(driver->strip);
}

static esp_err_t cb_ws2812_clear(void *context)
{
    cb_ws2812_context_t *driver = (cb_ws2812_context_t *)context;
    esp_err_t result;

    if ((driver == NULL) || (driver->strip == NULL))
    {
        return ESP_ERR_INVALID_STATE;
    }

    result = led_strip_clear(driver->strip);
    if (result != ESP_OK)
    {
        return result;
    }

    return led_strip_refresh(driver->strip);
}

esp_err_t cb_ws2812_status_indicator_init(cb_status_indicator_hal_t *status_indicator)
{
    const led_strip_config_t strip_config = {
        .strip_gpio_num = CONFIG_CYBERBOAT_STATUS_LED_GPIO,
        .max_leds = 1U,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
    };
    const led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10U * 1000U * 1000U,
        .mem_block_symbols = 64U,
        .flags = {
            .with_dma = false,
        },
    };

    if (status_indicator == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (cb_ws2812_context.strip != NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    status_indicator->context = &cb_ws2812_context;
    status_indicator->set_rgb = cb_ws2812_set_rgb;
    status_indicator->clear = cb_ws2812_clear;

    return led_strip_new_rmt_device(&strip_config, &rmt_config, &cb_ws2812_context.strip);
}