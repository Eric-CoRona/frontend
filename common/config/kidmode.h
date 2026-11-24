#pragma once

// Kid mode: a GBA-like experience for young children.
// Enabled when the flag file KID_MODE_FLAG exists. The result is read once and cached.
#define KID_MODE_FLAG "/opt/muos/config/kidmode"

// Written by the parent combo in the kid picker; kid mode menus stay off until reboot.
#define KID_PARENT_FILE "/tmp/kid_parent"

// How often the in-game runner writes changed in-game saves to the card in kid mode.
#define KID_SRAM_FLUSH_SECONDS 10
#define KID_IDLE_SLEEP_SECONDS 180

// Lowest kid brightness: muOS's own safe minimum (bright.sh brings 0 back up to this).
#define KID_MIN_BRIGHT 10

// Kid UI sounds (8-bit blips), used instead of the theme's sounds in kid mode.
#define KID_SOUND_DIR "/opt/muos/share/kid/sound"

// Left by the game runner when a game fails to start or crashes; the picker shows a kid
// message for it and removes it.
#define KID_ERROR_FILE "/tmp/kid_game_error"

// Last known good time (seconds since 1970), saved every few minutes in kid mode. At boot an
// init script puts the clock back to it if the clock reset (fully drained battery), so the
// clock never jumps backwards for Pokemon Emerald.
#define KID_CLOCK_FILE         "/opt/muos/config/kid_clock"
#define KID_CLOCK_SAVE_SECONDS 600

// Kid picker in charge (kid mode on and the parent combo not used since boot).
int kid_child_active(void);

// bright.sh argument for one dim step from `current`: a normal step, the last step down to
// KID_MIN_BRIGHT, or NULL when already there (so the screen never goes black).
static inline const char *kid_dim_arg(const int current, const int step) {
    if (current - step >= KID_MIN_BRIGHT) return "D";
    if (current > KID_MIN_BRIGHT) return "10";
    return 0;
}

int kid_mode_active(void);
