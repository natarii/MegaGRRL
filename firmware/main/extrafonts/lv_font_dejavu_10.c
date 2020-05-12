#include "lvgl/lvgl.h"

/*******************************************************************************
 * Size: 10 px
 * Bpp: 1
 * Opts: 
 ******************************************************************************/

#ifndef LV_FONT_DEJAVU_10
#define LV_FONT_DEJAVU_10 1
#endif

#if LV_FONT_DEJAVU_10

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t gylph_bitmap[] = {
    /* U+20 " " */
    0x0,

    /* U+21 "!" */
    0xf9,

    /* U+22 "\"" */
    0xb6, 0x80,

    /* U+23 "#" */
    0x28, 0x53, 0xf9, 0x44, 0x9f, 0x94, 0x28,

    /* U+24 "$" */
    0x23, 0xe9, 0x47, 0x14, 0xbe, 0x21, 0x0,

    /* U+25 "%" */
    0xe2, 0x52, 0x29, 0x1d, 0x1, 0x30, 0xa4, 0x92,
    0x6,

    /* U+26 "&" */
    0x30, 0x81, 0x3, 0x9, 0x51, 0xa3, 0x3a,

    /* U+27 "'" */
    0xe0,

    /* U+28 "(" */
    0x5a, 0xaa, 0x50,

    /* U+29 ")" */
    0xa5, 0x56, 0x80,

    /* U+2A "*" */
    0x25, 0x5d, 0xf2, 0x0,

    /* U+2B "+" */
    0x10, 0x23, 0xf8, 0x81, 0x2, 0x0,

    /* U+2C "," */
    0xc0,

    /* U+2D "-" */
    0xe0,

    /* U+2E "." */
    0x80,

    /* U+2F "/" */
    0x24, 0xa4, 0x94, 0x80,

    /* U+30 "0" */
    0x76, 0xe3, 0x18, 0xc7, 0x6e,

    /* U+31 "1" */
    0xe1, 0x8, 0x42, 0x10, 0x9f,

    /* U+32 "2" */
    0x70, 0x42, 0x11, 0x11, 0x1f,

    /* U+33 "3" */
    0xf0, 0x42, 0xe0, 0x84, 0x3e,

    /* U+34 "4" */
    0x18, 0x62, 0x92, 0x4b, 0xf0, 0x82,

    /* U+35 "5" */
    0x72, 0x10, 0xe0, 0x84, 0x3e,

    /* U+36 "6" */
    0x7a, 0x21, 0xe8, 0xc6, 0x2e,

    /* U+37 "7" */
    0xf8, 0x44, 0x21, 0x10, 0x8c,

    /* U+38 "8" */
    0x74, 0x62, 0xe8, 0xc6, 0x2e,

    /* U+39 "9" */
    0x74, 0x63, 0x17, 0x84, 0x5e,

    /* U+3A ":" */
    0x88,

    /* U+3B ";" */
    0x86,

    /* U+3C "<" */
    0x6, 0x73, 0x3, 0x80, 0xc0, 0x40,

    /* U+3D "=" */
    0xfc, 0xf, 0xc0,

    /* U+3E ">" */
    0xc0, 0x70, 0x18, 0xce, 0x0, 0x0,

    /* U+3F "?" */
    0xe1, 0x12, 0x44, 0x4,

    /* U+40 "@" */
    0x1e, 0x31, 0x90, 0x73, 0xda, 0x2d, 0x16, 0x8a,
    0xbe, 0x60, 0xf, 0x0,

    /* U+41 "A" */
    0x10, 0x50, 0xa1, 0x44, 0x4f, 0x91, 0x41,

    /* U+42 "B" */
    0xf4, 0x63, 0xe8, 0xc6, 0x3e,

    /* U+43 "C" */
    0x39, 0x18, 0x20, 0x82, 0x4, 0x4e,

    /* U+44 "D" */
    0xf2, 0x28, 0x61, 0x86, 0x18, 0xbc,

    /* U+45 "E" */
    0xfc, 0x21, 0xe8, 0x42, 0x1f,

    /* U+46 "F" */
    0xf8, 0x8f, 0x88, 0x88,

    /* U+47 "G" */
    0x39, 0x18, 0x20, 0x8e, 0x14, 0x4e,

    /* U+48 "H" */
    0x86, 0x18, 0x7f, 0x86, 0x18, 0x61,

    /* U+49 "I" */
    0xff,

    /* U+4A "J" */
    0x24, 0x92, 0x49, 0x38,

    /* U+4B "K" */
    0x8a, 0x4a, 0x30, 0xe2, 0xc9, 0x22,

    /* U+4C "L" */
    0x84, 0x21, 0x8, 0x42, 0x1f,

    /* U+4D "M" */
    0xc7, 0x8f, 0x2d, 0x5a, 0xb2, 0x60, 0xc1,

    /* U+4E "N" */
    0xc7, 0x1a, 0x69, 0x96, 0x58, 0xe3,

    /* U+4F "O" */
    0x38, 0x8a, 0xc, 0x18, 0x30, 0x51, 0x1c,

    /* U+50 "P" */
    0xf4, 0x63, 0x1f, 0x42, 0x10,

    /* U+51 "Q" */
    0x38, 0x8a, 0xc, 0x18, 0x30, 0x51, 0x1c, 0x4,

    /* U+52 "R" */
    0xf2, 0x28, 0xa2, 0xf2, 0x68, 0xa1,

    /* U+53 "S" */
    0x7c, 0x61, 0xc1, 0x84, 0x3e,

    /* U+54 "T" */
    0xfe, 0x20, 0x40, 0x81, 0x2, 0x4, 0x8,

    /* U+55 "U" */
    0x86, 0x18, 0x61, 0x86, 0x1c, 0xde,

    /* U+56 "V" */
    0x82, 0x89, 0x12, 0x22, 0x85, 0xe, 0x8,

    /* U+57 "W" */
    0x8c, 0x93, 0x24, 0xc9, 0x52, 0x52, 0x8c, 0xc3,
    0x30, 0xcc,

    /* U+58 "X" */
    0x44, 0x50, 0xa0, 0x83, 0x5, 0x11, 0x22,

    /* U+59 "Y" */
    0x44, 0x88, 0xa0, 0x81, 0x2, 0x4, 0x8,

    /* U+5A "Z" */
    0xfc, 0x30, 0x84, 0x21, 0xc, 0x3f,

    /* U+5B "[" */
    0xea, 0xaa, 0xb0,

    /* U+5C "\\" */
    0x91, 0x24, 0x89, 0x20,

    /* U+5D "]" */
    0xd5, 0x55, 0x70,

    /* U+5E "^" */
    0x31, 0xac, 0x40,

    /* U+5F "_" */
    0xf8,

    /* U+60 "`" */
    0x90,

    /* U+61 "a" */
    0xf0, 0x5f, 0x18, 0xbc,

    /* U+62 "b" */
    0x84, 0x3d, 0x18, 0xc6, 0x3e,

    /* U+63 "c" */
    0x7c, 0x88, 0xc7,

    /* U+64 "d" */
    0x8, 0x5f, 0x18, 0xc6, 0x2f,

    /* U+65 "e" */
    0x74, 0x7f, 0x8, 0x3c,

    /* U+66 "f" */
    0x74, 0xe4, 0x44, 0x44,

    /* U+67 "g" */
    0x7c, 0x63, 0x18, 0xbc, 0x2e,

    /* U+68 "h" */
    0x84, 0x2d, 0x98, 0xc6, 0x31,

    /* U+69 "i" */
    0xbf,

    /* U+6A "j" */
    0x45, 0x55, 0x70,

    /* U+6B "k" */
    0x84, 0x25, 0x4c, 0x72, 0xd2,

    /* U+6C "l" */
    0xff,

    /* U+6D "m" */
    0xb7, 0x64, 0x62, 0x31, 0x18, 0x8c, 0x44,

    /* U+6E "n" */
    0xb6, 0x63, 0x18, 0xc4,

    /* U+6F "o" */
    0x74, 0x63, 0x18, 0xb8,

    /* U+70 "p" */
    0xf4, 0x63, 0x18, 0xfa, 0x10,

    /* U+71 "q" */
    0x7c, 0x63, 0x18, 0xbc, 0x21,

    /* U+72 "r" */
    0xf2, 0x49, 0x0,

    /* U+73 "s" */
    0xf8, 0xc3, 0x1e,

    /* U+74 "t" */
    0x44, 0xf4, 0x44, 0x47,

    /* U+75 "u" */
    0x8c, 0x63, 0x18, 0xbc,

    /* U+76 "v" */
    0x89, 0x24, 0x94, 0x30, 0xc0,

    /* U+77 "w" */
    0x99, 0x5a, 0x5a, 0x5a, 0x66, 0x24,

    /* U+78 "x" */
    0x49, 0xc3, 0xc, 0x49, 0x20,

    /* U+79 "y" */
    0x89, 0x24, 0x8c, 0x30, 0xc2, 0x18,

    /* U+7A "z" */
    0xf8, 0x88, 0x88, 0x7c,

    /* U+7B "{" */
    0x32, 0x22, 0x2c, 0x22, 0x23,

    /* U+7C "|" */
    0xff, 0xe0,

    /* U+7D "}" */
    0xc4, 0x44, 0x43, 0x44, 0x4c,

    /* U+7E "~" */
    0x63, 0x38,

    /* U+F001 "" */
    0x0, 0x40, 0xf1, 0xfc, 0x7d, 0x18, 0x44, 0x11,
    0x4, 0x47, 0x71, 0xdc, 0x0,

    /* U+F008 "" */
    0xbf, 0x78, 0x7e, 0x1e, 0xfd, 0xe1, 0xe8, 0x5f,
    0xfc,

    /* U+F00B "" */
    0xef, 0xfb, 0xf0, 0x3, 0xbf, 0xef, 0xc0, 0xe,
    0xff, 0xbf,

    /* U+F00C "" */
    0x0, 0xc0, 0x64, 0x33, 0x98, 0x7c, 0xe, 0x1,
    0x0,

    /* U+F00D "" */
    0xc7, 0xdd, 0xf1, 0xc7, 0xdd, 0xf1, 0x80,

    /* U+F011 "" */
    0xc, 0xb, 0x46, 0xdb, 0x33, 0xcc, 0xf0, 0x3c,
    0x1d, 0x86, 0x3e, 0x0,

    /* U+F013 "" */
    0x8, 0xe, 0x1f, 0xff, 0xf6, 0x33, 0x1b, 0xdf,
    0xff, 0x5c, 0xe, 0x0,

    /* U+F015 "" */
    0x4, 0x83, 0x70, 0xd6, 0x37, 0x6d, 0xf6, 0x7f,
    0xe, 0xe1, 0xdc, 0x3b, 0x80,

    /* U+F019 "" */
    0xe, 0x1, 0xc0, 0x38, 0x1f, 0xc1, 0xf0, 0x1c,
    0x3d, 0x7f, 0xfd, 0xff, 0xe0,

    /* U+F01C "" */
    0x3f, 0x8c, 0x19, 0x1, 0x60, 0x3f, 0x1f, 0xff,
    0xff, 0xf8,

    /* U+F021 "" */
    0x0, 0x47, 0xd6, 0x1d, 0xf, 0x0, 0x0, 0xf,
    0xb, 0x6, 0xe3, 0x27, 0x80,

    /* U+F026 "" */
    0x8, 0xff, 0xff, 0xfc, 0x61,

    /* U+F027 "" */
    0x8, 0x18, 0xf8, 0xf9, 0xf9, 0xfa, 0x18, 0x8,

    /* U+F028 "" */
    0x8, 0x83, 0x2b, 0xe2, 0xfc, 0xdf, 0x9b, 0xf5,
    0x46, 0x70, 0x42, 0x0, 0x80,

    /* U+F03E "" */
    0xff, 0xe7, 0xf9, 0xef, 0xf1, 0xc8, 0x60, 0x1f,
    0xfc,

    /* U+F048 "" */
    0x86, 0x7b, 0xff, 0xff, 0xfb, 0xe7, 0x84,

    /* U+F04B "" */
    0xc0, 0x70, 0x3e, 0x1f, 0xcf, 0xff, 0xff, 0xfd,
    0xf8, 0xf0, 0x70, 0x0,

    /* U+F04C "" */
    0xf7, 0xfb, 0xfd, 0xfe, 0xff, 0x7f, 0xbf, 0xdf,
    0xef, 0xf7, 0x80,

    /* U+F04D "" */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0x80,

    /* U+F051 "" */
    0x87, 0x9f, 0x7f, 0xff, 0xff, 0x79, 0x84,

    /* U+F052 "" */
    0x18, 0x1e, 0x1f, 0x8f, 0xef, 0xf8, 0x3, 0xff,
    0xff,

    /* U+F053 "" */
    0x19, 0x99, 0x8c, 0x30, 0xc3,

    /* U+F054 "" */
    0x86, 0x18, 0x63, 0xb3, 0x10,

    /* U+F067 "" */
    0x8, 0x4, 0x2, 0x1, 0xf, 0xf8, 0x40, 0x20,
    0x10, 0x8, 0x0,

    /* U+F068 "" */
    0xff, 0xff, 0xc0,

    /* U+F06E "" */
    0x1f, 0x6, 0x31, 0x93, 0x73, 0x7e, 0xee, 0xdd,
    0x8c, 0x60, 0xf8,

    /* U+F070 "" */
    0xc0, 0x1, 0xde, 0x1, 0xc6, 0x3, 0x4c, 0x67,
    0xb9, 0xc6, 0xe3, 0xf, 0x6, 0x18, 0xf, 0x18,
    0x0, 0x30,

    /* U+F071 "" */
    0x4, 0x1, 0xc0, 0x3c, 0xd, 0x83, 0xb8, 0x77,
    0x1f, 0xf3, 0xdf, 0xfb, 0xff, 0xfc,

    /* U+F074 "" */
    0x1, 0xb8, 0xf3, 0x58, 0x30, 0x18, 0xd, 0xee,
    0x3c, 0x6,

    /* U+F077 "" */
    0x1c, 0x1f, 0x1d, 0xdc, 0x7c, 0x18,

    /* U+F078 "" */
    0xc1, 0xb1, 0x8d, 0x83, 0x80, 0x80,

    /* U+F079 "" */
    0x20, 0x3, 0x9f, 0x3e, 0x8, 0x40, 0x42, 0x2,
    0x10, 0x7c, 0xf9, 0xc0, 0x4,

    /* U+F07B "" */
    0xf8, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xfc,

    /* U+F093 "" */
    0xc, 0x7, 0x83, 0xf0, 0xf0, 0xc, 0x3, 0xe,
    0xdf, 0xfd, 0xff, 0xc0,

    /* U+F095 "" */
    0x1, 0x80, 0x70, 0x3c, 0x7, 0x1, 0x80, 0x62,
    0x33, 0xf8, 0xfc, 0x3e, 0x0,

    /* U+F0C4 "" */
    0x0, 0x79, 0xe5, 0xcf, 0xc1, 0xc3, 0xe3, 0xd9,
    0x26, 0x61, 0x0,

    /* U+F0C5 "" */
    0x1d, 0xe, 0xf7, 0x1b, 0xfd, 0xfe, 0xff, 0x7f,
    0xbf, 0xc0, 0x7e, 0x0,

    /* U+F0C7 "" */
    0xfe, 0x41, 0xa0, 0xf0, 0x7f, 0xff, 0x3f, 0x9f,
    0xff,

    /* U+F0E7 "" */
    0x73, 0xcf, 0x3f, 0xfc, 0x63, 0xc, 0x20,

    /* U+F0EA "" */
    0x10, 0x76, 0x3f, 0x1c, 0xe, 0xd7, 0x67, 0xbf,
    0xdf, 0xf, 0x87, 0xc0,

    /* U+F0F3 "" */
    0x8, 0xe, 0xf, 0x8f, 0xe7, 0xf3, 0xf9, 0xfd,
    0xff, 0x0, 0xe, 0x0,

    /* U+F11C "" */
    0xff, 0xf5, 0x57, 0xff, 0xea, 0xbf, 0xff, 0x41,
    0x7f, 0xf8,

    /* U+F124 "" */
    0x0, 0xc0, 0xf0, 0xf8, 0xfe, 0xff, 0xbf, 0xc0,
    0xf0, 0x38, 0xe, 0x3, 0x0,

    /* U+F15B "" */
    0xf4, 0xf6, 0xf1, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff,

    /* U+F1EB "" */
    0x1f, 0xc3, 0xef, 0xb0, 0xe, 0x3f, 0x3, 0xc,
    0x0, 0x0, 0x18, 0x0, 0xc0,

    /* U+F240 "" */
    0xff, 0xe8, 0x3, 0xbf, 0xdb, 0xfd, 0x80, 0x3f,
    0xff,

    /* U+F241 "" */
    0xff, 0xe8, 0x3, 0xbf, 0x1b, 0xf1, 0x80, 0x3f,
    0xff,

    /* U+F242 "" */
    0xff, 0xe8, 0x3, 0xbc, 0x1b, 0xc1, 0x80, 0x3f,
    0xff,

    /* U+F243 "" */
    0xff, 0xe8, 0x3, 0xb0, 0x1b, 0x1, 0x80, 0x3f,
    0xff,

    /* U+F244 "" */
    0xff, 0xf8, 0x3, 0x80, 0x18, 0x1, 0x80, 0x3f,
    0xff,

    /* U+F287 "" */
    0x1, 0x0, 0x3c, 0x2, 0x1, 0xd0, 0x2f, 0xff,
    0xf2, 0x8, 0xb, 0x0, 0x38,

    /* U+F293 "" */
    0x3c, 0x76, 0xf3, 0xdb, 0xe7, 0xe7, 0xdb, 0xf3,
    0x76, 0x3c,

    /* U+F2ED "" */
    0x1c, 0x7f, 0xc0, 0xf, 0xe5, 0x52, 0xa9, 0x54,
    0xaa, 0x55, 0x3f, 0x80,

    /* U+F304 "" */
    0x1, 0x80, 0xf0, 0x5c, 0x3a, 0x1f, 0xf, 0x87,
    0xc3, 0xe0, 0xf0, 0x38, 0x0,

    /* U+F55A "" */
    0x1f, 0xf9, 0xed, 0xdf, 0xf, 0xfc, 0xf7, 0xc3,
    0x9e, 0xdc, 0x7f, 0xe0,

    /* U+F7C2 "" */
    0x3e, 0xb7, 0x6f, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xfc,

    /* U+F8A2 "" */
    0x0, 0x48, 0x36, 0xf, 0xff, 0x60, 0x8, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 51, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 64, .box_w = 1, .box_h = 8, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 2, .adv_w = 74, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 4, .adv_w = 134, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 11, .adv_w = 102, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 18, .adv_w = 152, .box_w = 9, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 27, .adv_w = 125, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 34, .adv_w = 44, .box_w = 1, .box_h = 3, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 35, .adv_w = 62, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 38, .adv_w = 62, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 41, .adv_w = 80, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 45, .adv_w = 134, .box_w = 7, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 51, .adv_w = 51, .box_w = 1, .box_h = 2, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 52, .adv_w = 58, .box_w = 3, .box_h = 1, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 53, .adv_w = 51, .box_w = 1, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 54, .adv_w = 54, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 58, .adv_w = 102, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 63, .adv_w = 102, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 68, .adv_w = 102, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 73, .adv_w = 102, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 78, .adv_w = 102, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 84, .adv_w = 102, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 89, .adv_w = 102, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 94, .adv_w = 102, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 99, .adv_w = 102, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 104, .adv_w = 102, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 109, .adv_w = 54, .box_w = 1, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 110, .adv_w = 54, .box_w = 1, .box_h = 7, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 111, .adv_w = 134, .box_w = 7, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 117, .adv_w = 134, .box_w = 6, .box_h = 3, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 120, .adv_w = 134, .box_w = 7, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 126, .adv_w = 85, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 130, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 142, .adv_w = 109, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 149, .adv_w = 110, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 154, .adv_w = 112, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 160, .adv_w = 123, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 166, .adv_w = 101, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 171, .adv_w = 92, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 175, .adv_w = 124, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 181, .adv_w = 120, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 187, .adv_w = 47, .box_w = 1, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 188, .adv_w = 47, .box_w = 3, .box_h = 10, .ofs_x = -1, .ofs_y = -2},
    {.bitmap_index = 192, .adv_w = 105, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 198, .adv_w = 89, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 203, .adv_w = 138, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 210, .adv_w = 120, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 216, .adv_w = 126, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 223, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 228, .adv_w = 126, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 236, .adv_w = 111, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 242, .adv_w = 102, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 247, .adv_w = 98, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 254, .adv_w = 117, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 260, .adv_w = 109, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 267, .adv_w = 158, .box_w = 10, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 277, .adv_w = 110, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 284, .adv_w = 98, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 291, .adv_w = 110, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 297, .adv_w = 62, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 300, .adv_w = 54, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 304, .adv_w = 62, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 307, .adv_w = 134, .box_w = 6, .box_h = 3, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 310, .adv_w = 80, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 311, .adv_w = 80, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 312, .adv_w = 98, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 316, .adv_w = 102, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 321, .adv_w = 88, .box_w = 4, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 324, .adv_w = 102, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 329, .adv_w = 98, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 333, .adv_w = 56, .box_w = 4, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 337, .adv_w = 102, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 342, .adv_w = 101, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 347, .adv_w = 44, .box_w = 1, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 348, .adv_w = 44, .box_w = 2, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 351, .adv_w = 93, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 356, .adv_w = 44, .box_w = 1, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 357, .adv_w = 156, .box_w = 9, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 364, .adv_w = 101, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 368, .adv_w = 98, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 372, .adv_w = 102, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 377, .adv_w = 102, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 382, .adv_w = 66, .box_w = 3, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 385, .adv_w = 83, .box_w = 4, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 388, .adv_w = 63, .box_w = 4, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 392, .adv_w = 101, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 396, .adv_w = 95, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 401, .adv_w = 131, .box_w = 8, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 407, .adv_w = 95, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 412, .adv_w = 95, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 418, .adv_w = 84, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 422, .adv_w = 102, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 427, .adv_w = 54, .box_w = 1, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 429, .adv_w = 102, .box_w = 4, .box_h = 10, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 434, .adv_w = 134, .box_w = 7, .box_h = 2, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 436, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 449, .adv_w = 160, .box_w = 10, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 458, .adv_w = 160, .box_w = 10, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 468, .adv_w = 160, .box_w = 10, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 477, .adv_w = 110, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 484, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 496, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 508, .adv_w = 180, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 521, .adv_w = 160, .box_w = 11, .box_h = 9, .ofs_x = -1, .ofs_y = -1},
    {.bitmap_index = 534, .adv_w = 180, .box_w = 11, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 544, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 557, .adv_w = 80, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 562, .adv_w = 120, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 570, .adv_w = 180, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 583, .adv_w = 160, .box_w = 10, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 592, .adv_w = 140, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 599, .adv_w = 140, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 611, .adv_w = 140, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 622, .adv_w = 140, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 633, .adv_w = 140, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 640, .adv_w = 140, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 649, .adv_w = 100, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 654, .adv_w = 100, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 659, .adv_w = 140, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 670, .adv_w = 140, .box_w = 9, .box_h = 2, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 673, .adv_w = 180, .box_w = 11, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 684, .adv_w = 200, .box_w = 14, .box_h = 10, .ofs_x = -1, .ofs_y = -1},
    {.bitmap_index = 702, .adv_w = 180, .box_w = 11, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 716, .adv_w = 160, .box_w = 10, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 726, .adv_w = 140, .box_w = 9, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 732, .adv_w = 140, .box_w = 9, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 738, .adv_w = 200, .box_w = 13, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 751, .adv_w = 160, .box_w = 10, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 760, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 772, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 785, .adv_w = 140, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 796, .adv_w = 140, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 808, .adv_w = 140, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 817, .adv_w = 100, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 824, .adv_w = 140, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 836, .adv_w = 140, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 848, .adv_w = 180, .box_w = 11, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 858, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 871, .adv_w = 120, .box_w = 8, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 881, .adv_w = 200, .box_w = 13, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 894, .adv_w = 200, .box_w = 12, .box_h = 6, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 903, .adv_w = 200, .box_w = 12, .box_h = 6, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 912, .adv_w = 200, .box_w = 12, .box_h = 6, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 921, .adv_w = 200, .box_w = 12, .box_h = 6, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 930, .adv_w = 200, .box_w = 12, .box_h = 6, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 939, .adv_w = 200, .box_w = 13, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 952, .adv_w = 140, .box_w = 8, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 962, .adv_w = 140, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 974, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 987, .adv_w = 200, .box_w = 13, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 999, .adv_w = 120, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1008, .adv_w = 161, .box_w = 10, .box_h = 6, .ofs_x = 0, .ofs_y = 1}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_1[] = {
    0x0, 0x7, 0xa, 0xb, 0xc, 0x10, 0x12, 0x14,
    0x18, 0x1b, 0x20, 0x25, 0x26, 0x27, 0x3d, 0x47,
    0x4a, 0x4b, 0x4c, 0x50, 0x51, 0x52, 0x53, 0x66,
    0x67, 0x6d, 0x6f, 0x70, 0x73, 0x76, 0x77, 0x78,
    0x7a, 0x92, 0x94, 0xc3, 0xc4, 0xc6, 0xe6, 0xe9,
    0xf2, 0x11b, 0x123, 0x15a, 0x1ea, 0x23f, 0x240, 0x241,
    0x242, 0x243, 0x286, 0x292, 0x2ec, 0x303, 0x559, 0x7c1,
    0x8a1
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 61441, .range_length = 2210, .glyph_id_start = 96,
        .unicode_list = unicode_list_1, .glyph_id_ofs_list = NULL, .list_length = 57, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

/*Store all the custom data of the font*/
static lv_font_fmt_txt_dsc_t font_dsc = {
    .glyph_bitmap = gylph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 2,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0
};


/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
lv_font_t lv_font_dejavu_10 = {
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 12,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
    .dsc = &font_dsc           /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
};

#endif /*#if LV_FONT_DEJAVU_10*/

