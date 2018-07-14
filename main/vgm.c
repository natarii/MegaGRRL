#include "vgm.h"
#include "esp_log.h"

static const char* TAG = "Vgm";

uint8_t Vgm_CommandLength(uint8_t Command) { //including the command itself. only for fixed size commands
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

bool Vgm_CommandIsFixedSize(uint8_t Command) {
  if (Command == 0x67 || Command == 0x68) { //data block, pcm ram write
    return false;
  }
  return true;
}

void VgmParseHeader(FILE *f, VgmInfoStruct_t *info) {

}

void VgmParseDevices(FILE *f, VgmInfoStruct_t *info, VgmDeviceStruct_t *devices) {

}

void VgmParseDataBlocks(FILE *f, VgmInfoStruct_t *info, VgmDataBlockStruct_t *blocks) {
  
}