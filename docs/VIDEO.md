# The demo and tour video

[`media/the_bronze_choir_demo.mp4`](../media/the_bronze_choir_demo.mp4) is 1920×1080, 30 fps, H.264 plus AAC, and 178 s long. It opens and closes with the title card **THE BRONZE CHOIR / Designed exclusively for Opus by Nola Drift**.

## What is in it

**Real engine output:**
- Every sound is the engine's own output. `host/bc_host` plays the score through the same `bc_dsp.c`, `bc_page.c` and `bronzechoir.c` that the `.opmod` is built from.
- Every screen frame is the engine's own page drawing (T1 or T2) at that moment.
- Every knob move, shift turn and preset push goes through the engine's own handlers.

**Computer-made, not from an OP-1:**
- The screen is rasterized on a computer, not captured from an OP-1.
- The device body, the encoders turned to the knob values, and the shift indicator are a mock-up.
- The captions and title cards are added.

## Timeline

| Time | Section | What you see and hear |
|---|---|---|
| 0:00–0:07 | Title | THE BRONZE CHOIR / Designed exclusively for Opus by Nola Drift, over the cathedral bell |
| 0:07–0:21 | Concept | An original Opus engine; inharmonic modal synthesis, no samples; four bells, four registers |
| 0:21–0:41 | Octave-triggered bells | Each bell struck in its register: cathedral, church, ceremonial, chime. Only the struck bell swings. Two-bell and four-bell chords swing several bells |
| 0:41–0:47 | Blue: size | Size swept from small to enormous over repeated strikes |
| 0:47–0:53 | Green: strike | Soft to hard |
| 0:53–0:59 | White: metal | Dark bronze to bright metal |
| 0:59–1:05 | Orange: decay | Tight chime to a long ring |
| 1:05–1:12 | Shift + green: sympathy | Turned up through the engine's turn handler; a three-bell chord, then again |
| 1:12–1:19 | Shift + orange: detune | Turned up; beating doublets |
| 1:19–2:07 | The eight presets | 6 s each, stepped with the orange push, each with a phrase written for it |
| 2:07–2:23 | T2: the architecture | The struck bell's modes live: inharmonic ratios, per-mode decay, doublets, each bell's structure |
| 2:23–2:50 | Performance | Cathedral Rain, then Hollow Vespers: layered six-voice chords across all four bells |
| 2:50–2:58 | Close | The title card and credit again |

## Rebuilding it

Needs: Linux with a C compiler and make, Python 3 with Pillow, ffmpeg (libx264, aac), and the Inter font (`fonts-inter`; it falls back to DejaVu Sans).

```
python3 tools/make_video.py                 # the score, the engine render, the 9 screenshots, the MP4
python3 tools/make_video.py --stills-only   # only the screenshots (media/screenshots/)
python3 tools/make_video.py --jobs 4        # frames rendered in parallel
```

The steps:

1. `media/demo/make_score.py` writes `media/demo/score.txt`, the timeline above as timed events: notes, knob ramps, shift turns, pushes, page changes, captions.
2. `host/bc_host media/demo/score.txt demo.wav frames.txt 30` plays it through the engine and writes the audio plus every frame's drawing commands to `media/demo/build/`.
3. `tools/make_video.py` rasterizes each frame (`host/render_frames.py`), composes the mock-up, captions and titles, and pipes everything to ffmpeg.

To change the edit, change `make_score.py`: its captions, timing and notes. Everything after that step is regenerated from it.
