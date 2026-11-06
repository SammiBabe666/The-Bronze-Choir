# Known limitations

## Not verified on hardware

- **Never run on an OP-1, in a real Opus core, or in the Opus emulator.** The emulator has not been released yet. Everything in [TESTING.md](TESTING.md) ran on a computer: the real engine sources against an OS stand-in, and the Blackfin instruction simulator. The first device run should be done with developer mode on.
- **The low bell on the speaker.** The cathedral bell's hum and prime sit low even an octave up; on the OP-1's own speaker its lowest keys are about 6 dB weaker above 300 Hz than the top keys. Headphones or a line out show it whole.
- **Beta 4 (1.1.0) is mostly build-checked.** First device test (2026-11-06, Sammi): it loads and plays; the low keys thudded and the top keys missed the chime, which the new registers fix. Sammi played the fixed build on the OP-1 the same day and confirmed it works. Its CPU figure has not been read on the device yet. The fast-memory leases, the page's cycle-counter clock and the matrix on the shift layer have not been seen on a device. With nothing else playing the leases should hold L1; when heavier plugins of higher priority take it, the render reads its sine and scratch from the pool, which the simulator does not model (about 72 lines a block, roughly 8 k cycles at the docs' 107 a line).
- **`cpu.txt` (140,000 cycles) is a simulator estimate.** The simulator does not model memory stalls. If the real worst block is more than 175,000 (140,000 plus 25%), Opus would switch the plugin off outside developer mode until `cpu.txt` is corrected. Even then the engine would use under a third of the block.
- **Encoder pushes on T1** (blue and orange for the presets) are declared in the manifest as `synth t1 modal` and handled in the engine's key handler. How they interact with the OS's own push behaviour has not been seen on a device.
- **Two firmware calls.** The engine calls `fw_shift_held` (the same call the kit's outatime example uses for its shift layer) and `fw_snprintf`. They are firmware #246 functions, not core services. Both live in one small file, `engine/noladrift.bronzechoir/bc_fw.c`, the only file that includes `op1fw.h`, so a firmware or core change touches that file alone.

## The approximations in the computer renders

- The OS stand-in mixes six voices and decimates 88.2 to 44.1 kHz by averaging pairs. The OS's real decimation filter is not documented. The OS's voice allocation and stealing are also approximated.
- The screenshots and video draw the engine's own page commands with a rasterizer written for this project. The OP-1 screen's font is approximated (Inter), line widths and antialiasing are approximations, and the device body in the video is a mock-up. None of it is a capture of an OP-1's screen.

## By design

- **Six voices**, as the OS plays them. With six bells ringing, each voice keeps its bell's most important modes (at least 4) to stay within the CPU budget. A lone voice keeps every mode.
- **Fixed registers.** The keys B2, F3 and B3 are the bell boundaries (the OP-1's normal octave, F2 to E4, six keys a bell). They are not adjustable.
- **Strike acts on the next strike.** It shapes the excitation, so it does not change a bell that is already ringing. The other parameters are live.
- **The soft limiter** engages above 0.7 FS in dense six-voice playing at loud settings. In those cases it eases peaks rather than clipping.
- **Sympathy** follows each bell's **last struck** pitch, not every ringing voice of that bell.
- **No stereo.** Opus engines are mono.

## The toolchain

From 1.1.0 the shipped `.opmod` is built with the shared reference toolchain (GCC 13.3.0, binutils 2.38, the kit's own recipe). 1.0.x was built with binutils 2.42 from Ubuntu's unverified source packages.
