#include "ver.h"
#include "hal.h"

#if defined LCD_IS_ILI9341_STANDARD
#define LCD_NAME "ILI9341"
#elif defined LCD_IS_ILI9341_INV
#define LCD_NAME "ILI9341 (INV)"
#elif defined LCD_IS_ST7789_TYPE_A
#define LCD_NAME "ST7789 (A)"
#elif defined LCD_IS_QDTECH_MSP322x
#define LCD_NAME "QDtech MSP322x"
#else
#define LCD_NAME "UNK"
#endif

const char *FWVER = FWVER_D" ("LCD_NAME")";