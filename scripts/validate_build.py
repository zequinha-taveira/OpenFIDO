#!/usr/bin/env python3
"""
Build Validation Script for OpenFIDO

This script validates that all source files referenced in CMakeLists.txt
actually exist in the filesystem.

Property 1: Source file existence
For any source file path referenced in CMakeLists.txt, that file SHALL exist 
in the project filesystem.

Validates: Requirements 1.1
"""

import os
import re
import sys
from pathlib import Path


def parse_cmake_sources(cmake_file_path):
    """
    Parse CMakeLists.txt and extract all source file references.
    
    Returns:
        list: List of source file paths referenced in CMakeLists.txt
    """
    source_files = []
    
    with open(cmake_file_path, 'r') as f:
        content = f.read()
    
    # Pattern to match .c and .h files in the CMakeLists.txt
    # This matches paths like src/crypto/crypto.c
    file_pattern = re.compile(r'src/[a-zA-Z0-9_/]+\.[ch]')
    
    # Find all matches
    matches = file_pattern.findall(content)
    
    # Remove duplicates while preserving order
    seen = set()
    for match in matches:
        if match not in seen:
            source_files.append(match)
            seen.add(match)
    
    return source_files


def verify_files_exist(source_files, project_root):
    """
    Verify that each source file exists in the filesystem.
    
    Args:
        source_files: List of source file paths
        project_root: Root directory of the project
        
    Returns:
        tuple: (all_exist: bool, missing_files: list)
    """
    missing_files = []
    
    for file_path in source_files:
        full_path = os.path.join(project_root, file_path)
        if not os.path.isfile(full_path):
            missing_files.append(file_path)
    
    return len(missing_files) == 0, missing_files


def main():
    """Main validation function."""
    # Determine project root (script is in scripts/ directory)
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    
    cmake_file = project_root / "CMakeLists.txt"
    
    # Check if CMakeLists.txt exists
    if not cmake_file.exists():
        print(f"ERROR: CMakeLists.txt not found at {cmake_file}", file=sys.stderr)
        return 1
    
    print("Validating build configuration...")
    print(f"Project root: {project_root}")
    print(f"CMakeLists.txt: {cmake_file}")
    print()
    
    # Parse source files from CMakeLists.txt
    source_files = parse_cmake_sources(cmake_file)
    print(f"Found {len(source_files)} source file references in CMakeLists.txt")
    print()
    
    # Verify all files exist
    all_exist, missing_files = verify_files_exist(source_files, project_root)
    
    if all_exist:
        print("[SUCCESS] All referenced source files exist")
        print()
        print("Validated files:")
        for file_path in sorted(source_files):
            print(f"  [OK] {file_path}")
        return 0
    else:
        print("[FAILURE] Missing source files detected", file=sys.stderr)
        print(file=sys.stderr)
        print("Missing files:", file=sys.stderr)
        for file_path in missing_files:
            print(f"  [MISSING] {file_path}", file=sys.stderr)
        print(file=sys.stderr)
        print(f"Total missing: {len(missing_files)}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
