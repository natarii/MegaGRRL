#include "options.h"
#include "player.h"
#include "esp_log.h"
#include "driver.h"
#include "leddrv.h"
#include "loader.h"
#include "userled.h"
#include "ui/filebrowser.h"
#include "player.h"

static const char* TAG = "OptionsMgr";

volatile bool OptionsMgr_Unsaved = false;
volatile uint8_t OptionsMgr_ShittyTimer = 0;

const char *OptionCatNames[OPTION_CATEGORY_COUNT] = {
    "File Browser",
    "LEDs",
    "Playback",
    //"Screen", portable only, so getting rid of it for now
};

static void opts_mutingupdate() {
    xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_UPDATE_MUTING);
}

static void opts_invalidatefilebrowser() {
    
}

const option_t Options[OPTION_COUNT] = {
    {
        "Loop count",
        "Number of times the looping section of the track should be played.",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_LOOPS,
        &Player_SetLoopCount,
        2,
        NULL
    },
    {
        "Play mode",
        "Playlist behavior",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_PLAYMODE,
        &Player_RepeatMode,
        REPEAT_ALL,
        NULL
    },/*
    {
        "vgm_trim mitigation",
        "Silence erroneous notes at start of track caused by vgm_trim.",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_BOOL,
        &Driver_MitigateVgmTrim,
        true,
        NULL
    },*/
    {
        "Ignore zero-sample loops",
        "Some badly made VGMs specify a loop offset without a loop length. Deflemask is known to do this. Turning this option off will fix looping in some broken VGMs, but will cause unwanted looping in other broken VGMs.",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_BOOL,
        &Loader_IgnoreZeroSampleLoops,
        true,
        NULL
    },/*
    {
        "Allow PCM across block boundaries",
        "Allow PCM to be played across data block boundaries. No VGMs are known to require this, so it is recommended to leave this off. Enabling this may cause significant slowdown.",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_BOOL,
        NULL, //
        false,
        NULL
    },
    {
        "Correct PSG frequency",
        "Automatically correct for differences between discrete TI PSGs and Sega VDP PSGs when channel frequency is set to 0.",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_BOOL,
        &Driver_FixPsgFrequency,
        true,
        NULL
    },*/
    {
        "Stereo/Mono toggle",
        "Setting this to mono will force mono sound in all VGMs.",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_STEREOMONO,
        &Driver_ForceMono,
        false,
        opts_mutingupdate
    },
    {
        "Channel LED brightness",
        "Sets the overall brightness of the channel status LEDs",
        OPTION_CATEGORY_LEDS,
        OPTION_TYPE_NUMERIC,
        &LedDrv_Brightness, //
        0x40,
        LedDrv_UpdateBrightness
    },
    {
        "User LED A",
        "Sets the source of User LED A",
        OPTION_CATEGORY_LEDS,
        OPTION_TYPE_USERLED,
        &UserLedMgr_Source[0],
        USERLED_SRC_PLAYPAUSE,
        NULL
    },
    {
        "User LED B",
        "Sets the source of User LED B",
        OPTION_CATEGORY_LEDS,
        OPTION_TYPE_USERLED,
        &UserLedMgr_Source[1],
        USERLED_SRC_DISK_ALL,
        NULL
    },
    {
        "User LED C",
        "Sets the source of User LED C",
        OPTION_CATEGORY_LEDS,
        OPTION_TYPE_USERLED,
        &UserLedMgr_Source[2],
        USERLED_SRC_NONE,
        NULL
    },
    {
        "Sort directory contents",
        "Enabling this sorts directory contents according to the sorting options. When disabled, contents will be displayed in the order they are found in the filesystem.",
        OPTION_CATEGORY_FILEBROWSER,
        OPTION_TYPE_BOOL,
        &Ui_FileBrowser_Sort,
        true,
        opts_invalidatefilebrowser
    },
    {
        "Alphabetical sort direction",
        "Set whether directory contents are sorted in ascending or descending order.",
        OPTION_CATEGORY_FILEBROWSER,
        OPTION_TYPE_SORTDIR,
        &Ui_FileBrowser_SortDir,
        SORT_ASCENDING,
        opts_invalidatefilebrowser
    },
    {
        "Sort dirs before files",
        "Set whether directories appear before files.",
        OPTION_CATEGORY_FILEBROWSER,
        OPTION_TYPE_BOOL,
        &Ui_FileBrowser_DirsBeforeFiles,
        true,
        opts_invalidatefilebrowser
    },
    {
        "Overwrite VGZ files",
        "When this is enabled, VGZ files will be overwritten with extracted VGM equivalents when first played. This will replace the files on the SD card, but will result in much shorter track load times on future plays.",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_BOOL,
        &Player_UnvgzReplaceOriginal,
        true,
        NULL
    }
/* just getting rid of this for now. portable only
    {
        "Backlight timer",
        "Length of time after the last keypress that the backlight will remain on.",
        OPTION_CATEGORY_SCREEN,
        OPTION_TYPE_NUMERIC,
        NULL, //
        10
    },
*/
};

void OptionsMgr_Save() {
    /*ESP_LOGE(TAG, "saving disabled during testing");
    return;*/
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
                if (Options[i].cb != NULL) Options[i].cb();
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
        if (Options[i].cb != NULL) Options[i].cb();
    }

    /*//write out new options file
    OptionsMgr_Save();*/
}

void OptionsMgr_Touch() {
    OptionsMgr_Unsaved = true;
    OptionsMgr_ShittyTimer = 0;
}

void OptionsMgr_Main() {
    while (1) {
        if (OptionsMgr_Unsaved && OptionsMgr_ShittyTimer++ == 2) {
            OptionsMgr_Unsaved = false;
            OptionsMgr_ShittyTimer = 0;
            OptionsMgr_Save();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}