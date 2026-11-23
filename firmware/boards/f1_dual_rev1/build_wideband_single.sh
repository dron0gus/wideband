#!/bin/bash

set -eo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

USE_OPENBLT=${USE_OPENBLT:-yes}
BUILD_VARIANT=f1_single_rev1
if [ "$USE_OPENBLT" != "yes" ]; then
    BUILD_VARIANT=${BUILD_VARIANT}_no_bl
fi

# Simplified rev1 hardware: left LSU and AUX0 output only, no EGT or AUX inputs.
# Separate objects and deliveries prevent mixing single and dual builds.
BOARD=f1_dual_rev1 \
USE_OPENBLT="$USE_OPENBLT" \
BUILDDIR="build/$BUILD_VARIANT" \
DEPDIR=".dep/$BUILD_VARIANT" \
DELIVER_DIR=deliver/f1_single_rev1 \
UDEFS="${UDEFS:-} -DWIDEBAND_SINGLE -DHAL_USE_SPI=FALSE" \
    ../build_f1_board.sh
