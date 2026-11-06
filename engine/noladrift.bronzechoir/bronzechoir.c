/* The Bronze Choir: four bronze church bells, an original synth engine for Opus 2 on the original OP-1.
 * Designed exclusively for Opus by Nola Drift.
 *
 * The engine glue: the voices (bc_dsp.c), the shift layer and the presets on T1, the pages (bc_page.c).
 *
 *   T1, plain turns (the OS's, knobs 0-3):  blue size, green strike, white metal, orange decay
 *   T1, shift + green:   sympathy (knob 5)      shift + orange:  detune (knob 7)
 *   T1, shift + blue / shift + white: taken and ignored (as outatime does), so the OS changes nothing unseen
 *   T1, orange push: the next preset            blue push: the previous preset
 *   T2: the struck bell's modes, live (the engine shapes its own envelope: the OS's T2 envelope is not used)
 */
#include "opus_plugin.h"
#include "bc_fw.h"
#include "bc_dsp.h"
#include "bc_page.h"
#include "bc_presets.h"

#define SHIFT_STEP      2                  /* of 99, a detent */
#define KEY_BLUE_PUSH   0x3B
#define KEY_ORANGE_PUSH 0x3E

static struct opl_self *me;
const struct bc_voice *volatile bc_newest;   /* the newest note's voice, for the T2 page */

static int k99(int v)                      /* a knob as 0-99 */
{
    return ((v < 0 ? 0 : v) * 99 + 16383) / 32767;
}

static int from99(int v)
{
    v = v < 0 ? 0 : v > 99 ? 99 : v;
    return (v * 32767 + 98) / 99;
}

/* ---------------------------------------------------------------- the voices */

static void v_init(void *voice, int index)
{
    bc_voice_init((struct bc_voice *)voice, index);
}

static void v_note_on(void *voice, const short *knobs)
{
    (void)knobs;                           /* the strike reads the knobs as the voice hears them, in its render */
    bc_note_on((struct bc_voice *)voice);
    bc_newest = (const struct bc_voice *)voice;
}

static void v_release(void *voice, const short *knobs)
{
    (void)knobs;
    bc_release((struct bc_voice *)voice);
}

/* Fast memory on lease (core 2.21): the render's scratch (filled each render before it is read) and the sine table
 * (set once). Each render takes them where the core put them this block, L1 while the engine plays, else the pool's
 * copies; without a lease the pool's copies alone. */
static struct opus_fast_lease *scratch_lease, *sine_lease;

static void v_render(void *voice, short *out, const short *par, const unsigned *pitch)
{
    bc_mem((int *)opus_fast_use(scratch_lease), (const short *)opus_fast_use(sine_lease));
    bc_render((struct bc_voice *)voice, out, par, pitch);
}

static int v_finished(void *voice)
{
    return bc_finished((const struct bc_voice *)voice);
}

static void e_block(short *buf)
{
    bc_block(buf);
}

/* ---------------------------------------------------------------- the presets */

/* The current sound's preset: its engine data's first byte, 1-8 (0: none chosen yet). */
int bc_preset_now(void)
{
    unsigned char d;
    return opus_engine_data(&d, 1) == 1 && d <= BC_PRESETS ? d : 0;
}

/* 1 when the sound's knobs are still the preset's (the page marks an edited preset). */
int bc_preset_intact(const short *k)
{
    const int p = bc_preset_now();
    if (!p)
        return 0;
    const struct bc_preset *q = &BC_PRESET[p - 1];
    const unsigned char want[6] = { q->size, q->strike, q->metal, q->decay, q->sympathy, q->detune };
    static const unsigned char KNOB[6] = { BC_SIZE, BC_STRIKE, BC_METAL, BC_DECAY, BC_SYMPATHY, BC_DETUNE };
    for (int i = 0; i < 6; ++i) {
        const int d = k99(k[KNOB[i]]) - want[i];
        if (d > 1 || d < -1)
            return 0;
    }
    return 1;
}

static void preset_apply(int p)            /* p 1-8; key thread */
{
    const struct bc_preset *q = &BC_PRESET[p - 1];
    opus_engine_knob_set(BC_SIZE, from99(q->size));
    opus_engine_knob_set(BC_STRIKE, from99(q->strike));
    opus_engine_knob_set(BC_METAL, from99(q->metal));
    opus_engine_knob_set(BC_DECAY, from99(q->decay));
    opus_engine_knob_set(BC_SYMPATHY, from99(q->sympathy));
    opus_engine_knob_set(BC_DETUNE, from99(q->detune));
    const unsigned char d = (unsigned char)p;
    opus_engine_data_set(&d, 1);
}

/* ---------------------------------------------------------------- the controls */

static int e_turn(int encoder, int detents, int page)
{
    if (page != 0 || !bc_shift_held())
        return 0;                          /* plain turns: the OS's (knobs 0-3) */
    if (detents && (encoder == 1 || encoder == 3)) {
        const int k = encoder == 1 ? BC_SYMPATHY : BC_DETUNE;
        short now[8];
        opus_engine_knobs(now);
        opus_engine_knob_set(k, from99(k99(now[k]) + SHIFT_STEP * detents));
    }
    return 1;                              /* shift + blue and shift + white: taken, nothing */
}

static int e_key(int key, int pressed, int page)
{
    if (page != 0 || (key != KEY_BLUE_PUSH && key != KEY_ORANGE_PUSH))
        return 0;
    if (pressed) {
        int p = bc_preset_now();
        if (key == KEY_ORANGE_PUSH)
            p = p >= BC_PRESETS ? 1 : p + 1;
        else
            p = p <= 1 ? BC_PRESETS : p - 1;
        preset_apply(p);
    }
    return 1;
}

/* ---------------------------------------------------------------- the engine */

static void page_t1(void)
{
    bc_page_t1();
}

static void page_t2(void)
{
    bc_page_t2(bc_newest);
}

static const struct opus_engine BRONZE_CHOIR = {
    .size = sizeof(struct opus_engine),
    .name = "bronzechoir",
    /* a new sound: size 50, strike 50, metal 50, decay 60, sympathy 20, detune 25 (0-99) */
    .knobs = { 16549, 16549, 16549, 19859, 0, 6620, 0, 8274 },
    .voice = sizeof(struct bc_voice),
    .init = v_init,
    .note_on = v_note_on,
    .release = v_release,
    .render = v_render,
    .block = e_block,
    .page = page_t1,
    .page_t2 = page_t2,
    .turn = e_turn,
    .key = e_key,
    .flags = OPUS_ENGINE_OWN_ENVELOPE,
    .finished = v_finished,
    .shift_knobs = { -1, BC_SYMPATHY, -1, BC_DETUNE },   /* for LFO types such as the matrix (core 2.21) */
};

static void fast_lease(struct opl_self *self)
{
    const unsigned sb = (unsigned)bc_scratch_bytes(), tb = BC_SINE_LEN * sizeof(short);
    int *const scratch = opus_alloc_audio(self, sb);
    short *const sine = opus_alloc_audio(self, tb);
    if (!scratch || !sine)
        return;                            /* no room: the render keeps its own static buffers */
    bc_sine_copy(sine);
    bc_mem(scratch, sine);
    scratch_lease = opus_fast_lease(self, sb, OPUS_FAST_SCRATCH, scratch);
    sine_lease = opus_fast_lease(self, tb, OPUS_FAST_CONST, sine);
}

int opl_init(struct opl_self *self)
{
    me = self;
    bc_sum_init();
    fast_lease(self);                      /* the render's scratch and sine in L1 while it plays */
    bc_page_init(self);
    return opus_engine_add(self, &BRONZE_CHOIR) < 0 ? -1 : 0;
}
