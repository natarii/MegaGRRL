#include "options.h"
#include "player.h"
#include "esp_log.h"
#include "driver.h"
#include "leddrv.h"
#include "loader.h"
#include "userled.h"
#include "ui/filebrowser.h"
#include "player.h"
#include "clk.h"
#include "pitch.h"

static const char* TAG = "OptionsMgr";

volatile bool OptionsMgr_Unsaved = false;
volatile uint8_t OptionsMgr_ShittyTimer = 0;

const char *OptionCatNames[OPTION_CATEGORY_COUNT] = {
    "File Browser",
    "LEDs",
    "Playback",
    "Advanced"
    //"Screen", portable only, so getting rid of it for now
};

static void opts_mutingupdate() {
    xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_UPDATE_MUTING);
}

static void opts_pitchupdate() {
    Pitch_Adjust(Pitch_Get()); //this will snap down to the max locked clock if they switch from unlocked -> locked
}

static bool loaded[OPTION_COUNT];

const option_t Options[OPTION_COUNT] = {
    {
        0x0000,
        "Loop count",
        "Number of times the looping section of the track should be played.",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_LOOPS,
        &Player_LoopCount,
        2,
        NULL,
        NULL
    },
    {
        0x0001,
        "Play mode",
        "Playlist behavior",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_PLAYMODE,
        &Player_RepeatMode,
        REPEAT_ALL,
        NULL,
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
        0x0002,
        "Ignore zero-sample loops",
        "Some badly made VGMs specify a loop offset without a loop length. Deflemask is known to do this. Turning this option off will fix looping in some broken VGMs, but will cause unwanted looping in other broken VGMs.",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_BOOL,
        &Loader_IgnoreZeroSampleLoops,
        true,
        NULL,
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
        0x0003,
        "Stereo/Mono toggle",
        "Setting this to mono will force mono sound in all VGMs.",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_STEREOMONO,
        &Driver_ForceMono,
        false,
        opts_mutingupdate,
        opts_mutingupdate
    },
    {
        0x0004,
        "Channel LED brightness",
        "Sets the overall brightness of the channel status LEDs",
        OPTION_CATEGORY_LEDS,
        OPTION_TYPE_NUMERIC,
        &LedDrv_Brightness, //
        0x40,
        LedDrv_UpdateBrightness,
        LedDrv_UpdateBrightness
    },
    {
        0x000e,
        "Fade out",
        "Enables and disables fading out at the end of each track",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_BOOL,
        &Driver_FadeEnabled,
        true,
        NULL,
        NULL
    },
    {
        0x000f,
        "Fade length",
        "Sets length of fade out, in seconds",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_NUMERIC,
        &Driver_FadeLength,
        3,
        NULL,
        NULL
    },
    {
        0x0005,
        "User LED A",
        "Sets the source of User LED A",
        OPTION_CATEGORY_LEDS,
        OPTION_TYPE_USERLED,
        &UserLedMgr_Source[0],
        USERLED_SRC_PLAYPAUSE,
        NULL,
        NULL
    },
    {
        0x0006,
        "User LED B",
        "Sets the source of User LED B",
        OPTION_CATEGORY_LEDS,
        OPTION_TYPE_USERLED,
        &UserLedMgr_Source[1],
        USERLED_SRC_DISK_ALL,
        NULL,
        NULL
    },
    {
        0x0007,
        "User LED C",
        "Sets the source of User LED C",
        OPTION_CATEGORY_LEDS,
        OPTION_TYPE_USERLED,
        &UserLedMgr_Source[2],
        USERLED_SRC_NONE,
        NULL,
        NULL
    },
    {
        0x0008,
        "Sort directory contents",
        "Enabling this sorts directory contents according to the sorting options. When disabled, contents will be displayed in the order they are found in the filesystem.",
        OPTION_CATEGORY_FILEBROWSER,
        OPTION_TYPE_BOOL,
        &Ui_FileBrowser_Sort,
        true,
        Ui_FileBrowser_InvalidateDirEntry,
        NULL
    },
    {
        0x0009,
        "Alphabetical sort direction",
        "Set whether directory contents are sorted in ascending or descending order.",
        OPTION_CATEGORY_FILEBROWSER,
        OPTION_TYPE_SORTDIR,
        &Ui_FileBrowser_SortDir,
        SORT_ASCENDING,
        Ui_FileBrowser_InvalidateDirEntry,
        NULL
    },
    {
        0x000a,
        "Sort dirs before files",
        "Set whether directories appear before files.",
        OPTION_CATEGORY_FILEBROWSER,
        OPTION_TYPE_BOOL,
        &Ui_FileBrowser_DirsBeforeFiles,
        true,
        Ui_FileBrowser_InvalidateDirEntry,
        NULL
    },
    {
        0x000b,
        "Overwrite VGZ files",
        "When this is enabled, VGZ files will be overwritten with extracted VGM equivalents when first played. This will replace the files on the SD card, but will result in much shorter track load times on future plays.",
        OPTION_CATEGORY_PLAYBACK,
        OPTION_TYPE_BOOL,
        &Player_UnvgzReplaceOriginal,
        true,
        NULL,
        NULL
    },
    {
        0x000c,
        "Narrow font",
        "Use a narrow font in the file browser.",
        OPTION_CATEGORY_FILEBROWSER,
        OPTION_TYPE_BOOL,
        &Ui_FileBrowser_Narrow,
        false,
        NULL,
        NULL
    },
    {
        0x000d,
        "Unlock pitch",
        "Allow additional pitch adjustment. Warning: Sound chips will be overclocked.",
        OPTION_CATEGORY_ADVANCED,
        OPTION_TYPE_BOOL,
        &Clk_Unlock,
        false,
        opts_pitchupdate,
        NULL
    },
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
    ESP_LOGI(TAG, "saving options");
    FILE *f = fopen("/sd/.mega/options.mgo", "w");
    uint8_t tmp = OPTIONS_VER;
    fwrite(&tmp, 1, 1, f);
    tmp = OPTION_COUNT;
    fwrite(&tmp, 1, 1, f);
    for (uint8_t i=0;i<OPTION_COUNT;i++) {
        fwrite(&Options[i].uid, 2, 1, f);
        if (Options[i].var == NULL) {
            tmp = 0;
            fwrite(&tmp, 1, 1, f);
            continue;
        }
        fwrite(Options[i].var, 1, 1, f);
    }
    fclose(f);
    ESP_LOGI(TAG, "options saved");
}

void OptionsMgr_Setup() {
    //load options, apply defaults if file not found
    memset(&loaded, false, OPTION_COUNT);
    ESP_LOGI(TAG, "checking options file");
    FILE *f = fopen("/sd/.mega/options.mgo", "r");
    if (f) {
        uint8_t tmp = 0;
        fread(&tmp, 1, 1, f);
        if (tmp == OPTIONS_VER) {
            fread(&tmp, 1, 1, f);
            ESP_LOGI(TAG, "file contains %d options", tmp);
            for (uint8_t i=0;i<tmp;i++) {
                uint16_t uid = 0;
                fread(&uid, 2, 1, f);
                uint8_t v = 0;
                fread(&v, 1, 1, f);
                bool found = false;
                for (uint8_t j=0;j<OPTION_COUNT;j++) {
                    if (Options[j].uid == uid) {
                        found = true;
                        if (Options[j].var != NULL) *(volatile uint8_t*)Options[j].var = v;
                        if (Options[j].cb_initial != NULL) Options[j].cb_initial();
                        loaded[j] = true;
                        ESP_LOGI(TAG, "applied option uid %d - %d", uid, v);
                        break;
                    }
                }
                if (!found) {
                    ESP_LOGW(TAG, "skipping unknown option uid %d", uid);
                }
            }
            fclose(f);
            ESP_LOGI(TAG, "finished loading options file");
        } else {
            ESP_LOGW(TAG, "options file bad ver (0x%02x != 0x%02x)", tmp, OPTIONS_VER);
        }
    } else {
        ESP_LOGI(TAG, "no options file exists");
    }

    for (uint8_t i=0;i<OPTION_COUNT;i++) {
        if (!loaded[i]) {
            if (Options[i].var != NULL) *(volatile uint8_t*)Options[i].var = (uint8_t)Options[i].defaultval;
            if (Options[i].cb_initial != NULL) Options[i].cb_initial();
            ESP_LOGW(TAG, "applied defaults for option uid %d", Options[i].uid);
        }
    }

    ESP_LOGI(TAG, "done");
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