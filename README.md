# shimmerstuff

Independent shimmer effects by SpeakingColors: **ShimmerDelay** and **ShimmerReverb** (VST3 + Standalone, Linux/Windows/macOS).

ShimmerReverb types: Plate, Hall, Shimmer Plate, Shimmer Hall, Lush.
DSP is header-only in `src/Reverb.h` (shared with WhiskerMix and the catfx library in cat-ui).

Status: DSP unit-tested, plugins build in CI. Not yet auditioned by ear.

    sh tests/run.sh

## Support

<p align="center">
  <a href="https://ko-fi.com/studio87"><img src="https://img.shields.io/badge/Support_Studio_87-on_Ko--fi-FF5E5B?style=for-the-badge&logo=kofi&logoColor=white" alt="Support Studio 87 on Ko-fi"></a>
</p>

☕ Free and open source, made by one person. If it helps your music, a coffee on [Ko-fi](https://ko-fi.com/studio87) keeps it coming. 💜

## Design mockup

An interactive mockup of both plugins is in [docs/mockup/shimmerstuff.html](docs/mockup/shimmerstuff.html). Download it and open it in a browser. It runs a port of the plugin DSP, so it sounds like the real thing.

The mockup (v3) also shows where the plugin is going: a free routing diagram (Input FX, Reverb, Delay and Wet FX; drag the boxes in series or side by side, bypass, jump to the controls, edit values in the boxes), a Decay view with several draggable handles (decay, size, pre-delay, damping, shimmer, mix), a duck with an optional sidechain source, free or BPM synced shapes, and a main preset that holds the reverb, delay and routing together. The routing, wet FX and main preset are not in the C++ plugins yet.
