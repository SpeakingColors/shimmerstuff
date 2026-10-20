#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;
using namespace wm;

const juce::StringArray& ShimmerstuffProcessor::typeNames() { static const juce::StringArray n { "Plate", "Hall", "Shimmer Plate", "Shimmer Hall", "Velvet" }; return n; }
const char* ShimmerstuffProcessor::layoutGraph(int i)
{
    static const char* g[kLayouts] = { "ifx>rev>del>wfx", "ifx>del>rev>wfx", "ifx>rev|del>wfx", "ifx>rev>wfx>del", "ifx>del>wfx>rev", "rev>del>wfx>ifx", "wfx>ifx>rev>del", "rev|del|ifx|wfx" };
    return g[juce::jlimit(0, kLayouts - 1, i)];
}
const juce::StringArray& ShimmerstuffProcessor::layoutNames()
{
    static const juce::StringArray n { "FX, Reverb, Delay, Wet FX", "FX, Delay, Reverb, Wet FX", "FX, Reverb and Delay in parallel, Wet FX", "FX, Reverb, Wet FX, Delay", "FX, Delay, Wet FX, Reverb (FX between)",
        "Reverb, Delay, Wet FX, then FX", "Wet FX first, then FX, Reverb, Delay", "Everything in parallel", "Custom (your own)" };
    return n;
}
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
    p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "route", 1 }, "Routing layout", layoutNames(), 0));
    static const B bools[] = { { "rev_on", "Reverb on", true }, { "del_on", "Delay on", false }, { "ifx_on", "Input FX on", true }, { "wfx_on", "Wet FX on", true }, { "byp", "Bypass", false } };
    for (const auto& b : bools)
        p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ b.id, 1 }, b.name, b.def));
    return { p.begin(), p.end() };
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

ShimmerstuffProcessor::ShimmerstuffProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", layout())
{
    for (int i = 0; i < kLayouts; ++i) layoutCode[i] = encodeGraph(fx::parseGraph(layoutGraph(i)));
    graphCode.store(layoutCode[0]);
    for (auto& t : tapIn) t.store(0.f);
    for (auto& t : tapOut) t.store(0.f);
}

bool ShimmerstuffProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    if (l.getMainInputChannelSet() != juce::AudioChannelSet::stereo() || l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    const auto sc = l.getChannelSet(true, 1);
    return sc.isDisabled() || sc == juce::AudioChannelSet::stereo() || sc == juce::AudioChannelSet::mono();
}

juce::String ShimmerstuffProcessor::getGraph() const
{
    const int r = (int) get("route");
    return r < kLayouts ? juce::String(layoutGraph(r)) : graphStr;
}
void ShimmerstuffProcessor::setGraph(const juce::String& s)
{
    const auto parsed = fx::parseGraph(s.toStdString());
    const juce::String norm(fx::graphString(parsed));
    for (int i = 0; i < kLayouts; ++i)
        if (norm == layoutGraph(i)) { setParam("route", (float) i); return; }
    graphStr = norm; graph = parsed;
    graphCode.store(encodeGraph(graph));
    apvts.state.setProperty("graph", graphStr, nullptr);
    setParam("route", (float) kLayouts);
}
void ShimmerstuffProcessor::setParam(const juce::String& id, float v)
{
    if (auto* q = apvts.getParameter(id)) q->setValueNotifyingHost(q->convertTo0to1(v));
}

void ShimmerstuffProcessor::resetKeys(const std::vector<const char*>& keys, bool onOff)
{
    // the Reverb and Delay tabs only edit their unit: a tab preset leaves the on/off state alone unless it sets it, the routing stays as chosen
    for (auto* k : keys)
        if (onOff || (juce::String(k) != "rev_on" && juce::String(k) != "del_on"))
            if (auto* q = apvts.getParameter(k)) q->setValueNotifyingHost(q->getDefaultValue());
}
void ShimmerstuffProcessor::applyKeys(const std::vector<const char*>& keys, const ssp::Pre* p, bool onOff)
{
    resetKeys(keys, onOff);
    if (p == nullptr) return;
    for (const auto& kv : p->v) setParam(kv.first, kv.second);
    if (p->graph != nullptr) setGraph(p->graph);
}
int ShimmerstuffProcessor::numTabPresets(int tab) const { return (int) (tab == 0 ? ssp::reverbPresets() : tab == 1 ? ssp::delayPresets() : ssp::fxPresets()).size(); }
juce::String ShimmerstuffProcessor::tabPresetName(int tab, int i) const
{
    const auto& l = tab == 0 ? ssp::reverbPresets() : tab == 1 ? ssp::delayPresets() : ssp::fxPresets();
    return juce::isPositiveAndBelow(i, (int) l.size()) ? l[(size_t) i].name : "";
}
void ShimmerstuffProcessor::applyTabPreset(int tab, int i)
{
    const auto& l = tab == 0 ? ssp::reverbPresets() : tab == 1 ? ssp::delayPresets() : ssp::fxPresets();
    if (!juce::isPositiveAndBelow(i, (int) l.size()) || tab < 0 || tab > 2) return;
    lastTabPreset[tab] = i;
    applyKeys(tab == 0 ? ssp::reverbKeys() : tab == 1 ? ssp::delayKeys() : ssp::fxKeys(), &l[(size_t) i], tab == 2);
}
int ShimmerstuffProcessor::numMainPresets() const { return (int) ssp::mainPresets().size(); }
juce::String ShimmerstuffProcessor::mainPresetName(int i) const { return juce::isPositiveAndBelow(i, numMainPresets()) ? ssp::mainPresets()[(size_t) i].name : ""; }
void ShimmerstuffProcessor::applyMainPreset(int i)
{
    if (!juce::isPositiveAndBelow(i, numMainPresets())) return;
    program = i;
    const auto& m = ssp::mainPresets()[(size_t) i];
    auto find = [](const std::vector<ssp::Pre>& l, const char* n) -> const ssp::Pre* { for (const auto& p : l) if (juce::String(p.name) == n) return &p; return nullptr; };
    applyKeys(ssp::reverbKeys(), find(ssp::reverbPresets(), m.rev), true);
    applyKeys(ssp::delayKeys(), find(ssp::delayPresets(), m.del), true);
    applyKeys(ssp::fxKeys(), find(ssp::fxPresets(), m.fx), true);
    for (const auto& kv : m.extra) setParam(kv.first, kv.second);
    setParam("byp", 0.f);
    for (int t = 0; t < 3; ++t) lastTabPreset[t] = -1;
}
int ShimmerstuffProcessor::getNumPrograms() { return numMainPresets(); }
const juce::String ShimmerstuffProcessor::getProgramName(int i) { return mainPresetName(i); }
void ShimmerstuffProcessor::setCurrentProgram(int i) { applyMainPreset(i); }

juce::XmlElement* ShimmerstuffProcessor::makePresetXml(int tab) const
{
    // a small validated format: PARAM elements with id and value, plus the custom graph
    auto* xml = new juce::XmlElement("STATE");
    const std::vector<const char*>& keys = tab == 0 ? ssp::reverbKeys() : tab == 1 ? ssp::delayKeys() : ssp::fxKeys();
    if (tab < 0)
    {
        for (auto* par : const_cast<ShimmerstuffProcessor*>(this)->getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(par); r != nullptr && r->paramID != "byp")
            { auto* e = xml->createNewChildElement("PARAM"); e->setAttribute("id", r->paramID); e->setAttribute("value", (double) r->convertFrom0to1(r->getValue())); }
    }
    else
        for (auto* k : keys)
            if (auto* r = apvts.getParameter(k)) { auto* e = xml->createNewChildElement("PARAM"); e->setAttribute("id", k); e->setAttribute("value", (double) r->convertFrom0to1(r->getValue())); }
    if (tab < 0 || tab == 2) xml->setAttribute("graph", graphStr);     // only the FX and routing tab and main presets carry the routing
    xml->setAttribute("tab", tab);
    return xml;
}
void ShimmerstuffProcessor::applyUserPresetXml(const juce::XmlElement& xml)
{
    // validated: only known parameter ids are read, values are clamped by the parameters
    for (auto* e : xml.getChildIterator())
        if (e->hasTagName("PARAM"))
            if (auto* q = apvts.getParameter(e->getStringAttribute("id")))
                if (q->paramID != "byp")
                    q->setValueNotifyingHost(q->convertTo0to1((float) e->getDoubleAttribute("value", (double) q->convertFrom0to1(q->getValue()))));
    const auto g = xml.getStringAttribute("graph", {});
    if (g.isNotEmpty()) { const auto parsed = fx::parseGraph(g.toStdString()); graphStr = juce::String(fx::graphString(parsed)); graph = parsed; graphCode.store(encodeGraph(graph)); apvts.state.setProperty("graph", graphStr, nullptr); }
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
    const int rt = juce::jlimit(0, kLayouts, (int) get("route"));
    const fx::Graph g = decodeGraph(rt < kLayouts ? layoutCode[rt] : graphCode.load());
    float pkIn = 0, pkOut = 0, minDg = 1; double sq = 0; int tp = tapPos.load(std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < 9; ++j) sm[j] += k * (targets[j] - sm[j]);
        smDt += kT * (dtime - smDt);
        rs.mix = sm[0]; dp.mix = sm[1]; dp.feedback = sm[2]; dp.timeMs = smDt; eng.ip.satMix = sm[3]; wp.satMix = sm[4]; wp.squashMix = sm[5]; dk.amt = sm[6]; wp.width = sm[7]; wp.boostDb = sm[8];
        const float l = L[i], r = R[i], tl = useSc ? sL[i] : l, tr = useSc ? sR[i] : r;
        float ol, orr; eng.tick(g, l, r, tl, tr, ol, orr);
        if (byp) { ol = l; orr = r; }
        L[i] = ol; R[i] = orr;
        tapIn[(size_t) tp].store(0.5f * (l + r), std::memory_order_relaxed); tapOut[(size_t) tp].store(0.5f * (ol + orr), std::memory_order_relaxed); tp = (tp + 1) & (kTap - 1);
        sq += 0.5 * ((double) ol * ol + (double) orr * orr);
        pkIn = std::max(pkIn, std::max(std::fabs(l), std::fabs(r))); pkOut = std::max(pkOut, std::max(std::fabs(ol), std::fabs(orr)));
        minDg = std::min(minDg, eng.wfx.duck().dg);
    }
    tapPos.store(tp, std::memory_order_release);
    rms.store((float) std::sqrt(sq / (double) std::max(1, n)));
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
        const auto g = xml->getStringAttribute("graph", graphStr);
        const auto parsed = fx::parseGraph(g.toStdString());
        graphStr = juce::String(fx::graphString(parsed)); graph = parsed; graphCode.store(encodeGraph(graph));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ShimmerstuffProcessor(); }
