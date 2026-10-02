/** @file test_servo_pwm.c
 *  @brief Unity tests for servo pulse mapping and ESP32 configuration checks.
 */

#include "cyberboat/drivers/servo_pwm.h"

#include "sdkconfig.h"
#include "unity.h"

static cb_servo_pwm_config_t get_valid_test_config(cb_servo_role_t role)
{
    cb_servo_pwm_config_t config = {};

    (void)cb_servo_pwm_get_default_config(role, &config);
    return config;
}

static void test_servo_maps_axis_endpoints_and_center(void)
{
    const cb_servo_pwm_config_t config = get_valid_test_config(CB_SERVO_ROLE_RUDDER);
    uint32_t pulse_width_us = 0U;

    TEST_ASSERT_EQUAL(ESP_OK,
                      cb_servo_pwm_position_to_pulse_width_us(&config, CB_AXIS_MIN, &pulse_width_us));
    TEST_ASSERT_EQUAL_UINT32(config.min_pulse_width_us, pulse_width_us);

    TEST_ASSERT_EQUAL(ESP_OK,
                      cb_servo_pwm_position_to_pulse_width_us(&config, 0, &pulse_width_us));
    TEST_ASSERT_EQUAL_UINT32(config.center_pulse_width_us, pulse_width_us);

    TEST_ASSERT_EQUAL(ESP_OK,
                      cb_servo_pwm_position_to_pulse_width_us(&config, CB_AXIS_MAX, &pulse_width_us));
    TEST_ASSERT_EQUAL_UINT32(config.max_pulse_width_us, pulse_width_us);
}

static void test_servo_maps_intermediate_positions(void)
{
    const cb_servo_pwm_config_t config = get_valid_test_config(CB_SERVO_ROLE_RUDDER);
    uint32_t pulse_width_us = 0U;

    TEST_ASSERT_EQUAL(ESP_OK,
                      cb_servo_pwm_position_to_pulse_width_us(&config, -500, &pulse_width_us));
    TEST_ASSERT_EQUAL_UINT32(1000U, pulse_width_us);

    TEST_ASSERT_EQUAL(ESP_OK,
                      cb_servo_pwm_position_to_pulse_width_us(&config, 500, &pulse_width_us));
    TEST_ASSERT_EQUAL_UINT32(2000U, pulse_width_us);
}

static void test_servo_rejects_out_of_range_position_and_null_arguments(void)
{
    const cb_servo_pwm_config_t config = get_valid_test_config(CB_SERVO_ROLE_RUDDER);
    uint32_t pulse_width_us = 0U;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      cb_servo_pwm_position_to_pulse_width_us(&config, 1001, &pulse_width_us));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      cb_servo_pwm_position_to_pulse_width_us(&config, -1001, &pulse_width_us));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      cb_servo_pwm_position_to_pulse_width_us(NULL, 0, &pulse_width_us));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      cb_servo_pwm_position_to_pulse_width_us(&config, 0, NULL));
}

static void test_servo_rejects_invalid_calibration(void)
{
    cb_servo_pwm_config_t config = get_valid_test_config(CB_SERVO_ROLE_RUDDER);

    config.center_pulse_width_us = config.min_pulse_width_us;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_servo_pwm_validate_config(&config));

    config = get_valid_test_config(CB_SERVO_ROLE_RUDDER);
    config.max_pulse_width_us = 20000U;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_servo_pwm_validate_config(&config));
}

static void test_servo_validates_esp32_output_gpio_and_led_conflict(void)
{
    cb_servo_pwm_config_t config = get_valid_test_config(CB_SERVO_ROLE_RUDDER);

    config.gpio_num = -1;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_servo_pwm_validate_config(&config));

    config = get_valid_test_config(CB_SERVO_ROLE_RUDDER);
    config.gpio_num = CONFIG_CYBERBOAT_STATUS_LED_GPIO;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_servo_pwm_validate_config(&config));

#ifdef CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    config = get_valid_test_config(CB_SERVO_ROLE_RUDDER);
    config.gpio_num = 19;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_servo_pwm_validate_config(&config));
    config.gpio_num = 20;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_servo_pwm_validate_config(&config));
#endif
}

static void test_servo_bind_initializes_hal_without_touching_hardware(void)
{
    cb_servo_pwm_driver_t driver = {};
    const cb_servo_pwm_config_t config = get_valid_test_config(CB_SERVO_ROLE_RUDDER);
    cb_servo_hal_t servo_hal = {};

    TEST_ASSERT_EQUAL(ESP_OK, cb_servo_pwm_bind(&driver, &config, &servo_hal));
    TEST_ASSERT_NOT_NULL(servo_hal.context);
    TEST_ASSERT_NOT_NULL(servo_hal.init);
    TEST_ASSERT_NOT_NULL(servo_hal.set_position);
    TEST_ASSERT_NOT_NULL(servo_hal.set_pulse_width_us);
    TEST_ASSERT_FALSE(driver.initialized);
}

static void test_servo_initialization_obeys_actuator_safety_gate(void)
{
    cb_servo_pwm_driver_t driver = {};
    const cb_servo_pwm_config_t config = get_valid_test_config(CB_SERVO_ROLE_RUDDER);
    cb_servo_hal_t servo_hal = {};

    TEST_ASSERT_EQUAL(ESP_OK, cb_servo_pwm_bind(&driver, &config, &servo_hal));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, servo_hal.init(servo_hal.context));
    TEST_ASSERT_FALSE(driver.initialized);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, driver.last_error);
}

static void test_esp32_servo_timer_and_channel_defaults_are_valid(void)
{
    cb_servo_pwm_config_t config = {};
    cb_servo_pwm_config_t auxiliary_config = {};

    TEST_ASSERT_EQUAL(ESP_OK,
                      cb_servo_pwm_get_default_config(CB_SERVO_ROLE_RUDDER, &config));
    TEST_ASSERT_EQUAL_UINT8(CONFIG_CYBERBOAT_RUDDER_SERVO_LEDC_TIMER, config.ledc_timer);
    TEST_ASSERT_EQUAL_UINT8(CONFIG_CYBERBOAT_RUDDER_SERVO_LEDC_CHANNEL, config.ledc_channel);
    TEST_ASSERT_EQUAL(ESP_OK,
                      cb_servo_pwm_get_default_config(CB_SERVO_ROLE_AUXILIARY, &auxiliary_config));
    TEST_ASSERT_EQUAL_UINT8(CONFIG_CYBERBOAT_AUX_SERVO_LEDC_TIMER, auxiliary_config.ledc_timer);
    TEST_ASSERT_EQUAL_UINT8(CONFIG_CYBERBOAT_AUX_SERVO_LEDC_CHANNEL, auxiliary_config.ledc_channel);
    TEST_ASSERT_NOT_EQUAL(config.gpio_num, auxiliary_config.gpio_num);
    TEST_ASSERT_EQUAL(ESP_OK, cb_servo_pwm_validate_config(&config));
    TEST_ASSERT_EQUAL(ESP_OK, cb_servo_pwm_validate_config(&auxiliary_config));
}

static void test_servo_roles_reject_shared_ledc_channels_and_conflicting_timer_frequency(void)
{
    cb_servo_pwm_config_t config = get_valid_test_config(CB_SERVO_ROLE_RUDDER);
    const cb_servo_pwm_config_t auxiliary_config =
        get_valid_test_config(CB_SERVO_ROLE_AUXILIARY);

    config.ledc_channel = auxiliary_config.ledc_channel;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_servo_pwm_validate_config(&config));

    config = get_valid_test_config(CB_SERVO_ROLE_RUDDER);
    config.ledc_timer = auxiliary_config.ledc_timer;
    config.frequency_hz = auxiliary_config.frequency_hz + 1U;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_servo_pwm_validate_config(&config));
}

void cb_servo_pwm_run_unit_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_servo_maps_axis_endpoints_and_center);
    RUN_TEST(test_servo_maps_intermediate_positions);
    RUN_TEST(test_servo_rejects_out_of_range_position_and_null_arguments);
    RUN_TEST(test_servo_rejects_invalid_calibration);
    RUN_TEST(test_servo_validates_esp32_output_gpio_and_led_conflict);
    RUN_TEST(test_servo_bind_initializes_hal_without_touching_hardware);
    RUN_TEST(test_servo_initialization_obeys_actuator_safety_gate);
    RUN_TEST(test_esp32_servo_timer_and_channel_defaults_are_valid);
    RUN_TEST(test_servo_roles_reject_shared_ledc_channels_and_conflicting_timer_frequency);
    (void)UNITY_END();
}