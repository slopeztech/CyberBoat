# Standards and Quality

## Current Status

The project follows some practices aligned with MISRA C and CERT C, but does not claim compliance or certification. No MISRA/CERT analyzer is currently installed or configured.

## Compiler Gate

Project-owned C components use `-Wall`, `-Wextra`, `-Werror`, `-Wconversion`, `-Wsign-conversion`, `-Wshadow`, `-Wstrict-prototypes`, and `-Wmissing-prototypes`. ESP-IDF dependencies use the framework's own compiler options, not the project component options.

## Requirements for a Compliance Claim

1. Agree on the MISRA C and CERT C editions, profiles, and applicable rules.
2. Select an analyzer that supports those rules and pin its version in CI.
3. Define mandatory rules, justified deviations, and approval owners.
4. Analyze project-owned code, review findings, and verify configuration changes.
5. Keep reproducible evidence of tools, versions, reports, and tests.

A clean build does not replace static analysis, security review, unit tests, hardware tests, or formal assessment.

## Doxygen

Public APIs document types, fields, units, ranges, parameters, return values, and errors. `Doxyfile` enables warnings for undocumented items. Run `doxygen Doxyfile` when Doxygen is installed.