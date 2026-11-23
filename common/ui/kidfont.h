#pragma once

#include <lvgl/lvgl.h>

// Kid mode theme: the blocky pixel lettering and colours of the THEO'S GAMEBOY boot logo.

#define KID_COL_BG       0x0A285A
#define KID_COL_BG_LOW   0x085A78
#define KID_COL_PANEL    0x081F45
#define KID_COL_GREEN    0x78E650
#define KID_COL_BLUE     0x8CD2FF
#define KID_COL_SEA      0x46967D
#define KID_COL_SHADOW   0x0A1E28
#define KID_COL_WHITE    0xF2F7FF

// A 5x7 pixel font drawn at a whole-number scale (1 to 8), so letters stay crisp.
// Lowercase letters use the capital shapes, like the logo.
const lv_font_t *kid_font(int scale);

// Paints the boot-screen background (navy to teal gradient with a faint pixel dot grid) on obj.
void kid_style_background(lv_obj_t *obj);
