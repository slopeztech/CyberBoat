/** @file test_receiver_mapping.c
 *  @brief Unity tests for raw PWM to semantic pilot-command mapping.
 */

#include "cyberboat/receiver_mapping.h"

#include "unity.h"

static cb_receiver_mapping_config_t get_valid_mapping_config(void)
{
    cb_receiver_mapping_config_t config = {};
    (void)cb_receiver_mapping_get_default_config(&config);
    return config;
}

static cb_receiver_raw_frame_t get_fresh_four_channel_frame(void)
{
    cb_receiver_raw_frame_t frame = {};
    frame.channel_count = 4U;
    frame.valid_channels_mask = 0x000FU;
    frame.pulse_width_us[0] = 1500U;
    frame.pulse_width_us[1] = 1000U;
    frame.pulse_width_us[2] = 1000U;
    frame.pulse_width_us[3] = 1500U;
    return frame;
}

static void test_default_mapping_selects_throttle_and_steering(void)
{
    const cb_receiver_mapping_config_t config = get_valid_mapping_config();
    TEST_ASSERT_EQUAL_UINT8(1U, config.throttle_channel);
    TEST_ASSERT_EQUAL_UINT8(0U, config.steering_channel);
    #if CONFIG_CYBERBOAT_MOTOR_COUNT == 2 || defined(CONFIG_CYBERBOAT_RUDDER_SERVO_ENABLED)
    TEST_ASSERT_TRUE(config.steering_enabled);
    #else
    TEST_ASSERT_FALSE(config.steering_enabled);
    #endif
    TEST_ASSERT_FALSE(config.arm_enabled);
    TEST_ASSERT_EQUAL(ESP_OK, cb_receiver_mapping_validate_config(&config));
}

static void test_forward_only_throttle_maps_stop_and_full_forward(void)
{
    const cb_receiver_mapping_config_t config = get_valid_mapping_config();
    cb_receiver_raw_frame_t frame = get_fresh_four_channel_frame();
    cb_receiver_frame_t command = {};

    TEST_ASSERT_FALSE(config.throttle_bidirectional);
    TEST_ASSERT_EQUAL(ESP_OK, cb_control_map_receiver(&frame, &config, &command));
    TEST_ASSERT_EQUAL(0, command.command.throttle);
    TEST_ASSERT_EQUAL(0, command.command.steering);
    TEST_ASSERT_FALSE(command.arm_requested);
    TEST_ASSERT_EQUAL_UINT32(config.frame_timeout_ms, command.valid_for_ms);

    frame.pulse_width_us[config.throttle_channel] = config.throttle.max_pulse_us;
    TEST_ASSERT_EQUAL(ESP_OK, cb_control_map_receiver(&frame, &config, &command));
    TEST_ASSERT_EQUAL(CB_AXIS_MAX, command.command.throttle);
}

static void test_centered_throttle_maps_reverse_and_forward_endpoints(void)
{
    cb_receiver_mapping_config_t config = get_valid_mapping_config();
    cb_receiver_raw_frame_t frame = get_fresh_four_channel_frame();
    cb_receiver_frame_t command = {};

    config.throttle_bidirectional = true;
    frame.pulse_width_us[config.throttle_channel] = config.throttle.min_pulse_us;
    TEST_ASSERT_EQUAL(ESP_OK, cb_control_map_receiver(&frame, &config, &command));
    TEST_ASSERT_EQUAL(CB_AXIS_MIN, command.command.throttle);

    frame.pulse_width_us[config.throttle_channel] = config.throttle.max_pulse_us;
    TEST_ASSERT_EQUAL(ESP_OK, cb_control_map_receiver(&frame, &config, &command));
    TEST_ASSERT_EQUAL(CB_AXIS_MAX, command.command.throttle);
}

static void test_steering_maps_left_and_right_endpoints(void)
{
    cb_receiver_mapping_config_t config = get_valid_mapping_config();
    cb_receiver_raw_frame_t frame = get_fresh_four_channel_frame();
    cb_receiver_frame_t command = {};

    config.steering_enabled = true;
    frame.pulse_width_us[config.steering_channel] = config.steering.min_pulse_us;
    TEST_ASSERT_EQUAL(ESP_OK, cb_control_map_receiver(&frame, &config, &command));
    TEST_ASSERT_EQUAL(CB_AXIS_MIN, command.command.steering);

    frame.pulse_width_us[config.steering_channel] = config.steering.max_pulse_us;
    TEST_ASSERT_EQUAL(ESP_OK, cb_control_map_receiver(&frame, &config, &command));
    TEST_ASSERT_EQUAL(CB_AXIS_MAX, command.command.steering);
}

static void test_deadband_maps_center_noise_to_zero(void)
{
    cb_receiver_mapping_config_t config = get_valid_mapping_config();
    cb_receiver_raw_frame_t frame = get_fresh_four_channel_frame();
    cb_receiver_frame_t command = {};

    config.steering_enabled = true;
    frame.pulse_width_us[config.steering_channel] =
        (uint16_t)(config.steering.center_pulse_us - config.deadband_us);
    TEST_ASSERT_EQUAL(ESP_OK, cb_control_map_receiver(&frame, &config, &command));
    TEST_ASSERT_EQUAL(0, command.command.steering);

    frame.pulse_width_us[config.steering_channel] =
        (uint16_t)(config.steering.center_pulse_us + config.deadband_us);
    TEST_ASSERT_EQUAL(ESP_OK, cb_control_map_receiver(&frame, &config, &command));
    TEST_ASSERT_EQUAL(0, command.command.steering);
}

static void test_reversal_inverts_mapped_axis(void)
{
    cb_receiver_mapping_config_t config = get_valid_mapping_config();
    cb_receiver_raw_frame_t frame = get_fresh_four_channel_frame();
    cb_receiver_frame_t command = {};

    config.steering_reversed = true;
    frame.pulse_width_us[config.steering_channel] = config.steering.max_pulse_us;
    TEST_ASSERT_EQUAL(ESP_OK, cb_control_map_receiver(&frame, &config, &command));
    TEST_ASSERT_EQUAL(CB_AXIS_MIN, command.command.steering);
}

static void test_stale_or_missing_required_channel_fails_safe(void)
{
    cb_receiver_mapping_config_t config = get_valid_mapping_config();
    cb_receiver_raw_frame_t frame = get_fresh_four_channel_frame();
    cb_receiver_frame_t command = {};

    config.steering_enabled = true;
    frame.channel_age_ms[config.throttle_channel] = (uint32_t)config.frame_timeout_ms + 1U;
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, cb_control_map_receiver(&frame, &config, &command));
    TEST_ASSERT_EQUAL(0, command.command.throttle);
    TEST_ASSERT_EQUAL(0, command.command.steering);
    TEST_ASSERT_FALSE(command.arm_requested);

    frame = get_fresh_four_channel_frame();
    frame.channel_age_ms[config.throttle_channel] = config.frame_timeout_ms;
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, cb_control_map_receiver(&frame, &config, &command));

    frame = get_fresh_four_channel_frame();
    frame.pulse_width_us[config.throttle_channel] = config.throttle.max_pulse_us;
    frame.valid_channels_mask &= (uint16_t)~(uint16_t)(1U << config.steering_channel);
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, cb_control_map_receiver(&frame, &config, &command));
    TEST_ASSERT_EQUAL(0, command.command.throttle);
    TEST_ASSERT_EQUAL(0, command.command.steering);
}

static void test_unused_steering_channel_is_not_required(void)
{
    cb_receiver_mapping_config_t config = get_valid_mapping_config();
    cb_receiver_raw_frame_t frame = get_fresh_four_channel_frame();
    cb_receiver_frame_t command = {};

    config.steering_enabled = false;
    config.auxiliary_servo_enabled = false;
    frame.channel_count = 2U;
    frame.valid_channels_mask = (uint16_t)(1U << config.throttle_channel);
    frame.pulse_width_us[config.throttle_channel] = config.throttle.max_pulse_us;

    TEST_ASSERT_EQUAL(ESP_OK, cb_control_map_receiver(&frame, &config, &command));
    TEST_ASSERT_EQUAL(CB_AXIS_MAX, command.command.throttle);
    TEST_ASSERT_EQUAL(0, command.command.steering);
}

static void test_auxiliary_channel_requests_arm_at_threshold(void)
{
    cb_receiver_mapping_config_t config = get_valid_mapping_config();
    cb_receiver_raw_frame_t frame = get_fresh_four_channel_frame();
    cb_receiver_frame_t command = {};

    config.arm_enabled = true;
    frame.pulse_width_us[config.arm_channel] = config.arm_threshold_us;
    TEST_ASSERT_EQUAL(ESP_OK, cb_control_map_receiver(&frame, &config, &command));
    TEST_ASSERT_TRUE(command.arm_requested);

    frame.pulse_width_us[config.arm_channel] = (uint16_t)(config.arm_threshold_us - 1U);
    TEST_ASSERT_EQUAL(ESP_OK, cb_control_map_receiver(&frame, &config, &command));
    TEST_ASSERT_FALSE(command.arm_requested);
}

static void test_invalid_mapping_pulse_and_arguments_are_rejected(void)
{
    cb_receiver_mapping_config_t config = get_valid_mapping_config();
    cb_receiver_raw_frame_t frame = get_fresh_four_channel_frame();
    cb_receiver_frame_t command = {};

    config.steering_enabled = true;
    config.steering_channel = config.throttle_channel;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_receiver_mapping_validate_config(&config));

    config = get_valid_mapping_config();
    config.deadband_us = (uint16_t)(config.throttle.max_pulse_us - config.throttle.min_pulse_us);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_receiver_mapping_validate_config(&config));

    config = get_valid_mapping_config();
    frame.pulse_width_us[config.throttle_channel] =
        (uint16_t)(config.throttle.max_pulse_us + 1U);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_control_map_receiver(&frame, &config, &command));
    TEST_ASSERT_EQUAL(0, command.command.throttle);
    TEST_ASSERT_EQUAL(0, command.command.steering);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cb_control_map_receiver(NULL, &config, &command));
}

void cb_receiver_mapping_run_unit_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_default_mapping_selects_throttle_and_steering);
    RUN_TEST(test_forward_only_throttle_maps_stop_and_full_forward);
    RUN_TEST(test_centered_throttle_maps_reverse_and_forward_endpoints);
    RUN_TEST(test_steering_maps_left_and_right_endpoints);
    RUN_TEST(test_deadband_maps_center_noise_to_zero);
    RUN_TEST(test_reversal_inverts_mapped_axis);
    RUN_TEST(test_stale_or_missing_required_channel_fails_safe);
    RUN_TEST(test_unused_steering_channel_is_not_required);
    RUN_TEST(test_auxiliary_channel_requests_arm_at_threshold);
    RUN_TEST(test_invalid_mapping_pulse_and_arguments_are_rejected);
    (void)UNITY_END();
}