set(cyberboat_control_sources "src/control.c" "src/receiver_mapping.c" "src/receiver_drive.c")
set(cyberboat_control_requires cyberboat_types cyberboat_config esp_common)
if(CONFIG_CYBERBOAT_RUN_UNIT_TESTS)
    list(APPEND cyberboat_control_sources "test/test_control.c" "test/test_receiver_mapping.c" "test/test_receiver_drive.c")
    list(APPEND cyberboat_control_requires unity)
endif()

idf_component_register(
    SRCS ${cyberboat_control_sources}
    INCLUDE_DIRS "include"
    REQUIRES ${cyberboat_control_requires}
)

if(CONFIG_CYBERBOAT_RUN_UNIT_TESTS)
    idf_component_get_property(unity_component_dir unity COMPONENT_DIR)
    target_include_directories(${COMPONENT_LIB} PRIVATE
        "${unity_component_dir}/include"
        "${unity_component_dir}/unity/src"
    )
endif()

cyberboat_apply_c_quality(${COMPONENT_LIB})