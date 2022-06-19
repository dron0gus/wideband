#!/bin/bash


#BOARD=f1_dual USE_OPENBLT=yes ../common_make.sh
# set optimization level to 0 until ADC issue is fixed for GD32
BOARD=f1_dual \
USE_OPT="-O0 -ggdb -fomit-frame-pointer -falign-functions=16 -fsingle-precision-constant" \../common_make.sh
