# CyberBoat Architecture

## Status

CyberBoat is RC boat controller firmware currently based on the Waveshare ESP32-S3-Zero and ESP-IDF. The application captures parallel PWM receiver channels, maps fresh channel values, and toggles the onboard WS2812 every 500 ms. The user's FS-T4B radio set has not yet been validated with physical wiring. One servo is connected for the auxiliary-output test; no ESC, motor bridge, or GPS is connected.

## Components

## Vessel Modes

`CYBERBOAT_VESSEL_MODE` is one exclusive build-time Kconfig choice. Its eight options cover one or two motors, with or without a rudder servo and with or without an independent auxiliary servo. Derived Kconfig capabilities feed `cb_control_get_vessel_mode()`; startup logs the selected descriptor. Runtime code does not switch topology after boot.

The rudder servo consumes mapped steering. The auxiliary servo consumes its separately assigned RC channel and calibration. In two-motor modes without a rudder, steering is available to the differential mixer. Motor outputs remain disabled until the separate actuator safety gate is enabled.

| Component | Responsibility | Status |
| --- | --- | --- |
| `cyberboat_types` | Transport-independent commands, normalized outputs, and GPS data. | Implemented |
| `cyberboat_hal` | Motor, servo, GPS, raw receiver, and indicator contracts. | Implemented |
| `cyberboat_config` | ESP-IDF Kconfig build options. | Implemented |
| `cyberboat_control` | Vessel mode, legacy mapping, and calibrated four-channel differential drive mapping. | Implemented and connected to normal-mode motor/servo HALs; actuator gate disabled by default |
| `cyberboat_control` receiver mapping | Maps raw pulses to throttle, steering, and arm request. | Implemented and covered by Unity tests |
| `cyberboat_driver_hbridge` | LEDC direction/duty outputs for one or two brushed motors. | Implemented; output initialization gated and not tested with a bridge |
| `cyberboat_driver_pwm_receiver` | GPIO interrupt capture of one-to-eight independent PWM channels. | Implemented; physical signal not yet validated |
| `cyberboat_driver_ws2812` | RGB indicator using RMT and `led_strip`. | Implemented and tested on the board |
| `cyberboat_driver_servo` | Role-aware conventional LEDC PWM servos with independent calibration/resources. | Implemented and compiled; no physical servo |
| `cyberboat_hardware_test` | Poll-driven servo, receiver, and bounded two-motor H-bridge diagnostics, separate from boat control. | Implemented; servo PWM logged on GPIO13; D0-D3 profile builds but has not been uploaded/run |
| `cyberboat_receiver_debug` | Toggleable, rate-limited console logging of raw receiver channels and the mapped command. | Implemented and covered by Unity tests |
| `src` | Application composition root and startup. | Mode resolution, production control, unit-test runner, or selected hardware diagnostic |

Each concrete driver implements a HAL and hides ESP-IDF details. Control code does not access GPIO, UART, LEDC, RMT, or other peripherals directly.

## Safe Initial Configuration

Production selects two motors plus an auxiliary servo, PWM receiver capture on GPIO7-10, and status LED GPIO21. Its actuator gate is enabled in the current COM8 bench image; generic defaults and Unity remain actuator-disabled. The H-bridge diagnostic profile ramps both motors together and reaches 100% duty; it was not uploaded. GPS is disabled. The user's propellers are removed; motor/bridge direction and loaded behavior remain to be checked.

In twin-motor modes without a rudder, the mixer combines throttle and steering and proportionally scales both outputs to `-1000..1000`. When a rudder is selected, the rudder consumes steering and both motors receive the same throttle. Single-motor modes pass throttle to motor one and leave motor two at zero.

## RC Receiver and Mapping

`cyberboat_driver_pwm_receiver` captures individual servo-style PWM outputs using GPIO edge interrupts. `cyberboat_control` provides a configurable drive mapper for the selected twin-motor + auxiliary-servo profile: CH1 selects auxiliary-servo position, CH2 sets a power limit, CH3 continuously scales speed and its sign selects reverse/forward, and CH4 reduces the inside motor for differential steering. Combined power is CH2 multiplied by `abs(CH3)`, so two 50% inputs yield 25%. Motor outputs are bounded by this combined power and zero at CH3 center. The default pulse calibrations are based on the user's measured values and are adjustable in Kconfig. All channels must be fresh; a stale or missing channel emergency-stops and disarms both motors. Rearming requires throttle at stop and CH3 neutral.

The PWM driver expects one separate PWM output per channel. It does not decode the 2.4 GHz link and does not support combined PPM, SBUS, or serial data. Confirm the paired receiver actually exposes individual servo-style PWM pins before wiring.

The application initializes PWM capture in the normal profile and maps drive frames in its main loop. The auxiliary servo follows CH1 and the two motors follow the calibrated CH2-CH4 command; on invalid input, motor output is stopped and the servo returns to center. The current production image initializes the actuator outputs; the separate unit-test profile does not. `cyberboat_receiver_debug` logs raw channels and mapped drive commands at a configurable interval.

## Servo Driver

The LEDC driver supports independent rudder and auxiliary hobby servos controlled by three-wire PWM. Each role has its own GPIO, frequency, endpoints, center, timer, and channel in Kconfig. It converts normalized positions `-1000..1000` to calibrated pulse widths in microseconds or accepts a direct pulse width. Validation rejects occupied GPIOs, shared LEDC channels, and incompatible frequencies on a shared timer. Diagnostics report software state and the last command, not mechanical position or measured waveform. Serial and bus servos are not implemented.

## H-Bridge Motor Driver

`cyberboat_driver_hbridge` accepts two logic inputs per brushed DC motor. It applies PWM to the input selected by direction and holds the other input low; each motor has an optional direction inversion. The output mapper is pure and tested independently of GPIO. The driver uses 10-bit LEDC duty and a configurable frequency, timer, channel base, and GPIO pair for each motor. L298 enable pins such as ENA/ENB are not controlled by this driver and must be configured according to the board/module datasheet. Nine Unity tests cover mapping, configuration, GPIO conflicts, and the disabled-actuator initialization gate.

Initialization returns `ESP_ERR_NOT_SUPPORTED` unless `CONFIG_CYBERBOAT_ACTUATORS_ENABLED` is enabled. The normal profile leaves it disabled. This driver has no current sensing, encoder feedback, or physical emergency power disconnect.

The control suite has three tests for mode resolution and mixing; the legacy semantic mapper has ten; the four-channel drive mapper has eleven for calibration validation, forward/reverse, proportional throttle/direction scaling, throttle-limited steering, configurable motor-side assignment, full servo-travel mapping, neutral rearm, and failsafe. Together with H-bridge (9), role-aware servo PWM (9), receiver capture (4), hardware-test sequencing (4), and console logging (7), 57 Unity tests compile in `esp32-s3-zero-unit-tests`. The new drive-mapping tests have not been run on target. Pure tests do not initialize actuator outputs.

The hardware-test component defaults to slow +/-5% servo steps around center, offers a stable center pulse for scope work, includes a receiver pulse/freshness monitor, and provides a separate D0-D3 H-bridge test. In that test both motors receive the same 0-to-100% ramp in 10% increments, then are stopped and disarmed. Its firmware profile has only been built; no bridge or motors were exercised.

## Safety and Standards

Drivers validate parameters and return `esp_err_t`; they must not hide errors. Actuator outputs remain disabled by default.

Project-owned C components compile with `-Wall -Wextra -Wconversion -Wsign-conversion -Wshadow -Werror`. This is a quality baseline, not MISRA C or CERT C certification. See [Standards and Quality](standards.md).