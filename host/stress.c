/* stress: the ten stress tests of the spec, run through the real engine sources on a computer (os_sim.c).
 *
 *   stress [OUTDIR]     prints a Markdown table; with OUTDIR also writes each test's audio as a WAV there
 *
 * What it measures, per test: the output's peak; hard clips (samples at full scale); samples the soft limiter
 * eased; the most voices busy and the voice steals; voices still busy after the test's quiet tail (stuck voices);
 * clicks (a sample step far above the signal's local step level, away from note onsets); and the computer's time
 * per audio block. What it cannot measure: the OP-1's CPU cycles. Those come from sim/ (the Blackfin simulator)
 * and, for real, from the device's mods list (docs/TESTING.md). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bc_dsp.h"
#include "os_sim.h"

static FILE *wav;
static double t_now;
static double last_onset = -1;
static double peak;
static long clips, clicks, blocks_run, eased0;
static int max_busy;
static double cpu_max_us, cpu_sum_us;
static double hist[441];                   /* the last 10 ms of |step| for the click test */
static int hpos;
static double hsum;
static short prev_sample;

static unsigned rng = 12345;
static int rnd(int n)
{
    rng = rng * 1103515245u + 12345u;
    return (int)((rng >> 8) % (unsigned)n);
}

static void note_on(int m)
{
    os_note_on(m);
    last_onset = t_now;
}

static void run(double secs)
{
    const int n = (int)(secs * 44100.0 / 64.0);
    short buf[64];
    for (int b = 0; b < n; ++b) {
        struct timespec a, z;
        clock_gettime(CLOCK_MONOTONIC, &a);
        os_block(buf);
        clock_gettime(CLOCK_MONOTONIC, &z);
        const double us = (z.tv_sec - a.tv_sec) * 1e6 + (z.tv_nsec - a.tv_nsec) / 1e3;
        cpu_sum_us += us;
        cpu_max_us = us > cpu_max_us ? us : cpu_max_us;
        ++blocks_run;
        for (int i = 0; i < 64; ++i) {
            const double x = buf[i] / 32768.0, ax = fabs(x);
            peak = ax > peak ? ax : peak;
            if (buf[i] >= 32767 || buf[i] <= -32768)
                ++clips;
            const double st = fabs((double)buf[i] - prev_sample) / 32768.0;
            prev_sample = buf[i];
            const double mean = hsum / 441.0;
            if (st > 0.08 && st > 10 * mean && t_now - last_onset > 0.02)
            { ++clicks; if (getenv("BC_CLICKS")) fprintf(stderr, "click t=%.4f step=%.3f mean=%.4f busy=%d\n", t_now, st, mean, os_busy_voices()); }
            hsum += st - hist[hpos];
            hist[hpos] = st;
            hpos = (hpos + 1) % 441;
            t_now += 1 / 44100.0;
        }
        if (wav)
            fwrite(buf, 2, 64, wav);
        const int busy = os_busy_voices();
        max_busy = busy > max_busy ? busy : max_busy;
    }
}

static void knob99(int k, int v)
{
    os_set_knob(k, (v < 0 ? 0 : v > 99 ? 99 : v) * 32767 / 99);
}

static void defaults(void)
{
    static const int D[8] = { 50, 50, 50, 60, 0, 20, 0, 25 };
    for (int k = 0; k < 8; ++k)
        if (k != 4)
            knob99(k, D[k]);
}

static void chord(const int *m, int n, int on)
{
    for (int i = 0; i < n; ++i) {
        if (on)
            note_on(m[i]);
        else
            os_note_off(m[i]);
    }
}

static const int WIDE[6] = { 36, 48, 55, 64, 72, 84 };   /* all four bells */

static void t1(void) { knob99(BC_DECAY, 99); note_on(36); run(30); os_note_off(36); }
static void t2(void) { knob99(BC_DECAY, 99); note_on(84); run(30); os_note_off(84); }
static void t3(void)
{
    knob99(BC_DECAY, 80);
    knob99(BC_STRIKE, 99);
    knob99(BC_METAL, 99);
    for (int r = 0; r < 40; ++r) {
        chord(WIDE, 6, 1);
        run(0.4);
        chord(WIDE, 6, 0);
        run(0.1);
    }
}
static void t4(void)
{
    knob99(BC_STRIKE, 99);
    for (int r = 0; r < 200; ++r) {
        note_on(r & 1 ? 60 : 62);
        run(0.025);
        os_note_off(r & 1 ? 60 : 62);
        run(0.025);
    }
}
static void t5(void)
{
    knob99(BC_DECAY, 99);
    for (int r = 0; r < 48; ++r) {
        const int m = 36 + (r * 7) % 49;
        note_on(m);
        run(0.12);
        os_note_off(m);
        run(0.13);
    }
}
static void t6(void)
{
    knob99(BC_SYMPATHY, 99);
    knob99(BC_DECAY, 99);
    for (int r = 0; r < 10; ++r) {
        static const int OCTS[4] = { 36, 48, 60, 72 };   /* octaves: the strongest sympathetic case */
        chord(OCTS, 4, 1);
        run(1.5);
        chord(OCTS, 4, 0);
        run(0.5);
    }
}
static void t7(void)
{
    knob99(BC_DETUNE, 99);
    knob99(BC_DECAY, 90);
    for (int r = 0; r < 5; ++r) {
        chord(WIDE, 6, 1);
        run(3);
        chord(WIDE, 6, 0);
        run(1);
    }
}
static void t8(void)
{
    static const int CH[4] = { 43, 55, 67, 79 };
    chord(CH, 4, 1);
    for (int b = 0; b < 15 * 44100 / 64; ++b) {
        const double ph = b * 64 / 44100.0 / 0.3;   /* end to end in 0.15 s */
        const int v = (int)(99 * fabs(fmod(ph, 2.0) - 1.0));
        for (int k = 0; k < 8; ++k)
            if (k != 4 && k != 6)
                knob99(k, (k & 1) ? v : 99 - v);
        if (b % 200 == 0)
            chord(CH, 4, 1);
        run(64 / 44100.0);
    }
    chord(CH, 4, 0);
}
static void t9(void)
{
    for (int r = 0; r < 32; ++r) {
        chord(WIDE, 6, 1);
        run(0.25);
        os_key(0x3E, 1);                   /* the orange push: the next preset */
        os_key(0x3E, 0);
        run(0.25);
        chord(WIDE, 6, 0);
    }
}
static void t10(void)
{
    for (int s = 0; s < 600 * 10; ++s) {   /* ten minutes, a tenth of a second at a time */
        if (rnd(3) == 0)
            note_on(30 + rnd(62));
        if (rnd(3) == 0)
            os_note_off(30 + rnd(62));
        if (rnd(40) == 0) {
            os_key(rnd(2) ? 0x3E : 0x3B, 1);
            os_key(0x3E, 0);
        }
        if (rnd(20) == 0)
            knob99(rnd(4), rnd(100));
        if (rnd(200) == 0)
            os_all_off();
        run(0.1);
    }
}

struct test { const char *name; void (*fn)(void); };
static const struct test T[10] = {
    { "1 sustained low note (C2, decay 99, 30 s)", t1 },
    { "2 sustained high note (C6, decay 99, 30 s)", t2 },
    { "3 dense multi-octave chords (6 voices, 4 bells)", t3 },
    { "4 rapid repeated strikes (20 a second)", t4 },
    { "5 long overlapping release tails", t5 },
    { "6 maximum sympathetic resonance", t6 },
    { "7 maximum detuning", t7 },
    { "8 rapid encoder movement, notes held", t8 },
    { "9 preset switching during playback", t9 },
    { "10 extended operation (10 min random)", t10 },
};

int main(int argc, char **argv)
{
    os_init();
    printf("| test | peak | hard clips | limiter-eased samples | max voices | steals | stuck after tail | clicks | "
           "host us/block (mean / max) |\n|---|---|---|---|---|---|---|---|---|\n");
    for (int i = 0; i < 10; ++i) {
        defaults();
        if (argc > 1) {
            char path[512];
            snprintf(path, sizeof path, "%s/stress_%02d.wav", argv[1], i + 1);
            wav = fopen(path, "wb");
            fwrite("RIFF\0\0\0\0WAVEfmt \x10\0\0\0\1\0\1\0\x44\xac\0\0\x88\x58\1\0\2\0\x10\0data\0\0\0\0", 1, 44, wav);
        }
        peak = 0;
        clips = clicks = blocks_run = 0;
        max_busy = 0;
        cpu_max_us = cpu_sum_us = 0;
        const unsigned long long steals0 = os_steals;
        eased0 = bc_clips;
        T[i].fn();
        os_all_off();
        run(40);                           /* the quiet tail: every released voice must end */
        const int stuck = os_busy_voices();
        if (wav) {
            const long n = ftell(wav) - 44;
            unsigned v = (unsigned)(n + 36);
            fseek(wav, 4, SEEK_SET);
            fwrite(&v, 4, 1, wav);
            v = (unsigned)n;
            fseek(wav, 40, SEEK_SET);
            fwrite(&v, 4, 1, wav);
            fclose(wav);
            wav = 0;
        }
        printf("| %s | %.3f | %ld | %ld | %d | %llu | %d | %ld | %.1f / %.1f |\n", T[i].name, peak, clips,
               (long)bc_clips - eased0, max_busy, os_steals - steals0, stuck, clicks, cpu_sum_us / blocks_run,
               cpu_max_us);
        fflush(stdout);
    }
    return 0;
}
