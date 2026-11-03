#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;
using namespace wm;

const juce::StringArray& ShimmerstuffProcessor::typeNames() { static const juce::StringArray n { "Plate", "Hall", "Velvet", "Room", "Chamber", "Arena", "Spring", "Gated", "Reverse" }; return n; }
const char* ShimmerstuffProcessor::layoutGraph(int i)
{
    static const char* g[kLayouts] = { "ifx>rev|del|mod>wfx", "ifx>rev>del>mod>wfx", "ifx>mod>rev>del>wfx", "ifx>del>rev>mod>wfx", "ifx>rev|del>mod>wfx", "ifx>rev>mod>wfx>del", "ifx>del>mod>wfx>rev",
        "rev>del>mod>wfx>ifx", "wfx>ifx>mod>rev>del", "rev|del|mod|ifx|wfx" };
    return g[juce::jlimit(0, kLayouts - 1, i)];
}
const juce::StringArray& ShimmerstuffProcessor::layoutNames()
{
    static const juce::StringArray n { "FX, Reverb, Delay and Modulation side by side, Wet FX", "FX, Reverb, Delay, Modulation, Wet FX", "FX, Modulation, Reverb, Delay, Wet FX", "FX, Delay, Reverb, Modulation, Wet FX",
        "FX, Reverb and Delay in parallel, Modulation, Wet FX", "FX, Reverb, Modulation, Wet FX, Delay", "FX, Delay, Modulation, Wet FX, Reverb (FX between)", "Reverb, Delay, Modulation, Wet FX, then FX",
        "Wet FX first, then FX, Modulation, Reverb, Delay", "Everything in parallel", "Custom (your own)" };
    return n;
}
const juce::StringArray& ShimmerstuffProcessor::ratioNames() { static const juce::StringArray n { "1/16", "1/8", "1/4", "1/2", "1 bar", "2 bars", "4 bars" }; return n; }

namespace
{
struct F { const char* id; const char* name; float lo, hi, def, mid; };
// id, name, min, max, default, skew centre (0 = linear). Same ids and defaults as the design mockup.
const F kFloats[] = {
    { "decay", "Decay", 0.f, 1.55f, 0.6f, 0.f }, { "damp", "Damping", 500.f, 16000.f, 6000.f, 3000.f }, { "predelay", "Pre-delay", 0.f, 250.f, 20.f, 40.f },
    { "size", "Size", 0.5f, 1.6f, 1.f, 0.f }, { "mod", "Modulation", 0.f, 1.f, 0.5f, 0.f }, { "color", "Velvet colour", -1.f, 1.f, 0.f, 0.f },
    { "shift", "Shimmer pitch", -12.f, 12.f, 12.f, 0.f }, { "shimmer", "Shimmer amount", 0.f, 1.f, 0.5f, 0.f },
    { "shift2", "Shimmer 2 pitch", -12.f, 12.f, 7.f, 0.f }, { "shimmer2", "Shimmer 2 amount", 0.f, 1.f, 0.f, 0.f },
    { "hp", "Wet high-pass", 20.f, 20000.f, 20.f, 400.f }, { "lp", "Wet low-pass", 20.f, 20000.f, 20000.f, 2500.f }, { "mix", "Reverb mix", 0.f, 1.f, 0.35f, 0.f },
    { "dtime", "Delay time", 20.f, 2000.f, 450.f, 400.f }, { "dfb", "Delay feedback", 0.f, 0.95f, 0.55f, 0.f },
    { "dshift", "Delay shift", -12.f, 12.f, 12.f, 0.f }, { "dshim", "Delay shimmer", 0.f, 1.f, 1.f, 0.f },
    { "ddamp", "Delay damping", 500.f, 16000.f, 5000.f, 3000.f }, { "dmix", "Delay mix", 0.f, 1.f, 0.4f, 0.f },
    { "i_hp", "Input high-pass", 20.f, 20000.f, 20.f, 400.f }, { "i_lp", "Input low-pass", 20.f, 20000.f, 20000.f, 2500.f }, { "lim_in", "Limiter in", -12.f, 24.f, 0.f, 0.f }, { "lim_out", "Limiter out", -12.f, 0.f, -1.f, 0.f }, { "i_band", "Input band shift", -4.f, 4.f, 0.f, 0.f }, { "rband", "Wet band shift", -4.f, 4.f, 0.f, 0.f },
    { "i_drive", "Input drive", 0.f, 30.f, 6.f, 0.f }, { "i_mix", "Input saturation mix", 0.f, 1.f, 0.f, 0.f },
    { "w_drive", "Wet drive", 0.f, 30.f, 8.f, 0.f }, { "w_smix", "Wet saturation mix", 0.f, 1.f, 0.f, 0.f },
    { "w_depth", "Squash depth", 0.f, 1.f, 0.6f, 0.f }, { "w_qmix", "Squash mix", 0.f, 1.f, 0.f, 0.f },
    { "w_width", "Wet width", 0.f, 2.f, 1.f, 0.f }, { "w_boost", "Wet boost", -12.f, 12.f, 0.f, 0.f },
    { "w_duck", "Duck amount", 0.f, 1.f, 0.f, 0.f }, { "w_drel", "Duck release", 20.f, 1000.f, 250.f, 200.f },
    { "w_drate", "Duck rate", 0.25f, 16.f, 2.f, 2.f },
    // Lexicat (Lexicon-style reverb module) controls and twin-delay extras. New ids; older saved states do not contain them and get these defaults.
    { "lexmix", "Lexicat blend", 0.f, 1.f, 0.f, 0.f },
    { "bass", "Bass multiply", 0.5f, 2.5f, 1.3f, 0.f }, { "xover", "Bass crossover", 100.f, 2000.f, 500.f, 500.f },
    { "shape", "Shape", 0.f, 1.f, 0.5f, 0.f }, { "spread", "Spread", 0.f, 1.f, 0.4f, 0.f }, { "diff", "Diffusion", 0.f, 1.f, 0.7f, 0.f },
    { "spin", "Spin", 0.f, 1.f, 0.5f, 0.f }, { "wander", "Wander", 0.f, 1.f, 0.5f, 0.f }, { "early", "Pre-echoes", 0.f, 1.f, 0.5f, 0.f },
    { "dxfb", "Delay cross feedback", -1.f, 1.f, 0.f, 0.f }, { "dwide", "Delay width", 0.f, 0.5f, 0.f, 0.f },
    { "dtap2", "Delay second voice", 0.f, 1.f, 0.f, 0.f }, { "dratio2", "Second voice time", 0.25f, 1.5f, 0.5f, 0.f },
    { "dvoices", "Delay voices", 1.f, 8.f, 1.f, 0.f }, { "dslope", "Delay slope", -1.f, 1.f, 0.f, 0.f }, { "dwander", "Delay wander", 0.f, 1.f, 0.f, 0.f },
    { "dspin", "Delay spin", 0.f, 1.f, 0.5f, 0.f }, { "ddiff", "Delay diffusion", 0.f, 1.f, 0.f, 0.f },
    // reverb types, delay character, modulation chain and the three Level knobs (dB, 0 = as before)
    { "rspec", "Reverb special", 0.f, 1.f, 0.5f, 0.f }, { "rlevel", "Reverb level", -12.f, 12.f, 0.f, 0.f }, { "splevel", "Space level", -12.f, 12.f, 0.f, 0.f }, { "shlevel", "Shimmer level", -12.f, 12.f, 0.f, 0.f }, { "lxlevel", "Lexicat level", -12.f, 12.f, 0.f, 0.f }, { "dlevel", "Delay level", -12.f, 12.f, 0.f, 0.f }, { "m_level", "Modulation level", -12.f, 12.f, 0.f, 0.f },
    { "dduck", "Delay ducker", 0.f, 1.f, 0.f, 0.f }, { "dthr", "Ducker threshold", 0.f, 1.f, 0.3f, 0.f }, { "dsmear", "Delay smear", 0.f, 1.f, 0.f, 0.f },
    { "dmod", "Delay mod depth", 0.f, 1.f, 0.5f, 0.f }, { "drate", "Delay mod rate", 0.1f, 8.f, 0.8f, 1.f }, { "m_mix", "Modulation mix", 0.f, 1.f, 1.f, 0.f },
};
// Per effect settings of the modulation chain: ids m0_speed ... m7_shift. Order of the effects: Chorus, Flanger, Phaser, Tremolo, Pan, Rotary, Vibrato, Detune.
struct MF { const char* key; const char* name; float lo, hi, mid; int kind; };   // kind 0 float, 1 int
const MF kModF[ShimmerstuffProcessor::kModKeys] = {
    { "speed", "speed", 0.05f, 20.f, 1.5f, 0 }, { "depth", "depth", 0.f, 1.f, 0.f, 0 }, { "voices", "voices", 1.f, 7.f, 0.f, 1 }, { "stages", "stages", 0.f, 2.f, 0.f, 1 },
    { "regen", "regen", -0.9f, 0.9f, 0.f, 0 }, { "pre", "pre-delay", 0.f, 20.f, 0.f, 0 }, { "diff", "diffusion", 0.f, 1.f, 0.f, 0 }, { "wave", "waveform", 0.f, 2.f, 0.f, 1 },
    { "phase", "phase", 0.f, 1.f, 0.f, 0 }, { "spread", "spread", 0.f, 1.f, 0.f, 0 }, { "drive", "drive", 0.f, 1.f, 0.f, 0 }, { "min", "min speed", 0.2f, 4.f, 0.f, 0 },
    { "max", "max speed", 2.f, 12.f, 0.f, 0 }, { "dop", "doppler", 0.f, 1.f, 0.f, 0 }, { "shift", "detune", 0.f, 100.f, 0.f, 0 } };
const float kModBase[ShimmerstuffProcessor::kModKeys] = { 0.8f, 0.5f, 2.f, 1.f, 0.f, 3.f, 0.f, 0.f, 0.5f, 0.7f, 0.2f, 0.8f, 6.5f, 0.5f, 12.f };
float modDefault(int t, int k)
{
    struct O { int t, k; float v; };
    static const O o[] = { { 1, 0, 0.25f }, { 1, 1, 0.8f }, { 1, 4, 0.5f }, { 1, 5, 1.f }, { 2, 0, 0.3f }, { 2, 1, 0.7f }, { 2, 4, 0.4f }, { 3, 0, 4.f }, { 3, 1, 0.6f }, { 4, 0, 0.5f }, { 4, 1, 1.f },
        { 6, 0, 5.f }, { 6, 1, 0.4f }, { 7, 5, 12.f }, { 7, 2, 5.f }, { 7, 14, 18.f } };
    for (const auto& e : o) if (e.t == t && e.k == k) return e.v;
    return kModBase[k];
}
const char* const kModNames[8] = { "Chorus", "Flanger", "Phaser", "Tremolo", "Pan", "Rotary", "Vibrato", "Detune" };

}

APVTS::ParameterLayout ShimmerstuffProcessor::layout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "type", 1 }, "Reverb type", typeNames(), 1));
    p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "shim_on", 1 }, "Shimmer on", true));
    p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "dtype", 1 }, "Delay type", juce::StringArray { "Studio", "Tape", "Pong", "Modulated", "Reverse" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "m_type", 1 }, "Modulation: effect being edited", juce::StringArray(kModNames, 8), 0));
    p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "lexmode", 1 }, "Lexicat mode", juce::StringArray { "Hall", "Random Hall", "Plate", "Chamber", "Room" }, 0));
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
    for (int t = 0; t < 8; ++t)
    {
        p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ juce::String("mo") + juce::String(t), 1 }, juce::String(kModNames[t]) + " on", t == 0));
        for (int k = 0; k < kModKeys; ++k)
        {
            const auto& m = kModF[k]; const juce::String id = "m" + juce::String(t) + "_" + m.key, nm = juce::String(kModNames[t]) + " " + m.name;
            if (m.kind == 1) p.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{ id, 1 }, nm, (int) m.lo, (int) m.hi, (int) modDefault(t, k)));
            else { juce::NormalisableRange<float> r(m.lo, m.hi); if (m.mid > 0.f) r.setSkewForCentre(m.mid); p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id, 1 }, nm, r, modDefault(t, k))); }
        }
    }
    static const B bools[] = { { "rev_on", "Reverb on", true }, { "del_on", "Delay on", false }, { "ifx_on", "Input FX on", true }, { "wfx_on", "Wet FX on", true }, { "mod_on", "Modulation on", false }, { "dhold", "Delay hold", false }, { "w_lim", "Wet limiter", false }, { "byp", "Bypass", false } };
    for (const auto& b : bools)
        p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ b.id, 1 }, b.name, b.def));
    return { p.begin(), p.end() };
}

// The graph is stored in one number: for each of the five nodes, its stage (3 bits) and its place inside the stage (3 bits).
static uint64_t encodeGraph(const fx::Graph& g)
{
    uint64_t c = 0;
    for (int s = 0; s < g.stages; ++s) for (int j = 0; j < g.count[s]; ++j) { const int n = g.items[s][j]; c |= uint64_t(s | (j << 3)) << (6 * n); }
    return c;
}
static fx::Graph decodeGraph(uint64_t c)
{
    fx::Graph g; int cnt[fx::kNumNodes] = {}, item[fx::kNumNodes][fx::kNumNodes] = {};
    for (int n = 0; n < fx::kNumNodes; ++n) { const int s = int((c >> (6 * n)) & 7u), j = int((c >> (6 * n + 3)) & 7u); if (s < fx::kNumNodes && j < fx::kNumNodes) { item[s][j] = n; cnt[s] = std::max(cnt[s], j + 1); } }
    for (int s = 0; s < fx::kNumNodes; ++s) if (cnt[s] > 0) { for (int j = 0; j < cnt[s]; ++j) g.items[g.stages][j] = item[s][j]; g.count[g.stages++] = cnt[s]; }
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
    for (int t = 0; t < 8; ++t)
    {
        modOnP[t] = apvts.getRawParameterValue("mo" + juce::String(t));
        for (int k = 0; k < kModKeys; ++k) modP[t][k] = apvts.getRawParameterValue("m" + juce::String(t) + "_" + kModF[k].key);
    }
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
static const std::vector<ssp::Pre>& tabList(int tab) { return tab == 0 ? ssp::reverbPresets() : tab == 1 ? ssp::delayPresets() : tab == 2 ? ssp::modPresets() : ssp::fxPresets(); }
static const std::vector<const char*>& tabKeys(int tab) { return tab == 0 ? ssp::reverbKeys() : tab == 1 ? ssp::delayKeys() : tab == 2 ? ssp::modKeys() : ssp::fxKeys(); }
int ShimmerstuffProcessor::numTabPresets(int tab) const { return (int) tabList(juce::jlimit(0, 3, tab)).size(); }
juce::String ShimmerstuffProcessor::tabPresetName(int tab, int i) const
{
    const auto& l = tabList(juce::jlimit(0, 3, tab));
    return juce::isPositiveAndBelow(i, (int) l.size()) ? l[(size_t) i].name : "";
}
void ShimmerstuffProcessor::applyTabPreset(int tab, int i)
{
    if (tab < 0 || tab > 3) return;
    const auto& l = tabList(tab);
    if (!juce::isPositiveAndBelow(i, (int) l.size())) return;
    lastTabPreset[tab] = i;
    applyKeys(tabKeys(tab), &l[(size_t) i], tab == 3);
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
    applyKeys(ssp::modKeys(), m.mod != nullptr ? find(ssp::modPresets(), m.mod) : nullptr, true);
    applyKeys(ssp::fxKeys(), find(ssp::fxPresets(), m.fx), true);
    for (const auto& kv : m.extra) setParam(kv.first, kv.second);
    setParam("byp", 0.f);
    for (int t = 0; t < 4; ++t) lastTabPreset[t] = -1;
}
int ShimmerstuffProcessor::getNumPrograms() { return numMainPresets(); }
const juce::String ShimmerstuffProcessor::getProgramName(int i) { return mainPresetName(i); }
void ShimmerstuffProcessor::setCurrentProgram(int i) { applyMainPreset(i); }

juce::XmlElement* ShimmerstuffProcessor::makePresetXml(int tab) const
{
    // a small validated format: PARAM elements with id and value, plus the custom graph
    auto* xml = new juce::XmlElement("STATE");
    const std::vector<const char*>& keys = tab < 0 ? ssp::fxKeys() : tabKeys(tab);
    if (tab < 0)
    {
        for (auto* par : const_cast<ShimmerstuffProcessor*>(this)->getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(par); r != nullptr && r->paramID != "byp")
            { auto* e = xml->createNewChildElement("PARAM"); e->setAttribute("id", r->paramID); e->setAttribute("value", (double) r->convertFrom0to1(r->getValue())); }
    }
    else
        for (auto* k : keys)
            if (auto* r = apvts.getParameter(k)) { auto* e = xml->createNewChildElement("PARAM"); e->setAttribute("id", k); e->setAttribute("value", (double) r->convertFrom0to1(r->getValue())); }
    if (tab < 0 || tab == 3) xml->setAttribute("graph", graphStr);     // only the FX and routing tab and main presets carry the routing
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
    auto& rs = eng.rs; rs.type = (wm::engine::ReverbType) juce::jlimit(0, 8, (int) get("type")); rs.p.special = get("rspec");
    eng.modOn = get("mod_on") > .5f;
    rs.p.decay = get("decay"); rs.p.dampHz = get("damp"); rs.p.preDelayMs = get("predelay"); rs.p.size = get("size"); rs.p.modDepth = get("mod"); rs.p.color = get("color");
    rs.p.bass = get("bass"); rs.p.xoverHz = get("xover"); rs.p.shape = get("shape"); rs.p.spread = get("spread"); rs.p.diffusion = get("diff");
    rs.lexBlend = get("lexmix"); rs.p.lexMode = juce::jlimit(0, 4, (int) get("lexmode")); rs.shimmer = get("shim_on") > .5f;
    { auto lg = [&](const char* k) { return std::pow(10.f, get(k) / 20.f); }; rs.spGain = lg("splevel"); rs.shGain = lg("shlevel"); rs.lxGain = lg("lxlevel"); }
    rs.p.spin = get("spin"); rs.p.wander = get("wander"); rs.p.early = get("early");
    rs.shimmerSemis = get("shift"); rs.shimmerAmount = get("shimmer"); rs.shimmer2Semis = get("shift2"); rs.shimmer2Amount = get("shimmer2");
    { const float k = std::pow(2.f, get("rband")); rs.wetLpHz = juce::jlimit(20.f, 20000.f, get("lp") * k); rs.wetHpHz = juce::jmin(juce::jlimit(20.f, 20000.f, get("hp") * k), rs.wetLpHz); }
    auto& dp = eng.dp; dp.shiftSemis = get("dshift"); dp.shimmerAmount = get("dshim"); dp.dampHz = get("ddamp");
    dp.voices = juce::jlimit(1, 8, (int) std::lround(get("dvoices"))); dp.slope = get("dslope"); dp.wander = get("dwander"); dp.spin = get("dspin"); dp.diffusion = get("ddiff");
    dp.type = juce::jlimit(0, 4, (int) get("dtype")); dp.hold = get("dhold") > .5f; dp.duck = get("dduck"); dp.duckThr = get("dthr"); dp.smear = get("dsmear"); dp.modDepth = get("dmod"); dp.modRate = get("drate");
    for (int t = 0; t < 8; ++t)
    {
        eng.modUse[t] = modOnP[t]->load() > .5f; auto& m = eng.mps[t]; auto v = [&](int k) { return modP[t][k]->load(); };
        m.type = t; m.speed = v(0); m.depth = v(1); m.voices = juce::roundToInt(v(2)); m.stages = juce::roundToInt(v(3)); m.regen = v(4); m.preDelayMs = v(5); m.diffusion = v(6); m.waveform = juce::roundToInt(v(7));
        m.phase = v(8); m.spread = v(9); m.drive = v(10); m.minHz = v(11); m.maxHz = v(12); m.doppler = v(13); m.shift = v(14);
    }
    dp.crossFb = get("dxfb"); dp.wide = get("dwide"); dp.tap2 = get("dtap2"); dp.tap2Ratio = get("dratio2");
    { const float k = std::pow(2.f, get("i_band")); eng.ip.lp = juce::jlimit(20.f, 20000.f, get("i_lp") * k); eng.ip.hp = juce::jmin(juce::jlimit(20.f, 20000.f, get("i_hp") * k), eng.ip.lp); } eng.ip.drive = get("i_drive");
    eng.limOn = get("w_lim") > .5f; eng.limInDb = get("lim_in"); eng.limOutDb = get("lim_out");
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
    auto lin = [](float db) { return std::pow(10.f, db / 20.f); };
    const float targets[16] = { get("mix"), get("dmix"), get("dfb"), get("i_mix"), get("w_smix"), get("w_qmix"), get("w_duck"), get("w_width"), get("w_boost"), lin(get("rlevel")), lin(get("dlevel")), lin(get("m_level")), get("m_mix"), 0.f, 0.f, 0.f };
    if (!smInit) { for (int i = 0; i < 13; ++i) sm[i] = targets[i]; smDt = get("dtime"); smInit = true; }
    const float k = 1.f - std::exp(-1.f / (0.015f * (float) fs)), kT = 1.f - std::exp(-1.f / (0.08f * (float) fs));
    const float dtime = get("dtime");
    const int rt = juce::jlimit(0, kLayouts, (int) get("route"));
    const fx::Graph g = decodeGraph(rt < kLayouts ? layoutCode[rt] : graphCode.load());
    float pkIn = 0, pkOut = 0, minDg = 1; double sq = 0; int tp = tapPos.load(std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < 13; ++j) sm[j] += k * (targets[j] - sm[j]);
        smDt += kT * (dtime - smDt);
        rs.mix = sm[0]; dp.mix = sm[1]; dp.feedback = sm[2]; dp.timeMs = smDt; eng.ip.satMix = sm[3]; wp.satMix = sm[4]; wp.squashMix = sm[5]; dk.amt = sm[6]; wp.width = sm[7]; wp.boostDb = sm[8]; rs.level = sm[9]; dp.level = sm[10]; eng.modLevel = sm[11]; eng.modMix = sm[12];
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
        auto tree = juce::ValueTree::fromXml(*xml);
        auto param = [&](const char* id) { return tree.getChildWithProperty("id", id); };
        if (auto t = param("type"); t.isValid() && !param("shim_on").isValid())   // older state: shimmer was part of the type
        {
            const int old = juce::roundToInt((float) t.getProperty("value", 3.f));
            const int nt = old == 2 ? 0 : old == 3 ? 1 : old == 4 ? 2 : juce::jlimit(0, 2, old);
            t.setProperty("value", (float) nt, nullptr);
            juce::ValueTree c("PARAM"); c.setProperty("id", "shim_on", nullptr); c.setProperty("value", (old == 2 || old == 3) ? 1.f : 0.f, nullptr); tree.appendChild(c, nullptr);
        }
        if (!param("mod_on").isValid())   // older state: no modulation box and the routing list was shorter
        {
            static const int remap[9] = { 1, 3, 4, 5, 6, 7, 8, 9, 10 };
            if (auto r = param("route"); r.isValid()) r.setProperty("value", (float) remap[juce::jlimit(0, 8, juce::roundToInt((float) r.getProperty("value", 0.f)))], nullptr);
            else { juce::ValueTree c("PARAM"); c.setProperty("id", "route", nullptr); c.setProperty("value", 1.f, nullptr); tree.appendChild(c, nullptr); }
        }
        apvts.replaceState(tree);
        const auto g = xml->getStringAttribute("graph", graphStr);
        const auto parsed = fx::parseGraph(g.toStdString());
        graphStr = juce::String(fx::graphString(parsed)); graph = parsed; graphCode.store(encodeGraph(graph));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ShimmerstuffProcessor(); }
