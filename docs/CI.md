# GitHub Actions CI Workflow

This document describes the Continuous Integration (CI) workflow for OpenFIDO.

## Overview

The CI workflow automatically builds and tests the OpenFIDO project on every push and pull request to the `main` and `develop` branches.

## Workflow Jobs

### 1. Code Formatting Check

Validates that all C source code follows the project's formatting standards defined in `.clang-format`.

- **Runs on**: Ubuntu Latest
- **Tool**: clang-format
- **Scope**: All `.c` and `.h` files in `src/` and `tests/`
- **Failure**: If any file doesn't match the formatting rules

### 2. Native Build & Tests

Builds the project natively on Ubuntu and runs the complete test suite.

- **Runs on**: Ubuntu Latest
- **Build System**: CMake
- **Dependencies**: mbedTLS
- **Tests**: All unit tests via CTest
- **Artifacts**: Test results uploaded for review

### 3. PlatformIO Multi-Platform Builds

Builds firmware for all supported embedded platforms using PlatformIO.

**Supported Platforms**:
- ESP32-S3 (Espressif ESP32-S3 DevKit)
- ESP32-S2 (Espressif ESP32-S2 Saola)
- STM32F4 (STM32 Nucleo F401RE)
- nRF52840 (Nordic nRF52840 DK)

**Features**:
- Parallel builds for faster CI
- Dependency caching for improved performance
- Firmware artifacts uploaded (30-day retention)

### 4. Build Summary

Aggregates results from all jobs and provides a final pass/fail status.

## Viewing CI Results

1. Navigate to the **Actions** tab in your GitHub repository
2. Click on the latest workflow run
3. Review individual job results
4. Download artifacts (firmware binaries, test results) if needed

## CI Badge

The README includes a CI status badge that shows the current build status:

[![CI](https://github.com/yourusername/OpenFIDO/workflows/CI/badge.svg)](https://github.com/yourusername/OpenFIDO/actions)

## Troubleshooting

### Format Check Fails

Run locally before pushing:
```bash
find src tests -name '*.c' -o -name '*.h' | xargs clang-format -i
git add .
git commit -m "Fix code formatting"
```

### Native Tests Fail

Run tests locally:
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
ctest --output-on-failure
```

### PlatformIO Build Fails

Test locally:
```bash
pio run -e esp32s3  # or esp32s2, stm32f4, nrf52840
```

## Performance

- **Format Check**: ~30 seconds
- **Native Build & Tests**: ~2-3 minutes
- **Each PlatformIO Build**: ~3-5 minutes (with cache)
- **Total Workflow Time**: ~5-8 minutes (parallel execution)

## GitHub Actions Minutes

- **Public repositories**: Free unlimited minutes
- **Private repositories**: Consumes from your account's monthly quota

## Customization

To modify the CI workflow, edit `.github/workflows/ci.yml`:

- **Change trigger branches**: Modify the `on.push.branches` and `on.pull_request.branches` sections
- **Add/remove platforms**: Update the `matrix.environment` list
- **Adjust caching**: Modify the cache keys in the PlatformIO job
- **Change artifact retention**: Update `retention-days` in upload steps

## Future Enhancements

Potential improvements to the CI workflow:

- [ ] Code coverage reporting (Codecov/Coveralls)
- [ ] Static analysis (Cppcheck, Clang-Tidy)
- [ ] Security scanning (CodeQL)
- [ ] Automated releases on tags
- [ ] Hardware-in-the-loop testing
- [ ] Performance benchmarking
