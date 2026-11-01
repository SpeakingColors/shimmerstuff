# Lexicon-style reverb and delay: design notes

Our own notes, written in our own words, for reuse in any reverb or delay plugin. No vendor code or manual text is copied.
Labels: **VERIFIED** (read in a primary document), **COMMUNITY** (forum or insider post, author not confirmed), **UNVERIFIED** (not confirmed, not relied on).

## What the algorithms are (control level)
- Reverb family: Hall, Plate, Chamber, Room, Random Hall, Concert Hall. VERIFIED by owner manuals: the control set below. COMMUNITY: which family member is smooth or modulated.
- Common controls: Mid reverb time, Bass Multiply (low-band decay as a multiple of mid), Crossover (where bass takes over), HF damping inside the loop, output HF cutoff (6 dB per octave), Size, Shape and Spread (the energy build-up envelope), Diffusion (how fast echo density grows), Pre-delay, 2 to 6 pre-echo taps that bypass the tank and ignore pre-delay, Spin and Wander (slow random delay movement).
- Random-type algorithms add random delay elements: this removes long-lived modes, so the decay is smoother and less metallic. VERIFIED (manual statement). Large Wander (about 10 ms) suits large sizes; Spin high enough to blur modes, low enough that sustained piano or guitar pitch stays stable.
- Plate: smooth, dense, little modulation. Hall (newer): stable tail. Random Hall: obvious modulation, the classic "big" sound. Concert Hall (224 lineage): chorus controls and audible pitch movement. COMMUNITY.
- Delay family (Twin Delays): four voices, each with its own time, level and feedback; feedback can be negative; feedback of voices 3 and 4 is cross-panned to the other channel; per-voice low-pass. VERIFIED (manual).

## Hardware facts (only what we checked)
- 224 and 224X: Intel 8080 host on a single-board computer; DSP is a microprogrammed unit with a 293 ns cycle, a 16-bit by 6-bit multiplier, a 20-bit accumulator and a writable control store. VERIFIED (224X service manual, section 3). Sample rate 34.13 kHz (224X, 20.48 kHz option), 20 kHz on the first 224. VERIFIED. Converters: 12-bit mantissa plus 2-bit exponent, converted to 16-bit fixed point. VERIFIED.
- 480L: 18-bit converters, two high-speed processor boards. VERIFIED (service manual listing). CPU and DSP chip names: UNVERIFIED.
- AMD 2901 bit-slice, Z80 or 80186 hosts, "companding": UNVERIFIED, not needed.
- Lesson: the era hardware ran at 20 to 34 kHz with 12 to 18 bit converters. We match topology and control feel, not the converter sound. A "vintage" switch (lower bandwidth and bit depth) is an optional idea, not built.

## How our implementation maps (src/Reverb.h, class LexReverb)
- Pre-delay, then 4 input allpass diffusers (coefficient 0.2 to 0.75 from Diffusion).
- Shape and Spread: the diffused input reaches the network through 8 taps across a window (3 to 193 ms). Shape 0 = single tap (instant), 1 = rising weights (slow build-up).
- Tank: 8 delay lines (31 to 74 ms, near-prime), Householder mixing. Per line: HF damping one-pole, then a one-pole split at Crossover; low band decays with RT x Bass, the rest with RT. Per-pass gain is 10^(-3 L / RT).
- Spin and Wander: each line delay moves by a smoothed random value (new target every 1/Spin seconds). Movement 0: up to 1.2 ms; Movement 1: up to 8.5 ms, scaled by Size.
- Early: four taps from the raw input (11, 17.5, 26, 38 ms, scaled by Size), alternating L and R, with a little movement.
- The module is called Lexicat (our name). It is not a reverb type: Blend (0 to 1) mixes it side by side with whatever type is chosen, shimmer types included; the shifted tail feeds both. Movement (0 to 1) goes from stable hall to big random-hall movement. The shimmer feedback tap divides out the module's level boost so the loop gain stays what it was.
- Density: a modulated allpass sits inside every tank line (counted in the decay time), which made the tail smoother. Measured late-tail spectrum ripple (1.0 to 1.5 s, 200 Hz to 8 kHz): Hall 29.4 dB, Lexicat 24.5 dB (smaller is less metallic). Level matched to Hall within 0.1 dB at equal Decay.
- Delay extras (off by default): cross feedback (-1..1, self plus cross share never exceed the feedback setting), width (right channel longer), second voice (level and time ratio).

## Measured in tests (tests/reverb_test.cpp, section 8 and 9)
- Higher Mid RT gives a longer tail and still decays; Bass Multiply 2 raises low-band energy late in the tail by more than 2x while the highs change much less; slow Shape and Spread cut early energy; Early adds energy in the first 60 ms; all outputs finite, bounded after 2 s of loud noise at full shimmer; delay extras at default are bit-identical to the old delay.

## Not done yet (honest list)
- The owner heard the first version as only slightly different from the plain hall. The second version is denser and the presets are bolder; still to be judged by ear and against recorded impulse responses.
- No listening comparison against a real Lexicon unit. Plan: the owner records impulse responses through their own plugins at 100 percent wet, 48 kHz, then we compare RT60 per band, echo density, spectral decay and stereo width, and adjust constants.
- The mockup shows the controls (playable, same code); the real plugin window does not yet. They are normal automatable parameters.
- Standing UI rule: no text selection anywhere in a plugin window or mockup (inputs excepted).
- Inverse and gated slopes, Chamber and Room variants, Concert Hall chorus: not built.
