#ifndef CYBERBOAT_DRIVER_WS2812_H
#define CYBERBOAT_DRIVER_WS2812_H

/** @file ws2812.h
 *  @brief WS2812 implementation of the status-indicator HAL.
 */

#include "cyberboat/hal/status_indicator.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the configured WS2812 and bind its operations to the HAL.
 *
 * @param[out] status_indicator HAL structure to initialize.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for an invalid argument,
 *         ESP_ERR_INVALID_STATE if the static driver was already initialized,
 *         or an ESP-IDF LED-strip error.
 */
esp_err_t cb_ws2812_status_indicator_init(cb_status_indicator_hal_t *status_indicator);

#ifdef __cplusplus
}
#endif

#endif