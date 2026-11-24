#include <common/config/kidmode.h>
#include <stdio.h>
#include <common/base/options.h>
#include <common/runtime/exec.h>
#include <common/storage/fileio.h>

int kid_mode_active(void) {
    static int cached = -1;
    if (cached < 0) cached = file_exist(KID_MODE_FLAG) ? 1 : 0;
    return cached;
}

int kid_child_active(void) {
    return kid_mode_active() && !file_exist(KID_PARENT_FILE);
}

void kid_bright_press(const int dir, const int current, const uint32_t now_ms) {
    static int last = -1;
    static uint32_t last_ms;

    const int from = (last >= 0 && now_ms - last_ms < 2000) ? last : current;
    const int to = kid_bright_next(from, dir);
    if (to < 0) return;

    char value[8];
    snprintf(value, sizeof(value), "%d", to);
    const char *args[] = {DEV_SCRIPT "bright.sh", value, NULL};
    run_exec(args, A_SIZE(args), 1, 0, NULL, NULL);
    last = to;
    last_ms = now_ms;
}

int kid_low_warn_due(kid_low_warn *w, const int low, const int busy, const uint32_t now_ms) {
    if (!low) {
        w->warned = 0;
        return 0;
    }
    if (busy) return 0;
    if (w->warned && now_ms - w->warned_ms < KID_LOW_REPEAT_MS) return 0;

    w->warned = 1;
    w->warned_ms = now_ms;
    return 1;
}
