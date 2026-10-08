# Testing

## Available Verification

| Layer | Verification | Status |
| --- | --- | --- |
| Production build | `platformio run -e esp32-s3-zero` with strict project-owned C warnings | Passed cleanly; twin-motor + auxiliary-servo profile selected. |
| Unity build | `platformio run -e esp32-s3-zero-unit-tests` | Last known successful build had 56 tests; a full-travel servo mapping test was added afterward. |
| Unity on target | Mode/control, H-bridge, role-aware servo, PWM receiver, receiver mapping, receiver drive mapping, receiver console logging, and hardware-test sequencing | 38 earlier tests passed on target; current 57-test build has not completed after the servo-range test was added. |
| Hardware-test runtime | `esp32-s3-zero-hardware-tests`, auxiliary role on GPIO13, small-step mode | Previously uploaded on COM8; serial log confirmed repeated 1500/1450/1550 us commands. It is no longer the active firmware. Visual shaft movement and oscilloscope waveform remain unverified. |
| H-bridge test build | `esp32-s3-zero-hbridge-tests`, twin-motor D0-D3 sequence | Build passed after explicit component-dependency fix; not uploaded or run with motors/bridge. |
| PWM receiver capture | Unity tests for channel count, reserved USB/actuator GPIO conflicts, HAL binding, and uninitialized reads | Passed on ESP32-S3-Zero: 4 tests, 0 failures. |
| Receiver command mapping | Unity tests for calibration, throttle modes, optional steering, auxiliary channel, and stale/missing channels | Passed on ESP32-S3-Zero: 10 tests, 0 failures. |
| Production upload | `platformio run -e esp32-s3-zero -t upload --upload-port COM8 -j 1` | Passed; twin-motor + auxiliary-servo drive image with actuator gate enabled is running on COM8 for the propeller-free bench test. |
| LED | 115200-baud monitor shows `ON`/`OFF`; physical LED toggles | Passed on the ESP32-S3-Zero. |
| Doxygen | `doxygen Doxyfile` with Doxygen 1.15.0 | HTML generation passed without warnings. |
| Servo hardware | Pulse, direction, endpoints, load, and power | Partially set up: servo connected and PWM commands logged; physical movement, power adequacy, endpoints, and scope waveform still pending. See [Oscilloscope Test Guide](oscilloscope-testing.md). |
| H-bridge hardware | Direction, coast, enable inputs, motor load, current, and stop behavior | Bench test started without propellers. Software reports fresh receiver frames; drive remains disarmed because observed CH2 is about 1178 us rather than the configured stop threshold (1086 us) while CH3 is centered. Motor direction and loaded behavior still need confirmation. |
| PWM receiver hardware | Receiver pulse levels, channel order, timeout, and FS-T4B/FS-R6B operation | Live capture verified on GPIO7-10: all four channels reported valid with `invalid_pulses=0`. Exact receiver model, measured signal voltage, and control-direction calibration remain unverified. |
| Receiver/GPS/motor system | End-to-end command flow, link loss, and failsafe | Pending. |

The 57 Unity cases cover vessel mode and topology-aware mixing (3), H-bridge mapping/safety (9), role-aware servo mapping/configuration and actuator gating (9), PWM receiver GPIO/binding (4), legacy receiver mapping (10), new receiver-drive mapping (11), hardware-test sequencing (4), and receiver console-logging rate limiter/formatters (7). The new servo-travel test and receiver-drive tests have not been run on target. Pure tests do not initialize actuator outputs. No H-bridge oscilloscope waveform has been recorded.

For safe probing of receiver inputs, servo PWM, or dedicated D0-D3 motor-test pulses, follow the [Oscilloscope Test Guide](oscilloscope-testing.md). The normal drive image is now uploaded with the actuator gate enabled for propeller-free testing; the dedicated H-bridge ramp image was not uploaded.

## Minimum Criteria for a New Driver

- Compile with `-Werror` in all affected profiles.
- Test valid and invalid configurations, boundaries, null pointers, and occupied resources.
- Verify safe state at startup, after initialization failure, and after input loss.
- Test on hardware with the model, pinout, and power documented.
- Record the command, configuration, expected/observed result, and limitations in [logs/testing.md](../logs/testing.md).

Unity component tests live next to each driver/control component. The top-level `test/` directory remains available for PlatformIO integration tests.