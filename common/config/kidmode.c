#include <common/config/kidmode.h>
#include <common/storage/fileio.h>

int kid_mode_active(void) {
    static int cached = -1;
    if (cached < 0) cached = file_exist(KID_MODE_FLAG) ? 1 : 0;
    return cached;
}
