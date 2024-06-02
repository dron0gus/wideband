# Wideband

This fork is for development of FW for [RusEFI](https://github.com/rusefi) [dual channel wideband](https://github.com/rusefi/rusefi-hardware/tree/main/lambda-x2). Based on [initial desing](https://github.com/mck1117/wideband) from [mck1117](https://github.com/mck1117)

For stm32f042 FW please refer to original repo. While I try to keep it buildable and unaffected by my modification I can not quaranty its functionality and provide any support.

[OpenBLT](https://github.com/feaser/openblt) bootloader is used for FW update functionality. Please reffer to original [documentation](https://www.feaser.com/openblt/doku.php?id=faq) on how to compile host tools and use it.

# Original readme

[![Build Firmware](https://github.com/mck1117/wideband/actions/workflows/build-firmware.yaml/badge.svg)](https://github.com/mck1117/wideband/actions/workflows/build-firmware.yaml) ![license](https://img.shields.io/github/license/mck1117/wideband)

# rusEFI Wideband Controller

[User Documentation](https://rusefi.com/s/wb)

[Forum Thread](https://rusefi.com/forum/viewtopic.php?f=4&t=1856)

## Building Firmware

The `firmware/boards` directory contains configuration for each board this firmware supports.

For the standalone board and the module built in to rusEFI Hellen boards, `f0_module` is the correct target.  Use the `build_wideband.sh` script to build and package the wideband firmware: both a bin including the bootloader, and a header file consumed by rusEFI that contains no bootloader, to be uploaded over CAN (via the aforementioned bootloader).
