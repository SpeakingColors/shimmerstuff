// Factory presets of shimmerstuff, ported from the design mockup (docs/mockup/shimmerstuff.html).
// Tab presets set only the controls of their own tab; main presets set the whole plugin.
#pragma once
#include <utility>
#include <vector>

namespace ssp
{
using V = std::vector<std::pair<const char*, float>>;
struct Pre { const char* name; V v; const char* graph = nullptr; };

inline const std::vector<const char*>& reverbKeys() { static const std::vector<const char*> k { "rev_on", "type", "decay", "damp", "predelay", "size", "mod", "color", "shift", "shimmer", "shift2", "shimmer2", "hp", "lp", "rband", "mix", "bass", "xover", "shape", "spread", "diff", "spin", "wander", "early", "lexmix", "lexmode", "shim_on", "rspec", "rlevel" }; return k; }
inline const std::vector<const char*>& delayKeys() { static const std::vector<const char*> k { "del_on", "dtime", "dfb", "dshift", "dshim", "ddamp", "dmix", "dxfb", "dwide", "dtap2", "dratio2", "dvoices", "dslope", "dwander", "dspin", "ddiff", "dtype", "dhold", "dduck", "dthr", "dsmear", "dmod", "drate", "dlevel" }; return k; }
inline const std::vector<const char*>& fxKeys() { static const std::vector<const char*> k { "route", "rev_on", "del_on", "mod_on", "ifx_on", "wfx_on", "i_hp", "i_lp", "i_band", "i_drive", "i_mix", "w_drive", "w_smix", "w_depth", "w_qmix", "w_duck", "w_drel", "w_dmode", "w_dsrc", "w_dshape", "w_drate", "w_dratio", "w_width", "w_boost", "w_lim", "lim_in", "lim_out" }; return k; }

// w_dmode: 0 follow, 1 free, 2 BPM sync. w_dsrc: 0 none, 1 plugin input, 2 sidechain. w_dratio: 0 1/16, 1 1/8, 2 1/4, 3 1/2, 4 1 bar, 5 2 bars, 6 4 bars.
inline const std::vector<Pre>& reverbPresets()
{
    static const std::vector<Pre> p = {
        { "Init", {} },
        { "Shimmer Pad Bloom", { { "type", 1 }, { "shim_on", 1 }, { "decay", .75f }, { "size", 1.2f }, { "shimmer", .45f }, { "mix", .4f }, { "hp", 200 }, { "lp", 9000 } } },
        { "Hoover Lead Tail", { { "type", 1 }, { "shim_on", 0 }, { "decay", .92f }, { "predelay", 204 }, { "hp", 290 }, { "lp", 4950 }, { "mix", .23f }, { "shimmer", 0 } } },
        { "Plate Snare", { { "type", 0 }, { "shim_on", 0 }, { "decay", .25f }, { "predelay", 0 }, { "hp", 150 }, { "lp", 12000 }, { "mix", .3f }, { "shimmer", 0 } } },
        { "Velvet Wash", { { "type", 2 }, { "shim_on", 0 }, { "decay", .7f }, { "mod", .6f }, { "color", .2f }, { "mix", .35f }, { "hp", 180 }, { "lp", 10000 }, { "shimmer", 0 } } },
        { "Octave Up Cathedral", { { "type", 1 }, { "shim_on", 1 }, { "decay", .9f }, { "size", 1.5f }, { "predelay", 60 }, { "shift", 12 }, { "shimmer", .6f }, { "shift2", 7 }, { "shimmer2", .25f }, { "mix", .45f }, { "hp", 250 }, { "lp", 8000 } } },
        { "Fifths Shimmer Plate", { { "type", 0 }, { "shim_on", 1 }, { "decay", .6f }, { "shift", 7 }, { "shimmer", .5f }, { "shift2", 12 }, { "shimmer2", .3f }, { "mix", .35f }, { "hp", 200 } } },
        { "Down Drone", { { "type", 1 }, { "shim_on", 1 }, { "decay", .85f }, { "size", 1.4f }, { "shift", -12 }, { "shimmer", .5f }, { "mix", .5f }, { "hp", 120 }, { "lp", 5000 } } },
        { "Tight Room", { { "type", 1 }, { "shim_on", 0 }, { "decay", .1f }, { "size", .5f }, { "predelay", 5 }, { "mix", .2f }, { "shimmer", 0 } } },
        { "Trance Pluck Space", { { "type", 1 }, { "shim_on", 0 }, { "decay", .55f }, { "predelay", 80 }, { "mod", .4f }, { "mix", .3f }, { "hp", 300 }, { "lp", 9000 }, { "shimmer", 0 } } },
        { "Lexicat Large Hall", { { "type", 1 }, { "lexmix", 1 }, { "decay", .68f }, { "size", 1.4f }, { "predelay", 25 }, { "bass", 1.4f }, { "shape", .55f }, { "spread", .55f }, { "diff", .55f }, { "spin", .55f }, { "wander", .65f }, { "lexmode", 0 }, { "early", .7f }, { "damp", 5000 }, { "mix", .3f }, { "hp", 100 }, { "shimmer", 0 } } },
        { "Lexicat Random Bloom", { { "type", 1 }, { "lexmix", 1 }, { "decay", .82f }, { "size", 1.5f }, { "predelay", 40 }, { "bass", 1.5f }, { "shape", .7f }, { "spread", .7f }, { "diff", .7f }, { "spin", .7f }, { "wander", .8f }, { "lexmode", 1 }, { "early", .5f }, { "damp", 4500 }, { "mix", .35f }, { "hp", 120 }, { "shimmer", 0 } } },
        { "Lexicat Tight Plate Feel", { { "type", 1 }, { "lexmix", 1 }, { "decay", .3f }, { "size", .6f }, { "predelay", 0 }, { "bass", 1 }, { "shape", 0 }, { "spread", .1f }, { "diff", 1 }, { "spin", .3f }, { "wander", .2f }, { "lexmode", 2 }, { "early", .15f }, { "damp", 9000 }, { "mix", .25f }, { "shimmer", 0 } } },
        { "Shimmer plus Lexicat", { { "type", 1 }, { "shim_on", 1 }, { "lexmix", .6f }, { "decay", .85f }, { "size", 1.5f }, { "predelay", 50 }, { "bass", 1.3f }, { "shape", .6f }, { "spread", .6f }, { "spin", .6f }, { "wander", .7f }, { "lexmode", 1 }, { "shift", 12 }, { "shimmer", .5f }, { "mix", .45f }, { "hp", 200 }, { "lp", 9000 } } },
        { "Room Close", { { "type", 3 }, { "shim_on", 0 }, { "decay", .45f }, { "predelay", 4 }, { "mix", .25f }, { "hp", 120 } } },
        { "Chamber Vocal", { { "type", 4 }, { "shim_on", 0 }, { "decay", .55f }, { "predelay", 22 }, { "mix", .28f }, { "hp", 160 }, { "lp", 12000 } } },
        { "Arena Clap", { { "type", 5 }, { "shim_on", 0 }, { "decay", .55f }, { "size", 1.2f }, { "predelay", 10 }, { "mix", .3f }, { "hp", 200 }, { "lp", 9000 } } },
        { "Spring Tank", { { "type", 6 }, { "shim_on", 0 }, { "decay", .5f }, { "rspec", .6f }, { "mix", .3f }, { "hp", 150 } } },
        { "Gated Snare", { { "type", 7 }, { "shim_on", 0 }, { "decay", .57f }, { "rspec", .35f }, { "predelay", 0 }, { "mix", .4f }, { "hp", 150 }, { "lp", 12000 } } },
        { "Reverse Swell", { { "type", 8 }, { "shim_on", 0 }, { "decay", .7f }, { "rspec", .6f }, { "predelay", 0 }, { "mix", .4f }, { "hp", 200 }, { "lp", 10000 } } },
    };
    return p;
}
inline const std::vector<Pre>& delayPresets()
{
    static const std::vector<Pre> p = {
        { "Init", {} },
        { "Octave Echo", {} },
        { "Rising Stairs", { { "dtime", 375 }, { "dfb", .6f }, { "ddamp", 6000 }, { "dmix", .35f } } },
        { "Falling Stairs", { { "dshift", -12 }, { "dfb", .55f } } },
        { "Fifth Up Echo", { { "dshift", 7 }, { "dshim", .7f }, { "dtime", 500 } } },
        { "Subtle Shimmer Slap", { { "dtime", 120 }, { "dfb", .3f }, { "dshim", .35f }, { "ddamp", 7000 }, { "dmix", .25f } } },
        { "Plain Dark Delay", { { "dshift", 0 }, { "dshim", 0 }, { "dfb", .45f }, { "ddamp", 2500 }, { "dtime", 400 }, { "dmix", .3f } } },
        { "Lead Echo (dotted 1/8 at 150)", { { "dtime", 300 }, { "dfb", .4f }, { "dshim", .3f }, { "ddamp", 6500 }, { "dmix", .25f } } },
        { "Ambient Cloud", { { "dtime", 1200 }, { "dfb", .8f }, { "dshim", .8f }, { "ddamp", 4000 }, { "dmix", .5f } } },
        { "Twin Cross Echo", { { "dtime", 375 }, { "dfb", .6f }, { "dshift", 0 }, { "dshim", 0 }, { "dxfb", .55f }, { "dwide", .12f }, { "dtap2", .5f }, { "dratio2", .5f }, { "ddamp", 5500 }, { "dmix", .35f } } },
        { "Hollow Bounce", { { "dtime", 300 }, { "dfb", .65f }, { "dshift", 0 }, { "dshim", 0 }, { "dxfb", -.7f }, { "dwide", .25f }, { "ddamp", 4500 }, { "dmix", .35f } } },
        { "Voice Cloud", { { "dtime", 600 }, { "dfb", .5f }, { "dshift", 0 }, { "dshim", 0 }, { "dvoices", 6 }, { "dslope", -.4f }, { "dwander", .5f }, { "dspin", .4f }, { "ddiff", .5f }, { "ddamp", 5000 }, { "dmix", .4f } } },
        { "Swelling Tail", { { "dtime", 800 }, { "dfb", .6f }, { "dshift", 12 }, { "dshim", .5f }, { "dvoices", 5 }, { "dslope", .7f }, { "dwander", .3f }, { "ddiff", .3f }, { "dmix", .4f } } },
        { "Tape Echo", { { "dtype", 1 }, { "dshift", 0 }, { "dshim", 0 }, { "dtime", 350 }, { "dfb", .5f }, { "ddamp", 4500 }, { "dsmear", .4f }, { "dmod", .5f }, { "drate", .8f }, { "dmix", .35f } } },
        { "Ping Pong", { { "dtype", 2 }, { "dshift", 0 }, { "dshim", 0 }, { "dtime", 375 }, { "dfb", .6f }, { "ddamp", 5500 }, { "dmix", .35f } } },
        { "Chorus Echo", { { "dtype", 3 }, { "dshift", 0 }, { "dshim", 0 }, { "dtime", 420 }, { "dfb", .5f }, { "dmod", .5f }, { "drate", 1 }, { "dmix", .35f } } },
        { "Reverse Echo", { { "dtype", 4 }, { "dshift", 0 }, { "dshim", 0 }, { "dtime", 500 }, { "dfb", .5f }, { "ddamp", 6000 }, { "dmix", .4f } } },
        { "Ducked Dotted Echo", { { "dshift", 0 }, { "dshim", 0 }, { "dtime", 300 }, { "dfb", .5f }, { "dduck", .7f }, { "dthr", .3f }, { "dmix", .4f } } },
    };
    return p;
}

// Modulation: every effect has its own switch (mo0 ... mo7) and settings (m0_speed ...). Index of a setting inside an effect:
enum MK_ { kSpeed, kDepth, kVoices, kStages, kRegen, kPre, kDiff, kWave, kPhase, kSpread, kDrive, kMin, kMax, kDop, kShift, kNumMK };
#define SSP_MODIDS(t) { "m" #t "_speed", "m" #t "_depth", "m" #t "_voices", "m" #t "_stages", "m" #t "_regen", "m" #t "_pre", "m" #t "_diff", "m" #t "_wave", "m" #t "_phase", "m" #t "_spread", "m" #t "_drive", "m" #t "_min", "m" #t "_max", "m" #t "_dop", "m" #t "_shift" }
inline const char* modId(int t, int k) { static const char* ids[8][kNumMK] = { SSP_MODIDS(0), SSP_MODIDS(1), SSP_MODIDS(2), SSP_MODIDS(3), SSP_MODIDS(4), SSP_MODIDS(5), SSP_MODIDS(6), SSP_MODIDS(7) }; return ids[t][k]; }
inline const char* modOnId(int t) { static const char* ids[8] = { "mo0", "mo1", "mo2", "mo3", "mo4", "mo5", "mo6", "mo7" }; return ids[t]; }
inline const std::vector<const char*>& modKeys()
{
    static const std::vector<const char*> k = [] { std::vector<const char*> v { "mod_on", "m_mix", "m_level" }; for (int t = 0; t < 8; ++t) { v.push_back(modOnId(t)); for (int i = 0; i < kNumMK; ++i) v.push_back(modId(t, i)); } return v; }();
    return k;
}
// A modulation preset: one effect (t) on with some settings, the others off.
inline Pre modPre(const char* name, int t, std::vector<std::pair<int, float>> s)
{
    Pre p { name, {} };
    for (int e = 0; e < 8; ++e) p.v.push_back({ modOnId(e), e == t ? 1.f : 0.f });
    for (const auto& kv : s) p.v.push_back({ modId(t, kv.first), kv.second });
    return p;
}
inline const std::vector<Pre>& modPresets()
{
    static const std::vector<Pre> p = {
        { "Init", { { "mo0", 1 } } },
        modPre("Lush Chorus", 0, { { kVoices, 3 }, { kSpeed, .5f }, { kDepth, .6f }, { kDiff, .3f } }),
        modPre("Wide Chorus Pad", 0, { { kVoices, 4 }, { kSpeed, .3f }, { kDepth, .8f }, { kPhase, 1 }, { kPre, 6 } }),
        modPre("Jet Flanger", 1, { { kSpeed, .25f }, { kDepth, .9f }, { kRegen, .7f }, { kPre, 1 } }),
        modPre("Slow Phaser", 2, { { kStages, 1 }, { kSpeed, .2f }, { kDepth, .8f }, { kRegen, .5f } }),
        modPre("Deep Phaser 12", 2, { { kStages, 2 }, { kSpeed, .35f }, { kDepth, 1 }, { kRegen, .65f } }),
        modPre("Trance Tremolo", 3, { { kSpeed, 6 }, { kDepth, .8f }, { kWave, 2 }, { kPhase, 0 } }),
        modPre("Auto Pan", 4, { { kSpeed, .6f }, { kDepth, 1 } }),
        modPre("Rotary Slow", 5, { { kSpeed, .8f }, { kDrive, .25f } }),
        modPre("Rotary Fast", 5, { { kSpeed, 6.5f }, { kDrive, .3f }, { kDop, .6f } }),
        modPre("Gentle Vibrato", 6, { { kSpeed, 5 }, { kDepth, .4f } }),
        modPre("Detune Thickener", 7, { { kVoices, 2 }, { kShift, 12 }, { kPre, 12 } }),
        modPre("Detune Wide", 7, { { kVoices, 2 }, { kShift, 25 }, { kPre, 18 } }),
        modPre("Supersaw Detune", 7, { { kVoices, 7 }, { kShift, 30 }, { kSpread, .9f }, { kPre, 0 } }),
        modPre("Supersaw Subtle", 7, { { kVoices, 5 }, { kShift, 14 }, { kSpread, .6f }, { kPre, 0 } }),
    };
    return p;
}
inline const std::vector<Pre>& fxPresets()
{
    static const std::vector<Pre> p = {
        { "Init (reverb only)", {} },
        { "Delay only", { { "rev_on", 0 }, { "del_on", 1 } } },
        { "Reverb into Delay", { { "del_on", 1 } } },
        { "Delay into Reverb", { { "del_on", 1 } }, "ifx>del>rev>mod>wfx" },
        { "Parallel Space", { { "del_on", 1 }, { "w_width", 1.3f } }, "ifx>rev|del>mod>wfx" },
        { "FX between Delay and Reverb", { { "del_on", 1 }, { "w_smix", .4f }, { "w_drive", 12 } }, "ifx>del>mod>wfx>rev" },
        { "Tail Lift (squash)", { { "w_qmix", .6f }, { "w_depth", .7f }, { "w_boost", 2 } } },
        { "Pumping Tail (follow duck)", { { "w_duck", .7f }, { "w_drel", 220 }, { "w_boost", 3 } } },
        { "Sync Pump 1/4", { { "w_duck", .85f }, { "w_dmode", 2 }, { "w_dsrc", 1 }, { "w_dratio", 2 }, { "w_dshape", 0 }, { "w_boost", 3 } } },
        { "Trance Gate 1/16", { { "w_duck", .9f }, { "w_dmode", 2 }, { "w_dsrc", 0 }, { "w_dratio", 0 }, { "w_dshape", 3 }, { "w_boost", 2 } } },
        { "Offbeat Swell 1/2", { { "w_duck", .8f }, { "w_dmode", 2 }, { "w_dsrc", 0 }, { "w_dratio", 3 }, { "w_dshape", 4 }, { "w_boost", 2 } } },
        { "Dirty Dark Tail", { { "w_smix", .5f }, { "w_drive", 14 }, { "i_hp", 150 }, { "w_boost", 1 } } },
        { "Wide Bright Wash", { { "del_on", 1 }, { "w_width", 1.6f }, { "w_qmix", .35f }, { "w_depth", .5f }, { "w_boost", 2 } }, "ifx>rev|del>mod>wfx" },
        { "Gated Snare Chain", { { "i_drive", 14 }, { "i_mix", .7f }, { "w_qmix", .8f }, { "w_depth", .8f }, { "w_width", 1.4f }, { "w_boost", 2 } } },
        { "Lead Space (HP in, duck)", { { "del_on", 1 }, { "i_hp", 250 }, { "w_duck", .5f }, { "w_drel", 300 }, { "w_qmix", .25f } } },
    };
    return p;
}

// Main presets: one reverb preset, one delay preset, one FX preset and extra values (names refer to the lists above).
struct Main { const char* name; const char* rev; const char* del; const char* fx; V extra; const char* mod = nullptr; };
inline const std::vector<Main>& mainPresets()
{
    static const std::vector<Main> p = {
        { "Init (reverb only)", "Init", "Init", "Init (reverb only)", { { "type", 1 }, { "shim_on", 1 }, { "decay", .6f }, { "mix", .35f } } },
        { "Hoover Lead Space", "Hoover Lead Tail", "Lead Echo (dotted 1/8 at 150)", "Lead Space (HP in, duck)", {} },
        { "Trance Pluck Wash", "Trance Pluck Space", "Subtle Shimmer Slap", "Parallel Space", {} },
        { "Pumping Shimmer Pad", "Shimmer Pad Bloom", "Octave Echo", "Sync Pump 1/4", { { "del_on", 0 } } },
        { "Octave Cathedral + Echo", "Octave Up Cathedral", "Rising Stairs", "Reverb into Delay", { { "w_boost", 2 } } },
        { "Delay Clouds into Reverb", "Down Drone", "Ambient Cloud", "Delay into Reverb", { { "w_qmix", .3f }, { "w_depth", .5f } } },
        { "FX Between: Dirty Echo Tail", "Velvet Wash", "Plain Dark Delay", "FX between Delay and Reverb", {} },
        { "Gated Snare (big 80s)", "Gated Snare", "Init", "Gated Snare Chain", { { "mix", .5f }, { "del_on", 0 } } },
        { "Trance Gate Tail", "Plate Snare", "Falling Stairs", "Trance Gate 1/16", { { "del_on", 1 } } },
    };
    return p;
}
}
