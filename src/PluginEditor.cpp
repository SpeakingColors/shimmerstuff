#include "PluginEditor.h"
#include <catui/CatLookAndFeel.h>
#include <complex>

using namespace ss;

// ---------------------------------------------------------------- helpers
juce::Font ss::font(float size, bool bold, float kern)
{
    auto o = juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain);
    if (kern > 0.f) o = o.withKerningFactor(kern);
    return juce::Font(o);
}
juce::Font ss::mono(float size) { return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain)); }

static float strW(const juce::Font& f, const juce::String& t)
{
    juce::GlyphArrangement ga; ga.addLineOfText(f, t, 0.f, 0.f);
    return ga.getBoundingBox(0, -1, true).getWidth();
}
static void setNorm(ShimmerstuffProcessor& p, const juce::String& id, float n)
{ if (auto* q = p.apvts.getParameter(id)) q->setValueNotifyingHost(juce::jlimit(0.f, 1.f, n)); }
static float getNorm(ShimmerstuffProcessor& p, const juce::String& id)
{ if (auto* q = p.apvts.getParameter(id)) return q->getValue(); return 0.f; }
static void gesture(ShimmerstuffProcessor& p, const juce::String& id, bool begin)
{ if (auto* q = p.apvts.getParameter(id)) { if (begin) q->beginChangeGesture(); else q->endChangeGesture(); } }
static void resetParam(ShimmerstuffProcessor& p, const juce::String& id)
{ if (auto* q = p.apvts.getParameter(id)) { q->beginChangeGesture(); q->setValueNotifyingHost(q->getDefaultValue()); q->endChangeGesture(); } }
static float hzNorm(ShimmerstuffProcessor& p, const juce::String& id, float hz)
{ if (auto* q = p.apvts.getParameter(id)) return q->convertTo0to1(hz); return 0.f; }

static juce::String fmtHz(double v)
{
    if (v >= 1000.0) { auto s = juce::String(v / 1000.0, 1); if (s.endsWith(".0")) s = s.dropLastCharacters(2); return s + " kHz"; }
    return juce::String(juce::roundToInt(v)) + " Hz";
}
static juce::String fmtVal(const juce::String& id, float v)
{
    if (id == "i_hp" || id == "i_lp" || id == "hp" || id == "lp" || id == "damp" || id == "ddamp") return fmtHz(v);
    if (id == "predelay" || id == "dtime" || id == "w_drel") return juce::String(juce::roundToInt(v)) + " ms";
    if (id == "w_boost" || id == "i_drive" || id == "w_drive") return juce::String(v, 1) + " dB";
    return juce::String(juce::roundToInt(v * 100.f)) + " %";
}

// ---------------------------------------------------------------- look and feel
ShimLook::ShimLook()
{
    setColour(juce::ComboBox::backgroundColourId, col::disp); setColour(juce::ComboBox::textColourId, col::ink);
    setColour(juce::ComboBox::outlineColourId, col::line); setColour(juce::ComboBox::arrowColourId, col::lilac);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff0e0f13)); setColour(juce::PopupMenu::textColourId, col::ink);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, col::sel); setColour(juce::PopupMenu::highlightedTextColourId, col::lilac);
    setColour(juce::TextButton::buttonColourId, col::panel2); setColour(juce::TextButton::buttonOnColourId, col::sel);
    setColour(juce::TextButton::textColourOffId, col::ink); setColour(juce::TextButton::textColourOnId, col::lilac);
    setColour(juce::Label::textColourId, col::ink);
    setColour(juce::TextEditor::backgroundColourId, col::disp); setColour(juce::TextEditor::textColourId, col::ink);
    setColour(juce::TextEditor::outlineColourId, col::line); setColour(juce::TextEditor::focusedOutlineColourId, col::lilac);
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff0a0a0c)); setColour(juce::TooltipWindow::textColourId, col::ink);
    setColour(juce::TooltipWindow::outlineColourId, juce::Colour(0xff4a4d58));
    setColour(juce::AlertWindow::backgroundColourId, col::panel); setColour(juce::AlertWindow::textColourId, col::ink); setColour(juce::AlertWindow::outlineColourId, col::line);
}
void ShimLook::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto area = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(4.f);
    const float d = juce::jmin(area.getWidth(), area.getHeight()), r = d * 0.5f;
    const auto c = area.getCentre(); const auto accent = s.findColour(juce::Slider::rotarySliderFillColourId);
    const float arcR = r + 2.f;
    juce::Path trk; trk.addCentredArc(c.x, c.y, arcR, arcR, 0.f, start, end, true);
    g.setColour(juce::Colour(0xff33353d)); g.strokePath(trk, juce::PathStrokeType(3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float ang = start + pos * (end - start);
    const bool bi = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float a0 = bi ? start + (float) s.valueToProportionOfLength(0.0) * (end - start) : start;
    if (std::abs(ang - a0) > 0.01f)
    {
        juce::Path v; v.addCentredArc(c.x, c.y, arcR, arcR, 0.f, juce::jmin(a0, ang), juce::jmax(a0, ang), true);
        g.setColour(accent); g.strokePath(v, juce::PathStrokeType(3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff33363f), c.x - r * 0.3f, c.y - r * 0.4f, juce::Colour(0xff1e2026), c.x + r * 0.6f, c.y + r * 0.8f, true));
    g.fillEllipse(c.x - r, c.y - r, d, d);
    g.setColour(juce::Colour(0xff3a3d48)); g.drawEllipse(c.x - r, c.y - r, d, d, 1.f);
    const float p0 = r * 0.2f, p1 = r * 0.85f;
    g.setColour(col::ink);
    g.drawLine(c.x + std::sin(ang) * p0, c.y - std::cos(ang) * p0, c.x + std::sin(ang) * p1, c.y - std::cos(ang) * p1, 2.f);
}
void ShimLook::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& b)
{
    const auto r = juce::Rectangle<float>(0.f, 0.f, (float) w, (float) h);
    g.setColour(col::disp); g.fillRoundedRectangle(r, 3.f);
    g.setColour(b.hasKeyboardFocus(true) ? col::lilac : col::line); g.drawRoundedRectangle(r.reduced(0.5f), 3.f, 1.f);
    juce::Path a; const float cx = (float) w - 14.f, cy = (float) h * 0.5f;
    a.addTriangle(cx - 4.f, cy - 2.f, cx + 4.f, cy - 2.f, cx, cy + 3.f);
    g.setColour(col::cyan); g.fillPath(a);
}
void ShimLook::positionComboBoxText(juce::ComboBox& b, juce::Label& l)
{ l.setBounds(8, 1, b.getWidth() - 30, b.getHeight() - 2); l.setFont(getComboBoxFont(b)); }
void ShimLook::drawPopupMenuBackground(juce::Graphics& g, int w, int h)
{ g.fillAll(juce::Colour(0xff0e0f13)); g.setColour(col::lilac.withAlpha(0.6f)); g.drawRect(0, 0, w, h, 1); }
void ShimLook::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hi, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = b.getToggleState();
    juce::Colour fill = on ? b.findColour(juce::TextButton::buttonOnColourId) : col::panel2;
    if (hi) fill = fill.brighter(0.08f);
    if (down) fill = fill.brighter(0.15f);
    g.setColour(fill); g.fillRoundedRectangle(r, 3.f);
    g.setColour(on ? b.findColour(juce::TextButton::textColourOnId) : hi ? juce::Colour(0xff4a4d58) : col::line);
    g.drawRoundedRectangle(r, 3.f, 1.f);
}
void TabButton::paintButton(juce::Graphics& g, bool hi, bool)
{
    const auto r = getLocalBounds().toFloat().reduced(0.5f); const bool on = getToggleState();
    g.setColour(on ? col::sel : hi ? col::panel2 : col::panel); g.fillRoundedRectangle(r, 3.f);
    g.setColour(on ? col::lilac : col::line); g.drawRoundedRectangle(r, 3.f, on ? 1.5f : 1.f);
    g.setColour(on ? juce::Colour(0xffE1CBF4) : col::ink); g.setFont(font(20.f, true, 0.16f));
    g.drawText(title, getLocalBounds().withTrimmedTop(7).withHeight(26), juce::Justification::centred);
    g.setColour(on ? juce::Colour(0xffE1CBF4) : col::dim); g.setFont(font(10.5f, false, 0.14f));
    g.drawText(subtitle, getLocalBounds().withTrimmedTop(33).withHeight(16), juce::Justification::centred);
}

// ---------------------------------------------------------------- Knob and Combo
Knob::Knob()
{
    addAndMakeVisible(s); addAndMakeVisible(name); addAndMakeVisible(val);
    s.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    s.setMouseDragSensitivity(180);
    name.setJustificationType(juce::Justification::centred); name.setFont(font(10.5f, false, 0.12f)); name.setColour(juce::Label::textColourId, col::dim);
    name.setInterceptsMouseClicks(false, false);
    val.setJustificationType(juce::Justification::centred); val.setFont(mono(12.5f)); val.setEditable(false, true, false);
}
void Knob::init(juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& nm, Kind k, juce::Colour accent, const juce::String& tip)
{
    name.setText(nm.toUpperCase(), juce::dontSendNotification);
    s.setColour(juce::Slider::rotarySliderFillColourId, accent); val.setColour(juce::Label::textColourId, accent);
    s.setTooltip(tip + " Double-click the knob to reset it, double-click the number to type a value.");
    parent.addAndMakeVisible(*this);
    a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, id, s);
    if (auto* par = apvts.getParameter(id)) s.setDoubleClickReturnValue(true, par->convertFrom0to1(par->getDefaultValue()));
    s.textFromValueFunction = [k](double v) -> juce::String
    {
        switch (k)
        {
            case Pct: return juce::String(juce::roundToInt(v * 100.0)) + " %";
            case Size: return "x" + juce::String(v, 2);
            case Ms: return juce::String(juce::roundToInt(v)) + " ms";
            case Hz: return fmtHz(v);
            case St: return (v > 0.4 ? "+" : "") + juce::String(juce::roundToInt(v)) + " st";
            case Db: return juce::String(v, 1) + " dB";
            default: { const int p = juce::roundToInt(v * 100.0); return (p > 0 ? "+" : "") + juce::String(p); }
        }
    };
    s.valueFromTextFunction = [k](const juce::String& t) -> double
    {
        const double n = t.retainCharacters("0123456789.-").getDoubleValue();
        switch (k) { case Pct: case Plain: return n / 100.0; case Hz: return t.containsIgnoreCase("k") ? n * 1000.0 : n; default: return n; }
    };
    s.onValueChange = [this] { val.setText(s.getTextFromValue(s.getValue()), juce::dontSendNotification); };
    val.onTextChange = [this] { s.setValue(s.getValueFromText(val.getText()), juce::sendNotificationSync); val.setText(s.getTextFromValue(s.getValue()), juce::dontSendNotification); };
    s.onValueChange();
}
void Knob::resized()
{
    const int d = juce::jmin(getWidth(), getHeight() - 30, 54);
    s.setBounds((getWidth() - d) / 2, 0, d, d);
    name.setBounds(0, d + 1, getWidth(), 14); val.setBounds(0, d + 15, getWidth(), 15);
}
void Combo::init(juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& nm, const juce::StringArray& items, const juce::String& tip)
{
    addAndMakeVisible(c); addAndMakeVisible(l); c.addItemList(items, 1); c.setTooltip(tip);
    l.setText(nm.toUpperCase(), juce::dontSendNotification); l.setFont(font(10.5f, false, 0.14f)); l.setColour(juce::Label::textColourId, col::dim);
    parent.addAndMakeVisible(*this);
    a = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, id, c);
}

// ---------------------------------------------------------------- DecayView
float DecayView::signature() const
{
    float s = 0; int i = 1;
    for (const char* id : { "type", "decay", "damp", "predelay", "size", "mod", "color", "shift", "shimmer", "shift2", "shimmer2", "hp", "lp" })
        s += proc.get(id) * (float) (i++) * 1.37f;
    return s;
}
void DecayView::tick()
{
    if (!isShowing()) return;
    const float sig = signature();
    if (sig != lastSig) { lastSig = sig; stable = 0; dirty = true; }
    else if (dirty && ++stable >= 2) { measure(); dirty = false; }
    repaint();
}
void DecayView::measure()
{
    // the real reverb at a 16 kHz rate: a full wet impulse response, then Schroeder backward integration (energy decay curve)
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
float DecayView::rt60Seconds() const
{
    for (size_t k = 0; k < edc.size(); ++k) if (edc[k] <= -60.f) return (float) k / (float) (kPts - 1) * kSeconds;
    return kSeconds;
}
juce::Rectangle<float> DecayView::plot() const { return { 52.f, 16.f, (float) getWidth() - 52.f - 52.f, (float) getHeight() - 16.f - 30.f }; }
float DecayView::curveY(float frac) const
{
    const auto p = plot(); float db = 0.f;
    if (edc.size() == (size_t) kPts) { const float f = juce::jlimit(0.f, 1.f, frac) * (float) (kPts - 1); const int i = (int) f; const float t = f - (float) i; db = edc[(size_t) i] * (1.f - t) + edc[(size_t) juce::jmin(kPts - 1, i + 1)] * t; }
    return p.getY() + juce::jlimit(0.f, 70.f, -db) / 70.f * p.getHeight();
}
juce::Point<float> DecayView::handle(int i) const
{
    const auto p = plot(); auto& pr = const_cast<ShimmerstuffProcessor&>(proc);
    switch (i)
    {
        case 0: return { p.getX() + rt60Seconds() / kSeconds * p.getWidth(), p.getY() + (1.f - getNorm(pr, "size")) * p.getHeight() };
        case 1: return { p.getX() + proc.get("predelay") / (kSeconds * 1000.f) * p.getWidth(), p.getY() + 8.f };
        case 2: { const float f = 0.04f + 0.16f * getNorm(pr, "damp"); return { p.getX() + f * p.getWidth(), curveY(f) }; }
        case 3: { const float f = 0.2f + 0.4f * proc.get("shimmer"); return { p.getX() + f * p.getWidth(), curveY(f) }; }
        default: return { p.getRight() + 26.f, p.getBottom() - proc.get("mix") * p.getHeight() };
    }
}
int DecayView::hit(juce::Point<float> pt) const
{
    int best = -1; float bd = 16.f;
    for (int i = 4; i >= 0; --i) { if (i == 3 && !shimmerOn()) continue; const float d = pt.getDistanceFrom(handle(i)); if (d < bd) { bd = d; best = i; } }
    return best;
}
void DecayView::paint(juce::Graphics& g)
{
    g.fillAll(col::disp);
    g.setColour(col::line); g.drawRect(getLocalBounds(), 1);
    const auto p = plot();
    g.setFont(mono(10.5f));
    for (int d = 0; d <= 70; d += 10)
    {
        const float y = p.getY() + (float) d / 70.f * p.getHeight();
        g.setColour(juce::Colour(0xff1b2523)); g.drawHorizontalLine((int) y, p.getX(), p.getRight());
        g.setColour(col::lilac.withAlpha(0.6f)); g.drawText(juce::String(-d) + " dB", 2, (int) y - 6, 46, 12, juce::Justification::right);
    }
    for (int t = 0; t <= (int) kSeconds; t += 2)
    {
        const float x = p.getX() + (float) t / kSeconds * p.getWidth();
        g.setColour(juce::Colour(0xff1b2523)); g.drawVerticalLine((int) x, p.getY(), p.getBottom());
        g.setColour(col::lilac.withAlpha(0.6f)); g.drawText(juce::String(t) + " s", (int) x - 20, (int) p.getBottom() + 4, 40, 12, juce::Justification::centred);
    }
    if (edc.size() == (size_t) kPts)
    {
        juce::Path path;
        for (int k = 0; k < kPts; ++k)
        {
            const float f = (float) k / (float) (kPts - 1); const float x = p.getX() + f * p.getWidth(), y = curveY(f);
            k ? path.lineTo(x, y) : path.startNewSubPath(x, y);
        }
        juce::Path fill(path); fill.lineTo(p.getRight(), p.getBottom()); fill.lineTo(p.getX(), p.getBottom()); fill.closeSubPath();
        g.setGradientFill(juce::ColourGradient(col::purple.withAlpha(0.35f), 0.f, p.getY(), col::purple.withAlpha(0.03f), 0.f, p.getBottom(), false)); g.fillPath(fill);
        g.setColour(col::lilac); g.strokePath(path, juce::PathStrokeType(2.f));
    }
    g.setColour(col::yellow); g.setFont(mono(15.f));
    g.drawText("RT60 " + juce::String(rt60Seconds(), 1) + " s" + (shimmerOn() ? "  (with shimmer)" : "") + "   size x" + juce::String(proc.get("size"), 2), (int) p.getRight() - 460, (int) p.getY() + 4, 452, 18, juce::Justification::right);
    if (dirty) { g.setColour(col::dim); g.setFont(mono(11.f)); g.drawText("updating...", (int) p.getRight() - 200, (int) p.getY() + 24, 192, 14, juce::Justification::right); }
    g.setColour(col::dim); g.setFont(font(12.f, false, 0.03f));
    g.drawText("Yellow: decay (sideways) and size (up or down).  Cyan: pre-delay.  Blue: damping.  Orange: shimmer.  Pink: mix.  Double-click a dot to reset it.", (int) p.getX() + 6, (int) p.getBottom() - 20, (int) p.getWidth() - 12, 16, juce::Justification::left);
    const float mx = p.getRight() + 26.f;
    g.setColour(col::pink); g.setFont(font(10.5f, true, 0.12f)); g.drawText("MIX", (int) mx - 20, 0, 40, 14, juce::Justification::centred);
    g.setColour(col::pink.withAlpha(0.5f)); g.fillRoundedRectangle(mx - 2.f, p.getY() + 2.f, 4.f, p.getHeight() - 2.f, 2.f);
    const juce::Colour cc[5] = { col::yellow, col::cyan, col::blue, col::orange, col::pink };
    for (int i = 0; i < 5; ++i)
    {
        auto h = handle(i); const bool act = (drag == i);
        if (act && i == 0) h = startHandle + (curPos - startPos);
        const float a = (i == 3 && !shimmerOn()) ? 0.3f : 1.f;
        g.setColour(cc[i].withAlpha(a));
        if (i == 1) g.fillRect(h.x - 6.f, h.y - 6.f, 12.f, 12.f);
        else g.fillEllipse(h.x - (i == 0 ? 8.f : 6.5f), h.y - (i == 0 ? 8.f : 6.5f), i == 0 ? 16.f : 13.f, i == 0 ? 16.f : 13.f);
        if (act) { g.setColour(juce::Colours::white.withAlpha(0.8f)); g.drawEllipse(h.x - 10.f, h.y - 10.f, 20.f, 20.f, 1.2f); }
    }
}
void DecayView::mouseDown(const juce::MouseEvent& e)
{
    drag = hit(e.position); if (drag < 0) return;
    startPos = curPos = e.position; startHandle = handle(drag);
    if (drag == 0) { startA = getNorm(proc, "decay"); startB = getNorm(proc, "size"); gesture(proc, "decay", true); gesture(proc, "size", true); }
    else { static const char* ids[5] = { "", "predelay", "damp", "shimmer", "mix" }; gesture(proc, ids[drag], true); mouseDrag(e); }
}
void DecayView::mouseDrag(const juce::MouseEvent& e)
{
    if (drag < 0) return; const auto p = plot(); curPos = e.position;
    const float shiftK = e.mods.isShiftDown() ? 0.25f : 1.f;
    if (drag == 0) { setNorm(proc, "decay", startA + (e.position.x - startPos.x) / p.getWidth() * 1.2f * shiftK); setNorm(proc, "size", startB - (e.position.y - startPos.y) / p.getHeight() * shiftK); }
    else if (drag == 1) setNorm(proc, "predelay", hzNorm(proc, "predelay", juce::jlimit(0.f, 250.f, (e.position.x - p.getX()) / p.getWidth() * kSeconds * 1000.f)));
    else if (drag == 2) setNorm(proc, "damp", ((e.position.x - p.getX()) / p.getWidth() - 0.04f) / 0.16f);
    else if (drag == 3) setNorm(proc, "shimmer", ((e.position.x - p.getX()) / p.getWidth() - 0.2f) / 0.4f);
    else if (drag == 4) setNorm(proc, "mix", (p.getBottom() - e.position.y) / p.getHeight());
}
void DecayView::mouseUp(const juce::MouseEvent&)
{
    if (drag == 0) { gesture(proc, "decay", false); gesture(proc, "size", false); }
    else if (drag > 0) { static const char* ids[5] = { "", "predelay", "damp", "shimmer", "mix" }; gesture(proc, ids[drag], false); }
    drag = -1;
}
void DecayView::mouseDoubleClick(const juce::MouseEvent& e)
{
    const int i = hit(e.position); static const char* ids[5] = { "decay", "predelay", "damp", "shimmer", "mix" };
    if (i < 0) return;
    resetParam(proc, ids[i]); if (i == 0) resetParam(proc, "size");
}

// ---------------------------------------------------------------- ToneView
namespace
{
void fft(std::vector<std::complex<float>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const float ang = -2.f * juce::MathConstants<float>::pi / (float) len; const std::complex<float> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) { std::complex<float> w(1.f, 0.f); for (size_t j = 0; j < len / 2; ++j) { const auto u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; } }
    }
}
}
ToneView::ToneView(ShimmerstuffProcessor& p, int mode) : proc(p)
{
    if (mode == 0) lines = { { "hp", "HP", col::blue }, { "damp", "Damp", col::yellow }, { "lp", "LP", col::blue } };
    else if (mode == 1) lines = { { "ddamp", "Damp", col::yellow } };
    else lines = { { "i_hp", "In HP", col::blue }, { "i_lp", "In LP", col::blue } };
    magIn.assign(N / 2, -120.f); magOut.assign(N / 2, -120.f);
}
juce::Rectangle<float> ToneView::plot() const { return { 52.f, 16.f, (float) getWidth() - 52.f - 20.f, (float) getHeight() - 16.f - 28.f }; }
float ToneView::fx(float hz) const { const auto p = plot(); return p.getX() + std::log(juce::jmax(20.f, hz) / 20.f) / std::log(1000.f) * p.getWidth(); }
float ToneView::hzAt(float x) const { const auto p = plot(); return 20.f * std::pow(1000.f, juce::jlimit(0.f, 1.f, (x - p.getX()) / p.getWidth())); }
void ToneView::spectrum(bool input, std::vector<float>& out)
{
    const auto& tap = input ? proc.tapIn : proc.tapOut; const int pos = proc.tapPos.load(std::memory_order_acquire);
    std::vector<std::complex<float>> buf((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        const int idx = (pos - N + i + 4 * ShimmerstuffProcessor::kTap) % ShimmerstuffProcessor::kTap;
        const float w = 0.5f - 0.5f * std::cos(2.f * juce::MathConstants<float>::pi * (float) i / (float) (N - 1));
        buf[(size_t) i] = { tap[(size_t) idx].load(std::memory_order_relaxed) * w, 0.f };
    }
    fft(buf);
    for (int k = 1; k < N / 2; ++k)
    {
        const float db = juce::Decibels::gainToDecibels(std::abs(buf[(size_t) k]) / ((float) N / 4.f), -120.f);
        out[(size_t) k] = out[(size_t) k] * 0.55f + db * 0.45f;
    }
}
void ToneView::tick()
{
    if (!isShowing()) return;
    any = proc.inPk.load() > 1e-5f || proc.outPk.load() > 1e-5f;
    spectrum(true, magIn); spectrum(false, magOut); repaint();
}
void ToneView::paint(juce::Graphics& g)
{
    g.fillAll(col::disp); g.setColour(col::line); g.drawRect(getLocalBounds(), 1);
    const auto p = plot(); g.setFont(mono(10.5f));
    for (int d = 0; d <= 90; d += 15)
    {
        const float y = p.getY() + (float) d / 90.f * p.getHeight();
        g.setColour(juce::Colour(0xff1b2523)); g.drawHorizontalLine((int) y, p.getX(), p.getRight());
        g.setColour(col::lilac.withAlpha(0.6f)); g.drawText(juce::String(-d) + " dB", 2, (int) y - 6, 46, 12, juce::Justification::right);
    }
    for (float hz : { 20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f })
    {
        const float x = fx(hz); g.setColour(juce::Colour(0xff1b2523)); g.drawVerticalLine((int) x, p.getY(), p.getBottom());
        g.setColour(col::lilac.withAlpha(0.6f)); g.drawText(hz >= 1000.f ? juce::String((int) (hz / 1000.f)) + "k" : juce::String((int) hz), (int) x - 20, (int) p.getBottom() + 4, 40, 12, juce::Justification::centred);
    }
    auto draw = [&](const std::vector<float>& m, juce::Colour c, bool fill)
    {
        juce::Path path; bool first = true;
        for (int k = 1; k < N / 2; ++k)
        {
            const float hz = (float) k * (float) proc.getSampleRate() / (float) N; if (hz < 20.f || hz > 20000.f) continue;
            const float x = fx(hz), y = p.getY() + juce::jlimit(0.f, 90.f, -m[(size_t) k]) / 90.f * p.getHeight();
            if (first) { path.startNewSubPath(x, y); first = false; } else path.lineTo(x, y);
        }
        if (first) return;
        if (fill) { juce::Path f(path); f.lineTo(p.getRight(), p.getBottom()); f.lineTo(p.getX(), p.getBottom()); f.closeSubPath(); g.setColour(c.withAlpha(0.18f)); g.fillPath(f); }
        g.setColour(c); g.strokePath(path, juce::PathStrokeType(fill ? 1.6f : 1.f));
    };
    draw(magIn, col::dim.withAlpha(0.7f), false); draw(magOut, col::mint, true);
    g.setColour(col::dim); g.setFont(font(12.f, false, 0.03f));
    g.drawText(any ? "Grey: plugin input.  Green: plugin output.  Drag a line sideways to change its filter; double-click resets it." : "Play audio through the plugin to see its spectrum.  Drag a line sideways to change its filter; double-click resets it.",
               (int) p.getX() + 6, (int) p.getBottom() - 20, (int) p.getWidth() - 12, 16, juce::Justification::left);
    for (auto& l : lines)
    {
        const float hz = proc.get(l.id), x = fx(hz);
        g.setColour(l.c.withAlpha(0.7f)); const float dash[2] = { 5.f, 4.f }; g.drawDashedLine(juce::Line<float>(x, p.getY(), x, p.getBottom()), dash, 2, 1.2f);
        g.setColour(l.c); g.fillEllipse(x - 6.5f, p.getY() + 4.f, 13.f, 13.f);
        g.setFont(mono(12.f)); const bool left = x > p.getRight() - 130.f;
        g.drawText(juce::String(l.name) + " " + fmtHz(hz), (int) (left ? x - 134.f : x + 10.f), (int) p.getY() + 3, 124, 14, left ? juce::Justification::right : juce::Justification::left);
    }
}
void ToneView::mouseDown(const juce::MouseEvent& e)
{
    drag = -1; float bd = 14.f;
    for (size_t i = 0; i < lines.size(); ++i) { const float d = std::abs(e.position.x - fx(proc.get(lines[i].id))); if (d < bd) { bd = d; drag = (int) i; } }
    if (drag >= 0) { gesture(proc, lines[(size_t) drag].id, true); mouseDrag(e); }
}
void ToneView::mouseDrag(const juce::MouseEvent& e) { if (drag >= 0) setNorm(proc, lines[(size_t) drag].id, hzNorm(proc, lines[(size_t) drag].id, hzAt(e.position.x))); }
void ToneView::mouseUp(const juce::MouseEvent&) { if (drag >= 0) gesture(proc, lines[(size_t) drag].id, false); drag = -1; }
void ToneView::mouseDoubleClick(const juce::MouseEvent& e)
{
    for (auto& l : lines) if (std::abs(e.position.x - fx(proc.get(l.id))) < 14.f) { resetParam(proc, l.id); return; }
}

// ---------------------------------------------------------------- EchoView
juce::Rectangle<float> EchoView::plot() const { return { 52.f, 30.f, (float) getWidth() - 52.f - 20.f, (float) getHeight() - 30.f - 30.f }; }
void EchoView::paint(juce::Graphics& g)
{
    g.fillAll(col::disp); g.setColour(col::line); g.drawRect(getLocalBounds(), 1);
    const auto p = plot(); const float fb = proc.get("dfb"), t = proc.get("dtime"), shim = proc.get("dshim"), semis = proc.get("dshift") * shim, total = 8000.f;
    g.setFont(mono(10.5f));
    for (int d = 0; d <= 60; d += 10)
    {
        const float y = p.getY() + (float) d / 60.f * p.getHeight();
        g.setColour(juce::Colour(0xff1b2523)); g.drawHorizontalLine((int) y, p.getX(), p.getRight());
        g.setColour(col::lilac.withAlpha(0.6f)); g.drawText(juce::String(-d) + " dB", 2, (int) y - 6, 46, 12, juce::Justification::right);
    }
    for (int s = 0; s <= 8; ++s)
    {
        const float x = p.getX() + (float) s / 8.f * p.getWidth(); g.setColour(juce::Colour(0xff1b2523)); g.drawVerticalLine((int) x, p.getY(), p.getBottom());
        g.setColour(col::lilac.withAlpha(0.6f)); g.drawText(juce::String(s) + " s", (int) x - 20, (int) p.getBottom() + 4, 40, 12, juce::Justification::centred);
    }
    g.setColour(col::dim); g.setFont(font(12.f, false, 0.03f));
    g.drawText("Yellow numbers: pitch of each repeat in semitones. Level is energy compared with the input.", (int) p.getX(), 6, (int) p.getWidth(), 16, juce::Justification::right);
    g.drawText("Drag the first dot sideways for Time, the second up or down for Feedback.", (int) p.getX() + 6, (int) p.getBottom() - 20, (int) p.getWidth() - 12, 16, juce::Justification::left);
    auto yOf = [&](float amp) { return p.getY() + juce::jlimit(0.f, 60.f, -juce::Decibels::gainToDecibels(amp, -60.f)) / 60.f * p.getHeight(); };
    float amp = 1.f; juce::Point<float> d1, d2;
    for (int k = 1; (float) k * t < total && k < 80; ++k)
    {
        const float x = p.getX() + (float) k * t / total * p.getWidth(), y = yOf(amp);
        if (k == 1) d1 = { x, y };
        if (k == 2) d2 = { x, y };
        if (amp < 0.001f && k > 2) break;
        g.setGradientFill(juce::ColourGradient(col::mint.withAlpha(0.95f), 0.f, y, col::mint.withAlpha(0.15f), 0.f, p.getBottom(), false));
        g.fillRect(x - 3.f, y, 6.f, p.getBottom() - y); g.setColour(col::yellow); g.fillRect(x - 3.f, y, 6.f, 2.f);
        g.setFont(mono(11.f)); g.drawText((semis * (float) k > 0.4f ? "+" : "") + juce::String(juce::roundToInt(semis * (float) k)), (int) x - 24, (int) y - 16, 48, 12, juce::Justification::centred);
        amp *= fb;
    }
    if (d2 == juce::Point<float>()) d2 = { p.getX() + 2.f * t / total * p.getWidth(), yOf(fb) };
    g.setColour(col::yellow); g.fillEllipse(d1.x - 6.f, d1.y - 6.f, 12.f, 12.f); g.fillEllipse(d2.x - 6.f, d2.y - 6.f, 12.f, 12.f);
    g.setFont(mono(12.5f)); g.setColour(col::ink);
    g.drawText("Time " + juce::String(juce::roundToInt(t)) + " ms", (int) d1.x + 12, (int) d1.y - 8, 120, 16, juce::Justification::left);
    g.drawText("Feedback " + juce::String(juce::roundToInt(fb * 100.f)) + " %", (int) d2.x + 12, (int) d2.y - 8, 140, 16, juce::Justification::left);
}
void EchoView::mouseDown(const juce::MouseEvent& e)
{
    const auto p = plot(); const float t = proc.get("dtime"), fb = proc.get("dfb");
    const float x1 = p.getX() + t / 8000.f * p.getWidth(), x2 = p.getX() + 2.f * t / 8000.f * p.getWidth();
    const float y2 = p.getY() + juce::jlimit(0.f, 60.f, -juce::Decibels::gainToDecibels(fb, -60.f)) / 60.f * p.getHeight();
    drag = std::abs(e.position.x - x2) < 16.f && std::abs(e.position.y - y2) < 18.f ? 1 : std::abs(e.position.x - x1) < 16.f ? 0 : -1;
    if (drag >= 0) { gesture(proc, drag == 0 ? "dtime" : "dfb", true); mouseDrag(e); }
}
void EchoView::mouseDrag(const juce::MouseEvent& e)
{
    const auto p = plot();
    if (drag == 0) setNorm(proc, "dtime", hzNorm(proc, "dtime", juce::jlimit(20.f, 2000.f, (e.position.x - p.getX()) / p.getWidth() * 8000.f)));
    else if (drag == 1) { const float db = -juce::jlimit(0.f, 60.f, (e.position.y - p.getY()) / p.getHeight() * 60.f); setNorm(proc, "dfb", hzNorm(proc, "dfb", juce::jlimit(0.f, 0.95f, juce::Decibels::decibelsToGain(db)))); }
}
void EchoView::mouseUp(const juce::MouseEvent&) { if (drag >= 0) gesture(proc, drag == 0 ? "dtime" : "dfb", false); drag = -1; }
void EchoView::mouseDoubleClick(const juce::MouseEvent& e)
{
    const auto p = plot(); const float x1 = p.getX() + proc.get("dtime") / 8000.f * p.getWidth();
    resetParam(proc, std::abs(e.position.x - x1) < 16.f ? "dtime" : "dfb");
}

// ---------------------------------------------------------------- ShapeView
void ShapeView::paint(juce::Graphics& g)
{
    const int sel = (int) proc.get("w_dshape"), n = wm::fx::kNumShapes; const float cw = (float) getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        const auto cell = juce::Rectangle<float>(cw * (float) i + 2.f, 2.f, cw - 4.f, (float) getHeight() - 4.f);
        g.setColour(i == sel ? col::sel : col::panel2); g.fillRoundedRectangle(cell, 4.f);
        g.setColour(i == sel ? col::lilac : col::line); g.drawRoundedRectangle(cell, 4.f, 1.f);
        juce::Path pa; const auto in = cell.reduced(5.f, 4.f).withTrimmedBottom(11.f);
        for (int k = 0; k <= 30; ++k)
        {
            const float v = 1.f - wm::fx::shapeAt(i, (float) k / 30.f), x = in.getX() + (float) k / 30.f * in.getWidth(), y = in.getBottom() - v * in.getHeight();
            k ? pa.lineTo(x, y) : pa.startNewSubPath(x, y);
        }
        g.setColour(col::yellow); g.strokePath(pa, juce::PathStrokeType(1.5f));
        g.setColour(col::dim); g.setFont(font(9.5f)); g.drawText(wm::fx::shapeName(i), cell.withTrimmedTop(cell.getHeight() - 12.f).toNearestInt(), juce::Justification::centred);
    }
}
void ShapeView::mouseDown(const juce::MouseEvent& e)
{
    const int i = juce::jlimit(0, wm::fx::kNumShapes - 1, (int) (e.position.x / ((float) getWidth() / (float) wm::fx::kNumShapes)));
    gesture(proc, "w_dshape", true); proc.setParam("w_dshape", (float) i); gesture(proc, "w_dshape", false); repaint();
}

// ---------------------------------------------------------------- RouteView
const char* RouteView::nodeName(int n) { static const char* s[4] = { "INPUT FX", "REVERB", "DELAY", "WET FX" }; return s[n & 3]; }
const char* RouteView::nodeSub(int n) { static const char* s[4] = { "filter + saturation", "", "", "sat, squash, duck, width" }; return s[n & 3]; }
const char* RouteView::onId(int n) { static const char* s[4] = { "ifx_on", "rev_on", "del_on", "wfx_on" }; return s[n & 3]; }
const char* RouteView::rowId(int n, int i)
{
    static const char* s[4][3] = { { "i_hp", "i_lp", "i_mix" }, { "decay", "predelay", "mix" }, { "dtime", "dfb", "dmix" }, { "w_boost", "w_duck", "w_width" } };
    return s[n & 3][i % 3];
}
const char* RouteView::rowName(int n, int i)
{
    static const char* s[4][3] = { { "HP", "LP", "Sat mix" }, { "Decay", "Pre-delay", "Mix" }, { "Time", "Feedback", "Mix" }, { "Boost", "Duck", "Width" } };
    return s[n & 3][i % 3];
}
int RouteView::stageCount() const { return wm::fx::parseGraph(proc.getGraph().toStdString()).stages; }
float RouteView::colX(int stage) const { return (float) getWidth() * ((float) stage + 0.5f) / (float) (stageCount() + 2); }
juce::Rectangle<float> RouteView::boxRect(int stage, int idx, int count) const
{
    const float colW = (float) getWidth() / (float) (stageCount() + 2), bw = juce::jmin(200.f, colW - 34.f);
    const float availH = (float) getHeight() * 0.62f, gap = 8.f;
    const float bh = count <= 1 ? 116.f : juce::jmin(116.f, (availH - gap * (float) (count - 1)) / (float) count);
    const float cy = (float) getHeight() * 0.46f, total = (float) count * bh + (float) (count - 1) * gap, y0 = cy - total * 0.5f + (float) idx * (bh + gap);
    return { colX(stage + 1) - bw * 0.5f, y0, bw, bh };
}
std::vector<RouteView::Box> RouteView::boxes() const
{
    std::vector<Box> v; const auto g = wm::fx::parseGraph(proc.getGraph().toStdString());
    for (int s = 0; s < g.stages; ++s) for (int j = 0; j < g.count[s]; ++j) v.push_back({ g.items[s][j], boxRect(s, j, g.count[s]) });
    return v;
}
int RouteView::rowAt(const Box& b, juce::Point<float> pt) const
{
    const bool big = b.r.getHeight() >= 100.f; const float top = b.r.getY() + (big ? 48.f : 30.f), rh = (b.r.getBottom() - 6.f - top) / 3.f;
    if (pt.y < top || pt.y > b.r.getBottom() - 6.f || pt.x < b.r.getX() + 6.f || pt.x > b.r.getRight() - 6.f) return -1;
    return juce::jlimit(0, 2, (int) ((pt.y - top) / rh));
}
void RouteView::paint(juce::Graphics& g)
{
    g.fillAll(col::disp); g.setColour(col::line); g.drawRect(getLocalBounds(), 1);
    const auto gr = wm::fx::parseGraph(proc.getGraph().toStdString());
    const auto bx = boxes(); const float cy = (float) getHeight() * 0.46f;
    auto boxOf = [&](int node) { for (auto& b : bx) if (b.node == node) return b.r; return juce::Rectangle<float>(); };
    g.setColour(col::dim); g.setFont(font(13.f, false, 0.03f));
    g.drawText("Drag a box to move it: drop between two columns for in series, or on a column for side by side.", 18, 8, getWidth() - 36, 18, juce::Justification::left);
    g.drawText("Power icon: bypass.  Arrow icon: jump to its controls.  Hold a number and drag up or down to change it (Shift = fine, double-click = reset).", 18, 26, getWidth() - 36, 18, juce::Justification::left);
    const juce::Rectangle<float> inR(colX(0) - 32.f, cy - 29.f, 64.f, 58.f), outR(colX(gr.stages + 1) - 32.f, cy - 29.f, 64.f, 58.f);
    auto link = [&](juce::Point<float> a, juce::Point<float> b)
    {
        juce::Path p; p.startNewSubPath(a); p.cubicTo(a.x + 24.f, a.y, b.x - 24.f, b.y, b.x, b.y);
        g.setColour(col::dim.withAlpha(0.8f)); g.strokePath(p, juce::PathStrokeType(1.5f));
        juce::Path ar; ar.addTriangle(b.x, b.y, b.x - 7.f, b.y - 4.f, b.x - 7.f, b.y + 4.f); g.fillPath(ar);
    };
    for (int j = 0; j < gr.count[0]; ++j) { const auto r = boxOf(gr.items[0][j]); link({ inR.getRight(), cy }, { r.getX(), r.getCentreY() }); }
    for (int s = 0; s + 1 < gr.stages; ++s)
        for (int a = 0; a < gr.count[s]; ++a) for (int b = 0; b < gr.count[s + 1]; ++b)
        { const auto ra = boxOf(gr.items[s][a]), rb = boxOf(gr.items[s + 1][b]); link({ ra.getRight(), ra.getCentreY() }, { rb.getX(), rb.getCentreY() }); }
    for (int j = 0; j < gr.count[gr.stages - 1]; ++j) { const auto r = boxOf(gr.items[gr.stages - 1][j]); link({ r.getRight(), r.getCentreY() }, { outR.getX(), cy }); }
    for (int k = 0; k < 2; ++k)
    {
        const auto r = k == 0 ? inR : outR;
        g.setColour(col::panel2); g.fillRoundedRectangle(r, 8.f); g.setColour(col::dim); g.drawRoundedRectangle(r, 8.f, 1.5f);
        g.setColour(col::ink); g.setFont(font(17.f, true, 0.08f)); g.drawText(k == 0 ? "IN" : "OUT", r.withTrimmedTop(8.f).withHeight(24.f).toNearestInt(), juce::Justification::centred);
        g.setColour(col::dim); g.setFont(font(11.f, false, 0.12f)); g.drawText(k == 0 ? "dry" : "mix", r.withTrimmedTop(32.f).withHeight(16.f).toNearestInt(), juce::Justification::centred);
    }
    float dry = 1.f;
    for (int s = 0; s < gr.stages; ++s)
    {
        float sum = 0.f;
        for (int j = 0; j < gr.count[s]; ++j) { const int n = gr.items[s][j]; sum += n == wm::fx::kRev && proc.get("rev_on") > .5f ? 1.f - proc.get("mix") : n == wm::fx::kDel && proc.get("del_on") > .5f ? 1.f - proc.get("dmix") : 1.f; }
        dry *= sum / (float) gr.count[s];
    }
    {
        const float yb = (float) getHeight() - 44.f; juce::Path p; p.startNewSubPath(inR.getCentreX(), inR.getBottom()); p.lineTo(inR.getCentreX(), yb); p.lineTo(outR.getCentreX(), yb); p.lineTo(outR.getCentreX(), outR.getBottom());
        juce::Path dashed; const float dl[2] = { 6.f, 5.f }; juce::PathStrokeType(1.4f).createDashedStroke(dashed, p, dl, 2); g.setColour(col::dim.withAlpha(0.8f)); g.fillPath(dashed);
        g.setFont(mono(12.f)); g.setColour(col::dim);
        g.drawText("dry path: " + juce::String(juce::roundToInt(dry * 100.f)) + " % of the input goes straight to the output (wet effects take the rest)", getLocalBounds().withY((int) yb - 22).withHeight(18), juce::Justification::centred);
    }
    for (auto& b : bx)
    {
        auto r = b.r; if (moving && b.node == dragNode) r = r.withCentre(cur);
        const bool on = proc.get(onId(b.node)) > .5f; const bool big = r.getHeight() >= 100.f;
        const juce::Colour c = b.node == wm::fx::kRev ? col::lilac : b.node == wm::fx::kDel ? col::mint : b.node == wm::fx::kIfx ? col::blue : col::yellow;
        g.setColour(col::panel2.withAlpha(moving && b.node == dragNode ? 0.75f : 1.f)); g.fillRoundedRectangle(r, 7.f);
        if (on) { g.setColour(c); g.drawRoundedRectangle(r, 7.f, 2.f); }
        else { juce::Path rp; rp.addRoundedRectangle(r, 7.f); juce::Path d; const float dl[2] = { 5.f, 4.f }; juce::PathStrokeType(1.3f).createDashedStroke(d, rp, dl, 2); g.setColour(col::dim.withAlpha(0.7f)); g.fillPath(d); }
        const auto pw = juce::Rectangle<float>(r.getX() + 8.f, r.getY() + 7.f, 14.f, 14.f);
        g.setColour(on ? c : col::dim);
        { juce::Path a; a.addCentredArc(pw.getCentreX(), pw.getCentreY(), 5.5f, 5.5f, 0.f, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true); g.strokePath(a, juce::PathStrokeType(1.6f)); g.drawLine(pw.getCentreX(), pw.getY() + 1.f, pw.getCentreX(), pw.getCentreY() - 1.f, 1.6f); }
        const auto t = juce::Rectangle<float>(r.getRight() - 24.f, r.getY() + 7.f, 14.f, 13.f);
        g.setColour(col::dim); g.drawRect(t, 1.2f); g.drawLine(t.getX() + 4.f, t.getBottom() - 3.f, t.getRight() + 1.f, t.getY() - 1.f, 1.2f);
        g.drawLine(t.getRight() - 4.f, t.getY() - 1.f, t.getRight() + 1.f, t.getY() - 1.f, 1.2f); g.drawLine(t.getRight() + 1.f, t.getY() - 1.f, t.getRight() + 1.f, t.getY() + 4.f, 1.2f);
        g.setColour(on ? c : col::dim); g.setFont(font(big ? 16.f : 13.f, true, 0.03f));
        g.drawText(juce::String(nodeName(b.node)) + (on ? "" : " (off)"), r.withTrimmedLeft(26.f).withTrimmedRight(26.f).withHeight(big ? 26.f : 22.f).toNearestInt(), juce::Justification::centred);
        if (big)
        {
            juce::String sub = nodeSub(b.node);
            if (b.node == wm::fx::kRev) sub = ShimmerstuffProcessor::typeNames()[juce::jlimit(0, 4, (int) proc.get("type"))];
            if (b.node == wm::fx::kDel) sub = (proc.get("dshift") > 0.4f ? "+" : "") + juce::String(juce::roundToInt(proc.get("dshift"))) + " st per repeat";
            g.setColour(col::dim); g.setFont(font(11.f, false, 0.03f)); g.drawText(sub, r.withTrimmedTop(26.f).withHeight(16.f).toNearestInt(), juce::Justification::centred);
        }
        const float top = r.getY() + (big ? 48.f : 30.f), rh = (r.getBottom() - 6.f - top) / 3.f;
        for (int i = 0; i < 3; ++i)
        {
            const juce::String id = rowId(b.node, i); const auto row = juce::Rectangle<float>(r.getX() + 6.f, top + rh * (float) i, r.getWidth() - 12.f, rh - 1.f);
            g.setColour(col::disp); g.fillRoundedRectangle(row, 2.f);
            g.setColour(c.withAlpha(on ? 0.35f : 0.12f)); g.fillRoundedRectangle(row.withWidth(row.getWidth() * getNorm(proc, id)), 2.f);
            g.setFont(font(juce::jmin(12.f, rh - 2.f))); g.setColour(on ? col::dim : col::dim.withAlpha(0.6f)); g.drawText(rowName(b.node, i), row.reduced(6.f, 0.f).toNearestInt(), juce::Justification::left);
            g.setFont(mono(juce::jmin(12.5f, rh - 2.f))); g.setColour(on ? col::ink : col::dim); g.drawText(fmtVal(id, proc.get(id)), row.reduced(6.f, 0.f).toNearestInt(), juce::Justification::right);
        }
    }
}
void RouteView::mouseDown(const juce::MouseEvent& e)
{
    dragNode = valNode = valRow = -1; moving = false; down = e.position; cur = e.position;
    for (auto& b : boxes())
    {
        if (!b.r.contains(e.position)) continue;
        if (juce::Rectangle<float>(b.r.getX(), b.r.getY(), 30.f, 28.f).contains(e.position))
        { const juce::String id = onId(b.node); gesture(proc, id, true); setNorm(proc, id, proc.get(id) > .5f ? 0.f : 1.f); gesture(proc, id, false); repaint(); return; }
        if (juce::Rectangle<float>(b.r.getRight() - 30.f, b.r.getY(), 30.f, 28.f).contains(e.position))
        { openTab(b.node == wm::fx::kRev ? 0 : b.node == wm::fx::kDel ? 1 : 2); return; }
        const int row = rowAt(b, e.position);
        if (row >= 0) { valNode = b.node; valRow = row; valStart = getNorm(proc, rowId(b.node, row)); gesture(proc, rowId(b.node, row), true); return; }
        dragNode = b.node; return;
    }
}
void RouteView::mouseDrag(const juce::MouseEvent& e)
{
    cur = e.position;
    if (valNode >= 0) { setNorm(proc, rowId(valNode, valRow), valStart + (down.y - e.position.y) / (e.mods.isShiftDown() ? 1000.f : 200.f)); repaint(); return; }
    if (dragNode >= 0 && (moving || e.position.getDistanceFrom(down) > 6.f)) { moving = true; repaint(); }
}
void RouteView::mouseUp(const juce::MouseEvent& e)
{
    if (valNode >= 0) { gesture(proc, rowId(valNode, valRow), false); valNode = -1; return; }
    if (dragNode >= 0 && moving)
    {
        auto g = wm::fx::parseGraph(proc.getGraph().toStdString());
        for (int s = 0; s < g.stages; ++s)
            for (int j = 0; j < g.count[s]; ++j)
                if (g.items[s][j] == dragNode) { for (int k = j; k + 1 < g.count[s]; ++k) g.items[s][k] = g.items[s][k + 1]; --g.count[s]; }
        for (int s = 0; s < g.stages; ) { if (g.count[s] == 0) { for (int k = s; k + 1 < g.stages; ++k) { g.count[k] = g.count[k + 1]; for (int j = 0; j < 4; ++j) g.items[k][j] = g.items[k + 1][j]; } --g.stages; } else ++s; }
        const float w = (float) getWidth(); int placed = -1;
        for (int s = 0; s < g.stages; ++s)
        {
            const float cx = w * ((float) (s + 1) + 0.5f) / (float) (g.stages + 2);
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
        if (b.r.contains(e.position)) { const int row = rowAt(b, e.position); if (row >= 0) resetParam(proc, rowId(b.node, row)); }
}

// ---------------------------------------------------------------- LevelMeter
static juce::String dbText(float v) { return v < 1e-4f ? "-inf" : juce::String(juce::Decibels::gainToDecibels(v), 1); }
void LevelMeter::tick()
{
    inV = juce::jmax(proc.inPk.load(), inV * 0.88f); outV = juce::jmax(proc.outPk.load(), outV * 0.88f);
    inHold = juce::jmax(inV, inHold - 0.004f); outHold = juce::jmax(outV, outHold - 0.004f);
    inMax = juce::jmax(inMax, inV); outMax = juce::jmax(outMax, outV);
    if (outV >= 1.f || inV >= 1.f) clip = true;
    repaint();
}
void LevelMeter::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff131418)); g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.f);
    g.setColour(col::line); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.f, 1.f);
    g.setColour(col::dim); g.setFont(font(12.f, true, 0.2f)); g.drawText("LEVEL", 10, 6, 80, 18, juce::Justification::left);
    g.drawText("CLIP", getWidth() - 62, 6, 40, 18, juce::Justification::right);
    g.setColour(clip ? col::pink : juce::Colour(0xff3a1020)); g.fillEllipse((float) getWidth() - 18.f, 10.f, 10.f, 10.f);
    const auto r = juce::Rectangle<float>(8.f, 30.f, (float) getWidth() - 16.f, (float) getHeight() - 30.f - 124.f);
    g.setColour(col::disp); g.fillRect(r); g.setColour(col::line); g.drawRect(r, 1.f);
    auto db2y = [&](float db) { return r.getBottom() - juce::jlimit(0.f, 1.f, (db + 54.f) / 54.f) * r.getHeight(); };
    g.setFont(mono(10.f));
    for (int d = 0; d >= -54; d -= 6)
    {
        const float y = db2y((float) d); g.setColour(juce::Colour(0xff1b2523)); g.drawHorizontalLine((int) y, r.getX(), r.getRight());
        g.setColour(d == 0 ? col::yellow : col::lilac.withAlpha(0.7f)); g.drawText(juce::String(d), (int) r.getCentreX() - 20, (int) y - 6, 40, 12, juce::Justification::centred);
    }
    const float bw = 12.f;
    for (int c = 0; c < 2; ++c)
    {
        const float v = c == 0 ? inV : outV, hold = c == 0 ? inHold : outHold, x = c == 0 ? r.getX() + 6.f : r.getRight() - 6.f - 2.f * bw - 4.f;
        const float top = db2y(juce::Decibels::gainToDecibels(v, -60.f));
        for (int ch = 0; ch < 2; ++ch)
        {
            const float bx = x + (float) ch * (bw + 4.f);
            g.setGradientFill(juce::ColourGradient(col::pink, 0.f, db2y(0.f), col::mint, 0.f, db2y(-24.f), false)); g.fillRect(bx, top, bw, r.getBottom() - top);
            g.setColour(col::ink); g.fillRect(bx, db2y(juce::Decibels::gainToDecibels(hold, -60.f)), bw, 2.f);
        }
        g.setColour(col::dim); g.setFont(font(10.f, false, 0.1f)); g.drawText(c == 0 ? "IN" : "OUT", (int) x - 4, (int) r.getBottom() - 14, (int) (2.f * bw + 12.f), 12, juce::Justification::centred);
    }
    const bool delTab = tab == 1;
    struct R { juce::String a, av, b, bv; };
    const R rows[3] = { { "In peak", dbText(inMax), "Out peak", dbText(outMax) }, { "RMS", dbText(proc.rms.load()), tab == 0 ? "RT60" : "Echo", tab == 0 ? juce::String(rt60, 1) + " s" : juce::String(juce::roundToInt(proc.get("dtime"))) + " ms" },
                        { "Mix", juce::String(juce::roundToInt((delTab ? proc.get("dmix") : proc.get("mix")) * 100.f)) + " %", "Clip", clip ? "yes" : "no" } };
    g.setFont(mono(11.f));
    for (int i = 0; i < 3; ++i)
    {
        const int y = (int) r.getBottom() + 8 + i * 17;
        g.setColour(col::dim); g.drawText(rows[i].a, 8, y, 60, 15, juce::Justification::left); g.drawText(rows[i].b, getWidth() / 2 + 2, y, 60, 15, juce::Justification::left);
        g.setColour(col::yellow); g.drawText(rows[i].av, 8, y, getWidth() / 2 - 14, 15, juce::Justification::right); g.drawText(rows[i].bv, getWidth() / 2 + 2, y, getWidth() / 2 - 10, 15, juce::Justification::right);
    }
}

// ---------------------------------------------------------------- Body
namespace
{
const char* kTabTitle[3] = { "SHIMMER REVERB", "SHIMMER DELAY", "FX & ROUTING" };
const char* kTabSub[3] = { "VST3 and Standalone", "VST3 and Standalone", "input and wet processing" };
const char* kHeadTitle[3] = { "SHIMMER REVERB", "SHIMMER DELAY", "FX AND ROUTING" };
const char* kSubName[3][2] = { { "DECAY", "TONE" }, { "ECHOES", "TONE" }, { "ROUTING", "TONE" } };
const char* kDirName[3] = { "reverb", "delay", "fx" };
const std::vector<const char*>& keysOf(int t) { return t == 0 ? ssp::reverbKeys() : t == 1 ? ssp::delayKeys() : ssp::fxKeys(); }
}

Body::Body(ShimmerstuffProcessor& p)
    : proc(p), decay(p), toneRev(p, 0), toneDel(p, 1), toneFx(p, 2), echo(p), route(p, [this](int t) { setTab(t); }), shape(p), meter(p)
{
    setLookAndFeel(&laf); setSize(IW, IH);
    for (int i = 0; i < 3; ++i)
    {
        tabBtn[i].title = kTabTitle[i]; tabBtn[i].subtitle = kTabSub[i]; tabBtn[i].onClick = [this, i] { setTab(i); }; addAndMakeVisible(tabBtn[i]);
        tabBtn[i].setTooltip(i == 0 ? "Reverb: plate, hall, shimmer plate, shimmer hall and velvet. A view for fine tuning: the routing stays as you set it." : i == 1 ? "Delay: echoes that climb (or fall) in pitch with every repeat. A view for fine tuning: the routing stays as you set it." : "Route the reverb and delay (one, the other, in series or in parallel) and add saturation, squash, duck and width.");
        for (int j = 0; j < 2; ++j) { subBtn[i][j].setButtonText(kSubName[i][j]); subBtn[i][j].onClick = [this, i, j] { sub[i] = j; setTab(tab); }; addChildComponent(subBtn[i][j]); }
    }
    for (auto* b : { &pPrev, &pNext, &pSave, &pMore, &bGuide, &bSet, &bAbout, &mainSave, &turnOn, &peakReset }) addAndMakeVisible(*b);
    turnOn.setVisible(false);
    pPrev.setTooltip("Previous preset."); pNext.setTooltip("Next preset."); pSave.setTooltip("Save the settings of this tab as your own preset (kept on this computer).");
    pMore.setTooltip("Delete your presets or open the presets folder."); mainSave.setTooltip("Save the whole plugin (reverb, delay, FX and routing) as your own main preset.");
    bGuide.setTooltip("Quick guide: what each view and control does."); bSet.setTooltip("Info bubbles."); bAbout.setTooltip("About Studio 87, privacy, and ways to support the project.");
    peakReset.setTooltip("Clears the held peaks and the clip light.");
    pPrev.onClick = [this] { stepTabPreset(-1); }; pNext.onClick = [this] { stepTabPreset(1); }; pSave.onClick = [this] { saveTabPreset(); }; pMore.onClick = [this] { showMore(); };
    mainSave.onClick = [this] { saveMainPreset(); }; bGuide.onClick = [this] { showGuide(); }; bSet.onClick = [this] { showSettings(); }; bAbout.onClick = [this] { showAbout(); };
    peakReset.onClick = [this] { meter.resetPeaks(); };
    addAndMakeVisible(tabCombo); addAndMakeVisible(mainCombo);
    tabCombo.setTooltip("Presets for this tab: factory ones and your own. Moving a control shows Custom."); tabCombo.onChange = [this] { tabPresetChosen(); };
    mainCombo.setTooltip("Main presets: the whole plugin (reverb, delay, FX and routing) in one go."); mainCombo.setTextWhenNothingSelected("Main preset: choose..."); mainCombo.onChange = [this] { mainPresetChosen(); };
    for (auto* v : { (juce::Component*) &decay, (juce::Component*) &toneRev, (juce::Component*) &toneDel, (juce::Component*) &toneFx, (juce::Component*) &echo, (juce::Component*) &route, (juce::Component*) &shape }) addChildComponent(*v);
    addAndMakeVisible(meter); peakReset.toFront(false);
    auto& a = p.apvts;
    rType.init(*this, a, "type", "Type", ShimmerstuffProcessor::typeNames(), "Plate, hall, shimmer plate (tail pitch shifted back into the reverb), shimmer hall, or velvet (dense and smooth).");
    lay.init(*this, a, "route", "Layout", ShimmerstuffProcessor::layoutNames(), "Ready made arrangements of Input FX, Reverb, Delay and Wet FX. You can also drag the boxes in the diagram; a layout of your own shows as Custom.");
    dMode.init(*this, a, "w_dmode", "Timing", juce::StringArray { "Follow the source", "Free (Hz)", "BPM sync" }, "Follow: the effect dips while the source plays (classic tail duck). Free: a repeating shape in Hz. BPM sync: a repeating shape locked to the host tempo.");
    dSrc.init(*this, a, "w_dsrc", "Sidechain source", juce::StringArray { "None", "Plugin input (dry)", "Sidechain input" }, "Where the duck listens. Sidechain input: the plugin's sidechain (enable it in your DAW).");
    dRatio.init(*this, a, "w_dratio", "Ratio", ShimmerstuffProcessor::ratioNames(), "Length of one duck cycle at the host tempo (4/4). 1/4 is one beat.");
    shape.setTooltip("Duck shape: click one. Taller means more ducking at that moment of the cycle.");
    struct K { const char* id; const char* name; Knob::Kind k; };
    const K rks[13] = { { "decay", "Decay", Knob::Pct }, { "size", "Size", Knob::Size }, { "predelay", "Pre-delay", Knob::Ms }, { "mod", "Modulation", Knob::Pct }, { "color", "Velvet colour", Knob::Plain },
        { "shift", "Pitch", Knob::St }, { "shimmer", "Amount", Knob::Pct }, { "shift2", "Pitch 2", Knob::St }, { "shimmer2", "Amount 2", Knob::Pct },
        { "damp", "Damping", Knob::Hz }, { "hp", "Wet HP", Knob::Hz }, { "lp", "Wet LP", Knob::Hz }, { "mix", "Mix", Knob::Pct } };
    const char* rtip[13] = { "Tail length. Drag the yellow dot in the Decay view sideways too.", "Room size. Drag the yellow dot up or down too.", "Gap before the reverb starts. Drag the cyan handle too.", "Slow movement inside the tail, for a lusher sound.",
        "Velvet only: dark to bright tilt.", "Pitch of the shimmer voice fed back into the reverb (shimmer types).", "How much shifted tail is fed back (shimmer types). Drag the orange handle too.", "Pitch of the second shimmer voice.", "Amount of the second shimmer voice. 0 is off.",
        "Darkens the tail. Drag the blue handle or the line in the Tone view too.", "High-pass on the reverb tail only. Keeps the tail out of the bass.", "Low-pass on the reverb tail only.", "Reverb dry/wet. Drag the pink handle too." };
    const juce::Colour rcol[13] = { col::lilac, col::lilac, col::lilac, col::lilac, col::lilac, col::yellow, col::yellow, col::yellow, col::yellow, col::blue, col::blue, col::blue, col::pink };
    for (int i = 0; i < 13; ++i) rk[i].init(*this, a, rks[i].id, rks[i].name, rks[i].k, rcol[i], rtip[i]);
    const K dks[6] = { { "dtime", "Time", Knob::Ms }, { "dfb", "Feedback", Knob::Pct }, { "dshift", "Shift", Knob::St }, { "dshim", "Shimmer", Knob::Pct }, { "ddamp", "Damping", Knob::Hz }, { "dmix", "Mix", Knob::Pct } };
    const char* dtip[6] = { "Time between repeats. Drag the first dot in the Echoes view too.", "How much of each repeat goes back in. Drag the second dot too.", "Pitch change added at every repeat. +12 climbs an octave each time.",
        "How much of each repeat is shifted. 0 is a plain delay.", "Darkens every repeat a little more.", "Delay dry/wet." };
    const juce::Colour dcol[6] = { col::mint, col::mint, col::yellow, col::yellow, col::blue, col::pink };
    for (int i = 0; i < 6; ++i) dk[i].init(*this, a, dks[i].id, dks[i].name, dks[i].k, dcol[i], dtip[i]);
    const K iks[4] = { { "i_hp", "Input HP", Knob::Hz }, { "i_lp", "Input LP", Knob::Hz }, { "i_drive", "Drive", Knob::Db }, { "i_mix", "Sat mix", Knob::Pct } };
    const char* itip[4] = { "Cuts lows before they reach the effects.", "Cuts highs before they reach the effects.", "Saturation on the signal that feeds the effects. Antialiased.", "Dry/wet of the input saturation. 0 is off." };
    const juce::Colour icol[4] = { col::blue, col::blue, col::yellow, col::pink };
    for (int i = 0; i < 4; ++i) ik[i].init(*this, a, iks[i].id, iks[i].name, iks[i].k, icol[i], itip[i]);
    const K wks[6] = { { "w_drive", "Drive", Knob::Db }, { "w_smix", "Sat mix", Knob::Pct }, { "w_width", "Width", Knob::Pct }, { "w_boost", "Wet boost", Knob::Db }, { "w_depth", "Depth", Knob::Pct }, { "w_qmix", "Mix", Knob::Pct } };
    const char* wtip[6] = { "Saturation on the wet signal only. Antialiased.", "Dry/wet of the wet saturation. 0 is off.", "Stereo width of the wet signal. 100 % is unchanged, 0 is mono.", "Extra level on the wet signal, to push the effect harder.",
        "How hard the three bands are pushed: quiet up, loud down. Dense, bright tails (OTT style).", "Dry/wet of the squash. 0 is off." };
    const juce::Colour wcol[6] = { col::yellow, col::pink, col::blue, col::yellow, col::yellow, col::pink };
    for (int i = 0; i < 6; ++i) wk[i].init(*this, a, wks[i].id, wks[i].name, wks[i].k, wcol[i], wtip[i]);
    const K dus[3] = { { "w_duck", "Amount", Knob::Pct }, { "w_drel", "Release", Knob::Ms }, { "w_drate", "Rate", Knob::Plain } };
    const char* utip[3] = { "How deep the effect dips. 0 is off.", "Follow mode: how fast the effect comes back after the source stops.", "Free mode: how often the shape repeats, in Hz." };
    for (int i = 0; i < 3; ++i) dukn[i].init(*this, a, dus[i].id, dus[i].name, dus[i].k, col::yellow, utip[i]);
    dukn[2].s.textFromValueFunction = [](double v) { return juce::String(v, 2) + " Hz"; };
    dukn[2].s.valueFromTextFunction = [](const juce::String& t) { return t.retainCharacters("0123456789.").getDoubleValue(); };
    dukn[2].s.onValueChange();
    static const char* onIds[4] = { "rev_on", "del_on", "ifx_on", "wfx_on" }; static const char* onNm[4] = { "REVERB", "DELAY", "INPUT FX", "WET FX" };
    for (int i = 0; i < 4; ++i)
    {
        onBtn[i].setButtonText(onNm[i]); onBtn[i].setClickingTogglesState(true); addAndMakeVisible(onBtn[i]); onBtn[i].setTooltip(juce::String("Switch ") + onNm[i] + " on or off in the routing.");
        onBtn[i].setColour(juce::TextButton::buttonOnColourId, col::sel); onBtn[i].setColour(juce::TextButton::textColourOnId, i == 2 ? col::blue : i == 3 ? col::yellow : i == 1 ? col::mint : col::lilac);
        aOn[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(a, onIds[i], onBtn[i]);
    }
    for (auto* b : { &revOn, &delOn })
    { b->setClickingTogglesState(true); addAndMakeVisible(*b); b->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff0f2a26)); b->setColour(juce::TextButton::textColourOnId, col::mint); }
    revOn.setTooltip("Switches the reverb on or off. Same switch as the power icon on the Reverb box in FX and Routing: the routing you chose stays active on every tab, this tab is only for fine tuning.");
    delOn.setTooltip("Switches the delay on or off. Same switch as the power icon on the Delay box in FX and Routing: the routing you chose stays active on every tab, this tab is only for fine tuning.");
    aRev = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(a, "rev_on", revOn); aDel = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(a, "del_on", delOn);
    turnOn.onClick = [this]
    {
        const char* id = tab == 0 ? "rev_on" : "del_on"; gesture(proc, id, true); setNorm(proc, id, 1.f); gesture(proc, id, false);
    };
    refreshMainPresets();
    setTab(0);
}

juce::File Body::presetRoot() const { return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Studio 87").getChildFile("shimmerstuff").getChildFile("presets"); }
juce::File Body::tabDir(int t) const { return presetRoot().getChildFile(kDirName[t]); }

std::vector<float> Body::snapshot(int t) const
{
    std::vector<float> v;
    for (auto* k : keysOf(t))
    {
        if (t < 2 && (juce::String(k) == "rev_on" || juce::String(k) == "del_on")) continue;      // the on/off switch is not part of a tab preset's identity
        v.push_back(getNorm(proc, k));
    }
    if (t == 2 && (int) proc.get("route") >= ShimmerstuffProcessor::kLayouts) v.push_back((float) (proc.getGraph().hashCode() & 0xffff));
    return v;
}
int Body::matchFactory(int t) const
{
    const int n = proc.numTabPresets(t); const auto& list = t == 0 ? ssp::reverbPresets() : t == 1 ? ssp::delayPresets() : ssp::fxPresets();
    for (int i = 0; i < n; ++i)
    {
        const auto& pre = list[(size_t) i]; bool ok = true;
        for (auto* k : keysOf(t))
        {
            const juce::String id(k);
            if (t < 2 && (id == "rev_on" || id == "del_on")) continue;
            auto* q = proc.apvts.getParameter(id); if (q == nullptr) continue;
            if (id == "route" && pre.graph != nullptr)
            {
                const juce::String want(wm::fx::graphString(wm::fx::parseGraph(std::string(pre.graph))));
                if (proc.getGraph() != want) { ok = false; break; }
                continue;
            }
            float want = q->getDefaultValue();
            for (const auto& kv : pre.v) if (id == kv.first) want = q->convertTo0to1(kv.second);
            if (std::abs(q->getValue() - want) > 0.004f) { ok = false; break; }
        }
        if (ok) return i;
    }
    return -1;
}
void Body::updateTabComboText()
{
    juce::String label;
    if (selLabel[tab].isNotEmpty() && selSnap[tab] == snapshot(tab)) label = selLabel[tab];
    else { selLabel[tab] = {}; const int i = matchFactory(tab); label = i >= 0 ? proc.tabPresetName(tab, i) : "Custom"; }
    if (tabCombo.getText() != label) tabCombo.setText(label, juce::dontSendNotification);
}
void Body::refreshTabPresets()
{
    tabCombo.clear(juce::dontSendNotification); comboIds.clear(); userFiles.clear();
    for (int i = 0; i < proc.numTabPresets(tab); ++i) { tabCombo.addItem(proc.tabPresetName(tab, i), i + 1); comboIds.push_back(i + 1); }
    auto d = tabDir(tab);
    if (d.isDirectory()) for (auto& f : d.findChildFiles(juce::File::findFiles, false, "*.ssp")) userFiles.push_back(f);
    if (!userFiles.empty()) tabCombo.addSeparator();
    for (size_t i = 0; i < userFiles.size(); ++i) { tabCombo.addItem(userFiles[i].getFileNameWithoutExtension() + " (mine)", 1000 + (int) i); comboIds.push_back(1000 + (int) i); }
    updateTabComboText();
}
void Body::refreshMainPresets()
{
    mainCombo.clear(juce::dontSendNotification); userMain.clear();
    for (int i = 0; i < proc.numMainPresets(); ++i) mainCombo.addItem(proc.mainPresetName(i), i + 1);
    auto d = presetRoot();
    if (d.isDirectory()) for (auto& f : d.findChildFiles(juce::File::findFiles, false, "*.ssp")) userMain.push_back(f);
    if (!userMain.empty()) mainCombo.addSeparator();
    for (size_t i = 0; i < userMain.size(); ++i) mainCombo.addItem(userMain[i].getFileNameWithoutExtension() + " (mine)", 1000 + (int) i);
    mainCombo.setSelectedId(0, juce::dontSendNotification);
}
void Body::tabPresetChosen()
{
    const int id = tabCombo.getSelectedId();
    if (id >= 1000) { const size_t i = (size_t) (id - 1000); if (i < userFiles.size()) if (auto xml = juce::XmlDocument::parse(userFiles[i])) if (xml->hasTagName("STATE")) { proc.applyUserPresetXml(*xml); selLabel[tab] = userFiles[i].getFileNameWithoutExtension() + " (mine)"; } }
    else if (id >= 1) { proc.applyTabPreset(tab, id - 1); selLabel[tab] = proc.tabPresetName(tab, id - 1); }
    selSnap[tab] = snapshot(tab); repaint();
}
void Body::stepTabPreset(int d)
{
    if (comboIds.empty()) return;
    int cur = -1; const int sid = tabCombo.getSelectedId(); for (size_t i = 0; i < comboIds.size(); ++i) if (comboIds[i] == sid) cur = (int) i;
    if (cur < 0) { const int m = matchFactory(tab); cur = m >= 0 ? m : (d > 0 ? -1 : 0); }
    const int n = (int) comboIds.size(); const int next = ((cur + d) % n + n) % n;
    tabCombo.setSelectedId(comboIds[(size_t) next], juce::sendNotificationSync);
}
void Body::mainPresetChosen()
{
    const int id = mainCombo.getSelectedId();
    if (id >= 1000) { const size_t i = (size_t) (id - 1000); if (i < userMain.size()) if (auto xml = juce::XmlDocument::parse(userMain[i])) if (xml->hasTagName("STATE")) proc.applyUserPresetXml(*xml); }
    else if (id >= 1) proc.setCurrentProgram(id - 1);
    for (int t = 0; t < 3; ++t) selLabel[t] = {};
    repaint();
}
static void askName(juce::Component* owner, const juce::String& title, const juce::String& msg, std::function<void(juce::String)> done)
{
    auto* w = new juce::AlertWindow(title, msg, juce::MessageBoxIconType::NoIcon, owner);
    w->addTextEditor("name", "My preset"); w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey)); w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->enterModalState(true, juce::ModalCallbackFunction::create([w, done](int r) { if (r == 1) done(juce::File::createLegalFileName(w->getTextEditorContents("name").trim())); delete w; }), false);
}
void Body::saveTabPreset()
{
    askName(this, "Save preset", "Name for your preset (the settings of this tab):", [this](juce::String name)
    {
        if (name.isEmpty()) return;
        auto d = tabDir(tab); d.createDirectory(); std::unique_ptr<juce::XmlElement> xml(proc.makePresetXml(tab)); xml->writeTo(d.getChildFile(name + ".ssp"));
        refreshTabPresets(); selLabel[tab] = name + " (mine)"; selSnap[tab] = snapshot(tab); updateTabComboText();
    });
}
void Body::saveMainPreset()
{
    askName(this, "Save main preset", "Name for your preset (the whole plugin: reverb, delay, FX and routing):", [this](juce::String name)
    {
        if (name.isEmpty()) return;
        auto d = presetRoot(); d.createDirectory(); std::unique_ptr<juce::XmlElement> xml(proc.makePresetXml(-1)); xml->writeTo(d.getChildFile(name + ".ssp"));
        refreshMainPresets();
    });
}
void Body::showMore()
{
    juce::PopupMenu m; int id = 1;
    for (auto& f : userFiles) m.addItem(id++, "Delete: " + f.getFileNameWithoutExtension());
    if (userFiles.empty()) m.addItem(-1, "No saved presets for this tab yet", false, false);
    m.addSeparator(); m.addItem(500, "Open presets folder");
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&pMore), [this](int r)
    {
        if (r == 500) { auto d = presetRoot(); d.createDirectory(); d.revealToUser(); }
        else if (r >= 1 && (size_t) r <= userFiles.size()) { userFiles[(size_t) r - 1].deleteFile(); refreshTabPresets(); refreshMainPresets(); }
    });
}
void Body::showSettings()
{
    juce::PopupMenu m; m.addItem(1, "Info bubbles after 1 s of hovering", true, proc.bubbles()); m.addItem(2, "Open presets folder");
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&bSet), [this](int r)
    {
        if (r == 1) { proc.setBubbles(!proc.bubbles()); if (tooltipWindow) tooltipWindow->setMillisecondsBeforeTipAppears(proc.bubbles() ? 1000 : 3600000); }
        else if (r == 2) { auto d = presetRoot(); d.createDirectory(); d.revealToUser(); }
    });
}
void Body::showAbout()
{
    auto* w = new juce::AlertWindow("About & Support", "shimmerstuff is a free and open source plugin by Studio 87 (AGPL-3.0-or-later). It collects nothing and never connects to the internet.\n\nShimmer Reverb (plate, hall, shimmer plate, shimmer hall, velvet), Shimmer Delay (repeats that climb or fall in pitch), and FX and Routing in one plugin.\n\nIf it helps your music, a coffee on Ko-fi keeps new plugins coming.", juce::MessageBoxIconType::NoIcon, this);
    w->addButton("Support Studio 87 on Ko-fi", 1); w->addButton("GitHub", 2); w->addButton("Close", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->enterModalState(true, juce::ModalCallbackFunction::create([w](int r)
    {
        if (r == 1) juce::URL("https://ko-fi.com/studio87").launchInDefaultBrowser();
        if (r == 2) juce::URL("https://github.com/SpeakingColors").launchInDefaultBrowser();
        delete w;
    }), false);
}
void Body::showGuide()
{
    auto* w = new juce::AlertWindow("Quick guide",
        "Shimmer Reverb, Shimmer Delay and FX & Routing are three views of one plugin. The routing you choose stays active whichever tab is open; the ON button on the Reverb and Delay tabs is the same switch as the power icon in the routing diagram.\n\n"
        "Decay: drag the yellow dot sideways for the decay and up or down for the size. Cyan: pre-delay. Blue: damping. Orange: shimmer. Pink: mix.\n"
        "Tone: the live spectrum with draggable filter lines.\n"
        "Echoes: drag the first dot for Time and the second for Feedback.\n"
        "Routing: drag the boxes (between two columns for in series, onto a column for side by side). Hold a number in a box and drag up or down.\n\n"
        "Double-click any knob or dot to reset it. Hover for 1 second for info bubbles.", juce::MessageBoxIconType::NoIcon, this);
    w->addButton("Close", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->enterModalState(true, juce::ModalCallbackFunction::create([w](int) { delete w; }), false);
}

void Body::setTab(int t)
{
    tab = t;
    for (int i = 0; i < 3; ++i) tabBtn[i].setToggleState(i == t, juce::dontSendNotification);
    const juce::Colour acc[3] = { col::lilac, col::mint, col::yellow };
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 2; ++j)
    {
        subBtn[i][j].setVisible(i == t); subBtn[i][j].setToggleState(sub[i] == j, juce::dontSendNotification);
        subBtn[i][j].setColour(juce::TextButton::textColourOnId, acc[i]);
    }
    meter.tab = t;
    refreshTabPresets(); layoutTab(); repaint();
}

void Body::layoutTab()
{
    const int L = 14, LW = 1072, bottom = 872, dispTop = 198;
    pPrev.setBounds(430, 18, 30, 34); tabCombo.setBounds(466, 18, 340, 34); pNext.setBounds(812, 18, 30, 34); pSave.setBounds(848, 18, 66, 34); pMore.setBounds(920, 18, 34, 34);
    bGuide.setBounds(978, 18, 66, 34); bSet.setBounds(1050, 18, 90, 34); bAbout.setBounds(1146, 18, 140, 34);
    for (int i = 0; i < 3; ++i) tabBtn[i].setBounds(L + i * 277, 92, 273, 56);
    mainCombo.setBounds(944, 100, 226, 36); mainSave.setBounds(1178, 96, 108, 44);
    for (int j = 0; j < 2; ++j) subBtn[tab][j].setBounds(L + j * 78, 162, j == 0 ? 74 : 62, 30);
    turnOn.setBounds(480, 172, 100, 22);
    meter.setBounds(1096, 162, 190, bottom - 162); peakReset.setBounds(1096 + 8, bottom - 40, 174, 32);
    for (auto* v : { (juce::Component*) &decay, (juce::Component*) &toneRev, (juce::Component*) &toneDel, (juce::Component*) &toneFx, (juce::Component*) &echo, (juce::Component*) &route, (juce::Component*) &shape }) v->setVisible(false);
    for (auto& k : rk) k.setVisible(false);
    for (auto& k : dk) k.setVisible(false);
    for (auto& k : ik) k.setVisible(false);
    for (auto& k : wk) k.setVisible(false);
    for (auto& k : dukn) k.setVisible(false);
    for (auto* c : { &rType, &lay, &dMode, &dSrc, &dRatio }) c->setVisible(false);
    for (auto& b : onBtn) b.setVisible(false);
    revOn.setVisible(false); delOn.setVisible(false);
    panels.clear();
    auto place = [](juce::Component& c, int x, int y, int w, int h) { c.setBounds(x, y, w, h); c.setVisible(true); };
    auto knobRow = [&](Knob* ks, int n, int x, int y, int cw) { for (int i = 0; i < n; ++i) place(ks[i], x + i * cw, y, cw, 76); };
    if (tab == 0)
    {
        const int rowH = 112, p2 = bottom - rowH, p1 = p2 - 8 - rowH;
        if (sub[0] == 0) decay.setVisible(true); else toneRev.setVisible(true);
        decay.setBounds(L, dispTop, LW, p1 - 8 - dispTop); toneRev.setBounds(decay.getBounds());
        panels.push_back({ { L, p1, LW, rowH }, 1, "SPACE", "type and size of the room" });
        panels.push_back({ { L, p2, 532, rowH }, 2, "SHIMMER", "for the two shimmer types" });
        panels.push_back({ { L + 540, p2, 532, rowH }, 3, "TONE AND MIX", "what reaches the output" });
        place(revOn, L + LW - 78, p1 + 6, 66, 24);
        place(rType, L + 12, p1 + 44, 132, 52); knobRow(rk, 5, L + 160, p1 + 32, 100);
        knobRow(rk + 5, 4, L + 16, p2 + 32, 100); knobRow(rk + 9, 4, L + 540 + 16, p2 + 32, 100);
    }
    else if (tab == 1)
    {
        const int rowH = 120, p1 = bottom - rowH, w3 = (LW - 16) / 3;
        if (sub[1] == 0) echo.setVisible(true); else toneDel.setVisible(true);
        echo.setBounds(L, dispTop, LW, p1 - 8 - dispTop); toneDel.setBounds(echo.getBounds());
        panels.push_back({ { L, p1, w3, rowH }, 1, "ECHO", "timing and repeats" }); panels.push_back({ { L + w3 + 8, p1, w3, rowH }, 2, "SHIMMER", "pitch of the repeats" }); panels.push_back({ { L + 2 * (w3 + 8), p1, w3, rowH }, 3, "TONE AND MIX", "what reaches the output" });
        place(delOn, L + w3 - 78, p1 + 6, 66, 24);
        knobRow(dk, 2, L + 16, p1 + 38, 110); knobRow(dk + 2, 2, L + w3 + 8 + 16, p1 + 38, 110); knobRow(dk + 4, 2, L + 2 * (w3 + 8) + 16, p1 + 38, 110);
    }
    else
    {
        const int r2 = 124, r2y = bottom - r2, r1 = 196, r1y = r2y - 8 - r1, w3 = (LW - 16) / 3;
        if (sub[2] == 0) route.setVisible(true); else toneFx.setVisible(true);
        route.setBounds(L, dispTop, LW, r1y - 8 - dispTop); toneFx.setBounds(route.getBounds());
        panels.push_back({ { L, r1y, w3, r1 }, 1, "ROUTING", "drag the boxes in the diagram" }); panels.push_back({ { L + w3 + 8, r1y, w3, r1 }, 2, "INPUT FX", "before the reverb and delay" });
        panels.push_back({ { L + 2 * (w3 + 8), r1y, w3, r1 }, 3, "WET SATURATION AND LEVEL", "" });
        panels.push_back({ { L, r2y, w3, r2 }, 4, "SQUASH", "lifts quiet tails, tames loud ones" }); panels.push_back({ { L + w3 + 8, r2y, 2 * w3 + 8, r2 }, 5, "DUCK", "the effect dips, then swells back" });
        place(lay, L + 12, r1y + 34, w3 - 24, 46);
        for (int i = 0; i < 4; ++i) place(onBtn[i], L + 12 + (i % 2) * 150, r1y + 92 + (i / 2) * 44, 142, 34);
        const int ix = L + w3 + 8 + 8, wx = L + 2 * (w3 + 8) + 8, cw = (w3 - 16) / 3;
        for (int i = 0; i < 3; ++i) place(ik[i], ix + i * cw, r1y + 32, cw, 76);
        place(ik[3], ix, r1y + 112, cw, 76);
        for (int i = 0; i < 3; ++i) place(wk[i], wx + i * cw, r1y + 32, cw, 76);
        place(wk[3], wx, r1y + 112, cw, 76);
        place(wk[4], L + 8, r2y + 34, 100, 76); place(wk[5], L + 8 + 100, r2y + 34, 100, 76);
        const int dx = L + w3 + 8;
        place(dMode, dx + 14, r2y + 30, 210, 40); place(dSrc, dx + 14, r2y + 74, 210, 40);
        place(dukn[0], dx + 244, r2y + 36, 100, 76);
        const int m = (int) proc.get("w_dmode");
        dukn[1].setVisible(m == 0); dukn[2].setVisible(m == 1); dRatio.setVisible(m == 2);
        dukn[1].setBounds(dx + 350, r2y + 36, 100, 76); dukn[2].setBounds(dx + 350, r2y + 36, 100, 76); dRatio.setBounds(dx + 356, r2y + 46, 90, 44);
        shape.setVisible(m != 0); shape.setBounds(dx + 466, r2y + 40, 2 * w3 + 8 - 480, 64);
    }
    repaint();
}

void Body::tickFrame()
{
    if (!isShowing()) return;
    if (decay.isVisible()) decay.tick();
    if (toneRev.isVisible()) toneRev.tick();
    if (toneDel.isVisible()) toneDel.tick();
    if (toneFx.isVisible()) toneFx.tick();
    meter.rt60 = decay.rt60Seconds(); meter.tick();
    if (echo.isVisible()) echo.repaint();
    if (route.isVisible()) route.repaint();
    if (shape.isVisible()) shape.repaint();
    if ((slow++ % 6) == 0)
    {
        updateTabComboText();
        if (tab == 2) { const int m = (int) proc.get("w_dmode"); dukn[1].setVisible(m == 0); dukn[2].setVisible(m == 1); dRatio.setVisible(m == 2); shape.setVisible(m != 0); }
        if (tab == 0)
        {
            const bool sh = decay.shimmerOn(); for (int i = 5; i < 9; ++i) rk[i].setAlpha(sh ? 1.f : 0.4f);
            rk[4].setAlpha((int) proc.get("type") == 4 ? 1.f : 0.35f);
        }
        const bool off = (tab == 0 && proc.get("rev_on") < .5f) || (tab == 1 && proc.get("del_on") < .5f);
        if (turnOn.isVisible() != off) turnOn.setVisible(off);
        repaint(14, 150, IW - 28, 48);
    }
}

void Body::paint(juce::Graphics& g)
{
    g.fillAll(col::panel);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1e1f23), (float) IW * 0.5f, 0.f, col::panel, (float) IW * 0.5f, 520.f, false)); g.fillRect(0, 0, IW, 520);
    catui::drawCatHead(g, { 14.f, 14.f, 44.f, 44.f }, col::ink, col::panel);
    g.setColour(col::ink); g.setFont(font(32.f, true, 0.12f)); g.drawText(kHeadTitle[tab], 68, 8, 360, 38, juce::Justification::left);
    g.setColour(col::yellow); g.setFont(font(12.f, false, 0.32f)); g.drawText("STUDIO 87", 70, 46, 200, 16, juce::Justification::left);
    g.setColour(col::line); g.drawHorizontalLine(78, 14.f, (float) IW - 14.f); g.drawVerticalLine(852, 98.f, 142.f);
    g.setColour(col::dim); g.setFont(font(10.5f, false, 0.16f)); g.drawText("MAIN PRESET", 858, 100, 84, 36, juce::Justification::centredRight);
    for (auto& p : panels)
    {
        g.setColour(juce::Colour(0xff131418)); g.fillRoundedRectangle(p.r.toFloat(), 4.f); g.setColour(col::line); g.drawRoundedRectangle(p.r.toFloat().reduced(0.5f), 4.f, 1.f);
        g.setColour(col::yellow); g.fillEllipse((float) p.r.getX() + 10.f, (float) p.r.getY() + 8.f, 20.f, 20.f);
        g.setColour(juce::Colour(0xff111111)); g.setFont(font(13.f, true)); g.drawText(juce::String(p.num), p.r.getX() + 10, p.r.getY() + 8, 20, 20, juce::Justification::centred);
        const auto tf = font(13.5f, true, 0.16f); g.setColour(col::ink); g.setFont(tf); g.drawText(p.title, p.r.getX() + 38, p.r.getY() + 8, 300, 20, juce::Justification::left);
        g.setColour(col::dim); g.setFont(font(12.5f)); g.drawText(p.sub, p.r.getX() + 38 + (int) strW(tf, p.title) + 12, p.r.getY() + 8, p.r.getWidth() - 60 - (int) strW(tf, p.title), 20, juce::Justification::left);
    }
    {
        const auto gr = wm::fx::parseGraph(proc.getGraph().toStdString()); juce::String sig = "Signal: ";
        static const char* nm[4] = { "Input FX", "Reverb", "Delay", "Wet FX" }; static const char* on[4] = { "ifx_on", "rev_on", "del_on", "wfx_on" };
        for (int s = 0; s < gr.stages; ++s)
        {
            if (s) sig << " > ";
            for (int j = 0; j < gr.count[s]; ++j) { const int n = gr.items[s][j] & 3; if (j) sig << " | "; sig << nm[n] << (proc.get(on[n]) > .5f ? "" : " (off)"); }
        }
        g.setColour(col::dim); g.setFont(font(13.f, false, 0.03f)); g.drawText(sig, 600, 168, 484, 20, juce::Justification::right);
        const bool off = (tab == 0 && proc.get("rev_on") < .5f) || (tab == 1 && proc.get("del_on") < .5f);
        if (off) { g.setColour(juce::Colour(0xffff7bb1)); g.setFont(font(13.f, false, 0.03f)); g.drawText(tab == 0 ? "The reverb is switched off in the routing." : "The delay is switched off in the routing.", 330, 152, 300, 18, juce::Justification::left); }
    }
    g.setColour(col::line); g.drawHorizontalLine(884, 14.f, (float) IW - 14.f);
    g.setColour(col::ink); g.setFont(font(12.f, true, 0.16f)); g.drawText("STUDIO 87", 14, 892, 80, 18, juce::Justification::left);
    g.setColour(col::dim); g.setFont(font(12.f, false, 0.16f)); g.drawText("SHIMMERSTUFF", 98, 892, 300, 18, juce::Justification::left);
    g.drawText("WINDOW " + juce::String(winPct) + " %", IW - 214, 892, 200, 18, juce::Justification::right);
}

// ---------------------------------------------------------------- editor
ShimmerstuffEditor::ShimmerstuffEditor(ShimmerstuffProcessor& p) : AudioProcessorEditor(&p), proc(p), body(p)
{
    addAndMakeVisible(body); body.tooltipWindow = &tooltip;
    tooltip.setMillisecondsBeforeTipAppears(p.bubbles() ? 1000 : 3600000);
    constrainer.setMinimumSize(baseW / 2, baseH / 2); constrainer.setMaximumSize(baseW * 2, baseH * 2); constrainer.setFixedAspectRatio((double) baseW / (double) baseH);
    setConstrainer(&constrainer); setResizable(true, false);
    border.setBorderThickness(juce::BorderSize<int>(6)); addAndMakeVisible(border);
    setSize(baseW, baseH);
    gate.onFrame = [this] { body.tickFrame(); };
    gate.start(30);
}
void ShimmerstuffEditor::paint(juce::Graphics& g)
{
    g.fillAll(ss::col::ground);
    const float s = (float) getWidth() / (float) baseW;
    catui::drawWoodCheeks(g, getLocalBounds(), juce::roundToInt((float) cheek * s));
}
void ShimmerstuffEditor::resized()
{
    const float s = (float) getWidth() / (float) baseW;
    body.setBounds(0, 0, ss::Body::IW, ss::Body::IH);
    body.setTransform(juce::AffineTransform::scale(s).translated((float) cheek * s, 0.f));
    body.winPct = juce::roundToInt(s * 100.f);
    border.setBounds(getLocalBounds());
}
void ShimmerstuffEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    const int c = juce::roundToInt((float) cheek * (float) getWidth() / (float) baseW);
    if (e.x < c || e.x > getWidth() - c) setSize(baseW, baseH);
}
