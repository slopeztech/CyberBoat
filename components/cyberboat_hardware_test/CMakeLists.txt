set(hardware_test_sources "src/hardware_test.c")
set(hardware_test_requires cyberboat_driver_servo cyberboat_driver_pwm_receiver cyberboat_driver_hbridge cyberboat_config freertos esp_common)
if(CONFIG_CYBERBOAT_RUN_UNIT_TESTS)
    list(APPEND hardware_test_requires unity)
endif()

idf_component_register(
    SRCS ${hardware_test_sources}
    INCLUDE_DIRS "include"
    REQUIRES ${hardware_test_requires}
)

if(CONFIG_CYBERBOAT_RUN_UNIT_TESTS)
    idf_component_get_property(unity_component_dir unity COMPONENT_DIR)
    target_include_directories(${COMPONENT_LIB} PRIVATE
        "${unity_component_dir}/include"
        "${unity_component_dir}/unity/src"
    )
endif()

cyberboat_apply_c_quality(${COMPONENT_LIB})