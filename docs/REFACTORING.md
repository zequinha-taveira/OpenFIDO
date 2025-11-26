# Refactoring Documentation

This document describes the architectural changes introduced in the refactoring of November 2025.

## Summary of Changes

### 1. Include Path Standardization
All include paths have been standardized to use direct includes (e.g., `#include "header.h"`) instead of relative paths. The build system (`CMakeLists.txt`) now handles include directories globally.

### 2. Common Headers
A new `src/common` directory was created containing:
- `types.h`: Common type definitions and result codes (`openfido_result_t`).

### 3. FIDO2 Module Reorganization
The `src/fido2` module has been reorganized into subdirectories:
- `core/`: Core CTAP2 and U2F implementation (`ctap2.c`, `cbor.c`, `u2f.c`).
- `commands/`: CTAP2 command handlers (`ctap2_commands.c`).
- `extensions/`: Protocol extensions (`ctap2_config.c`, `ctap2_credential_mgmt.c`, `ctap2_large_blobs.c`).

### 4. Smart Card Abstraction
A new `src/smartcard` directory was created with `smartcard.h` defining a common interface for smart card protocols (PIV, OpenPGP). This allows for better decoupling from the USB CCID layer.

### 5. Logging Improvements
The logging system (`src/utils/logger.h`) now supports:
- Runtime log level configuration (`logger_set_level`).
- Context-aware logging (automatically includes filename and line number).
- Standardized log levels (`ERROR`, `WARN`, `INFO`, `DEBUG`).

## Migration Guide

### For New Modules
- Add your module source files to `CMakeLists.txt`.
- Add your module directory to `include_directories` in `CMakeLists.txt`.
- Use `#include "logger.h"` for logging.
- Use `OPENFIDO_OK` and `OPENFIDO_ERROR` codes from `types.h`.

### For Existing Code
- If you encounter include errors, ensure your module path is in `CMakeLists.txt`.
- Replace relative includes with direct includes.
