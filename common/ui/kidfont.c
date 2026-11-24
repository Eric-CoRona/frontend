#include <string.h>
#include <common/ui/kidfont.h>
#include <common/ui/common.h>
#include <common/platform/battery.h>
#include <common/storage/fileio.h>

#define KID_GLYPH_W 5
#define KID_GLYPH_H 7
#define KID_MAX_SCALE 8

static const uint8_t kid_glyphs[95][KID_GLYPH_H] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ' ' */
    {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}, /* '!' */
    {0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00}, /* '"' */
    {0x0A, 0x1F, 0x0A, 0x0A, 0x0A, 0x1F, 0x0A}, /* '#' */
    {0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04}, /* '$' */
    {0x19, 0x1A, 0x02, 0x04, 0x08, 0x0B, 0x13}, /* '%' */
    {0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D}, /* '&' */
    {0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00}, /* "'" */
    {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}, /* '(' */
    {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}, /* ')' */
    {0x00, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0x00}, /* '*' */
    {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}, /* '+' */
    {0x00, 0x00, 0x00, 0x00, 0x06, 0x04, 0x08}, /* ',' */
    {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}, /* '-' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06}, /* '.' */
    {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10}, /* '/' */
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, /* '0' */
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* '1' */
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, /* '2' */
    {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}, /* '3' */
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, /* '4' */
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, /* '5' */
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, /* '6' */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, /* '7' */
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, /* '8' */
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}, /* '9' */
    {0x00, 0x06, 0x06, 0x00, 0x06, 0x06, 0x00}, /* ':' */
    {0x00, 0x06, 0x06, 0x00, 0x06, 0x04, 0x08}, /* ';' */
    {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02}, /* '<' */
    {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00}, /* '=' */
    {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08}, /* '>' */
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04}, /* '?' */
    {0x0E, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0E}, /* '@' */
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* 'A' */
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, /* 'B' */
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, /* 'C' */
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, /* 'D' */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, /* 'E' */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, /* 'F' */
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}, /* 'G' */
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* 'H' */
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* 'I' */
    {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}, /* 'J' */
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, /* 'K' */
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, /* 'L' */
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, /* 'M' */
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, /* 'N' */
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* 'O' */
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, /* 'P' */
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, /* 'Q' */
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, /* 'R' */
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}, /* 'S' */
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, /* 'T' */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* 'U' */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, /* 'V' */
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}, /* 'W' */
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, /* 'X' */
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, /* 'Y' */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, /* 'Z' */
    {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E}, /* '[' */
    {0x10, 0x08, 0x08, 0x04, 0x02, 0x02, 0x01}, /* backslash */
    {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E}, /* ']' */
    {0x04, 0x0A, 0x11, 0x00, 0x00, 0x00, 0x00}, /* '^' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F}, /* '_' */
    {0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00}, /* '`' */
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* 'a' */
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, /* 'b' */
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, /* 'c' */
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, /* 'd' */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, /* 'e' */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, /* 'f' */
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}, /* 'g' */
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* 'h' */
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* 'i' */
    {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}, /* 'j' */
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, /* 'k' */
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, /* 'l' */
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, /* 'm' */
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, /* 'n' */
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* 'o' */
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, /* 'p' */
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, /* 'q' */
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, /* 'r' */
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}, /* 's' */
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, /* 't' */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* 'u' */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, /* 'v' */
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}, /* 'w' */
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, /* 'x' */
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, /* 'y' */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, /* 'z' */
    {0x02, 0x04, 0x04, 0x08, 0x04, 0x04, 0x02}, /* '{' */
    {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, /* '|' */
    {0x08, 0x04, 0x04, 0x02, 0x04, 0x04, 0x08}, /* '}' */
    {0x00, 0x00, 0x08, 0x15, 0x02, 0x00, 0x00}, /* '~' */
};

// Lowercase letters (easier to read than all capitals): 5 wide, 9 tall. Rows 0-6 sit on the
// baseline like the capitals, rows 7-8 hang below it for g, j, p, q and y.
#define KID_LOWER_H 9
static const uint8_t kid_lower[26][KID_LOWER_H] = {
    {0x00, 0x00, 0x0E, 0x01, 0x0F, 0x11, 0x0F, 0x00, 0x00}, /* 'a' */
    {0x10, 0x10, 0x16, 0x19, 0x11, 0x11, 0x1E, 0x00, 0x00}, /* 'b' */
    {0x00, 0x00, 0x0E, 0x10, 0x10, 0x11, 0x0E, 0x00, 0x00}, /* 'c' */
    {0x01, 0x01, 0x0D, 0x13, 0x11, 0x11, 0x0F, 0x00, 0x00}, /* 'd' */
    {0x00, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E, 0x00, 0x00}, /* 'e' */
    {0x06, 0x09, 0x08, 0x1C, 0x08, 0x08, 0x08, 0x00, 0x00}, /* 'f' */
    {0x00, 0x00, 0x0F, 0x11, 0x11, 0x0F, 0x01, 0x11, 0x0E}, /* 'g' */
    {0x10, 0x10, 0x16, 0x19, 0x11, 0x11, 0x11, 0x00, 0x00}, /* 'h' */
    {0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x0E, 0x00, 0x00}, /* 'i' */
    {0x02, 0x00, 0x06, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}, /* 'j' */
    {0x10, 0x10, 0x12, 0x14, 0x18, 0x14, 0x12, 0x00, 0x00}, /* 'k' */
    {0x0C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E, 0x00, 0x00}, /* 'l' */
    {0x00, 0x00, 0x1A, 0x15, 0x15, 0x15, 0x15, 0x00, 0x00}, /* 'm' */
    {0x00, 0x00, 0x16, 0x19, 0x11, 0x11, 0x11, 0x00, 0x00}, /* 'n' */
    {0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E, 0x00, 0x00}, /* 'o' */
    {0x00, 0x00, 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, /* 'p' */
    {0x00, 0x00, 0x0F, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x01}, /* 'q' */
    {0x00, 0x00, 0x16, 0x19, 0x10, 0x10, 0x10, 0x00, 0x00}, /* 'r' */
    {0x00, 0x00, 0x0F, 0x10, 0x0E, 0x01, 0x1E, 0x00, 0x00}, /* 's' */
    {0x08, 0x08, 0x1C, 0x08, 0x08, 0x09, 0x06, 0x00, 0x00}, /* 't' */
    {0x00, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0D, 0x00, 0x00}, /* 'u' */
    {0x00, 0x00, 0x11, 0x11, 0x11, 0x0A, 0x04, 0x00, 0x00}, /* 'v' */
    {0x00, 0x00, 0x11, 0x11, 0x15, 0x15, 0x0A, 0x00, 0x00}, /* 'w' */
    {0x00, 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x00, 0x00}, /* 'x' */
    {0x00, 0x00, 0x11, 0x11, 0x11, 0x0F, 0x01, 0x11, 0x0E}, /* 'y' */
    {0x00, 0x00, 0x1F, 0x02, 0x04, 0x08, 0x1F, 0x00, 0x00}, /* 'z' */
};

static lv_font_t kid_fonts[KID_MAX_SCALE + 1];
static uint8_t kid_bitmap[KID_GLYPH_W * KID_MAX_SCALE * KID_LOWER_H * KID_MAX_SCALE];

// Accented Latin letters (U+00C0..U+00FF) drawn as their plain letter, so "Pokémon" reads POKEMON.
static uint32_t fold_accent(const uint32_t c) {
    static const char latin1[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPsaaaaaaaceeeeiiiidnooooo/ouuuuypy";
    if (c >= 0xC0 && c <= 0xFF) return (uint32_t) (unsigned char) latin1[c - 0xC0];
    if (c == 0x2018 || c == 0x2019) return '\'';
    if (c == 0x201C || c == 0x201D) return '"';
    if (c == 0x2013 || c == 0x2014) return '-';
    if (c == 0x2026) return '.';
    return c;
}

static int is_lower(uint32_t letter) {
    letter = fold_accent(letter);
    return letter >= 'a' && letter <= 'z';
}

static int glyph_index(uint32_t letter) {
    letter = fold_accent(letter);
    if (letter >= 'a' && letter <= 'z') letter -= 'a' - 'A';
    if (letter < 32 || letter > 126) return '?' - 32;
    return (int) letter - 32;
}

static int font_scale(const lv_font_t *font) {
    return (int) (intptr_t) font->dsc;
}

static int kid_get_glyph_dsc(
    const lv_font_t *font, lv_font_glyph_dsc_t *dsc, const uint32_t letter, const uint32_t letter_next
) {
    LV_UNUSED(letter_next);
    const int s = font_scale(font);

    dsc->resolved_font = font;
    dsc->adv_w = (uint16_t) ((KID_GLYPH_W + 1) * s);
    dsc->box_w = (uint16_t) (KID_GLYPH_W * s);
    dsc->box_h = (uint16_t) (KID_GLYPH_H * s);
    dsc->ofs_x = 0;
    dsc->ofs_y = 0;
    if (is_lower(letter)) {
        dsc->box_h = (uint16_t) (KID_LOWER_H * s);
        dsc->ofs_y = (int16_t) (-(KID_LOWER_H - KID_GLYPH_H) * s);
    }
    dsc->bpp = 8;
    dsc->is_placeholder = 0;

    // Characters like the zero-width joiner or newline get no space.
    if (letter == '\n' || letter == '\r' || letter == 0x200B) {
        dsc->adv_w = dsc->box_w = dsc->box_h = 0;
    }

    return 1;
}

static const uint8_t *kid_get_glyph_bitmap(const lv_font_t *font, const uint32_t letter) {
    const int s = font_scale(font);
    const int lower = is_lower(letter);
    const uint8_t *rows = lower ? kid_lower[fold_accent(letter) - 'a'] : kid_glyphs[glyph_index(letter)];
    const int h = lower ? KID_LOWER_H : KID_GLYPH_H;
    const int w = KID_GLYPH_W * s;

    for (int y = 0; y < h * s; y++) {
        const uint8_t bits = rows[y / s];
        for (int x = 0; x < w; x++) {
            const int on = (bits >> (KID_GLYPH_W - 1 - x / s)) & 1;
            kid_bitmap[y * w + x] = on ? 0xFF : 0x00;
        }
    }

    return kid_bitmap;
}

const lv_font_t *kid_font(int scale) {
    if (scale < 1) scale = 1;
    if (scale > KID_MAX_SCALE) scale = KID_MAX_SCALE;

    lv_font_t *f = &kid_fonts[scale];
    if (!f->get_glyph_dsc) {
        memset(f, 0, sizeof(*f));
        f->get_glyph_dsc = kid_get_glyph_dsc;
        f->get_glyph_bitmap = kid_get_glyph_bitmap;
        f->line_height = (lv_coord_t) ((KID_GLYPH_H + 3) * scale);
        f->base_line = (lv_coord_t) (2 * scale);
        f->subpx = LV_FONT_SUBPX_NONE;
        f->underline_position = (int8_t) -scale;
        f->underline_thickness = (int8_t) scale;
        f->dsc = (const void *) (intptr_t) scale;
    }

    return f;
}

static void kid_bg_draw_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);

    lv_area_t a;
    lv_obj_get_coords(obj, &a);

    lv_draw_rect_dsc_t dot;
    lv_draw_rect_dsc_init(&dot);
    dot.bg_color = lv_color_hex(KID_COL_BLUE);
    dot.bg_opa = LV_OPA_10;

    // Only draw the dots inside the area LVGL is redrawing (often a small strip).
    const lv_area_t *clip = draw_ctx->clip_area;
    lv_coord_t y0 = a.y1 + 8, x0 = a.x1 + 8;
    if (clip->y1 > y0) y0 += ((clip->y1 - y0 + 15) / 16) * 16;
    if (clip->x1 > x0) x0 += ((clip->x1 - x0 + 15) / 16) * 16;
    const lv_coord_t y_end = LV_MIN(a.y2, clip->y2 + 1);
    const lv_coord_t x_end = LV_MIN(a.x2, clip->x2 + 1);

    for (lv_coord_t y = y0; y < y_end; y += 16) {
        for (lv_coord_t x = x0; x < x_end; x += 16) {
            const lv_area_t d = {x, y, (lv_coord_t) (x + 1), (lv_coord_t) (y + 1)};
            lv_draw_rect(draw_ctx, &dot, &d);
        }
    }
}

void kid_style_background(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(KID_COL_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(obj, lv_color_hex(KID_COL_BG_LOW), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(obj, kid_bg_draw_cb, LV_EVENT_DRAW_MAIN_END, NULL);
}

static void kid_style_bar(lv_obj_t *panel, lv_obj_t *icon, lv_obj_t *bar, const uint32_t icon_col, const uint32_t bar_col) {
    if (panel) {
        lv_obj_set_style_radius(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(panel, lv_color_hex(KID_COL_PANEL), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(panel, lv_color_hex(KID_COL_SEA), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(panel, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(panel, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (icon) {
        lv_obj_set_style_img_recolor(icon, lv_color_hex(icon_col), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (bar) {
        lv_obj_set_style_radius(bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bar, lv_color_hex(KID_COL_SHADOW), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bar, lv_color_hex(bar_col), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_dir(bar, LV_GRAD_DIR_NONE, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }
}

// Brightness and volume use opposite colours so they are easy to tell apart.
void kid_style_progress_bars(void) {
    kid_style_bar(
        ui_pnl_progress_brightness, ui_ico_progress_brightness, ui_bar_progress_brightness, KID_COL_GREEN, KID_COL_BLUE
    );
    kid_style_bar(ui_pnl_progress_volume, ui_ico_progress_volume, ui_bar_progress_volume, KID_COL_BLUE, KID_COL_GREEN);
}

// ---- Pixel battery ----------------------------------------------------------------------

#define KID_BAT_W 60
#define KID_BAT_H 32
#define KID_BAT_SEGS 4

static const char *kid_bolt[6] = {"00110", "01100", "11111", "00110", "01100", "11000"};

static lv_obj_t *kid_box(lv_obj_t *parent, const int x, const int y, const int w, const int h) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    return o;
}

// Children of the returned container: 0 = body (bars inside it), 1 = nub, 2 = bolt.
lv_obj_t *kid_battery_create(lv_obj_t *parent, int scale) {
    if (scale < 1) scale = 1;
    const int s = scale;
    const int w = KID_BAT_W * s, h = KID_BAT_H * s;

    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(root, w + 6 * s + 6 + 27 * s, h + 4);

    lv_obj_t *body = kid_box(root, 0, 0, w, h);
    lv_obj_set_style_bg_color(body, lv_color_hex(KID_COL_PANEL), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(body, 3 * s, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(body, lv_color_hex(KID_COL_SEA), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(body, lv_color_hex(KID_COL_SHADOW), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_x(body, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_y(body, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(body, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    const int gap = 3 * s;
    const int seg_w = (w - 10 * s - (KID_BAT_SEGS - 1) * gap) / KID_BAT_SEGS;
    for (int i = 0; i < KID_BAT_SEGS; i++) {
        kid_box(body, 2 * s + i * (seg_w + gap), 2 * s, seg_w, h - 11 * s);
    }

    kid_box(root, w, h / 2 - 6 * s, 6 * s, 12 * s);

    lv_obj_t *bolt = lv_obj_create(root);
    lv_obj_remove_style_all(bolt);
    lv_obj_clear_flag(bolt, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(bolt, w + 12 * s, h / 2 - 9 * s);
    lv_obj_set_size(bolt, 15 * s, 18 * s);
    for (int r = 0; r < 6; r++) {
        for (int c = 0; c < 5; c++) {
            if (kid_bolt[r][c] != '1') continue;
            lv_obj_t *px = kid_box(bolt, c * 3 * s, r * 3 * s, 3 * s, 3 * s);
            lv_obj_set_style_bg_color(px, lv_color_hex(KID_COL_BLUE), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
    lv_obj_add_flag(bolt, LV_OBJ_FLAG_HIDDEN);

    kid_battery_set(root, 100, 0);  // real level set by the caller
    return root;
}

void kid_battery_set(lv_obj_t *battery, int percent, const int charging) {
    if (!battery || !lv_obj_is_valid(battery)) return;
    if (percent < 0) percent = 100; // no reading yet: show full rather than a false "low"
    if (percent > 100) percent = 100;

    lv_obj_t *body = lv_obj_get_child(battery, 0);
    lv_obj_t *nub = lv_obj_get_child(battery, 1);
    lv_obj_t *bolt = lv_obj_get_child(battery, 2);

    const int low = percent <= KID_LOW_BATTERY_PCT && !charging;
    int segs = (percent + 24) / 25;
    if (segs < 1) segs = 1;

    const uint32_t edge = low ? KID_COL_LOW : KID_COL_SEA;
    const uint32_t fill = charging ? KID_COL_BLUE : (low ? KID_COL_LOW : KID_COL_GREEN);

    lv_obj_set_style_border_color(body, lv_color_hex(edge), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(nub, lv_color_hex(edge), LV_PART_MAIN | LV_STATE_DEFAULT);

    for (int i = 0; i < KID_BAT_SEGS; i++) {
        lv_obj_t *seg = lv_obj_get_child(body, i);
        lv_obj_set_style_bg_color(seg, lv_color_hex(fill), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (i < segs) {
            lv_obj_clear_flag(seg, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(seg, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (charging) {
        lv_obj_clear_flag(bolt, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(bolt, LV_OBJ_FLAG_HIDDEN);
    }
}

// -1 until the battery daemon has written a reading (so nothing looks "low" at boot).
int kid_battery_percent(void) {
    if (!file_exist("/run/muos/battery/capacity")) return -1;
    return battery_get_capacity();
}

int kid_battery_charging(void) {
    return battery_is_charging();
}

int kid_battery_low(void) {
    const int pct = kid_battery_percent();
    return pct >= 0 && !battery_is_charging() && pct <= KID_LOW_BATTERY_PCT;
}
