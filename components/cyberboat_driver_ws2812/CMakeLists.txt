idf_component_register(
    SRCS "src/ws2812.c"
    INCLUDE_DIRS "include"
    REQUIRES cyberboat_hal cyberboat_config led_strip esp_common
)

cyberboat_apply_c_quality(${COMPONENT_LIB})