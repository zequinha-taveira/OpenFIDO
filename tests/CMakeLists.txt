cmake_minimum_required(VERSION 3.16)

project(OpenFIDO_Tests)

# Enable testing
enable_testing()

# Include directories
include_directories(
    ../src/fido2
    ../src/crypto
    ../src/storage
    ../src/hal
    ../src/usb
    ../src/utils
)

# Test framework (using simple assert-based tests)
set(TEST_SOURCES
    test_cbor.c
    test_crypto.c
    test_storage.c
    test_ctap2.c
    test_buffer.c
)

# Mock HAL for testing
set(MOCK_HAL_SOURCES
    mock_hal.c
)

# Source files to test
set(SRC_FILES
    ../src/fido2/cbor.c
    ../src/crypto/crypto.c
    ../src/storage/storage.c
    ../src/fido2/ctap2.c
    ../src/utils/buffer.c
)

# Create test executable
add_executable(run_tests
    ${TEST_SOURCES}
    ${MOCK_HAL_SOURCES}
    ${SRC_FILES}
)

# Link mbedTLS
find_package(MbedTLS REQUIRED)
target_link_libraries(run_tests MbedTLS::mbedtls MbedTLS::mbedcrypto)

# Add tests
add_test(NAME cbor_tests COMMAND run_tests cbor)
add_test(NAME crypto_tests COMMAND run_tests crypto)
add_test(NAME storage_tests COMMAND run_tests storage)
add_test(NAME ctap2_tests COMMAND run_tests ctap2)
add_test(NAME buffer_tests COMMAND run_tests buffer)

# Coverage (optional)
option(ENABLE_COVERAGE "Enable code coverage" OFF)
if(ENABLE_COVERAGE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()
