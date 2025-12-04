#!/usr/bin/env python3
"""
Property-Based Test for Build Validation

Feature: fix-build-system, Property 1: Source file existence
For any source file path referenced in CMakeLists.txt, that file SHALL exist 
in the project filesystem.

Validates: Requirements 1.1
"""

import subprocess
import sys
import tempfile
import shutil
from pathlib import Path


def test_property_all_referenced_files_exist():
    """
    Property 1: Source file existence
    
    Test that all source files referenced in the actual CMakeLists.txt exist.
    This is the core property that must hold for the build system to work.
    """
    # Run the validation script on the actual project
    result = subprocess.run(
        [sys.executable, "scripts/validate_build.py"],
        capture_output=True,
        text=True
    )
    
    print("Property Test: All referenced source files exist")
    print(f"Exit code: {result.returncode}")
    
    if result.returncode == 0:
        print("✓ PASS: All source files referenced in CMakeLists.txt exist")
        print(result.stdout)
        return True
    else:
        print("✗ FAIL: Some source files are missing")
        print(result.stderr)
        return False


def test_error_detection_missing_file():
    """
    Example test: Verify the script detects missing files.
    
    This tests that when a non-existent file is referenced,
    the validation script properly fails.
    """
    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        
        # Create a CMakeLists.txt with a missing file reference
        cmake_content = """
set(TEST_SOURCES
    src/real_file.c
    src/nonexistent_file.c
)
"""
        cmake_file = tmpdir_path / "CMakeLists.txt"
        cmake_file.write_text(cmake_content)
        
        # Create only one of the files
        src_dir = tmpdir_path / "src"
        src_dir.mkdir()
        (src_dir / "real_file.c").write_text("// real file")
        
        # Copy validation script
        scripts_dir = tmpdir_path / "scripts"
        scripts_dir.mkdir()
        shutil.copy("scripts/validate_build.py", scripts_dir / "validate_build.py")
        
        # Run validation
        result = subprocess.run(
            [sys.executable, str(scripts_dir / "validate_build.py")],
            cwd=tmpdir,
            capture_output=True,
            text=True
        )
        
        print("\nExample Test: Missing file detection")
        print(f"Exit code: {result.returncode}")
        
        # Should fail with exit code 1
        if result.returncode == 1 and "nonexistent_file.c" in result.stderr:
            print("✓ PASS: Script correctly detected missing file")
            return True
        else:
            print("✗ FAIL: Script did not detect missing file")
            print(result.stderr)
            return False


def test_error_detection_all_files_exist():
    """
    Example test: Verify the script succeeds when all files exist.
    """
    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        
        # Create a CMakeLists.txt with valid file references
        cmake_content = """
set(TEST_SOURCES
    src/file1.c
    src/file2.c
)
"""
        cmake_file = tmpdir_path / "CMakeLists.txt"
        cmake_file.write_text(cmake_content)
        
        # Create all referenced files
        src_dir = tmpdir_path / "src"
        src_dir.mkdir()
        (src_dir / "file1.c").write_text("// file 1")
        (src_dir / "file2.c").write_text("// file 2")
        
        # Copy validation script
        scripts_dir = tmpdir_path / "scripts"
        scripts_dir.mkdir()
        shutil.copy("scripts/validate_build.py", scripts_dir / "validate_build.py")
        
        # Run validation
        result = subprocess.run(
            [sys.executable, str(scripts_dir / "validate_build.py")],
            cwd=tmpdir,
            capture_output=True,
            text=True
        )
        
        print("\nExample Test: All files exist")
        print(f"Exit code: {result.returncode}")
        
        # Should succeed with exit code 0
        if result.returncode == 0:
            print("✓ PASS: Script correctly validated all files exist")
            return True
        else:
            print("✗ FAIL: Script failed when all files exist")
            print(result.stderr)
            return False


def main():
    """Run all tests"""
    print("=" * 70)
    print("Build Validation Property-Based Tests")
    print("=" * 70)
    print()
    
    results = []
    
    # Run property test
    results.append(("Property 1: Source file existence", 
                   test_property_all_referenced_files_exist()))
    
    # Run example tests
    results.append(("Example: Missing file detection", 
                   test_error_detection_missing_file()))
    results.append(("Example: All files exist", 
                   test_error_detection_all_files_exist()))
    
    # Summary
    print()
    print("=" * 70)
    print("Test Summary")
    print("=" * 70)
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for name, result in results:
        status = "✓ PASS" if result else "✗ FAIL"
        print(f"{status}: {name}")
    
    print()
    print(f"Total: {passed}/{total} tests passed")
    
    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())
