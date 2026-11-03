# Changelog

## Compatibility audit (2026-11-03): Opus 2 public beta 3.2

No change to the plugin. `dist/bronzechoir.opmod` is byte-identical.

- The vendored kit is the beta 3.2 kit (LizardMan245/opus `d7f28b0`, core 2.13), still the upstream head. The engine builds against it unchanged.
- Every core service the engine calls exists in core 2.13, and none is deprecated.
- The manifest keeps `core 2.8`. That is the lowest core whose services the engine uses, as the kit's manifest rules require. Raising it would only lock out older cores.
- Removed the kit's Blackfin compiler build script from the vendored copy. The compiler is shared infrastructure and lives outside the project. INSTALL.md now points at the shared toolchain.

## 1.0.0 (2026-11-02)

First release.

- Four synthesized bells, one per register: cathedral (below C3), church (C3 to B3), ceremonial (C4 to B4) and chime (C5 and up). They use 13, 11, 10 and 8 inharmonic sine modes, with doublets, frequency-dependent exponential decays, an adjustable strike and impact noise.
- Six parameters: size, strike, metal and decay, plus sympathy and detune on the shift layer.
- Bounded, feedback-free sympathetic resonance between the four bells.
- Eight presets, stepped with the blue and orange pushes on T1.
- T1 page: four hanging line-art bells that swing as pendulums driven by the notes played, each with a clapper and an orange strike light. T2 page: the struck bell's modes, live.
- CPU: a mode budget shared by the live voices, summing at 22.05 or 44.1 kHz, and per-mode caches. The simulator's worst block is 98,846 cycles, about 17% of a block. `cpu.txt` is set to 140,000 and is a simulator estimate, not a device measurement.
- Not yet run on an OP-1 (see docs/KNOWN_LIMITATIONS.md).
