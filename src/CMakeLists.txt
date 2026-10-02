idf_component_register(
	SRCS "main.cpp"
	INCLUDE_DIRS "."
	REQUIRES cyberboat_control cyberboat_hal cyberboat_driver_hbridge cyberboat_driver_pwm_receiver cyberboat_driver_servo cyberboat_driver_ws2812 cyberboat_receiver_debug unity
)