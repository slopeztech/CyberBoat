/** @file test_control.c
 *  @brief Unity tests for compile-time vessel-mode resolution.
 */

#include "cyberboat/control.h"

#include "sdkconfig.h"
#include "unity.h"

static void test_vessel_mode_resolves_selected_capabilities(void)
{
    cb_vessel_mode_config_t mode = {};

    TEST_ASSERT_EQUAL(ESP_OK, cb_control_get_vessel_mode(&mode));
    TEST_ASSERT_NOT_NULL(mode.name);
    TEST_ASSERT_EQUAL_UINT8(CONFIG_CYBERBOAT_MOTOR_COUNT, mode.motor_count);
#ifdef CONFIG_CYBERBOAT_RUDDER_SERVO_ENABLED
    TEST_ASSERT_TRUE(mode.rudder_servo_enabled);
#else
    TEST_ASSERT_FALSE(mode.rudder_servo_enabled);
#endif
#ifdef CONFIG_CYBERBOAT_AUX_SERVO_ENABLED
    TEST_ASSERT_TRUE(mode.auxiliary_servo_enabled);
#else
    TEST_ASSERT_FALSE(mode.auxiliary_servo_enabled);
#endif

#if CONFIG_CYBERBOAT_MODE_SINGLE_MOTOR_ONLY
    TEST_ASSERT_EQUAL(CB_VESSEL_MODE_SINGLE_MOTOR_ONLY, mode.id);
#elif CONFIG_CYBERBOAT_MODE_SINGLE_MOTOR_RUDDER
    TEST_ASSERT_EQUAL(CB_VESSEL_MODE_SINGLE_MOTOR_RUDDER, mode.id);
#elif CONFIG_CYBERBOAT_MODE_SINGLE_MOTOR_AUX_SERVO
    TEST_ASSERT_EQUAL(CB_VESSEL_MODE_SINGLE_MOTOR_AUX_SERVO, mode.id);
#elif CONFIG_CYBERBOAT_MODE_SINGLE_MOTOR_RUDDER_AUX
    TEST_ASSERT_EQUAL(CB_VESSEL_MODE_SINGLE_MOTOR_RUDDER_AUX, mode.id);
#elif CONFIG_CYBERBOAT_MODE_TWIN_MOTOR_ONLY
    TEST_ASSERT_EQUAL(CB_VESSEL_MODE_TWIN_MOTOR_ONLY, mode.id);
#elif CONFIG_CYBERBOAT_MODE_TWIN_MOTOR_RUDDER
    TEST_ASSERT_EQUAL(CB_VESSEL_MODE_TWIN_MOTOR_RUDDER, mode.id);
#elif CONFIG_CYBERBOAT_MODE_TWIN_MOTOR_AUX_SERVO
    TEST_ASSERT_EQUAL(CB_VESSEL_MODE_TWIN_MOTOR_AUX_SERVO, mode.id);
#elif CONFIG_CYBERBOAT_MODE_TWIN_MOTOR_RUDDER_AUX
    TEST_ASSERT_EQUAL(CB_VESSEL_MODE_TWIN_MOTOR_RUDDER_AUX, mode.id);
#else
#error "Select exactly one vessel mode for this test"
#endif
}

static void test_vessel_mode_rejects_null_output(void)
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_control_get_vessel_mode(NULL));
}

static void test_motor_mixer_respects_selected_steering_topology(void)
{
    const cb_pilot_command_t command = {.throttle = 400, .steering = 300};
    cb_motor_outputs_t outputs = {};

    TEST_ASSERT_EQUAL(ESP_OK, cb_control_mix_motors(&command, &outputs));
    TEST_ASSERT_EQUAL_INT16(400, outputs.motor[0]);
#if CONFIG_CYBERBOAT_MOTOR_COUNT == 1
    TEST_ASSERT_EQUAL_INT16(0, outputs.motor[1]);
#elif defined(CONFIG_CYBERBOAT_RUDDER_SERVO_ENABLED)
    TEST_ASSERT_EQUAL_INT16(400, outputs.motor[1]);
#else
    TEST_ASSERT_EQUAL_INT16(700, outputs.motor[1]);
#endif
}

void cb_control_run_unit_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_vessel_mode_resolves_selected_capabilities);
    RUN_TEST(test_vessel_mode_rejects_null_output);
    RUN_TEST(test_motor_mixer_respects_selected_steering_topology);
    (void)UNITY_END();
}