# noladrift.bronzechoir: The Bronze Choir

An Opus 2 synth engine for the original OP-1: four bronze bells by inharmonic additive / modal synthesis.
Designed exclusively for Opus by Nola Drift.

Build from the repository root:

```
python3 third_party/opus-plugin-kit/plugin_tool.py build engine/noladrift.bronzechoir --out dist/bronzechoir.opmod
```

The repository's README.md and docs/ describe the sound, the controls, the presets, the tests and the limitations.
`cpu.txt` is a Blackfin-simulator estimate, not a device measurement (docs/TESTING.md).
