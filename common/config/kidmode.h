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

// bright.sh argument for one dim step from `current`: a normal step, the last step down to
// KID_MIN_BRIGHT, or NULL when already there (so the screen never goes black).
static inline const char *kid_dim_arg(const int current, const int step) {
    if (current - step >= KID_MIN_BRIGHT) return "D";
    if (current > KID_MIN_BRIGHT) return "10";
    return 0;
}

int kid_mode_active(void);
