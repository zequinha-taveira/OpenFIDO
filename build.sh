#!/bin/bash
#
# Build script for OpenFIDO FIDO2 Security Key
#
# This script builds OpenFIDO for different platforms and configurations.
# Supports ESP32, STM32, nRF52, and native builds with testing.
#

set -e

# Default configuration
PLATFORM="NATIVE"
BUILD_TYPE="Debug"
CLEAN=false
ENABLE_BLE=false
INSTALL_DEPS=false
VERBOSE=false
BUILD_DIR="build"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Print functions
print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_info() {
    echo -e "${CYAN}ℹ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_header() {
    echo ""
    echo -e "${BLUE}═══════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE} $1${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════════${NC}"
    echo ""
}

# Show usage
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Build OpenFIDO for different platforms and configurations.

OPTIONS:
    -p, --platform PLATFORM    Target platform: ESP32, STM32, NRF52, NATIVE (default: NATIVE)
    -b, --build-type TYPE      Build type: Debug, Release (default: Debug)
    -c, --clean                Clean build directory before building
    -e, --enable-ble           Enable BLE transport support
    -i, --install-deps         Install required dependencies
    -v, --verbose              Enable verbose build output
    -h, --help                 Show this help message

EXAMPLES:
    $0                                    # Build for native platform in Debug mode
    $0 -p NRF52 -b Release -e            # Build for nRF52 with BLE in Release mode
    $0 -i                                # Install dependencies and build

EOF
    exit 0
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--platform)
            PLATFORM="$2"
            shift 2
            ;;
        -b|--build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -e|--enable-ble)
            ENABLE_BLE=true
            shift
            ;;
        -i|--install-deps)
            INSTALL_DEPS=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            print_error "Unknown option: $1"
            usage
            ;;
    esac
done

# Validate platform
case $PLATFORM in
    ESP32|STM32|NRF52|NATIVE)
        ;;
    *)
        print_error "Invalid platform: $PLATFORM"
        usage
        ;;
esac

# Auto-enable BLE for nRF52
if [ "$PLATFORM" = "NRF52" ]; then
    ENABLE_BLE=true
fi

# Check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Install dependencies
install_dependencies() {
    print_header "Installing Dependencies"

    # Detect OS
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        # Linux
        if command_exists apt-get; then
            print_info "Installing dependencies via apt..."
            sudo apt-get update
            sudo apt-get install -y cmake build-essential libmbedtls-dev git
            print_success "Dependencies installed"
        elif command_exists yum; then
            print_info "Installing dependencies via yum..."
            sudo yum install -y cmake gcc gcc-c++ mbedtls-devel git
            print_success "Dependencies installed"
        else
            print_error "Unsupported package manager"
            exit 1
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        if command_exists brew; then
            print_info "Installing dependencies via Homebrew..."
            brew install cmake mbedtls
            print_success "Dependencies installed"
        else
            print_error "Homebrew not found. Install from: https://brew.sh/"
            exit 1
        fi
    else
        print_error "Unsupported OS: $OSTYPE"
        exit 1
    fi
}

# Clean build directories
clean_build() {
    print_header "Cleaning Build Directory"

    if [ -d "$BUILD_DIR" ]; then
        print_info "Removing $BUILD_DIR..."
        rm -rf "$BUILD_DIR"
        print_success "Removed $BUILD_DIR"
    fi

    print_success "Clean complete"
}

# Check prerequisites
check_prerequisites() {
    print_header "Checking Prerequisites"

    local all_good=true

    # Check CMake
    if command_exists cmake; then
        cmake_version=$(cmake --version | head -n1)
        print_success "CMake: $cmake_version"
    else
        print_error "CMake not found"
        all_good=false
    fi

    # Check compiler based on platform
    if [ "$PLATFORM" = "NATIVE" ]; then
        if command_exists gcc; then
            gcc_version=$(gcc --version | head -n1)
            print_success "GCC: $gcc_version"
        else
            print_error "GCC not found"
            all_good=false
        fi
    fi

    # Check platform-specific tools
    case $PLATFORM in
        ESP32)
            if command_exists idf.py; then
                print_success "ESP-IDF found"
            else
                print_error "ESP-IDF not found. Install from: https://docs.espressif.com/projects/esp-idf/"
                all_good=false
            fi
            ;;
        NRF52)
            if command_exists nrfjprog; then
                print_success "nRF Command Line Tools found"
            else
                print_warning "nrfjprog not found (optional for flashing)"
            fi
            ;;
    esac

    if [ "$all_good" = false ]; then
        print_error "Prerequisites check failed. Use -i to install missing dependencies."
        exit 1
    fi

    print_success "All prerequisites satisfied"
}

# Build for native platform
build_native() {
    print_header "Building for Native Platform ($BUILD_TYPE)"

    # Create build directory
    mkdir -p "$BUILD_DIR"

    # Configure CMake
    print_info "Configuring CMake..."
    cd "$BUILD_DIR"

    cmake_args=(
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
        -DPLATFORM=NATIVE
    )

    if [ "$ENABLE_BLE" = true ]; then
        cmake_args+=(-DENABLE_BLE=ON)
    fi

    if [ "$VERBOSE" = true ]; then
        cmake_args+=(-DCMAKE_VERBOSE_MAKEFILE=ON)
    fi

    cmake "${cmake_args[@]}" ..
    print_success "CMake configuration complete"

    # Build
    print_info "Building..."
    make_args=()
    if [ "$VERBOSE" = true ]; then
        make_args+=(VERBOSE=1)
    fi

    make -j$(nproc) "${make_args[@]}"
    print_success "Build complete"

    # Show binary info
    if [ -f "openfido" ]; then
        size=$(stat -f%z "openfido" 2>/dev/null || stat -c%s "openfido")
        size_kb=$((size / 1024))
        print_info "Binary size: ${size_kb} KB"
    fi

    cd ..
}

# Build for ESP32
build_esp32() {
    print_header "Building for ESP32"

    if ! command_exists idf.py; then
        print_error "ESP-IDF not found. Please install ESP-IDF first."
        exit 1
    fi

    print_info "Setting target to ESP32-S3..."
    idf.py set-target esp32s3

    print_info "Building firmware..."
    idf.py build

    print_success "ESP32 build complete"
    print_info "Flash with: idf.py -p /dev/ttyUSB0 flash monitor"
}

# Build for STM32
build_stm32() {
    print_header "Building for STM32"

    print_warning "STM32 build requires ARM GCC toolchain"
    print_info "Please ensure arm-none-eabi-gcc is in your PATH"

    # Create build directory
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    cmake -DPLATFORM=STM32 -DCMAKE_BUILD_TYPE="$BUILD_TYPE" ..
    make -j$(nproc)

    print_success "STM32 build complete"
    cd ..
}

# Build for nRF52
build_nrf52() {
    print_header "Building for nRF52"

    print_info "nRF52 build with BLE support enabled"

    # Create build directory
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    cmake -DPLATFORM=NRF52 -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DENABLE_BLE=ON ..
    make -j$(nproc)

    print_success "nRF52 build complete"
    print_info "Flash with: nrfjprog --program openfido.hex --chiperase"
    cd ..
}

# Main execution
main() {
    print_header "OpenFIDO Build Script"
    print_info "Platform: $PLATFORM"
    print_info "Build Type: $BUILD_TYPE"
    if [ "$ENABLE_BLE" = true ]; then
        print_info "BLE Support: Enabled"
    else
        print_info "BLE Support: Disabled"
    fi

    # Install dependencies if requested
    if [ "$INSTALL_DEPS" = true ]; then
        install_dependencies
    fi

    # Clean if requested
    if [ "$CLEAN" = true ]; then
        clean_build
    fi

    # Check prerequisites
    check_prerequisites

    # Build based on platform
    case $PLATFORM in
        NATIVE)
            build_native
            ;;
        ESP32)
            build_esp32
            ;;
        STM32)
            build_stm32
            ;;
        NRF52)
            build_nrf52
            ;;
    esac

    print_header "Build Complete"
    print_success "Build finished successfully!"
}

# Run main function
main
