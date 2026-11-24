#pragma once

// Kid mode: a GBA-like experience for young children.
// Enabled when the flag file KID_MODE_FLAG exists. The result is read once and cached.
//
// Two checks, on purpose:
//   kid_mode_active()  kid mode installed (cached). Device-wide rules that stay even after the
//                      parent combo: in-game hotkeys off, X=A/Y=B, no save states, kid brightness
//                      ladder, power-loss box hidden, clock save, Wi-Fi/Bluetooth off.
//   kid_child_active() kid mode and the parent combo not used since boot (checked every call).
//                      Child-only rules: kid picker, kid sounds, kid error/low-battery messages,
//                      emergency combos off, 3-minute idle sleep.
//
// Kid screens are laid out for the RG34XX's 720x480 display; most sizes scale from the screen
// size, but a few fixed pixel sizes (buttons, panels, battery icon) assume that resolution.

#include <stdint.h>
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

// Kid brightness moves along an even ladder (10, 26, 42 ... 250, then 255) so every press is the
// same size, instead of bright.sh's U/D which creep by 1 near the bottom. Returns the next level
// from `current` in direction dir (+1 brighter, -1 dimmer), or -1 when already at the end.
#define KID_BRIGHT_STEP 16
#define KID_MAX_BRIGHT  255

static inline int kid_bright_next(const int current, const int dir) {
    if (dir > 0) {
        if (current >= KID_MAX_BRIGHT) return -1;
        for (int v = KID_MIN_BRIGHT; v < KID_MAX_BRIGHT; v += KID_BRIGHT_STEP)
            if (v > current) return v;
        return KID_MAX_BRIGHT;
    }
    if (current <= KID_MIN_BRIGHT) return -1;
    int best = KID_MIN_BRIGHT;
    for (int v = KID_MIN_BRIGHT; v < current && v < KID_MAX_BRIGHT; v += KID_BRIGHT_STEP) best = v;
    return best;
}

int kid_mode_active(void);

// One kid brightness press (dir +1 / -1): steps the ladder from the level last set (trusted for
// 2 s, because the device brightness reading catches up later) or from `current`, and runs
// bright.sh. Shared by the picker and the in-game runner so both behave the same.
void kid_bright_press(int dir, int current, uint32_t now_ms);

// Low-battery reminder pacing: warn once when the battery turns low, then every
// KID_LOW_REPEAT_MS while it stays low. Returns 1 when a warning should show now; `busy`
// (another box on screen) postpones it without using up the warning.
#define KID_LOW_REPEAT_MS (5 * 60 * 1000)

typedef struct {
    int warned;
    uint32_t warned_ms;
} kid_low_warn;

int kid_low_warn_due(kid_low_warn *w, int low, int busy, uint32_t now_ms);
