set(servo_pwm_sources "src/servo_pwm.c")
set(servo_pwm_requires cyberboat_hal cyberboat_config esp_driver_gpio esp_driver_ledc esp_common)
if(CONFIG_CYBERBOAT_RUN_UNIT_TESTS)
    list(APPEND servo_pwm_sources "test/test_servo_pwm.c")
    list(APPEND servo_pwm_requires unity)
endif()

idf_component_register(
    SRCS ${servo_pwm_sources}
    INCLUDE_DIRS "include"
    REQUIRES ${servo_pwm_requires}
)

if(CONFIG_CYBERBOAT_RUN_UNIT_TESTS)
    idf_component_get_property(unity_component_dir unity COMPONENT_DIR)
    target_include_directories(${COMPONENT_LIB} PRIVATE
        "${unity_component_dir}/include"
        "${unity_component_dir}/unity/src"
    )
endif()

cyberboat_apply_c_quality(${COMPONENT_LIB})