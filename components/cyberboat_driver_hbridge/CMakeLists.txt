set(hbridge_motor_sources "src/hbridge_motor.c")
set(hbridge_motor_requires cyberboat_hal cyberboat_config esp_driver_gpio esp_driver_ledc esp_common)
if(CONFIG_CYBERBOAT_RUN_UNIT_TESTS)
    list(APPEND hbridge_motor_sources "test/test_hbridge_motor.c")
    list(APPEND hbridge_motor_requires unity)
endif()

idf_component_register(
    SRCS ${hbridge_motor_sources}
    INCLUDE_DIRS "include"
    REQUIRES ${hbridge_motor_requires}
)

if(CONFIG_CYBERBOAT_RUN_UNIT_TESTS)
    idf_component_get_property(unity_component_dir unity COMPONENT_DIR)
    target_include_directories(${COMPONENT_LIB} PRIVATE
        "${unity_component_dir}/include"
        "${unity_component_dir}/unity/src"
    )
endif()

cyberboat_apply_c_quality(${COMPONENT_LIB})