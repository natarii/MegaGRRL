#include "options.h"
#include "player.h"
#include "esp_log.h"
#include "driver.h"

static const char* TAG = "OptionsMgr";

volatile bool OptionsMgr_Unsaved = false;
volatile uint8_t OptionsMgr_ShittyTimer = 0;

const char *OptionCatNames[] = {
    "Playback",
    "Screen",
    "LEDs",
};

const option_t Options[OPTION_COUNT] = {
    {
        "Loop count",
        "Number of times the looping section of the track should be played.",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_LOOPS,
        &Player_SetLoopCount,
        2
    },
    {
        "Play mode",
        "Playlist behavior",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_PLAYMODE,
        &Player_RepeatMode,
        REPEAT_ALL
    },
    {
        "vgm_trim mitigation",
        "Silence erroneous notes at start of track caused by vgm_trim.",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_BOOL,
        &Driver_MitigateVgmTrim,
        true
    },
    {
        "Ignore zero-sample loops",
        "Some badly made VGMs specify a loop offset without a loop length. Deflemask is known to do this. Turning this option off will fix looping in some broken VGMs, but will cause unwanted looping in other broken VGMs.",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_BOOL,
        NULL, //
        true
    },
    {
        "Allow PCM across block boundaries",
        "Allow PCM to be played across data block boundaries. No VGMs are known to require this, so it is recommended to leave this off. Enabling this may cause significant slowdown.",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_BOOL,
        NULL, //
        false
    },
    {
        "Correct PSG frequency",
        "Automatically correct for differences between discrete TI PSGs and Sega VDP PSGs when channel frequency is set to 0.",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_BOOL,
        &Driver_FixPsgFrequency,
        true
    },
    {
        "Stereo/Mono toggle",
        "Setting this to mono will force mono sound in all VGMs.",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_STEREOMONO,
        &Driver_ForceMono,
        false
    },

    {
        "Backlight timer",
        "Length of time after the last keypress that the backlight will remain on.",
        OPTION_CATEGORY_SCREEN,
        OPTION_TYPE_NUMERIC,
        NULL, //
        10
    },
};

void OptionsMgr_Save() {
    ESP_LOGE(TAG, "saving disabled during testing");
    return;
    FILE *f = fopen("/sd/.mega/options.mgo", "w");
    uint8_t tmp = OPTIONS_VER;
    fwrite(&tmp, 1, 1, f);
    for (uint8_t i=0;i<(sizeof(Options)/sizeof(option_t));i++) {
        if (Options[i].var == NULL) {
            uint8_t z = 0;
            fwrite(&z, 1, 1, f);
            continue;
        }
        fwrite(Options[i].var, 1, 1, f);
    }
    fclose(f);
    ESP_LOGI(TAG, "options saved");
}

void OptionsMgr_Setup() {
    //load options, apply defaults if file not found
    FILE *f = fopen("/sd/.mega/options.mgo", "r");
    if (f) {
        uint8_t tmp = 0;
        fread(&tmp, 1, 1, f);
        if (tmp == OPTIONS_VER) {
            for (uint8_t i=0;i<(sizeof(Options)/sizeof(option_t));i++) {
                uint8_t v = 0;
                fread(&v, 1, 1, f);
                if (Options[i].var == NULL) continue;
                *(volatile uint8_t*)Options[i].var = v;
            }
            fclose(f);
            ESP_LOGI(TAG, "loaded options");
            return;
        } else {
            ESP_LOGW(TAG, "options file bad ver");
        }
    } else {
        ESP_LOGI(TAG, "no options file exists");
    }

    //if we get here, we need to apply defaults.
    for (uint8_t i=0;i<(sizeof(Options)/sizeof(option_t));i++) {
        if (Options[i].var == NULL) continue;
        *(volatile uint8_t*)Options[i].var = (uint8_t)Options[i].defaultval;
    }

    //write out new options file
    OptionsMgr_Save();
}

void OptionsMgr_Touch() {
    OptionsMgr_Unsaved = true;
    OptionsMgr_ShittyTimer = 0;
}

void OptionsMgr_Main() {
    while (1) {
        if (OptionsMgr_Unsaved && OptionsMgr_ShittyTimer++ == 3) {
            OptionsMgr_Unsaved = false;
            OptionsMgr_ShittyTimer = 0;
            OptionsMgr_Save();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}