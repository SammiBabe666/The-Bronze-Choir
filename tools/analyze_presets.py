#!/usr/bin/env python3
"""analyze_presets.py: renders each of the eight presets through the real engine (host/bc_host) and measures how
they differ, so "eight genuinely different presets" is a measured claim, not only a listening one.

    python3 tools/analyze_presets.py [--wav-dir DIR]

For each preset it plays the same test: one strike on each bell (C2, D3, D4, G5) and one four-bell chord, and
measures on the result:
  brightness  the spectral centroid of the first second after a strike over the struck note's frequency (how far up
              the partials the energy sits), averaged over the four bells
  t60         the time a strike takes to fall 60 dB (extrapolated from its first 30 dB), s
  attack      the time from the strike to within 3 dB of its peak, ms
  beating     how much the ringing's loudness moves at 0.5-8 Hz (the doublets and instability), percent
  sympathy    the level the sympathetic resonators add to the four-bell chord, against the chord itself, dB (the
              same score rendered again with the sympathy knob at 0: the voices are identical, the difference is
              the resonators alone)
Prints a Markdown table (docs/PRESETS.md has the last run).
"""
import argparse
import os
import subprocess
import sys
import tempfile
import wave

import numpy as np

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
sys.path.insert(0, os.path.join(ROOT, 'engine'))
SR = 44100
NAMES = ['funeral procession', 'bronze garden', 'the ninth bell', 'little saint', 'cathedral rain',
         'distant warning', 'the brass procession', 'hollow vespers']
NOTES = [36, 50, 62, 79]
GAP = 7.0


def render(preset, path, chord=True, wav_dir=None):
    lines = []
    for _ in range(preset):
        lines.append('0 push orange')
    t = 0.2
    for m in NOTES:
        lines += [f'{t:.2f} on {m}', f'{t + 0.3:.2f} off {m}']
        t += GAP
    if chord:
        for m in NOTES:
            lines += [f'{t:.2f} on {m}', f'{t + 0.3:.2f} off {m}']
    lines.append(f'{t + GAP:.2f} end')
    with open(path + '.txt', 'w') as fh:
        fh.write('\n'.join(lines) + '\n')
    subprocess.run([os.path.join(ROOT, 'host', 'bc_host'), path + '.txt', path], check=True,
                   stderr=subprocess.DEVNULL)
    with wave.open(path) as w:
        x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float64) / 32768
    if wav_dir:
        os.makedirs(wav_dir, exist_ok=True)
        os.replace(path, os.path.join(wav_dir, f'preset_{preset}_{NAMES[preset - 1].replace(" ", "_")}.wav'))
    return x


def env_db(seg, hop=441):
    n = len(seg) // hop
    e = np.sqrt(np.mean(seg[:n * hop].reshape(n, hop) ** 2, axis=1)) + 1e-9
    return 20 * np.log10(e), e


def measure(x):
    cents, t60s, atts, beats = [], [], [], []
    for i in range(len(NOTES)):
        s0 = int((0.2 + i * GAP) * SR)
        seg = x[s0:s0 + int(GAP * SR) - 2000]
        first = seg[:SR] * np.hanning(SR)
        sp = np.abs(np.fft.rfft(first)) ** 2
        fr = np.fft.rfftfreq(SR, 1 / SR)
        f0 = 440.0 * 2 ** ((NOTES[i] - 69) / 12)
        cents.append(float((sp * fr).sum() / sp.sum()) / f0)
        db, lin = env_db(seg)
        pk = int(np.argmax(db))
        atts.append(float(np.nonzero(db[:pk + 1] >= db[pk] - 3)[0][0]) * 10.0)
        tail = db[pk:]
        below = np.nonzero(tail < db[pk] - 30)[0]
        t60s.append(2 * below[0] * 0.01 if len(below) else 2 * len(tail) * 0.01)
        # beating: the envelope's 0.5-8 Hz movement over 0.3-2.3 s, the decay's trend removed
        live = np.nonzero(db[pk:] < db[pk] - 40)[0]
        a, b = pk + 30, min(len(lin), pk + 230, pk + (live[0] if len(live) else 10 ** 6))
        if b - a > 50:
            y = np.log(lin[a:b])
            tt = np.arange(len(y))
            y = y - np.polyval(np.polyfit(tt, y, 2), tt)
            beats.append(float(np.std(y) * 100))
    # sympathy: the chord against the sum of the single strikes' level
    c0 = int((0.2 + len(NOTES) * GAP) * SR)
    chord = x[c0:c0 + 4 * SR]
    return float(np.exp(np.mean(np.log(cents)))), np.median(t60s), np.mean(atts), np.mean(beats) if beats else 0, chord


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--wav-dir')
    a = ap.parse_args()
    subprocess.run(['make', '-C', os.path.join(ROOT, 'host'), 'bc_host'], check=True, stdout=subprocess.DEVNULL)
    tmp = tempfile.mkdtemp()
    print('| preset | brightness (centroid / f0) | T60 (s) | attack (ms) | beating (%) | sympathy (dB) |')
    print('|---|---|---|---|---|---|')
    for p in range(1, 9):
        x = render(p, os.path.join(tmp, f'p{p}.wav'), wav_dir=a.wav_dir)
        cen, t60, att, beat, chord = measure(x)
        # sympathy: the same chord with sympathy forced to 0 (knob 5), compared over its 4 s
        path = os.path.join(tmp, f'p{p}_dry.wav')
        with open(os.path.join(tmp, f'p{p}.wav.txt')) as fh:
            lines = fh.read().split('\n')
        lines.insert(p, '0.01 knob 5 0')
        t = 0.2 + len(NOTES) * GAP
        with open(path + '.txt', 'w') as fh:
            fh.write('\n'.join(lines) + '\n')
        subprocess.run([os.path.join(ROOT, 'host', 'bc_host'), path + '.txt', path], check=True,
                       stderr=subprocess.DEVNULL)
        with wave.open(path) as w:
            y = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float64) / 32768
        c0 = int(t * SR)
        dry = y[c0:c0 + 4 * SR]
        n = min(len(dry), len(chord))
        diff = chord[:n] - dry[:n]
        sym_db = 20 * np.log10(np.sqrt(np.mean(diff ** 2)) / (np.sqrt(np.mean(dry[:n] ** 2)) + 1e-12) + 1e-9)
        sym = f'{sym_db:.1f}' if sym_db > -80 else 'off'
        print(f'| {p:02d} {NAMES[p - 1]} | {cen:.2f} | {t60:.1f} | {att:.0f} | {beat:.1f} | {sym} |', flush=True)


if __name__ == '__main__':
    main()
