#!/usr/bin/env python3
"""render_frames.py: turns bc_host's drawing commands (one frame of the engine's page each) into pictures.

The commands are what the engine's page code called (opus_page_stroke_line, opus_page_dot, opus_page_bar,
opus_page_text) in the OP-1's 320 x 160 drawing units, recorded by host/os_sim.c. This draws them as the core would,
approximately: anti-aliased lines and dots in the stock encoder colours, the OP-1 font's capitals approximated with
Inter (the OP-1's own font is not used here). It is a picture of the same code that runs on the device, not a
screenshot of an OP-1.

    python3 host/render_frames.py FRAMES.txt OUTDIR [--scale 4] [--frames 0,10,20]
"""
import argparse
import os

from PIL import Image, ImageDraw, ImageFont

# The stock theme's encoder colours (approximations) and the palette ids the page uses.
COLOURS = {
    -10: (0x4D, 0x8B, 0xFF),   # E1 blue
    -11: (0x00, 0xE6, 0x8F),   # E2 green
    -12: (0xF2, 0xF2, 0xFF),   # E3 white
    -13: (0xFF, 0x6A, 0x1F),   # E4 orange
    14: (0xFF, 0xFF, 0xFF),    # text
    12: (0x7C, 0x79, 0x90),    # dim
    4: (0x35, 0x32, 0x38),     # off
    1: (0, 0, 0),
}
BAR_OFF = (0x2E, 0x2C, 0x33)
FONT_PATHS = ['/usr/share/fonts/opentype/inter/Inter-Medium.otf', '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf']
_fonts = {}


def font(px):
    if px not in _fonts:
        for p in FONT_PATHS:
            if os.path.exists(p):
                _fonts[px] = ImageFont.truetype(p, px)
                break
        else:
            _fonts[px] = ImageFont.load_default()
    return _fonts[px]


def colour(c):
    return COLOURS.get(c, (255, 255, 255))


def read_frames(path, wanted=None):
    """Yields (index, page, shift, commands, knobs) for each frame (only those in wanted, when given); knobs are the
    sound's size, strike, metal, decay, sympathy and detune (0-32767) as bc_host recorded them."""
    cur = None
    with open(path) as f:
        for line in f:
            if line.startswith('F '):
                if cur and (wanted is None or cur[0] in wanted):
                    yield cur
                f = line.split()
                cur = (int(f[1]), int(f[2]), int(f[3]), [], [int(x) for x in f[4:]])
            elif cur is not None and (wanted is None or cur[0] in wanted):
                cur[3].append(line.rstrip('\n'))
    if cur and (wanted is None or cur[0] in wanted):
        yield cur


def draw(cmds, scale=4, ss=2):
    """One frame's commands -> an RGB image of 320*scale x 160*scale pixels."""
    k = scale * ss
    img = Image.new('RGB', (320 * k, 160 * k), (0, 0, 0))
    d = ImageDraw.Draw(img)
    texts = []
    for c in cmds:
        p = c.split(' ')
        t = p[0]
        if t == 'S':
            x0, y0, x1, y1, w, col = map(int, p[1:7])
            wpx = max(1, round(w / 256 * scale * 0.8 * ss))
            pts = [(x0 / 256 * k, y0 / 256 * k), (x1 / 256 * k, y1 / 256 * k)]
            d.line(pts, fill=colour(col), width=wpx)
            r = wpx / 2
            for (x, y) in pts:
                d.ellipse([x - r, y - r, x + r, y + r], fill=colour(col))
        elif t == 'D':
            x, y, r, filled, col = map(int, p[1:6])
            box = [(x - r) * k, (y - r) * k, (x + r) * k, (y + r) * k]
            if filled:
                d.ellipse(box, fill=colour(col))
            else:
                d.ellipse(box, outline=colour(col), width=max(1, round(scale * 0.8 * ss)))
        elif t == 'B':
            x0, y0, x1, y1, v, m, col = map(int, p[1:8])
            d.rectangle([x0 * k, y0 * k, (x1 + 1) * k - 1, (y1 + 1) * k - 1], fill=BAR_OFF)
            lit = x0 + (x1 + 1 - x0) * (v / m if m else 0)
            if lit * k - 1 >= x0 * k:
                d.rectangle([x0 * k, y0 * k, lit * k - 1, (y1 + 1) * k - 1], fill=colour(col))
        elif t == 'T':
            q = c.split(' ', 6)
            texts.append((int(q[2]), int(q[3]), int(q[4]), int(q[5]), q[6] if len(q) > 6 else ''))
    img = img.resize((320 * scale, 160 * scale), Image.LANCZOS)
    d = ImageDraw.Draw(img)
    for x, y, size, col, s in texts:
        f = font(max(6, round(size * scale * 1.32)))
        d.text((x * scale, y * scale - size * scale * 0.18), s.upper(), font=f, fill=colour(col))
    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('frames')
    ap.add_argument('outdir')
    ap.add_argument('--scale', type=int, default=4)
    ap.add_argument('--frames', dest='which', default=None, help='comma-separated frame numbers')
    a = ap.parse_args()
    os.makedirs(a.outdir, exist_ok=True)
    wanted = set(int(x) for x in a.which.split(',')) if a.which else None
    for i, page, shift, cmds, knobs in read_frames(a.frames, wanted):
        draw(cmds, a.scale).save(os.path.join(a.outdir, f'frame_{i:05d}.png'))


if __name__ == '__main__':
    main()
