#ifndef AGR_VGM_H
#define AGR_VGM_H

#include <stdlib.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "esp_system.h"

enum VgmDeviceType {
  VGM_DEVICE_SN76489,
  VGM_DEVICE_T6W28, //neogeo pocket psg
  VGM_DEVICE_YM2413,
  VGM_DEVICE_YM2612,
  VGM_DEVICE_YM2151,
  VGM_DEVICE_SEGAPCM,
  VGM_DEVICE_RF5C68,
  VGM_DEVICE_YM2203,
  VGM_DEVICE_YM2608,
  VGM_DEVICE_YM2610,
  VGM_DEVICE_YM2610B, //alternate YM2610 type
  VGM_DEVICE_YM3812,
  VGM_DEVICE_YM3526,
  VGM_DEVICE_Y8950,
  VGM_DEVICE_YMF262,
  VGM_DEVICE_YMF278B,
  VGM_DEVICE_YMF271,
  VGM_DEVICE_YMZ280B,
  VGM_DEVICE_RF5C164,
  VGM_DEVICE_PWM,
  VGM_DEVICE_AY8910,
  VGM_DEVICE_GB_DMG,
  VGM_DEVICE_NES_APU,
  VGM_DEVICE_MULTIPCM,
  VGM_DEVICE_UPD7759,
  VGM_DEVICE_OKIM6258,
  VGM_DEVICE_OKIM6295,
  VGM_DEVICE_K051649,
  VGM_DEVICE_K054539,
  VGM_DEVICE_HUC6280,
  VGM_DEVICE_C140,
  VGM_DEVICE_K053260,
  VGM_DEVICE_POKEY,
  VGM_DEVICE_QSOUND,
  VGM_DEVICE_SCSP,
  VGM_DEVICE_WONDERSWAN,
  VGM_DEVICE_VSU,
  VGM_DEVICE_SAA1099,
  VGM_DEVICE_ES5503,
  VGM_DEVICE_ES5506,
  VGM_DEVICE_X1010,
  VGM_DEVICE_C352,
  VGM_DEVICE_GA20,

  //alternate AY8910 types
  VGM_DEVICE_AY8912,
  VGM_DEVICE_AY8913,
  VGM_DEVICE_AY8930,
  VGM_DEVICE_YM2149,
  VGM_DEVICE_YM3439,
  VGM_DEVICE_YMZ284,
  VGM_DEVICE_YMZ294,
};

typedef struct {
    //all offsets are absolute positions
    uint32_t EofOffset;
    uint32_t Version;
    uint32_t Gd3Offset;
    uint32_t TotalSamples;
    uint32_t LoopOffset;
    uint32_t LoopSamples;
    uint32_t Rate;
    uint32_t DataOffset;
    uint8_t VolumeModifier;
    uint8_t LoopBase;
    uint8_t LoopModifier;
    uint32_t ExtraHeaderOffset;
} VgmInfoStruct_t;

typedef struct {
    uint8_t Type;
    uint8_t Count;
    uint32_t ClockFrequency;
    uint32_t Flags1;
    uint32_t Flags2;
} VgmDeviceStruct_t;

typedef struct {
    uint8_t Type;
    uint32_t Size;
    uint32_t Offset; //in vgm file
    uint32_t StartAddress; //for partial rom dumps
    uint32_t RomSize;
    uint8_t CompressionType; //decomp table, nbit, dpcm
    uint32_t UncompressedSize; //nbit, dpcm
    uint8_t BitsDecompressed; //nbit, dpcm
    uint8_t BitsCompressed; //nbit, dpcm
    uint8_t Subtype; //decomp table, nbit, dpcm
    uint16_t CompValue; //decomp table = num values, nbit = added value, dpcm = start value
} VgmDataBlockStruct_t;

uint8_t VgmCommandLength(uint8_t Command);
bool VgmCommandIsFixedSize(uint8_t Command);
bool VgmParseHeader(FILE *f, VgmInfoStruct_t *info);
bool VgmParseDataBlock(FILE *f, VgmDataBlockStruct_t *block);

#endif