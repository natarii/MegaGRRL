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
    0x63, 0x38
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
    {.bitmap_index = 434, .adv_w = 134, .box_w = 7, .box_h = 2, .ofs_x = 1, .ofs_y = 3}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
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
    .cmap_num = 1,
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

