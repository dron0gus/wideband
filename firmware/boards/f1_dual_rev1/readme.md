Firmware for https://github.com/rusefi/rusefi-hardware/tree/main/lambda-x2/export/rev1

Run `./build_wideband_single.sh` for the simplified rev1 hardware with only
left LSU/channel 0, one AUX DAC output on PC6/PC7, and no EGT or AUX inputs.
The retained channel uses the original
rev1 pin assignments. The Bluetooth name is `RusEFI WBO x1`; the TunerStudio
configuration layout and signature remain compatible with the dual board.

The ADC retains the original 10-channel scan, sample times and oversampling,
including unused right-channel and AUX inputs, to preserve sampling timing.
The AUX output uses TIM8 channel 1 on PC6 and the inverted TIM8 channel 2 on PC7
for ripple cancellation. It retains the configurable AUX0 scaling and source
(channel-0 AFR by default). The second AUX output on PC8/PC9 is disabled.

The single-channel board uses its blue PA8 LED for sensor status: fast blinking
during preheat/warmup, slow blinking in closed loop, and counted flashes for
faults. Communication activity does not override this LED. The dual board's
PB13/PC1 LED pins are left as inputs in the single-channel build.

The script also works from the firmware directory:

```sh
./boards/f1_dual_rev1/build_wideband_single.sh
```

Build files are kept in `build/f1_single_rev1`, and deliveries are written to
`deliver/f1_single_rev1`. OpenBLT is included by default, as in the normal rev1
build. Use `USE_OPENBLT=no ./build_wideband_single.sh` for a standalone image,
which uses separate build files in `build/f1_single_rev1_no_bl`.

To flash the **single-channel** version using ST-Link, connect SWDIO, SWCLK,
GND and target voltage reference to a powered board. Install OpenOCD and Python 3,
then run from the firmware directory:

```sh
./flash_stlink.sh f1_single_rev1 --dry-run
./flash_stlink.sh f1_single_rev1
```

The script uses the packaged `deliver/f1_single_rev1/wideband.bin`, which contains
OpenBLT at `0x08000000` and the application at `0x08002000`, including its OpenBLT
checksum. It validates the image, programs and verifies both parts, then resets
the board. It erases only the pages occupied by the image and rejects images
overlapping the settings area at `0x0803C000`.

The first argument selects the delivery board name, as with `flash_can.sh`.
For the dual-channel version, use `./flash_stlink.sh f1_dual_rev1`.
To use another packaged image, pass its path after the board name.
The script does not build firmware; do not pass the application-only
`build/wideband.bin` or `wideband_update.*`. Vector checks validate the combined
image layout, but cannot identify its board variant or firmware version.
