// Kid mode game picker: a single-row carousel of the games on the SD card.
//
// Left/Right picks a game, A plays it, L2/R2 change brightness. Holding
// Select+Start+L1+R1 for 3 seconds leaves kid mode until the next reboot.

#include "muxshare.h"
#include <common/config/kidmode.h>

#define KID_MAX_GAMES  512
#define KID_IDX_FILE   "/tmp/kid_idx"
#define KID_PARENT_MS  3000
#define KID_GBA_CAT    "Nintendo Game Boy Advance"

#define KID_BG         0x2E2A6E
#define KID_TILE       0x4B45A8
#define KID_TILE_FOCUS 0xF2F2F7
#define KID_TEXT       0xFFFFFF
#define KID_TEXT_DIM   0xB9B6E8
#define KID_TILE_TEXT  0x1E1B4B

typedef struct {
    char path[PATH_MAX];
    char name[256];
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
static lv_obj_t *lbl_name;
static lv_obj_t *lbl_count;
static lv_obj_t *lbl_empty;

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

static void add_games_from(const char *dir, const int in_gba_dir) {
    DIR *d = opendir(dir);
    if (!d) return;

    const struct dirent *ent;
    while ((ent = readdir(d)) != NULL && game_count < KID_MAX_GAMES) {
        if (!is_game_file(ent->d_name, in_gba_dir)) continue;

        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
        remove_double_slashes(full);
        if (!file_exist(full)) continue;

        kid_game *g = &games[game_count++];
        snprintf(g->path, sizeof(g->path), "%s", full);
        make_display_name(ent->d_name, g->name, sizeof(g->name));
    }

    closedir(d);
}

static int game_cmp(const void *a, const void *b) {
    return strcasecmp(((const kid_game *) a)->name, ((const kid_game *) b)->name);
}

// Games come from the folder named on the first line of the kid mode flag file,
// or else from every GBA-looking folder under ROMS (plus loose .gba files there).
static void scan_games(void) {
    games = calloc(KID_MAX_GAMES, sizeof(kid_game));
    game_count = 0;
    if (!games) return;

    char *custom = read_line_char_from(KID_MODE_FLAG, 1);
    if (custom && custom[0] && dir_exist(custom)) {
        add_games_from(custom, 1);
    } else {
        char root[PATH_MAX];
        union_get_roms_root(root, sizeof(root));

        add_games_from(root, 0);

        DIR *d = opendir(root);
        if (d) {
            const struct dirent *ent;
            while ((ent = readdir(d)) != NULL) {
                if (ent->d_name[0] == '.') continue;

                char sub[PATH_MAX];
                snprintf(sub, sizeof(sub), "%s/%s", root, ent->d_name);
                if (!dir_exist(sub)) continue;

                add_games_from(sub, is_gba_dir(ent->d_name));
            }
            closedir(d);
        }
    }
    free(custom);

    qsort(games, (size_t) game_count, sizeof(kid_game), game_cmp);
    LOG_INFO(mux_module, "Kid picker found %d games", game_count);
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

static void fill_tile(const int slot, const int index, const int w, const int h) {
    const int visible = game_count > 0 && (slot == slot_cur || game_count > 2 || (game_count == 2 && slot == slot_next));
    if (!visible) {
        lv_obj_add_flag(tile[slot], LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(tile[slot], LV_OBJ_FLAG_HIDDEN);

    const kid_game *g = &games[index];
    char art[PATH_MAX];

    if (box_art_path(g, art, sizeof(art))) {
        lv_obj_add_flag(tile_lbl[slot], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(tile_img[slot], LV_OBJ_FLAG_HIDDEN);

        const struct image_settings settings = {
            art, LV_ALIGN_CENTER, (int16_t) (w - 8), (int16_t) (h - 8), 0, 0, 0, 0
        };
        update_image(tile_img[slot], settings);
    } else {
        clear_image(tile_img[slot]);
        lv_obj_add_flag(tile_img[slot], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(tile_lbl[slot], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(tile_lbl[slot], g->name);
    }
}

static int tile_w[slot_count];
static int tile_h[slot_count];

static void refresh_carousel(void) {
    if (game_count <= 0) {
        for (int s = 0; s < slot_count; s++)
            lv_obj_add_flag(tile[s], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_name, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_count, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lbl_empty, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    fill_tile(slot_prev, wrap_index(current_game - 1), tile_w[slot_prev], tile_h[slot_prev]);
    fill_tile(slot_cur, current_game, tile_w[slot_cur], tile_h[slot_cur]);
    fill_tile(slot_next, wrap_index(current_game + 1), tile_w[slot_next], tile_h[slot_next]);

    lv_label_set_text(lbl_name, games[current_game].name);

    char count[32];
    snprintf(count, sizeof(count), "%d / %d", current_game + 1, game_count);
    lv_label_set_text(lbl_count, count);
}

static lv_obj_t *make_tile(const int w, const int h, const int x, const int y, const int focus) {
    lv_obj_t *t = lv_obj_create(ui_screen);
    lv_obj_remove_style_all(t);
    lv_obj_set_size(t, w, h);
    lv_obj_align(t, LV_ALIGN_CENTER, x, y);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(t, 12, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(t, lv_color_hex(focus ? KID_TILE_FOCUS : KID_TILE), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(t, focus ? LV_OPA_COVER : LV_OPA_70, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(t, focus ? 4 : 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(t, lv_color_hex(0xFFD23F), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_clip_corner(t, true, MU_OBJ_MAIN_DEFAULT);
    return t;
}

static void build_ui(void) {
    lv_obj_set_style_bg_color(ui_screen, lv_color_hex(KID_BG), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_screen, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    if (ui_img_wall) lv_obj_add_flag(ui_img_wall, LV_OBJ_FLAG_HIDDEN);
    if (ui_pnl_content) lv_obj_add_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);

    const int sw = device.mux.width;
    const int sh = device.mux.height;

    tile_w[slot_cur] = sw * 42 / 100;
    tile_h[slot_cur] = sh * 52 / 100;
    tile_w[slot_prev] = tile_w[slot_next] = sw * 24 / 100;
    tile_h[slot_prev] = tile_h[slot_next] = sh * 34 / 100;

    const int y = -sh * 6 / 100;
    const int side_x = sw * 37 / 100;
    const int xs[slot_count] = {-side_x, 0, side_x};

    for (int s = 0; s < slot_count; s++) {
        const int focus = s == slot_cur;
        tile[s] = make_tile(tile_w[s], tile_h[s], xs[s], y, focus);

        tile_img[s] = lv_img_create(tile[s]);
        lv_obj_set_size(tile_img[s], tile_w[s], tile_h[s]);
        lv_obj_center(tile_img[s]);

        tile_lbl[s] = lv_label_create(tile[s]);
        lv_obj_set_width(tile_lbl[s], tile_w[s] - 20);
        lv_label_set_long_mode(tile_lbl[s], LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(tile_lbl[s], LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_text_color(tile_lbl[s], lv_color_hex(focus ? KID_TILE_TEXT : KID_TEXT), MU_OBJ_MAIN_DEFAULT);
        lv_obj_center(tile_lbl[s]);
    }

    lbl_name = lv_label_create(ui_screen);
    lv_obj_set_width(lbl_name, sw - 40);
    lv_label_set_long_mode(lbl_name, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lbl_name, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(lbl_name, lv_color_hex(KID_TEXT), MU_OBJ_MAIN_DEFAULT);
    lv_obj_align(lbl_name, LV_ALIGN_CENTER, 0, y + tile_h[slot_cur] / 2 + sh * 7 / 100);

    lbl_count = lv_label_create(ui_screen);
    lv_obj_set_style_text_color(lbl_count, lv_color_hex(KID_TEXT_DIM), MU_OBJ_MAIN_DEFAULT);
    lv_obj_align(lbl_count, LV_ALIGN_CENTER, 0, y + tile_h[slot_cur] / 2 + sh * 14 / 100);

    lbl_empty = lv_label_create(ui_screen);
    lv_obj_set_width(lbl_empty, sw - 60);
    lv_label_set_long_mode(lbl_empty, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(lbl_empty, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(lbl_empty, lv_color_hex(KID_TEXT), MU_OBJ_MAIN_DEFAULT);
    lv_label_set_text(lbl_empty, "No games yet.\nAsk a grown-up to add some!");
    lv_obj_center(lbl_empty);
    lv_obj_add_flag(lbl_empty, LV_OBJ_FLAG_HIDDEN);

    lv_obj_move_foreground(ui_pnl_header);
    lv_obj_move_foreground(ui_pnl_footer);
}

static void kid_bright(const char *dir) {
    const char *args[] = {DEV_SCRIPT "bright.sh", dir, NULL};
    run_exec(args, A_SIZE(args), 1, 0, NULL, NULL);
}

static void handle_l2(void) {
    const int step = config.settings.advanced.inc_bright > 0 ? config.settings.advanced.inc_bright : 16;
    if (current_brightness >= step * 2) kid_bright("D");
    ui_common_handle_bright_down();
}

static void handle_r2(void) {
    kid_bright("U");
    ui_common_handle_bright_up();
}

static void move(const int dir) {
    if (game_count <= 1) return;
    play_sound(snd_navigate);
    current_game = wrap_index(current_game + dir);
    refresh_carousel();
}

static void handle_left(void) {
    move(-1);
}

static void handle_right(void) {
    move(+1);
}

static void handle_a(void) {
    if (game_count <= 0) return;

    const kid_game *g = &games[current_game];
    write_text_to_file(KID_IDX_FILE, "w", INT, current_game);

    char launch_path[PATH_MAX];
    snprintf(launch_path, sizeof(launch_path), "%s", g->path);

    if (!load_content(0, launch_path)) {
        play_sound(snd_error);
        toast_message("This game isn't ready yet. Ask a grown-up!", tst_wait_m);
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

    refresh_carousel();

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .hold_disabled = 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
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

    free(games);
    games = NULL;
    game_count = 0;

    return exit_status;
}
