// Kid mode game picker: a single-row carousel of the games on the SD card.
//
// Left/Right picks a game, A plays it, L2/R2 change brightness. Holding
// Select+Start+L1+R1 for 3 seconds leaves kid mode until the next reboot.

#include "muxshare.h"
#include <common/config/kidmode.h>
#include <common/ui/kidfont.h>

#define KID_MAX_GAMES  512
#define KID_SCAN_DEPTH 3
#define KID_IDX_FILE   "/tmp/kid_idx"
#define KID_PARENT_MS  3000
#define KID_GBA_CAT    "Nintendo Game Boy Advance"
#define KID_BAT_POLL_MS   5000


typedef struct {
    char path[PATH_MAX];
    char name[256];
    int art_known; // cover size looked up yet?
    int art_w;     // cover size at the middle tile's scale (0 = no cover)
    int art_h;
} kid_game;

static kid_game *games;
static int game_count;
static int current_game;
static int exit_status;

static uint32_t parent_hold_start;
static int parent_fired;

enum { slot_prev, slot_cur, slot_next, slot_count };

static lv_obj_t *tile[slot_count];
static lv_obj_t *tile_img[slot_count];
static lv_obj_t *tile_lbl[slot_count];
static int tile_game[slot_count]; // game loaded into each tile object (-1 = none), so a slide reuses it

#define KID_SLIDE_MS 170
static int tile_y;
static int tile_x[slot_count];
static int slide_from_x[slot_count], slide_from_w[slot_count], slide_from_h[slot_count];
static int slide_running;
static int slide_var; // animation handle (address only)
static lv_obj_t *lbl_name;
static lv_obj_t *lbl_name_shadow;
static lv_obj_t *lbl_hint;
static lv_obj_t *lbl_count;
static lv_obj_t *lbl_empty;
static lv_obj_t *btn_power;
static lv_obj_t *lbl_power;
static lv_obj_t *pnl_confirm;
static lv_obj_t *btn_no;
static lv_obj_t *btn_yes;
static lv_obj_t *lbl_no;
static lv_obj_t *lbl_yes;
static lv_obj_t *arr_power;
static lv_obj_t *bat_icon;
static lv_obj_t *pnl_low;
static lv_obj_t *msg_panel;
static int confirm_yes;
static kid_low_warn low_warn;

static lv_obj_t *pnl_msg;
static lv_obj_t *lbl_msg;

static int power_focus;
static int confirm_open;
static int msg_open;
static uint32_t last_bright_ms;

static int has_ext(const char *name, const char *ext) {
    const char *dot = strrchr(name, '.');
    return dot && strcasecmp(dot + 1, ext) == 0;
}

static int is_gba_dir(const char *name) {
    return strcasestr(name, "gba") || strcasestr(name, "advance");
}

static int is_game_file(const char *name, const int in_gba_dir) {
    if (name[0] == '.') return 0;
    if (has_ext(name, "gba")) return 1;
    return in_gba_dir && (has_ext(name, "zip") || has_ext(name, "7z"));
}

// "Pokemon - Emerald Version (USA, Europe) [!].gba" -> "Pokemon - Emerald Version"
static void make_display_name(const char *file, char *out, const size_t out_size) {
    snprintf(out, out_size, "%s", file);

    char *dot = strrchr(out, '.');
    if (dot) *dot = '\0';

    char *cut = out;
    while ((cut = strpbrk(cut, "([")) != NULL) {
        if (cut > out) {
            *cut = '\0';
            break;
        }
        cut++;
    }

    size_t len = strlen(out);
    while (len > 0 && (out[len - 1] == ' ' || out[len - 1] == '_' || out[len - 1] == '-'))
        out[--len] = '\0';

    for (char *p = out; *p; p++)
        if (*p == '_') *p = ' ';

    if (!out[0]) snprintf(out, out_size, "%s", file);
}

static int game_known(const char *real) {
    for (int i = 0; i < game_count; i++)
        if (strcmp(games[i].path, real) == 0) return 1;
    return 0;
}

// Walks a folder (and up to KID_SCAN_DEPTH levels below it) for GBA games.
static void add_games_from(const char *dir, const int in_gba_dir, const int depth) {
    DIR *d = opendir(dir);
    if (!d) return;

    const struct dirent *ent;
    while ((ent = readdir(d)) != NULL && game_count < KID_MAX_GAMES) {
        if (ent->d_name[0] == '.') continue;

        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
        remove_double_slashes(full);

        if (dir_exist(full)) {
            if (depth < KID_SCAN_DEPTH) add_games_from(full, in_gba_dir || is_gba_dir(ent->d_name), depth + 1);
            continue;
        }

        if (!is_game_file(ent->d_name, in_gba_dir)) continue;
        if (game_known(full)) continue;

        kid_game *g = &games[game_count++];
        snprintf(g->path, sizeof(g->path), "%s", full);
        make_display_name(ent->d_name, g->name, sizeof(g->name));
        g->art_known = 0;
        g->art_w = g->art_h = 0;
    }

    closedir(d);
}

static int game_cmp(const void *a, const void *b) {
    return strcasecmp(((const kid_game *) a)->name, ((const kid_game *) b)->name);
}

// Games come from the folder named on the first line of the kid mode flag file,
// or else from GBA games anywhere under the ROMS folder. Only the first ROMS
// folder that exists is used, so union mounts don't list a game twice.
static void scan_games(void) {
    if (!games) games = calloc(KID_MAX_GAMES, sizeof(kid_game));
    game_count = 0;
    if (!games) return;

    char *custom = read_line_char_from(KID_MODE_FLAG, 1);
    if (custom && custom[0] && dir_exist(custom)) {
        add_games_from(custom, 1, 0);
    } else {
        char roots[5][PATH_MAX];
        union_get_roms_root(roots[0], sizeof(roots[0]));
        snprintf(roots[1], sizeof(roots[1]), "/mnt/union/" MAIN_ROM_DIR);
        snprintf(roots[2], sizeof(roots[2]), "/mnt/mmc/" MAIN_ROM_DIR);
        snprintf(roots[3], sizeof(roots[3]), "/mnt/sdcard/" MAIN_ROM_DIR);
        snprintf(roots[4], sizeof(roots[4]), "/mnt/usb/" MAIN_ROM_DIR);

        for (size_t i = 0; i < A_SIZE(roots); i++) {
            if (!dir_exist(roots[i])) continue;
            add_games_from(roots[i], 0, 0);
            LOG_INFO(mux_module, "Kid picker scanned %s: %d games", roots[i], game_count);
            if (game_count > 0) break;
        }

        // Nothing in GBA-named folders: accept zipped games in any folder too.
        for (size_t i = 0; i < A_SIZE(roots) && game_count == 0; i++) {
            if (dir_exist(roots[i])) add_games_from(roots[i], 1, 0);
        }
    }
    free(custom);

    qsort(games, (size_t) game_count, sizeof(kid_game), game_cmp);
}

static int box_art_path(const kid_game *g, char *out, const size_t out_size) {
    char catalogue[MAX_BUFFER_SIZE] = {0};
    get_catalogue_name_for_content(g->path, catalogue, sizeof(catalogue));

    char *base = strip_ext(get_file_name(g->path));
    const char *cats[] = {catalogue, KID_GBA_CAT};

    int found = 0;
    for (size_t i = 0; i < A_SIZE(cats) && !found; i++) {
        if (!cats[i][0]) continue;
        snprintf(out, out_size, "%s/%s/box/%s.png", INFO_CAT_PATH, cats[i], base);
        found = file_exist(out);
    }

    free(base);
    return found;
}

static int wrap_index(const int i) {
    if (game_count <= 0) return 0;
    return ((i % game_count) + game_count) % game_count;
}

static int tile_w[slot_count];
static int tile_h[slot_count];

// Covers are always loaded at the middle tile's size; side tiles show them zoomed down,
// so a tile can slide between positions without reloading its picture.
#define KID_BORDER_CUR  5
#define KID_BORDER_SIDE 3

static int tgt_w[slot_count]; // where each tile is heading: its own cover's shape
static int tgt_h[slot_count];

// Width and height of a PNG, from its header (box art is PNG).
static int png_size(const char *path, int *w, int *h) {
    unsigned char hdr[24];
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    const size_t n = fread(hdr, 1, sizeof(hdr), f);
    fclose(f);
    if (n != sizeof(hdr) || memcmp(hdr + 1, "PNG", 3) != 0 || memcmp(hdr + 12, "IHDR", 4) != 0) return 0;
    *w = (int) ((uint32_t) hdr[16] << 24 | (uint32_t) hdr[17] << 16 | (uint32_t) hdr[18] << 8 | hdr[19]);
    *h = (int) ((uint32_t) hdr[20] << 24 | (uint32_t) hdr[21] << 16 | (uint32_t) hdr[22] << 8 | hdr[23]);
    return *w > 0 && *h > 0 && *w <= 8192 && *h <= 8192;
}

// Largest cover size inside the middle tile, keeping its shape (same maths as update_image).
static void cover_fit(const int iw, const int ih, int *aw, int *ah) {
    const float wr = (float) (tile_w[slot_cur] - 2 * KID_BORDER_CUR) / (float) iw;
    const float hr = (float) (tile_h[slot_cur] - 2 * KID_BORDER_CUR) / (float) ih;
    const float zr = wr < hr ? wr : hr;
    *aw = (int) ((float) iw * zr);
    *ah = (int) ((float) ih * zr);
}

// Side tiles are the middle tile shrunk by this factor (x1000).
static int side_k(void) {
    const int kw = tile_w[slot_prev] * 1000 / tile_w[slot_cur];
    const int kh = tile_h[slot_prev] * 1000 / tile_h[slot_cur];
    return kw < kh ? kw : kh;
}

// The middle-tile width for whatever game a tile shows (cover + outline, or the plain box).
static int cur_width_for(const int game) {
    if (game >= 0 && game < game_count && games[game].art_w > 0) return games[game].art_w + 2 * KID_BORDER_CUR;
    return tile_w[slot_cur];
}

static int tile_zoom(const int slot, const int w) {
    const int zoom = LV_IMG_ZOOM_NONE * w / cur_width_for(tile_game[slot]);
    return zoom > 0 ? zoom : 1;
}

static void place_tile(const int slot, const int x, const int w, const int h) {
    lv_obj_set_size(tile[slot], w, h);
    lv_obj_align(tile[slot], LV_ALIGN_CENTER, x, tile_y);
    lv_img_set_zoom(tile_img[slot], (uint16_t) tile_zoom(slot, w));
}

// Loading a cover resets its zoom to 100%; put the side tiles' zoom back once it lands.
// The timer only runs for about a second after a cover change, then pauses (saves battery).
#define ZOOM_FIX_MS    30
#define ZOOM_FIX_TICKS 34

static lv_timer_t *zoom_timer = NULL;
static int zoom_fix_left = 0;

static void zoom_fix_arm(void) {
    zoom_fix_left = ZOOM_FIX_TICKS;
    if (zoom_timer) lv_timer_resume(zoom_timer);
}

static void zoom_fix_task(lv_timer_t *timer) {
    if (slide_running) return;
    for (int i = 0; i < slot_count; i++) {
        const int zoom = tile_zoom(i, lv_obj_get_width(tile[i]));
        if (lv_img_get_zoom(tile_img[i]) != zoom) lv_img_set_zoom(tile_img[i], (uint16_t) zoom);
    }
    if (--zoom_fix_left <= 0) lv_timer_pause(timer);
}

// Frame size for a slot: snug around the cover (outline included), or the standard box.
static void slot_target(const int slot, const int game) {
    const kid_game *g = &games[game];
    if (g->art_w <= 0) {
        tgt_w[slot] = tile_w[slot];
        tgt_h[slot] = tile_h[slot];
    } else if (slot == slot_cur) {
        tgt_w[slot] = g->art_w + 2 * KID_BORDER_CUR;
        tgt_h[slot] = g->art_h + 2 * KID_BORDER_CUR;
    } else {
        const int k = side_k();
        tgt_w[slot] = g->art_w * k / 1000 + 2 * KID_BORDER_SIDE;
        tgt_h[slot] = g->art_h * k / 1000 + 2 * KID_BORDER_SIDE;
    }
}

static void fill_tile(const int slot, const int index) {
    const int visible = game_count > 0 && (slot == slot_cur || game_count > 2 || (game_count == 2 && slot == slot_next));
    if (!visible) {
        lv_obj_add_flag(tile[slot], LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(tile[slot], LV_OBJ_FLAG_HIDDEN);

    // Name tile text: same size everywhere, green in the middle.
    const int cur = slot == slot_cur;
    lv_obj_set_style_text_font(tile_lbl[slot], kid_font(2), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_line_space(tile_lbl[slot], 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(tile_lbl[slot], lv_color_hex(cur ? KID_COL_GREEN : KID_COL_BLUE), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_width(tile_lbl[slot], tile_w[slot] - 24);

    kid_game *g = &games[index];
    char art[PATH_MAX];
    const int has_art = box_art_path(g, art, sizeof(art));

    if (!g->art_known) {
        int iw = 0, ih = 0;
        g->art_known = 1;
        g->art_w = g->art_h = 0;
        if (has_art && png_size(art, &iw, &ih)) cover_fit(iw, ih, &g->art_w, &g->art_h);
    }
    slot_target(slot, index);

    if (tile_game[slot] == index) return;
    tile_game[slot] = index;

    if (has_art) {
        lv_obj_add_flag(tile_lbl[slot], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(tile_img[slot], LV_OBJ_FLAG_HIDDEN);

        const struct image_settings settings = {
            art, LV_ALIGN_CENTER, (int16_t) (tile_w[slot_cur] - 2 * KID_BORDER_CUR),
            (int16_t) (tile_h[slot_cur] - 2 * KID_BORDER_CUR), 0, 0, 0, 0
        };
        update_image(tile_img[slot], settings);
        zoom_fix_arm();
    } else {
        clear_image(tile_img[slot]);
        lv_obj_add_flag(tile_img[slot], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(tile_lbl[slot], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(tile_lbl[slot], g->name);
    }
}


// Game names get two lines. Big letters when the name fits in two lines,
// a size smaller when it doesn't, and "..." only if it is still too long.
static void set_name_text(const char *name) {
    const lv_coord_t w = lv_obj_get_width(lbl_name);
    int scale = 3;

    for (; scale > 2; scale--) {
        lv_point_t size;
        lv_txt_get_size(&size, name, kid_font(scale), 0, scale * 2, w, LV_TEXT_FLAG_NONE);
        if (size.y <= kid_font(scale)->line_height * 2 + scale * 2) break;
    }

    const lv_font_t *f = kid_font(scale);
    const lv_coord_t two_lines = f->line_height * 2 + scale * 2;
    lv_point_t size;
    lv_txt_get_size(&size, name, f, 0, scale * 2, w, LV_TEXT_FLAG_NONE);
    const lv_coord_t text_h = size.y < two_lines ? size.y : two_lines;

    // Centre the name in the gap between the middle cover and the button hint.
    const int sh = device.mux.height;
    const int cover_bottom = sh / 2 + tile_y + tgt_h[slot_cur] / 2;
    const int hint_top = sh - sh * 4 / 100 - kid_font(2)->line_height;
    const int top = (cover_bottom + hint_top) / 2 - text_h / 2;

    lv_obj_t *labels[2] = {lbl_name_shadow, lbl_name};
    for (int i = 0; i < 2; i++) {
        lv_obj_set_style_text_font(labels[i], f, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_text_line_space(labels[i], scale * 2, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_height(labels[i], text_h);
        lv_obj_align(labels[i], LV_ALIGN_TOP_MID, i == 0 ? 2 : 0, top + (i == 0 ? 2 : 0));
        lv_label_set_text(labels[i], name);
    }
}

static void refresh_carousel(void) {
    if (game_count <= 0) {
        for (int s = 0; s < slot_count; s++)
            lv_obj_add_flag(tile[s], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_name, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_name_shadow, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_count, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lbl_empty, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    fill_tile(slot_prev, wrap_index(current_game - 1));
    fill_tile(slot_cur, current_game);
    fill_tile(slot_next, wrap_index(current_game + 1));

    set_name_text(games[current_game].name);

    // Outside a slide, put each frame straight at its cover's shape.
    if (!slide_running) {
        for (int i = 0; i < slot_count; i++) place_tile(i, tile_x[i], tgt_w[i], tgt_h[i]);
    }

    char count[32];
    snprintf(count, sizeof(count), "%d / %d", current_game + 1, game_count);
    lv_label_set_text(lbl_count, count);
}

static lv_obj_t *make_label(lv_obj_t *parent, const int scale, const uint32_t colour) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, kid_font(scale), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(l, lv_color_hex(colour), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_line_space(l, scale * 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_shadow_width(l, 0, MU_OBJ_MAIN_DEFAULT);
    return l;
}

// Square panel with a pixel outline, like the box around the boot logo.
static lv_obj_t *make_panel(lv_obj_t *parent, const int w, const int h) {
    lv_obj_t *t = lv_obj_create(parent);
    lv_obj_remove_style_all(t);
    lv_obj_set_size(t, w, h);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(t, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(t, lv_color_hex(KID_COL_PANEL), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(t, LV_OPA_80, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(t, lv_color_hex(KID_COL_SEA), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(t, 3, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_shadow_color(t, lv_color_hex(KID_COL_SHADOW), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_shadow_ofs_x(t, 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_shadow_ofs_y(t, 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_shadow_spread(t, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_shadow_width(t, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_shadow_opa(t, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    return t;
}

static void set_panel_focus(lv_obj_t *panel, const int focus) {
    lv_obj_set_style_border_color(panel, lv_color_hex(focus ? KID_COL_GREEN : KID_COL_SEA), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(panel, focus ? 5 : 3, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(panel, focus ? LV_OPA_COVER : LV_OPA_70, MU_OBJ_MAIN_DEFAULT);
}

static void build_ui(void) {
    kid_style_background(ui_screen);
    if (ui_img_wall) lv_obj_add_flag(ui_img_wall, LV_OBJ_FLAG_HIDDEN);
    if (ui_pnl_content) lv_obj_add_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
    if (ui_pnl_header) lv_obj_add_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
    if (ui_pnl_footer) lv_obj_add_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);

    const int sw = device.mux.width;
    const int sh = device.mux.height;

    // Shut Down button, top right. Up selects it, A asks to confirm.
    // A small pixel up-arrow shows it is reached with Up.
    btn_power = make_panel(ui_screen, 9 * 6 * 2 + 28 + 22, 7 * 2 + 22);
    lv_obj_align(btn_power, LV_ALIGN_TOP_RIGHT, -sw * 4 / 100, sh * 4 / 100);
    arr_power = lv_obj_create(btn_power);
    lv_obj_remove_style_all(arr_power);
    lv_obj_clear_flag(arr_power, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(arr_power, 8, 8);
    lv_obj_align(arr_power, LV_ALIGN_LEFT_MID, 8, 0);
    for (int r = 0; r < 4; r++) {
        lv_obj_t *row = lv_obj_create(arr_power);
        lv_obj_remove_style_all(row);
        lv_obj_set_pos(row, 3 - r, r * 2);
        lv_obj_set_size(row, 2 + r * 2, 2);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    }
    lbl_power = make_label(btn_power, 2, KID_COL_BLUE);
    lv_label_set_text(lbl_power, "SHUT DOWN");
    lv_obj_align(lbl_power, LV_ALIGN_LEFT_MID, 24, 0);

    // Battery level, top left, level with the Shut Down button.
    bat_icon = kid_battery_create(ui_screen, 1);
    lv_obj_align(bat_icon, LV_ALIGN_TOP_LEFT, sw * 4 / 100, sh * 4 / 100 + 2);
    kid_battery_set(bat_icon, kid_battery_percent(), kid_battery_charging());

    tile_w[slot_cur] = sw * 38 / 100;
    tile_h[slot_cur] = sh * 53 / 100; // covers 20% bigger than the first layout
    tile_w[slot_prev] = tile_w[slot_next] = sw * 22 / 100;
    tile_h[slot_prev] = tile_h[slot_next] = sh * 36 / 100;

    const int y = -sh * 7 / 100;
    const int side_x = sw * 36 / 100;
    tile_y = y;
    tile_x[slot_prev] = -side_x;
    tile_x[slot_cur] = 0;
    tile_x[slot_next] = side_x;

    for (int s = 0; s < slot_count; s++) {
        tile_game[s] = -1;
        tile[s] = make_panel(ui_screen, tile_w[s], tile_h[s]);
        lv_obj_align(tile[s], LV_ALIGN_CENTER, tile_x[s], y);
        lv_obj_set_style_clip_corner(tile[s], true, MU_OBJ_MAIN_DEFAULT);

        // Sized to the picture itself: a larger image object makes LVGL tile (repeat) the cover.
        tile_img[s] = lv_img_create(tile[s]);
        lv_img_set_src(tile_img[s], &ui_img_blank);
        lv_obj_set_size(tile_img[s], LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_center(tile_img[s]);

        tile_lbl[s] = make_label(tile[s], s == slot_cur ? 3 : 2, s == slot_cur ? KID_COL_GREEN : KID_COL_BLUE);
        lv_obj_set_width(tile_lbl[s], tile_w[s] - 24);
        lv_label_set_long_mode(tile_lbl[s], LV_LABEL_LONG_WRAP);
        lv_obj_center(tile_lbl[s]);
        tgt_w[s] = tile_w[s];
        tgt_h[s] = tile_h[s];
        place_tile(s, tile_x[s], tile_w[s], tile_h[s]);
    }

    // Game name under the middle tile: up to two lines, with a dark drop shadow behind it.
    const int name_y = sh / 2 + y + tile_h[slot_cur] / 2 + sh * 3 / 100;
    lbl_name_shadow = make_label(ui_screen, 3, KID_COL_SHADOW);
    lv_obj_set_width(lbl_name_shadow, sw - sw * 8 / 100);
    lv_label_set_long_mode(lbl_name_shadow, LV_LABEL_LONG_DOT);
    lv_obj_align(lbl_name_shadow, LV_ALIGN_TOP_MID, 2, name_y + 2);

    lbl_name = make_label(ui_screen, 3, KID_COL_WHITE);
    lv_obj_set_width(lbl_name, sw - sw * 8 / 100);
    lv_label_set_long_mode(lbl_name, LV_LABEL_LONG_DOT);
    lv_obj_align(lbl_name, LV_ALIGN_TOP_MID, 0, name_y);

    // The "2 / 5" counter is kept for the empty-state logic but not shown.
    lbl_count = make_label(ui_screen, 2, KID_COL_SEA);
    lv_obj_add_flag(lbl_count, LV_OBJ_FLAG_HIDDEN);

    // Button hint along the bottom.
    lbl_hint = make_label(ui_screen, 2, KID_COL_BLUE);
    lv_label_set_recolor(lbl_hint, true);
    lv_label_set_text(lbl_hint, "#78E650 < >#  PICK      #78E650 A#  PLAY");
    lv_obj_align(lbl_hint, LV_ALIGN_BOTTOM_MID, 0, -sh * 4 / 100);

    lbl_empty = make_label(ui_screen, 4, KID_COL_GREEN);
    lv_obj_set_width(lbl_empty, sw - 60);
    lv_label_set_long_mode(lbl_empty, LV_LABEL_LONG_WRAP);
    lv_label_set_text(lbl_empty, "NO GAMES YET!\n\nASK DAD TO\nADD SOME");
    lv_obj_center(lbl_empty);
    lv_obj_add_flag(lbl_empty, LV_OBJ_FLAG_HIDDEN);

    pnl_confirm = make_panel(ui_screen, sw * 66 / 100, sh * 46 / 100);
    lv_obj_set_style_bg_opa(pnl_confirm, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(pnl_confirm, lv_color_hex(KID_COL_GREEN), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(pnl_confirm, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_center(pnl_confirm);

    lv_obj_t *lbl_confirm = make_label(pnl_confirm, 4, KID_COL_GREEN);
    lv_label_set_text(lbl_confirm, "SHUT DOWN?");
    lv_obj_align(lbl_confirm, LV_ALIGN_TOP_MID, 0, sh * 5 / 100);

    // NO is on the left and selected first, so a quick A A does not turn it off.
    btn_no = make_panel(pnl_confirm, 130, 52);
    lv_obj_set_style_shadow_opa(btn_no, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_align(btn_no, LV_ALIGN_CENTER, -90, sh * 2 / 100);
    lbl_no = make_label(btn_no, 3, KID_COL_BLUE);
    lv_label_set_text(lbl_no, "NO");
    lv_obj_center(lbl_no);

    btn_yes = make_panel(pnl_confirm, 130, 52);
    lv_obj_set_style_shadow_opa(btn_yes, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_align(btn_yes, LV_ALIGN_CENTER, 90, sh * 2 / 100);
    lbl_yes = make_label(btn_yes, 3, KID_COL_BLUE);
    lv_label_set_text(lbl_yes, "YES");
    lv_obj_center(lbl_yes);

    lv_obj_t *lbl_choice = make_label(pnl_confirm, 2, KID_COL_BLUE);
    lv_label_set_recolor(lbl_choice, true);
    lv_label_set_text(lbl_choice, "#78E650 < ># PICK    #78E650 A# OK");
    lv_obj_align(lbl_choice, LV_ALIGN_BOTTOM_MID, 0, -sh * 4 / 100);
    lv_obj_add_flag(pnl_confirm, LV_OBJ_FLAG_HIDDEN);

    // Message box in the same style, for "this game isn't set up" (any button closes it).
    pnl_msg = make_panel(ui_screen, sw * 76 / 100, sh * 42 / 100);
    lv_obj_set_style_bg_opa(pnl_msg, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(pnl_msg, lv_color_hex(KID_COL_GREEN), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(pnl_msg, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_center(pnl_msg);

    lbl_msg = make_label(pnl_msg, 3, KID_COL_GREEN);
    lv_obj_set_width(lbl_msg, sw * 68 / 100);
    lv_label_set_long_mode(lbl_msg, LV_LABEL_LONG_WRAP);
    lv_obj_align(lbl_msg, LV_ALIGN_CENTER, 0, -sh * 4 / 100);

    lv_obj_t *lbl_ok = make_label(pnl_msg, 3, KID_COL_BLUE);
    lv_label_set_recolor(lbl_ok, true);
    lv_label_set_text(lbl_ok, "#78E650 A#  OK");
    lv_obj_align(lbl_ok, LV_ALIGN_BOTTOM_MID, 0, -sh * 4 / 100);
    lv_obj_add_flag(pnl_msg, LV_OBJ_FLAG_HIDDEN);

    // Low battery box: orange edge, battery picture, A closes it.
    pnl_low = make_panel(ui_screen, sw * 66 / 100, sh * 46 / 100);
    lv_obj_set_style_bg_opa(pnl_low, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(pnl_low, lv_color_hex(KID_COL_LOW), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(pnl_low, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_center(pnl_low);

    lv_obj_t *low_bat = kid_battery_create(pnl_low, 1);
    lv_obj_align(low_bat, LV_ALIGN_TOP_MID, 14, sh * 4 / 100);
    kid_battery_set(low_bat, 5, 0);

    lv_obj_t *lbl_low = make_label(pnl_low, 4, KID_COL_LOW);
    lv_label_set_text(lbl_low, "BATTERY LOW!");
    lv_obj_align(lbl_low, LV_ALIGN_CENTER, 0, -sh * 2 / 100);

    lv_obj_t *lbl_charge = make_label(pnl_low, 3, KID_COL_BLUE);
    lv_label_set_text(lbl_charge, "PLEASE CHARGE");
    lv_obj_align(lbl_charge, LV_ALIGN_CENTER, 0, sh * 7 / 100);

    lv_obj_t *lbl_low_ok = make_label(pnl_low, 2, KID_COL_BLUE);
    lv_label_set_recolor(lbl_low_ok, true);
    lv_label_set_text(lbl_low_ok, "#78E650 A# OK");
    lv_obj_align(lbl_low_ok, LV_ALIGN_BOTTOM_MID, 0, -sh * 4 / 100);
    lv_obj_add_flag(pnl_low, LV_OBJ_FLAG_HIDDEN);

    kid_style_progress_bars();
}

static void open_panel(lv_obj_t *panel) {
    if (msg_open && msg_panel && msg_panel != panel) lv_obj_add_flag(msg_panel, LV_OBJ_FLAG_HIDDEN);
    msg_open = 1;
    msg_panel = panel;
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(panel);
}

static void show_msg(const char *text) {
    lv_label_set_text(lbl_msg, text);
    open_panel(pnl_msg);
}

static int close_msg(void) {
    if (!msg_open) return 0;
    msg_open = 0;
    if (msg_panel) lv_obj_add_flag(msg_panel, LV_OBJ_FLAG_HIDDEN);
    msg_panel = NULL;
    play_sound(snd_back);
    return 1;
}

static void refresh_focus(void) {
    set_panel_focus(btn_power, power_focus);
    lv_obj_set_style_text_color(lbl_power, lv_color_hex(power_focus ? KID_COL_GREEN : KID_COL_BLUE), MU_OBJ_MAIN_DEFAULT);
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(arr_power); i++) {
        lv_obj_set_style_bg_color(
            lv_obj_get_child(arr_power, (int32_t) i), lv_color_hex(power_focus ? KID_COL_GREEN : KID_COL_BLUE),
            MU_OBJ_MAIN_DEFAULT
        );
    }

    if (game_count > 0) set_panel_focus(tile[slot_cur], !power_focus);
    set_panel_focus(tile[slot_prev], 0);
    set_panel_focus(tile[slot_next], 0);
}

static void refresh_confirm(void) {
    set_panel_focus(btn_no, !confirm_yes);
    set_panel_focus(btn_yes, confirm_yes);
    lv_obj_set_style_bg_opa(btn_no, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(btn_yes, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(lbl_no, lv_color_hex(confirm_yes ? KID_COL_BLUE : KID_COL_GREEN), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(lbl_yes, lv_color_hex(confirm_yes ? KID_COL_GREEN : KID_COL_BLUE), MU_OBJ_MAIN_DEFAULT);
}

static void show_confirm(const int show) {
    confirm_open = show;
    if (show) {
        confirm_yes = 0;
        refresh_confirm();
        lv_obj_clear_flag(pnl_confirm, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(pnl_confirm);
    } else {
        lv_obj_add_flag(pnl_confirm, LV_OBJ_FLAG_HIDDEN);
    }
}

static void handle_up(void) {
    if (msg_open || confirm_open || power_focus) return;
    play_sound(snd_navigate);
    power_focus = 1;
    refresh_focus();
}

static void handle_down(void) {
    if (msg_open || confirm_open || !power_focus) return;
    play_sound(snd_navigate);
    power_focus = 0;
    refresh_focus();
}

static void handle_b(void) {
    if (close_msg()) return;

    if (confirm_open) {
        play_sound(snd_back);
        show_confirm(0);
        return;
    }

    // B on the Shut Down button goes back to the games.
    if (power_focus) {
        play_sound(snd_back);
        power_focus = 0;
        refresh_focus();
    }
}

// The SD card can still be mounting at boot: look again while the list is empty.
static void rescan_task(lv_timer_t *timer) {
    if (game_count > 0) {
        lv_timer_del(timer);
        return;
    }

    scan_games();
    if (game_count > 0) {
        lv_obj_add_flag(lbl_empty, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lbl_name, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lbl_name_shadow, LV_OBJ_FLAG_HIDDEN);
        refresh_carousel();
        refresh_focus();
        lv_timer_del(timer);
    }
}

// Keep the battery picture current, and say so (once, then every few minutes) when it runs low.
static void kid_battery_task(lv_timer_t *timer) {
    LV_UNUSED(timer);
    kid_battery_set(bat_icon, kid_battery_percent(), kid_battery_charging());

    if (!kid_low_warn_due(&low_warn, kid_battery_low(), confirm_open || msg_open, SDL_GetTicks())) return;
    play_sound(snd_error);
    open_panel(pnl_low);
}

static void kid_bright(const int dir) {
    kid_bright_press(dir, current_brightness, SDL_GetTicks());
}

// Holding L2/R2 repeats quickly; space the brightness steps out so each one lands.
static int bright_ready(void) {
    const uint32_t now = SDL_GetTicks();
    if (now - last_bright_ms < 150) return 0;
    last_bright_ms = now;
    return 1;
}

static void handle_l2(void) {
    if (!bright_ready()) return;
    kid_bright(-1);
    ui_common_handle_bright_down();
}

static void handle_r2(void) {
    if (!bright_ready()) return;
    kid_bright(+1);
    ui_common_handle_bright_up();
}

static void slide_step(void *var, const int32_t v) {
    LV_UNUSED(var);
    for (int i = 0; i < slot_count; i++) {
        const int x = slide_from_x[i] + (tile_x[i] - slide_from_x[i]) * v / 256;
        const int w = slide_from_w[i] + (tgt_w[i] - slide_from_w[i]) * v / 256;
        const int h = slide_from_h[i] + (tgt_h[i] - slide_from_h[i]) * v / 256;
        place_tile(i, x, w, h);
    }
}

static void slide_done(lv_anim_t *a) {
    LV_UNUSED(a);
    slide_running = 0;
    zoom_fix_arm();
}

static void slide_start(void) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, &slide_var);
    lv_anim_set_exec_cb(&a, slide_step);
    lv_anim_set_values(&a, 0, 256);
    lv_anim_set_time(&a, KID_SLIDE_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&a, slide_done);
    slide_running = 1;
    slide_step(NULL, 0);
    lv_anim_start(&a);
}

// A new press while still sliding: jump the last slide to its end first.
static void slide_finish(void) {
    if (!slide_running) return;
    lv_anim_del(&slide_var, NULL);
    slide_step(NULL, 256);
    slide_running = 0;
    zoom_fix_arm();
}

static void move(const int dir) {
    if (msg_open) return;
    if (confirm_open) {
        if ((dir > 0) != confirm_yes) {
            play_sound(snd_navigate);
            confirm_yes = dir > 0;
            refresh_confirm();
        }
        return;
    }
    if (power_focus || game_count <= 1) return;
    play_sound(snd_navigate);

    if (game_count < 3) {
        current_game = wrap_index(current_game + dir);
        for (int i = 0; i < slot_count; i++) tile_game[i] = -1;
        refresh_carousel();
        refresh_focus();
        return;
    }

    slide_finish();

    // Rotate the three tile objects one place so the pictures already loaded slide along;
    // only the tile coming in at the edge loads a new game.
    lv_obj_t *t = tile[0], *ti = tile_img[0], *tl = tile_lbl[0];
    int tg = tile_game[0];
    int from[slot_count];
    if (dir > 0) {
        tile[0] = tile[1], tile_img[0] = tile_img[1], tile_lbl[0] = tile_lbl[1], tile_game[0] = tile_game[1];
        tile[1] = tile[2], tile_img[1] = tile_img[2], tile_lbl[1] = tile_lbl[2], tile_game[1] = tile_game[2];
        tile[2] = t, tile_img[2] = ti, tile_lbl[2] = tl, tile_game[2] = tg;
        from[slot_prev] = slot_cur;
        from[slot_cur] = slot_next;
        from[slot_next] = -1;
    } else {
        t = tile[2], ti = tile_img[2], tl = tile_lbl[2], tg = tile_game[2];
        tile[2] = tile[1], tile_img[2] = tile_img[1], tile_lbl[2] = tile_lbl[1], tile_game[2] = tile_game[1];
        tile[1] = tile[0], tile_img[1] = tile_img[0], tile_lbl[1] = tile_lbl[0], tile_game[1] = tile_game[0];
        tile[0] = t, tile_img[0] = ti, tile_lbl[0] = tl, tile_game[0] = tg;
        from[slot_prev] = -1;
        from[slot_cur] = slot_prev;
        from[slot_next] = slot_cur;
    }

    // Tiles that were already on screen start from their current frame.
    for (int i = 0; i < slot_count; i++) {
        if (from[i] < 0) continue;
        slide_from_x[i] = tile_x[from[i]];
        slide_from_w[i] = lv_obj_get_width(tile[i]);
        slide_from_h[i] = lv_obj_get_height(tile[i]);
    }

    current_game = wrap_index(current_game + dir);
    refresh_carousel();

    // The new tile comes in from just past its edge, already at its own shape.
    for (int i = 0; i < slot_count; i++) {
        if (from[i] >= 0) continue;
        slide_from_x[i] = tile_x[i] * 2;
        slide_from_w[i] = tgt_w[i];
        slide_from_h[i] = tgt_h[i];
    }
    refresh_focus();
    lv_obj_move_foreground(tile[slot_cur]);
    slide_start();
}

static void handle_left(void) {
    move(-1);
}

static void handle_right(void) {
    move(+1);
}

static void handle_a(void) {
    if (close_msg()) return;

    if (confirm_open) {
        if (!confirm_yes) {
            play_sound(snd_back);
            show_confirm(0);
            return;
        }
        play_sound(snd_confirm);
        exit_status = 0;
        load_mux("shutdown");
        mux_input_stop();
        return;
    }

    if (power_focus) {
        play_sound(snd_confirm);
        show_confirm(1);
        return;
    }

    if (game_count <= 0) return;

    const kid_game *g = &games[current_game];
    write_text_to_file(KID_IDX_FILE, "w", INT, current_game);

    char launch_path[PATH_MAX];
    snprintf(launch_path, sizeof(launch_path), "%s", g->path);

    if (!load_content(0, launch_path)) {
        play_sound(snd_error);
        show_msg("THIS GAME ISN'T READY YET.\nASK DAD!");
        return;
    }

    play_sound(snd_confirm);
    write_text_to_file(MUOS_HST_LOAD, "w", CHAR, get_file_name(g->path));
    if (!content_launch_is_integrated()) fade_out_screen();

    exit_status = 1;
    load_mux("kid");
    mux_input_stop();
}

static void parent_press(void) {
    parent_hold_start = SDL_GetTicks();
    parent_fired = 0;
}

static void parent_hold(void) {
    if (parent_fired || !parent_hold_start) return;
    if (SDL_GetTicks() - parent_hold_start < KID_PARENT_MS) return;

    parent_fired = 1;
    LOG_INFO(mux_module, "Parent combo held: leaving kid picker until reboot");

    write_text_to_file(KID_PARENT_FILE, "w", CHAR, "");
    play_sound(snd_confirm);

    exit_status = 0;
    load_mux("launcher");
    mux_input_stop();
}

static void parent_release(void) {
    parent_hold_start = 0;
}

int muxkid_main(void) {
    exit_status = 0;
    parent_hold_start = 0;
    parent_fired = 0;

    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, "Games");
    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    init_fonts();
    build_ui();

    header_and_footer_setup();
    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0}, {ui_lbl_nav_a, "Play", 0}, {NULL, NULL, 0}});

    scan_games();

    current_game = file_exist(KID_IDX_FILE) ? read_line_int_from(KID_IDX_FILE, 1) : 0;
    if (current_game < 0 || current_game >= game_count) current_game = 0;

    power_focus = 0;
    confirm_open = 0;
    msg_open = 0;
    msg_panel = NULL;
    confirm_yes = 0;
    slide_running = 0;
    last_bright_ms = 0;
    refresh_carousel();
    refresh_focus();

    lv_timer_t *rescan = game_count > 0 ? NULL : lv_timer_create(rescan_task, 2000, NULL);
    lv_timer_t *bat_timer = lv_timer_create(kid_battery_task, KID_BAT_POLL_MS, NULL);
    zoom_timer = lv_timer_create(zoom_fix_task, ZOOM_FIX_MS, NULL);
    zoom_fix_arm();

    // A game that failed to start or crashed: say so in kid words, once.
    if (file_exist(KID_ERROR_FILE)) {
        char *why = read_all_char_from(KID_ERROR_FILE);
        remove(KID_ERROR_FILE);
        play_sound(snd_error);
        show_msg(why && strcmp(why, "start") == 0 ? "THIS GAME DIDN'T START.\nASK DAD!"
                                                  : "OOPS! THE GAME STOPPED.\nASK DAD!");
        free(why);
    }
    kid_battery_task(NULL);

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .hold_disabled = 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_dpad_up] = handle_up,
                [mux_input_dpad_down] = handle_down,
                [mux_input_l2] = handle_l2,
                [mux_input_r2] = handle_r2,
                [mux_input_dpad_left] = handle_left,
                [mux_input_dpad_right] = handle_right,
            },
        .hold_handler =
            {
                [mux_input_l2] = handle_l2,
                [mux_input_r2] = handle_r2,
                [mux_input_dpad_left] = handle_left,
                [mux_input_dpad_right] = handle_right,
            },
    };

    init_input(&input_opts, 1);

    append_combo(
        &input_opts, (mux_input_combo) {
                         .type_mask = BIT(mux_input_select) | BIT(mux_input_start) | BIT(mux_input_l1)
                                      | BIT(mux_input_r1),
                         .press_handler = parent_press,
                         .hold_handler = parent_hold,
                         .release_handler = parent_release,
                     }
    );

    mux_input_task(&input_opts);

    if (rescan && game_count == 0) lv_timer_del(rescan);
    lv_timer_del(bat_timer);
    lv_timer_del(zoom_timer);
    zoom_timer = NULL;
    lv_anim_del(&slide_var, NULL);
    slide_running = 0;

    free(games);
    games = NULL;
    game_count = 0;

    return exit_status;
}
