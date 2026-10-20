// Factory presets of shimmerstuff, ported from the design mockup (docs/mockup/shimmerstuff.html).
// Tab presets set only the controls of their own tab; main presets set the whole plugin.
#pragma once
#include <utility>
#include <vector>

namespace ssp
{
using V = std::vector<std::pair<const char*, float>>;
struct Pre { const char* name; V v; const char* graph = nullptr; };

inline const std::vector<const char*>& reverbKeys() { static const std::vector<const char*> k { "rev_on", "type", "decay", "damp", "predelay", "size", "mod", "color", "shift", "shimmer", "shift2", "shimmer2", "hp", "lp", "mix" }; return k; }
inline const std::vector<const char*>& delayKeys() { static const std::vector<const char*> k { "del_on", "dtime", "dfb", "dshift", "dshim", "ddamp", "dmix" }; return k; }
inline const std::vector<const char*>& fxKeys() { static const std::vector<const char*> k { "route", "rev_on", "del_on", "ifx_on", "wfx_on", "i_hp", "i_lp", "i_drive", "i_mix", "w_drive", "w_smix", "w_depth", "w_qmix", "w_duck", "w_drel", "w_dmode", "w_dsrc", "w_dshape", "w_drate", "w_dratio", "w_width", "w_boost" }; return k; }

// w_dmode: 0 follow, 1 free, 2 BPM sync. w_dsrc: 0 none, 1 plugin input, 2 sidechain. w_dratio: 0 1/16, 1 1/8, 2 1/4, 3 1/2, 4 1 bar, 5 2 bars, 6 4 bars.
inline const std::vector<Pre>& reverbPresets()
{
    static const std::vector<Pre> p = {
        { "Init", {} },
        { "Shimmer Pad Bloom", { { "type", 3 }, { "decay", .75f }, { "size", 1.2f }, { "shimmer", .45f }, { "mix", .4f }, { "hp", 200 }, { "lp", 9000 } } },
        { "Hoover Lead Tail", { { "type", 1 }, { "decay", .92f }, { "predelay", 204 }, { "hp", 290 }, { "lp", 4950 }, { "mix", .23f }, { "shimmer", 0 } } },
        { "Plate Snare", { { "type", 0 }, { "decay", .25f }, { "predelay", 0 }, { "hp", 150 }, { "lp", 12000 }, { "mix", .3f }, { "shimmer", 0 } } },
        { "Velvet Wash", { { "type", 4 }, { "decay", .7f }, { "mod", .6f }, { "color", .2f }, { "mix", .35f }, { "hp", 180 }, { "lp", 10000 }, { "shimmer", 0 } } },
        { "Octave Up Cathedral", { { "type", 3 }, { "decay", .9f }, { "size", 1.5f }, { "predelay", 60 }, { "shift", 12 }, { "shimmer", .6f }, { "shift2", 7 }, { "shimmer2", .25f }, { "mix", .45f }, { "hp", 250 }, { "lp", 8000 } } },
        { "Fifths Shimmer Plate", { { "type", 2 }, { "decay", .6f }, { "shift", 7 }, { "shimmer", .5f }, { "shift2", 12 }, { "shimmer2", .3f }, { "mix", .35f }, { "hp", 200 } } },
        { "Down Drone", { { "type", 3 }, { "decay", .85f }, { "size", 1.4f }, { "shift", -12 }, { "shimmer", .5f }, { "mix", .5f }, { "hp", 120 }, { "lp", 5000 } } },
        { "Tight Room", { { "type", 1 }, { "decay", .1f }, { "size", .5f }, { "predelay", 5 }, { "mix", .2f }, { "shimmer", 0 } } },
        { "Trance Pluck Space", { { "type", 1 }, { "decay", .55f }, { "predelay", 80 }, { "mod", .4f }, { "mix", .3f }, { "hp", 300 }, { "lp", 9000 }, { "shimmer", 0 } } },
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
    };
    return p;
}
inline const std::vector<Pre>& fxPresets()
{
    static const std::vector<Pre> p = {
        { "Init (reverb only)", {} },
        { "Delay only", { { "rev_on", 0 }, { "del_on", 1 } } },
        { "Reverb into Delay", { { "del_on", 1 } } },
        { "Delay into Reverb", { { "del_on", 1 } }, "ifx>del>rev>wfx" },
        { "Parallel Space", { { "del_on", 1 }, { "w_width", 1.3f } }, "ifx>rev|del>wfx" },
        { "FX between Delay and Reverb", { { "del_on", 1 }, { "w_smix", .4f }, { "w_drive", 12 } }, "ifx>del>wfx>rev" },
        { "Tail Lift (squash)", { { "w_qmix", .6f }, { "w_depth", .7f }, { "w_boost", 2 } } },
        { "Pumping Tail (follow duck)", { { "w_duck", .7f }, { "w_drel", 220 }, { "w_boost", 3 } } },
        { "Sync Pump 1/4", { { "w_duck", .85f }, { "w_dmode", 2 }, { "w_dsrc", 1 }, { "w_dratio", 2 }, { "w_dshape", 0 }, { "w_boost", 3 } } },
        { "Trance Gate 1/16", { { "w_duck", .9f }, { "w_dmode", 2 }, { "w_dsrc", 0 }, { "w_dratio", 0 }, { "w_dshape", 3 }, { "w_boost", 2 } } },
        { "Offbeat Swell 1/2", { { "w_duck", .8f }, { "w_dmode", 2 }, { "w_dsrc", 0 }, { "w_dratio", 3 }, { "w_dshape", 4 }, { "w_boost", 2 } } },
        { "Dirty Dark Tail", { { "w_smix", .5f }, { "w_drive", 14 }, { "i_hp", 150 }, { "w_boost", 1 } } },
        { "Wide Bright Wash", { { "del_on", 1 }, { "w_width", 1.6f }, { "w_qmix", .35f }, { "w_depth", .5f }, { "w_boost", 2 } }, "ifx>rev|del>wfx" },
        { "Lead Space (HP in, duck)", { { "del_on", 1 }, { "i_hp", 250 }, { "w_duck", .5f }, { "w_drel", 300 }, { "w_qmix", .25f } } },
    };
    return p;
}

// Main presets: one reverb preset, one delay preset, one FX preset and extra values (names refer to the lists above).
struct Main { const char* name; const char* rev; const char* del; const char* fx; V extra; };
inline const std::vector<Main>& mainPresets()
{
    static const std::vector<Main> p = {
        { "Init (reverb only)", "Init", "Init", "Init (reverb only)", { { "type", 3 }, { "decay", .6f }, { "mix", .35f } } },
        { "Hoover Lead Space", "Hoover Lead Tail", "Lead Echo (dotted 1/8 at 150)", "Lead Space (HP in, duck)", {} },
        { "Trance Pluck Wash", "Trance Pluck Space", "Subtle Shimmer Slap", "Parallel Space", {} },
        { "Pumping Shimmer Pad", "Shimmer Pad Bloom", "Octave Echo", "Sync Pump 1/4", { { "del_on", 0 } } },
        { "Octave Cathedral + Echo", "Octave Up Cathedral", "Rising Stairs", "Reverb into Delay", { { "w_boost", 2 } } },
        { "Delay Clouds into Reverb", "Down Drone", "Ambient Cloud", "Delay into Reverb", { { "w_qmix", .3f }, { "w_depth", .5f } } },
        { "FX Between: Dirty Echo Tail", "Velvet Wash", "Plain Dark Delay", "FX between Delay and Reverb", {} },
        { "Trance Gate Tail", "Plate Snare", "Falling Stairs", "Trance Gate 1/16", { { "del_on", 1 } } },
    };
    return p;
}
}
