# Changelog

## 1.0.0 (2026-11-02)

First release.

- Four synthesized bells, one per register: cathedral (below C3), church (C3 to B3), ceremonial (C4 to B4) and chime (C5 and up). They use 13, 11, 10 and 8 inharmonic sine modes, with doublets, frequency-dependent exponential decays, an adjustable strike and impact noise.
- Six parameters: size, strike, metal and decay, plus sympathy and detune on the shift layer.
- Bounded, feedback-free sympathetic resonance between the four bells.
- Eight presets, stepped with the blue and orange pushes on T1.
- T1 page: four hanging line-art bells that swing as pendulums driven by the notes played, each with a clapper and an orange strike light. T2 page: the struck bell's modes, live.
- CPU: a mode budget shared by the live voices, summing at 22.05 or 44.1 kHz, and per-mode caches. The simulator's worst block is 98,846 cycles, about 17% of a block. `cpu.txt` is set to 140,000 and is a simulator estimate, not a device measurement.
- Not yet run on an OP-1 (see docs/KNOWN_LIMITATIONS.md).
