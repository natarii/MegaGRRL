#include "tutorial.h"
#include "options.h"
#include "ui/modal.h"

static const char* TAG = "Tutorial";

volatile uint8_t tutorial_completion = 0;

static char* title = "#ff2020 \xef\x80\x84# Tutorial";
#define COLOR_APP "00ffff"
#define COLOR_KEY "00ff00"

bool tutorial_section_is_complete(uint8_t section) {
    //return tutorial_completion & section;
    return false; //HACK for testing...
}

void tutorial_section_mark_complete(uint8_t section) {
    tutorial_completion |= section;
    OptionsMgr_Touch();
}

void tutorial_show_if_not_complete(uint8_t section, bool mark_complete, uint8_t extra_data) {
    if (tutorial_section_is_complete(section)) return;

    if (section == TUT_CMPL_MAINMENU) {
        modal_show_simple(TAG, title,
        "#7f7fff Welcome to MegaGRRL!#\n\nThis is the "
        "#"COLOR_APP" Home Screen#.\n\nYou can use the #"COLOR_KEY" LEFT#"
        " and\n#"COLOR_KEY" RIGHT# keys to select apps.\n\nThe #"COLOR_KEY" A#"
        ", #"COLOR_KEY" B#, and #"COLOR_KEY" C# keys match up with the bar at "
        "the bottom of the screen.\n\nTo close this pop-up, press "
        "#"COLOR_KEY" C#.", LV_SYMBOL_OK" OK");
    } else if (section == TUT_CMPL_NOWPLAYING) {
        if (extra_data == 0) { //something playing
            modal_show_simple(TAG, title,
            "This is the #"COLOR_APP" Music Player#.\nYou can control the "
            "currently playing track from here.\n\nThe #"COLOR_KEY" D-PAD# is "
            "primarily used to control this app. For example, pressing "
            "#"COLOR_KEY" UP# will pause playback.\n\n"
            "Keys #"COLOR_KEY" B# and #"COLOR_KEY" C# provide a shortcut to "
            "#"COLOR_APP" File Browser#, and quick access to common playback "
            "settings.\n\nTo close this pop-up, press #"COLOR_KEY" C#.",
            LV_SYMBOL_OK" OK");
        } else if (extra_data == 1) { //nothing playing
            modal_show_simple(TAG, title,
            "This is the #"COLOR_APP" Music Player#.\n\n"
            "If a track was being played, you could control it here.\n\n"
            "Try going to the #"COLOR_APP" File Browser# and playing "
            "something.\n\nPress #"COLOR_KEY" C# to close this pop-up, and "
            "then #"COLOR_KEY" A# to go to the\n#"COLOR_APP" Home Screen#, "
            "or #"COLOR_KEY" B# for a shortcut to the #"COLOR_APP" File "
            "Browser#.", LV_SYMBOL_OK" OK");
        }
    }

    if (mark_complete) tutorial_section_mark_complete(section);
}