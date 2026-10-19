#include "PluginEditor.h"
#include <catui/CatLookAndFeel.h>

using namespace ss;

// ---------------------------------------------------------------- helpers
void Knob::init(juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& name,
                const juce::String& suffix, const juce::String& tip, bool pct)
{
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 15);
    if (pct)
    {
        s.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + " %"; };
        s.valueFromTextFunction = [](const juce::String& t) { return t.getDoubleValue() / 100.0; };
    }
    else s.setTextValueSuffix(suffix);
    s.setTooltip(tip); s.setColour(juce::Slider::rotarySliderFillColourId, col::lilac);
    l.setText(name, juce::dontSendNotification); l.setJustificationType(juce::Justification::centred);
    l.setFont(juce::FontOptions(12.f)); l.setColour(juce::Label::textColourId, col::dim);
    parent.addAndMakeVisible(s); parent.addAndMakeVisible(l);
    a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, id, s);
}
void Combo::init(juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& name,
                 const juce::StringArray& items, const juce::String& tip)
{
    c.addItemList(items, 1); c.setTooltip(tip);
    l.setText(name, juce::dontSendNotification); l.setFont(juce::FontOptions(12.f)); l.setColour(juce::Label::textColourId, col::dim);
    parent.addAndMakeVisible(c); parent.addAndMakeVisible(l);
    a = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, id, c);
}
static void setNorm(ShimmerstuffProcessor& p, const juce::String& id, float n)
{ if (auto* q = p.apvts.getParameter(id)) q->setValueNotifyingHost(juce::jlimit(0.f, 1.f, n)); }
static float getNorm(ShimmerstuffProcessor& p, const juce::String& id)
{ if (auto* q = p.apvts.getParameter(id)) return q->getValue(); return 0.f; }
static void gesture(ShimmerstuffProcessor& p, const juce::String& id, bool begin)
{ if (auto* q = p.apvts.getParameter(id)) { if (begin) q->beginChangeGesture(); else q->endChangeGesture(); } }

// ---------------------------------------------------------------- DecayView
DecayView::DecayView(ShimmerstuffProcessor& p) : proc(p) { startTimerHz(30); }

float DecayView::signature() const
{
    float s = 0; int i = 1;
    for (const char* id : { "type", "decay", "damp", "predelay", "size", "mod", "color", "shift", "shimmer", "shift2", "shimmer2", "hp", "lp" })
        s += proc.get(id) * (float) (i++) * 1.37f;
    return s;
}
void DecayView::timerCallback()
{
    if (!isShowing()) return;
    const float sig = signature();
    if (sig != lastSig) { lastSig = sig; stable = 0; dirty = true; }
    else if (dirty && ++stable >= 2) { measure(); dirty = false; repaint(); }
    if (drag >= 0 || dirty) repaint();
}
void DecayView::measure()
{
    // the real reverb, measured at a 16 kHz rate: a full wet impulse response, then Schroeder backward integration (energy decay curve)
    const float sr = 16000.f; wm::engine::ReverbUnit u; u.prepare(sr);
    wm::engine::ReverbSettings s;
    s.type = (wm::engine::ReverbType) juce::jlimit(0, 4, (int) proc.get("type"));
    s.p.decay = proc.get("decay"); s.p.dampHz = proc.get("damp"); s.p.preDelayMs = proc.get("predelay"); s.p.size = proc.get("size");
    s.p.modDepth = proc.get("mod"); s.p.color = proc.get("color"); s.shimmerSemis = proc.get("shift"); s.shimmerAmount = proc.get("shimmer");
    s.shimmer2Semis = proc.get("shift2"); s.shimmer2Amount = proc.get("shimmer2"); s.wetHpHz = proc.get("hp"); s.wetLpHz = juce::jmin(proc.get("lp"), 7500.f); s.mix = 1.f;
    const int N = (int) (kSeconds * sr); std::vector<double> e((size_t) N);
    for (int i = 0; i < N; ++i) { float ol, orr; const float in = i == 0 ? 1.f : 0.f; u.process(in, in, ol, orr, s); e[(size_t) i] = 0.5 * ((double) ol * ol + (double) orr * orr); }
    double total = 0; for (double v : e) total += v;
    edc.assign((size_t) kPts, -80.f);
    if (total <= 1e-20) return;
    std::vector<double> back((size_t) N + 1, 0.0);
    for (int i = N - 1; i >= 0; --i) back[(size_t) i] = back[(size_t) i + 1] + e[(size_t) i];
    for (int k = 0; k < kPts; ++k) { const int i = juce::jmin(N - 1, k * N / kPts); edc[(size_t) k] = juce::jlimit(-80.f, 0.f, (float) (10.0 * std::log10(back[(size_t) i] / total + 1e-12))); }
}
float DecayView::rt60() const
{
    for (size_t k = 0; k < edc.size(); ++k) if (edc[k] <= -60.f) return (float) k / (float) kPts * kSeconds;
    return kSeconds;
}
juce::Rectangle<float> DecayView::plot() const { return juce::Rectangle<float>(46.f, 10.f, (float) getWidth() - 60.f, (float) getHeight() - 10.f - 86.f); }
juce::Rectangle<float> DecayView::track(int i) const
{
    const auto p = plot(); return { p.getX(), p.getBottom() + 22.f + (float) i * 22.f, p.getWidth(), 10.f };
}
void DecayView::paint(juce::Graphics& g)
{
    g.fillAll(col::disp);
    const auto p = plot();
    g.setFont(juce::FontOptions(10.5f));
    for (int d = 0; d >= -80; d -= 20)
    {
        const float y = p.getY() + (float) (-d) / 80.f * p.getHeight();
        g.setColour(juce::Colour(0xff1b2523)); g.drawHorizontalLine((int) y, p.getX(), p.getRight());
        g.setColour(col::dim); g.drawText(juce::String(d) + " dB", 2, (int) y - 6, 42, 12, juce::Justification::right);
    }
    for (int t = 0; t <= (int) kSeconds; t += 2)
    {
        const float x = p.getX() + (float) t / kSeconds * p.getWidth();
        g.setColour(juce::Colour(0xff1b2523)); g.drawVerticalLine((int) x, p.getY(), p.getBottom());
        g.setColour(col::dim); g.drawText(juce::String(t) + " s", (int) x - 14, (int) p.getBottom() + 1, 28, 12, juce::Justification::centred);
    }
    if (edc.size() == (size_t) kPts)
    {
        juce::Path path;
        for (int k = 0; k < kPts; ++k)
        {
            const float x = p.getX() + (float) k / (float) (kPts - 1) * p.getWidth(), y = p.getY() + (-edc[(size_t) k]) / 80.f * p.getHeight();
            k ? path.lineTo(x, y) : path.startNewSubPath(x, y);
        }
        juce::Path fill(path); fill.lineTo(p.getRight(), p.getY()); fill.lineTo(p.getX(), p.getY()); fill.closeSubPath();
        g.setColour(col::purple.withAlpha(0.18f)); g.fillPath(fill);
        g.setColour(col::purple); g.strokePath(path, juce::PathStrokeType(2.f));
    }
    g.setColour(col::dim); g.setFont(juce::FontOptions(11.f));
    g.drawText("Energy decay of the real reverb: drag the yellow dot (sideways Decay, up and down Size)", (int) p.getX() + 6, (int) p.getY() + 2, (int) p.getWidth() - 12, 14, juce::Justification::left);
    // yellow handle at the -60 dB point
    float hx = p.getX() + rt60() / kSeconds * p.getWidth(), hy = p.getY() + 60.f / 80.f * p.getHeight();
    if (drag == 0) { hx = juce::jlimit(p.getX(), p.getRight(), startPos.x + (float) 0); }
    g.setColour(col::yellow); g.fillEllipse(hx - 7.f, hy - 7.f, 14.f, 14.f);
    g.setColour(col::yellow.withAlpha(0.8f)); g.drawText(juce::String(rt60(), 1) + " s", (int) hx + 10, (int) hy - 8, 60, 14, juce::Justification::left);
    // tracks
    const char* names[3] = { "Pre-delay", "Damping", "Mix" }; const char* ids[3] = { "predelay", "damp", "mix" };
    const juce::Colour cols[3] = { juce::Colour(0xff01FCDC), col::blue, col::pink };
    for (int i = 0; i < 3; ++i)
    {
        const auto t = track(i); g.setColour(col::dim); g.setFont(juce::FontOptions(11.f));
        g.drawText(names[i], 2, (int) t.getY() - 4, 44, 14, juce::Justification::right);
        g.setColour(col::line); g.fillRoundedRectangle(t.getX(), t.getCentreY() - 2.f, t.getWidth(), 4.f, 2.f);
        const float x = t.getX() + getNorm(proc, ids[i]) * t.getWidth();
        g.setColour(cols[i]); g.fillEllipse(x - 7.f, t.getCentreY() - 7.f, 14.f, 14.f);
        juce::String txt; auto* q = proc.apvts.getParameter(ids[i]);
        if (q) txt = q->getText(q->getValue(), 12) + (i == 0 ? " ms" : i == 1 ? " Hz" : "");
        if (i == 2) txt = juce::String(juce::roundToInt(proc.get("mix") * 100.f)) + " %";
        g.drawText(txt, (int) (x + 10.f), (int) t.getY() - 4, 90, 14, juce::Justification::left);
    }
}
void DecayView::mouseDown(const juce::MouseEvent& e)
{
    drag = -1; const auto p = plot();
    const float hx = p.getX() + rt60() / kSeconds * p.getWidth(), hy = p.getY() + 60.f / 80.f * p.getHeight();
    if (e.position.getDistanceFrom({ hx, hy }) < 16.f)
    {
        drag = 0; startDecay = getNorm(proc, "decay"); startSize = getNorm(proc, "size"); startPos = e.position; gesture(proc, "decay", true); gesture(proc, "size", true); return;
    }
    const char* ids[3] = { "predelay", "damp", "mix" };
    for (int i = 0; i < 3; ++i)
        if (track(i).expanded(0.f, 9.f).contains(e.position)) { drag = i + 1; dragId = ids[i]; gesture(proc, dragId, true); mouseDrag(e); return; }
}
void DecayView::mouseDrag(const juce::MouseEvent& e)
{
    if (drag == 0)
    {
        const auto p = plot(); const float dx = e.position.x - startPos.x, dy = e.position.y - startPos.y;
        setNorm(proc, "decay", startDecay + dx / p.getWidth() * 1.2f);
        setNorm(proc, "size", startSize - dy / p.getHeight() * 1.0f);
    }
    else if (drag > 0) { const auto t = track(drag - 1); setNorm(proc, dragId, (e.position.x - t.getX()) / t.getWidth()); }
}
void DecayView::mouseUp(const juce::MouseEvent&)
{
    if (drag == 0) { gesture(proc, "decay", false); gesture(proc, "size", false); }
    else if (drag > 0) gesture(proc, dragId, false);
    drag = -1;
}

// ---------------------------------------------------------------- EchoView
void EchoView::paint(juce::Graphics& g)
{
    g.fillAll(col::disp);
    const auto r = getLocalBounds().toFloat().reduced(12.f, 20.f);
    const float fb = proc.get("dfb"), t = proc.get("dtime"), shim = proc.get("dshim"), semis = proc.get("dshift") * shim, damp = proc.get("ddamp"), total = 6000.f;
    g.setFont(juce::FontOptions(11.f)); g.setColour(col::dim);
    g.drawText("Echoes: drag the first bar sideways for the time, any bar up or down for the feedback", getLocalBounds().withHeight(18).reduced(10, 0), juce::Justification::left);
    for (int ms = 0; ms <= (int) total; ms += 1000)
    {
        const float x = r.getX() + (float) ms / total * r.getWidth(); g.setColour(juce::Colour(0xff1b2523)); g.drawVerticalLine((int) x, r.getY(), r.getBottom());
        g.setColour(col::dim); g.drawText(juce::String(ms) + " ms", (int) x - 22, (int) r.getBottom() + 2, 44, 12, juce::Justification::centred);
    }
    float level = 1.f; const float tone = 1.f - 0.06f * std::fabs(semis) / 12.f * (5000.f / juce::jmax(500.f, damp));
    for (int k = 1; (float) k * t < total && k < 60; ++k)
    {
        level *= fb * juce::jlimit(0.2f, 1.f, tone); if (level < 0.01f) break;
        const float x = r.getX() + (float) k * t / total * r.getWidth(), h = r.getHeight() * juce::jmin(1.f, level);
        g.setColour(col::mint.withAlpha(0.9f)); g.fillRect(x - 3.f, r.getBottom() - h, 6.f, h);
        g.setColour(col::lilac); g.setFont(juce::FontOptions(11.f));
        g.drawText(juce::String(juce::roundToInt(semis * (float) k)) + " st", (int) x - 22, (int) (r.getBottom() - h) - 14, 44, 12, juce::Justification::centred);
    }
    g.setColour(col::yellow); g.fillEllipse(r.getX() + t / total * r.getWidth() - 6.f, r.getY() + 4.f, 12.f, 12.f);
}
void EchoView::mouseDown(const juce::MouseEvent& e)
{
    const auto r = getLocalBounds().toFloat().reduced(12.f, 20.f);
    const float x1 = r.getX() + proc.get("dtime") / 6000.f * r.getWidth();
    drag = std::abs(e.position.x - x1) < 14.f && e.position.y < r.getY() + 30.f ? 0 : 1;
    gesture(proc, drag == 0 ? "dtime" : "dfb", true); mouseDrag(e);
}
void EchoView::mouseDrag(const juce::MouseEvent& e)
{
    const auto r = getLocalBounds().toFloat().reduced(12.f, 20.f);
    if (drag == 0) { const float ms = juce::jlimit(20.f, 2000.f, (e.position.x - r.getX()) / r.getWidth() * 6000.f); setNorm(proc, "dtime", proc.apvts.getParameter("dtime")->convertTo0to1(ms)); }
    else if (drag == 1) setNorm(proc, "dfb", (r.getBottom() - e.position.y) / r.getHeight() / 0.95f);
}
void EchoView::mouseUp(const juce::MouseEvent&) { if (drag >= 0) gesture(proc, drag == 0 ? "dtime" : "dfb", false); drag = -1; }

// ---------------------------------------------------------------- ShapeView
void ShapeView::paint(juce::Graphics& g)
{
    const int sel = (int) proc.get("w_dshape"), n = wm::fx::kNumShapes; const float cw = (float) getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        const auto cell = juce::Rectangle<float>(cw * (float) i + 2.f, 2.f, cw - 4.f, (float) getHeight() - 4.f);
        g.setColour(i == sel ? juce::Colour(0xff241238) : col::panel2); g.fillRoundedRectangle(cell, 4.f);
        g.setColour(i == sel ? col::lilac : col::line); g.drawRoundedRectangle(cell, 4.f, 1.f);
        juce::Path pa; const auto in = cell.reduced(5.f, 4.f).withTrimmedBottom(11.f);
        for (int k = 0; k <= 30; ++k)
        {
            const float v = 1.f - wm::fx::shapeAt(i, (float) k / 30.f), x = in.getX() + (float) k / 30.f * in.getWidth(), y = in.getBottom() - v * in.getHeight();
            k ? pa.lineTo(x, y) : pa.startNewSubPath(x, y);
        }
        g.setColour(col::yellow); g.strokePath(pa, juce::PathStrokeType(1.5f));
        g.setColour(col::dim); g.setFont(juce::FontOptions(9.5f)); g.drawText(wm::fx::shapeName(i), cell.withTrimmedTop(cell.getHeight() - 12.f).toNearestInt(), juce::Justification::centred);
    }
}
void ShapeView::mouseDown(const juce::MouseEvent& e)
{
    const int i = juce::jlimit(0, wm::fx::kNumShapes - 1, (int) (e.position.x / ((float) getWidth() / (float) wm::fx::kNumShapes)));
    gesture(proc, "w_dshape", true); proc.setParam("w_dshape", (float) i); gesture(proc, "w_dshape", false); repaint();
}

// ---------------------------------------------------------------- RouteView
const char* RouteView::nodeName(int n) { static const char* s[4] = { "Input FX", "Reverb", "Delay", "Wet FX" }; return s[n & 3]; }
const char* RouteView::onId(int n) { static const char* s[4] = { "ifx_on", "rev_on", "del_on", "wfx_on" }; return s[n & 3]; }
const char* RouteView::valId(int n) { static const char* s[4] = { "i_mix", "mix", "dmix", "w_boost" }; return s[n & 3]; }
const char* RouteView::valLabel(int n) { static const char* s[4] = { "saturation", "mix", "mix", "boost" }; return s[n & 3]; }
float RouteView::colX(int stage) const { return (float) getWidth() * ((float) stage + 0.5f) / (float) (wm::fx::parseGraph(proc.getGraph().toStdString()).stages + 2); }
juce::Rectangle<float> RouteView::boxRect(int stage, int idx, int count) const
{
    const float bw = juce::jmin(150.f, (float) getWidth() / (float) (wm::fx::parseGraph(proc.getGraph().toStdString()).stages + 2) - 20.f), bh = 86.f, gap = 12.f;
    const float total = (float) count * bh + (float) (count - 1) * gap, y0 = ((float) getHeight() - total) * 0.5f + (float) idx * (bh + gap);
    return { colX(stage + 1) - bw * 0.5f, y0, bw, bh };
}
std::vector<RouteView::Box> RouteView::boxes() const
{
    std::vector<Box> v; const auto g = wm::fx::parseGraph(proc.getGraph().toStdString());
    for (int s = 0; s < g.stages; ++s) for (int j = 0; j < g.count[s]; ++j) v.push_back({ g.items[s][j], boxRect(s, j, g.count[s]) });
    return v;
}
void RouteView::paint(juce::Graphics& g)
{
    g.fillAll(col::disp);
    const auto gr = wm::fx::parseGraph(proc.getGraph().toStdString());
    const auto bx = boxes(); const float cy = (float) getHeight() * 0.5f;
    auto boxOf = [&](int node) { for (auto& b : bx) if (b.node == node) return b.r; return juce::Rectangle<float>(); };
    g.setColour(col::dim); g.setFont(juce::FontOptions(11.f));
    g.drawText("Drag a box to rearrange. Power corner: on or off. Corner arrow: jump to its controls. Drag a value up or down to change it.", getLocalBounds().withHeight(16).reduced(8, 0), juce::Justification::left);
    const juce::Point<float> inP(colX(0) + 22.f, cy), outP(colX(gr.stages + 1) - 22.f, cy);
    auto link = [&](juce::Point<float> a, juce::Point<float> b) { juce::Path p; p.startNewSubPath(a); p.cubicTo(a.x + 30.f, a.y, b.x - 30.f, b.y, b.x, b.y); g.setColour(col::lilac.withAlpha(0.7f)); g.strokePath(p, juce::PathStrokeType(2.f)); };
    for (int j = 0; j < gr.count[0]; ++j) { const auto r = boxOf(gr.items[0][j]); link(inP, { r.getX(), r.getCentreY() }); }
    for (int s = 0; s + 1 < gr.stages; ++s)
        for (int a = 0; a < gr.count[s]; ++a) for (int b = 0; b < gr.count[s + 1]; ++b)
        { const auto ra = boxOf(gr.items[s][a]), rb = boxOf(gr.items[s + 1][b]); link({ ra.getRight(), ra.getCentreY() }, { rb.getX(), rb.getCentreY() }); }
    for (int j = 0; j < gr.count[gr.stages - 1]; ++j) { const auto r = boxOf(gr.items[gr.stages - 1][j]); link({ r.getRight(), r.getCentreY() }, outP); }
    for (auto pt : { std::make_pair(inP, "IN"), std::make_pair(outP, "OUT") })
    { g.setColour(col::panel2); g.fillEllipse(pt.first.x - 22.f, pt.first.y - 22.f, 44.f, 44.f); g.setColour(col::lilac); g.drawEllipse(pt.first.x - 22.f, pt.first.y - 22.f, 44.f, 44.f, 1.5f); g.setColour(col::ink); g.setFont(juce::FontOptions(13.f, juce::Font::bold)); g.drawText(pt.second, (int) pt.first.x - 22, (int) pt.first.y - 8, 44, 16, juce::Justification::centred); }
    for (auto& b : bx)
    {
        auto r = b.r; if (moving && b.node == dragNode) r = r.withCentre(cur);
        const bool on = proc.get(onId(b.node)) > .5f; const juce::Colour c = b.node == wm::fx::kRev ? col::purple : b.node == wm::fx::kDel ? col::mint : col::orange;
        g.setColour(col::panel2.withAlpha(moving && b.node == dragNode ? 0.8f : 1.f)); g.fillRoundedRectangle(r, 8.f);
        g.setColour(on ? c : col::line); g.drawRoundedRectangle(r, 8.f, on ? 2.f : 1.f);
        g.setColour(on ? c : col::dim); g.fillEllipse(r.getX() + 7.f, r.getY() + 7.f, 12.f, 12.f);   // power corner
        g.setColour(col::ground); g.drawEllipse(r.getX() + 7.f, r.getY() + 7.f, 12.f, 12.f, 1.f);
        juce::Path tabIcon; const auto t = juce::Rectangle<float>(r.getRight() - 22.f, r.getY() + 7.f, 14.f, 12.f);   // "open in tab" corner icon
        tabIcon.addRectangle(t); g.setColour(col::dim); g.strokePath(tabIcon, juce::PathStrokeType(1.2f));
        g.drawLine(t.getRight() - 3.f, t.getY() + 3.f, t.getRight() - 10.f, t.getBottom() - 3.f, 1.2f); g.drawLine(t.getRight() - 3.f, t.getY() + 3.f, t.getRight() - 7.f, t.getY() + 3.f, 1.2f); g.drawLine(t.getRight() - 3.f, t.getY() + 3.f, t.getRight() - 3.f, t.getY() + 7.f, 1.2f);
        g.setColour(on ? col::ink : col::dim); g.setFont(juce::FontOptions(14.f, juce::Font::bold));
        g.drawText(nodeName(b.node), r.withTrimmedTop(20.f).withHeight(22.f).toNearestInt(), juce::Justification::centred);
        auto* q = proc.apvts.getParameter(valId(b.node));
        juce::String txt = juce::String(valLabel(b.node)) + " ";
        if (b.node == wm::fx::kWfx) txt += juce::String(proc.get("w_boost"), 1) + " dB"; else txt += juce::String(juce::roundToInt(proc.get(valId(b.node)) * 100.f)) + " %";
        juce::ignoreUnused(q);
        const auto vr = r.withTrimmedTop(r.getHeight() - 28.f).reduced(8.f, 3.f);
        g.setColour(col::disp); g.fillRoundedRectangle(vr, 4.f); g.setColour(on ? col::yellow : col::dim); g.setFont(juce::FontOptions(12.f));
        g.drawText(txt, vr.toNearestInt(), juce::Justification::centred);
        if (!on) { g.setColour(col::dim); g.setFont(juce::FontOptions(10.f)); g.drawText("OFF", r.withTrimmedTop(42.f).withHeight(14.f).toNearestInt(), juce::Justification::centred); }
    }
}
void RouteView::mouseDown(const juce::MouseEvent& e)
{
    dragNode = valNode = -1; moving = false; down = e.position; cur = e.position;
    for (auto& b : boxes())
    {
        if (!b.r.contains(e.position)) continue;
        if (juce::Rectangle<float>(b.r.getX(), b.r.getY(), 26.f, 26.f).contains(e.position))
        { const juce::String id = onId(b.node); gesture(proc, id, true); setNorm(proc, id, proc.get(id) > .5f ? 0.f : 1.f); gesture(proc, id, false); repaint(); return; }
        if (juce::Rectangle<float>(b.r.getRight() - 28.f, b.r.getY(), 28.f, 26.f).contains(e.position))
        { openTab(b.node == wm::fx::kRev ? 0 : b.node == wm::fx::kDel ? 1 : 2); return; }
        if (e.position.y > b.r.getBottom() - 30.f) { valNode = b.node; valStart = getNorm(proc, valId(b.node)); gesture(proc, valId(b.node), true); return; }
        dragNode = b.node; return;
    }
}
void RouteView::mouseDrag(const juce::MouseEvent& e)
{
    cur = e.position;
    if (valNode >= 0) { setNorm(proc, valId(valNode), valStart + (down.y - e.position.y) / 160.f); repaint(); return; }
    if (dragNode >= 0 && (moving || e.position.getDistanceFrom(down) > 6.f)) { moving = true; repaint(); }
}
void RouteView::mouseUp(const juce::MouseEvent& e)
{
    if (valNode >= 0) { gesture(proc, valId(valNode), false); valNode = -1; return; }
    if (dragNode >= 0 && moving)
    {
        auto g = wm::fx::parseGraph(proc.getGraph().toStdString());
        // remove from its stage
        for (int s = 0; s < g.stages; ++s)
            for (int j = 0; j < g.count[s]; ++j)
                if (g.items[s][j] == dragNode) { for (int k = j; k + 1 < g.count[s]; ++k) g.items[s][k] = g.items[s][k + 1]; --g.count[s]; }
        for (int s = 0; s < g.stages; ) { if (g.count[s] == 0) { for (int k = s; k + 1 < g.stages; ++k) { g.count[k] = g.count[k + 1]; for (int j = 0; j < 4; ++j) g.items[k][j] = g.items[k + 1][j]; } --g.stages; } else ++s; }
        // decide: join a column (parallel) or insert between columns (series), by the x of the drop; columns are laid out for the new stage count
        const float w = (float) getWidth(); int placed = -1;
        for (int s = 0; s < g.stages; ++s)
        {
            const float cx = w * ((float) (s + 1) + 0.5f) / (float) (g.stages + 2 + 0);
            if (std::abs(e.position.x - cx) < 0.3f * w / (float) (g.stages + 2) && g.count[s] < 4) { g.items[s][g.count[s]++] = dragNode; placed = s; break; }
        }
        if (placed < 0)
        {
            int at = g.stages;
            for (int s = 0; s < g.stages; ++s) { const float cx = w * ((float) (s + 1) + 0.5f) / (float) (g.stages + 2); if (e.position.x < cx) { at = s; break; } }
            for (int k = g.stages; k > at; --k) { g.count[k] = g.count[k - 1]; for (int j = 0; j < 4; ++j) g.items[k][j] = g.items[k - 1][j]; }
            g.items[at][0] = dragNode; g.count[at] = 1; ++g.stages;
        }
        proc.setGraph(juce::String(wm::fx::graphString(g)));
    }
    moving = false; dragNode = -1; repaint();
}
void RouteView::mouseDoubleClick(const juce::MouseEvent& e)
{
    for (auto& b : boxes())
        if (b.r.contains(e.position) && e.position.y > b.r.getBottom() - 30.f)
            if (auto* q = proc.apvts.getParameter(valId(b.node))) { gesture(proc, valId(b.node), true); q->setValueNotifyingHost(q->getDefaultValue()); gesture(proc, valId(b.node), false); }
}

// ---------------------------------------------------------------- LevelMeter
void LevelMeter::paint(juce::Graphics& g)
{
    inV = juce::jmax(proc.inPk.load(), inV * 0.9f); outV = juce::jmax(proc.outPk.load(), outV * 0.9f);
    inHold = juce::jmax(inV, inHold - 0.004f); outHold = juce::jmax(outV, outHold - 0.004f);
    g.fillAll(col::panel);
    const auto r = getLocalBounds().toFloat().reduced(6.f, 22.f).withTrimmedBottom(14.f);
    g.setColour(col::dim); g.setFont(juce::FontOptions(11.f)); g.drawText("Level", getLocalBounds().withHeight(18), juce::Justification::centred);
    auto db2y = [&](float db) { return r.getBottom() - juce::jlimit(0.f, 1.f, (db + 60.f) / 60.f) * r.getHeight(); };
    for (int d = 0; d >= -60; d -= 12) { g.setColour(col::line); g.drawHorizontalLine((int) db2y((float) d), r.getX() + 24.f, r.getRight()); g.setColour(col::dim); g.setFont(juce::FontOptions(9.5f)); g.drawText(juce::String(d), (int) r.getX() - 4, (int) db2y((float) d) - 6, 26, 12, juce::Justification::right); }
    const float bw = (r.getWidth() - 30.f) / 2.f - 4.f;
    for (int c = 0; c < 2; ++c)
    {
        const float v = c == 0 ? inV : outV, hold = c == 0 ? inHold : outHold; const float x = r.getX() + 28.f + (float) c * (bw + 6.f);
        const float top = db2y(juce::Decibels::gainToDecibels(v, -60.f)); const juce::Rectangle<float> bar(x, top, bw, r.getBottom() - top);
        g.setColour(c == 0 ? col::mint : col::lilac); g.fillRect(bar);
        g.setColour(col::ink); g.fillRect(x, db2y(juce::Decibels::gainToDecibels(hold, -60.f)), bw, 2.f);
        g.setColour(col::dim); g.drawText(c == 0 ? "in" : "out", (int) x - 6, (int) r.getBottom() + 2, (int) bw + 12, 12, juce::Justification::centred);
    }
}

// ---------------------------------------------------------------- editor
ShimmerstuffEditor::ShimmerstuffEditor(ShimmerstuffProcessor& p)
    : AudioProcessorEditor(&p), proc(p), decay(p), echo(p), route(p, [this](int t) { setTab(t); }), shape(p), meter(p), border(this, &constrainer)
{
    setLookAndFeel(&laf);
    for (int i = 0; i < 3; ++i)
    {
        static const char* nm[3] = { "Shimmer Reverb", "Shimmer Delay", "FX and Routing" };
        tabBtn[i].setButtonText(nm[i]); tabBtn[i].setClickingTogglesState(false); tabBtn[i].onClick = [this, i] { setTab(i); };
        tabBtn[i].setTooltip(i == 0 ? "Reverb: plate, hall, shimmer plate, shimmer hall and velvet." : i == 1 ? "Delay: echoes that climb (or fall) in pitch with every repeat." : "Route the reverb and delay (one, the other, in series or side by side) and add saturation, squash, duck and width.");
        addAndMakeVisible(tabBtn[i]);
    }
    addAndMakeVisible(presets); presets.setTooltip("Main presets: the whole plugin (reverb, delay, FX and routing) in one go. Your own saved presets are listed after the factory ones.");
    presets.onChange = [this] { presetChosen(); };
    addAndMakeVisible(save); save.setTooltip("Save the whole plugin as your own main preset (kept on this computer)."); save.onClick = [this] { savePreset(); };
    addAndMakeVisible(bypass); bypass.setClickingTogglesState(true); bypass.setTooltip("Hear the unprocessed signal.");
    aByp = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "byp", bypass);
    for (auto* c : { (juce::Component*) &decay, (juce::Component*) &echo, (juce::Component*) &route, (juce::Component*) &meter, (juce::Component*) &shape }) addAndMakeVisible(*c);
    auto& a = p.apvts;
    rType.init(*this, a, "type", "Reverb type", ShimmerstuffProcessor::typeNames(), "Plate, hall, shimmer plate (tail pitch shifted back into the reverb), shimmer hall, or velvet (dense and smooth).");
    struct K { const char* id; const char* name; const char* suf; const char* tip; bool pct; };
    const K rks[13] = {
        { "decay", "Decay", "", "Tail length. Drag the yellow dot in the decay view sideways too.", true }, { "damp", "Damping", " Hz", "Darkens the tail. Drag the blue handle under the decay view too.", false },
        { "predelay", "Pre-delay", " ms", "Gap before the reverb starts. Drag the cyan handle too.", false }, { "size", "Size", "", "Room size. Drag the yellow dot up or down too.", false },
        { "mod", "Modulation", "", "Slow movement inside the tail, for a lusher sound.", true }, { "color", "Colour", "", "Velvet only: dark to bright tilt.", false },
        { "shift", "Shimmer pitch", " st", "Pitch of the shimmer voice fed back into the reverb (shimmer types).", false }, { "shimmer", "Shimmer", "", "How much shifted tail is fed back (shimmer types).", true },
        { "shift2", "Shimmer 2 pitch", " st", "Pitch of the second shimmer voice.", false }, { "shimmer2", "Shimmer 2", "", "Amount of the second shimmer voice. 0 is off.", true },
        { "hp", "Wet HP", " Hz", "High-pass on the reverb tail only. Keeps the tail out of the bass.", false }, { "lp", "Wet LP", " Hz", "Low-pass on the reverb tail only.", false },
        { "mix", "Mix", "", "Reverb dry/wet. Drag the pink handle under the decay view too.", true } };
    for (int i = 0; i < 13; ++i) rk[i].init(*this, a, rks[i].id, rks[i].name, rks[i].suf, rks[i].tip, rks[i].pct);
    const K dks[6] = {
        { "dtime", "Time", " ms", "Time between repeats. Drag the first bar in the echo view too.", false }, { "dfb", "Feedback", "", "How much of each repeat goes back in. Drag any bar up or down too.", true },
        { "dshift", "Shift", " st", "Pitch change added at every repeat. +12 climbs an octave each time.", false }, { "dshim", "Shimmer", "", "How much of each repeat is shifted. 0 is a plain delay.", true },
        { "ddamp", "Damping", " Hz", "Darkens every repeat a little more.", false }, { "dmix", "Mix", "", "Delay dry/wet.", true } };
    for (int i = 0; i < 6; ++i) dk[i].init(*this, a, dks[i].id, dks[i].name, dks[i].suf, dks[i].tip, dks[i].pct);
    const K iks[4] = { { "i_hp", "Input HP", " Hz", "Cuts lows before they reach the effects.", false }, { "i_lp", "Input LP", " Hz", "Cuts highs before they reach the effects.", false },
        { "i_drive", "Drive", " dB", "Saturation on the signal that feeds the effects. Antialiased.", false }, { "i_mix", "Sat mix", "", "Dry/wet of the input saturation. 0 is off.", true } };
    for (int i = 0; i < 4; ++i) ik[i].init(*this, a, iks[i].id, iks[i].name, iks[i].suf, iks[i].tip, iks[i].pct);
    const K wks[6] = { { "w_drive", "Drive", " dB", "Saturation on the wet signal only. Antialiased.", false }, { "w_smix", "Sat mix", "", "Dry/wet of the wet saturation. 0 is off.", true },
        { "w_depth", "Squash depth", "", "How hard the three bands are pushed: quiet up, loud down. Dense, bright tails (OTT style).", true }, { "w_qmix", "Squash mix", "", "Dry/wet of the squash. 0 is off.", true },
        { "w_width", "Width", "", "Stereo width of the wet signal. 100 % is unchanged, 0 is mono.", true }, { "w_boost", "Boost", " dB", "Extra level on the wet signal, to push the effect harder.", false } };
    for (int i = 0; i < 6; ++i) wk[i].init(*this, a, wks[i].id, wks[i].name, wks[i].suf, wks[i].tip, wks[i].pct);
    const K dus[3] = { { "w_duck", "Duck amount", "", "How deep the effect dips. 0 is off.", true }, { "w_drel", "Release", " ms", "Follow mode: how fast the effect comes back after the source stops.", false },
        { "w_drate", "Rate", " Hz", "Free mode: how often the shape repeats.", false } };
    for (int i = 0; i < 3; ++i) dukn[i].init(*this, a, dus[i].id, dus[i].name, dus[i].suf, dus[i].tip, dus[i].pct);
    dMode.init(*this, a, "w_dmode", "Duck timing", juce::StringArray { "Follow the source", "Free (Hz)", "BPM sync" }, "Follow: the effect dips while the source plays (classic tail duck). Free: a repeating shape in Hz. BPM sync: a repeating shape locked to the host tempo.");
    dSrc.init(*this, a, "w_dsrc", "Duck source", juce::StringArray { "None", "Plugin input", "Sidechain input" }, "Where the duck listens. None: nothing triggers it. Sidechain input: the plugin's sidechain (enable it in your DAW).");
    dRatio.init(*this, a, "w_dratio", "Sync ratio", ShimmerstuffProcessor::ratioNames(), "Length of one duck cycle at the host tempo (4/4). 1/4 is one beat.");
    shape.setTooltip("Duck shape: click one. Taller means more ducking at that moment of the cycle.");
    static const char* onIds[4] = { "ifx_on", "rev_on", "del_on", "wfx_on" }; static const char* onNm[4] = { "Input FX", "Reverb", "Delay", "Wet FX" };
    for (int i = 0; i < 4; ++i) { onBtn[i].setButtonText(onNm[i]); addAndMakeVisible(onBtn[i]); onBtn[i].setTooltip(juce::String("Switch ") + onNm[i] + " on or off in the routing."); aOn[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(a, onIds[i], onBtn[i]); }
    hint.setFont(juce::FontOptions(12.f)); hint.setColour(juce::Label::textColourId, col::dim); addAndMakeVisible(hint);
    refreshPresets();
    constrainer.setMinimumSize(baseW / 2, baseH / 2); constrainer.setMaximumSize(baseW * 2, baseH * 2);
    constrainer.setFixedAspectRatio((double) baseW / (double) baseH);
    setConstrainer(&constrainer); setResizable(true, false);
    border.setBorderThickness(juce::BorderSize<int>(6)); addAndMakeVisible(border);
    setSize(baseW, baseH);
    setTab(0);
    gate.onFrame = [this] { decay.repaint(); echo.repaint(); route.repaint(); meter.repaint(); shape.repaint(); };
    gate.start(30);
}

void ShimmerstuffEditor::setTab(int t)
{
    tab = t;
    for (int i = 0; i < 3; ++i) tabBtn[i].setColour(juce::TextButton::buttonColourId, i == t ? juce::Colour(0xff241238) : col::panel2);
    decay.setVisible(t == 0); echo.setVisible(t == 1); route.setVisible(t == 2);
    rType.show(t == 0); for (auto& k : rk) k.show(t == 0);
    for (auto& k : dk) k.show(t == 1);
    for (auto& k : ik) k.show(t == 2); for (auto& k : wk) k.show(t == 2); for (auto& k : dukn) k.show(t == 2);
    dMode.show(t == 2); dSrc.show(t == 2); dRatio.show(t == 2); shape.setVisible(t == 2);
    hint.setText(t == 0 ? "" : t == 1 ? (proc.get("del_on") > .5f ? "" : "The delay is switched off in the routing. Press Delay above (or the power corner in FX and Routing) to turn it on.") : "", juce::dontSendNotification);
    resized(); repaint();
}

juce::File ShimmerstuffEditor::presetDir() const
{ return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Studio 87").getChildFile("shimmerstuff").getChildFile("presets"); }
void ShimmerstuffEditor::refreshPresets()
{
    presets.clear(juce::dontSendNotification);
    for (int i = 0; i < proc.getNumPrograms(); ++i) presets.addItem(proc.getProgramName(i), i + 1);
    userFiles.clear(); auto d = presetDir();
    if (d.isDirectory()) for (auto& f : d.findChildFiles(juce::File::findFiles, false, "*.ssp")) userFiles.push_back(f);
    if (!userFiles.empty()) presets.addSeparator();
    for (size_t i = 0; i < userFiles.size(); ++i) presets.addItem(userFiles[i].getFileNameWithoutExtension() + " (mine)", 1000 + (int) i);
    presets.setSelectedId(proc.getCurrentProgram() + 1, juce::dontSendNotification);
}
void ShimmerstuffEditor::presetChosen()
{
    const int id = presets.getSelectedId();
    if (id >= 1000) { const size_t i = (size_t) (id - 1000); if (i < userFiles.size()) if (auto xml = juce::XmlDocument::parse(userFiles[i])) if (xml->hasTagName("STATE")) proc.applyUserPresetXml(*xml); }
    else if (id >= 1) proc.setCurrentProgram(id - 1);
    repaint();
}
void ShimmerstuffEditor::savePreset()
{
    auto* w = new juce::AlertWindow("Save main preset", "Name for your preset (the whole plugin: reverb, delay, FX and routing):", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor("name", "My preset"); w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey)); w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->enterModalState(true, juce::ModalCallbackFunction::create([this, w](int r)
    {
        if (r == 1)
        {
            auto name = juce::File::createLegalFileName(w->getTextEditorContents("name").trim());
            if (name.isNotEmpty())
            {
                auto d = presetDir(); d.createDirectory();
                std::unique_ptr<juce::XmlElement> xml(proc.makePresetXml());
                xml->writeTo(d.getChildFile(name + ".ssp"));
                refreshPresets();
            }
        }
        delete w;
    }), false);
}

void ShimmerstuffEditor::paint(juce::Graphics& g)
{
    g.fillAll(col::panel);
    catui::drawWoodCheeks(g, getLocalBounds(), cheek);
    catui::drawCatHead(g, { (float) cheek + 10.f, 8.f, 36.f, 36.f }, catui::SodaPop::lilac());
    g.setColour(col::lilac); g.setFont(juce::FontOptions(22.f, juce::Font::bold)); g.drawText("shimmerstuff", cheek + 54, 6, 240, 26, juce::Justification::left);
    g.setColour(col::dim); g.setFont(juce::FontOptions(12.f)); g.drawText("Studio 87 - shimmer reverb, shimmer delay, FX and routing", cheek + 54, 30, 420, 16, juce::Justification::left);
    g.setColour(col::panel2); g.fillRoundedRectangle(knobR.toFloat(), 6.f);
    g.setColour(col::dim); g.setFont(juce::FontOptions(11.f, juce::Font::bold));
    if (tab == 2)
    {
        const int w = knobR.getWidth();
        g.drawText("INPUT FX", knobR.getX() + 10, knobR.getY() + 4, 200, 14, juce::Justification::left);
        g.drawText("WET FX", knobR.getX() + (int) (w * 0.27f), knobR.getY() + 4, 200, 14, juce::Justification::left);
        g.drawText("DUCK", knobR.getX() + (int) (w * 0.66f), knobR.getY() + 4, 200, 14, juce::Justification::left);
    }
}
void ShimmerstuffEditor::mouseDoubleClick(const juce::MouseEvent& e)
{ if (e.x < cheek || e.x > getWidth() - cheek) setSize(baseW, baseH); }

void ShimmerstuffEditor::resized()
{
    border.setBounds(getLocalBounds());
    auto r = getLocalBounds().reduced(cheek + 8, 8);
    auto top = r.removeFromTop(48);
    bypass.setBounds(top.removeFromRight(80).withSizeKeepingCentre(76, 28)); top.removeFromRight(6);
    save.setBounds(top.removeFromRight(64).withSizeKeepingCentre(60, 28)); top.removeFromRight(6);
    presets.setBounds(top.removeFromRight(300).withSizeKeepingCentre(300, 28));
    auto tabs = r.removeFromTop(38);
    for (int i = 0; i < 3; ++i) tabBtn[i].setBounds(tabs.removeFromLeft(190).reduced(2));
    { auto tg = tabs.removeFromRight(4 * 96); for (int i = 0; i < 4; ++i) onBtn[i].setBounds(tg.removeFromLeft(96).withSizeKeepingCentre(94, 24)); }
    hint.setBounds(tabs.reduced(8, 0));
    r.removeFromTop(6);
    meterR = r.removeFromRight(84); meter.setBounds(meterR); r.removeFromRight(8);
    displayR = r.removeFromTop(juce::roundToInt(r.getHeight() * 0.50f));
    decay.setBounds(displayR); echo.setBounds(displayR); route.setBounds(displayR);
    r.removeFromTop(8); knobR = r;
    auto k = knobR.reduced(6, 6);
    if (tab == 0)
    {
        auto row1 = k.removeFromTop(k.getHeight() / 2); const int w = row1.getWidth() / 7;
        rType.place(row1.removeFromLeft(w).reduced(6, 10));
        for (int i = 0; i < 6; ++i) rk[i].place(row1.removeFromLeft(w).reduced(2));
        for (int i = 6; i < 13; ++i) rk[i].place(k.removeFromLeft(w).reduced(2));
    }
    else if (tab == 1)
    {
        const int w = k.getWidth() / 6; for (auto& d : dk) d.place(k.removeFromLeft(w).reduced(4, 4));
    }
    else
    {
        auto g1 = k.removeFromLeft((int) (k.getWidth() * 0.26f)), g2 = k.removeFromLeft((int) (k.getWidth() * 0.39f / 0.74f)), g3 = k;
        g1.removeFromTop(16); g2.removeFromTop(16); g3.removeFromTop(16);
        { const int w = g1.getWidth() / 2; auto a = g1.removeFromTop(g1.getHeight() / 2); for (int i = 0; i < 2; ++i) ik[i].place(a.removeFromLeft(w).reduced(2)); for (int i = 2; i < 4; ++i) ik[i].place(g1.removeFromLeft(w).reduced(2)); }
        { const int w = g2.getWidth() / 3; auto a = g2.removeFromTop(g2.getHeight() / 2); for (int i = 0; i < 3; ++i) wk[i].place(a.removeFromLeft(w).reduced(2)); for (int i = 3; i < 6; ++i) wk[i].place(g2.removeFromLeft(w).reduced(2)); }
        {
            auto a = g3.removeFromTop(48); const int w = a.getWidth() / 3; dMode.place(a.removeFromLeft(w).reduced(3, 0)); dSrc.place(a.removeFromLeft(w).reduced(3, 0)); dRatio.place(a.reduced(3, 0));
            shape.setBounds(g3.removeFromTop(48).reduced(3, 4));
            const int kw = g3.getWidth() / 3; for (int i = 0; i < 3; ++i) dukn[i].place(g3.removeFromLeft(kw).reduced(2));
        }
    }
}
