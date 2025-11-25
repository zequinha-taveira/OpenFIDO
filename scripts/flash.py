#!/usr/bin/env python3
"""
OpenFIDO Unified Flasher Script
Supports STM32 (via st-flash/openocd) and nRF52 (via nrfjprog)
"""

import argparse
import subprocess
import sys
import os
import platform

def log(msg):
    print(f"[OpenFIDO Flasher] {msg}")

def error(msg):
    print(f"[ERROR] {msg}", file=sys.stderr)
    sys.exit(1)

def check_tool(tool_name):
    from shutil import which
    if which(tool_name) is None:
        return False
    return True

def flash_stm32(firmware_path, method='st-flash'):
    log(f"Flashing STM32 with {firmware_path} using {method}...")
    
    if method == 'st-flash':
        if not check_tool('st-flash'):
            error("st-flash not found. Please install stlink-tools.")
        
        # Address 0x08000000 is standard for STM32 flash start
        cmd = ['st-flash', 'write', firmware_path, '0x8000000']
        
    elif method == 'openocd':
        if not check_tool('openocd'):
            error("openocd not found.")
            
        # Assuming STM32F4 discovery/generic config
        cmd = [
            'openocd',
            '-f', 'interface/stlink.cfg',
            '-f', 'target/stm32f4x.cfg',
            '-c', f'program {firmware_path} verify reset exit 0x08000000'
        ]
    else:
        error(f"Unknown STM32 flash method: {method}")

    try:
        subprocess.run(cmd, check=True)
        log("Flashing complete!")
    except subprocess.CalledProcessError:
        error("Flashing failed.")

def flash_nrf52(firmware_path):
    log(f"Flashing nRF52 with {firmware_path}...")
    
    if not check_tool('nrfjprog'):
        error("nrfjprog not found. Please install nRF Command Line Tools.")

    try:
        # Erase
        log("Erasing chip...")
        subprocess.run(['nrfjprog', '--eraseall'], check=True)
        
        # Program
        log("Programming...")
        subprocess.run(['nrfjprog', '--program', firmware_path, '--verify'], check=True)
        
        # Reset
        log("Resetting...")
        subprocess.run(['nrfjprog', '--reset'], check=True)
        
        log("Flashing complete!")
    except subprocess.CalledProcessError:
        error("Flashing failed.")

def main():
    parser = argparse.ArgumentParser(description='OpenFIDO Unified Flasher')
    parser.add_argument('--platform', required=True, choices=['stm32', 'nrf52'], help='Target platform')
    parser.add_argument('--firmware', required=True, help='Path to firmware file (.bin for STM32, .hex for nRF52)')
    parser.add_argument('--method', default='st-flash', choices=['st-flash', 'openocd'], help='Flashing method (STM32 only)')
    
    args = parser.parse_args()

    if not os.path.exists(args.firmware):
        error(f"Firmware file not found: {args.firmware}")

    if args.platform == 'stm32':
        flash_stm32(args.firmware, args.method)
    elif args.platform == 'nrf52':
        flash_nrf52(args.firmware)

if __name__ == '__main__':
    main()
