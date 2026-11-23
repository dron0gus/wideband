#!/usr/bin/env bash
# Flash a packaged board image (bootloader + application) over ST-Link.
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
board=""
image=""
dry_run=false
image_set=false

usage() {
    cat <<'EOF'
Usage: flash_stlink.sh <board> [--dry-run] [combined-firmware.bin]

Flash and verify both bootloader and application, then restart the board.
Requires OpenOCD, Python 3 and an ST-Link connected over SWD to a powered board.

Boards: f0_module, f1_rev2, f1_rev3, f1_dual, f1_dual_rev1, f1_single_rev1.
Default image: deliver/<board>/wideband.bin relative to this script.
For f0_module: deliver/f0_module/wideband_image_with_bl.bin.
Use a packaged combined .bin, not build/wideband.bin or wideband_update.*.
This script does not build firmware. --dry-run validates without connecting.
Set OPENOCD to override the OpenOCD executable (default: openocd).

Example: ./flash_stlink.sh f1_single_rev1
EOF
}

for arg in "$@"; do
    case "$arg" in
        --dry-run) dry_run=true ;;
        -h|--help) usage; exit 0 ;;
        -*) echo "Unknown option: $arg" >&2; usage >&2; exit 2 ;;
        *)
            if [[ -z "$board" ]]; then
                board=$arg
                continue
            fi
            if "$image_set"; then
                echo "Only one image may be specified." >&2
                exit 2
            fi
            image=$arg
            image_set=true
            ;;
    esac
done

if [[ -z "$board" ]]; then
    echo "Please provide board name." >&2
    usage >&2
    exit 2
fi

target=target/stm32f1x.cfg
image_name=wideband.bin
case "$board" in
    f1_rev2|f1_rev3|f1_dual|f1_dual_rev1|f1_single_rev1) ;;
    f0_module)
        target=target/stm32f0x.cfg
        image_name=wideband_image_with_bl.bin
        ;;
    *) echo "Unsupported board: $board" >&2; usage >&2; exit 2 ;;
esac
if ! "$image_set"; then
    image="$script_dir/deliver/$board/$image_name"
fi

if [[ ! -f "$image" || ! -r "$image" ]]; then
    echo "Combined firmware image is missing or unreadable: $image" >&2
    exit 1
fi

# Snapshot the image so that validation and programming use identical bytes.
# The temporary filename is also safe for OpenOCD's Tcl program helper.
work_dir=$(mktemp -d /tmp/wideband-stlink.XXXXXXXX)
trap 'rm -rf -- "$work_dir"' EXIT
cp -- "$image" "$work_dir/wideband.bin"

python3 - "$work_dir/wideband.bin" "$board" <<'PY'
import pathlib
import struct
import sys
import zlib

data = pathlib.Path(sys.argv[1]).read_bytes()
board = sys.argv[2]
flash = 0x08000000
# Application offset, first reserved settings address, physical RAM size.
# F1 rev3 retains the 128 KiB linker layout on an STM32F103xE.
profiles = {
    "f0_module": (0x1800, 0x7C00, 0x1800),
    "f1_rev2": (0x2000, 0x1E000, 0x5000),
    "f1_rev3": (0x2000, 0x1E000, 0xC000),
    "f1_dual": (0x2000, 0x1E000, 0x5000),
    # f1_common/f1_port.cpp reserves pages 120..127 (2 KiB each).
    "f1_dual_rev1": (0x2000, 0x3C000, 0xC000),
    "f1_single_rev1": (0x2000, 0x3C000, 0xC000),
}
app_offset, settings_offset, ram_size = profiles[board]

def require(condition, message):
    if not condition:
        sys.exit("Invalid combined image: " + message)

require(app_offset + 32 <= len(data) <= settings_offset,
        f"size must include both vector tables and stay below settings at {flash + settings_offset:#010x}")
for name, offset, end in (("bootloader", 0, app_offset),
                          ("application", app_offset, len(data))):
    sp, reset = struct.unpack_from("<II", data, offset)
    require(0x20000000 < sp <= 0x20000000 + ram_size and sp % 4 == 0,
            name + " stack pointer is outside board RAM")
    require(reset & 1 and flash + offset <= (reset & ~1) < flash + end,
            name + " reset vector is outside its flash region")
if board == "f0_module":
    require(len(data) == settings_offset, "F0 image must include application padding and CRC")
    require(zlib.crc32(data[app_offset:-4]) == int.from_bytes(data[-4:], "big"),
            "application CRC is missing or incorrect")
else:
    require(sum(struct.unpack_from("<8I", data, app_offset)) & 0xFFFFFFFF == 0,
            "application OpenBLT vector checksum is missing or incorrect")
print(f"Validated combined image: {len(data)} bytes")
print(f"Bootloader: {flash:#010x}; application: {flash + app_offset:#010x}")
PY

echo "Board: $board"
echo "Image: $image"
if "$dry_run"; then
    echo "Dry run: validation passed; no hardware accessed."
    exit 0
fi

"${OPENOCD:-openocd}" \
    -f interface/stlink.cfg \
    -f "$target" \
    -c 'adapter speed 1000' \
    -c "program {$work_dir/wideband.bin} 0x08000000 verify reset exit"
