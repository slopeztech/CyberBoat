# CyberBoat

Modular RC boat controller firmware for the Waveshare ESP32-S3-Zero. CyberBoat captures independent PWM receiver channels and maps them to differential motor commands and an auxiliary servo.

> **Prototype status:** receiver capture and command mapping have been exercised with live receiver signals. Motor direction, bridge wiring, loaded performance, and servo travel still require bench verification. Generic build defaults leave actuator outputs disabled; enable them only for a secured, propeller-free test setup.

## Controls

The default four-channel mapping uses receiver CH1-CH4 on GPIO7-10. Channel indexes in Kconfig are zero-based.

| Channel | GPIO | Function |
| --- | ---: | --- |
| CH1 | 7 | Auxiliary servo position, limited to 50% of calibrated travel by default. |
| CH2 | 8 | Motor power limit, from stop to full power. |
| CH3 | 9 | Proportional signed speed: reverse through zero to forward. |
| CH4 | 10 | Differential steering; reduces the inside motor. |

Motor power is CH2 multiplied by the magnitude of CH3. For example, CH2 at 50% and CH3 at 50% produce 25% motor output. Full CH3 travel uses all of CH2's available power. CH3 centered or CH2 at stop commands zero. Missing or stale input stops and disarms both motors.

Default pulse calibration is based on measured transmitter output and can be adjusted in Kconfig:

| Input | Minimum / center / maximum (us) |
| --- | --- |
| CH1 servo | 976 / 1492 / 1933 |
| CH2 throttle | 1056 / 1377 / 1698 |
| CH3 direction/speed | 1096 / 1472 / 1924 |
| CH4 steering | 993 / 1508 / 1950 |

Motor 1 on D0/D1 (GPIO3/4) is the default left motor; motor 2 on D2/D3 (GPIO5/6) is the default right motor. Kconfig can swap the physical side assignment and independently invert each motor's polarity. GPIO3 is an ESP32-S3 strapping pin; verify the bridge input does not interfere with reset.

## Hardware and Software

- Waveshare ESP32-S3-Zero / ESP32-S3FH4R2, with 4 MB flash and 2 MB PSRAM.
- PlatformIO with Espressif32 and ESP-IDF. Last validated with Espressif32 7.1.3 and ESP-IDF 6.1.0.
- An RC receiver with one independent servo-style PWM output per channel. Combined PPM, SBUS, and serial receiver protocols are not implemented.
- A compatible H-bridge for brushed DC motors. The driver controls the two direction inputs per motor; it does not control bridge enable pins and has no current sensing or physical emergency disconnect.

ESP32 GPIOs are not 5 V tolerant. Verify receiver voltage, common ground, GPIO availability, bridge logic levels, motor supply, and connector pinout before wiring. Power servos from a suitable external supply, not an ESP32 GPIO.

## Build and Test

Install PlatformIO Core or the PlatformIO IDE extension, then build the default production environment from the project directory:

```powershell
platformio run
```

The default environment is `esp32-s3-zero`. Build and run the Unity suite on the board with:

```powershell
platformio run -e esp32-s3-zero-unit-tests
platformio run -e esp32-s3-zero-unit-tests -t upload --upload-port COM8
platformio device monitor -p COM8 -b 115200
```

Replace `COM8` with the serial port assigned by your system. The Unity profile leaves actuator outputs disabled. The project currently compiles 57 Unity tests; mapping tests do not prove electrical or mechanical behavior.

Configure vessel mode, channel calibration, GPIOs, motor side, and polarity with:

```powershell
platformio run -e esp32-s3-zero -t menuconfig
```

## Safety

The checked-in `sdkconfig.defaults` keeps `CONFIG_CYBERBOAT_ACTUATORS_ENABLED` disabled. Review the effective sdkconfig before upload; a local sdkconfig can override defaults. Before enabling outputs, verify bridge/motor wiring, polarity, supply and current limits, GPIO3 reset bias, and the physical stop procedure. Secure the boat and remove propellers before any motor test. Rearming requires fresh channels, CH2 at stop, and CH3 centered. A stale or missing channel triggers a software stop, but this is not a substitute for disconnecting motor power.

The `esp32-s3-zero-hbridge-tests` environment deliberately ramps both motors to 100% duty. It is a diagnostic, not the normal boat firmware. Do not upload it unless the motors are secured and the propellers are removed.

## Architecture and Documentation

Control logic, receiver capture, hardware abstraction interfaces, and concrete ESP-IDF drivers are separated into components. Project-owned C code builds with strict warnings including `-Werror`.

- [User guide](docs/user-guide.md)
- [Hardware and pin map](docs/hardware.md)
- [Configuration reference](docs/configuration.md)
- [Architecture and implementation status](docs/architecture.md)
- [Testing status](docs/testing.md)
- [Oscilloscope and bench safety](docs/oscilloscope-testing.md)
- [Development guide](docs/development.md)
- [Standards and quality](docs/standards.md)