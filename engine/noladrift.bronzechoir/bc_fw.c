/* bc_fw.c: The Bronze Choir's two firmware calls, the only file that includes op1fw.h (bc_fw.h says why). */
#include "op1fw.h"
#include "bc_fw.h"

int bc_shift_held(void)
{
    return fw_shift_held() ? 1 : 0;
}

void bc_preset_title(char *buf, unsigned n, int preset, const char *name, int edited)
{
    if (preset)
        fw_snprintf(buf, n, "0%i %s%s", preset, name, edited ? " *" : "");
    else
        fw_snprintf(buf, n, "the bronze choir");
}
