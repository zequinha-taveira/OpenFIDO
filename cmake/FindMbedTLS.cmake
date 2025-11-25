# FindMbedTLS.cmake
# Find MbedTLS library
#
# This module defines:
#  MbedTLS_FOUND - System has MbedTLS
#  MbedTLS_INCLUDE_DIRS - MbedTLS include directories
#  MbedTLS_LIBRARIES - Libraries needed to use MbedTLS
#  MbedTLS::mbedtls - Imported target for mbedtls
#  MbedTLS::mbedcrypto - Imported target for mbedcrypto
#  MbedTLS::mbedx509 - Imported target for mbedx509

# Find the include directory
find_path(MbedTLS_INCLUDE_DIR
    NAMES mbedtls/ssl.h
    PATHS
        /usr/include
        /usr/local/include
        /opt/local/include
    PATH_SUFFIXES mbedtls
)

# Find the libraries
find_library(MbedTLS_LIBRARY
    NAMES mbedtls
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/local/lib
    PATH_SUFFIXES x86_64-linux-gnu aarch64-linux-gnu arm-linux-gnueabihf
)

find_library(MbedCrypto_LIBRARY
    NAMES mbedcrypto
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/local/lib
    PATH_SUFFIXES x86_64-linux-gnu aarch64-linux-gnu arm-linux-gnueabihf
)

find_library(MbedX509_LIBRARY
    NAMES mbedx509
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/local/lib
    PATH_SUFFIXES x86_64-linux-gnu aarch64-linux-gnu arm-linux-gnueabihf
)

# Handle standard arguments
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MbedTLS
    REQUIRED_VARS
        MbedTLS_LIBRARY
        MbedCrypto_LIBRARY
        MbedX509_LIBRARY
        MbedTLS_INCLUDE_DIR
    FAIL_MESSAGE "Could not find MbedTLS library"
)

if(MbedTLS_FOUND)
    set(MbedTLS_INCLUDE_DIRS ${MbedTLS_INCLUDE_DIR})
    set(MbedTLS_LIBRARIES ${MbedTLS_LIBRARY} ${MbedCrypto_LIBRARY} ${MbedX509_LIBRARY})

    # Create imported targets
    if(NOT TARGET MbedTLS::mbedtls)
        add_library(MbedTLS::mbedtls UNKNOWN IMPORTED)
        set_target_properties(MbedTLS::mbedtls PROPERTIES
            IMPORTED_LOCATION "${MbedTLS_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${MbedTLS_INCLUDE_DIR}"
        )
    endif()

    if(NOT TARGET MbedTLS::mbedcrypto)
        add_library(MbedTLS::mbedcrypto UNKNOWN IMPORTED)
        set_target_properties(MbedTLS::mbedcrypto PROPERTIES
            IMPORTED_LOCATION "${MbedCrypto_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${MbedTLS_INCLUDE_DIR}"
        )
    endif()

    if(NOT TARGET MbedTLS::mbedx509)
        add_library(MbedTLS::mbedx509 UNKNOWN IMPORTED)
        set_target_properties(MbedTLS::mbedx509 PROPERTIES
            IMPORTED_LOCATION "${MbedX509_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${MbedTLS_INCLUDE_DIR}"
        )
    endif()
endif()

# Mark variables as advanced
mark_as_advanced(
    MbedTLS_INCLUDE_DIR
    MbedTLS_LIBRARY
    MbedCrypto_LIBRARY
    MbedX509_LIBRARY
)
