# Development Guide

## Current Components

- `components/cyberboat_types`: data structures without transport details.
- `components/cyberboat_hal`: contracts consumed by the application and control layers.
- `components/cyberboat_config`: Kconfig options.
- `components/cyberboat_control`: deterministic control without peripheral access.
- `components/cyberboat_driver_hbridge`: configurable one/two-motor direction-and-duty driver.
- `components/cyberboat_driver_*`: concrete ESP-IDF/hardware adapters.
- `src`: composition root connecting configuration, HALs, and drivers.

## Adding a Driver

1. Confirm the physical component, protocol, datasheet, pins, power, and safe state.
2. Define or extend the HAL contract without exposing ESP-IDF-specific types to control logic.
3. Create a `cyberboat_driver_*` component with `CMakeLists.txt`, a public API under `include/cyberboat/drivers/`, and implementation under `src/`.
4. Declare IDF dependencies and GPIO/timing/calibration Kconfig options. Validate settings before acquiring peripherals.
5. Return `esp_err_t`; do not abort from a library with `ESP_ERROR_CHECK`.
6. Add diagnostics, parameter tests, and hardware tests when possible.
7. Document APIs with Doxygen, update affected guides, and record validation in `logs/`.

For a bridge with D0-D3 logic inputs, map the selected board's two inputs per motor through Kconfig. Check whether the module has separate enable pins; this driver does not own those pins. Keep `CONFIG_CYBERBOAT_ACTUATORS_ENABLED` disabled during bring-up until wiring and fail-safe behavior are validated.

## Conventions

- Shared types and APIs use explicit ranges and units; normalized axes use `-1000..1000`.
- Prefer static storage and avoid dynamic allocation in the control path.
- Do not assume a software command confirms physical movement.
- Keep propulsion disabled until arming, timeout, link-loss, and emergency-stop behavior exist.
- Every new or modified C/C++ API must have Doxygen comments in the source.

## Common Commands

```powershell
platformio run -e esp32-s3-zero
platformio run -e esp32-s3-zero -t upload
platformio device monitor -p COM8 -b 115200
platformio run -e esp32-s3-zero-unit-tests
doxygen Doxyfile
```

The dedicated test profile selects two motors for compile-time coverage while leaving `CONFIG_CYBERBOAT_ACTUATORS_ENABLED` off. Its generated `sdkconfig.unit-tests` file is ignored and must not be copied over the production `sdkconfig.esp32-s3-zero`.

Doxygen must be installed to generate HTML. `Doxyfile` excludes `managed_components` and `.pio`.