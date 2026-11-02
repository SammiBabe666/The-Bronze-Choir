#!/usr/bin/env python3
"""make_score.py: writes media/demo/score.txt, the demo and instrument tour's timeline.

Every sound in the video is the engine's own output: host/bc_host plays this score through the real engine
sources (the same bc_dsp.c, bc_page.c and bronzechoir.c that build the .opmod) and records the audio and every
frame of the page. 'title', 'card' and 'caption' lines are the video's text (tools/make_video.py); bc_host skips them.
Notes are MIDI numbers: bell one below 48 (C3), bell two 48-59, bell three 60-71, bell four 72 and up.
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
L = []


def ev(t, *a):
    L.append(f'{t:.3f} ' + ' '.join(str(x) for x in a))


def note(t, m, dur=0.4):
    ev(t, 'on', m)
    ev(t + dur, 'off', m)


def caption(t, text):
    ev(t, 'caption', text)


# ---------------------------------------------------------------- 0-7 the title
ev(0.0, 'push', 'orange')                      # preset 1, funeral procession
ev(0.0, 'title', 'THE BRONZE CHOIR|Designed exclusively for Opus by Nola Drift')
note(1.0, 36, 3.0)
note(3.6, 31, 2.0)

# ---------------------------------------------------------------- 7-21 the concept, the four bells
ev(7.0, 'card', '')
caption(7.0, 'An original instrument engine for Opus on the original OP-1|'
             'four bronze church bells, synthesized in real time: no samples, no recordings')
caption(12.5, 'Inharmonic additive / modal synthesis in fixed point|'
              'running on the OP-1\'s own Blackfin processor')
caption(16.5, 'Four bells, four registers|below C3, C3 to B3, C4 to B4, C5 and up')

# ---------------------------------------------------------------- 21-41 octave-triggered bells
for k, v in ((0, 50), (1, 60), (2, 55), (3, 55), (5, 20), (7, 25)):
    ev(20.5, 'knob', k, v)
caption(21.0, 'Bell one: the cathedral bell|notes below C3')
note(21.3, 36, 1.0)
note(23.0, 43, 0.6)
caption(25.0, 'Bell two: the church bell|C3 to B3')
note(25.3, 50, 1.0)
note(27.0, 55, 0.6)
caption(29.0, 'Bell three: the ceremonial bell|C4 to B4')
note(29.3, 62, 1.0)
note(30.8, 67, 0.6)
caption(33.0, 'Bell four: the small chime|C5 and up')
note(33.3, 79, 0.6)
note(34.2, 84, 0.6)
note(35.0, 88, 0.6)
caption(36.5, 'Only the struck register\'s bell swings|a chord across octaves swings each of its bells')
for m in (36, 55):
    note(36.8, m, 1.0)
for m in (36, 50, 64, 79):
    note(38.8, m, 1.2)

# ---------------------------------------------------------------- 41-79 the six parameters
caption(41.0, 'BLUE  bell size|the body: hum weight, partial tuning, ring length (the pitch stays in tune)')
ev(41.0, 'ramp', 0, 5, 95, 5.5)
for i in range(5):
    note(41.3 + i * 1.15, 50, 0.4)
caption(47.0, 'GREEN  strike|a soft ceremonial touch to a hard bright clapper')
ev(47.0, 'knob', 0, 50)
ev(47.0, 'ramp', 1, 3, 99, 5.5)
for i in range(6):
    note(47.3 + i * 0.95, 62, 0.3)
caption(53.0, 'WHITE  metal|warm dark bronze to bright, complex metal')
ev(53.0, 'knob', 1, 60)
ev(53.0, 'ramp', 2, 0, 99, 5.5)
for i in range(5):
    note(53.3 + i * 1.15, 55, 0.4)
caption(59.0, 'ORANGE  decay|a tight chime to an enormous cathedral ring')
ev(59.0, 'knob', 2, 55)
ev(59.0, 'knob', 3, 5)
note(59.3, 60, 0.3)
note(60.3, 64, 0.3)
ev(61.0, 'ramp', 3, 5, 95, 2.0)
note(63.2, 48, 0.4)
caption(65.0, 'SHIFT + GREEN  sympathetic resonance|the other bells answer, never feeding back')
ev(65.0, 'shift', 1)
for i in range(16):
    ev(65.2 + i * 0.2, 'turn', 1, 3)               # through the engine's own turn handler
for m in (36, 48, 60):
    note(65.4, m, 0.5)
for m in (36, 48, 60):
    note(68.4, m, 0.5)
caption(72.0, 'SHIFT + ORANGE  detuning|split modes beat: metallic movement')
for i in range(16):
    ev(72.2 + i * 0.2, 'turn', 3, 3)
note(72.4, 45, 2.0)
note(72.4, 57, 2.0)
note(75.6, 69, 1.0)
ev(78.5, 'shift', 0)
ev(78.6, 'alloff')

# ---------------------------------------------------------------- 79-127 the eight presets (the pushes on T1)
ev(79.0, 'push', 'blue')
ev(79.02, 'push', 'orange')                    # 8 -> 1: preset one applied again
PRESETS = [
    ('01  Funeral Procession', 'a solemn, enormous cathedral bell',
     [(0.3, 36, 2.0), (2.6, 31, 2.0)]),
    ('02  Bronze Garden', 'a delicate garden of small ceremonial bells',
     [(0.2 + i * 0.42, m, 0.25) for i, m in enumerate((72, 76, 79, 84, 79, 76, 72, 74, 79))]),
    ('03  The Ninth Bell', 'a massive bell with a complex metallic voice',
     [(0.3, 40, 1.5), (0.3, 52, 1.5), (2.8, 47, 1.0)]),
    ('04  Little Saint', 'a tiny intimate handbell',
     [(0.2 + i * 0.3, m, 0.15) for i, m in enumerate((84, 88, 91, 96, 91, 88, 84, 86, 91, 84))]),
    ('05  Cathedral Rain', 'overlapping, shimmering bells',
     [(0.2 + i * 0.45, m, 0.3) for i, m in enumerate((60, 64, 67, 72, 55, 48, 64, 79))]),
    ('06  Distant Warning', 'a huge bell far across an empty landscape',
     [(0.3, 33, 1.5), (3.0, 40, 1.5)]),
    ('07  The Brass Procession', 'a bright ceremonial ensemble',
     [(0.2 + i * 0.33, m, 0.2) for i, m in enumerate((55, 62, 67, 62, 55, 62, 67, 74, 67, 62, 55, 60))]),
    ('08  Hollow Vespers', 'a haunting, unstable ensemble',
     [(0.3, 46, 1.0), (1.6, 58, 1.0), (2.9, 65, 1.0), (4.2, 77, 0.8)]),
]
t0 = 79.0
for i, (name, what, notes) in enumerate(PRESETS):
    t = t0 + i * 6.0
    if i:
        ev(t, 'push', 'orange')
    caption(t, f'Preset {name}|{what}')
    for dt, m, d in notes:
        note(t + dt, m, d)

# ---------------------------------------------------------------- 127-143 the synthesized architecture (T2)
ev(127.0, 'alloff')
for _ in range(3):
    ev(127.0, 'push', 'orange')                # 8 -> 1 -> 2 -> 3: the ninth bell
ev(127.0, 'page', 1)
caption(127.0, 'T2: the struck bell\'s modes, live|8 to 13 sine modes at inharmonic bell ratios: hum, prime, tierce, quint, nominal...')
note(127.4, 36, 1.0)
caption(131.5, 'Each mode decays exponentially at its own rate|higher modes die sooner; the hum rings longest')
note(131.8, 62, 1.0)
caption(135.5, 'Doublets: split modes a few cents apart beat like real bronze|(orange lines)')
note(135.8, 79, 0.8)
caption(139.0, 'Each bell its own modal structure and personality|cathedral, church, ceremonial, chime')
note(139.3, 50, 1.0)

# ---------------------------------------------------------------- 143-170 a performance
ev(143.0, 'page', 0)
ev(143.0, 'alloff')
ev(143.0, 'push', 'orange')
ev(143.02, 'push', 'orange')                   # 3 -> 5: cathedral rain
caption(143.0, 'Performance: layered and sustained|six voices, all four bells')
for m in (36, 55, 64, 72):
    note(143.4, m, 3.0)
for i, m in enumerate((76, 79, 84, 79)):
    note(145.0 + i * 0.6, m, 0.3)
for m in (41, 57, 65, 77):
    note(149.0, m, 3.0)
for i, m in enumerate((81, 84, 89)):
    note(150.6 + i * 0.6, m, 0.3)
ev(154.6, 'push', 'orange')
ev(154.62, 'push', 'orange')
ev(154.64, 'push', 'orange')                   # 5 -> 8: hollow vespers
caption(154.6, 'Hollow Vespers|')
for m in (43, 59, 67, 79):
    note(155.0, m, 3.0)
for i, m in enumerate((74, 71, 67)):
    note(157.0 + i * 0.8, m, 0.4)
for m in (36, 48, 60, 72, 84):
    note(161.0, m, 4.0)
ev(165.0, 'alloff')

# ---------------------------------------------------------------- 170-178 the close
ev(170.0, 'title', 'THE BRONZE CHOIR|Designed exclusively for Opus by Nola Drift')
ev(178.0, 'end')

L.sort(key=lambda s: float(s.split()[0]))
with open(os.path.join(HERE, 'score.txt'), 'w') as f:
    f.write('# The Bronze Choir: the demo and tour score (written by media/demo/make_score.py)\n')
    f.write('\n'.join(L) + '\n')
print('wrote', os.path.join(HERE, 'score.txt'))
