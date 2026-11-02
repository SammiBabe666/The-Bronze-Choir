# Known limitations

## Not verified on hardware

- **Never run on an OP-1, in a real Opus core, or in the Opus emulator.** The emulator has not been released yet. Everything in [TESTING.md](TESTING.md) ran on a computer: the real engine sources against an OS stand-in, and the Blackfin instruction simulator. The first device run should be done with developer mode on.
- **`cpu.txt` (140,000 cycles) is a simulator estimate.** The simulator does not model memory stalls. If the real worst block is more than 175,000 (140,000 plus 25%), Opus would switch the plugin off outside developer mode until `cpu.txt` is corrected. Even then the engine would use under a third of the block.
- **Encoder pushes on T1** (blue and orange for the presets) are declared in the manifest as `synth t1 modal` and handled in the engine's key handler. How they interact with the OS's own push behaviour has not been seen on a device.
- **`fw_shift_held`** is a firmware call, the same one the kit's outatime example uses for its shift layer. It is not a core service.

## The approximations in the computer renders

- The OS stand-in mixes six voices and decimates 88.2 to 44.1 kHz by averaging pairs. The OS's real decimation filter is not documented. The OS's voice allocation and stealing are also approximated.
- The screenshots and video draw the engine's own page commands with a rasterizer written for this project. The OP-1 screen's font is approximated (Inter), line widths and antialiasing are approximations, and the device body in the video is a mock-up. None of it is a capture of an OP-1's screen.

## By design

- **Six voices**, as the OS plays them. With six bells ringing, each voice keeps its bell's most important modes (at least 4) to stay within the CPU budget. A lone voice keeps every mode.
- **Fixed registers.** C3, C4 and C5 are the bell boundaries. They are not adjustable.
- **Strike acts on the next strike.** It shapes the excitation, so it does not change a bell that is already ringing. The other parameters are live.
- **The soft limiter** engages above 0.7 FS in dense six-voice playing at loud settings. In those cases it eases peaks rather than clipping.
- **Sympathy** follows each bell's **last struck** pitch, not every ringing voice of that bell.
- **No stereo.** Opus engines are mono.

## The toolchain

The shipped `.opmod` was built with GCC 13.3.0 and **binutils 2.42**, built from Ubuntu's source packages, because GNU's mirrors could not be reached. The kit specifies binutils 2.38 from GNU, with signatures verified. The Ubuntu sources were not signature-checked. Rebuilding with the kit's own script gives the reference build ([INSTALL.md](INSTALL.md)).
