#!/usr/bin/env bash
# Build sim/bench.c with the engine's DSP for the Blackfin and run it in bfin-elf-run (docs/TESTING.md).
#   sim/run_bench.sh [BUDGET]
set -euo pipefail
cd "$(dirname "$0")"
E=../engine/noladrift.bronzechoir
B=${1:-}
DEF=(); [ -n "$B" ] && DEF=(-DBC_OSC_BUDGET="$B")
bfin-elf-gcc -mcpu=bf524-any -O2 -ffreestanding -fno-builtin -Wall -Wextra -Werror "${DEF[@]}" -I"$E" \
    -nostdlib -Wl,-e,__start -o bench.elf crt.S bench.c "$E/bc_dsp.c" -lgcc
bfin-elf-run bench.elf
