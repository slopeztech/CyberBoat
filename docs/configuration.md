# Configuration

Options are declared in `components/cyberboat_config/Kconfig`; base values are in `sdkconfig.defaults`, and effective settings are in `sdkconfig.esp32-s3-zero`. Select the vessel topology with the exclusive `CYBERBOAT_VESSEL_MODE` choice before building; the firmware resolves and logs that fixed mode at startup.

| Vessel mode | Motors | Rudder servo | Auxiliary servo |
| --- | ---: | --- | --- |
| `CYBERBOAT_MODE_SINGLE_MOTOR_ONLY` | 1 | No | No |
| `CYBERBOAT_MODE_SINGLE_MOTOR_RUDDER` | 1 | Yes | No |
| `CYBERBOAT_MODE_SINGLE_MOTOR_AUX_SERVO` | 1 | No | Yes |
| `CYBERBOAT_MODE_SINGLE_MOTOR_RUDDER_AUX` | 1 | Yes | Yes |
| `CYBERBOAT_MODE_TWIN_MOTOR_ONLY` | 2 | No | No |
| `CYBERBOAT_MODE_TWIN_MOTOR_RUDDER` | 2 | Yes | No |
| `CYBERBOAT_MODE_TWIN_MOTOR_AUX_SERVO` | 2 | No | Yes |
| `CYBERBOAT_MODE_TWIN_MOTOR_RUDDER_AUX` | 2 | Yes | Yes |

Motor count and servo-role capabilities are derived from this choice; do not configure independent topology flags. In twin-motor modes without a rudder, steering is intended for differential mixing. With a rudder role, steering is routed to that servo and throttle remains the propulsion command. The auxiliary servo uses its own receiver channel.

| Option | Default | Current effect |
| --- | --- | --- |
| `CONFIG_CYBERBOAT_MOTOR_COUNT` | derived | Motor count determined by the selected vessel mode. |
| `CONFIG_CYBERBOAT_ACTUATORS_ENABLED` | disabled in `sdkconfig.defaults` | Production `sdkconfig.esp32-s3-zero` is enabled for the user's no-propeller bench test. Other profiles retain their own gate settings. Disable it to prevent motor/servo GPIO initialization. |
| `CONFIG_CYBERBOAT_RUN_UNIT_TESTS` | disabled | Enables the dedicated Unity runner; does not initialize actuator hardware. |
| `CONFIG_CYBERBOAT_RUN_HARDWARE_TESTS` | disabled | Replaces normal application composition with the selected receiver, servo, or bounded H-bridge diagnostic. |
| `CONFIG_CYBERBOAT_HARDWARE_TEST_HBRIDGE` | disabled | Selects the bounded D0-D3 test; requires a two-motor vessel mode. |
| `CONFIG_CYBERBOAT_MOTOR_1_IN1_GPIO` / `IN2_GPIO` | `3` / `4` | Left motor logic-input pins by convention; GPIO3 is a strapping pin, so check reset bias before wiring. |
| `CONFIG_CYBERBOAT_MOTOR_1_REVERSED` | disabled | Inverts motor 1 logical direction. |
| `CONFIG_CYBERBOAT_MOTOR_2_IN1_GPIO` / `IN2_GPIO` | `5` / `6` | Right motor logic-input pins by convention when two motors are selected. |
| `CONFIG_CYBERBOAT_MOTOR_2_REVERSED` | disabled | Inverts motor 2 logical direction. |
| `CONFIG_CYBERBOAT_MOTOR_PWM_FREQUENCY_HZ` | `1000` | H-bridge PWM frequency; Kconfig range 100-20000 Hz. |
| `CONFIG_CYBERBOAT_MOTOR_LEDC_TIMER` / `CHANNEL_BASE` | `1` / `4` | Timer and first of two LEDC channels per motor. |
| `CONFIG_CYBERBOAT_RUDDER_SERVO_*` | GPIO `18`, `50 Hz`, `500/1500/2500 us`, timer/channel `0/0` | Independent rudder PWM, calibration, and LEDC resources. |
| `CONFIG_CYBERBOAT_AUX_SERVO_*` | GPIO `13`, `50 Hz`, `500/1500/2500 us`, timer/channel `2/1` | Independent auxiliary-servo PWM, calibration, and LEDC resources. GPIO19/20 are reserved for native USB Serial/JTAG. |
| `CONFIG_CYBERBOAT_GPS_ENABLED` | disabled | Configuration option; no GPS driver exists yet. |
| `CONFIG_CYBERBOAT_RECEIVER_*` | PWM | Selects the transport; the generic individual-channel PWM driver is implemented. |
| `CONFIG_CYBERBOAT_PWM_RECEIVER_CHANNEL_COUNT` | `4` | Number of independent PWM output channels to capture, range 1-8. |
| `CONFIG_CYBERBOAT_PWM_RECEIVER_CH1..CH8_GPIO` | `7, 8, 9, 10, 12, 13, 14, 15` | Per-channel GPIO defaults; CH1-4 default to GPIO 7-10, CH5-8 default to GPIO 12-15 (GPIO 11 is intentionally skipped), and channels 5-8 are configured only when selected. These are examples, not wired pins. |
| `CONFIG_CYBERBOAT_PWM_RECEIVER_MIN/MAX_PULSE_US` | `900` / `2100` | Accepted high-pulse range per receiver channel. |
| `CONFIG_CYBERBOAT_PWM_RECEIVER_TIMEOUT_MS` | `100` | Channel age at or above which the raw sample is stale. |
| `CONFIG_CYBERBOAT_RECEIVER_CONSOLE_LOG` | enabled | Logs raw channel pulse widths/ages and the mapped command to the console; disable to silence receiver debug output. |
| `CONFIG_CYBERBOAT_RECEIVER_CONSOLE_LOG_INTERVAL_MS` | `500` | Minimum time between receiver console log lines, range 50-5000 ms. |
| `CONFIG_CYBERBOAT_RC_STEERING/THROTTLE_CHANNEL` | `0` / `1` | Legacy generic mapper used by modes other than twin-motor + auxiliary servo. |
| `CONFIG_CYBERBOAT_RC_AUX_SERVO_CHANNEL` | `3` | Legacy generic mapper; the selected drive profile maps servo position from CH1 instead. |
| `CONFIG_CYBERBOAT_RC_STEERING_REVERSED` / `THROTTLE_REVERSED` | disabled | Reverse the normalized direction for that axis. |
| `CONFIG_CYBERBOAT_RC_THROTTLE_BIDIRECTIONAL` | disabled | Legacy mapper option only; the selected four-channel drive profile multiplies CH2 power by signed CH3 speed. |
| `CONFIG_CYBERBOAT_RC_*_MIN/CENTER/MAX_US` | `1000` / `1500` / `2000` | Per-axis calibration endpoints and center. |
| `CONFIG_CYBERBOAT_RC_DEADBAND_US` | `30` | Steering/centered-axis deadband around neutral. |
| `CONFIG_CYBERBOAT_RC_ARM_ENABLED` | disabled | Enables an auxiliary arm-request mapping; it never arms hardware by itself. |
| `CONFIG_CYBERBOAT_RC_ARM_CHANNEL` / `ARM_THRESHOLD_US` | `2` / `1500` | Zero-based auxiliary channel and pulse threshold when arm mapping is enabled. |
| `CONFIG_CYBERBOAT_RC_DRIVE_*_CHANNEL` | `0`, `1`, `2`, `3` | In `CYBERBOAT_MODE_TWIN_MOTOR_AUX_SERVO`: CH1 servo, CH2 power limit, CH3 proportional reverse/neutral/forward speed, CH4 differential steering. All assignments are zero-based and must be unique. |
| `CONFIG_CYBERBOAT_RC_DRIVE_LEFT_MOTOR_INDEX` | `0` | Selects which configured H-bridge motor is physically left: `0` means motor 1 on IN1/IN2 (D0/D1), `1` means motor 2 on IN3/IN4 (D2/D3). The other motor is right. |
| `CONFIG_CYBERBOAT_RC_DRIVE_SERVO_MIN/CENTER/MAX_US` | `976` / `1492` / `1933` | CH1 left/center/right pulse calibration. |
| `CONFIG_CYBERBOAT_RC_DRIVE_THROTTLE_MIN/MAX_US` | `1056` / `1698` | CH2 stop/full-power calibration; intermediate values scale linearly. The center option is informational for calibration. |
| `CONFIG_CYBERBOAT_RC_DRIVE_DIRECTION_MIN/CENTER/MAX_US` | `1096` / `1472` / `1924` | CH3 full reverse/zero speed/full forward calibration; intermediate stick positions proportionally scale CH2 power. |
| `CONFIG_CYBERBOAT_RC_DRIVE_STEERING_MIN/CENTER/MAX_US` | `993` / `1508` / `1950` | CH4 left/center/right calibration. Turns reduce only the inside motor; neither output can exceed CH2 power. |
| `CONFIG_CYBERBOAT_RC_DRIVE_DEADBAND_US` | `30` | Pulse deadband around the centered channels and CH2 stop. |
| `CONFIG_CYBERBOAT_RC_DRIVE_SERVO_TRAVEL_PERCENT` | `50` | Uses half the auxiliary servo's calibrated range, nominally 90 degrees total when its configured span represents 180 degrees. |
| `CONFIG_CYBERBOAT_RC_DRIVE_*_REVERSED` and `CONFIG_CYBERBOAT_MOTOR_[12]_REVERSED` | disabled | Independently invert servo, forward/reverse, steering, or each motor's electrical direction to match installation polarity. |
| `CONFIG_CYBERBOAT_STATUS_LED_GPIO` | `21` | ESP32-S3-Zero onboard WS2812. |

Edit settings with PlatformIO's `menuconfig` target and build:

```powershell
platformio run -e esp32-s3-zero -t menuconfig
platformio run -e esp32-s3-zero
```

In the production `CYBERBOAT_MODE_TWIN_MOTOR_AUX_SERVO` profile, the four-channel drive mapper feeds both H-bridge motors and the auxiliary servo. Motor power is CH2 multiplied by the absolute CH3 fraction (each normalized to 0-1000), with the CH3 sign selecting forward/reverse. For example, CH2=50% and CH3=50% produce 25% motor power; CH3 at full travel uses all of CH2's available power. The system starts disarmed, rearms only after all channels are fresh and CH2 is at stop with CH3 neutral, and emergency-stops on a stale/missing channel. Steering scales the inside motor down; output remains bounded by the combined CH2/CH3 power. The production actuator gate is enabled for the current propeller-free bench test; the generic defaults remain disabled. Keep the motors secured and propellers removed while characterizing motor direction and steering. The Unity profile leaves actuators disabled and runs tests without initializing receiver interrupts or actuator GPIO:

## Receiver Console Logging

`CONFIG_CYBERBOAT_RECEIVER_CONSOLE_LOG` prints receiver data to the serial console at startup and periodically thereafter, for bring-up with a FlySky FS-R6B or similar generic individual-channel PWM receiver. Each log cycle prints one line per captured channel (GPIO, pulse width, age, and valid/stale status) followed by the mapped throttle, steering, auxiliary servo, and arm request:

```
I (1234) receiver: Raw channels (valid_pulses=512, invalid_pulses=0):
I (1234) receiver:   CH1 gpio=7 pulse=1500 us age=12 ms valid
I (1234) receiver:   CH2 gpio=8 pulse=1620 us age=10 ms valid
I (1234) receiver:   CH3 gpio=9 pulse=1000 us age=15 ms valid
I (1234) receiver:   CH4 gpio=10 pulse=1500 us age=9 ms valid
I (1234) receiver: Drive command: motor_left=250 motor_right=180 auxiliary_servo=0 rearm_safe=no valid_for=88ms
```

It is enabled by default so receiver data is visible as soon as the firmware boots; disable it in `menuconfig` to reduce log noise once wiring is confirmed. `CONFIG_CYBERBOAT_RECEIVER_CONSOLE_LOG_INTERVAL_MS` controls how often subsequent lines print (the first line always prints immediately). This logging only runs in the normal application profile, not in unit tests or hardware tests.

```powershell
$env:GIT_CEILING_DIRECTORIES = "$env:USERPROFILE\.platformio;$PWD"
platformio run -e esp32-s3-zero-unit-tests
platformio run -e esp32-s3-zero-unit-tests -t upload
platformio device monitor -p COM8 -b 115200
platformio run -e esp32-s3-zero -t upload
```

The test application repeats the Unity cases every five seconds. The last command restores the normal twin-motor + auxiliary-servo profile with the actuator safety gate disabled.

The normal application now binds the H-bridge and maps receiver commands in twin-motor + auxiliary-servo mode. It initializes the motor and servo output peripherals only when `CONFIG_CYBERBOAT_ACTUATORS_ENABLED` is enabled; this option remains disabled in the current production profile. With the gate disabled the application continues to read/log receiver data but does not emit actuator PWM. Do not enable it until the peripherals, power, wiring, polarity, GPIO3 reset bias, and physical stop procedure have been verified.

Each servo role validates its GPIO, reserved-pin conflicts, frequency, pulse ordering, PWM period, and LEDC resource conflicts. Software validation does not replace calibration with the actual servo.

For connector-level mapping, signal wiring, and oscilloscope procedures, see [Hardware](hardware.md) and the [Oscilloscope Test Guide](oscilloscope-testing.md).

## Hardware Diagnostic Profile

Use the isolated `esp32-s3-zero-hardware-tests` environment to run a diagnostic instead of the boat application. Its defaults select the auxiliary servo, GPIO13, small-step mode, and `single motor + auxiliary servo` topology. The selected test initializes only the servo PWM path; it never binds or initializes the H-bridge. The actuator gate is enabled in this profile only so the selected servo test can emit PWM. Keep motors and the H-bridge disconnected.

Build and upload the default slow-step auxiliary-servo test:

```powershell
platformio run -e esp32-s3-zero-hardware-tests
platformio run -e esp32-s3-zero-hardware-tests -t upload
platformio device monitor -p COM8 -b 115200
```

Open this environment's `menuconfig` with `platformio run -e esp32-s3-zero-hardware-tests -t menuconfig`. Select the component (`servo output` or `PWM receiver input monitor`), servo role, and behavior (`hold center pulse` or `small +/-5% steps`). Center mode provides a stable default 1500 us pulse at 50 Hz. The default small-step mode visits center, +50, center, and -50 on the normalized axis, one second apart. With a linear 180-degree servo calibration, each change is approximately 4.5 degrees; actual movement depends on the servo and its calibrated pulse range.

The diagnostics can also be called from an application composition root:

```c
ESP_ERROR_CHECK(cb_hardware_test_start_servo_test(
	CB_SERVO_ROLE_AUXILIARY,
	CB_SERVO_TEST_MODE_CENTER));
```

Call `cb_hardware_test_poll_servo_test()` periodically after startup. The selected Kconfig profile in `src/main.cpp` already performs this dispatch for the dedicated hardware-test environment.

The receiver monitor initializes only the configured receiver GPIOs and reports pulse widths and freshness once per second. It does not map commands or drive servos/motors.

## H-Bridge D0-D3 Test

The isolated `esp32-s3-zero-hbridge-tests` environment selects two motors, enables the actuator gate for this diagnostic only, and runs the D0-D3 test instead of the boat application. Defaults map D0/D1 to GPIO3/4 and D2/D3 to GPIO5/6. GPIO3 is an ESP32-S3 strapping pin: verify that D0's input/pull circuit does not affect reset or boot, or assign D0 to another free GPIO. Build first; keep propellers removed and secure motors. Disconnect motor power/leads for a logic-only scope test.

```powershell
platformio run -e esp32-s3-zero-hbridge-tests
platformio run -e esp32-s3-zero-hbridge-tests -t upload --upload-port COM8
platformio device monitor -p COM8 -b 115200
```

The sequence sets both motors to zero for one second, ramps both together through 10%, 20%, ... 100% for 300 ms per level, returns both to zero for one second, then emergency-stops and disarms. Each motor receives the same positive command, so both turn in their configured forward direction. The bridge enable pins are not driven by the firmware; configure them according to the module datasheet. At 100% duty, motors can accelerate rapidly: secure them and remove propellers before uploading this profile.

To stop the diagnostic and restore the safe normal application, upload the production environment:

```powershell
platformio run -e esp32-s3-zero -t upload
```