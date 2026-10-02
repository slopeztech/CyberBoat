#include "cyberboat/control.h"
#include "cyberboat/drivers/ws2812.h"
#include "cyberboat/hal/status_indicator.h"
#if CONFIG_CYBERBOAT_RECEIVER_PWM
#include "cyberboat/drivers/pwm_receiver.h"
#include "cyberboat/drivers/hbridge_motor.h"
#include "cyberboat/hal/motor.h"
#include "cyberboat/receiver_debug.h"
#include "cyberboat/receiver_drive.h"
#include "cyberboat/receiver_mapping.h"
#endif
#if CONFIG_CYBERBOAT_RUN_UNIT_TESTS
#include "cyberboat/hardware_test.h"
#include "cyberboat/drivers/hbridge_motor.h"
#include "cyberboat/drivers/pwm_receiver.h"
#include "cyberboat/drivers/servo_pwm.h"
#include "cyberboat/receiver_debug.h"
#include "cyberboat/receiver_drive.h"
#include "cyberboat/receiver_mapping.h"
#endif
#if CONFIG_CYBERBOAT_RUN_HARDWARE_TESTS
#include "cyberboat/hardware_test.h"
#endif
#if CONFIG_CYBERBOAT_SERVO_ENABLED
#include "cyberboat/drivers/servo_pwm.h"
#include "cyberboat/hal/servo.h"
#endif
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>

static const char *TAG = "cyberboat";

#if CONFIG_CYBERBOAT_SERVO_ENABLED && CONFIG_CYBERBOAT_ACTUATORS_ENABLED && \
    !CONFIG_CYBERBOAT_RUN_UNIT_TESTS && !CONFIG_CYBERBOAT_RUN_HARDWARE_TESTS
static esp_err_t cb_start_servo(
    cb_servo_role_t role,
    const char *role_name,
    cb_servo_pwm_driver_t *driver,
    cb_servo_pwm_config_t *config,
    cb_servo_hal_t *servo_hal)
{
    esp_err_t result = cb_servo_pwm_get_default_config(role, config);

    if (result == ESP_OK)
    {
        result = cb_servo_pwm_bind(driver, config, servo_hal);
    }
    if (result == ESP_OK)
    {
        result = servo_hal->init(servo_hal->context);
    }
    if (result == ESP_OK)
    {
        result = servo_hal->set_position(servo_hal->context, 0);
    }
    if (result == ESP_OK)
    {
        ESP_LOGI(TAG, "%s servo ready on GPIO %" PRId32, role_name, config->gpio_num);
    }

    return result;
}
#endif

/** @brief Compose board drivers according to the build-selected vessel mode. */
extern "C" void app_main(void)
{
#if CONFIG_CYBERBOAT_RUN_UNIT_TESTS
    while (true)
    {
        ESP_LOGI(TAG, "Running vessel-mode tests");
        cb_control_run_unit_tests();
        ESP_LOGI(TAG, "Running H-bridge mapping tests");
        cb_hbridge_motor_run_unit_tests();
        ESP_LOGI(TAG, "Running servo PWM tests");
        cb_servo_pwm_run_unit_tests();
        ESP_LOGI(TAG, "Running hardware-test sequence tests");
        cb_hardware_test_run_unit_tests();
        ESP_LOGI(TAG, "Running PWM receiver and mapping tests");
        cb_pwm_receiver_run_unit_tests();
        cb_receiver_mapping_run_unit_tests();
        ESP_LOGI(TAG, "Running receiver drive mapping tests");
        cb_receiver_drive_run_unit_tests();
        ESP_LOGI(TAG, "Running receiver debug logging tests");
        cb_receiver_debug_run_unit_tests();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
#elif CONFIG_CYBERBOAT_RUN_HARDWARE_TESTS
#if CONFIG_CYBERBOAT_HARDWARE_TEST_SERVO
    {
    cb_servo_role_t role;
    cb_servo_test_mode_t mode;

#if CONFIG_CYBERBOAT_HARDWARE_TEST_SERVO_AUXILIARY
    role = CB_SERVO_ROLE_AUXILIARY;
#else
    role = CB_SERVO_ROLE_RUDDER;
#endif
#if CONFIG_CYBERBOAT_HARDWARE_TEST_SERVO_SMALL_STEPS
    mode = CB_SERVO_TEST_MODE_SMALL_STEPS;
#else
    mode = CB_SERVO_TEST_MODE_CENTER;
#endif
    ESP_ERROR_CHECK(cb_hardware_test_start_servo_test(role, mode));
    }
#elif CONFIG_CYBERBOAT_HARDWARE_TEST_PWM_RECEIVER
    ESP_ERROR_CHECK(cb_hardware_test_start_pwm_receiver_test());
#elif CONFIG_CYBERBOAT_HARDWARE_TEST_HBRIDGE
    ESP_ERROR_CHECK(cb_hardware_test_start_hbridge_test());
#endif

    ESP_LOGI(TAG, "Hardware diagnostic active; propulsion driver is not initialized");
    while (true)
    {
#if CONFIG_CYBERBOAT_HARDWARE_TEST_SERVO
    ESP_ERROR_CHECK(cb_hardware_test_poll_servo_test());
#elif CONFIG_CYBERBOAT_HARDWARE_TEST_PWM_RECEIVER
    ESP_ERROR_CHECK(cb_hardware_test_poll_pwm_receiver_test());
#elif CONFIG_CYBERBOAT_HARDWARE_TEST_HBRIDGE
        {
            const esp_err_t test_result = cb_hardware_test_poll_hbridge_test();
            if (test_result != ESP_OK)
            {
                ESP_LOGE(TAG, "H-bridge diagnostic error: %s", esp_err_to_name(test_result));
            }
        }
#endif
    vTaskDelay(pdMS_TO_TICKS(20U));
    }
#else
    cb_status_indicator_hal_t status_indicator = {};
    cb_vessel_mode_config_t vessel_mode = {};
    ESP_ERROR_CHECK(cb_control_get_vessel_mode(&vessel_mode));
    ESP_ERROR_CHECK(cb_ws2812_status_indicator_init(&status_indicator));
    ESP_LOGI(TAG, "BOOT: mode=%s, motors=%u, rudder=%s, auxiliary_servo=%s",
             vessel_mode.name,
             (unsigned int)vessel_mode.motor_count,
             vessel_mode.rudder_servo_enabled ? "enabled" : "disabled",
             vessel_mode.auxiliary_servo_enabled ? "enabled" : "disabled");

#if CONFIG_CYBERBOAT_RECEIVER_PWM
    cb_pwm_receiver_driver_t receiver_driver = {};
    cb_pwm_receiver_config_t receiver_config = {};
    cb_receiver_hal_t receiver_hal = {};
    cb_receiver_mapping_config_t mapping_config = {};
    cb_receiver_drive_config_t drive_config = {};
    cb_receiver_debug_config_t receiver_debug_config = {};
    cb_receiver_debug_state_t receiver_debug_state = {};
    cb_hbridge_motor_driver_t motor_driver = {};
    cb_hbridge_motor_config_t motor_config = {};
    cb_motor_hal_t motor_hal = {};
    cb_motor_outputs_t last_motor_outputs = {};
    bool receiver_ready = false;
    bool motors_ready = false;
    bool motors_armed = false;
    const bool differential_auxiliary_mode =
        (vessel_mode.id == CB_VESSEL_MODE_TWIN_MOTOR_AUX_SERVO);

    (void)cb_receiver_debug_get_default_config(&receiver_debug_config);

    esp_err_t receiver_result = cb_pwm_receiver_get_default_config(&receiver_config);
    if (receiver_result == ESP_OK)
    {
        if (differential_auxiliary_mode)
        {
            receiver_result = cb_receiver_drive_get_default_config(&drive_config);
            if (receiver_result == ESP_OK)
            {
                receiver_result = cb_receiver_drive_validate_config(&drive_config);
            }
        }
        else
        {
            receiver_result = cb_receiver_mapping_get_default_config(&mapping_config);
            if (receiver_result == ESP_OK)
            {
                receiver_result = cb_receiver_mapping_validate_config(&mapping_config);
            }
        }
    }
    if (receiver_result == ESP_OK)
    {
        receiver_result = cb_pwm_receiver_bind(&receiver_driver, &receiver_config, &receiver_hal);
    }
    if (receiver_result == ESP_OK)
    {
        receiver_result = receiver_hal.init(receiver_hal.context);
    }

    if (receiver_result == ESP_OK)
    {
        receiver_ready = true;
        ESP_LOGI(TAG, "PWM receiver ready: %u channels, GPIO %" PRId32 "..%" PRId32,
                 receiver_config.channel_count,
                 receiver_config.channel_gpio[0],
                 receiver_config.channel_gpio[receiver_config.channel_count - 1U]);
    }
    else
    {
        ESP_LOGE(TAG, "PWM receiver initialization failed: %s", esp_err_to_name(receiver_result));
    }

    if (differential_auxiliary_mode)
    {
        esp_err_t motor_result = cb_hbridge_motor_get_default_config(&motor_config);
        if (motor_result == ESP_OK)
        {
            motor_result = cb_hbridge_motor_bind(&motor_driver, &motor_config, &motor_hal);
        }
#if CONFIG_CYBERBOAT_ACTUATORS_ENABLED
        if (motor_result == ESP_OK)
        {
            motor_result = motor_hal.init(motor_hal.context);
        }
        if (motor_result == ESP_OK)
        {
            motors_ready = true;
            ESP_LOGI(TAG, "Differential motors ready; awaiting safe neutral rearm sequence");
        }
        else
        {
            ESP_LOGE(TAG, "H-bridge initialization failed: %s", esp_err_to_name(motor_result));
        }
#else
        ESP_LOGW(TAG, "Differential motor outputs are configured but disabled by safety gate");
#endif
    }
#endif

#if CONFIG_CYBERBOAT_SERVO_ENABLED
#if CONFIG_CYBERBOAT_ACTUATORS_ENABLED
    cb_servo_pwm_driver_t rudder_driver = {};
    cb_servo_pwm_driver_t auxiliary_driver = {};
    cb_servo_pwm_config_t rudder_config = {};
    cb_servo_pwm_config_t auxiliary_config = {};
#endif
    cb_servo_hal_t rudder_servo = {};
    cb_servo_hal_t auxiliary_servo = {};
    bool rudder_ready = false;
    bool auxiliary_ready = false;
    cb_axis_t last_rudder_position = 0;
    cb_axis_t last_auxiliary_position = 0;

#if CONFIG_CYBERBOAT_ACTUATORS_ENABLED
    if (vessel_mode.rudder_servo_enabled)
    {
        ESP_ERROR_CHECK(cb_start_servo(
            CB_SERVO_ROLE_RUDDER, "Rudder", &rudder_driver, &rudder_config, &rudder_servo));
        rudder_ready = true;
    }
    if (vessel_mode.auxiliary_servo_enabled)
    {
        ESP_ERROR_CHECK(cb_start_servo(
            CB_SERVO_ROLE_AUXILIARY, "Auxiliary", &auxiliary_driver,
            &auxiliary_config, &auxiliary_servo));
        auxiliary_ready = true;
    }
#else
    ESP_LOGW(TAG, "Servo roles are selected, but actuator outputs are disabled by configuration");
#endif
#endif

    TickType_t last_led_toggle = xTaskGetTickCount();
    bool status_led_on = false;
    bool receiver_link_valid = false;
    while (true)
    {
#if CONFIG_CYBERBOAT_RECEIVER_PWM
        if (receiver_ready)
        {
            cb_receiver_raw_frame_t raw_frame = {};
            cb_receiver_frame_t legacy_command = {};
            cb_receiver_drive_command_t drive_command = {};
            esp_err_t receive_result = receiver_hal.read_frame(receiver_hal.context, &raw_frame);

            if (receive_result == ESP_OK)
            {
                if (differential_auxiliary_mode)
                {
                    receive_result = cb_receiver_drive_map(&raw_frame, &drive_config, &drive_command);
                }
                else
                {
                    receive_result = cb_control_map_receiver(
                        &raw_frame, &mapping_config, &legacy_command);
                    if (receive_result == ESP_OK)
                    {
                        cb_motor_outputs_t legacy_outputs = {};
                        receive_result = cb_control_mix_motors(
                            &legacy_command.command, &legacy_outputs);
                        drive_command.motors = legacy_outputs;
                        drive_command.auxiliary_servo = legacy_command.auxiliary_servo;
                        drive_command.valid_for_ms = legacy_command.valid_for_ms;
                    }
                }
            }

            if (receive_result == ESP_OK)
            {
                if (!receiver_link_valid)
                {
                    ESP_LOGI(TAG, "PWM receiver signal valid");
                }
                receiver_link_valid = true;

#if CONFIG_CYBERBOAT_SERVO_ENABLED
                if (rudder_ready && (legacy_command.command.steering != last_rudder_position))
                {
                    const esp_err_t servo_result =
                        rudder_servo.set_position(
                            rudder_servo.context, legacy_command.command.steering);
                    if (servo_result == ESP_OK)
                    {
                        last_rudder_position = legacy_command.command.steering;
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Rudder command failed: %s", esp_err_to_name(servo_result));
                    }
                }
                if (auxiliary_ready &&
                    (drive_command.auxiliary_servo != last_auxiliary_position))
                {
                    const esp_err_t servo_result = auxiliary_servo.set_position(
                        auxiliary_servo.context, drive_command.auxiliary_servo);
                    if (servo_result == ESP_OK)
                    {
                        last_auxiliary_position = drive_command.auxiliary_servo;
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Auxiliary servo command failed: %s", esp_err_to_name(servo_result));
                    }
                }
#endif

                if (motors_ready && !motors_armed && drive_command.rearm_safe)
                {
                    const esp_err_t arm_result = motor_hal.set_armed(motor_hal.context, true);
                    if (arm_result == ESP_OK)
                    {
                        motors_armed = true;
                        ESP_LOGI(TAG, "Drive armed at zero throttle and neutral direction");
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Drive arming failed: %s", esp_err_to_name(arm_result));
                    }
                }

                if (motors_ready && motors_armed)
                {
                    uint8_t motor_index;
                    bool output_failed = false;

                    for (motor_index = 0U; motor_index < motor_config.motor_count; ++motor_index)
                    {
                        if (drive_command.motors.motor[motor_index] !=
                            last_motor_outputs.motor[motor_index])
                        {
                            const esp_err_t motor_result = motor_hal.set_output(
                                motor_hal.context, motor_index,
                                drive_command.motors.motor[motor_index]);
                            if (motor_result != ESP_OK)
                            {
                                ESP_LOGE(TAG, "Motor %u command failed: %s",
                                         (unsigned int)(motor_index + 1U),
                                         esp_err_to_name(motor_result));
                                output_failed = true;
                                break;
                            }
                            last_motor_outputs.motor[motor_index] =
                                drive_command.motors.motor[motor_index];
                        }
                    }

                    if (output_failed)
                    {
                        const esp_err_t stop_result = motor_hal.emergency_stop(motor_hal.context);
                        if (stop_result != ESP_OK)
                        {
                            ESP_LOGE(TAG, "Motor emergency stop failed: %s",
                                     esp_err_to_name(stop_result));
                        }
                        motors_armed = false;
                        last_motor_outputs.motor[0] = 0;
                        last_motor_outputs.motor[1] = 0;
                    }
                }
            }
            else
            {
                if (receiver_link_valid)
                {
                    ESP_LOGW(TAG, "PWM receiver signal lost or invalid: %s", esp_err_to_name(receive_result));
                }
                receiver_link_valid = false;

                if (motors_ready && motors_armed)
                {
                    const esp_err_t stop_result = motor_hal.emergency_stop(motor_hal.context);
                    if (stop_result != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Motor failsafe stop failed: %s", esp_err_to_name(stop_result));
                    }
                    motors_armed = false;
                    last_motor_outputs.motor[0] = 0;
                    last_motor_outputs.motor[1] = 0;
                }

#if CONFIG_CYBERBOAT_SERVO_ENABLED
                if (rudder_ready && (last_rudder_position != 0))
                {
                    const esp_err_t servo_result = rudder_servo.set_position(rudder_servo.context, 0);
                    if (servo_result == ESP_OK)
                    {
                        last_rudder_position = 0;
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Rudder failsafe command failed: %s", esp_err_to_name(servo_result));
                    }
                }
                if (auxiliary_ready && (last_auxiliary_position != 0))
                {
                    const esp_err_t servo_result = auxiliary_servo.set_position(
                        auxiliary_servo.context, 0);
                    if (servo_result == ESP_OK)
                    {
                        last_auxiliary_position = 0;
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Auxiliary servo failsafe command failed: %s", esp_err_to_name(servo_result));
                    }
                }
#endif
            }

            {
                const uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
                bool should_log_receiver = false;

                if ((cb_receiver_debug_should_log(
                         &receiver_debug_config, &receiver_debug_state, now_ms, &should_log_receiver) ==
                     ESP_OK) &&
                    should_log_receiver)
                {
                    cb_pwm_receiver_diagnostics_t receiver_diagnostics = {};
                    cb_receiver_debug_command_t logged_command = {};
                    const uint8_t right_motor_index =
                        (uint8_t)(1U - drive_config.left_motor_index);

                    logged_command.motor_left =
                        drive_command.motors.motor[drive_config.left_motor_index];
                    logged_command.motor_right = drive_command.motors.motor[right_motor_index];
                    logged_command.auxiliary_servo = drive_command.auxiliary_servo;
                    logged_command.rearm_safe = drive_command.rearm_safe;
                    logged_command.valid_for_ms = drive_command.valid_for_ms;

                    if (cb_pwm_receiver_get_diagnostics(&receiver_driver, &receiver_diagnostics) ==
                        ESP_OK)
                    {
                        (void)cb_receiver_debug_log_raw_frame(&receiver_diagnostics);
                    }
                    (void)cb_receiver_debug_log_mapped_command(&logged_command);
                }
            }
        }
#endif

        const TickType_t current_tick = xTaskGetTickCount();
        if ((TickType_t)(current_tick - last_led_toggle) >= pdMS_TO_TICKS(500U))
        {
            last_led_toggle = current_tick;
            status_led_on = !status_led_on;
            if (status_led_on)
            {
                ESP_ERROR_CHECK(status_indicator.set_rgb(status_indicator.context, 0U, 0U, 255U));
                ESP_LOGI(TAG, "ON");
            }
            else
            {
                ESP_ERROR_CHECK(status_indicator.clear(status_indicator.context));
                ESP_LOGI(TAG, "OFF");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10U));
    }
#endif
}