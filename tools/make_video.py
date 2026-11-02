#!/usr/bin/env python3
"""make_video.py: renders The Bronze Choir's demo and instrument tour (MP4) and the screenshots, reproducibly.

    python3 tools/make_video.py [--out media/the_bronze_choir_demo.mp4] [--stills-only] [--jobs 4]

What it does, in order:
 1. media/demo/make_score.py writes the timeline (media/demo/score.txt).
 2. host/bc_host plays it through the real engine sources (bc_dsp.c, bc_page.c, bronzechoir.c, the very files the
    .opmod is built from) in host/os_sim.c's stand-in for the OS: the audio (WAV, 44.1 kHz) and every video frame of
    the engine's page, as drawing commands.
 3. host/render_frames.py draws each frame's commands; this script puts them on a device-style mock-up with the four
    encoders turned to the sound's knob values, adds the score's titles and captions, and pipes the frames and the
    engine's audio to ffmpeg (H.264 + AAC, 1920 x 1080, 30 fps).

Needs: a C compiler and make, Python 3 with Pillow, ffmpeg. The picture is the engine's own drawing code run on a
computer, not a capture of an OP-1's screen; the device body is a mock-up.
"""
import argparse
import math
import os
import subprocess
import sys
from multiprocessing import Pool

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
sys.path.insert(0, os.path.join(ROOT, 'host'))
import render_frames  # noqa: E402

W, H, FPS = 1920, 1080, 30
BG = (12, 12, 14)
BODY = (28, 28, 32)
BODY_EDGE = (52, 52, 58)
ENC = [render_frames.COLOURS[-10], render_frames.COLOURS[-11], render_frames.COLOURS[-12], render_frames.COLOURS[-13]]
FONT = '/usr/share/fonts/opentype/inter/Inter-Medium.otf'
FONT_B = '/usr/share/fonts/opentype/inter/Inter-SemiBold.otf'
FONT_L = '/usr/share/fonts/opentype/inter/Inter-Light.otf'


def f(path, size):
    try:
        return ImageFont.truetype(path, size)
    except OSError:
        return ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', size)


def read_score(path):
    """The score's titles and captions: [(time, kind, line1, line2)]."""
    out = []
    with open(path) as fh:
        for line in fh:
            if line.startswith('#'):
                continue
            p = line.rstrip('\n').split(' ', 2)
            if len(p) >= 2 and p[1] in ('title', 'caption', 'card'):
                text = p[2] if len(p) > 2 else ''
                a, _, b = text.partition('|')
                out.append((float(p[0]), p[1], a, b))
    return out


def text_events(score):
    titles = [(t, a, b) for t, k, a, b in score if k == 'title']
    caps = [(t, a, b) for t, k, a, b in score if k in ('caption', 'card')]
    return titles, caps


def title_at(titles, t):
    """A title shows for 7 s (the opening) or to the end (the close); returns (alpha, line1, line2) or None."""
    for i, (t0, a, b) in enumerate(titles):
        t1 = t0 + 7.0 if i == 0 else 1e9
        if t0 <= t < t1:
            fade_in = min(1.0, (t - t0) / 0.8) if t0 > 0 else min(1.0, t / 0.5)
            fade_out = min(1.0, (t1 - t) / 0.8)
            return max(0.0, min(fade_in, fade_out)), a, b
    return None


def caption_at(caps, t):
    cur = None
    for t0, a, b in caps:
        if t0 <= t:
            cur = (t0, a, b)
    if not cur:
        return None
    t0, a, b = cur
    return min(1.0, (t - t0) / 0.35), a, b


def draw_encoder(d, cx, cy, r, colour, value, lit):
    d.ellipse([cx - r - 6, cy - r - 6, cx + r + 6, cy + r + 6], fill=(18, 18, 21))
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(40, 40, 46), outline=colour, width=6 if lit else 4)
    ang = math.radians(-225 + 270 * value / 32767.0)
    x1, y1 = cx + math.cos(ang) * (r - 10), cy + math.sin(ang) * (r - 10)
    d.line([(cx + math.cos(ang) * 10, cy + math.sin(ang) * 10), (x1, y1)], fill=colour, width=7)


def device(screen, knobs, shift):
    """The front-facing mock-up: a body, the screen, the four encoders turned to the sound's knobs."""
    img = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([250, 40, 1670, 905], radius=46, fill=BODY, outline=BODY_EDGE, width=3)
    d.rounded_rectangle([310, 78, 1610, 738], radius=18, fill=(0, 0, 0), outline=(60, 60, 68), width=2)
    img.paste(screen, (320, 88))
    # the encoders: plain turns size, strike, metal, decay; with shift, green and orange are sympathy and detune
    vals = list(knobs[:4]) if knobs else [0, 0, 0, 0]
    if shift and knobs and len(knobs) >= 6:
        vals[1], vals[3] = knobs[4], knobs[5]
    labels = ['size', 'sympathy' if shift else 'strike', 'metal', 'detune' if shift else 'decay']
    fl = f(FONT, 22)
    for i in range(4):
        cx = 560 + i * 267
        draw_encoder(d, cx, 818, 46, ENC[i], vals[i], not shift or i in (1, 3))
        d.text((cx, 878), labels[i].upper(), font=fl, fill=ENC[i] if (not shift or i in (1, 3)) else (90, 90, 98),
               anchor='mm')
    # shift
    d.rounded_rectangle([330, 800, 430, 836], radius=18, fill=(230, 230, 236) if shift else (48, 48, 54))
    d.text((380, 818), 'SHIFT', font=f(FONT_B, 18), fill=(20, 20, 24) if shift else (120, 120, 128), anchor='mm')
    return img


def caption(img, cap):
    if not cap:
        return
    alpha, a, b = cap
    d = ImageDraw.Draw(img)
    c1 = tuple(int(v * alpha) for v in (245, 245, 250))
    c2 = tuple(int(v * alpha) for v in (150, 150, 162))
    d.text((W // 2, 965), a, font=f(FONT_B, 40), fill=c1, anchor='mm')
    if b:
        d.text((W // 2, 1023), b, font=f(FONT, 29), fill=c2, anchor='mm')


def title(img, tl):
    alpha, a, b = tl
    over = Image.new('RGB', (W, H), (6, 6, 8))
    d = ImageDraw.Draw(over)
    big = f(FONT_L, 112)
    d.text((W // 2, 470), ' '.join(a), font=big, fill=(245, 245, 250), anchor='mm')
    # the four encoder colours as a rule under the title
    for i in range(4):
        d.rounded_rectangle([W // 2 - 300 + i * 152, 560, W // 2 - 300 + i * 152 + 140, 566], radius=3, fill=ENC[i])
    d.text((W // 2, 640), b, font=f(FONT, 40), fill=(200, 200, 210), anchor='mm')
    return Image.blend(img, over, alpha)


_titles = _caps = None


def _init(titles, caps):
    global _titles, _caps
    _titles, _caps = titles, caps


def compose(item):
    idx, page, shift, cmds, knobs = item
    t = idx / FPS
    screen = render_frames.draw(cmds, scale=4)
    img = device(screen, knobs, shift)
    caption(img, caption_at(_caps, t))
    tl = title_at(_titles, t)
    if tl and tl[0] > 0:
        img = title(img, tl)
    return idx, img.tobytes()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default=os.path.join(ROOT, 'media', 'the_bronze_choir_demo.mp4'))
    ap.add_argument('--work', default=os.path.join(ROOT, 'media', 'demo', 'build'))
    ap.add_argument('--stills-only', action='store_true')
    ap.add_argument('--jobs', type=int, default=os.cpu_count() or 2)
    a = ap.parse_args()
    os.makedirs(a.work, exist_ok=True)
    subprocess.run([sys.executable, os.path.join(ROOT, 'media', 'demo', 'make_score.py')], check=True)
    subprocess.run(['make', '-C', os.path.join(ROOT, 'host'), 'bc_host'], check=True, stdout=subprocess.DEVNULL)
    score = os.path.join(ROOT, 'media', 'demo', 'score.txt')
    wav = os.path.join(a.work, 'demo.wav')
    frames = os.path.join(a.work, 'frames.txt')
    subprocess.run([os.path.join(ROOT, 'host', 'bc_host'), score, wav, frames, str(FPS)], check=True)
    titles, caps = text_events(read_score(score))
    _init(titles, caps)
    stills(frames, titles, caps)
    if a.stills_only:
        return
    n = sum(1 for line in open(frames) if line.startswith('F '))
    ff = subprocess.Popen(['ffmpeg', '-y', '-loglevel', 'error', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s',
                           f'{W}x{H}', '-r', str(FPS), '-i', '-', '-i', wav, '-c:v', 'libx264', '-preset', 'slow',
                           '-crf', '21', '-pix_fmt', 'yuv420p', '-c:a', 'aac', '-b:a', '192k', '-shortest',
                           '-movflags', '+faststart', a.out], stdin=subprocess.PIPE)
    with Pool(a.jobs, initializer=_init, initargs=(titles, caps)) as pool:
        for k, (idx, data) in enumerate(pool.imap(compose, render_frames.read_frames(frames), chunksize=8)):
            ff.stdin.write(data)
            if k % 300 == 0:
                print(f'frame {k}/{n}', flush=True)
    ff.stdin.close()
    ff.wait()
    print('wrote', a.out)


STILLS = [
    # (seconds, file, what: 'device' the mock-up, 'screen' the page alone)
    (19.0, '01_mockup_front.png', 'device'),
    (19.0, '02_four_bells_at_rest.png', 'screen'),
    (25.75, '03_single_bell_swinging.png', 'screen'),
    (39.25, '04_four_bells_swinging.png', 'screen'),
    (36.95, '05_two_bells_swinging.png', 'screen'),
    (69.0, '06_parameter_feedback_shift.png', 'screen'),
    (128.2, '07_t2_modes_page.png', 'screen'),
    (57.0, '08_encoder_colours_device.png', 'device'),
    (151.0, '09_performance_device.png', 'device'),
]


def stills(frames, titles, caps):
    out = os.path.join(ROOT, 'media', 'screenshots')
    os.makedirs(out, exist_ok=True)
    want = {}
    for t, name, kind in STILLS:
        want.setdefault(int(round(t * FPS)), []).append((name, kind))
    for idx, page, shift, cmds, knobs in render_frames.read_frames(frames, set(want)):
        screen = render_frames.draw(cmds, scale=4)
        for name, kind in want[idx]:
            if kind == 'screen':
                screen.save(os.path.join(out, name))
            else:
                img = device(screen, knobs, shift)
                caption(img, caption_at(caps, idx / FPS))
                img.save(os.path.join(out, name))
            print('still', name)


if __name__ == '__main__':
    main()
