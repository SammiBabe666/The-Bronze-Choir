#!/usr/bin/env python3
"""make_tables.py: writes engine/noladrift.bronzechoir/bc_tables.h, every constant table The Bronze Choir's DSP reads.

The OP-1's Blackfin has no floating-point unit, so every curve the engine needs is computed here, once, and stored
as fixed-point integers. Run it after changing a bell's modes:

    python3 tools/make_tables.py

The four bells' modal structures live in BELLS below. Each mode is
    (ratio to the played pitch, level in dB, decay bias in octaves of T60, size coefficient in octaves, doublet)
and the modes of a bell are listed in perceptual priority order: when the engine's mode budget is short (many
voices at once) it drops modes from the end of the list first, so the nominal, prime, hum and tierce survive longest.
A doublet entry (doublet = +1 or -1) is the second half of a split mode: the same ratio, detuned up or down by the
DETUNE knob, which is what makes a real bell beat (its circular symmetry is never perfect).
"""
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'engine', 'noladrift.bronzechoir', 'bc_tables.h')

# ratio, dB, decay bias (octaves of T60), size coef (octaves of ratio at full size deviation), doublet
BELLS = {
    # Bell One: the cathedral bell. A minor-third church bell's partials (hum, prime, tierce, quint, nominal and
    # the upper partials) after the classic tuned-bell relationships, artistically shifted. Long hum.
    'cathedral': [
        (2.000,   0.0, 0.00,  0.000, 0),   # nominal
        (1.000,  -2.5, 0.45,  0.000, 0),   # prime (the strike note)
        (0.500,  -4.0, 1.10,  0.000, 0),   # hum
        (1.188,  -4.5, 0.25,  0.014, 0),   # tierce (minor third)
        (2.000,  -1.5, 0.00,  0.000, 1),   # nominal's doublet
        (2.662,  -8.0, 0.00, -0.012, 0),   # superquint
        (1.506, -10.0, 0.10, -0.008, 0),   # quint
        (0.500,  -6.0, 1.10,  0.000, -1),  # hum's doublet
        (3.011,  -9.0, 0.00, -0.015, 0),   # octave nominal
        (2.514, -12.0, 0.00, -0.010, 0),   # deciem
        (1.188,  -7.5, 0.25,  0.014, -1),  # tierce's doublet
        (4.166, -14.0, 0.00, -0.018, 0),   # upper octave
        (5.433, -18.0, 0.00, -0.020, 0),
    ],
    # Bell Two: a medium-large church bell. Sharper tierce, flat quint, brighter upper partials, shorter hum.
    'church': [
        (2.000,   0.0, 0.00,  0.000, 0),
        (1.000,  -2.0, 0.35,  0.000, 0),
        (0.502,  -6.0, 0.80,  0.000, 0),
        (1.204,  -5.0, 0.20,  0.012, 0),
        (2.000,  -2.0, 0.00,  0.000, 1),
        (2.610,  -7.0, 0.00, -0.010, 0),
        (1.470,  -9.0, 0.10, -0.006, 0),
        (3.050,  -9.0, 0.00, -0.014, 0),
        (0.502,  -8.0, 0.80,  0.000, -1),
        (4.020, -12.0, 0.00, -0.016, 0),
        (5.200, -16.0, 0.00, -0.018, 0),
    ],
    # Bell Three: a medium ceremonial bell (a cast handbell's voice): a strong fundamental and twelfth, a faint hum.
    'ceremonial': [
        (1.000,   0.0, 0.30,  0.000, 0),
        (3.000,  -4.0, 0.00, -0.010, 0),
        (2.420,  -6.0, 0.00,  0.010, 0),
        (1.000,  -2.0, 0.30,  0.000, 1),
        (2.080,  -9.0, 0.10,  0.008, 0),
        (4.130,  -9.0, 0.00, -0.012, 0),
        (3.000,  -6.0, 0.00, -0.010, -1),
        (5.380, -13.0, 0.00, -0.016, 0),
        (6.800, -17.0, 0.00, -0.020, 0),
        (0.500, -16.0, 0.80,  0.000, 0),
    ],
    # Bell Four: a small delicate chime: sparse, widely spaced, glassy partials.
    'chime': [
        (1.000,   0.0, 0.20,  0.000, 0),
        (2.320,  -5.0, 0.00,  0.015, 0),
        (4.250,  -7.0, 0.00,  0.020, 0),
        (1.000,  -2.5, 0.20,  0.000, 1),
        (2.980, -10.0, 0.00,  0.010, 0),
        (6.630, -11.0, 0.00,  0.025, 0),
        (2.320,  -7.0, 0.00,  0.015, -1),
        (9.380, -16.0, 0.00,  0.030, 0),
    ],
}
ORDER = ['cathedral', 'church', 'ceremonial', 'chime']
MAXMODES = max(len(v) for v in BELLS.values())


def q(x, bits):
    return int(round(x * (1 << bits)))


def main():
    L = []
    L.append('/* bc_tables.h: written by tools/make_tables.py; do not edit by hand. */')
    L.append('#ifndef BC_TABLES_H\n#define BC_TABLES_H\n')
    L.append('#define BC_SINE_LEN 1024')
    L.append('static const short BC_SINE[BC_SINE_LEN] = {')
    s = [max(-32767, min(32767, q(math.sin(2 * math.pi * i / 1024), 15))) for i in range(1024)]
    for i in range(0, 1024, 16):
        L.append('    ' + ', '.join(str(v) for v in s[i:i + 16]) + ',')
    L.append('};\n')
    L.append('/* 2^(-i/256) in Q15, i = 0..256 */')
    L.append('static const unsigned short BC_EXP2N[257] = {')
    e = [min(32768, q(2 ** (-i / 256), 15)) for i in range(257)]
    for i in range(0, 257, 16):
        L.append('    ' + ', '.join(str(v) for v in e[i:i + 16]) + ',')
    L.append('};\n')
    L.append('/* log2(1 + i/256) in Q12, i = 0..256 */')
    L.append('static const unsigned short BC_LOG2M[257] = {')
    g = [q(math.log2(1 + i / 256), 12) for i in range(257)]
    for i in range(0, 257, 16):
        L.append('    ' + ', '.join(str(v) for v in g[i:i + 16]) + ',')
    L.append('};\n')
    L.append(f'#define BC_MAXMODES {MAXMODES}')
    L.append('struct bc_mode_def {')
    L.append('    int ratio;          /* Q16: the mode\'s frequency over the played pitch */')
    L.append('    short lratio;       /* Q12: log2 of it */')
    L.append('    short level;        /* Q12: log2 of its amplitude (dB / 6.02) */')
    L.append('    short dbias;        /* Q12: octaves added to its T60 */')
    L.append('    short size;         /* Q12: octaves its ratio moves at the size knob\'s ends */')
    L.append('    signed char doublet;/* 0, or +1 / -1: the detuned half of a split mode */')
    L.append('};')
    L.append('struct bc_bell_def {\n    int n;\n    struct bc_mode_def m[BC_MAXMODES];\n};')
    L.append('static const struct bc_bell_def BC_BELL_DEF[4] = {')
    for name in ORDER:
        modes = BELLS[name]
        L.append(f'    {{ {len(modes)}, {{   /* {name} */')
        for r, db, dbias, sz, d in modes:
            L.append(f'        {{ {q(r, 16)}, {q(math.log2(r), 12)}, {q(db / (20 * math.log10(2)), 12)}, '
                     f'{q(dbias, 12)}, {q(sz, 12)}, {d} }},   /* {r:.3f} {db:+.1f} dB */')
        L.append('    } },')
    L.append('};\n')
    L.append('#endif')
    with open(OUT, 'w') as f:
        f.write('\n'.join(L) + '\n')
    print(f'wrote {os.path.normpath(OUT)}')


if __name__ == '__main__':
    main()
