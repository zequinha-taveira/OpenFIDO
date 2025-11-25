set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Check for NRF5_SDK_ROOT environment variable
if(NOT DEFINED ENV{NRF5_SDK_ROOT})
    message(FATAL_ERROR "NRF5_SDK_ROOT environment variable not set. Please set it to your nRF5 SDK installation path.")
endif()

set(NRF5_SDK_ROOT $ENV{NRF5_SDK_ROOT})

# Toolchain settings
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)

# Compiler flags
set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(COMMON_FLAGS "${CPU_FLAGS} -ffunction-sections -fdata-sections -fno-strict-aliasing -fno-builtin --short-enums")

set(CMAKE_C_FLAGS "${COMMON_FLAGS} -std=c99" CACHE INTERNAL "C Compiler options")
set(CMAKE_CXX_FLAGS "${COMMON_FLAGS} -std=c++11" CACHE INTERNAL "C++ Compiler options")
set(CMAKE_ASM_FLAGS "${COMMON_FLAGS} -x assembler-with-cpp" CACHE INTERNAL "ASM Compiler options")
set(CMAKE_EXE_LINKER_FLAGS "${CPU_FLAGS} -Wl,--gc-sections --specs=nano.specs" CACHE INTERNAL "Linker options")

# Search paths
set(CMAKE_FIND_ROOT_PATH ${NRF5_SDK_ROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Definitions
add_definitions(
    -DNRF52
    -DNRF52840_XXAA
    -DBOARD_PCA10056
    -DBSP_DEFINES_ONLY
    -DCONFIG_GPIO_AS_PINRESET
    -DFLOAT_ABI_HARD
)

# Include directories from SDK
include_directories(
    ${NRF5_SDK_ROOT}/components
    ${NRF5_SDK_ROOT}/components/boards
    ${NRF5_SDK_ROOT}/components/drivers_nrf/nrf_soc_nosd
    ${NRF5_SDK_ROOT}/components/libraries/atomic
    ${NRF5_SDK_ROOT}/components/libraries/balloc
    ${NRF5_SDK_ROOT}/components/libraries/bsp
    ${NRF5_SDK_ROOT}/components/libraries/delay
    ${NRF5_SDK_ROOT}/components/libraries/experimental_section_vars
    ${NRF5_SDK_ROOT}/components/libraries/log
    ${NRF5_SDK_ROOT}/components/libraries/log/src
    ${NRF5_SDK_ROOT}/components/libraries/memobj
    ${NRF5_SDK_ROOT}/components/libraries/ringbuf
    ${NRF5_SDK_ROOT}/components/libraries/strerror
    ${NRF5_SDK_ROOT}/components/libraries/timer
    ${NRF5_SDK_ROOT}/components/libraries/util
    ${NRF5_SDK_ROOT}/components/toolchain/cmsis/include
    ${NRF5_SDK_ROOT}/integration/nrfx
    ${NRF5_SDK_ROOT}/integration/nrfx/legacy
    ${NRF5_SDK_ROOT}/modules/nrfx
    ${NRF5_SDK_ROOT}/modules/nrfx/drivers/include
    ${NRF5_SDK_ROOT}/modules/nrfx/hal
    ${NRF5_SDK_ROOT}/modules/nrfx/mdk
)
