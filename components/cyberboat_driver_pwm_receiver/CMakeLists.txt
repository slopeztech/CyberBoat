set(pwm_receiver_sources "src/pwm_receiver.c")
set(pwm_receiver_requires cyberboat_hal cyberboat_config esp_driver_gpio esp_timer esp_common)
if(CONFIG_CYBERBOAT_RUN_UNIT_TESTS)
    list(APPEND pwm_receiver_sources "test/test_pwm_receiver.c")
    list(APPEND pwm_receiver_requires unity)
endif()

idf_component_register(
    SRCS ${pwm_receiver_sources}
    INCLUDE_DIRS "include"
    REQUIRES ${pwm_receiver_requires}
)

if(CONFIG_CYBERBOAT_RUN_UNIT_TESTS)
if(CONFIG_CYBERBOAT_RUN_UNIT_TESTS)
    idf_component_get_property(unity_component_dir unity COMPONENT_DIR)
    target_include_directories(${COMPONENT_LIB} PRIVATE
        "${unity_component_dir}/include"
        "${unity_component_dir}/unity/src"
    )
endif()

    idf_component_get_property(unity_component_dir unity COMPONENT_DIR)
    target_include_directories(${COMPONENT_LIB} PRIVATE
        "${unity_component_dir}/include"
        "${unity_component_dir}/unity/src"
    )
endif()

cyberboat_apply_c_quality(${COMPONENT_LIB})