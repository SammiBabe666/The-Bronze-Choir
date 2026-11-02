/* A stand-in for what the OP-1's OS and the Opus core do around an engine, so the very same engine sources
 * (bronzechoir.c, bc_dsp.c, bc_page.c) run on a computer: six voices, note on / release / finished as the engine
 * service describes them (third_party/opus-plugin-kit/docs/api.md, Engines), each voice rendered as 128 samples at
 * 88.2 kHz, the sum taken to 44.1 kHz and handed to the engine's block handler, and the page drawn into a list of
 * drawing commands. It is an approximation: the OS's own mixing gain, its voice allocation details and its LFOs are
 * not modelled. It is for listening, tests and the demo video, not a measurement of the OP-1. */
#ifndef OS_SIM_H
#define OS_SIM_H

#include <stdio.h>

void os_init(void);                        /* runs the plugin's opl_init */
void os_note_on(int midi);
void os_note_off(int midi);
void os_all_off(void);
void os_block(short *out64);               /* one audio block: 64 samples at 44.1 kHz */
int os_busy_voices(void);
void os_set_knob(int k, int v);            /* the user turns a plain knob (0-3) */
void os_turn(int encoder, int detents, int shift);   /* a turn on T1 through the engine's handler */
void os_key(int key, int pressed);         /* a key on T1 through the engine's handler */
void os_frame(FILE *cmd, int page);        /* draw one frame of T1 (page 0) or T2 (1) into cmd as text */
extern int os_shift;
extern short os_knobs[8];
extern unsigned long long os_steals;

#endif
