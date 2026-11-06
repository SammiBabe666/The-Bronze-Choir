/* bc_fw.c: The Bronze Choir's two firmware calls and the cycle counter, the only file that includes op1fw.h (bc_fw.h
 * says why). */
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

unsigned bc_cycles(void)
{
#ifdef __bfin__
    unsigned c;
    __asm__ volatile ("%0 = CYCLES;" : "=d"(c));
    return c;
#else
    static unsigned t;
    return t += BC_FRAME_CYCLES;
#endif
}
