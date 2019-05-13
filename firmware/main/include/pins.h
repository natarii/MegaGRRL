#ifndef AGR_PINS_H
#define AGR_PINS_H

#define PIN_DRIVER_SHCLK 19
#define PIN_DRIVER_DATA  23
#define PIN_DRIVER_SHSTO 18

#define PIN_I2C_CLK     16
#define PIN_I2C_DATA    17

#define PIN_IOEXP_INT   35

#define PIN_BAT_SENSE    34
#define PIN_BAT_SENSE_EN 5

#define PIN_DISP_DC     22
#define PIN_DISP_CS     21
#define PIN_DISP_SCK    32
#define PIN_TOUCH_CS    25
#define PIN_DISP_MISO   26
#define PIN_DISP_MOSI   27

//hacks for loboris tft library:
#define TFT_PIN_NUM_MISO PIN_DISP_MISO
#define TFT_PIN_NUM_MOSI PIN_DISP_MOSI
#define TFT_PIN_NUM_CLK  PIN_DISP_SCK
#define TFT_PIN_NUM_CS   PIN_DISP_CS
#define TFT_PIN_NUM_DC   PIN_DISP_DC
#define TFT_PIN_NUM_TCS  PIN_TOUCH_CS
#define TFT_PIN_NUM_RST  0 //disabled
#define TFT_PIN_NUM_BCKL 0 //disabled


#endif