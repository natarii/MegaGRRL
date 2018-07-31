#include "vgm.h"
#include "esp_log.h"
#include "math.h"

static const char* TAG = "Vgm";

uint8_t VgmCommandLength(uint8_t Command) { //including the command itself. only for fixed size commands
  //TODO: should this just be a LUT?
  if (Command == 0x30 || Command == 0x3f || Command == 0x4f || Command == 0x50) { //PSG stuff
    return 2;
  } else if ((Command >= 0x51 && Command <= 0x5f) || (Command >= 0xa5 && Command <= 0xaf)) { //YM series
    return 3;
  } else if (Command == 0x61) { //16bit wait
    return 3;
  } else if (Command == 0x62 || Command == 0x63) { //50hz and 60hz waits
    return 1;
  } else if (Command == 0x66) { //end of sound data
    return 1;
  } else if (Command >= 0x70 && Command <= 0x7f) { //4bit wait
    return 1;
  } else if (Command >= 0x80 && Command <= 0x8f) { //YM2612 PORT0 PCM WRITE
    return 1;
  } else if (Command == 0x90) { //dac stream control setup
    return 5;
  } else if (Command == 0x91) { //dac stream control set data
    return 5;
  } else if (Command == 0x92) { //dac stream control set frequency
    return 6;
  } else if (Command == 0x93) { //dac stream control start stream
    return 11;
  } else if (Command == 0x94) { //dac stream control stop
    return 2;
  } else if (Command == 0x95) { //dac stream control start stream (fast call)
    return 5;
  } else if (Command >= 0xa0 && Command <= 0xbf) { //misc 16bit chips
    return 3;
  } else if (Command >= 0xc0 && Command <= 0xd6) { //misc 24bit chips
    return 4;
  } else if (Command == 0xe0) { //PCM data type 0 seek
    return 5;
  } else if (Command == 0xe1) { //c352 register write
    return 5;
  }
  ESP_LOGW(TAG, "Vgm_CommandLength got bad command 0x%02x !!", Command);
  return 0xff;
}

bool VgmCommandIsFixedSize(uint8_t Command) {
  if (Command == 0x67 || Command == 0x68) { //data block, pcm ram write
    return false;
  }
  return true;
}

bool VgmParseHeader(FILE *f, VgmInfoStruct_t *info) {
  fseek(f, 0, SEEK_SET);

  uint32_t magic;
  fread(&magic, 4, 1, f);
  if (magic != 0x206d6756) {
    ESP_LOGE(TAG, "Bad magic !!");
    return false;
  }

  fread(&info->EofOffset, 4, 1, f);
  uint8_t tempver[4];
  fread(&tempver, 4, 1, f);
  //decode silly bcd version no
  info->Version = 0;
  uint8_t exp = 0;
  for (uint8_t i=0;i<4;i++) {
    info->Version += (uint32_t)(tempver[i] & 0xf)*(uint32_t)pow(10,exp++);
    info->Version += (uint32_t)(tempver[i]>>4)*(uint32_t)pow(10,exp++);
  }

  fseek(f, 0x14, SEEK_SET);
  fread(&info->Gd3Offset, 4, 1, f);
  info->Gd3Offset += 0x14;
  fread(&info->TotalSamples, 4, 1, f);
  fread(&info->LoopOffset, 4, 1, f);
  info->LoopOffset += 0x1c;
  fread(&info->LoopSamples, 4, 1, f);
  if (info->Version >= 101) fread(&info->Rate, 4, 1, f);

  if (info->Version >= 150) {
    fseek(f, 0x34, SEEK_SET);
    fread(&info->DataOffset, 4, 1, f);
    info->DataOffset += 0x34;
  }

  if (info->Version >= 160 && info->DataOffset > 0x7c) {
    fseek(f, 0x7c, SEEK_SET);
    fread(&info->VolumeModifier, 1, 1, f);
    if (info->DataOffset > 0x7e) {
      fseek(f, 1, SEEK_CUR);
      fread(&info->LoopBase, 1, 1, f);
    }
  }

  if (info->Version >= 151 && info->DataOffset > 0x7f) {
    fseek(f, 0x7f, SEEK_SET);
    fread(&info->LoopModifier, 1, 1, f);
  }

  if (info->Version >= 170 && info->DataOffset > (0xbc+0x04)) {
    fseek(f, 0xbc, SEEK_SET);
    fread(&info->ExtraHeaderOffset, 4, 1, f);
  }

  return true;
}

bool VgmParseDevices(FILE *f, VgmInfoStruct_t *info, VgmDeviceStruct_t *devices) {
  return false;
}

bool VgmParseDataBlocks(FILE *f, VgmInfoStruct_t *info, VgmDataBlockStruct_t *blocks) {
  return false;
}