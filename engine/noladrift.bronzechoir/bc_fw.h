/* The Bronze Choir's only two firmware calls (op1fw.h, tied to firmware #246), kept in bc_fw.c so that they sit in
 * one place: a new firmware or a core service in their stead changes bc_fw.c alone. Designed exclusively for Opus by
 * Nola Drift. */
#ifndef BC_FW_H
#define BC_FW_H

/* 1 while shift is held (fw_shift_held). */
int bc_shift_held(void);

/* The page's title: "0N name" with " *" when edited, or the engine's name when no preset is chosen (fw_snprintf). */
void bc_preset_title(char *buf, unsigned n, int preset, const char *name, int edited);

#endif
