cmake_minimum_required(VERSION 3.16)

# Project configuration
project(OpenFIDO VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Platform selection
if(NOT DEFINED PLATFORM)
    set(PLATFORM "ESP32" CACHE STRING "Target platform (ESP32, STM32, NRF52)")
endif()

message(STATUS "Building for platform: ${PLATFORM}")

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_C_FLAGS_DEBUG "-g -O0")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# Source files
set(FIDO2_SOURCES
    src/fido2/ctap2.c
    src/fido2/cbor.c
    src/fido2/authenticator.c
)

set(CRYPTO_SOURCES
    src/crypto/crypto.c
    src/crypto/attestation.c
)

set(STORAGE_SOURCES
    src/storage/storage.c
)

set(USB_SOURCES
    src/usb/usb_hid.c
    src/usb/usb_ccid.c
    src/usb/ykman.c
    src/usb/usb_keyboard.c
)

set(PIV_SOURCES
    src/piv/piv.c
)

set(OPENPGP_SOURCES
    src/openpgp/openpgp.c
)

set(OATH_SOURCES
    src/oath/oath.c
    src/oath/yubikey_otp.c
    src/oath/challenge_response.c
)

set(UTILS_SOURCES
    src/utils/logger.c
    src/utils/buffer.c
)

# Platform-specific configuration
if(PLATFORM STREQUAL "ESP32")
    # ESP32 uses ESP-IDF build system
    # This CMakeLists.txt is for reference; actual ESP32 build uses idf.py
    message(STATUS "ESP32 platform - use idf.py for building")
    
    set(HAL_SOURCES src/hal/esp32/hal_esp32.c)
    
elseif(PLATFORM STREQUAL "STM32")
    set(HAL_SOURCES src/hal/stm32/hal_stm32.c)
    
    # STM32-specific flags
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
    
elseif(PLATFORM STREQUAL "NRF52")
    set(HAL_SOURCES src/hal/nrf52/hal_nrf52.c)
    
    # nRF52-specific flags
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
    
else()
    message(FATAL_ERROR "Unsupported platform: ${PLATFORM}")
endif()

# Include directories
include_directories(
    src
    src/fido2
    src/crypto
    src/storage
    src/hal
    src/usb
    src/piv
    src/openpgp
    src/oath
    src/utils
)

# Main executable
add_executable(openfido
    src/main.c
    ${FIDO2_SOURCES}
    ${CRYPTO_SOURCES}
    ${STORAGE_SOURCES}
    ${HAL_SOURCES}
    ${USB_SOURCES}
    ${PIV_SOURCES}
    ${OPENPGP_SOURCES}
    ${OATH_SOURCES}
    ${UTILS_SOURCES}
)

# Link libraries (platform-specific)
if(PLATFORM STREQUAL "STM32" OR PLATFORM STREQUAL "NRF52")
    # Link mbedTLS for crypto
    find_package(MbedTLS REQUIRED)
    target_link_libraries(openfido MbedTLS::mbedtls MbedTLS::mbedcrypto)
endif()

# Testing
enable_testing()
add_subdirectory(tests)

# Installation
install(TARGETS openfido DESTINATION bin)

# Print configuration summary
message(STATUS "")
message(STATUS "OpenFIDO Configuration Summary:")
message(STATUS "  Platform: ${PLATFORM}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "")
