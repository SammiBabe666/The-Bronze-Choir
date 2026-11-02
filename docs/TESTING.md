# Testing and CPU

## What was and was not tested

| | Done? | How |
|---|---|---|
| Builds into a valid `.opmod` | **yes** | the kit's `plugin_tool.py build` (all warnings are errors), then read back with `plugin_tool.py show` |
| The ten stress tests | **yes, on a computer** | the real engine sources in `host/stress` against `host/os_sim.c` |
| CPU cost on the Blackfin | **yes, in a simulator** | `sim/`: the engine's DSP compiled with `bfin-elf-gcc -O2`, cycles counted in `bfin-elf-run` |
| The eight presets differ as intended | **yes, measured on a computer** | `tools/analyze_presets.py` ([PRESETS.md](PRESETS.md)) |
| Page drawing and animation | **yes, rendered on a computer** | the real `bc_page.c` drawing calls rasterized by `host/render_frames.py` (the screenshots and video) |
| **Running on an OP-1** | **no** | No device was available. |
| **Running in a real Opus core or the Opus emulator** | **no** | The emulator has not been released (the kit says it is still to come). |
| Listening on the device; the device's own CPU figure | **no** | This needs an OP-1 (see "To do on a device" below). |

## The ten stress tests

`make -C host stress && host/stress [WAV_DIR]` runs each test through the engine and the OS stand-in. That stand-in has six voices with oldest-first stealing, retrigger of a held note, the OS's mix and decimation, the shift layer, and encoder turns and pushes through the engine's own handlers. Each test is followed by a 40-block quiet tail, which checks that every released voice ends.

The columns:

- **peak:** the output's largest sample (1.0 = full scale)
- **hard clips:** samples at full scale
- **limiter-eased:** samples the soft limiter touched (above 0.7 FS)
- **stuck:** voices still busy after the tail
- **clicks:** sample steps that are over 0.08, over 10 times the local mean step, and not within 20 ms of a note onset
- **host µs/block:** wall time on the build machine, for relative comparison only

Results for 1.0.0:

| test | peak | hard clips | limiter-eased samples | max voices | steals | stuck after tail | clicks | host us/block (mean / max) |
|---|---|---|---|---|---|---|---|---|
| 1 sustained low note (C2, decay 99, 30 s) | 0.214 | 0 | 0 | 1 | 0 | 0 | 0 | 1.2 / 106.7 |
| 2 sustained high note (C6, decay 99, 30 s) | 0.264 | 0 | 0 | 1 | 0 | 0 | 0 | 0.6 / 147.9 |
| 3 dense multi-octave chords (6 voices, 4 bells) | 0.851 | 0 | 3770 | 6 | 0 | 0 | 0 | 3.2 / 304.7 |
| 4 rapid repeated strikes (20 a second) | 0.579 | 0 | 0 | 2 | 0 | 0 | 0 | 1.2 / 418.5 |
| 5 long overlapping release tails | 0.604 | 0 | 0 | 6 | 42 | 0 | 0 | 3.2 / 315.2 |
| 6 maximum sympathetic resonance | 0.776 | 0 | 25 | 4 | 0 | 0 | 0 | 3.0 / 155.3 |
| 7 maximum detuning | 0.590 | 0 | 0 | 6 | 0 | 0 | 0 | 2.8 / 47.3 |
| 8 rapid encoder movement, notes held | 0.911 | 0 | 16326 | 4 | 0 | 0 | 0 | 3.0 / 2093.7 |
| 9 preset switching during playback | 0.851 | 0 | 31090 | 6 | 0 | 0 | 0 | 3.3 / 323.3 |
| 10 extended operation (10 min random) | 0.855 | 0 | 715694 | 6 | 1754 | 0 | 0 | 4.5 / 3264.2 |

Summary: in all ten tests there were **no hard clips, no stuck voices and no detected clicks**.

- **The limiter.** It engages only in dense or extreme playing. In the 10-minute random test it touched about 2.7% of samples: six voices of random bells, strikes and knob values, with 1754 steals. Test 8 sweeps all six knobs to their extremes with notes held. Test 9 switches presets while six voices ring.
- **Sympathy.** At maximum (test 6) it stays bounded at a peak of 0.78.
- **Bugs found and fixed while making these pass:**
  - a wobble-correction overflow;
  - stale interpolation state after silence;
  - a loudness match that depended on strike history;
  - an impact-noise colour that could wrap negative into a full-scale Nyquist tone;
  - release tails long enough to hold voices indefinitely.

## CPU: the Blackfin simulator

`sim/run_bench.sh` compiles `sim/bench.c` with the engine's real `bc_dsp.c` for the Blackfin (`-mcpu=bf524-any -O2`). It runs it in GDB's Blackfin simulator, reads the CYCLES counter around every audio block (six voices' renders plus `bc_block`), and reports the worst and mean block. The scenarios use the heaviest knobs: the longest decay, sympathy and detune at maximum, and the brightest metal. In the re-struck scenarios a voice is struck again every 20 blocks, because a strike is the most expensive block.

Results for 1.0.0 (mode budget 54):

| scenario | voices | worst block (cycles) | mean block | worst voices-only |
|---|---|---|---|---|
| one cathedral voice, heaviest knobs | 1 | 29187 | 23299 | 16343 |
| six cathedral voices, heaviest knobs, fresh strikes | 6 | 96734 | 64535 | 83890 |
| six cathedral voices, re-struck every 20 blocks | 6 | 98846 | 70281 | 85999 |
| six voices across all four bells, heaviest knobs | 6 | 93814 | 64431 | 78663 |
| six voices across all four bells, default knobs | 6 | 93493 | 62691 | 78342 |
| six chime voices (C5-C7), heaviest knobs | 6 | 87637 | 58513 | 72486 |
| six chime voices, re-struck every 20 blocks | 6 | 89374 | 67036 | 74223 |

- **Worst case.** About **98,800 cycles, 17% of the 580,525-cycle block**, with six voices. The mean is around 11%.
- **One voice** costs about 29,000 cycles at worst.
- **History.** The first working version cost 238,000 cycles at worst. The changes described in [ARCHITECTURE.md](ARCHITECTURE.md#cpu-strategy) brought it down to the figure above.

**Why `cpu.txt` says 140,000.** Opus switches off a plugin that goes 25% over its declared figure in 8 of 64 blocks. A plugin with no figure is held to 58,052 cycles, which this engine needs more than. The simulator counts instructions but **not memory stalls**: the SDRAM and cache misses on the voice state and tables. So the shipped figure is the simulator's worst case plus about 16,000 cycles of memory allowance plus a margin. **It is an estimate, and `cpu.txt` says so.** It should be replaced with the worst block that the mods list reports on a device.

## To do on a device

On a device, these remain:

1. Install with developer mode on and confirm the mods list says "loaded".
2. Play six-voice chords across all registers with sympathy and detune at maximum. Read the worst block from the plugin's details (orange push on its row) and put it in `cpu.txt` as `cycles N`.
3. Check that the blue and orange pushes on T1 step the presets. Check that shift with green and orange turns sympathy and detune, and that the plain turns still work.
4. Listen through all eight presets. Check that save and reload keep the preset and knobs. Run one of the kit's `docs/sharing.md` checks: switching sounds, recording to tape, disk mode.
5. Watch the T1 animation at the device's frame rate.
