/* The Bronze Choir's eight presets: the six knobs as 0-99, in the order size, strike, metal, decay, sympathy,
 * detune. On the T1 page the orange push steps to the next preset and the blue push to the previous one; the knobs
 * are written into the sound (opus_engine_knob_set), so the OS's own hold-to-save keeps any of them as a user preset.
 * Designed exclusively for Opus by Nola Drift. */
#ifndef BC_PRESETS_H
#define BC_PRESETS_H

#define BC_PRESETS 8

struct bc_preset {
    const char *name;                      /* lower case: the OP-1's font draws capitals as icons */
    unsigned char size, strike, metal, decay, sympathy, detune;
};

static const struct bc_preset BC_PRESET[BC_PRESETS] = {
    /*                                 size strike metal decay sympathy detune */
    { "funeral procession",              92,   72,   20,   90,   22,     18 },
    { "bronze garden",                   16,   24,   80,   52,   30,     26 },
    { "the ninth bell",                  84,   62,   74,   88,   50,     64 },
    { "little saint",                     6,   58,   56,   20,    0,      4 },
    { "cathedral rain",                  50,   40,   76,   80,   78,     44 },
    { "distant warning",                 96,   10,    6,   86,   14,     30 },
    { "the brass procession",            44,   90,   88,   44,   34,     16 },
    { "hollow vespers",                  66,   30,   96,   96,   26,     82 },
};

#endif
