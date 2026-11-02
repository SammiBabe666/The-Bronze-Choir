/* bc_host: plays a score through the real engine sources on a computer (os_sim.c), writing the audio as a WAV and
 * every video frame of the page as drawing commands (for host/render_frames.py).
 *
 *   bc_host SCORE OUT.wav [FRAMES.txt] [FPS]
 *
 * A score has one event a line, "TIME COMMAND ARGS" (seconds); '#' starts a comment; lines starting with
 * "caption" are the video's and ignored here:
 *   on MIDI | off MIDI | alloff
 *   knob K V99            set knob K (0-7) to V (0-99) at once
 *   ramp K FROM TO SECS   sweep knob K from FROM to TO (0-99)
 *   turn ENC DETENTS      an encoder turn on T1 (through the engine's handler, with the current shift)
 *   shift 0|1             hold or let go of shift
 *   push blue|orange      an encoder push on T1 (the presets)
 *   page 0|1              which page the video shows (T1, T2)
 *   end                   the score's end (rendering goes on until then)
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "os_sim.h"

struct ev { double t; char cmd[16]; double a, b, c, d; };
static struct ev evs[20000];
static int nev;

struct ramp { int k; double from, to, t0, t1; int on; };
static struct ramp ramps[16];

static int to_knob(double v99)
{
    const int v = (int)lrint(v99 * 32767.0 / 99.0);
    return v < 0 ? 0 : v > 32767 ? 32767 : v;
}

static void wav_header(FILE *f, unsigned frames)
{
    const unsigned bytes = frames * 2;
    unsigned char h[44] = "RIFF....WAVEfmt \x10\0\0\0\1\0\1\0....\0\0\0\0\2\0\x10\0data....";
    const unsigned rate = 44100, br = rate * 2;
    unsigned v = 36 + bytes;
    memcpy(h + 4, &v, 4);
    memcpy(h + 24, &rate, 4);
    memcpy(h + 28, &br, 4);
    memcpy(h + 40, &bytes, 4);
    fwrite(h, 1, 44, f);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: bc_host SCORE OUT.wav [FRAMES.txt] [FPS]\n");
        return 2;
    }
    FILE *sf = fopen(argv[1], "r");
    if (!sf) {
        perror(argv[1]);
        return 1;
    }
    char line[512];
    double end = 0;
    while (fgets(line, sizeof line, sf)) {
        char *h = strchr(line, '#');
        if (h)
            *h = 0;
        struct ev e = { 0 };
        char w[32] = "";
        const int n = sscanf(line, "%lf %15s %31s %lf %lf %lf", &e.t, e.cmd, w, &e.b, &e.c, &e.d);
        if (n < 2 || !strcmp(e.cmd, "caption") || !strcmp(e.cmd, "title") || !strcmp(e.cmd, "card"))
            continue;
        if (!strcmp(e.cmd, "push"))
            e.a = !strcmp(w, "orange") ? 1 : 0;
        else
            e.a = atof(w);
        if (!strcmp(e.cmd, "end"))
            end = e.t;
        evs[nev++] = e;
        if (e.t > end)
            end = e.t;
    }
    fclose(sf);
    FILE *wf = fopen(argv[2], "wb");
    FILE *ff = argc > 3 ? fopen(argv[3], "w") : 0;
    const double fps = argc > 4 ? atof(argv[4]) : 30.0;
    os_init();
    const unsigned blocks = (unsigned)ceil(end * 44100.0 / 64.0);
    wav_header(wf, blocks * 64);
    int ei = 0, page = 0;
    long frame = 0;
    short buf[64];
    for (unsigned b = 0; b < blocks; ++b) {
        const double t = b * 64.0 / 44100.0;
        while (ei < nev && evs[ei].t <= t) {
            const struct ev *e = &evs[ei++];
            if (!strcmp(e->cmd, "on"))
                os_note_on((int)e->a);
            else if (!strcmp(e->cmd, "off"))
                os_note_off((int)e->a);
            else if (!strcmp(e->cmd, "alloff"))
                os_all_off();
            else if (!strcmp(e->cmd, "knob"))
                os_set_knob((int)e->a, to_knob(e->b));
            else if (!strcmp(e->cmd, "ramp")) {
                for (int r = 0; r < 16; ++r)
                    if (!ramps[r].on || ramps[r].k == (int)e->a) {
                        ramps[r] = (struct ramp){ (int)e->a, e->b, e->c, e->t, e->t + e->d, 1 };
                        break;
                    }
            } else if (!strcmp(e->cmd, "turn"))
                os_turn((int)e->a, (int)e->b, os_shift);
            else if (!strcmp(e->cmd, "shift"))
                os_shift = (int)e->a;
            else if (!strcmp(e->cmd, "push")) {
                os_key(e->a ? 0x3E : 0x3B, 1);
                os_key(e->a ? 0x3E : 0x3B, 0);
            } else if (!strcmp(e->cmd, "page"))
                page = (int)e->a;
        }
        for (int r = 0; r < 16; ++r)
            if (ramps[r].on) {
                double u = (t - ramps[r].t0) / (ramps[r].t1 - ramps[r].t0);
                if (u >= 1) {
                    u = 1;
                    ramps[r].on = 0;
                }
                os_set_knob(ramps[r].k, to_knob(ramps[r].from + (ramps[r].to - ramps[r].from) * u));
            }
        os_block(buf);
        fwrite(buf, 2, 64, wf);
        while (ff && frame / fps <= t) {
            fprintf(ff, "F %ld %d %d %d %d %d %d %d %d\n", frame, page, os_shift, os_knobs[0], os_knobs[1], os_knobs[2],
                    os_knobs[3], os_knobs[5], os_knobs[7]);
            os_frame(ff, page);
            ++frame;
        }
    }
    fclose(wf);
    if (ff)
        fclose(ff);
    fprintf(stderr, "%u blocks (%.1f s), %ld frames, %llu voice steals\n", blocks, blocks * 64 / 44100.0, frame,
            os_steals);
    return 0;
}
