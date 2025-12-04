# Implementation Plan

- [x] 1. Fix CMakeLists.txt configuration





  - Remove the non-existent `src/crypto/attestation.c` reference from CRYPTO_SOURCES variable
  - Verify CRYPTO_SOURCES only contains `src/crypto/crypto.c`
  - _Requirements: 1.1, 2.1, 2.3_

- [x] 2. Verify local build succeeds




  - Run CMake configuration: `cmake -DCMAKE_BUILD_TYPE=Release ..`
  - Run make build: `make -j$(nproc)`
  - Confirm build completes with exit code 0
  - Confirm openfido executable is created
  - _Requirements: 1.2, 1.5_

- [x] 2.1 Run existing test suite





  - Execute `ctest --output-on-failure` in build directory
  - Verify all existing tests pass
  - Confirm attestation-related tests work correctly
  - _Requirements: 1.2, 2.2_

- [x] 3. Verify Docker build succeeds











  - Build Docker image: `docker build -t openfido .`
  - Confirm Docker build completes successfully
  - Verify no CMake errors about missing source files
  - Confirm final image contains openfido executable
  - _Requirements: 1.2, 1.5_

- [ ] 4. Create build validation script
  - Write script to parse CMakeLists.txt and extract all source file references
  - Verify each referenced file exists in the filesystem
  - Script should exit with error if any file is missing
  - **Property 1: Source file existence**
  - **Validates: Requirements 1.1**

- [ ] 5. Test error handling for invalid file references
  - Temporarily add a non-existent file to CMakeLists.txt
  - Run CMake configuration and verify it fails
  - Confirm error message clearly identifies the missing file
  - Verify CMake exits with non-zero status code
  - Restore CMakeLists.txt to correct state
  - _Requirements: 1.3, 3.2_
