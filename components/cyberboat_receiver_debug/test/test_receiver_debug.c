/** @file test_receiver_debug.c
 *  @brief Unity tests for the receiver console logging rate limiter and formatters.
 */

#include "cyberboat/receiver_debug.h"

#include "unity.h"

static void test_should_log_rejects_null_arguments(void)
{
    cb_receiver_debug_config_t config = {};
    cb_receiver_debug_state_t state = {};
    bool should_log = false;

    config.enabled = true;
    config.interval_ms = 500U;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      cb_receiver_debug_should_log(NULL, &state, 0U, &should_log));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      cb_receiver_debug_should_log(&config, NULL, 0U, &should_log));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      cb_receiver_debug_should_log(&config, &state, 0U, NULL));
}

static void test_should_log_rejects_invalid_interval(void)
{
    cb_receiver_debug_config_t config = {};
    cb_receiver_debug_state_t state = {};
    bool should_log = false;

    config.enabled = true;
    config.interval_ms = 0U;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      cb_receiver_debug_should_log(&config, &state, 0U, &should_log));

    config.interval_ms = 60001U;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      cb_receiver_debug_should_log(&config, &state, 0U, &should_log));
}

static void test_should_log_is_false_when_disabled(void)
{
    cb_receiver_debug_config_t config = {};
    cb_receiver_debug_state_t state = {};
    bool should_log = true;

    config.enabled = false;
    config.interval_ms = 500U;

    TEST_ASSERT_EQUAL(ESP_OK, cb_receiver_debug_should_log(&config, &state, 1000U, &should_log));
    TEST_ASSERT_FALSE(should_log);
    TEST_ASSERT_FALSE(state.logged_once);
}

static void test_should_log_fires_immediately_on_first_call(void)
{
    cb_receiver_debug_config_t config = {};
    cb_receiver_debug_state_t state = {};
    bool should_log = false;

    config.enabled = true;
    config.interval_ms = 500U;

    TEST_ASSERT_EQUAL(ESP_OK, cb_receiver_debug_should_log(&config, &state, 42U, &should_log));
    TEST_ASSERT_TRUE(should_log);
    TEST_ASSERT_TRUE(state.logged_once);
    TEST_ASSERT_EQUAL_UINT32(42U, state.last_log_ms);
}

static void test_should_log_respects_interval_and_rollover(void)
{
    cb_receiver_debug_config_t config = {};
    cb_receiver_debug_state_t state = {};
    bool should_log = false;

    config.enabled = true;
    config.interval_ms = 500U;

    TEST_ASSERT_EQUAL(ESP_OK, cb_receiver_debug_should_log(&config, &state, 1000U, &should_log));
    TEST_ASSERT_TRUE(should_log);

    TEST_ASSERT_EQUAL(ESP_OK, cb_receiver_debug_should_log(&config, &state, 1200U, &should_log));
    TEST_ASSERT_FALSE(should_log);

    TEST_ASSERT_EQUAL(ESP_OK, cb_receiver_debug_should_log(&config, &state, 1500U, &should_log));
    TEST_ASSERT_TRUE(should_log);

    /* Simulate 32-bit millisecond tick rollover: last_log_ms wrapped near UINT32_MAX. */
    state.last_log_ms = 0xFFFFFF00U;
    TEST_ASSERT_EQUAL(ESP_OK,
                      cb_receiver_debug_should_log(&config, &state, 0x00000100U, &should_log));
    TEST_ASSERT_TRUE(should_log);
}

static void test_log_functions_reject_null_arguments(void)
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_receiver_debug_log_raw_frame(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_receiver_debug_log_mapped_command(NULL));
}

static void test_log_functions_accept_valid_data(void)
{
    cb_pwm_receiver_diagnostics_t diagnostics = {};
    cb_receiver_debug_command_t command = {};

    diagnostics.config.channel_count = 2U;
    diagnostics.config.channel_gpio[0] = 7;
    diagnostics.config.channel_gpio[1] = 8;
    diagnostics.pulse_width_us[0] = 1500U;
    diagnostics.pulse_width_us[1] = 1000U;
    diagnostics.valid_channels_mask = 0x1U;

    TEST_ASSERT_EQUAL(ESP_OK, cb_receiver_debug_log_raw_frame(&diagnostics));

    command.motor_left = 250;
    command.motor_right = -100;
    command.auxiliary_servo = 50;
    command.rearm_safe = true;
    command.valid_for_ms = 80U;

    TEST_ASSERT_EQUAL(ESP_OK, cb_receiver_debug_log_mapped_command(&command));
}

void cb_receiver_debug_run_unit_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_should_log_rejects_null_arguments);
    RUN_TEST(test_should_log_rejects_invalid_interval);
    RUN_TEST(test_should_log_is_false_when_disabled);
    RUN_TEST(test_should_log_fires_immediately_on_first_call);
    RUN_TEST(test_should_log_respects_interval_and_rollover);
    RUN_TEST(test_log_functions_reject_null_arguments);
    RUN_TEST(test_log_functions_accept_valid_data);
    (void)UNITY_END();
}
