#!/bin/bash

usage() {
    cat <<EOM
    Usage:
    $(basename $0) Please provide board name

EOM
    exit 0
}

[ -z $1 ] && { usage; }

BOARD=$1

# This script will try to flash/update RusEFI part of firmware over serial (ttyUSB) interface

# BootCommander -t=xcp_rs232 -b=115200 -d=/dev/ttyUSB0 deliver/$BOARD/wideband_update.srec

# OR
# You can build it from sources with:
# (cd ext/openblt/Host/Source/LibOpenBLT/ ; mkdir build ; cd build ; cmake .. ; make -j )
# and
# (cd ext/openblt/Host/Source/BootCommander/ ; mkdir build ; cd build ; cmake .. ; make -j )
# And run:
ext/openblt/Host/BootCommander -t=xcp_rs232 -b=115200 -d=/dev/ttyUSB0 deliver/$BOARD/wideband_update.srec
