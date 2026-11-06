/* The Bronze Choir's only two firmware calls (op1fw.h, tied to firmware #246) and its one read of the CPU, kept in
 * bc_fw.c so that they sit in one place: a new firmware or a core service in their stead changes bc_fw.c alone.
 * Designed exclusively for Opus by Nola Drift. */
#ifndef BC_FW_H
#define BC_FW_H

/* 1 while shift is held (fw_shift_held). */
int bc_shift_held(void);

/* The page's title: "0N name" with " *" when edited, or the engine's name when no preset is chosen (fw_snprintf). */
void bc_preset_title(char *buf, unsigned n, int preset, const char *name, int edited);

/* The CPU's cycle counter (400 a microsecond), the page's clock: on Opus beta 4 a still screen's frames run only every
 * fourth frame, so frames are no clock. The computer's builds count one display period (1/28.5 s) a call. */
#define BC_FRAME_CYCLES 14035088u       /* a display period: 400 MHz / 28.5 frames a second */
unsigned bc_cycles(void);

#endif
