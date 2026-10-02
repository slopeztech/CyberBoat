set(receiver_debug_sources "src/receiver_debug.c")
set(receiver_debug_requires cyberboat_types cyberboat_driver_pwm_receiver cyberboat_config esp_common)
if(CONFIG_CYBERBOAT_RUN_UNIT_TESTS)
    list(APPEND receiver_debug_sources "test/test_receiver_debug.c")
    list(APPEND receiver_debug_requires unity)
endif()

idf_component_register(
    SRCS ${receiver_debug_sources}
    INCLUDE_DIRS "include"
    REQUIRES ${receiver_debug_requires}
)

if(CONFIG_CYBERBOAT_RUN_UNIT_TESTS)
    idf_component_get_property(unity_component_dir unity COMPONENT_DIR)
    target_include_directories(${COMPONENT_LIB} PRIVATE
        "${unity_component_dir}/include"
        "${unity_component_dir}/unity/src"
    )
endif()

cyberboat_apply_c_quality(${COMPONENT_LIB})
