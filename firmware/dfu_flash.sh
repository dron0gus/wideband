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

# Flash FW and reset on exit
stm32flash -w deliver/$BOARD/wideband.hex -R /dev/ttyUSB0
