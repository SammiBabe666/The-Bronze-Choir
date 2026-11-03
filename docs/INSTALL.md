# Installing and building The Bronze Choir

## What you need

- An **original OP-1** running firmware 246, with **Opus 2** installed: public beta 3.2 (core 2.13) is current, and any core from 2.8 loads it. The engine uses `opus_page_still`, which was added in 2.8, and nothing newer.
- The plugin file, [`dist/bronzechoir.opmod`](../dist/bronzechoir.opmod).

## Install

1. **Developer mode** (recommended for a first run). Open Opus's menu by holding **synth + drum + tape + mixer** together, press **T1** for system settings, and switch developer mode on. While it is on, a plugin that crashes or goes over its CPU figure is reported but not switched off. That matters here because this engine's CPU figure comes from a simulator, not from a device ([TESTING.md](TESTING.md)).
2. **Disk mode:** press **shift + COM**, then **T3**, and connect the OP-1 to your computer.
3. Copy `bronzechoir.opmod` into the disk's **mods** folder. Eject the disk and switch the OP-1 off and on.
4. **Check it loaded:** open the menu again, press **T2**, and turn **blue** to *The Bronze Choir*. The row should say **loaded**. Push **orange** for its details, which include its memory use and its worst measured audio block in cycles. Please note that worst-block figure: it is the real CPU measurement this project still needs.
5. **Play it:** choose *The Bronze Choir* as a synth engine. T1 shows the four bells and T2 shows the struck bell's modes.

Once you are happy it runs, switch developer mode off again.

## Uninstall

Delete `bronzechoir.opmod` from the mods folder in disk mode. If the OP-1 ever fails to start with plugins installed, hold **Stop** while switching it on. That starts it with no plugins, and you can then delete the file. A plugin cannot touch te-boot, so you can always reinstall Opus by holding **COM** while switching on.

## Build from source

You need Python 3.8 or later and the Blackfin cross-compiler: `bfin-elf-gcc` 13.3.0 with binutils 2.38, the compiler Opus itself is built with. This repository does **not** contain or build the compiler. Use your existing shared Blackfin toolchain, built once with the Opus plugin kit's own recipe (`plugin-kit/tools/toolchain/build-bfin-toolchain.sh` in [LizardMan245/opus](https://github.com/LizardMan245/opus/tree/main/plugin-kit)). Point the kit at it:

```
export OP1_BFIN_TOOLCHAIN=/path/to/your/bfin-toolchain      # the folder that holds bin/bfin-elf-gcc
export PATH=$OP1_BFIN_TOOLCHAIN/bin:$PATH                  # for sim/run_bench.sh (bfin-elf-gcc, bfin-elf-run)
```

If your shared toolchain has an `env.sh` that sets these, source that instead.

The vendored kit leaves out `tools/re/`, `examples/` and `tools/toolchain/` ([VENDORED.txt](../third_party/opus-plugin-kit/VENDORED.txt)).

Then build:

```
python3 third_party/opus-plugin-kit/plugin_tool.py build engine/noladrift.bronzechoir --out dist/bronzechoir.opmod
python3 third_party/opus-plugin-kit/plugin_tool.py show dist/bronzechoir.opmod
```

The build treats every warning as an error and refuses anything the headers do not declare.

**How the shipped file was built.** GNU's FTP mirrors could not be reached from the build machine, so the compiler that built `dist/bronzechoir.opmod` was built from Ubuntu's `gcc-13-source` (GCC 13.3.0) and `binutils-source` (2.42, rather than the kit's 2.38) packages. Their GNU signatures were **not** checked. If that matters to you, rebuild the plugin with a toolchain built by the kit's own verified recipe (GCC 13.3.0, binutils 2.38). The result should behave the same, though the bytes may differ.

## Regenerating the bells' tables

The four bells' modal structures are defined in `tools/make_tables.py`. Running `python3 tools/make_tables.py` rewrites `engine/noladrift.bronzechoir/bc_tables.h`, which holds the modes plus the sine, exp2 and log2 tables.

## Running the computer-side tools

These run on Linux and need gcc, make, Python 3 with numpy and Pillow, and ffmpeg:

```
make -C host                                   # builds host/bc_host (score player) and host/stress
host/stress                                     # the ten stress tests (docs/TESTING.md)
python3 tools/analyze_presets.py                # the preset measurements (docs/PRESETS.md)
sim/run_bench.sh                                # the CPU benchmark in the Blackfin simulator (needs bfin-elf-run)
python3 tools/make_video.py                     # screenshots and the demo MP4 (docs/VIDEO.md)
```
