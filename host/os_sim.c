/* os_sim.c: the OS and the Opus core as the engine sees them, on a computer (os_sim.h says what it is and is not).
 * The opus_* and fw_* functions the plugin imports are defined here; the drawing ones write text commands that
 * host/render_frames.py turns into pictures. */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "opus_plugin.h"
#include "op1fw.h"
#include "os_sim.h"

int opl_init(struct opl_self *self);

static struct opl_self self = { sizeof(struct opl_self), "noladrift.bronzechoir", "/yaffs2/mod/noladrift.bronzechoir", 0 };
static const struct opus_engine *eng;
static unsigned char *vmem;
static int busy[6], held[6], note[6];
static unsigned age[6], clock_;
int os_shift;
short os_knobs[8];
unsigned long long os_steals;
static unsigned char data[OPUS_ENGINE_DATA];
static int data_n;
static FILE *cmdf;

/* ---------------------------------------------------------------- the core's services */

int opus_engine_add(struct opl_self *s, const struct opus_engine *e)
{
    (void)s;
    eng = e;
    vmem = calloc(6, e->voice);
    for (int k = 0; k < 8; ++k)
        os_knobs[k] = e->knobs[k];
    os_knobs[4] = 0x1234;                  /* the mark */
    for (int i = 0; i < 6; ++i)
        e->init(vmem + i * e->voice, i);
    return 0x1234;
}
int opus_engine_live(short par[8]) { memcpy(par, os_knobs, sizeof os_knobs); return 1; }
int opus_engine_knobs(short par[8]) { memcpy(par, os_knobs, sizeof os_knobs); return 1; }
int opus_engine_knob_set(int k, int v)
{
    if (k < 0 || k > 7 || k == 4)
        return 0;
    os_knobs[k] = (short)(v < 0 ? 0 : v > 32767 ? 32767 : v);
    return 1;
}
int opus_engine_data(void *buf, int max)
{
    const int n = data_n < max ? data_n : max;
    memcpy(buf, data, n);
    return n;
}
int opus_engine_data_set(const void *buf, int n)
{
    if (n < 0 || n > OPUS_ENGINE_DATA)
        return 0;
    memcpy(data, buf, n);
    data_n = n;
    return 1;
}
void *opus_fast(struct opl_self *s, unsigned bytes) { (void)s; return calloc(1, bytes); }
void opus_sincos(int t, int *s, int *c)
{
    const double a = t * 2 * M_PI / 4096.0;
    *s = (int)lrint(sin(a) * 4096);
    *c = (int)lrint(cos(a) * 4096);
}
int fw_shift_held(void) { return os_shift; }
int fw_snprintf(char *buf, unsigned n, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const int r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

/* the drawing: one text line a call */
int opus_page_still(struct opl_self *s, unsigned key) { (void)s; (void)key; return 0; }
void opus_page_still_done(struct opl_self *s) { (void)s; }
void opus_page_stroke_line(int x0, int y0, int x1, int y1, int w, int c)
{
    if (cmdf)
        fprintf(cmdf, "S %d %d %d %d %d %d\n", x0, y0, x1, y1, w, c);
}
void opus_page_dot(int x, int y, int r, int f, int c)
{
    if (cmdf)
        fprintf(cmdf, "D %d %d %d %d %d\n", x, y, r, f, c);
}
void opus_page_bar(int x0, int y0, int x1, int y1, unsigned v, unsigned m, int c)
{
    if (cmdf)
        fprintf(cmdf, "B %d %d %d %d %u %u %d\n", x0, y0, x1, y1, v, m, c);
}
void opus_page_text(struct opl_self *s, int slot, int x, int y, int size, int c, const char *t)
{
    (void)s;
    if (cmdf)
        fprintf(cmdf, "T %d %d %d %d %d %s\n", slot, x, y, size, c, t);
}

/* ---------------------------------------------------------------- the OS */

static unsigned step_of(int midi)
{
    const double f = 440.0 * pow(2.0, (midi - 69) / 12.0);
    return (unsigned)llround(f * 4294967296.0 / 88200.0);
}

void os_init(void)
{
    opl_init(&self);
}

void os_note_on(int midi)
{
    int v = -1;
    for (int i = 0; i < 6; ++i)            /* the same note held or ringing: retriggered */
        if (busy[i] && note[i] == midi)
            v = i;
    for (int i = 0; v < 0 && i < 6; ++i)
        if (!busy[i])
            v = i;
    if (v < 0) {                           /* all six busy: the oldest is taken */
        unsigned best = ~0u;
        for (int i = 0; i < 6; ++i)
            if (age[i] < best) {
                best = age[i];
                v = i;
            }
        ++os_steals;
    }
    busy[v] = held[v] = 1;
    note[v] = midi;
    age[v] = ++clock_;
    eng->note_on(vmem + v * eng->voice, os_knobs);
}

void os_note_off(int midi)
{
    for (int i = 0; i < 6; ++i)
        if (busy[i] && held[i] && note[i] == midi) {
            held[i] = 0;
            eng->release(vmem + i * eng->voice, os_knobs);
        }
}

void os_all_off(void)
{
    for (int i = 0; i < 6; ++i)
        if (busy[i] && held[i]) {
            held[i] = 0;
            eng->release(vmem + i * eng->voice, os_knobs);
        }
}

int os_busy_voices(void)
{
    int n = 0;
    for (int i = 0; i < 6; ++i)
        n += busy[i];
    return n;
}

void os_block(short *out64)
{
    static int sum[128];
    short vb[128];
    unsigned pitch[128];
    memset(sum, 0, sizeof sum);
    for (int i = 0; i < 6; ++i) {
        if (!busy[i])
            continue;
        void *vp = vmem + i * eng->voice;
        if (!held[i] && eng->finished && eng->finished(vp)) {
            busy[i] = 0;
            continue;
        }
        const unsigned st = step_of(note[i]);
        for (int j = 0; j < 128; ++j)
            pitch[j] = st;
        eng->render(vp, vb, os_knobs, pitch);
        for (int j = 0; j < 128; ++j)
            sum[j] += vb[j];
    }
    for (int j = 0; j < 64; ++j) {         /* 88.2 to 44.1 kHz: two averaged (the OS's own filter is not known) */
        const int s = (sum[2 * j] + sum[2 * j + 1]) >> 1;
        out64[j] = (short)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
    }
    if (eng->block)
        eng->block(out64);
}

void os_set_knob(int k, int v)
{
    opus_engine_knob_set(k, v);
}

void os_turn(int encoder, int detents, int shift)
{
    os_shift = shift;
    if (!eng->turn || !eng->turn(encoder, detents, 0)) {
        /* the OS's own: a plain turn on T1 moves knobs 0-3, about 1/99 a detent */
        const int k = encoder & 3, v = os_knobs[k] + detents * 331;
        os_knobs[k] = (short)(v < 0 ? 0 : v > 32767 ? 32767 : v);
    }
}

void os_key(int key, int pressed)
{
    if (eng->key)
        eng->key(key, pressed, 0);
}

void os_frame(FILE *cmd, int page)
{
    cmdf = cmd;
    if (page == 0 && eng->page)
        eng->page();
    else if (page == 1 && eng->page_t2)
        eng->page_t2();
    cmdf = 0;
}
