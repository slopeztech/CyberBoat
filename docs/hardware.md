# Hardware

## Confirmed Hardware

| Item | Confirmed detail | Status |
| --- | --- | --- |
| Board | Waveshare ESP32-S3-Zero / ESP32-S3FH4R2 | Model identified by the user and manufacturer documentation. |
| Flash | 4 MB | Detected when the board was connected. |
| PSRAM | 2 MB | Detected by esptool; not used by the application yet. |
| USB | Native USB Serial/JTAG | Used for upload and serial console. |
| Onboard RGB | WS2812 on GPIO 21 | Confirmed by Waveshare documentation. |
| RC radio | FS-T4B transmitter; paired receiver model not identified | Reported by the user; receiver output pinout and signal level are unverified. |
| RC receiver compatibility | FlySky FS-R6B and similar generic individual-channel PWM receivers | Driver and channel mapping are receiver-model-agnostic; any receiver exposing one PWM pulse per channel on its own pin is supported. |
| Servo | One servo connected by the user to GPIO13, 5 V, and GND | Model, power-source capacity, and visible shaft movement are not yet verified. |

PlatformIO uses `esp32-s3-devkitm-1`, recommended by Waveshare for this board, with 4 MB flash and the ESP-IDF single-app partition table. This is a build profile; it does not identify the PCB as a physical DevKitM.

## Configured Pins

The numbers below are ESP32-S3 GPIO numbers, not physical header-pin numbers. Locate the matching `GPIOx` label on the Waveshare board pinout; verify that the pad is exposed and available on the exact board revision before wiring.

| Function | Default GPIO | Connect to | Runtime status |
| --- | ---: | --- | --- |
| Onboard WS2812 | 21 | Already on the board; do not connect an external load here. | Active through RMT; not a conventional PWM signal. |
| Rudder servo signal | 18 | Rudder servo signal wire; power the servo from a suitable external supply and share ground. | Normal rudder mode with outputs enabled, or selected rudder hardware test. |
| Auxiliary servo signal | 13 | Auxiliary servo signal wire; power the servo from a suitable external supply and share ground. | Dedicated hardware-test profile defaults to this role; normal auxiliary mode also supports it when outputs are enabled. |
| H-bridge motor 1 left (IN1 / IN2) | 3 / 4 | GPIO3->IN1 and GPIO4->IN2; logic inputs, not motor terminals. GPIO3 is a strapping pin. | Normal twin-motor profile when actuator outputs are enabled; disabled in current firmware. |
| H-bridge motor 2 right (IN3 / IN4) | 5 / 6 | GPIO5->IN3 and GPIO6->IN4; logic inputs, not motor terminals. | Normal twin-motor profile when actuator outputs are enabled; disabled in current firmware. |
| Receiver CH1..CH4 signal | 7 / 8 / 9 / 10 | One independent receiver PWM signal per GPIO; connect receiver ground to board GND. | Receiver capture initializes these inputs in the normal PWM profile. |
| Receiver CH5..CH8 signal | 12 / 13 / 14 / 15 | Optional additional independent PWM signals. | Configurable; conflicts with the auxiliary servo on GPIO13 if CH6 is enabled. |
| GPS | Unassigned | No wiring until a module and interface are selected. | No GPS driver. |

Only connect D0-D3 to the bridge's logic control inputs after verifying the module labels; never connect them to motor terminals. The dedicated `esp32-s3-zero-hbridge-tests` profile ramps both motors together through 10% increments to 100%, holding each level for 300 ms, then returns both to zero and disarms. Remove propellers and secure motors before uploading; a 100% step can cause rapid movement. For a logic-only scope test, disconnect motor power/leads as appropriate. The normal application leaves GPIO3-6 inactive.

GPIO3 (D0 by default) is an ESP32-S3 strapping pin sampled during reset. Check the H-bridge input circuit and any pull resistors; do not connect D0 to GPIO3 if the module forces an incompatible reset level or prevents boot. In that case reassign D0 to another free GPIO in Kconfig before connecting the bridge. See the [ESP-IDF GPIO restrictions](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html#gpio-summary).

The selected twin-motor + auxiliary-servo mapping is zero-based: CH1 on GPIO7 selects servo position; CH2 on GPIO8 sets the power limit; CH3 on GPIO9 proportionally scales speed from reverse through neutral to forward; CH4 on GPIO10 steers differentially. Combined power is CH2 multiplied by the absolute CH3 fraction, so 50% on each produces 25% motor power. Left motor defaults to motor 1 (GPIO3/4), right to motor 2 (GPIO5/6); Kconfig can swap the motor-side assignment and independently reverse each motor's electrical polarity. CH4 only reduces the inside motor. All four channels must be fresh; otherwise the driver emergency-stops and disarms the motors. GPIO3 reset bias must be checked against the bridge before wiring.

GPIO19 and GPIO20 are reserved while the USB Serial/JTAG console is enabled: GPIO19 is USB D- and GPIO20 is USB D+. Do not assign either to a receiver, servo, or H-bridge signal unless the firmware is deliberately reconfigured to stop using that USB interface. The auxiliary-servo default is GPIO13; confirm pad access on the exact board revision before wiring.

Change GPIO assignments with the corresponding `CONFIG_CYBERBOAT_*_GPIO` setting in Kconfig. See [Configuration](configuration.md) for the mode matrix and [Oscilloscope Test Guide](oscilloscope-testing.md) for point-by-point signal checks. After changing a pin, rebuild and inspect the effective `sdkconfig.esp32-s3-zero`; do not infer GPIO numbering from connector position.

The exact H-bridge/motor and GPS models, wiring, and electrical limits have not been verified. One servo is connected, but its model and supply capability have not been documented. The FS-T4B transmitter is available; the paired receiver variant and wiring still need verification.

The H-bridge driver expects two ESP32 logic-input pins per motor (`IN1/IN2`, then `IN3/IN4`). It does not drive module enable inputs such as L298 `ENA/ENB`. The default GPIO values are examples, not a wiring recommendation; confirm the ESP32-S3-Zero pinout, bridge logic thresholds, enable jumpers, motor supply, polarity, and common ground before wiring. The normal application now has the receiver-to-H-bridge command path, but the production actuator safety gate is disabled, so it will not initialize GPIO3-6 or output motor PWM until that gate is enabled.

The PWM receiver driver expects one independent servo-style PWM output per channel, not a combined PPM, SBUS, or serial data line. Before wiring, confirm the receiver actually exposes individual PWM channel outputs, their signal voltage, ground, and connector pinout. ESP32 GPIO inputs are not 5 V tolerant. Measure the receiver's signal level before connecting it; use a suitable level shifter if required by the receiver output.

## H-Bridge D0-D3 Test

For a bridge whose logic inputs are marked D0-D3, the default map is D0->GPIO3, D1->GPIO4, D2->GPIO5, D3->GPIO6. Verify from the bridge datasheet that these are logic inputs, not motor terminals. The dedicated test sends the same positive command to both motors: zero for one second, then 10%, 20%, ... 100% for 300 ms per step, then zero for one second and disarm. The bridge enable pins are not controlled by this driver. GPIO3 is a strapping pin; verify its reset-level requirements before using the default D0 mapping.

Before upload, remove propellers and secure the motors. For a logic-only scope check, disconnect motor power/leads as appropriate. The sequence reaches 100% duty and can accelerate motors rapidly; verify the bridge, supply, enable pins, current limit, and common ground first.

## Decisions Required Before Wiring

- Servo model and power requirements; safe pulse range, frequency, and stall current.
- ESC or H-bridge, signal type, motor count, and fail-safe behavior.
- Receiver model and protocol: SBUS, PWM, nRF24L01, or Wi-Fi. Kconfig options are not implemented drivers.
- GPS model, UART, baud rate, and message format.
- Power distribution, common ground, protection, connectors, and antennas.
- Complete GPIO map without conflicts with USB, PSRAM, onboard LED, or selected peripherals.

Do not power a servo from the board's 3.3 V supply unless its datasheet and the power design explicitly allow it. Size the supply appropriately and verify common ground and logic levels against the datasheets before connecting hardware.

For physical pad identification, use the [Waveshare ESP32-S3-Zero pinout](https://www.waveshare.com/wiki/ESP32-S3-Zero#Pinout). GPIO19/20 USB assignments are documented in [ESP-IDF USB Serial/JTAG hardware requirements](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/usb-serial-jtag-console.html#hardware-requirements).

## Oscilloscope Safety

- Power down before moving probes or changing wiring. Remove propellers and disconnect motors during logic-level checks.
- Use a high-impedance 10x probe; do not enable a 50-ohm scope input on a GPIO signal.
- Connect the probe ground only to board/receiver logic GND. A bench oscilloscope ground clip is often earth-bonded; never clip it to an H-bridge motor terminal or switching output. Use an appropriately rated differential or isolated probe for measurements across motor terminals.
- Do not connect a receiver signal to an ESP32 GPIO until its high level is confirmed to be within the ESP32-S3 input limits.
- The unit-test profile deliberately initializes neither receiver interrupts nor actuator PWM. A successful Unity run is not an oscilloscope or peripheral hardware test.