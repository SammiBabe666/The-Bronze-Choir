/* The Bronze Choir's sound (bc_dsp.c): plain C with no SDK, so the OP-1 engine (bronzechoir.c) and the computer's
 * test and demo renderers (host/) build the very same file. Fixed point only: the OP-1's Blackfin has no FPU.
 *
 * Designed exclusively for Opus by Nola Drift. */
#ifndef BC_DSP_H
#define BC_DSP_H

#include "bc_tables.h"

/* The sound's eight knobs (0-32767). The OS turns 0-3 on T1 (blue, green, white, orange); 4 is the Opus core's mark;
 * 5 and 7 are the shift layer (shift + green, shift + orange), set by the engine's turn handler; 6 is unused. */
enum { BC_SIZE, BC_STRIKE, BC_METAL, BC_DECAY, BC_MARK, BC_SYMPATHY, BC_SPARE, BC_DETUNE };

#define BC_BLOCK        128                /* a voice's render: 128 samples at 88.2 kHz */
#define BC_SUM_BLOCK    64                 /* the voices' sum: 64 samples at 44.1 kHz */
#define BC_BELLS        4
#define BC_VOICES       6                  /* the OS plays six */
/* The mode budget, in units shared by every voice that sounds: a mode summed at 22.05 kHz costs one unit, at
 * 44.1 kHz (the chime's register) two. One voice keeps all of its bell's modes (8-13); six share BC_OSC_BUDGET, each
 * keeping its bell's most important modes first (the tables' order), and never fewer than BC_OSC_MIN. Set from the
 * simulator's cycle counts (docs/TESTING.md). */
#ifndef BC_OSC_BUDGET
#define BC_OSC_BUDGET   54
#endif
#define BC_OSC_MIN      4

struct bc_osc {
    unsigned ph;                           /* phase, 2^32 a cycle */
    int lvl;                               /* its decaying level: log2 of amplitude, Q20 */
    int init;                              /* its level at the strike, Q12 (for the live loudness match) */
    unsigned short amp;                    /* the amplitude it ended the last block on, Q15 */
    short mg;                              /* the metal's gain on it, Q12 log2 (kept: bc_render) */
    unsigned char over;                    /* above 19 kHz: silent (kept) */
    unsigned dlt;                          /* its decay a block, Q20 log2 (kept) */
    unsigned step;                         /* its phase step at the sum's rate (kept) */
};

struct bc_voice {
    struct bc_osc o[BC_MAXMODES];
    unsigned pitch;                        /* the phase step the bell was struck at */
    int lpitch;                            /* log2 of it, Q12 */
    signed char bell;                      /* 0-3: which of the four bells it is; -1 before the first strike */
    unsigned char n;                       /* its bell's modes */
    unsigned char pending;                 /* a strike waits for the next render (the pitch is only known there) */
    unsigned char released;
    unsigned char fading;                  /* a stolen voice: one block fading out before the new strike */
    unsigned char sounding;
    short k[8];                            /* its knobs, smoothed block by block */
    int att;                               /* the strike's contact: an attack gain, Q12 log2, rising to 0 */
    int att_step;
    int norm;                              /* the loudness match, Q12 log2 */
    int noise;                             /* the strike's impact noise: its envelope, Q15 */
    int noise_lp;
    unsigned short noise_dec, noise_col;
    unsigned rnd;
    unsigned wob;                          /* the slow instability's phase */
    unsigned wob_step;
    unsigned rtop;                         /* its bell's highest mode ratio (Q16), with a margin */
    int last;                              /* the last sample summed, for the interpolation up to 88.2 kHz */
    int lt_c, mt_c;                        /* the keys of what is kept per mode (bc_render) */
    unsigned f_c;
    unsigned char rel_c;
};

/* What the page reads (written in the audio block, single words; the page only reads). */
extern volatile unsigned bc_strikes[BC_BELLS];   /* counts every strike of each bell */
extern volatile short bc_strike_amt[BC_BELLS];   /* the strike knob at the last strike, 0-32767 */
extern volatile short bc_ring[BC_BELLS];         /* how loud each bell rings now, Q15 */
extern volatile short bc_sym[BC_BELLS];          /* each bell's sympathetic ringing now, Q15 */
extern volatile short bc_heard[8];               /* the newest voice's knobs, for the sum's chain and the page */
extern volatile int bc_cap;                      /* modes a voice may use now (the budget) */
extern volatile int bc_voices_now;               /* voices rendered in the last block */
extern volatile int bc_clips;                    /* samples the limiter had to hold (a test counter) */

/* Which bell a phase step (88.2 kHz) belongs to: below C3 bell one, C3-B3 two, C4-B4 three, C5 and up four. */
int bc_bell_of(unsigned pitch);

void bc_voice_init(struct bc_voice *v, int index);
void bc_note_on(struct bc_voice *v);
void bc_release(struct bc_voice *v);
void bc_render(struct bc_voice *v, short *out, const short *par, const unsigned *pitch);
int bc_finished(const struct bc_voice *v);

/* The voices' sum: the sympathetic resonance between the four bells, then a soft limiter. */
void bc_sum_init(void);
void bc_block(short *buf);

/* Fast memory (L1) for what every render reads and writes: its scratch and the sine table, bc_fast_bytes() bytes
 * from opus_fast, or 0 (then both stay in SDRAM). */
int bc_fast_bytes(void);
void bc_fast(void *mem);

/* A mode's live amplitude for the T2 page: voice v's mode k, Q15. */
int bc_mode_amp(const struct bc_voice *v, int k);

#endif
