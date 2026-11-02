# Presets

Eight presets, stepped on T1 with the **orange** push (next) and the **blue** push (previous). They wrap from 8 to 1. A preset writes all six knobs into the sound. The page shows its number and name, with `*` once any knob has moved away from the preset. Values are on a 0 to 99 scale and are defined in `engine/noladrift.bronzechoir/bc_presets.h`.

| # | Preset | Size | Strike | Metal | Decay | Sympathy | Detune | Character |
|---|---|---|---|---|---|---|---|---|
| 01 | **Funeral Procession** | 92 | 72 | 20 | 90 | 22 | 18 | A solemn, enormous cathedral bell: dark bronze, a long hum |
| 02 | **Bronze Garden** | 16 | 24 | 80 | 52 | 30 | 26 | A delicate garden of small ceremonial bells: soft touch, bright metal |
| 03 | **The Ninth Bell** | 84 | 62 | 74 | 88 | 50 | 64 | A massive bell with a complex metallic voice and strong beating |
| 04 | **Little Saint** | 6 | 58 | 56 | 20 | 0 | 4 | A tiny, intimate handbell: short, pure, dry |
| 05 | **Cathedral Rain** | 50 | 40 | 76 | 80 | 78 | 44 | Overlapping, shimmering bells that answer one another |
| 06 | **Distant Warning** | 96 | 10 | 6 | 86 | 14 | 30 | A huge bell far across an empty landscape: muffled, dark, slow attack |
| 07 | **The Brass Procession** | 44 | 90 | 88 | 44 | 34 | 16 | A bright ceremonial ensemble: hard, brassy, quick |
| 08 | **Hollow Vespers** | 66 | 30 | 96 | 96 | 26 | 82 | A haunting, unstable ensemble: brightest metal, the most detune |

## Measured differences

`python3 tools/analyze_presets.py` renders each preset through the real engine sources (`host/bc_host`). The same test is used for every preset: one strike on each bell (C2, D3, D4, G5), then a four-bell chord. It measures:

- **brightness:** the spectral centroid over the struck note's frequency, during the first second
- **T60:** time to fall 60 dB, extrapolated from the first 30 dB; median over the four bells
- **attack:** time to within 3 dB of the peak
- **beating:** how much the envelope moves at 0.5 to 8 Hz
- **sympathy:** the resonators' added level against the chord, found by rendering the same score again with sympathy at 0 and taking the difference

Last run, version 1.0.0:

| preset | brightness (centroid / f0) | T60 (s) | attack (ms) | beating (%) | sympathy (dB) |
|---|---|---|---|---|---|
| 01 funeral procession | 0.84 | 12.9 | 2 | 21.0 | -50.1 |
| 02 bronze garden | 1.16 | 1.6 | 12 | 19.3 | -44.6 |
| 03 the ninth bell | 1.82 | 10.8 | 8 | 32.0 | -26.6 |
| 04 little saint | 0.87 | 0.6 | 2 | 0.0 | off |
| 05 cathedral rain | 1.51 | 6.7 | 8 | 25.6 | -20.0 |
| 06 distant warning | 0.68 | 9.8 | 12 | 21.5 | -62.5 |
| 07 the brass procession | 1.68 | 0.9 | 2 | 25.0 | -32.3 |
| 08 hollow vespers | 2.19 | 11.3 | 10 | 28.8 | -38.1 |

How to read it:

- **Brightness below 1** means the energy sits under the struck note, in the hum (as for the large dark bells, 01 and 06). Higher values mean the energy is up among the partials.
- **Long and short.** 01, 03, 06 and 08 ring for about 10 to 13 s. 04 and 07 are gone in under a second.
- **Attack.** Soft strikes (02, 06, 08) give a slower contact than hard ones (01, 04, 07).
- **Sympathy.** 05 Cathedral Rain has the most audible sympathy and 04 Little Saint has none.

These are computer renders through an approximation of the OS's mix, not device recordings. They show that the presets differ and how. Judging them by ear on an OP-1 is still to be done.
