#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;
using namespace wm;

const juce::StringArray& ShimmerstuffProcessor::typeNames() { static const juce::StringArray n { "Plate", "Hall", "Shimmer Plate", "Shimmer Hall", "Velvet" }; return n; }
const juce::StringArray& ShimmerstuffProcessor::ratioNames() { static const juce::StringArray n { "1/16", "1/8", "1/4", "1/2", "1 bar", "2 bars", "4 bars" }; return n; }

namespace
{
struct F { const char* id; const char* name; float lo, hi, def, mid; };
// id, name, min, max, default, skew centre (0 = linear). Same ids and defaults as the design mockup.
const F kFloats[] = {
    { "decay", "Decay", 0.f, 1.f, 0.6f, 0.f }, { "damp", "Damping", 500.f, 16000.f, 6000.f, 3000.f }, { "predelay", "Pre-delay", 0.f, 250.f, 20.f, 40.f },
    { "size", "Size", 0.5f, 1.6f, 1.f, 0.f }, { "mod", "Modulation", 0.f, 1.f, 0.5f, 0.f }, { "color", "Velvet colour", -1.f, 1.f, 0.f, 0.f },
    { "shift", "Shimmer pitch", -12.f, 12.f, 12.f, 0.f }, { "shimmer", "Shimmer amount", 0.f, 1.f, 0.5f, 0.f },
    { "shift2", "Shimmer 2 pitch", -12.f, 12.f, 7.f, 0.f }, { "shimmer2", "Shimmer 2 amount", 0.f, 1.f, 0.f, 0.f },
    { "hp", "Wet high-pass", 20.f, 2000.f, 20.f, 150.f }, { "lp", "Wet low-pass", 1000.f, 20000.f, 20000.f, 6000.f }, { "mix", "Reverb mix", 0.f, 1.f, 0.35f, 0.f },
    { "dtime", "Delay time", 20.f, 2000.f, 450.f, 400.f }, { "dfb", "Delay feedback", 0.f, 0.95f, 0.55f, 0.f },
    { "dshift", "Delay shift", -12.f, 12.f, 12.f, 0.f }, { "dshim", "Delay shimmer", 0.f, 1.f, 1.f, 0.f },
    { "ddamp", "Delay damping", 500.f, 16000.f, 5000.f, 3000.f }, { "dmix", "Delay mix", 0.f, 1.f, 0.4f, 0.f },
    { "i_hp", "Input high-pass", 20.f, 2000.f, 20.f, 150.f }, { "i_lp", "Input low-pass", 1000.f, 20000.f, 20000.f, 6000.f },
    { "i_drive", "Input drive", 0.f, 30.f, 6.f, 0.f }, { "i_mix", "Input saturation mix", 0.f, 1.f, 0.f, 0.f },
    { "w_drive", "Wet drive", 0.f, 30.f, 8.f, 0.f }, { "w_smix", "Wet saturation mix", 0.f, 1.f, 0.f, 0.f },
    { "w_depth", "Squash depth", 0.f, 1.f, 0.6f, 0.f }, { "w_qmix", "Squash mix", 0.f, 1.f, 0.f, 0.f },
    { "w_width", "Wet width", 0.f, 2.f, 1.f, 0.f }, { "w_boost", "Wet boost", -12.f, 12.f, 0.f, 0.f },
    { "w_duck", "Duck amount", 0.f, 1.f, 0.f, 0.f }, { "w_drel", "Duck release", 20.f, 1000.f, 250.f, 200.f },
    { "w_drate", "Duck rate", 0.25f, 16.f, 2.f, 2.f },
};

struct Preset { const char* name; const char* graph; std::vector<std::pair<const char*, float>> v; };
const std::vector<Preset>& presets()
{
    static const std::vector<Preset> p = {
        { "Init: Reverb only", "ifx>rev>del>wfx", { { "rev_on", 1 }, { "del_on", 0 }, { "ifx_on", 0 }, { "wfx_on", 0 } } },
        { "Shimmer Pad", "ifx>rev>del>wfx", { { "rev_on", 1 }, { "del_on", 0 }, { "type", 3 }, { "decay", 0.8f }, { "shimmer", 0.6f }, { "mix", 0.4f }, { "ifx_on", 1 }, { "i_hp", 200 }, { "wfx_on", 1 }, { "w_width", 1.4f } } },
        { "Hoover Hall (8 s, 204 ms pre)", "ifx>rev>del>wfx", { { "rev_on", 1 }, { "del_on", 0 }, { "type", 1 }, { "decay", 0.93f }, { "predelay", 204 }, { "hp", 290 }, { "lp", 4950 }, { "mix", 0.23f }, { "ifx_on", 0 }, { "wfx_on", 0 } } },
        { "Delay into Reverb", "ifx>del>rev>wfx", { { "rev_on", 1 }, { "del_on", 1 }, { "dtime", 375 }, { "dshift", 12 }, { "dmix", 0.35f }, { "mix", 0.35f }, { "wfx_on", 1 }, { "w_width", 1.2f } } },
        { "Parallel Space", "ifx>rev|del>wfx", { { "rev_on", 1 }, { "del_on", 1 }, { "w_width", 1.3f }, { "wfx_on", 1 } } },
        { "FX between Delay and Reverb", "ifx>del>wfx>rev", { { "rev_on", 1 }, { "del_on", 1 }, { "wfx_on", 1 }, { "w_smix", 0.4f }, { "w_drive", 12 } } },
        { "Wide Bright Wash", "ifx>rev|del>wfx", { { "rev_on", 1 }, { "del_on", 1 }, { "wfx_on", 1 }, { "w_width", 1.6f }, { "w_qmix", 0.35f }, { "w_depth", 0.5f }, { "w_boost", 2 } } },
        { "Pumping Tail (1/4 duck)", "ifx>rev>del>wfx", { { "rev_on", 1 }, { "del_on", 0 }, { "wfx_on", 1 }, { "w_duck", 0.9f }, { "w_dmode", 2 }, { "w_dsrc", 0 }, { "w_dratio", 2 }, { "w_dshape", 0 }, { "mix", 0.5f } } },
        { "Delay only: octave climb", "ifx>rev>del>wfx", { { "rev_on", 0 }, { "del_on", 1 }, { "dtime", 450 }, { "dshift", 12 }, { "dfb", 0.6f }, { "dmix", 0.45f } } },
    };
    return p;
}
}

APVTS::ParameterLayout ShimmerstuffProcessor::layout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "type", 1 }, "Reverb type", typeNames(), 3));
    for (const auto& f : kFloats)
    {
        juce::NormalisableRange<float> r(f.lo, f.hi);
        if (f.mid > 0.f) r.setSkewForCentre(f.mid);
        p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ f.id, 1 }, f.name, r, f.def));
    }
    p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "w_dmode", 1 }, "Duck timing", juce::StringArray { "Follow the source", "Free (Hz)", "BPM sync" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "w_dsrc", 1 }, "Duck source", juce::StringArray { "None", "Plugin input", "Sidechain input" }, 1));
    juce::StringArray shapes; for (int i = 0; i < fx::kNumShapes; ++i) shapes.add(fx::shapeName(i));
    p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "w_dshape", 1 }, "Duck shape", shapes, 0));
    p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "w_dratio", 1 }, "Duck ratio", ratioNames(), 2));
    struct B { const char* id; const char* name; bool def; };
    static const B bools[] = { { "rev_on", "Reverb on", true }, { "del_on", "Delay on", false }, { "ifx_on", "Input FX on", true }, { "wfx_on", "Wet FX on", true }, { "byp", "Bypass", false } };
    for (const auto& b : bools)
        p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ b.id, 1 }, b.name, b.def));
    return { p.begin(), p.end() };
}

ShimmerstuffProcessor::ShimmerstuffProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", layout())
{
    setGraph(graphStr);
}

bool ShimmerstuffProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    if (l.getMainInputChannelSet() != juce::AudioChannelSet::stereo() || l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    const auto sc = l.getChannelSet(true, 1);
    return sc.isDisabled() || sc == juce::AudioChannelSet::stereo() || sc == juce::AudioChannelSet::mono();
}

static uint64_t encodeGraph(const fx::Graph& g)
{
    uint64_t c = ~uint64_t(0);
    for (int s = 0; s < 4; ++s) for (int j = 0; j < 4; ++j)
    {
        const uint64_t nib = (s < g.stages && j < g.count[s]) ? uint64_t(g.items[s][j]) : 15u;
        c &= ~(uint64_t(15) << (4 * (s * 4 + j))); c |= nib << (4 * (s * 4 + j));
    }
    return c;
}
static fx::Graph decodeGraph(uint64_t c)
{
    fx::Graph g;
    for (int s = 0; s < 4; ++s)
    {
        int n = 0;
        for (int j = 0; j < 4; ++j) { const int nib = int((c >> (4 * (s * 4 + j))) & 15u); if (nib < 4) g.items[s][n++] = nib; }
        if (n > 0) g.count[g.stages++] = n; else if (s < 4) { /* skip empty stage */ }
    }
    // compact stages (an empty stage in the middle is skipped above because count stays 0)
    return g;
}

juce::String ShimmerstuffProcessor::getGraph() const { return graphStr; }
void ShimmerstuffProcessor::setGraph(const juce::String& s)
{
    graph = fx::parseGraph(s.toStdString());
    graphStr = juce::String(fx::graphString(graph));
    graphCode.store(encodeGraph(graph));
    apvts.state.setProperty("graph", graphStr, nullptr);
}
void ShimmerstuffProcessor::setParam(const juce::String& id, float v)
{
    if (auto* q = apvts.getParameter(id)) q->setValueNotifyingHost(q->convertTo0to1(v));
}

int ShimmerstuffProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String ShimmerstuffProcessor::getProgramName(int i) { return juce::isPositiveAndBelow(i, getNumPrograms()) ? presets()[(size_t) i].name : ""; }
void ShimmerstuffProcessor::setCurrentProgram(int i)
{
    if (!juce::isPositiveAndBelow(i, getNumPrograms())) return;
    program = i;
    // a main preset sets every parameter: start from the defaults, then apply what differs
    for (const auto& f : kFloats) setParam(f.id, f.def);
    setParam("type", 3); setParam("w_dmode", 0); setParam("w_dsrc", 1); setParam("w_dshape", 0); setParam("w_dratio", 2);
    setParam("rev_on", 1); setParam("del_on", 0); setParam("ifx_on", 1); setParam("wfx_on", 1); setParam("byp", 0);
    const auto& pr = presets()[(size_t) i];
    for (const auto& kv : pr.v) setParam(kv.first, kv.second);
    setGraph(pr.graph);
}

juce::XmlElement* ShimmerstuffProcessor::makePresetXml() const
{
    auto xml = const_cast<juce::AudioProcessorValueTreeState&>(apvts).copyState().createXml().release();
    xml->setAttribute("graph", graphStr);
    return xml;
}
void ShimmerstuffProcessor::applyUserPresetXml(const juce::XmlElement& xml)
{
    // validated: only known parameter ids are read, values are clamped by the parameters
    for (auto* e : xml.getChildIterator())
        if (e->hasTagName("PARAM"))
            if (auto* q = apvts.getParameter(e->getStringAttribute("id")))
                q->setValueNotifyingHost(q->convertTo0to1((float) e->getDoubleAttribute("value", (double) q->convertFrom0to1(q->getValue()))));
    setGraph(xml.getStringAttribute("graph", graphStr));
}

void ShimmerstuffProcessor::prepareToPlay(double sr, int)
{
    fs = sr; eng.prepare((float) sr); smInit = false;
}

void ShimmerstuffProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples();
    auto mainIn = getBusBuffer(buf, true, 0);
    auto sc = getBusCount(true) > 1 && getBus(true, 1)->isEnabled() ? getBusBuffer(buf, true, 1) : juce::AudioBuffer<float>();
    sidechainOn = sc.getNumChannels() > 0 ? 1 : 0;
    if (mainIn.getNumChannels() < 2) return;
    float* L = mainIn.getWritePointer(0); float* R = mainIn.getWritePointer(1);
    const float* sL = sc.getNumChannels() > 0 ? sc.getReadPointer(0) : nullptr;
    const float* sR = sc.getNumChannels() > 1 ? sc.getReadPointer(1) : sL;

    const bool byp = get("byp") > .5f;
    eng.revOn = get("rev_on") > .5f; eng.delOn = get("del_on") > .5f; eng.ifxOn = get("ifx_on") > .5f; eng.wfxOn = get("wfx_on") > .5f;
    auto& rs = eng.rs; rs.type = (wm::engine::ReverbType) juce::jlimit(0, 4, (int) get("type"));
    rs.p.decay = get("decay"); rs.p.dampHz = get("damp"); rs.p.preDelayMs = get("predelay"); rs.p.size = get("size"); rs.p.modDepth = get("mod"); rs.p.color = get("color");
    rs.shimmerSemis = get("shift"); rs.shimmerAmount = get("shimmer"); rs.shimmer2Semis = get("shift2"); rs.shimmer2Amount = get("shimmer2");
    rs.wetHpHz = get("hp"); rs.wetLpHz = get("lp");
    auto& dp = eng.dp; dp.shiftSemis = get("dshift"); dp.shimmerAmount = get("dshim"); dp.dampHz = get("ddamp");
    eng.ip.hp = get("i_hp"); eng.ip.lp = get("i_lp"); eng.ip.drive = get("i_drive");
    auto& wp = eng.wp; wp.satDrive = get("w_drive"); wp.squashDepth = get("w_depth");
    auto& dk = wp.duck; dk.relMs = get("w_drel"); dk.rateHz = get("w_drate"); dk.mode = juce::jlimit(0, 2, (int) get("w_dmode")); dk.shape = (int) get("w_dshape");
    dk.beats = kRatioBeats[juce::jlimit(0, 6, (int) get("w_dratio"))];
    double bpm = 120.0;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto t = pos->getBpm()) bpm = *t;
    dk.bpm = (float) bpm;
    int src = juce::jlimit(0, 2, (int) get("w_dsrc"));
    const bool useSc = src == 2 && sL != nullptr;
    dk.src = src == 0 || (src == 2 && !useSc) ? 0 : 1;

    // smoothed per-sample values (about 15 ms), the delay time glides 80 ms
    const float targets[12] = { get("mix"), get("dmix"), get("dfb"), get("i_mix"), get("w_smix"), get("w_qmix"), get("w_duck"), get("w_width"), get("w_boost"), 0.f, 0.f, 0.f };
    if (!smInit) { for (int i = 0; i < 9; ++i) sm[i] = targets[i]; smDt = get("dtime"); smInit = true; }
    const float k = 1.f - std::exp(-1.f / (0.015f * (float) fs)), kT = 1.f - std::exp(-1.f / (0.08f * (float) fs));
    const float dtime = get("dtime");
    const fx::Graph g = decodeGraph(graphCode.load());
    float pkIn = 0, pkOut = 0, minDg = 1;
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < 9; ++j) sm[j] += k * (targets[j] - sm[j]);
        smDt += kT * (dtime - smDt);
        rs.mix = sm[0]; dp.mix = sm[1]; dp.feedback = sm[2]; dp.timeMs = smDt; eng.ip.satMix = sm[3]; wp.satMix = sm[4]; wp.squashMix = sm[5]; dk.amt = sm[6]; wp.width = sm[7]; wp.boostDb = sm[8];
        const float l = L[i], r = R[i], tl = useSc ? sL[i] : l, tr = useSc ? sR[i] : r;
        float ol, orr; eng.tick(g, l, r, tl, tr, ol, orr);
        if (byp) { ol = l; orr = r; }
        L[i] = ol; R[i] = orr;
        pkIn = std::max(pkIn, std::max(std::fabs(l), std::fabs(r))); pkOut = std::max(pkOut, std::max(std::fabs(ol), std::fabs(orr)));
        minDg = std::min(minDg, eng.wfx.duck().dg);
    }
    inPk = pkIn; outPk = pkOut; duckGain = minDg;
}

juce::AudioProcessorEditor* ShimmerstuffProcessor::createEditor() { return new ShimmerstuffEditor(*this); }

void ShimmerstuffProcessor::getStateInformation(juce::MemoryBlock& d)
{
    apvts.state.setProperty("graph", graphStr, nullptr);
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, d);
}
void ShimmerstuffProcessor::setStateInformation(const void* d, int s)
{
    if (auto xml = getXmlFromBinary(d, s))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        setGraph(xml->getStringAttribute("graph", graphStr));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ShimmerstuffProcessor(); }
