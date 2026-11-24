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
#define KID_COL_LOW      0xFF9A3C // low battery only

#define KID_LOW_BATTERY_PCT 10

// A 5x7 pixel font drawn at a whole-number scale (1 to 8), so letters stay crisp.
// Lowercase letters have their own shapes (with descenders), so game names read easily.
const lv_font_t *kid_font(int scale);

// Paints the boot-screen background (navy to teal gradient with a faint pixel dot grid) on obj.
void kid_style_background(lv_obj_t *obj);

// Restyles the brightness and volume pop-up bars (square, navy panel, green fill).
void kid_style_progress_bars(void);

// Pixel battery: a panel with four bars and a nub, plus a lightning bolt while charging.
// Scale 1 is 60x32 px (bolt adds 27 px to the right). Call kid_battery_set to update it.
lv_obj_t *kid_battery_create(lv_obj_t *parent, int scale);
void kid_battery_set(lv_obj_t *battery, int percent, int charging);

// Battery level from the muOS battery daemon, and whether it counts as low (and not charging).
int kid_battery_percent(void);
int kid_battery_charging(void);
int kid_battery_low(void);
