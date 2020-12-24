#include "gd3.h"
#include "esp_log.h"
#include "math.h"
#include "vgm.h"

static const char* TAG = "Gd3";

void Gd3ParseDescriptor(FILE *f, VgmInfoStruct_t *info, Gd3Descriptor_t *desc) {
    desc->parsed = false;
    if (info->Gd3Offset > 0) {
        ESP_LOGI(TAG, "Parsing gd3...");

        fseek(f, info->Gd3Offset, SEEK_SET);
        uint32_t magic;
        fread(&magic, 4, 1, f);
        if (magic != 0x20336447) {
            ESP_LOGE(TAG, "Bad magic !!");
            return;
        }

        fread(&desc->version, 4, 1, f);
        if (desc->version != 0x00000100) {
            ESP_LOGE(TAG, "Bad version !!");
            return;
        }

        //skip size
        fseek(f, 4, SEEK_CUR);

        for (uint8_t i=0;i<GD3STRING_COUNT;i++) {
            uint32_t len = 0;
            uint16_t chr = 0;
            desc->strings[i].off = ftell(f);
            do {
                fread(&chr, sizeof(chr), 1, f);
                len++;
            } while (chr != 0);
            len--; //doh
            ESP_LOGD(TAG, "str %d len %d", i, len);
            desc->strings[i].len = len;
        }

        desc->parsed = true;

        ESP_LOGI(TAG, "OK");
    } else {
        ESP_LOGW(TAG, "Vgm has no gd3");
    }
}

void Gd3GetStringChars(FILE *f, Gd3Descriptor_t *desc, Gd3String_t stringid, char *buf, uint32_t max) { //populate a char array. sticks the null terminator on the end, max does not include it
    ESP_LOGI(TAG, "Getting string %d from gd3", (uint8_t)stringid);
    uint16_t chr = 0;
    fseek(f, desc->strings[stringid].off, SEEK_SET);
    for (uint32_t i=0;i<desc->strings[stringid].len;i++) {
        if (max >= 3) { //cheezy
            fread(&chr, sizeof(chr), 1, f);
            if (chr < 0x80) {
                *buf++ = chr;
                max--;
            } else if (chr < 0x800) {
                *buf++ = 192+chr/64;
                *buf++ = 128+chr%64;
                max -= 2;
            } else {
                *buf++ = 224+chr/4096;
                *buf++ = 128+chr/64%64;
                *buf++ = 128+chr%64;
                max -= 3;
            }
        }
    }
    *buf = 0;
}
