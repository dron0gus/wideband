#!/bin/bash

DEV=/dev/ttyUSB0

#115200
stty -F $DEV speed 115200 cs8 -cstopb -parenb > /dev/null
#Send cmd_dfu
printf '%b' '\x00\x05\x5A\x00\xBA\x00\x00\x7C\x48\x5B\xB1' > $DEV
