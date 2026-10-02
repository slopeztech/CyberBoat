/** @file test_hbridge_motor.c
 *  @brief Unity tests for pure H-bridge mapping and configuration checks.
 */

#include "cyberboat/drivers/hbridge_motor.h"

#include "sdkconfig.h"
#include "unity.h"

static void test_zero_output_disables_both_inputs(void)
{
    cb_hbridge_motor_pwm_t pwm = {};
    TEST_ASSERT_EQUAL(ESP_OK, cb_hbridge_motor_map_output(0, false, &pwm));
    TEST_ASSERT_EQUAL_UINT32(0U, pwm.input_1_duty);
    TEST_ASSERT_EQUAL_UINT32(0U, pwm.input_2_duty);
}

static void test_positive_output_drives_input_one_only(void)
{
    cb_hbridge_motor_pwm_t pwm = {};
    TEST_ASSERT_EQUAL(ESP_OK, cb_hbridge_motor_map_output(CB_AXIS_MAX, false, &pwm));
    TEST_ASSERT_EQUAL_UINT32(CB_HBRIDGE_PWM_DUTY_MAX, pwm.input_1_duty);
    TEST_ASSERT_EQUAL_UINT32(0U, pwm.input_2_duty);
}

static void test_negative_output_drives_input_two_only(void)
{
    cb_hbridge_motor_pwm_t pwm = {};
    TEST_ASSERT_EQUAL(ESP_OK, cb_hbridge_motor_map_output(CB_AXIS_MIN, false, &pwm));
    TEST_ASSERT_EQUAL_UINT32(0U, pwm.input_1_duty);
    TEST_ASSERT_EQUAL_UINT32(CB_HBRIDGE_PWM_DUTY_MAX, pwm.input_2_duty);
}

static void test_half_command_maps_to_bounded_duty(void)
{
    cb_hbridge_motor_pwm_t pwm = {};
    TEST_ASSERT_EQUAL(ESP_OK, cb_hbridge_motor_map_output(500, false, &pwm));
    TEST_ASSERT_EQUAL_UINT32(511U, pwm.input_1_duty);
    TEST_ASSERT_EQUAL_UINT32(0U, pwm.input_2_duty);
}

static void test_reversed_configuration_swaps_direction(void)
{
    cb_hbridge_motor_pwm_t pwm = {};
    TEST_ASSERT_EQUAL(ESP_OK, cb_hbridge_motor_map_output(CB_AXIS_MAX, true, &pwm));
    TEST_ASSERT_EQUAL_UINT32(0U, pwm.input_1_duty);
    TEST_ASSERT_EQUAL_UINT32(CB_HBRIDGE_PWM_DUTY_MAX, pwm.input_2_duty);
}

static void test_invalid_range_and_null_output_are_rejected(void)
{
    cb_hbridge_motor_pwm_t pwm = {};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_hbridge_motor_map_output(1001, false, &pwm));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_hbridge_motor_map_output(0, false, NULL));
}

static void test_default_motor_pin_configuration_is_valid(void)
{
    cb_hbridge_motor_config_t config = {};
    TEST_ASSERT_EQUAL(ESP_OK, cb_hbridge_motor_get_default_config(&config));
    TEST_ASSERT_EQUAL(ESP_OK, cb_hbridge_motor_validate_config(&config));
}

static void test_esp32_gpio_conflicts_are_rejected(void)
{
    cb_hbridge_motor_config_t config = {};
    TEST_ASSERT_EQUAL(ESP_OK, cb_hbridge_motor_get_default_config(&config));
    config.input_gpio[0][0] = CONFIG_CYBERBOAT_STATUS_LED_GPIO;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_hbridge_motor_validate_config(&config));

#ifdef CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    TEST_ASSERT_EQUAL(ESP_OK, cb_hbridge_motor_get_default_config(&config));
    config.input_gpio[0][0] = 19;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_hbridge_motor_validate_config(&config));
    config.input_gpio[0][0] = 20;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_hbridge_motor_validate_config(&config));
#endif

    TEST_ASSERT_EQUAL(ESP_OK, cb_hbridge_motor_get_default_config(&config));
    config.input_gpio[0][1] = config.input_gpio[0][0];
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_hbridge_motor_validate_config(&config));
}

#ifndef CONFIG_CYBERBOAT_ACTUATORS_ENABLED
static void test_actuator_initialization_is_blocked_by_default(void)
{
    cb_hbridge_motor_config_t config = {};
    cb_hbridge_motor_driver_t driver = {};
    cb_motor_hal_t motor_hal = {};

    TEST_ASSERT_EQUAL(ESP_OK, cb_hbridge_motor_get_default_config(&config));
    TEST_ASSERT_EQUAL(ESP_OK, cb_hbridge_motor_bind(&driver, &config, &motor_hal));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, motor_hal.init(motor_hal.context));
    TEST_ASSERT_FALSE(driver.initialized);
    TEST_ASSERT_EQUAL_UINT8(0U, driver.configured_channels);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, driver.last_error);
}
#endif

void cb_hbridge_motor_run_unit_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_zero_output_disables_both_inputs);
    RUN_TEST(test_positive_output_drives_input_one_only);
    RUN_TEST(test_negative_output_drives_input_two_only);
    RUN_TEST(test_half_command_maps_to_bounded_duty);
    RUN_TEST(test_reversed_configuration_swaps_direction);
    RUN_TEST(test_invalid_range_and_null_output_are_rejected);
    RUN_TEST(test_default_motor_pin_configuration_is_valid);
    RUN_TEST(test_esp32_gpio_conflicts_are_rejected);
#ifndef CONFIG_CYBERBOAT_ACTUATORS_ENABLED
    RUN_TEST(test_actuator_initialization_is_blocked_by_default);
#endif
    (void)UNITY_END();
}