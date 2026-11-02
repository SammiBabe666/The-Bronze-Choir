# Parameters

All six parameters are live. They act on ringing bells as well as on the next strike, except where a row below says otherwise. Every change is smoothed per block in the log domain, so a turn of any encoder cannot click.

| Control | Parameter | Low end | High end | What it changes |
|---|---|---|---|---|
| **Blue** turn | **Size** | small, light bell | enormous bell | The body: the hum's weight, the tierce and upper partials' tuning (each mode has its own size coefficient), the ring's length, and the strike's colour (bigger bells have a darker, shorter impact noise). The played pitch stays the prime, so the instrument stays in tune at any size. |
| **Green** turn | **Strike** | soft mallet, ceremonial touch | hard metal clapper | The excitation, set at each strike: contact time (a slow attack of about 12 ms down to an instant one), the spectral tilt of the excited modes, and the impact noise's level and brightness. It changes the next strike, not a bell already ringing. |
| **White** turn | **Metal** | warm, dark bronze | bright, complex metal | The live balance of the upper modes. Above about 55% it also stretches the upper partials away from their tuned ratios, for a harsher, more complex alloy. |
| **Orange** turn | **Decay** | tight chime (T60 0.25 s) | enormous cathedral ring (T60 about 32 s) | The damping. The prime's T60 is set here, and every other mode's T60 follows from it, with higher modes and higher notes dying sooner. It also sets how much a released key damps the bell, and the Q of the sympathetic resonators. |
| **Shift + green** | **Sympathy** | off (costs nothing) | strong | The other bells answer: each bell's resonators, tuned to its last struck prime and nominal, are excited by the other three bells' sound. It is bounded and can never feed back ([ARCHITECTURE.md](ARCHITECTURE.md#sympathetic-resonance)). |
| **Shift + orange** | **Detune** | pure, still | unstable, beating | The doublets' split (up to 7 cents), a fixed mistuning between modes, and a slow instability of every mode's frequency. This is the beating and the metallic movement. |
| **Blue** push | previous preset | | | |
| **Orange** push | next preset | | | |

The shift layer turns 2% per detent. The plain turns use the OS's own knob handling. White and blue do nothing under shift; the manifest says so, so other plugins can see it.

**Knob storage.** Size, strike, metal and decay are the sound's knobs 0 to 3. Sympathy and detune are knobs 5 and 7 (knob 4 is the core's). The current preset number is kept in the sound's engine data. All of it is saved with the sound, so the OS's own hold-to-save keeps any edit as a user preset.

**Registers.** These are fixed, not parameters. Below C3 plays the cathedral bell, C3 to B3 the church bell, C4 to B4 the ceremonial bell, and C5 and up the chime. The bell is chosen from the note's pitch at the moment of the strike, so the OS's octave shift and transposition move notes between bells as you would expect.
