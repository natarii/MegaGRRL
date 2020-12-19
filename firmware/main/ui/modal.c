#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "lvgl.h"
#include "../ui.h"
#include "../lcddma.h"
#include "modal.h"
#include "softbar.h"

//static const char* TAG = "Ui_Modal";

static bool visible = false;
static volatile bool updated = false;
static volatile char *newtitle;
static volatile char *newtext;
static volatile char *newbutton;
static IRAM_ATTR lv_obj_t *container;
static lv_style_t container_style;
static IRAM_ATTR lv_obj_t *textcontainer;
static lv_style_t textcontainer_style;
static IRAM_ATTR lv_obj_t *titletext;
static lv_style_t titletext_style;
static IRAM_ATTR lv_obj_t *bodytext;
static lv_style_t bodytext_style;


void modal_show_simple(const char *CALLER_TAG, char *title, char *text, char *button) {
    newtitle = title;
    newtext = text;
    newbutton = button;
    updated = true;
    ESP_LOGI(CALLER_TAG, "Modal updated");
}

bool modal_is_visible() {
    if (updated && !visible) {
        updated = false;
        visible = true;
    }
    return visible;
}

bool modal_is_updated() {
    if (updated) {
        updated = false;
        return true;
    }
    return false;
}

void Ui_Modal_Key(KeyEvent_t event) {
    if (event.Key == KEY_C && event.State == KEY_EVENT_PRESS) {
        KeyMgr_Consume(KEY_C);
        visible = false;
    }
}

void modal_update(bool HandleMutex) {
    if (HandleMutex) LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_label_set_static_text(titletext, (const char *)newtitle);
    lv_obj_set_pos(titletext, 5, 3);
    lv_label_set_static_text(bodytext, (const char *)newtext);
    lv_obj_align(bodytext, titletext, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 1);
    lv_cont_set_fit4(textcontainer, LV_FIT_NONE, LV_FIT_NONE, LV_FIT_NONE, LV_FIT_TIGHT);
    Ui_SoftBar_UpdateModal(2, true, (char *)newbutton, false);
    if (HandleMutex) LcdDma_Mutex_Give();
}

void Ui_Modal_Setup(lv_obj_t *uiscreen) {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    
    //phantasmagoria of dim background
    //uiscreen isn't used, we draw on the top
    lv_obj_t *layer = lv_layer_sys();
    container = lv_cont_create(layer, NULL);
    lv_obj_set_height(container, 250);
    lv_obj_set_width(container, 240);
    lv_obj_set_pos(container, 0, 34+1);
    lv_style_copy(&container_style, &lv_style_plain);
    container_style.body.main_color = container_style.body.grad_color = LV_COLOR_BLACK;
    container_style.body.opa = LV_OPA_90;
    lv_cont_set_style(container, LV_CONT_STYLE_MAIN, &container_style);

    textcontainer = lv_cont_create(container, NULL);
    lv_style_copy(&textcontainer_style, &lv_style_plain);
    textcontainer_style.body.main_color = textcontainer_style.body.grad_color = LV_COLOR_BLACK;
    lv_cont_set_style(textcontainer, LV_CONT_STYLE_MAIN, &textcontainer_style);
    lv_obj_set_width(textcontainer, 250);
    //lv_obj_set_height(textcontainer, 50);
    lv_obj_align(textcontainer, container, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_auto_realign(textcontainer, true);

    titletext = lv_label_create(textcontainer, NULL);
    lv_style_copy(&titletext_style, &lv_style_plain);
    titletext_style.text.color = LV_COLOR_WHITE;
    titletext_style.text.font = &lv_font_dejavu_18;
    lv_label_set_long_mode(titletext, LV_LABEL_LONG_EXPAND);
    lv_obj_set_width(titletext, 230);
    lv_label_set_style(titletext, LV_LABEL_STYLE_MAIN, &titletext_style);
    lv_label_set_static_text(titletext, "Modal Title");
    lv_obj_set_auto_realign(titletext, true);
    lv_obj_set_pos(titletext, 5, 3);

    bodytext = lv_label_create(textcontainer, NULL);
    lv_style_copy(&bodytext_style, &lv_style_plain);
    bodytext_style.text.color = LV_COLOR_MAKE(170,170,170);
    bodytext_style.text.font = &lv_font_dejavu_14_2bpp;
    bodytext_style.text.line_space = -2;
    lv_label_set_long_mode(bodytext, LV_LABEL_LONG_BREAK);
    lv_obj_set_width(bodytext, 230);
    lv_label_set_style(bodytext, LV_LABEL_STYLE_MAIN, &bodytext_style);
    lv_label_set_align(bodytext, LV_LABEL_ALIGN_CENTER);
    lv_label_set_static_text(bodytext, "Modal Text");
    lv_obj_set_auto_realign(bodytext, true);
    lv_obj_align(bodytext, titletext, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 1);

    lv_cont_set_fit4(textcontainer, LV_FIT_NONE, LV_FIT_NONE, LV_FIT_NONE, LV_FIT_TIGHT);

    Ui_SoftBar_UpdateModal(0, false, "", false);
    Ui_SoftBar_UpdateModal(1, false, "", false);
    Ui_SoftBar_UpdateModal(2, true, LV_SYMBOL_OK" OK", false);
    Ui_SoftBar_ShowModal(true, false);
    
    modal_update(false);

    LcdDma_Mutex_Give();
}

void Ui_Modal_Destroy() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_obj_del(container);
    Ui_SoftBar_ShowModal(false, false);
    LcdDma_Mutex_Give();
}
