/** @file hardware_test.c
 *  @brief Poll-driven diagnostics for receiver and servo hardware.
 */

#include "cyberboat/hardware_test.h"

#include "cyberboat/drivers/pwm_receiver.h"
#if CONFIG_CYBERBOAT_HARDWARE_TEST_HBRIDGE || CONFIG_CYBERBOAT_RUN_UNIT_TESTS
#include "cyberboat/drivers/hbridge_motor.h"
#endif
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include <inttypes.h>

#define CB_SERVO_TEST_STEP_INTERVAL_MS 1000U
#define CB_RECEIVER_TEST_LOG_INTERVAL_MS 1000U
#define CB_HBRIDGE_TEST_RAMP_STEP_MS 300U
#define CB_HBRIDGE_TEST_ZERO_DWELL_MS 1000U

static const char *TAG = "hardware_test";

typedef struct
{
    cb_servo_pwm_driver_t driver;
    cb_servo_pwm_config_t config;
    cb_servo_hal_t hal;
    cb_servo_role_t role;
    cb_servo_test_mode_t mode;
    cb_axis_t position;
    uint8_t step_index;
    TickType_t last_step_tick;
    bool started;
} cb_servo_test_state_t;

typedef struct
{
    cb_pwm_receiver_driver_t driver;
    cb_pwm_receiver_config_t config;
    cb_receiver_hal_t hal;
    TickType_t last_report_tick;
    bool started;
} cb_receiver_test_state_t;

#if CONFIG_CYBERBOAT_HARDWARE_TEST_HBRIDGE || CONFIG_CYBERBOAT_RUN_UNIT_TESTS
typedef struct
{
    cb_axis_t output_both_motors;
    uint16_t duration_ms;
} cb_hbridge_test_step_t;

typedef struct
{
    cb_hbridge_motor_driver_t driver;
    cb_hbridge_motor_config_t config;
    cb_motor_hal_t hal;
    uint8_t step_index;
    TickType_t last_step_tick;
    bool started;
    bool completed;
} cb_hbridge_test_state_t;

static const cb_hbridge_test_step_t s_hbridge_test_steps[] = {
    {0, CB_HBRIDGE_TEST_ZERO_DWELL_MS},
    {100, CB_HBRIDGE_TEST_RAMP_STEP_MS},
    {200, CB_HBRIDGE_TEST_RAMP_STEP_MS},
    {300, CB_HBRIDGE_TEST_RAMP_STEP_MS},
    {400, CB_HBRIDGE_TEST_RAMP_STEP_MS},
    {500, CB_HBRIDGE_TEST_RAMP_STEP_MS},
    {600, CB_HBRIDGE_TEST_RAMP_STEP_MS},
    {700, CB_HBRIDGE_TEST_RAMP_STEP_MS},
    {800, CB_HBRIDGE_TEST_RAMP_STEP_MS},
    {900, CB_HBRIDGE_TEST_RAMP_STEP_MS},
    {CB_AXIS_MAX, CB_HBRIDGE_TEST_RAMP_STEP_MS},
    {0, CB_HBRIDGE_TEST_ZERO_DWELL_MS},
};
#endif

static cb_servo_test_state_t s_servo_test;
static cb_receiver_test_state_t s_receiver_test;
#if CONFIG_CYBERBOAT_HARDWARE_TEST_HBRIDGE || CONFIG_CYBERBOAT_RUN_UNIT_TESTS
static cb_hbridge_test_state_t s_hbridge_test;
#endif

#if CONFIG_CYBERBOAT_HARDWARE_TEST_HBRIDGE || CONFIG_CYBERBOAT_RUN_UNIT_TESTS
static esp_err_t cb_hardware_test_set_both_motor_outputs(cb_axis_t output)
{
    esp_err_t result = s_hbridge_test.hal.set_output(
        s_hbridge_test.hal.context, 0U, output);

    if (result == ESP_OK)
    {
        result = s_hbridge_test.hal.set_output(
            s_hbridge_test.hal.context, 1U, output);
    }
    if (result != ESP_OK)
    {
        (void)s_hbridge_test.hal.emergency_stop(s_hbridge_test.hal.context);
    }

    return result;
}
#endif

esp_err_t cb_hardware_test_get_servo_step(
    cb_servo_test_mode_t mode,
    uint8_t step_index,
    cb_axis_t *position)
{
    static const cb_axis_t small_steps[] = {0, 50, 0, -50};

    if (position == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (mode == CB_SERVO_TEST_MODE_CENTER)
    {
        if (step_index != 0U)
        {
            return ESP_ERR_INVALID_ARG;
        }
        *position = 0;
        return ESP_OK;
    }

    if (mode == CB_SERVO_TEST_MODE_SMALL_STEPS)
    {
        if (step_index >= (uint8_t)(sizeof(small_steps) / sizeof(small_steps[0])))
        {
            return ESP_ERR_INVALID_ARG;
        }
        *position = small_steps[step_index];
        return ESP_OK;
    }

    return ESP_ERR_INVALID_ARG;
}

esp_err_t cb_hardware_test_start_servo_test(cb_servo_role_t role, cb_servo_test_mode_t mode)
{
    cb_axis_t initial_position;
    esp_err_t result;

    if (s_servo_test.started)
    {
        return ESP_ERR_INVALID_STATE;
    }

#ifndef CONFIG_CYBERBOAT_ACTUATORS_ENABLED
    (void)role;
    (void)mode;
    return ESP_ERR_NOT_SUPPORTED;
#endif

    result = cb_hardware_test_get_servo_step(mode, 0U, &initial_position);
    if (result != ESP_OK)
    {
        return result;
    }

    result = cb_servo_pwm_get_default_config(role, &s_servo_test.config);
    if (result != ESP_OK)
    {
        return result;
    }

    result = cb_servo_pwm_bind(&s_servo_test.driver, &s_servo_test.config, &s_servo_test.hal);
    if (result != ESP_OK)
    {
        return result;
    }

    result = s_servo_test.hal.init(s_servo_test.hal.context);
    if (result != ESP_OK)
    {
        return result;
    }

    result = s_servo_test.hal.set_position(s_servo_test.hal.context, initial_position);
    if (result != ESP_OK)
    {
        return result;
    }

    s_servo_test.role = role;
    s_servo_test.mode = mode;
    s_servo_test.position = initial_position;
    s_servo_test.step_index = 0U;
    s_servo_test.last_step_tick = xTaskGetTickCount();
    s_servo_test.started = true;

    ESP_LOGI(TAG,
             "Servo test started: role=%s gpio=%" PRId32 " mode=%s freq=%" PRIu32
             "Hz pulse=%" PRIu32 "us",
             (role == CB_SERVO_ROLE_AUXILIARY) ? "auxiliary" : "rudder",
             s_servo_test.config.gpio_num,
             (mode == CB_SERVO_TEST_MODE_CENTER) ? "center" : "small-steps",
             s_servo_test.config.frequency_hz,
             s_servo_test.driver.last_pulse_width_us);

    return ESP_OK;
}

esp_err_t cb_hardware_test_poll_servo_test(void)
{
    const TickType_t current_tick = xTaskGetTickCount();
    cb_axis_t next_position;
    uint8_t next_step;
    esp_err_t result;

    if (!s_servo_test.started)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_servo_test.mode == CB_SERVO_TEST_MODE_CENTER)
    {
        return ESP_OK;
    }
    if ((TickType_t)(current_tick - s_servo_test.last_step_tick) <
        pdMS_TO_TICKS(CB_SERVO_TEST_STEP_INTERVAL_MS))
    {
        return ESP_OK;
    }

    next_step = (uint8_t)((s_servo_test.step_index + 1U) % 4U);
    result = cb_hardware_test_get_servo_step(s_servo_test.mode, next_step, &next_position);
    if (result != ESP_OK)
    {
        return result;
    }

    result = s_servo_test.hal.set_position(s_servo_test.hal.context, next_position);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Servo test step failed: %s", esp_err_to_name(result));
        return result;
    }

    s_servo_test.step_index = next_step;
    s_servo_test.position = next_position;
    s_servo_test.last_step_tick = current_tick;
    ESP_LOGI(TAG, "Servo test position=%d pulse=%" PRIu32 "us",
             (int)s_servo_test.position,
             s_servo_test.driver.last_pulse_width_us);

    return ESP_OK;
}

esp_err_t cb_hardware_test_start_pwm_receiver_test(void)
{
    esp_err_t result;

#ifndef CONFIG_CYBERBOAT_RECEIVER_PWM
    return ESP_ERR_NOT_SUPPORTED;
#endif

    if (s_receiver_test.started)
    {
        return ESP_ERR_INVALID_STATE;
    }

    result = cb_pwm_receiver_get_default_config(&s_receiver_test.config);
    if (result == ESP_OK)
    {
        result = cb_pwm_receiver_bind(
            &s_receiver_test.driver, &s_receiver_test.config, &s_receiver_test.hal);
    }
    if (result == ESP_OK)
    {
        result = s_receiver_test.hal.init(s_receiver_test.hal.context);
    }
    if (result != ESP_OK)
    {
        return result;
    }

    s_receiver_test.last_report_tick = 0U;
    s_receiver_test.started = true;
    ESP_LOGI(TAG, "PWM receiver monitor started: %u channels, timeout=%u ms",
             (unsigned int)s_receiver_test.config.channel_count,
             (unsigned int)s_receiver_test.config.timeout_ms);

    return ESP_OK;
}

esp_err_t cb_hardware_test_poll_pwm_receiver_test(void)
{
    cb_pwm_receiver_diagnostics_t diagnostics = {};
    const TickType_t current_tick = xTaskGetTickCount();
    esp_err_t result;
    uint8_t channel_index;

    if (!s_receiver_test.started)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if ((TickType_t)(current_tick - s_receiver_test.last_report_tick) <
        pdMS_TO_TICKS(CB_RECEIVER_TEST_LOG_INTERVAL_MS))
    {
        return ESP_OK;
    }

    result = cb_pwm_receiver_get_diagnostics(&s_receiver_test.driver, &diagnostics);
    if (result != ESP_OK)
    {
        return result;
    }

    s_receiver_test.last_report_tick = current_tick;
    for (channel_index = 0U; channel_index < diagnostics.config.channel_count; ++channel_index)
    {
        if ((diagnostics.valid_channels_mask & (uint16_t)(1U << channel_index)) != 0U)
        {
            ESP_LOGI(TAG, "Receiver CH%u gpio=%" PRId32 " pulse=%u us age=%" PRIu32 " ms",
                     (unsigned int)(channel_index + 1U),
                     diagnostics.config.channel_gpio[channel_index],
                     (unsigned int)diagnostics.pulse_width_us[channel_index],
                     diagnostics.channel_age_ms[channel_index]);
        }
        else
        {
            ESP_LOGW(TAG, "Receiver CH%u gpio=%" PRId32 " has no fresh valid pulse",
                     (unsigned int)(channel_index + 1U),
                     diagnostics.config.channel_gpio[channel_index]);
        }
    }

    return ESP_OK;
}

#if CONFIG_CYBERBOAT_HARDWARE_TEST_HBRIDGE || CONFIG_CYBERBOAT_RUN_UNIT_TESTS
esp_err_t cb_hardware_test_start_hbridge_test(void)
{
    esp_err_t result;

    if (s_hbridge_test.started)
    {
        return ESP_ERR_INVALID_STATE;
    }

#ifndef CONFIG_CYBERBOAT_ACTUATORS_ENABLED
    return ESP_ERR_NOT_SUPPORTED;
#endif

    result = cb_hbridge_motor_get_default_config(&s_hbridge_test.config);
    if (result != ESP_OK)
    {
        return result;
    }
    if (s_hbridge_test.config.motor_count != 2U)
    {
        return ESP_ERR_NOT_SUPPORTED;
    }

    result = cb_hbridge_motor_bind(
        &s_hbridge_test.driver, &s_hbridge_test.config, &s_hbridge_test.hal);
    if (result == ESP_OK)
    {
        result = s_hbridge_test.hal.init(s_hbridge_test.hal.context);
    }
    if (result == ESP_OK)
    {
        result = s_hbridge_test.hal.set_armed(s_hbridge_test.hal.context, true);
    }
    if (result != ESP_OK)
    {
        if (s_hbridge_test.hal.emergency_stop != NULL)
        {
            (void)s_hbridge_test.hal.emergency_stop(s_hbridge_test.hal.context);
        }
        return result;
    }

    s_hbridge_test.step_index = 0U;
    s_hbridge_test.last_step_tick = xTaskGetTickCount();
    s_hbridge_test.started = true;
    s_hbridge_test.completed = false;

    result = cb_hardware_test_set_both_motor_outputs(
        s_hbridge_test_steps[0].output_both_motors);
    if (result != ESP_OK)
    {
        (void)s_hbridge_test.hal.emergency_stop(s_hbridge_test.hal.context);
        s_hbridge_test.started = false;
        return result;
    }

    ESP_LOGW(TAG,
             "H-bridge ramp test: D0/D1=GPIO%d/GPIO%d, D2/D3=GPIO%d/GPIO%d; "
             "both motors ramp together from 0 to 100%% in 10%% steps",
             (int)s_hbridge_test.config.input_gpio[0][0],
             (int)s_hbridge_test.config.input_gpio[0][1],
             (int)s_hbridge_test.config.input_gpio[1][0],
             (int)s_hbridge_test.config.input_gpio[1][1]);

    return ESP_OK;
}

esp_err_t cb_hardware_test_poll_hbridge_test(void)
{
    const TickType_t current_tick = xTaskGetTickCount();
    const cb_hbridge_test_step_t *current_step;
    const cb_hbridge_test_step_t *next_step;
    esp_err_t result;

    if (!s_hbridge_test.started)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_hbridge_test.completed)
    {
        return ESP_OK;
    }

    current_step = &s_hbridge_test_steps[s_hbridge_test.step_index];
    if ((TickType_t)(current_tick - s_hbridge_test.last_step_tick) <
        pdMS_TO_TICKS(current_step->duration_ms))
    {
        return ESP_OK;
    }

    if ((size_t)(s_hbridge_test.step_index + 1U) >=
        (sizeof(s_hbridge_test_steps) / sizeof(s_hbridge_test_steps[0])))
    {
        result = s_hbridge_test.hal.set_armed(s_hbridge_test.hal.context, false);
        s_hbridge_test.completed = true;
        if (result == ESP_OK)
        {
            ESP_LOGI(TAG, "H-bridge D0-D3 test complete; outputs stopped and disarmed");
        }
        return result;
    }

    s_hbridge_test.step_index++;
    next_step = &s_hbridge_test_steps[s_hbridge_test.step_index];
    result = cb_hardware_test_set_both_motor_outputs(next_step->output_both_motors);
    if (result != ESP_OK)
    {
        (void)s_hbridge_test.hal.emergency_stop(s_hbridge_test.hal.context);
        s_hbridge_test.completed = true;
        ESP_LOGE(TAG, "H-bridge test stopped on error: %s", esp_err_to_name(result));
        return result;
    }

    s_hbridge_test.last_step_tick = current_tick;
    ESP_LOGI(TAG, "H-bridge both motors output=%d%%",
             (int)((next_step->output_both_motors * 100) / CB_AXIS_MAX));
    return ESP_OK;
}
#endif

#if CONFIG_CYBERBOAT_RUN_UNIT_TESTS
#include "unity.h"

static void test_center_mode_returns_neutral_position(void)
{
    cb_axis_t position = 1;
    TEST_ASSERT_EQUAL(ESP_OK,
                      cb_hardware_test_get_servo_step(CB_SERVO_TEST_MODE_CENTER, 0U, &position));
    TEST_ASSERT_EQUAL_INT16(0, position);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      cb_hardware_test_get_servo_step(CB_SERVO_TEST_MODE_CENTER, 1U, &position));
}

static void test_small_step_mode_stays_within_ten_percent(void)
{
    static const cb_axis_t expected[] = {0, 50, 0, -50};
    uint8_t step_index;

    for (step_index = 0U; step_index < 4U; ++step_index)
    {
        cb_axis_t position = 0;
        TEST_ASSERT_EQUAL(ESP_OK, cb_hardware_test_get_servo_step(
                                      CB_SERVO_TEST_MODE_SMALL_STEPS, step_index, &position));
        TEST_ASSERT_EQUAL_INT16(expected[step_index], position);
    }
}

static void test_servo_step_rejects_invalid_arguments(void)
{
    cb_axis_t position = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      cb_hardware_test_get_servo_step(CB_SERVO_TEST_MODE_CENTER, 0U, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      cb_hardware_test_get_servo_step(CB_SERVO_TEST_MODE_SMALL_STEPS, 4U, &position));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      cb_hardware_test_get_servo_step((cb_servo_test_mode_t)99, 0U, &position));
}

#if CONFIG_CYBERBOAT_RUN_UNIT_TESTS
static void test_hbridge_sequence_ramps_both_motors_together_and_stops(void)
{
    static const cb_axis_t expected_ramp[] = {
        0, 100, 200, 300, 400, 500, 600, 700, 800, 900, CB_AXIS_MAX, 0};
    uint8_t step_index;

    TEST_ASSERT_EQUAL_UINT8(12U,
                            (uint8_t)(sizeof(s_hbridge_test_steps) /
                                      sizeof(s_hbridge_test_steps[0])));
    for (step_index = 0U; step_index < 12U; ++step_index)
    {
        const cb_hbridge_test_step_t *step = &s_hbridge_test_steps[step_index];
        TEST_ASSERT_EQUAL_INT16(expected_ramp[step_index], step->output_both_motors);
        TEST_ASSERT_TRUE(step->output_both_motors >= 0);
        TEST_ASSERT_TRUE(step->output_both_motors <= CB_AXIS_MAX);
        TEST_ASSERT_EQUAL_UINT16(((step_index == 0U) || (step_index == 11U)) ?
                                     CB_HBRIDGE_TEST_ZERO_DWELL_MS :
                                     CB_HBRIDGE_TEST_RAMP_STEP_MS,
                                 step->duration_ms);
    }
}
#endif

void cb_hardware_test_run_unit_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_center_mode_returns_neutral_position);
    RUN_TEST(test_small_step_mode_stays_within_ten_percent);
    RUN_TEST(test_servo_step_rejects_invalid_arguments);
#if CONFIG_CYBERBOAT_RUN_UNIT_TESTS
    RUN_TEST(test_hbridge_sequence_ramps_both_motors_together_and_stops);
#endif
    (void)UNITY_END();
}
#endif