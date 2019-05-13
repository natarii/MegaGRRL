MegaGRRL - Handheld YM2612 VGM Player
========
**MegaGRRL** is a handheld ESP32-powered VGM player, playing tracks from the Sega Mega Drive/Genesis, Master System, and more.
## Overview
MegaGRRL is a system comprised of ESP32-powered hardware to interface with the original sound chips, and FreeRTOS-based firmware using the ESP-IDF framework. Its playback engine is focused on high accuracy and supporting as many features of the VGM file format as possible.

At the moment, this project is a work in progress, and the files in this repository are not complete.

This project is divided into several subdirectories:
  * **firmware/** - C code for the ESP32
  * **pcb/** - PCB and schematic files for EAGLE
  * **enclosure/** - 3D printing design files for the enclosure

Additional info and project logs are available on the [Hackaday.io project page](https://hackaday.io/project/161741-megagrrl-portable-ym2612-vgm-player)