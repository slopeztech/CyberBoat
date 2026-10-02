/** @file test_pwm_receiver.c
 *  @brief Unity tests for ESP32 PWM receiver configuration and HAL binding.
 */

#include "cyberboat/drivers/pwm_receiver.h"

#include "unity.h"

static void test_default_four_channel_gpio_configuration_is_valid(void)
{
    cb_pwm_receiver_config_t config = {};
    TEST_ASSERT_EQUAL(ESP_OK, cb_pwm_receiver_get_default_config(&config));
    TEST_ASSERT_EQUAL_UINT8(4U, config.channel_count);
    TEST_ASSERT_EQUAL(ESP_OK, cb_pwm_receiver_validate_config(&config));
}

static void test_receiver_rejects_duplicate_channel_pins(void)
{
    cb_pwm_receiver_config_t config = {};
    TEST_ASSERT_EQUAL(ESP_OK, cb_pwm_receiver_get_default_config(&config));
    config.channel_gpio[1] = config.channel_gpio[0];
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_pwm_receiver_validate_config(&config));
}

static void test_receiver_rejects_reserved_esp32_gpio(void)
{
    cb_pwm_receiver_config_t config = {};
    TEST_ASSERT_EQUAL(ESP_OK, cb_pwm_receiver_get_default_config(&config));
    config.channel_gpio[0] = CONFIG_CYBERBOAT_STATUS_LED_GPIO;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_pwm_receiver_validate_config(&config));

#ifdef CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    TEST_ASSERT_EQUAL(ESP_OK, cb_pwm_receiver_get_default_config(&config));
    config.channel_gpio[0] = 19;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_pwm_receiver_validate_config(&config));
    config.channel_gpio[0] = 20;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_pwm_receiver_validate_config(&config));
#endif
}

static void test_receiver_bind_and_uninitialized_read_do_not_install_isrs(void)
{
    cb_pwm_receiver_driver_t driver = {};
    cb_pwm_receiver_config_t config = {};
    cb_receiver_hal_t receiver_hal = {};
    cb_receiver_raw_frame_t frame = {};

    TEST_ASSERT_EQUAL(ESP_OK, cb_pwm_receiver_get_default_config(&config));
    TEST_ASSERT_EQUAL(ESP_OK, cb_pwm_receiver_bind(&driver, &config, &receiver_hal));
    TEST_ASSERT_NOT_NULL(receiver_hal.context);
    TEST_ASSERT_NOT_NULL(receiver_hal.init);
    TEST_ASSERT_NOT_NULL(receiver_hal.read_frame);
    TEST_ASSERT_FALSE(driver.initialized);
    TEST_ASSERT_EQUAL_UINT8(0U, driver.configured_channel_count);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, receiver_hal.read_frame(receiver_hal.context, &frame));
}

void cb_pwm_receiver_run_unit_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_default_four_channel_gpio_configuration_is_valid);
    RUN_TEST(test_receiver_rejects_duplicate_channel_pins);
    RUN_TEST(test_receiver_rejects_reserved_esp32_gpio);
    RUN_TEST(test_receiver_bind_and_uninitialized_read_do_not_install_isrs);
    (void)UNITY_END();
}