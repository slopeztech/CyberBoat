/** @file test_receiver_drive.c
 *  @brief Unity tests for configurable receiver-to-drive mapping.
 */

#include "cyberboat/receiver_drive.h"

#include "unity.h"

static cb_receiver_drive_config_t get_drive_config(void)
{
    cb_receiver_drive_config_t config = {};

    config.servo_channel = 0U;
    config.throttle_channel = 1U;
    config.direction_channel = 2U;
    config.steering_channel = 3U;
    config.left_motor_index = 0U;
    config.deadband_us = 30U;
    config.frame_timeout_ms = 100U;
    config.servo_travel_percent = 50U;
    config.servo.min_pulse_us = 976U;
    config.servo.center_pulse_us = 1492U;
    config.servo.max_pulse_us = 1933U;
    config.throttle.min_pulse_us = 1056U;
    config.throttle.center_pulse_us = 1377U;
    config.throttle.max_pulse_us = 1698U;
    config.direction.min_pulse_us = 1096U;
    config.direction.center_pulse_us = 1472U;
    config.direction.max_pulse_us = 1924U;
    config.steering.min_pulse_us = 993U;
    config.steering.center_pulse_us = 1508U;
    config.steering.max_pulse_us = 1950U;

    return config;
}

static cb_receiver_raw_frame_t get_neutral_frame(void)
{
    cb_receiver_raw_frame_t raw = {};

    raw.channel_count = 4U;
    raw.valid_channels_mask = 0xFU;
    raw.pulse_width_us[0] = 1492U;
    raw.pulse_width_us[1] = 1056U;
    raw.pulse_width_us[2] = 1472U;
    raw.pulse_width_us[3] = 1508U;
    raw.channel_age_ms[0] = 5U;
    raw.channel_age_ms[1] = 5U;
    raw.channel_age_ms[2] = 5U;
    raw.channel_age_ms[3] = 5U;

    return raw;
}

static void test_receiver_drive_rejects_null_and_invalid_config(void)
{
    cb_receiver_drive_config_t config = get_drive_config();
    cb_receiver_raw_frame_t raw = get_neutral_frame();
    cb_receiver_drive_command_t command = {};

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_receiver_drive_get_default_config(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_receiver_drive_validate_config(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_receiver_drive_map(NULL, &config, &command));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_receiver_drive_map(&raw, NULL, &command));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_receiver_drive_map(&raw, &config, NULL));

    config.steering_channel = config.direction_channel;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_receiver_drive_validate_config(&config));

    config.steering_channel = 3U;
    config.left_motor_index = CB_MAX_MOTORS;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_receiver_drive_validate_config(&config));
}

static void test_receiver_drive_maps_forward_throttle_and_right_turn(void)
{
    cb_receiver_drive_config_t config = get_drive_config();
    cb_receiver_raw_frame_t raw = get_neutral_frame();
    cb_receiver_drive_command_t command = {};

    raw.pulse_width_us[0] = config.servo.max_pulse_us;
    raw.pulse_width_us[1] = config.throttle.max_pulse_us;
    raw.pulse_width_us[2] = config.direction.max_pulse_us;
    raw.pulse_width_us[3] = config.steering.max_pulse_us;

    TEST_ASSERT_EQUAL(ESP_OK, cb_receiver_drive_map(&raw, &config, &command));
    TEST_ASSERT_EQUAL_INT16(CB_AXIS_MAX, command.motors.motor[0]);
    TEST_ASSERT_EQUAL_INT16(0, command.motors.motor[1]);
    TEST_ASSERT_EQUAL_INT16(500, command.auxiliary_servo);
    TEST_ASSERT_FALSE(command.rearm_safe);
    TEST_ASSERT_EQUAL_UINT32(95U, command.valid_for_ms);
}

static void test_receiver_drive_maps_reverse_and_left_turn(void)
{
    cb_receiver_drive_config_t config = get_drive_config();
    cb_receiver_raw_frame_t raw = get_neutral_frame();
    cb_receiver_drive_command_t command = {};

    raw.pulse_width_us[1] = config.throttle.max_pulse_us;
    raw.pulse_width_us[2] = config.direction.min_pulse_us;
    raw.pulse_width_us[3] = config.steering.min_pulse_us;

    TEST_ASSERT_EQUAL(ESP_OK, cb_receiver_drive_map(&raw, &config, &command));
    TEST_ASSERT_EQUAL_INT16(0, command.motors.motor[0]);
    TEST_ASSERT_EQUAL_INT16(-CB_AXIS_MAX, command.motors.motor[1]);
}

static void test_receiver_drive_multiplies_throttle_by_signed_direction_axis(void)
{
    cb_receiver_drive_config_t config = get_drive_config();
    cb_receiver_raw_frame_t raw = get_neutral_frame();
    cb_receiver_drive_command_t command = {};

    raw.pulse_width_us[1] = 1392U;
    raw.pulse_width_us[2] = 1713U;

    TEST_ASSERT_EQUAL(ESP_OK, cb_receiver_drive_map(&raw, &config, &command));
    TEST_ASSERT_EQUAL_INT16(250, command.motors.motor[0]);
    TEST_ASSERT_EQUAL_INT16(250, command.motors.motor[1]);

    raw.pulse_width_us[2] = 1269U;
    TEST_ASSERT_EQUAL(ESP_OK, cb_receiver_drive_map(&raw, &config, &command));
    TEST_ASSERT_EQUAL_INT16(-250, command.motors.motor[0]);
    TEST_ASSERT_EQUAL_INT16(-250, command.motors.motor[1]);
}

static void test_receiver_drive_swaps_physical_left_motor_assignment(void)
{
    cb_receiver_drive_config_t config = get_drive_config();
    cb_receiver_raw_frame_t raw = get_neutral_frame();
    cb_receiver_drive_command_t command = {};

    config.left_motor_index = 1U;
    raw.pulse_width_us[1] = config.throttle.max_pulse_us;
    raw.pulse_width_us[2] = config.direction.max_pulse_us;
    raw.pulse_width_us[3] = config.steering.max_pulse_us;

    TEST_ASSERT_EQUAL(ESP_OK, cb_receiver_drive_map(&raw, &config, &command));
    TEST_ASSERT_EQUAL_INT16(0, command.motors.motor[0]);
    TEST_ASSERT_EQUAL_INT16(CB_AXIS_MAX, command.motors.motor[1]);
}

static void test_receiver_drive_throttle_zero_overrides_direction(void)
{
    cb_receiver_drive_config_t config = get_drive_config();
    cb_receiver_raw_frame_t raw = get_neutral_frame();
    cb_receiver_drive_command_t command = {};

    raw.pulse_width_us[1] = config.throttle.min_pulse_us;
    raw.pulse_width_us[2] = config.direction.max_pulse_us;
    raw.pulse_width_us[3] = config.steering.max_pulse_us;

    TEST_ASSERT_EQUAL(ESP_OK, cb_receiver_drive_map(&raw, &config, &command));
    TEST_ASSERT_EQUAL_INT16(0, command.motors.motor[0]);
    TEST_ASSERT_EQUAL_INT16(0, command.motors.motor[1]);
    TEST_ASSERT_FALSE(command.rearm_safe);
}

static void test_receiver_drive_requires_safe_neutral_to_rearm(void)
{
    cb_receiver_drive_config_t config = get_drive_config();
    cb_receiver_raw_frame_t raw = get_neutral_frame();
    cb_receiver_drive_command_t command = {};

    TEST_ASSERT_EQUAL(ESP_OK, cb_receiver_drive_map(&raw, &config, &command));
    TEST_ASSERT_EQUAL_INT16(0, command.motors.motor[0]);
    TEST_ASSERT_EQUAL_INT16(0, command.motors.motor[1]);
    TEST_ASSERT_TRUE(command.rearm_safe);
}

static void test_receiver_drive_limits_motor_outputs_to_throttle(void)
{
    cb_receiver_drive_config_t config = get_drive_config();
    cb_receiver_raw_frame_t raw = get_neutral_frame();
    cb_receiver_drive_command_t command = {};
    int32_t left_magnitude;
    int32_t right_magnitude;

    raw.pulse_width_us[1] = 1350U;
    raw.pulse_width_us[2] = config.direction.max_pulse_us;
    raw.pulse_width_us[3] = config.steering.max_pulse_us;

    TEST_ASSERT_EQUAL(ESP_OK, cb_receiver_drive_map(&raw, &config, &command));
    left_magnitude = command.motors.motor[0];
    right_magnitude = command.motors.motor[1];
    if (left_magnitude < 0)
    {
        left_magnitude = -left_magnitude;
    }
    if (right_magnitude < 0)
    {
        right_magnitude = -right_magnitude;
    }

    TEST_ASSERT_TRUE(left_magnitude <= 500);
    TEST_ASSERT_TRUE(right_magnitude <= 500);
    TEST_ASSERT_TRUE(left_magnitude >= right_magnitude);
}

static void test_receiver_drive_fails_safe_for_stale_or_missing_channel(void)
{
    cb_receiver_drive_config_t config = get_drive_config();
    cb_receiver_raw_frame_t raw = get_neutral_frame();
    cb_receiver_drive_command_t command = {};

    command.motors.motor[0] = 200;
    command.motors.motor[1] = -200;
    command.auxiliary_servo = 500;
    raw.channel_age_ms[2] = config.frame_timeout_ms;

    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, cb_receiver_drive_map(&raw, &config, &command));
    TEST_ASSERT_EQUAL_INT16(0, command.motors.motor[0]);
    TEST_ASSERT_EQUAL_INT16(0, command.motors.motor[1]);
    TEST_ASSERT_EQUAL_INT16(0, command.auxiliary_servo);
    TEST_ASSERT_FALSE(command.rearm_safe);

    raw = get_neutral_frame();
    raw.valid_channels_mask = (uint16_t)(raw.valid_channels_mask & (uint16_t)~0x8U);
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, cb_receiver_drive_map(&raw, &config, &command));
}

static void test_receiver_drive_rejects_invalid_raw_frame(void)
{
    cb_receiver_drive_config_t config = get_drive_config();
    cb_receiver_raw_frame_t raw = get_neutral_frame();
    cb_receiver_drive_command_t command = {};

    raw.channel_count = 0U;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_receiver_drive_map(&raw, &config, &command));
}

void cb_receiver_drive_run_unit_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_receiver_drive_rejects_null_and_invalid_config);
    RUN_TEST(test_receiver_drive_maps_forward_throttle_and_right_turn);
    RUN_TEST(test_receiver_drive_maps_reverse_and_left_turn);
    RUN_TEST(test_receiver_drive_multiplies_throttle_by_signed_direction_axis);
    RUN_TEST(test_receiver_drive_swaps_physical_left_motor_assignment);
    RUN_TEST(test_receiver_drive_throttle_zero_overrides_direction);
    RUN_TEST(test_receiver_drive_requires_safe_neutral_to_rearm);
    RUN_TEST(test_receiver_drive_limits_motor_outputs_to_throttle);
    RUN_TEST(test_receiver_drive_fails_safe_for_stale_or_missing_channel);
    RUN_TEST(test_receiver_drive_rejects_invalid_raw_frame);
    (void)UNITY_END();
}
