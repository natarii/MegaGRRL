MegaGRRL - Real Hardware Sega Genesis VGM Player
========
**MegaGRRL** is an ESP32-powered VGM player, playing tracks from the Sega Mega Drive/Genesis, Master System, and more on real YM2612 and SN76489 sound chips.

## Overview
MegaGRRL is a system comprised of ESP32-powered hardware to interface with original YM2612 and SN76489 sound chips, and FreeRTOS-based firmware using the ESP-IDF framework. Its playback engine is focused on high accuracy and supporting as many features of the VGM file format as possible.

Originally, the MegaGRRL project started as only a portable, handheld player. This was expanded later on to include a desktop hardware version. The firmware in this repository works on both hardware versions with minimal changes. However, the PCB and enclosure files in this repo are for the portable version only, and the files for the desktop version should be obtained from the [MegaGRRL Desktop repository](https://git.agiri.ninja/natalie/MegaGRRL_Desktop).

This project is divided into several subdirectories:
  * **esp-idf-patches/** - ESP-IDF v3.3.4 SPI master driver patches, required for build
  * **firmware/** - Firmware for the ESP32
  * **utils/** - Utilities, such as for packing firmware updates

## Updating firmware
To update to the latest firmware after your player is up and running, download the latest .mgu file for your hardware version (desktop or portable) from the [Releases page](https://git.agiri.ninja/natalie/megagrrl/-/releases) and copy it to the SD card. Launch the update from MegaGRRL's file browser.

A script is included at `utils/megapacker.pl` to generate .mgu files from self-built .bin files. The simplest case would be to pack a new app image (i.e. built without FWUPDATE defined):

   - `perl utils/megapacker.pl --app=firmware/build/megagrrl.bin --outfile=megagrrl.mgu`

## Compiling and initial flash
0. If you are on Windows and flashing a MegaGRRL Desktop, a simplified guide is available [here](https://git.agiri.ninja/snippets/3). Otherwise, proceed with the instructions below.
1. Set up ESP-IDF v3.3.4 following the [Espressif documentation](https://docs.espressif.com/projects/esp-idf/en/v3.3.4/get-started/index.html).
2. Clone the repo
   - `cd ~`
   - `git clone https://git.agiri.ninja/natalie/megagrrl`
   - `cd megagrrl`
   - `git tag` <- find the latest version
   - `git checkout v0.00` <- use the latest version number here
3. Apply the patches contained in `esp-idf-patches/`.
   - `patch $IDF_PATH/components/driver/spi_master.c spi_master.c.patch`
   - `patch $IDF_PATH/components/driver/include/driver/spi_master.h spi_master.h.patch`
   - If you are on Windows and using Espressif's MSYS2 setup, you will need to install patch using `pacman -S patch`
4. Uncomment `#define FWUPDATE` in `firmware/main/hal.h`, and ensure the correct hardware version is uncommented.
5. Connect the MegaGRRL's UART to your computer. Ensure the SD card D2 pullup resistor is not populated (R999 on portable, RN2 on desktop), and no SD card is inserted.
6. Burn the 3.3 volt flash efuse using `$IDF_PATH/components/esptool_py/esptool/espefuse.py set_flash_voltage 3.3V`.
7. `make flash`. The firmware updater is now installed on the ESP32.
8. Re-comment `#define FWUPDATE`.
9. `make all`.
10. Format an SD card as FAT32, then find the `megagrrl.bin` file in `firmware/build/`, rename it to `factory.mgf`, and copy it to the root of the card.
11. Populate the SD card D2 pullup resistor.
12. Insert the card into the MegaGRRL and power it on. The firmware will now self-flash, and reboot into the OS.

Note: If the default serial device (`/dev/ttyUSB0`) doesn't work on your machine, you may need to set it manually using the `--port` option (for Python scripts) or the `ESPPORT` environment variable (for `make`). On macOS, for example, the device path may take the form of `/dev/cu.usbserial-1420` (with the Apple driver in Mojave and later) or `/dev/cu.wchusbserial1420` (with the OEM CH34x driver). On BSD systems, a path like `/dev/cuaU0` may be used.

## Development
For testing code during development, you must flash it to the correct partition. Under Linux with a standard esp-idf setup, a command such as the following will work:
`python $IDF_PATH/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 2000000 --before default_reset --after hard_reset write_flash -z --flash_mode qio --flash_freq 80m --flash_size detect 0x110000 build/megagrrl.bin`

Debugging should be done using serial (`make monitor`). JTAG is not usable due to GPIO overlap with the SD card interface.

## Obtaining VGM files
The defacto standard Genesis/Mega Drive VGM repository is [Project 2612](https://project2612.org/). Both .vgm and .vgz files are supported, but load times are shorter if using vgm.

## More information
Additional info and project logs are available on the [Hackaday.io project page](https://hackaday.io/project/161741-megagrrl-portable-ym2612-vgm-player)
