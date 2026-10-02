# Oscilloscope Test Guide

This procedure separates signals the firmware generates today from GPIO assignments reserved for future integration. Read [Hardware](hardware.md) before connecting anything; the receiver variant, exact board-pad availability, and external peripheral wiring have not yet been validated.

## Signal Availability

| Signal | Probe point | Expected observation | Available now |
| --- | --- | --- | --- |
| Receiver PWM input | GPIO 7-10 by default, measured relative to board GND | One pulse train per receiver channel; accepted high pulse is 900-2100 us by default; samples expire after 100 ms without a fresh pulse. | Yes, with a compatible individual-PWM receiver connected. |
| Rudder servo PWM | GPIO 18 by default, relative to board GND | 50 Hz default (20 ms period); default center command is 1500 us. | Normal firmware with rudder mode/output enabled, or dedicated servo diagnostic. |
| Auxiliary servo PWM | GPIO 13 by default, relative to board GND | 50 Hz default (20 ms period); receiver CH1 selects a nominal 90-degree total sweep when the actuator gate is enabled. | Dedicated servo test, or normal twin-motor + auxiliary-servo mode with actuator gate enabled. |
| H-bridge logic PWM | Motor 1 IN1/IN2=GPIO3/4, motor 2 IN3/IN4=GPIO5/6 | Normal drive profile: signed PWM magnitude follows CH2 and direction follows CH3; CH4 reduces the inside motor. | Normal twin-motor mode only when actuator gate is enabled, or the dedicated ramp test. GPIO3 is a strapping pin. |
| WS2812 data | GPIO 21 | RMT-encoded LED data, not hobby-servo PWM. | Yes, but it is not a useful servo PWM test point. |

The unit-test profile intentionally initializes no receiver GPIO interrupts and no actuator PWM. Its test results do not prove electrical waveforms or peripheral wiring. The current production profile has the actuator gate enabled for the propeller-free bench test; use extra care around the powered motor bridge.

GPIO19 and GPIO20 are USB Serial/JTAG D- and D+ respectively on this board configuration. The GPIO validators reject them while the native USB console is enabled; do not probe or reassign them as actuator/receiver pins while using that console.

## Probe Setup and Safety

1. Remove propellers. Disconnect motors from any bridge during GPIO checks. Keep actuator outputs disabled until the bench setup is inspected.
2. Use a 10x probe with the oscilloscope input set to high impedance (typically 1 MOhm), DC coupling, and a short ground spring where available. Do not use a 50-ohm termination on an ESP32 GPIO.
3. Connect the probe ground only to board/receiver logic GND. Most bench oscilloscope ground clips are connected to protective earth and all channel grounds are common.
4. Never connect a conventional probe ground clip to an H-bridge output terminal or either motor lead. Use an appropriately rated differential or isolated probe for a voltage measured across a motor or bridge output.
5. Power down before moving probes or changing wiring. Do not let a probe tip bridge adjacent GPIO pads.
6. Confirm receiver signal voltage at the receiver before connecting it to the ESP32. ESP32-S3 GPIOs are not 5 V tolerant; add a suitable level shifter if the measured signal exceeds the MCU input limits.

For an initial view of a 20 ms period, try 2 ms/div, trigger on a rising edge near 1.5 V, and use a 0-5 V vertical range. Zoom to approximately 0.5 ms/div to inspect a 1-2 ms high pulse. Adjust these values for the measured voltage and signal rate; do not treat them as electrical requirements.

## Receiver Input Test

1. Identify the receiver's exact model and output connector labels from its own documentation. The FS-T4B transmitter name does not identify the paired receiver board or prove that it exposes parallel PWM. The driver is generic and is equally compatible with a FlySky FS-R6B or similar individual-channel PWM receiver.
2. Power the receiver as its datasheet specifies, with the signal wire not yet connected to the ESP32. Measure the receiver output relative to receiver GND. Record frequency, high-pulse width, and high voltage while moving one transmitter control at a time.
3. Verify that each output is an independent servo-style PWM signal. The current driver does not decode PPM, SBUS, or serial receiver data. The default software pulse acceptance range is 900-2100 us; measure the actual signal rather than assuming 50 Hz or a particular channel order.
4. Power down. Join receiver GND to board GND. Connect the verified receiver signals to the configured receiver GPIOs, one channel at a time. Default assignments are CH1->GPIO7, CH2->GPIO8, CH3->GPIO9, and CH4->GPIO10. GPIO assignments can be changed with `CONFIG_CYBERBOAT_PWM_RECEIVER_CHn_GPIO`.
5. Probe the signal again at the ESP32-side GPIO. Confirm the voltage is within the ESP32-S3 input limits and that the pulse width is unchanged. If it is not, stop and correct the level/interface before running the firmware.
6. Build and upload the normal profile, then observe the serial monitor. With `CONFIG_CYBERBOAT_RECEIVER_CONSOLE_LOG` enabled (the default), each channel's GPIO, pulse width, age, and valid/stale status print periodically, along with the mapped command; this is the fastest way to confirm wiring and channel order. A fresh mapped frame should also produce `PWM receiver signal valid`; removing a required channel or stopping its pulses should produce a signal-lost warning after the configured timeout. Do not connect actuators during this test.

In the selected twin-motor + auxiliary-servo mode, mapping is zero-based: CH1 is servo position, CH2 is the power limit, CH3 is a proportional signed speed command, and CH4 is differential steering. CH2=50% and CH3=50% should map to 25% motor output before steering; full CH3 uses the full CH2 limit. Test channel calibration with the boat secured, propellers removed, and servo linkage disconnected. CH2 at stop and CH3 neutral are required before the software arms; stale/missing input emergency-stops both motors.

## Servo Output Test

Use the dedicated hardware-test profile for a servo movement check. It defaults to the auxiliary servo in small-step mode and bypasses receiver input, so the servo moves without an RC transmitter. Select center mode in `menuconfig` for a stable scope pulse. The diagnostic never initializes the H-bridge. Do not use the Unity profile for oscilloscope testing: that profile prevents actuator initialization.

1. With power off, connect the servo to GPIO13/GND and a suitable external supply, then build and upload `esp32-s3-zero-hardware-tests` using the commands in [Configuration](configuration.md#hardware-diagnostic-profile). Confirm `sdkconfig.hardware-tests` selects auxiliary role, GPIO13, and small-step mode. Keep motors and the H-bridge disconnected.
2. Probe GPIO13 relative to board GND. The test logs role, GPIO, mode, frequency, and pulse width at startup. At the default center command, expect a 20 ms period and approximately 1500 us high time. No transmitter signal is needed.
3. The default movement sequence cycles through positions 0, +50, 0, and -50, changing once per second. For a linearly calibrated 180-degree servo, each center-to-step change is approximately 4.5 degrees; actual angle depends on the servo mechanics and calibration. The sequence does not command full endpoints. Select center mode to hold a steady 1500 us pulse.
4. To test the rudder instead, select the rudder test role and a vessel mode that includes the rudder. Probe GPIO18. The component refuses a servo role that the selected vessel mode does not enable.
5. Do not connect a physical servo or test full travel until pulse endpoints have been calibrated to that servo's datasheet. The software defaults of 500/1500/2500 us do not guarantee that a particular servo can safely reach those endpoints.
6. After signal-only checks pass, power down before connecting the servo. Use a supply rated for the servo's voltage and stall current, connect supply ground to board GND, and connect only the signal wire to the configured GPIO. Never power a servo from an ESP32 GPIO or assume the board's 3.3 V rail can supply it.

When finished, upload the safe production environment as described in [Configuration](configuration.md#hardware-diagnostic-profile).

## H-Bridge Status

Normal firmware still does not bind or initialize the H-bridge. The dedicated `esp32-s3-zero-hbridge-tests` profile starts both motors at zero, ramps them together through 10% increments to 100% (300 ms per step), holds zero for one second, then stops and disarms. See the [H-Bridge D0-D3 Test](configuration.md#h-bridge-d0-d3-test) instructions before uploading it. Confirm that D0-D3 are module logic inputs, not motor terminals; secure motors and remove propellers because the final step is full commanded duty.

The driver uses 1000 Hz and applies PWM to one direction input at a time. During the ramp test both forward inputs receive matching duty commands and both reverse inputs stay low. Probe ESP-to-bridge logic inputs relative to logic GND; validate with motor supply/leads disconnected first. Never infer motor-terminal waveforms from a GPIO probe or attach an earth-grounded probe clip to bridge outputs. Use a rated differential/isolated probe for measurements across motor terminals.

## Record Measurements

For each test, record the board revision, firmware build profile, vessel mode, effective GPIO map, peripheral model, supply voltage, scope/probe settings, measured frequency, high-pulse width, logic-high voltage, expected value, and pass/fail result. Keep receiver electrical measurements separate from servo-output measurements, and mark H-bridge physical testing as pending until the application actually produces those outputs.