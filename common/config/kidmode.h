#pragma once

// Kid mode: a GBA-like experience for young children.
// Enabled when the flag file KID_MODE_FLAG exists. The result is read once and cached.
#define KID_MODE_FLAG "/opt/muos/config/kidmode"

// Written by the parent combo in the kid picker; kid mode menus stay off until reboot.
#define KID_PARENT_FILE "/tmp/kid_parent"

int kid_mode_active(void);
