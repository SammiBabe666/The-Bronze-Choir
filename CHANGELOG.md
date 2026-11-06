# Changelog

## 1.1.0 (2026-11-06): native for Opus 2 beta 4

Built with the Opus 2 beta 4 RC1 plugin kit (core 2.30) and the shared reference toolchain (GCC 13.3.0, binutils 2.38). Needs core 2.21; for beta 3.x keep 1.0.1. The controls, the pages and the presets are unchanged.

- **All four bells on the keyboard, an octave up** (from the first device test). At the OP-1's normal octave (F2 to E4) the old boundaries (C3, C4, C5) left the chime out, and the low keys' hum and prime sat under what the OP-1's speaker plays, so they thudded. The bells now split the keyboard six keys each (below B2 the cathedral, B2 to E3 the church bell, F3 to A#3 the ceremonial bell, B3 and up the chime) and every bell sounds an octave above the key, still in tune. Each key keeps its old decay. On a computer the lowest keys have about twice the level above 300 Hz; the ten stress tests still show no clips, clicks or stuck voices.

- **Fast memory on lease.** The render's scratch and sine table (2,304 bytes) are no longer taken from L1 for good at the start (`opus_fast`); they are on two leases (`opus_fast_lease`, a scratch and a constant table), fast while the engine plays and handed back when it stops, so other plugins get that memory too. No `priority` line: the engine's voices stay where the core made them (it keeps pointers to them for the sympathy, so `OPUS_ENGINE_STATE_MOVES` is not set).
- **The bells' swing keeps time.** Beta 4 calls a still page only every fourth frame, and a late frame comes two periods on, so a pendulum stepped once a frame would lag and slow down. The page now steps it once per display period of the CPU's cycle counter (`bc_cycles`, in `bc_fw.c`).
- **The shift layer is named for LFO types** (`shift_knobs`: shift + green sympathy, shift + orange detune), so the matrix and other LFO types move the right knobs.
- The sine table is in the plugin once (it was compiled into two files).
- The shipped `.opmod` is now built with binutils 2.38 (1.0.x was built with 2.42), the reference toolchain.

## 1.0.1 (2026-11-05)

- Fixed a control clash with Outatime. On a device the mods list said "shift blue turn is taken by the bronze choir", so whichever of the two engines loaded second was refused. The shift turns on T1 are now declared `modal`: they are taken only while this engine's own page shows, which is also the only time the engine's turn handler runs. Modal controls never clash, so both engines can be installed together. No sound, page or parameter changed.
- The engine's two firmware calls (`fw_shift_held`, `fw_snprintf`) moved into one small file, `bc_fw.c`. It is now the only file that includes `op1fw.h`; the page and the glue call `bc_shift_held` and `bc_preset_title` instead.

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
