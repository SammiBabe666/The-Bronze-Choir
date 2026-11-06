/* The Bronze Choir's sound: four bronze bells by inharmonic additive / modal synthesis. Plain C, fixed point, no
 * floating point, no allocation, no C library. Designed exclusively for Opus by Nola Drift.
 *
 * A voice is one struck bell. Which bell is decided by the incoming note itself (its pitch, at the strike), so that
 * the OP-1's keyboard at its normal octave (F2-E4) holds all four, six keys each: below B2 the cathedral bell, B2-E3
 * the church bell, F3-A#3 the ceremonial bell, B3 and up the small chime. Every bell sounds an octave above the key
 * (1.1.0: the key's octave put the cathedral's hum and prime under what the OP-1's speaker plays: a thud). Each bell
 * has its own modal structure (bc_tables.h, from tools/make_tables.py): 8 to 13 sine modes at inharmonic ratios
 * (hum, prime, tierce, quint, nominal and the upper partials for the church bells), each with its own exponentially
 * decaying envelope, and doublets: split modes a few cents apart that beat as a real bell's do.
 *
 * The envelopes live in the log domain: a mode's level is log2 of its amplitude (Q20), and an exponential decay is a
 * constant subtracted once a block. Everything that shapes the loudness of a mode (the strike, the metal, the size,
 * the attack, the loudness match) is a sum of logs, turned into an amplitude once a block through a table and
 * ramped linearly across the block's 128 samples, so no knob, preset change or voice steal can click.
 *
 *   SIZE (blue)      the bell's body: the hum's weight, the tierce and upper partials' tuning, the decay's length,
 *                    the strike's colour. The played pitch stays the prime: the instrument stays in tune.
 *   STRIKE (green)   the excitation: the contact time (a soft mallet's slow attack to a hard clapper's instant
 *                    one), the spectral tilt of the excited modes, and the impact noise.
 *   METAL (white)    the balance of the upper modes, live (dark warm bronze to bright complex metal), and above
 *                    0.55 a stretch of the upper partials away from their tuned ratios.
 *   DECAY (orange)   the damping: T60 of the prime from 0.25 s to 32 s, every mode's own T60 following from it
 *                    (frequency-dependent damping: higher modes and higher notes die sooner, the hum lasts longest);
 *                    and how much a released key damps the bell.
 *   SYMPATHY (shift + green, bc_block)  the four bells' resonators excited by the other bells' sound.
 *   DETUNE (shift + orange)  the doublets' split (up to 7 cents), a fixed mistuning between modes, and a slow
 *                    instability of every mode's frequency: the beating and the metallic movement. */
#include "bc_dsp.h"

#define OCT             4096               /* an octave in Q12 */

#define LOG_FLOOR       (-(14 << 20))       /* -84 dB: a mode below this is silent and skipped */
#define VOICE_GAIN_L    (-7115)            /* log2(0.30) in Q12: one voice's sum of mode amplitudes */
#define NYQ_LIMIT_8     3607772u           /* a step of 0.215 of 2^32 at 88.2 kHz (19 kHz), / 256 */
#define N               (BC_BLOCK / 2)     /* the modes are summed at 44.1 kHz: 64 samples a block */
#define STEP_C4         12740061u
#define LSTEP_C4        96677              /* log2 of STEP_C4, Q12 */
/* The bells' boundaries as sounding steps (an octave above the keys B2, F3 and B3) */
#define STEP_B3         12025015u
#define STEP_F4         17005939u
#define STEP_B4         24050030u

volatile unsigned bc_strikes[BC_BELLS];
volatile short bc_strike_amt[BC_BELLS];
volatile short bc_ring[BC_BELLS];
volatile short bc_sym[BC_BELLS];
volatile short bc_heard[8];
volatile int bc_cap = BC_MAXMODES;
volatile int bc_voices_now;
volatile int bc_clips;

static int ring_acc[BC_BELLS];
static struct bc_voice *voices[BC_VOICES]; /* every voice, as the OS built them (bc_voice_init) */
static int units = BC_OSC_BUDGET;          /* the budget a voice may use this block (bc_render's first call) */
static int renders;                        /* voices rendered since the last sum block */
/* The sends: each bell's sound in the last block, at 44.1 kHz, for the other bells' sympathetic resonators. */
static int send[BC_BELLS][BC_SUM_BLOCK];
static volatile unsigned tune[BC_BELLS] = { STEP_B3 / 2, STEP_B3, STEP_F4, STEP_B4 };   /* each bell's last pitch */

static int scratch_sdram[N];
static int *acc = scratch_sdram;           /* a voice's 128 samples, summed */
static const short *sine = BC_SINE;        /* the sine every mode reads, 128 times a block */

int bc_fast_bytes(void)
{
    return N * (int)sizeof(int) + BC_SINE_LEN * (int)sizeof(short);
}

void bc_fast(void *mem)
{
    if (!mem)
        return;                            /* no fast memory left: both stay in SDRAM */
    acc = (int *)mem;
    short *const t = (short *)(acc + N);
    for (int i = 0; i < BC_SINE_LEN; ++i)
        t[i] = BC_SINE[i];
    sine = t;
}

int bc_scratch_bytes(void)
{
    return N * (int)sizeof(int);
}

void bc_sine_copy(short *t)
{
    for (int i = 0; i < BC_SINE_LEN; ++i)
        t[i] = BC_SINE[i];
}

void bc_mem(int *scratch, const short *sine_table)
{
    if (scratch)
        acc = scratch;
    if (sine_table)
        sine = sine_table;
}

/* ---------------------------------------------------------------- integer math */

/* 2^(x/4096) in Q15 for x <= 0 (Q12 log2); 0 below -15 octaves. */
static inline int exp2q15(int x)
{
    if (x >= 0)
        return 32768;
    const int n = -x, i = n >> 12;
    if (i > 15)
        return 0;
    const int f = n & 4095, j = f >> 4, r = f & 15;
    const int e = BC_EXP2N[j] + (((BC_EXP2N[j + 1] - BC_EXP2N[j]) * r) >> 4);
    return e >> i;
}

/* log2(x) in Q12, x > 0. */
static int log2q12(unsigned x)
{
    if (!x)
        return -(32 << 12);
    const int e = 31 - __builtin_clz(x);
    const unsigned m = e >= 12 ? x >> (e - 12) : x << (12 - e);   /* 4096..8191 */
    const int idx = (int)(m - 4096) >> 4, r = (int)m & 15;
    return (e << 12) + BC_LOG2M[idx] + (((BC_LOG2M[idx + 1] - BC_LOG2M[idx]) * r) >> 4);
}

/* r (Q16, below 2^20) x 2^(a/4096), |a| < 4096. */
static inline unsigned scale2(unsigned r, int a)
{
    if (a >= 0)
        return ((r >> 3) * (unsigned)exp2q15(a - OCT)) >> 11;
    return ((r >> 3) * (unsigned)exp2q15(a)) >> 12;
}

static inline int kn(int v)                /* a knob kept within 0-32767 */
{
    return v < 0 ? 0 : v > 32767 ? 32767 : v;
}

int bc_bell_of(unsigned pitch)
{
    return pitch < STEP_B3 ? 0 : pitch < STEP_F4 ? 1 : pitch < STEP_B4 ? 2 : 3;
}

/* ---------------------------------------------------------------- the voice */

void bc_voice_init(struct bc_voice *v, int index)
{
    if (index >= 0 && index < BC_VOICES)
        voices[index] = v;
    for (int k = 0; k < BC_MAXMODES; ++k) {
        v->o[k].ph = 0;
        v->o[k].lvl = LOG_FLOOR;
        v->o[k].init = -(14 << 12);
        v->o[k].amp = 0;
    }
    v->pitch = 0;
    v->rtop = 1u << 20;
    v->last = 0;
    v->lpitch = LSTEP_C4;
    v->bell = -1;
    v->n = 0;
    v->pending = v->released = v->fading = v->sounding = 0;
    for (int k = 0; k < 8; ++k)
        v->k[k] = 0;
    v->att = 0;
    v->att_step = OCT;
    v->norm = VOICE_GAIN_L;
    v->noise = v->noise_lp = 0;
    v->noise_dec = 32000;
    v->noise_col = 8000;
    v->rnd = 0x9E3779B9u * (unsigned)(index + 1);
    v->wob = (unsigned)index * 0x2A000000u;
    v->wob_step = 0;
}

void bc_note_on(struct bc_voice *v)
{
    v->pending = 1;                        /* the strike happens in the next render, where the pitch is known */
    v->released = 0;
}

void bc_release(struct bc_voice *v)
{
    v->released = 1;
}

int bc_finished(const struct bc_voice *v)
{
    return !v->sounding && !v->pending;
}

int bc_mode_amp(const struct bc_voice *v, int k)
{
    return k >= 0 && k < v->n ? v->o[k].amp : 0;
}

/* An approximate log-sum of two Q20 levels: a re-strike adds energy to a ringing mode. */
static inline int logadd(int a, int b)
{
    const int hi = a > b ? a : b, d = a > b ? a - b : b - a;
    return hi + (d < (1 << 20) ? ((1 << 20) - d) >> 1 : 0);
}

/* A mode's level as the bell's size shapes it (Q12 log2): the hum and low modes heavier in a big bell, its upper
 * modes a little darker. The loudness match is made from these, not from the strike, so a hard strike is louder
 * than a soft one and re-striking a ringing bell can never raise the match. */
static int size_level(const struct bc_mode_def *m, int sz)
{
    if (m->lratio < 0)
        return m->level + (((sz - 16384) * 6144) >> 15);   /* +-1.5 octaves */
    if (m->lratio > 4915)                                   /* above 2.3 times the prime */
        return m->level - (((sz - 16384) * 2458) >> 15);
    return m->level;
}

/* The strike's levels: each mode's size-shaped level and the strike's tilt. */
static void strike_levels(struct bc_voice *v, const struct bc_bell_def *b, int add)
{
    const int st = kn(v->k[BC_STRIKE]), sz = kn(v->k[BC_SIZE]);
    const int tilt = -3686 + ((st * 6144) >> 15);          /* Q12 per octave of ratio: -0.9 (soft) .. +0.6 (hard) */
    for (int k = 0; k < v->n; ++k) {
        const struct bc_mode_def *m = &b->m[k];
        int l = size_level(m, sz) + ((m->lratio * tilt) >> 12);
        l = l > 0 ? 0 : l < -(14 << 12) ? -(14 << 12) : l;
        struct bc_osc *o = &v->o[k];
        if (add)
            o->lvl = logadd(o->lvl, l * 256);
        else {
            o->lvl = l * 256;
            o->ph = 0;                     /* sines start at zero: the onset never clicks */
            o->amp = 0;
        }
        o->init = l;
    }
}

static void strike(struct bc_voice *v, const short *par, unsigned pitch)
{
    const int bell = bc_bell_of(pitch);
    const int lp = log2q12(pitch);
    const int same = v->sounding && v->bell == bell && (lp - v->lpitch < 85 && v->lpitch - lp < 85);   /* 1/4 semitone */
    for (int k = 0; k < 8; ++k)
        v->k[k] = (short)kn(par[k]);
    const struct bc_bell_def *b = &BC_BELL_DEF[bell];
    if (!same) {
        for (int k = 0; k < BC_MAXMODES; ++k) {
            v->o[k].lvl = LOG_FLOOR;
            v->o[k].amp = 0;
        }
    }
    v->bell = (signed char)bell;
    v->n = (unsigned char)b->n;
    v->rtop = 0;
    for (int k = 0; k < b->n; ++k)         /* its highest mode, with room for the size's and metal's stretch */
        v->rtop = (unsigned)b->m[k].ratio > v->rtop ? (unsigned)b->m[k].ratio : v->rtop;
    v->rtop += v->rtop >> 3;
    v->pitch = pitch;
    v->lpitch = lp;
    strike_levels(v, b, same);
    const int st = kn(v->k[BC_STRIKE]), sz = kn(v->k[BC_SIZE]);
    /* the contact: a soft strike starts up to 5 octaves (30 dB) down and rises in about 12 ms; a hard one at once */
    v->att = same ? 0 : -(((32767 - st) * 5) >> 3);       /* Q12 */
    v->att_step = 2458 + ((st * 12288) >> 15);             /* 0.6 .. 3.6 octaves a block */
    /* the impact: noise as loud and as bright as the strike, darker for a bigger bell, 3-8 ms */
    v->noise = (st * st) >> 17;                            /* up to a quarter of full scale */
    v->noise_col = (unsigned short)(1500 + ((st * 29000) >> 15) - ((sz * st >> 15) * 9000 >> 15));   /* 1500..30500 */
    v->noise_dec = (unsigned short)(32234 + ((sz * 400) >> 15));
    v->wob_step = 4000 + (unsigned)bell * 900 + ((unsigned)(v->rnd >> 24) << 5);   /* 0.3-1.1 Hz a block */
    v->lt_c = 0x7FFFFFFF;                  /* every kept value worked out again at the next block */
    v->mt_c = 0x7FFFFFFF;
    v->f_c = ~0u;
    v->sounding = 1;
    tune[bell] = pitch;
    bc_strikes[bell] = bc_strikes[bell] + 1;
    bc_strike_amt[bell] = (short)st;
}

/* One mode's n samples (64 at 44.1 kHz, or 32 at 22.05 kHz) into acc: a sine at step s, its amplitude ramped from
 * a0 to a1 (Q15, below 32768; n = 1 << sh). The multiply is 16 x 16 (one cycle on the Blackfin; a 32-bit one takes
 * several). Two samples a pass: the compiler hides one sample's table-index move into an address register behind the
 * other's work (measured in sim/: 2 a pass beat 1 and 4). */
static inline __attribute__((always_inline))
unsigned mode_loop(int *dst, unsigned p, unsigned s, int a0, int a1, int first, int n, int sh)
{
    int a = a0 << 15;
    const int da = ((a1 - a0) * 32768) >> sh;
    const short *const t = sine;
    if (first)
        for (int i = 0; i < n; i += 2) {
            const unsigned p0 = p + s, p1 = p0 + s;
            const short g0 = (short)(a >> 15), g1 = (short)((a + da) >> 15);
            dst[i] = (t[p0 >> 22] * g0) >> 15;
            dst[i + 1] = (t[p1 >> 22] * g1) >> 15;
            p = p1;
            a += 2 * da;
        }
    else
        for (int i = 0; i < n; i += 2) {
            const unsigned p0 = p + s, p1 = p0 + s;
            const short g0 = (short)(a >> 15), g1 = (short)((a + da) >> 15);
            dst[i] += (t[p0 >> 22] * g0) >> 15;
            dst[i + 1] += (t[p1 >> 22] * g1) >> 15;
            p = p1;
            a += 2 * da;
        }
    return p;
}

void bc_render(struct bc_voice *v, short *out, const short *par, const unsigned *pitchv)
{
    const unsigned pitch = pitchv[0] < 0x80000000u ? pitchv[0] << 1 : 0xFFFFFFFFu;   /* an octave above the key */
    for (int k = 0; k < 8; ++k)
        bc_heard[k] = par[k];
    if (!renders++) {
        /* the block's first voice: the budget shared by every voice that sounds or is about to */
        int live = 0;
        for (int i = 0; i < BC_VOICES; ++i)
            live += voices[i] && (voices[i]->sounding || voices[i]->pending);
        units = BC_OSC_BUDGET / (live > 0 ? live : 1);
    }
    if (v->pending) {
        const int lpn = log2q12(pitch);
        const int near = v->bell == bc_bell_of(pitch) && lpn - v->lpitch < 85 && v->lpitch - lpn < 85;
        if (v->sounding && !v->fading && v->bell >= 0 && !near) {
            v->fading = 1;                 /* a stolen voice: fade the old bell out this block, strike the next */
        } else {
            v->fading = 0;
            v->pending = 0;
            strike(v, par, pitch);
        }
    }
    if (!v->sounding && !v->fading) {
        for (int i = 0; i < BC_BLOCK; ++i)
            out[i] = 0;
        v->last = 0;                       /* silence: the next strike's interpolation starts from it */
        return;
    }
    /* the knobs, smoothed: a quarter of the way a block (about 5 ms) */
    for (int k = 0; k < 8; ++k)
        v->k[k] = (short)(v->k[k] + ((kn(par[k]) - v->k[k]) >> 2));
    const int sz = v->k[BC_SIZE], mt = v->k[BC_METAL], dc = v->k[BC_DECAY], dt = v->k[BC_DETUNE];
    const struct bc_bell_def *b = &BC_BELL_DEF[v->bell];
    /* the bend and glide move the whole bell; its register stays the one it was struck in */
    const int lp = log2q12(pitch);
    /* METAL: the upper modes' tilt about the nominal (Q12 per octave of ratio) and their stretch */
    const int mtilt = -6554 + ((mt * 11469) >> 15);        /* -1.6 .. +1.2 */
    const int stretch = mt > 18022 ? ((mt - 18022) * 205) >> 14 : 0;   /* Q12: up to 0.05 octave an octave */
    /* DETUNE: the doublets' split, a fixed mistuning, the slow instability */
    const int split = (dt * 24) >> 15;                     /* Q12: up to 7 cents */
    const int wobble = (BC_SINE[v->wob >> 22] * ((dt * 64) >> 15)) >> 15;   /* Q16 octaves: up to ~1.2 cents */
    v->wob += v->wob_step << 6;
    /* SIZE: the partials' tuning (each mode's own coefficient) at the knob's ends */
    const int szd = sz - 16384;                            /* -16384 .. 16383 */
    /* DECAY: log2 of the prime's T60 in Q12 octaves of seconds: -2 (0.25 s) .. +5 (32 s), then the bell's own,
     * the size's, the note's (higher notes shorter) and the release's damping */
    static const short BELL_T[BC_BELLS] = { 3277, 1229, -1229, -3277 };   /* +0.8, +0.3, -0.3, -0.8 octaves */
    /* (the note's part measured from C5: each key keeps the decay it had before the bells went up an octave) */
    int lt = -8192 + ((dc * 28672) >> 15) + BELL_T[v->bell] + ((szd * 5734) >> 15) - ((lp - LSTEP_C4 - OCT) >> 1);
    if (v->released)
        lt -= 1229 + (((32767 - dc) * 12288) >> 15);       /* the hand on the bell: 0.3 .. 3.3 octaves shorter */
    /* the rate: a bell whose highest mode stays under 5.5 kHz is summed at 22.05 kHz (bells one to three across the
     * keyboard), the rest at 44.1 kHz; decided each block, so a bend upward moves it up in time */
    const int half = (pitch >> 16) * (v->rtop >> 8) < 1046191u;   /* 5.5 kHz as a step at 88.2 kHz, / 256 */
    const int n = half ? N / 2 : N, sh = half ? 5 : 6;
    /* the attack and the loudness match */
    const int att = v->att;
    v->att = att + v->att_step > 0 ? 0 : att + v->att_step;
    /* the mode budget: a mode summed at 44.1 kHz costs two units, at 22.05 kHz one */
    int cap = half ? units : units >> 1;
    cap = cap < BC_OSC_MIN ? BC_OSC_MIN : cap;
    if (v->fading)
        cap = 0;
    bc_cap = cap;
    /* What only changes with a knob or the pitch is kept per mode and worked out again only when it changed: the
     * decay steps (lt), the metal's gains and the loudness match (mtilt), the frequencies (pitch, size, metal's
     * stretch, the split). Each block then costs a mode one table look-up and its 64 or 32 samples. */
    if (lt != v->lt_c || v->released != v->rel_c) {
        const int tmax = v->released ? 4 * OCT + 2048 : 6 * OCT;   /* 64 s held, 22 s once let go */
        for (int k = 0; k < v->n; ++k) {
            /* the prime's T60 shortened for higher modes (0.75 an octave), lengthened by the mode's own bias */
            int t = lt + b->m[k].dbias - ((b->m[k].lratio * 3) >> 2);
            t = t < -7 * OCT ? -7 * OCT : t > tmax ? tmax : t;
            const int y = t + 7 * OCT, yi = y >> 12;       /* the step a block: 15166 / T60 in Q20 */
            v->o[k].dlt = ((1941248u >> yi) * (unsigned)(exp2q15(-(y & 4095)) >> 4)) >> 11;
        }
        v->lt_c = lt;
        v->rel_c = v->released;
    }
    const int nkey = mtilt + (szd >> 6) * 16384;
    if (nkey != v->mt_c) {
        int sum = 0;
        for (int k = 0; k < v->n; ++k) {
            const int mg = ((b->m[k].lratio - OCT) * mtilt) >> 12;
            v->o[k].mg = (short)(mg > 3 * OCT ? 3 * OCT : mg < -3 * OCT ? -3 * OCT : mg);
            sum += exp2q15(size_level(&b->m[k], sz) + v->o[k].mg);
        }
        v->norm = VOICE_GAIN_L - log2q12((unsigned)(sum > 0 ? sum : 1)) + 15 * OCT;
        v->mt_c = nkey;
    }
    const unsigned fkey = pitch ^ ((unsigned)szd << 7) ^ ((unsigned)stretch << 19) ^ ((unsigned)split << 25) ^
                          (unsigned)half;
    if (fkey != v->f_c) {
        unsigned mist = 0x6D2B79F5u;
        for (int k = 0; k < v->n; ++k) {
            const struct bc_mode_def *m = &b->m[k];
            mist = mist * 1103515245u + 12345u;
            int adj = (m->size * szd) >> 14;
            const int above = m->lratio - OCT;             /* octaves above the nominal (2 x the prime) */
            if (above > 0)
                adj += (above * stretch) >> 12;
            adj += m->doublet * split + ((((int)(mist >> 29) - 3) * split) >> 3);
            const unsigned r = scale2((unsigned)m->ratio, adj);
            v->o[k].over = (pitch >> 16) * (r >> 8) > NYQ_LIMIT_8;
            v->o[k].step = ((pitch >> 16) * r + (((pitch & 0xFFFFu) * (r >> 4)) >> 12)) << (half ? 2 : 1);
        }
        v->f_c = fkey;
    }
    /* Each mode's amplitude at the block's end. Their sum is the voice's worst-case peak: whatever a history of
     * hard and soft re-strikes and knob extremes left in the levels, it is held to 0.85 of full scale (the gain
     * eases in over the block, as every amplitude does), so a voice alone can never clip. */
    unsigned short a1s[BC_MAXMODES];
    int total = 0;                         /* the amplitude this block ends on, for the page and the hold */
    int alive = 0;
    for (int k = 0; k < v->n; ++k) {
        struct bc_osc *o = &v->o[k];
        a1s[k] = 0;
        if (o->lvl <= LOG_FLOOR && !o->amp)
            continue;
        o->lvl -= (int)o->dlt;
        if (o->lvl < LOG_FLOOR)
            o->lvl = LOG_FLOOR;
        if (k < cap && !o->over && o->lvl > LOG_FLOOR) {
            const int a1 = exp2q15((o->lvl >> 8) + o->mg + att + v->norm);   /* level + metal + attack + match */
            a1s[k] = (unsigned short)(a1 > 32767 ? 32767 : a1);
        }
        total += a1s[k];
        alive |= a1s[k] | (o->lvl > LOG_FLOOR);
    }
    if (total > 27852) {
        const int hold = (27852 << 15) / total;            /* Q15 */
        for (int k = 0; k < v->n; ++k)
            a1s[k] = (unsigned short)((a1s[k] * hold) >> 15);
        total = 27852;
    }
    int first = 1;
    for (int k = 0; k < v->n; ++k) {
        struct bc_osc *o = &v->o[k];
        const int a0 = o->amp, a1 = a1s[k];
        if (a0 | a1) {
            /* the slow instability: the step moved by up to ~1.2 cents, each mode by its own share */
            const unsigned st = o->step;
            const unsigned s = wobble ? st + (unsigned)(((int)(st >> 20) * (wobble * (1 + (k & 3)) * 710)) >> 8) : st;
            o->ph = mode_loop(acc, o->ph, s, a0, a1, first, n, sh);
            first = 0;
        }
        o->amp = (unsigned short)a1;
    }
    if (first)
        for (int i = 0; i < n; ++i)
            acc[i] = 0;
    /* the impact noise, filtered by the strike's colour */
    if (v->noise > 8 && !v->fading) {
        int e = v->noise, lpv = v->noise_lp;
        unsigned rnd = v->rnd;
        const int col = v->noise_col, dec = v->noise_dec;
        for (int i = 0; i < n; ++i) {
            rnd = rnd * 1664525u + 1013904223u;
            lpv += (col * ((int)(rnd >> 16) - 32768 - lpv)) >> 15;
            acc[i] += (lpv * e) >> 15;
            e = (e * dec) >> 15;
        }
        v->noise = e;
        v->noise_lp = lpv;
        v->rnd = rnd;
    } else
        v->noise = 0;
    /* out at 88.2 kHz, interpolated in straight lines from the sum's rate (the OS's own filter takes the voices' sum
     * back to 44.1 kHz), and this bell's send to the others at 44.1 kHz */
    int *const snd = send[v->bell];
    int prev = v->last;
    if (half)
        for (int i = 0; i < n; ++i) {
            int x = acc[i];
            x = x > 32767 ? 32767 : x < -32768 ? -32768 : x;
            const int d = x - prev;
            out[4 * i] = (short)(prev + (d >> 2));
            out[4 * i + 1] = (short)(prev + (d >> 1));
            out[4 * i + 2] = (short)(prev + ((3 * d) >> 2));
            out[4 * i + 3] = (short)x;
            snd[2 * i] += prev + (d >> 1);      /* six voices of full scale at most: no clamp needed */
            snd[2 * i + 1] += x;
            prev = x;
        }
    else
        for (int i = 0; i < n; ++i) {
            int x = acc[i];
            x = x > 32767 ? 32767 : x < -32768 ? -32768 : x;
            out[2 * i] = (short)((prev + x) >> 1);
            out[2 * i + 1] = (short)x;
            snd[i] += x;
            prev = x;
        }
    v->last = prev;
    if (total > ring_acc[(int)v->bell])
        ring_acc[(int)v->bell] = total;
    if (v->fading) {
        v->fading = 0;                     /* faded: the waiting strike comes next block */
        v->sounding = 0;
        v->noise = 0;
        for (int k = 0; k < BC_MAXMODES; ++k) {
            v->o[k].lvl = LOG_FLOOR;
            v->o[k].amp = 0;
        }
        return;
    }
    v->sounding = alive || v->noise > 8;
}

/* ---------------------------------------------------------------- the sum: sympathy and the limiter */

/* Each bell's sympathetic resonators: two state-variable band-passes at 22.05 kHz, tuned to the bell's prime and
 * nominal (its last struck pitch), fed only by the other three bells' sends. Nothing they output ever reaches their
 * own input or the voices, so there is no feedback loop at all: a stable filter (fixed coefficients within a
 * block, f below 1.3) on a bounded input is bounded, and the states are clamped as well. */
struct res { int low, band; };
static struct res rs[BC_BELLS][2];
static int lim_hold;

void bc_sum_init(void)
{
    for (int b = 0; b < BC_BELLS; ++b) {
        rs[b][0].low = rs[b][0].band = rs[b][1].low = rs[b][1].band = 0;
        for (int i = 0; i < BC_SUM_BLOCK; ++i)
            send[b][i] = 0;
    }
    lim_hold = 0;
}

#define RES_MAX  ((1 << 21) - 64)   /* >> 6 still fits a short: 16 x 16 multiplies */

static inline int clampr(int x)
{
    return x > RES_MAX ? RES_MAX : x < -RES_MAX ? -RES_MAX : x;   /* two one-cycle min / max on the Blackfin */
}

void bc_block(short *buf)
{
    const int sy = kn(bc_heard[BC_SYMPATHY]), dc = kn(bc_heard[BC_DECAY]);
    /* Each bell's two resonators, at 22.05 kHz, fed the other three bells' sends. */
    const int g = (sy * sy) >> 15;                         /* Q15: the sympathy's gain, a square law */
    static int sym_out[BC_SUM_BLOCK / 2];
    static short xin[BC_SUM_BLOCK / 2];
    static int tot[BC_SUM_BLOCK / 2];
    const int on = g >= 40;
    if (on)
        for (int i = 0; i < BC_SUM_BLOCK / 2; ++i) {
            sym_out[i] = 0;
            tot[i] = send[0][2 * i] + send[0][2 * i + 1] + send[1][2 * i] + send[1][2 * i + 1] + send[2][2 * i] +
                     send[2][2 * i + 1] + send[3][2 * i] + send[3][2 * i + 1];
        }
    /* Q from 60 to 360 with the decay */
    const short ds = (short)(16384 / (60 + ((dc * 300) >> 15)));   /* Q14: the damping, 1/Q */
    for (int b = 0; b < BC_BELLS; ++b) {
        if (!on) {                         /* off: the resonators rest and cost nothing */
            rs[b][0].low = rs[b][0].band = rs[b][1].low = rs[b][1].band = 0;
            bc_sym[b] = 0;
            continue;
        }
        /* the other bells' sound at 22.05 kHz (pairs summed), an eighth for headroom; quiet in and quiet resonators:
         * nothing to do */
        int loud = 0;
        for (int i = 0; i < BC_SUM_BLOCK / 2; ++i) {
            int x = (tot[i] - send[b][2 * i] - send[b][2 * i + 1]) >> 3;
            x = x > 32767 ? 32767 : x < -32768 ? -32768 : x;
            xin[i] = (short)x;
            loud |= x + 8 > 16;
        }
        struct res *const r0 = &rs[b][0], *const r1 = &rs[b][1];
        if (!loud && r0->band < 2048 && r0->band > -2048 && r1->band < 2048 && r1->band > -2048) {
            r0->low = r0->band = r1->low = r1->band = 0;
            bc_sym[b] = 0;
            continue;
        }
        int peak = 0;
        for (int h = 0; h < 2; ++h) {
            const unsigned st = tune[b] << h;              /* the prime, then the nominal (2 x) */
            if (st >= 0x10000000u)                         /* above 5.4 kHz: not at this rate */
                continue;
            int f = BC_SINE[(st << 1) >> 22];              /* Q15 sin(pi fc / 22050) = Q14 2 sin(...) */
            f = f > 21299 ? 21299 : f < 8 ? 8 : f;
            const short fs = (short)f;
            int low = rs[b][h].low, band = rs[b][h].band;
            for (int i = 0; i < BC_SUM_BLOCK / 2; ++i) {
                const short bs = (short)(band >> 6);
                low = clampr(low + ((bs * fs) >> 8));
                const int high = clampr(xin[i] - low - ((bs * ds) >> 8));
                band = clampr(band + (((short)(high >> 6) * fs) >> 8));
                sym_out[i] += ((short)(band >> 6) * ds) >> 5;   /* x d: unity at resonance, x 8 to sample units */
            }
            rs[b][h].low = low;
            rs[b][h].band = band;
            const int y = ((short)(band >> 6) * ds) >> 5, ay = y < 0 ? -y : y;
            peak += ay;                    /* the page's level: the block's last output, both resonators */
        }
        bc_sym[b] = (short)((((peak > 32767 ? 32767 : peak) * g) >> 15));
    }
    for (int i = 0; i < BC_SUM_BLOCK; ++i) {
        int z = on ? buf[i] + ((sym_out[i >> 1] * g) >> 14) : buf[i];   /* up to twice the resonance: it reaches no input, never a loop */
        /* the soft limiter: below 0.7 of full scale as it is, above eased toward full scale */
        const int a = z < 0 ? -z : z;
        if (a > 22938) {
            const int e = a - 22938;
            int y = 22938 + (9829 * e) / (e + 9829);
            ++lim_hold;
            z = z < 0 ? -y : y;
        }
        buf[i] = (short)(z > 32767 ? 32767 : z < -32768 ? -32768 : z);
    }
    for (int i = 0; i < BC_SUM_BLOCK; ++i)
        send[0][i] = send[1][i] = send[2][i] = send[3][i] = 0;
    for (int b = 0; b < BC_BELLS; ++b) {
        bc_ring[b] = (short)(ring_acc[b] > 32767 ? 32767 : ring_acc[b]);
        ring_acc[b] = 0;
    }
    bc_voices_now = renders;
    bc_clips = lim_hold;
    renders = 0;
}
