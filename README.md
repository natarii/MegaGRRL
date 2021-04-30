MegaGRRL - Real Hardware Sega Genesis VGM Player
========
**MegaGRRL** is an ESP32-powered VGM player, playing tracks from the Sega Mega Drive/Genesis, Master System, and more on real YM2612 and SN76489 sound chips.

## Overview
MegaGRRL is a system comprised of ESP32-powered hardware to interface with original YM2612 and SN76489 sound chips, and FreeRTOS-based firmware using the ESP-IDF framework. Its playback engine is focused on high accuracy and supporting as many features of the VGM file format as possible. It is a completely standalone player, requiring no connection to a host PC. MegaGRRL OS has a fully-featured, easy to use UI, and several unique features which make it the premier hardware playback solution.

There are currently two main hardware versions. The same firmware targets both.
  * **MegaGRRL Desktop**
  * **MegaGRRL Portable** (still in development)

The MegaGRRL Desktop hardware also supports the following **MegaMods** allowing sound chips other than the YM2612/SN76489 to be used:
  * **OPNA MegaMod** - Uses the OPNA (YM2608). Can also play VGMs for OPN (YM2203) and AY-3-8910.
  * **OPL3 MegaMod** - Uses the OPL3 (YMF262). Can also play VGMs for OPL2 (YM3812) and OPL (YM3526).

This repository contains the MegaGRRL OS firmware, and includes the hardware as submodules. Use `--recursive` when cloning the repo to get the hardware files as well.

  * **esp-idf-patches/** - ESP-IDF v3.3.4 SPI master driver patches, required for build
  * **hardware/** - Hardware files for all base board versions, and MegaMods for the Desktop version
  * **firmware/** - Firmware for the ESP32
  * **utils/** - Utilities, such as for packing firmware updates

## Compiling and initial flash
  * Windows users: A complete guide is available [here](https://git.agiri.ninja/snippets/3).
  * Other platforms: MegaGRRL OS is built like a standard ESP-IDF v3.3.4 project, but some patches are required. Please see [doc/compiling.md](https://git.agiri.ninja/natalie/megagrrl/doc/compiling.md) for more information.

## Updating firmware
To update to the latest firmware after your player is up and running, download the latest .mgu file for your hardware version (desktop or portable) from the [Releases page](https://git.agiri.ninja/natalie/megagrrl/-/releases) and copy it to the SD card. Launch the update from MegaGRRL's file browser.

## Obtaining VGM files
The defacto standard Genesis/Mega Drive VGM repository is [Project 2612](https://project2612.org/). Both .vgm and .vgz files are supported, but load times are shorter if using vgm. VGMs for other sound chips can be obtained from [VGMRips](https://vgmrips.net/packs/).
