# User Guide

## Current Status

The production configuration selects two differential motors plus an auxiliary servo and captures four PWM receiver channels. The receiver drive firmware is uploaded to COM8 with the production actuator gate enabled for the user's propeller-free bench test. It starts disarmed and requires CH2 at its low stop and CH3 centered before enabling the motor bridge. GPS and non-PWM receiver transports are not implemented.

## Build and Upload

With PlatformIO installed and the board connected over USB:

```powershell
platformio run -e esp32-s3-zero
platformio run -e esp32-s3-zero -t upload
platformio device monitor -p COM8 -b 115200
```

Replace `COM8` with the port assigned on your computer. A successful upload does not test peripherals that are not connected.

Choose one of the eight `CYBERBOAT_VESSEL_MODE` options in Kconfig before building. The choice covers one/two motors, optional rudder, and optional auxiliary servo; role-specific pins and PWM calibration are configured separately. Derived motor/servo flags are not independent user options.

Use [Hardware](hardware.md) for the GPIO-to-peripheral wiring map and [Oscilloscope Test Guide](oscilloscope-testing.md) before probing signals or enabling actuator outputs.

For a dedicated servo movement check, build `esp32-s3-zero-hardware-tests`. By default it moves the auxiliary servo on GPIO13 in small steps around center, bypasses the receiver, and never initializes the H-bridge. Select center mode for a steady oscilloscope signal. Upload `esp32-s3-zero` afterward to return to normal operation.

## FS-T4B / FS-R6B and Similar Receivers

The driver expects one independent servo-style PWM output per receiver channel; it supports the FS-R6B and similar receivers that provide this output format. Confirm channel labels, signal voltage, and connector pinout before wiring. Defaults are CH1 GPIO7, CH2 GPIO8, CH3 GPIO9, and CH4 GPIO10. In the current twin-motor + auxiliary-servo profile, CH1 selects servo position, CH2 sets a power limit, CH3 proportionally scales speed and its sign selects direction, and CH4 steers differentially. Combined power is CH2 multiplied by the magnitude of CH3: with CH2 at 50%, CH3 at 50% delivers 25%; CH3 at full travel delivers all of CH2's available power. CH2 at its low endpoint or CH3 at center commands zero. CH4 only reduces the inside motor. Missing or stale input stops and disarms both motors; rearming requires fresh channels, throttle at stop, and CH3 neutral.

The supplied approximate calibration is in Kconfig: CH1 `976/1492/1933 us`, CH2 stop/full `1056/1698 us`, CH3 reverse/neutral/forward `1096/1472/1924 us`, and CH4 left/center/right `993/1508/1950 us`. Adjust these under **Four-channel differential drive mapping** in `menuconfig` for another transmitter. `CONFIG_CYBERBOAT_RC_DRIVE_LEFT_MOTOR_INDEX=0` assigns motor 1 (D0/D1) to the left; set it to `1` to swap physical sides. `CONFIG_CYBERBOAT_MOTOR_1_REVERSED` and `CONFIG_CYBERBOAT_MOTOR_2_REVERSED` independently correct wiring polarity. Servo travel defaults to 50% of its calibrated range (nominally 90 degrees total for a 180-degree calibration); confirm actual endpoints with the servo datasheet and linkage disconnected.

## Receiver Console Logging

`CONFIG_CYBERBOAT_RECEIVER_CONSOLE_LOG` is enabled by default. The serial console prints each raw channel and the mapped left/right motor and servo targets every 500 ms. Disable it in `menuconfig` when no longer needed; see [Configuration](configuration.md#receiver-console-logging) for sample output.

## Servo

Do not enable `CONFIG_CYBERBOAT_ACTUATORS_ENABLED` until the bridge and motor model, wiring, polarity, supply/current limit, GPIOs, common ground, and stop procedure are verified. Remove propellers and secure the boat for the first motor test. The drive mapper does not replace a physical emergency stop. The auxiliary servo follows CH1 within its configured travel; keep its linkage disconnected during endpoint checks.

The driver diagnostics show configuration and the last software command. They do not detect a disconnected or stalled servo, or report its physical position.