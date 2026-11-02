/* bench: The Bronze Choir's audio code timed in the Blackfin instruction-set simulator (bfin-elf-run, built from
 * GDB's sources for the BF524's instruction set). The engine's very own bc_dsp.c is compiled with the plugin's own
 * flags; each audio block runs what the OS would run for it: up to six voices' renders (128 samples at 88.2 kHz) and
 * the sum's block handler (sympathy and limiter), timed with the CYCLES register as the Opus core times a plugin.
 *
 * The simulator counts each instruction's cycles (multi-cycle multiplies, branches, hardware loops) but not memory
 * stalls: SDRAM and cache misses on the OP-1 add to these figures. docs/TESTING.md estimates that share. */
#include "bc_dsp.h"

int sim_write(int fd, const void *b, int n);
void *memset(void *d, int c, unsigned n) { unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }
void *memcpy(void *d, const void *s, unsigned n) { unsigned char *p = d; const unsigned char *q = s; while (n--) *p++ = *q++; return d; }

static void puts_(const char *s) { int n = 0; while (s[n]) ++n; sim_write(1, s, n); }
static void putu(unsigned v) { char b[12]; int i = 11; b[i] = 0; do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v); puts_(b + i); }

static inline unsigned cycles(void) { unsigned c; __asm__ volatile ("%0 = CYCLES;" : "=d"(c)); return c; }

static struct bc_voice V[6];
static unsigned char fastmem[4096] __attribute__((aligned(32)));

static unsigned step_hz(unsigned milli_hz) { return (unsigned)(((unsigned long long)milli_hz << 32) / 88200000ull); }

/* one scenario: n voices struck at pitches (mHz), knobs k; returns the worst block's cycles over `blocks` blocks */
static unsigned scenario(const char *name, int n, const unsigned *mhz, const short *k, int blocks, int restrike)
{
    unsigned pitch[128];
    short out[128], sum[64];
    for (int i = 0; i < 6; ++i)
        bc_voice_init(&V[i], i);
    bc_sum_init();
    unsigned worst = 0, total = 0, worst_render = 0;
    for (int b = 0; b < blocks; ++b) {
        if (b == 0 || (restrike && b % restrike == 0))
            for (int i = 0; i < n; ++i)
                bc_note_on(&V[i]);
        const unsigned t0 = cycles();
        for (int i = 0; i < n; ++i) {
            const unsigned st = step_hz(mhz[i]);
            for (int j = 0; j < 128; ++j)
                pitch[j] = st;
            bc_render(&V[i], out, k, pitch);
            for (int j = 0; j < 64; ++j)
                sum[j] = out[2 * j];
        }
        const unsigned t1 = cycles();
        bc_block(sum);
        const unsigned t2 = cycles();
        const unsigned c = t2 - t0;
        worst = c > worst ? c : worst;
        worst_render = t1 - t0 > worst_render ? t1 - t0 : worst_render;
        total += c;
    }
    puts_("| "); puts_(name); puts_(" | "); putu(n); puts_(" | "); putu(worst); puts_(" | "); putu(total / blocks);
    puts_(" | "); putu(worst_render); puts_(" |\n");
    return worst;
}

int main(void)
{
    bc_fast(fastmem);
    static const short HEAVY[8] = { 32767, 32767, 32767, 32767, 0, 32767, 0, 32767 };
    static const short MID[8] = { 16384, 16384, 16384, 19859, 0, 6620, 0, 8274 };
    /* bell one (cathedral, 13 modes) is the heaviest bell: six voices of it, below C3 */
    static const unsigned LOW6[6] = { 65406, 73416, 82407, 87307, 97999, 110000 };
    static const unsigned WIDE6[6] = { 65406, 130813, 196000, 329628, 523251, 1046502 };
    static const unsigned ONE[1] = { 65406 };
    static const unsigned HIGH6[6] = { 523251, 659255, 783991, 1046502, 1318510, 2093005 };   /* the chime, 44.1 kHz */
    puts_("| scenario | voices | worst block (cycles) | mean block | worst voices-only |\n|---|---|---|---|---|\n");
    scenario("one cathedral voice, heaviest knobs", 1, ONE, HEAVY, 400, 0);
    scenario("six cathedral voices, heaviest knobs, fresh strikes", 6, LOW6, HEAVY, 200, 0);
    scenario("six cathedral voices, re-struck every 20 blocks", 6, LOW6, HEAVY, 400, 20);
    scenario("six voices across all four bells, heaviest knobs", 6, WIDE6, HEAVY, 400, 0);
    scenario("six voices across all four bells, default knobs", 6, WIDE6, MID, 400, 0);
    scenario("six chime voices (C5-C7), heaviest knobs", 6, HIGH6, HEAVY, 400, 0);
    scenario("six chime voices, re-struck every 20 blocks", 6, HIGH6, HEAVY, 400, 20);
    puts_("mode budget "); putu(BC_OSC_BUDGET); puts_(", cap per voice with six "); putu(bc_cap); puts_("\n");
    return 0;
}
