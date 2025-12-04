# Design Document

## Overview

The OpenFIDO build system currently references a non-existent source file (`src/crypto/attestation.c`) in the CMakeLists.txt configuration, causing Docker builds to fail. The attestation functionality is actually implemented within existing files (`src/storage/storage.c` and `src/crypto/crypto.c`). This design addresses the build configuration error by removing the invalid reference and ensuring the build system accurately reflects the project structure.

## Architecture

The fix involves a simple modification to the CMakeLists.txt file:

```
Current (Broken):
CMakeLists.txt → References attestation.c → File doesn't exist → Build fails

Fixed:
CMakeLists.txt → References only existing files → Build succeeds
```

The attestation functionality is already properly implemented:
- Attestation key management: `src/storage/storage.c` (storage_get_attestation_key, storage_set_attestation_key)
- Attestation signing: `src/crypto/crypto.c` (crypto_ecdsa_sign)
- Attestation usage: `src/fido2/commands/ctap2_commands.c` and `src/fido2/core/u2f.c`

## Components and Interfaces

### Affected Component: Build Configuration

**File:** `CMakeLists.txt`

**Current State:**
```cmake
set(CRYPTO_SOURCES
    src/crypto/crypto.c
    src/crypto/attestation.c  # ← This file doesn't exist
)
```

**Fixed State:**
```cmake
set(CRYPTO_SOURCES
    src/crypto/crypto.c
)
```

### Unaffected Components

The following components already correctly implement attestation functionality and require no changes:

1. **Storage Module** (`src/storage/storage.c`, `src/storage/storage.h`)
   - Provides `storage_get_attestation_key()` and `storage_set_attestation_key()`
   - Manages attestation key persistence

2. **Crypto Module** (`src/crypto/crypto.c`, `src/crypto/crypto.h`)
   - Provides `crypto_ecdsa_sign()` for attestation signatures
   - Implements all required cryptographic operations

3. **FIDO2 Commands** (`src/fido2/commands/ctap2_commands.c`)
   - Uses attestation key for MakeCredential responses

4. **U2F Implementation** (`src/fido2/core/u2f.c`)
   - Uses attestation key for U2F registration

## Data Models

No data model changes are required. The existing attestation data structures remain unchanged:

- Attestation key: 32-byte ECDSA P-256 private key (stored in `storage_state.attestation_key`)
- Attestation certificate: X.509 certificate (storage interface defined, implementation pending)

## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system-essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Acceptence Criteria Testing Prework:

1.1 WHEN CMake processes the CMakeLists.txt file THEN the Build System SHALL only reference source files that exist in the project
Thoughts: This is a property that should hold for all source files referenced in the build configuration. We can verify this by checking that every file path in the CMakeLists.txt corresponds to an actual file in the filesystem.
Testable: yes - property

1.2 WHEN the Docker build executes the make command THEN the Build System SHALL compile all required source files without errors
Thoughts: This is testing that the build completes successfully. This is a specific example - we run the build and check the exit code.
Testable: yes - example

1.3 WHEN a non-existent source file is referenced THEN the Build System SHALL fail with a clear error message during configuration
Thoughts: This is testing error handling behavior. We can test this by intentionally adding a non-existent file reference and verifying CMake fails appropriately.
Testable: yes - example

1.4 THE Build System SHALL validate all source file paths before attempting compilation
Thoughts: This is about CMake's built-in behavior during configuration. CMake automatically validates file paths when processing add_executable(). This is inherent to CMake's design.
Testable: no

1.5 WHEN the build completes successfully THEN the Docker image SHALL contain a functional OpenFIDO executable
Thoughts: This is testing that the final artifact exists and is executable. This is a specific example test.
Testable: yes - example

2.1 WHEN reviewing the CRYPTO_SOURCES variable THEN the CMakeLists.txt SHALL list only existing crypto implementation files
Thoughts: This is the same as 1.1 but scoped to CRYPTO_SOURCES. It's redundant with the broader property.
Testable: redundant with 1.1

2.2 WHEN attestation functionality is needed THEN the Build System SHALL use the existing implementations in storage.c and crypto.c
Thoughts: This is about ensuring the correct files are linked. If the build succeeds and tests pass, this is validated. This is covered by 1.2.
Testable: redundant with 1.2

2.3 THE CMakeLists.txt SHALL maintain consistency between declared source files and actual filesystem contents
Thoughts: This is the same as property 1.1, just stated differently.
Testable: redundant with 1.1

2.4 WHEN source files are added or removed THEN the Build System SHALL be updated to reflect these changes
Thoughts: This is a process requirement about maintaining the build system, not a testable property of the current system.
Testable: no

3.1 WHEN a build error occurs THEN the Build System SHALL output diagnostic information identifying the specific problem
Thoughts: This is about CMake's error reporting behavior, which is built into CMake itself.
Testable: no

3.2 WHEN CMake configuration fails THEN the Build System SHALL exit with a non-zero status code
Thoughts: This is CMake's standard behavior. We can verify this with an example test.
Testable: yes - example

3.3 THE Build System SHALL validate all dependencies before starting compilation
Thoughts: This is CMake's built-in behavior during the configuration phase.
Testable: no

3.4 WHEN the build succeeds THEN the Build System SHALL confirm successful compilation of all components
Thoughts: This is covered by 1.2 - successful build completion.
Testable: redundant with 1.2

### Property Reflection:

After reviewing all testable criteria:
- Property 1.1 covers all file path validation (2.1 and 2.3 are redundant)
- Example 1.2 covers successful build (2.2 and 3.4 are redundant)
- Example 1.3 covers error handling for missing files
- Example 1.5 covers executable artifact creation
- Example 3.2 covers CMake error exit codes

The remaining unique testable items are:
- Property 1.1: All referenced source files exist
- Example 1.2: Build completes successfully
- Example 1.3: Missing file causes clear error
- Example 1.5: Executable artifact is created
- Example 3.2: CMake fails with non-zero exit on error

### Correctness Properties:

Property 1: Source file existence
*For any* source file path referenced in CMakeLists.txt, that file SHALL exist in the project filesystem
**Validates: Requirements 1.1, 2.1, 2.3**

## Error Handling

### Build-Time Errors

1. **Missing Source File**
   - Detection: CMake configuration phase
   - Error: `Cannot find source file: src/crypto/attestation.c`
   - Resolution: Remove invalid reference from CRYPTO_SOURCES

2. **Missing Dependencies**
   - Detection: CMake configuration phase (find_package)
   - Error: `Could not find MbedTLS`
   - Resolution: Ensure mbedtls-dev is installed (already in Dockerfile)

3. **Compilation Errors**
   - Detection: Make build phase
   - Error: Compiler errors with file/line information
   - Resolution: Fix source code issues (not applicable to this fix)

### Runtime Errors

No runtime behavior changes are introduced by this fix. Attestation functionality continues to work as before using the existing implementations.

## Testing Strategy

### Unit Testing

Since this is a build configuration fix, traditional unit tests are not applicable. However, we will verify the fix through build validation:

1. **Successful Build Test**
   - Build the project using Docker
   - Verify exit code is 0
   - Verify executable is created
   - **Validates: Requirements 1.2, 1.5**

2. **CMake Configuration Test**
   - Run CMake configuration
   - Verify no errors about missing source files
   - Verify all source files in CRYPTO_SOURCES exist
   - **Validates: Requirements 1.1**

3. **Negative Test: Invalid File Reference**
   - Temporarily add a non-existent file to CMakeLists.txt
   - Verify CMake fails with clear error message
   - Verify non-zero exit code
   - **Validates: Requirements 1.3, 3.2**

### Property-Based Testing

Property-based testing is not applicable for build configuration validation. The correctness property (all referenced files exist) will be validated through:

1. **Static Analysis**: Script to parse CMakeLists.txt and verify all source file paths exist
2. **Build Verification**: Successful Docker build confirms all references are valid

### Integration Testing

After the fix:
1. Run existing test suite (`cd build && ctest`)
2. Verify all tests pass
3. Verify attestation functionality works correctly in FIDO2 operations

### Testing Framework

- **Build Testing**: Docker build process, CMake, Make
- **Static Analysis**: Custom shell script or Python script to validate file references
- **Functional Testing**: Existing CTest suite (test_crypto.c, test_ctap2.c, test_u2f.c)

## Implementation Notes

### Files to Modify

1. **CMakeLists.txt** (line ~30)
   - Remove `src/crypto/attestation.c` from CRYPTO_SOURCES

### Files NOT to Modify

- All source files remain unchanged
- Dockerfile remains unchanged (build process is correct)
- Test files remain unchanged
- All other build configuration remains unchanged

### Verification Steps

1. Remove the invalid reference
2. Build locally: `mkdir -p build && cd build && cmake .. && make`
3. Run tests: `cd build && ctest`
4. Build Docker image: `docker build -t openfido .`
5. Verify Docker build succeeds

### Backward Compatibility

This fix has no impact on backward compatibility:
- No API changes
- No behavior changes
- No data format changes
- Existing functionality remains identical

The fix simply corrects a build configuration error that prevented compilation.
