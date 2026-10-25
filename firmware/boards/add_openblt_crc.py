#!/usr/bin/env python3
"""
Add OpenBLT CRC checksum to ELF file
The CRC is the two's complement of the sum of the first 7 vector table entries (0x00-0x18)
and is stored at offset 0x1C in the vector table.
"""

import struct
import sys
import subprocess
import tempfile
import os

def calculate_openblt_crc(binary_data):
    """Calculate OpenBLT CRC from first 28 bytes of vector table"""
    if len(binary_data) < 28:
        raise ValueError("Binary too short for CRC calculation")

    # Sum first 7 vector table entries (each is 4 bytes little-endian)
    checksum = 0
    for i in range(0, 28, 4):
        value = struct.unpack('<I', binary_data[i:i+4])[0]
        checksum = (checksum + value) & 0xFFFFFFFF

    # Two's complement
    checksum = (~checksum + 1) & 0xFFFFFFFF

    return checksum

def add_crc_to_elf(elf_path):
    """Add OpenBLT CRC to ELF file at offset 0x1C"""

    # Extract binary from ELF
    bin_path = elf_path.replace('.elf', '.bin')
    subprocess.run(['arm-none-eabi-objcopy', '-O', 'binary', elf_path, bin_path], check=True)

    # Read binary
    with open(bin_path, 'rb') as f:
        binary_data = bytearray(f.read())

    # Calculate CRC
    crc = calculate_openblt_crc(binary_data)
    print(f"Calculated OpenBLT CRC: 0x{crc:08X}")

    # Write CRC at offset 0x1C in binary
    binary_data[0x1C:0x1C+4] = struct.pack('<I', crc)

    # Extract .vectors section from ELF
    vectors_bin = bin_path.replace('.bin', '_vectors.bin')
    result = subprocess.run(['arm-none-eabi-objcopy', '-O', 'binary', '-j', '.vectors',
                             elf_path, vectors_bin], capture_output=True)

    if result.returncode != 0:
        print("WARNING: Could not extract .vectors section, trying alternative method")
        # Fallback: just update binary/hex, ELF won't have CRC for gdb but hex will work
        with open(bin_path, 'wb') as f:
            f.write(binary_data)

        hex_path = elf_path.replace('.elf', '.hex')
        subprocess.run(['arm-none-eabi-objcopy', '-I', 'binary', '-O', 'ihex',
                        '--change-addresses=0x08002000', bin_path, hex_path], check=True)

        print(f"CRC-signed binary: {bin_path}")
        print(f"CRC-signed hex: {hex_path}")
        print(f"NOTE: ELF file NOT patched. Use 'restore {bin_path} binary 0x08002000' in gdb instead of 'load'")
        return

    # Patch .vectors section with CRC
    with open(vectors_bin, 'rb+') as f:
        vectors_data = bytearray(f.read())
        if len(vectors_data) >= 0x20:
            vectors_data[0x1C:0x20] = struct.pack('<I', crc)
            f.seek(0)
            f.write(vectors_data)

    # Update ELF with patched .vectors section
    subprocess.run(['arm-none-eabi-objcopy', elf_path, elf_path,
                    '--update-section', f'.vectors={vectors_bin}'], check=True)

    # Update binary
    with open(bin_path, 'wb') as f:
        f.write(binary_data)

    # Create hex
    hex_path = elf_path.replace('.elf', '.hex')
    subprocess.run(['arm-none-eabi-objcopy', '-I', 'binary', '-O', 'ihex',
                    '--change-addresses=0x08002000', bin_path, hex_path], check=True)

    # Clean up temp file
    os.remove(vectors_bin)

    print(f"CRC-signed binary: {bin_path}")
    print(f"CRC-signed hex: {hex_path}")
    print(f"CRC-signed ELF: {elf_path}")
    print(f"You can now 'load' in gdb and the CRC will be programmed correctly")

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <elf_file>")
        sys.exit(1)

    add_crc_to_elf(sys.argv[1])
