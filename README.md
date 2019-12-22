MegaGRRL - Real Hardware Sega Genesis VGM Player
========
**MegaGRRL** is an ESP32-powered VGM player, playing tracks from the Sega Mega Drive/Genesis, Master System, and more on real YM2612 and SN76489 sound chips.

## Overview
MegaGRRL is a system comprised of ESP32-powered hardware to interface with original YM2612 and SN76489 sound chips, and FreeRTOS-based firmware using the ESP-IDF framework. Its playback engine is focused on high accuracy and supporting as many features of the VGM file format as possible.

Originally, the MegaGRRL project started as only a portable, handheld player. This was expanded later on to include a desktop hardware version. The firmware in this repository works on both hardware versions with minimal changes. However, the PCB and enclosure files in this repo are for the portable version only, and the files for the desktop version should be obtained from the [MegaGRRL Desktop repository](https://git.agiri.ninja/natalie/MegaGRRL_Desktop).

This project is divided into several subdirectories:
  * **firmware/** - C code for the ESP32
  * **pcb/** - PCB and schematic files for EAGLE
  * **enclosure/** - 3D printing design files for the enclosure

## Compiling and initial flash
1. Set up ESP-IDF v3.3 following the [Espressif documentation](https://docs.espressif.com/projects/esp-idf/en/v3.3/get-started/index.html).
2. Apply the patches contained in `esp-idf-patches/`.
   - `patch $IDF_PATH/components/driver/spi_master.c spi_master.c.patch`
   - `patch $IDF_PATH/components/driver/include/driver/spi_master.h spi_master.h.patch`
   - If you are on Windows and using Espressif's MSYS2 setup, you will need to install patch using `pacman -S patch`
3. Uncomment `#define FWUPDATE` in `firmware/main/hal.h`, and ensure the correct hardware version is uncommented.
4. Connect the MegaGRRL's UART to your computer. Ensure the SD card D2 pullup resistor is not populated (R999 on portable, RN2 on desktop), and no SD card is inserted.
5. Burn the 3.3 volt flash efuse using `$IDF_PATH/components/esptool_py/esptool/espefuse.py set_flash_voltage 3.3V`.
5. `make flash`. The firmware updater is now installed on the ESP32.
6. Re-comment `#define FWUPDATE`.
7. `make all`.
8. Format an SD card as FAT32, then find the `megagrrl.bin` file in `firmware/build/`, rename it to `factory.mgf`, and copy it to the root of the card.
9. Populate the SD card D2 pullup resistor.
10. Insert the card into the MegaGRRL and power it on. The firmware will now self-flash, and reboot into the OS.

## More information

Additional info and project logs are available on the [Hackaday.io project page](https://hackaday.io/project/161741-megagrrl-portable-ym2612-vgm-player)
