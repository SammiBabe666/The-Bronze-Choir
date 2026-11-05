/* The Bronze Choir's pages. Designed exclusively for Opus by Nola Drift.
 *
 * T1: four bronze bells hanging from a beam in a frame, at four heights, each with its yoke, axle, clapper and
 * mouth, drawn as thin line art in the four encoder colours: blue the frame, green the hangers and axles, white the
 * bells and clappers, orange the strike lights. Each bell is a damped pendulum (bc_swing_step): a strike of its
 * register (counted by the audio, bc_strikes) pushes it, its clapper follows with a lag, and it settles back. Under
 * the bells: the four knobs as bars in their colours, the two shift knobs as thin bars under green and orange.
 *
 * The page reads what the audio wrote (single words: counters and levels) and never writes anything the audio
 * reads: the animation costs the audio nothing. Its still part (the frame and the labels) is kept by the core
 * (opus_page_still) and only the bells and bars are drawn every frame, with opus_page_stroke_line, the cheap line.
 *
 * T2: the newest note's modes, live: one line a mode at its frequency ratio (log scale), as tall as it rings. */
#include "bc_page.h"
#include "bc_fw.h"
#include "bc_presets.h"

static struct opl_self *me;

struct bell_geo {
    short cx, py, h;                       /* the axle (units) and the bell's height */
    short omega;                           /* the pendulum's angular frequency, Q12 radians a frame */
    short k;                               /* omega^2, Q12 */
    short imp;                             /* the strongest push (amplitude x omega) */
    short amax;                            /* the largest swing, 1/16 of 1/4096 turn */
};

/* Bell One the largest, far left; Bell Four the smallest, far right; four hanging heights. Periods (at the OS's
 * 28.5 frames a second): 1.7, 1.4, 1.1 and 0.85 s. */
static const struct bell_geo GEO[BC_BELLS] = {
    {  54, 30, 60, 536,  70, 196, 1500 },
    { 138, 42, 46, 643, 101, 267, 1700 },
    { 213, 28, 35, 804, 158, 393, 2000 },
    { 276, 50, 25, 1072, 281, 628, 2400 },
};

/* A bell's half profile from the lip up to the crown: (half width, depth) as 1/256 of its height, the crown at 0. */
#define PROF_N 12
static const short PROF[PROF_N][2] = {
    { 142, 256 }, { 136, 246 }, { 118, 228 }, { 100, 200 }, { 88, 165 }, { 82, 128 },
    { 79, 92 }, { 76, 60 }, { 70, 34 }, { 58, 16 }, { 38, 6 }, { 0, 2 },
};
#define CROWN   20                         /* the crown's depth below the axle, 1/256 of the height */

struct bc_swing bc_swings[BC_BELLS];

void bc_page_init(struct opl_self *self)
{
    me = self;
    for (int b = 0; b < BC_BELLS; ++b) {
        bc_swings[b].ang = bc_swings[b].vel = bc_swings[b].cl = bc_swings[b].cv = 0;
        bc_swings[b].flash = 0;
        bc_swings[b].seen = bc_strikes[b];
        bc_swings[b].dir = b & 1 ? -1 : 1;
    }
}

static int iabs(int x)
{
    return x < 0 ? -x : x;
}

/* One frame of the four pendulums. k: the sound's knobs (decay sets the swing's damping, strike its push). */
void bc_swing_step(const short *k)
{
    const int dc = k[BC_DECAY] < 0 ? 0 : k[BC_DECAY];
    const int zeta = 492 - ((dc * 369) >> 15);             /* Q12: 0.12 (short decay) .. 0.03 (long) */
    for (int b = 0; b < BC_BELLS; ++b) {
        struct bc_swing *s = &bc_swings[b];
        const struct bell_geo *g = &GEO[b];
        const unsigned n = bc_strikes[b];
        if (n != s->seen) {                /* struck: a push, along the swing if it moves, and the light */
            s->seen = n;
            const int st = bc_strike_amt[b];
            const int push = (g->imp * (14746 + ((st * 18022) >> 15))) >> 15;   /* 0.45 .. 1 of the strongest */
            if (iabs(s->vel) > 8)
                s->dir = s->vel > 0 ? 1 : -1;
            s->vel += s->dir * push;
            s->dir = -s->dir;
            const int vmax = (g->imp * 5) >> 2;
            s->vel = s->vel > vmax ? vmax : s->vel < -vmax ? -vmax : s->vel;
            s->flash = 7;
        } else if (s->flash)
            --s->flash;
        /* sympathy: a bell the others make ring sways a little, along its own motion */
        const int sym = bc_sym[b];
        if (sym > 300 && iabs(s->ang) < (g->amax >> 2)) {
            const int nudge = (sym * g->imp) >> 19;
            s->vel += (s->vel >= 0 ? 1 : -1) * (nudge > 0 ? nudge : 1);
        }
        /* the damped pendulum (semi-implicit Euler, a frame a step) */
        const int c = (2 * zeta * g->omega) >> 12;
        s->vel += (-(g->k * s->ang) - (c * s->vel)) >> 12;
        s->ang += s->vel;
        if (iabs(s->ang) < 6 && iabs(s->vel) < 3)
            s->ang = s->vel = 0;
        /* the clapper: a stiffer pendulum hung from the crown, dragged by the bell, stopped by the rim */
        const int rel = s->cl - s->ang, rv = s->cv - s->vel;
        s->cv += (-((g->k * 9 >> 2) * rel) - ((g->omega >> 1) * rv)) >> 12;
        s->cl += s->cv;
        const int lim = 640;               /* the rim, about 3.5 degrees from the bell's axis */
        if (s->cl - s->ang > lim) {
            s->cl = s->ang + lim;
            s->cv = s->vel;
        } else if (s->cl - s->ang < -lim) {
            s->cl = s->ang - lim;
            s->cv = s->vel;
        }
        if (iabs(s->cl) < 6 && iabs(s->cv) < 3 && !s->ang && !s->vel)
            s->cl = s->cv = 0;
    }
}

/* A point of a bell's own drawing (x, y in units x 256, the axle at 0) turned by (sn, cs) around its axle. */
static inline void turn_pt(int cx, int cy, int x, int y, int sn, int cs, int *ox, int *oy)
{
    *ox = cx + ((x * cs - y * sn) >> 12);
    *oy = cy + ((x * sn + y * cs) >> 12);
}

static void bell_draw(int b, int scale)    /* scale: Q12 of the size knob's look */
{
    const struct bell_geo *g = &GEO[b];
    const struct bc_swing *s = &bc_swings[b];
    const int h = (g->h * scale) >> 12;    /* units */
    const int cx = g->cx << 8, cy = g->py << 8;
    int sn, cs;
    opus_sincos(s->ang >> 4, &sn, &cs);
    const int white = OPUS_COLOR_E3, green = OPUS_COLOR_E2;
    /* the outline: left lip, up the left side, the crown, down the right side, right lip */
    int px = 0, py = 0;
    for (int j = 0; j < 2 * PROF_N - 1; ++j) {
        const int i = j < PROF_N ? j : 2 * PROF_N - 2 - j;   /* left: lip to crown; right: crown to lip */
        const int x = (j < PROF_N ? -PROF[i][0] : PROF[i][0]) * h, y = (PROF[i][1] + CROWN) * h;
        int ox, oy;
        turn_pt(cx, cy, x, y, sn, cs, &ox, &oy);
        if (j)
            opus_page_stroke_line(px, py, ox, oy, 384, white);
        px = ox;
        py = oy;
    }
    /* the mouth: the lip's ellipse seen a little from below, and the inner edge of the opening */
    static const short MOUTH[5][2] = { { -256, 0 }, { -181, 9 }, { 0, 13 }, { 181, 9 }, { 256, 0 } };
    const int lip = (256 + CROWN) * h, w = PROF[0][0], wi = PROF[0][0] - 14;
    for (int e = 0; e < 2; ++e) {
        const int ww = e ? wi : w, yy = e ? lip - 6 * h : lip, dy = e ? 1 : 2;
        for (int j = 0; j < 5; ++j) {
            int ox, oy;
            turn_pt(cx, cy, (MOUTH[j][0] * ww >> 8) * h, yy + (MOUTH[j][1] * h * dy >> 1), sn, cs, &ox, &oy);
            if (j)
                opus_page_stroke_line(px, py, ox, oy, e ? 256 : 384, white);
            px = ox;
            py = oy;
        }
    }
    /* the sound bow's band */
    int ax, ay, bx, by;
    turn_pt(cx, cy, -PROF[3][0] * h, (PROF[3][1] + CROWN) * h, sn, cs, &ax, &ay);
    turn_pt(cx, cy, PROF[3][0] * h, (PROF[3][1] + CROWN) * h, sn, cs, &bx, &by);
    opus_page_stroke_line(ax, ay, bx, by, 256, white);
    /* the yoke (headstock) on the axle */
    const int yw = 72 * h, yt = -18 * h, yb = (CROWN + 4) * h;
    int x0, y0, x1, y1, x2, y2, x3, y3;
    turn_pt(cx, cy, -yw, yt, sn, cs, &x0, &y0);
    turn_pt(cx, cy, yw, yt, sn, cs, &x1, &y1);
    turn_pt(cx, cy, yw, yb, sn, cs, &x2, &y2);
    turn_pt(cx, cy, -yw, yb, sn, cs, &x3, &y3);
    opus_page_stroke_line(x0, y0, x1, y1, 320, green);
    opus_page_stroke_line(x1, y1, x2, y2, 320, green);
    opus_page_stroke_line(x2, y2, x3, y3, 320, green);
    opus_page_stroke_line(x3, y3, x0, y0, 320, green);
    /* the clapper: hung inside the crown, its own angle (the lag), its ball near the sound bow */
    int csn, ccs;
    opus_sincos(s->cl >> 4, &csn, &ccs);
    const int hx = cx + ((-(CROWN + 10) * h * sn) >> 12), hy = cy + (((CROWN + 10) * h * cs) >> 12);
    const int len = 196 * h;
    const int tx = hx + ((-len * csn) >> 12), ty = hy + ((len * ccs) >> 12);
    opus_page_stroke_line(hx, hy, tx, ty, 320, white);
    const int r = h / 22 + 1;
    opus_page_dot(tx >> 8, ty >> 8, r, 1, white);
    /* the axle */
    opus_page_dot(g->cx, g->py, 2, 1, green);
    /* the strike light under the bell: lit at the strike, fading out */
    const int ly = g->py + ((h * (256 + CROWN + 14)) >> 8) + 9;
    if (s->flash) {
        opus_page_dot(g->cx, ly, 1 + (s->flash + 1) / 2, 1, OPUS_COLOR_E4);
        if (s->flash > 4)
            opus_page_dot(g->cx, ly, 6, 0, OPUS_COLOR_E4);
    } else
        opus_page_dot(g->cx, ly, 1, 0, OPUS_COLOR_E4);
}

static void still_part(int shift)
{
    /* the frame: a beam on two legs, blue */
    const int blue = OPUS_COLOR_E1, green = OPUS_COLOR_E2;
    opus_page_stroke_line(4 << 8, 14 << 8, 316 << 8, 14 << 8, 384, blue);
    opus_page_stroke_line(4 << 8, 18 << 8, 316 << 8, 18 << 8, 256, blue);
    opus_page_stroke_line(8 << 8, 18 << 8, 3 << 8, 116 << 8, 384, blue);
    opus_page_stroke_line(312 << 8, 18 << 8, 317 << 8, 116 << 8, 384, blue);
    opus_page_stroke_line(1 << 8, 116 << 8, 12 << 8, 116 << 8, 384, blue);
    opus_page_stroke_line(308 << 8, 116 << 8, 319 << 8, 116 << 8, 384, blue);
    /* the hangers from the beam to each axle, green */
    for (int b = 0; b < BC_BELLS; ++b) {
        const int x = GEO[b].cx << 8, y = GEO[b].py << 8;
        opus_page_stroke_line(x - (5 << 8), 18 << 8, x - (2 << 8), y, 256, green);
        opus_page_stroke_line(x + (5 << 8), 18 << 8, x + (2 << 8), y, 256, green);
    }
    static const char *const NAME[4] = { "size", "strike", "metal", "decay" };
    static const char *const SHIFTED[4] = { "-", "sympathy", "-", "detune" };
    for (int i = 0; i < 4; ++i)
        opus_page_text(me, 1 + i, 10 + 78 * i, 122, 8, OPUS_COLOR_E(i), shift ? SHIFTED[i] : NAME[i]);
}

void bc_page_t1(void)
{
    short k[8];
    opus_engine_knobs(k);
    const int shift = bc_shift_held();
    bc_swing_step(k);
    const int p = bc_preset_now();
    if (!opus_page_still(me, (unsigned)(shift + 2))) {
        still_part(shift);
        opus_page_still_done(me);
    }
    /* the preset: its name, marked when its knobs were turned since */
    static char name[32];
    bc_preset_title(name, sizeof name, p, p ? BC_PRESET[p - 1].name : "", p && !bc_preset_intact(k));
    opus_page_text(me, 0, 10, 3, 8, OPUS_COLOR_TEXT, name);
    /* the bells: the size knob makes all four a little larger or smaller (0.92 .. 1.08) */
    const int sz = k[BC_SIZE] < 0 ? 0 : k[BC_SIZE];
    const int scale = 3768 + ((sz * 655) >> 15);
    for (int b = 0; b < BC_BELLS; ++b)
        bell_draw(b, scale);
    /* the knobs: the four as bars, the shift two as thin bars under green and orange */
    for (int i = 0; i < 4; ++i) {
        const int x = 10 + 78 * i;
        const int v = k[i] < 0 ? 0 : k[i];
        if (shift && (i == 0 || i == 2))
            opus_page_bar(x, 135, x + 66, 138, 0, 32767, OPUS_COLOR_E(i));
        else if (shift)
            opus_page_bar(x, 135, x + 66, 138, (unsigned)(k[i == 1 ? BC_SYMPATHY : BC_DETUNE] & 32767), 32767,
                          OPUS_COLOR_E(i));
        else
            opus_page_bar(x, 135, x + 66, 138, (unsigned)v, 32767, OPUS_COLOR_E(i));
        if (!shift && (i == 1 || i == 3))
            opus_page_bar(x, 142, x + 66, 143, (unsigned)(k[i == 1 ? BC_SYMPATHY : BC_DETUNE] & 32767), 32767,
                          OPUS_COLOR_E(i));
    }
}

/* ---------------------------------------------------------------- T2: the modes */

static int log2_q8(int a)                  /* log2 of a positive amplitude, Q8 (a rough one: the page's) */
{
    if (a <= 0)
        return 0;
    int e = 0;
    while (a >= 512) {
        a >>= 1;
        ++e;
    }
    while (a < 256) {
        a <<= 1;
        --e;
    }
    return ((e + 8) << 8) + (a - 256);     /* the mantissa's straight-line log */
}

void bc_page_t2(const struct bc_voice *v)
{
    static const char *const BELL[4] = { "bell one: cathedral", "bell two: church", "bell three: ceremonial",
                                         "bell four: chime" };
    if (!opus_page_still(me, 100u + (unsigned)(v && v->bell >= 0 ? v->bell : 4))) {
        opus_page_stroke_line(16 << 8, 120 << 8, 304 << 8, 120 << 8, 256, OPUS_COLOR_E1);
        for (int o = 0; o <= 4; ++o) {     /* the octaves of the ratio: 0.5, 1, 2, 4, 8 */
            const int x = 16 + ((o * 4096 * 288) / (4 * 4096 + 2048)) + 11;
            opus_page_stroke_line(x << 8, 120 << 8, x << 8, 124 << 8, 256, OPUS_COLOR_E1);
        }
        opus_page_still_done(me);
    }
    opus_page_text(me, 10, 10, 3, 8, OPUS_COLOR_TEXT, v && v->bell >= 0 ? BELL[(int)v->bell] : "strike a bell");
    if (!v || v->bell < 0)
        return;
    const struct bc_bell_def *b = &BC_BELL_DEF[(int)v->bell];
    for (int m = 0; m < b->n; ++m) {
        const int lr = b->m[m].lratio + 4096 + (b->m[m].doublet ? b->m[m].doublet * 300 : 0);   /* from 0.5 */
        const int x = 16 + (lr * 288) / (4 * 4096 + 2048) + 11;
        const int a = bc_mode_amp(v, m);
        int hgt = a > 0 ? (log2_q8(a) - (15 << 8) + (12 << 8)) * 96 / (12 << 8) : 0;   /* -72 dB .. 0 */
        hgt = hgt < 0 ? 0 : hgt > 96 ? 96 : hgt;
        const int col = b->m[m].doublet ? OPUS_COLOR_E4 : b->m[m].lratio < 0 ? OPUS_COLOR_E1 :
                        b->m[m].lratio == 4096 ? OPUS_COLOR_E2 : OPUS_COLOR_E3;
        if (hgt)
            opus_page_stroke_line(x << 8, 119 << 8, x << 8, (119 - hgt) << 8, 512, col);
        opus_page_dot(x, 119 - hgt, 1, 1, col);
    }
}
