#pragma once
#include "PluginProcessor.h"
#include "DecayMap.h"
#include <functional>
#include <catui/CatLookAndFeel.h>
#include <catui/RenderGate.h>
#include <catui/Logo.h>
#include <catui/SodaPop.h>

// The editor is the approved design mockup (docs/mockup/shimmerstuff.html) built in JUCE: Look B, Purple Soda Pop, with the PurrOne wood cheeks.
// All the controls live in one Body component laid out at a fixed design size (1300 x 924); the editor scales it for resizing.
// The three tabs are only editing views: the routing and the on/off switches stay active whichever tab is open.
namespace ss
{
namespace col
{
const juce::Colour ground(0xff0b0b0d), panel(0xff15161a), panel2(0xff1c1d22), line(0xff2b2d34), ink(0xfff4f5f7), dim(0xffa9adb8), disp(0xff06080a),
    mint(0xff02EBBA), pink(0xffFF1383), purple(0xffA43DFF), lilac(0xffCD92FE), yellow(0xffFFD100), blue(0xff6CC9F5), orange(0xffF5A66E), cyan(0xff01FCDC), sel(0xff241238);
}

juce::Font font(float size, bool bold = false, float kern = 0.f);
juce::Font mono(float size);

class ShimLook : public juce::LookAndFeel_V4
{
public:
    ShimLook();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool hi, bool down) override;
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return font(12.5f, true, 0.09f); }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return mono(13.f); }
    juce::Font getPopupMenuFont() override { return mono(13.f); }
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground(juce::Graphics&, int w, int h) override;
};

struct Knob : public juce::Component
{
    enum Kind { Pct, Size, Ms, Hz, St, Db, Plain, Int, Wave, Dec, Spec };
    std::function<int()> typeFn;              // Dec only: the reverb type, because the same decay value is a different time per type
    juce::Slider s { juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox };
    juce::Label name, val;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a;
    juce::AudioProcessorValueTreeState* apv = nullptr;
    Knob();
    void rebind(const juce::String& id);      // point the knob at another parameter (the Modulation tab edits one effect at a time)
    void init(juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& nm, Kind k, juce::Colour accent, const juce::String& tip);
    void resized() override;
};

struct Combo : public juce::Component
{
    juce::ComboBox c; juce::Label l;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> a;
    void init(juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& nm, const juce::StringArray& items, const juce::String& tip);
    void resized() override { l.setBounds(0, 0, getWidth(), 14); c.setBounds(0, 15, getWidth(), getHeight() - 15); }
};

// Decay view: the measured energy decay curve of the real reverb with handles: yellow Decay (sideways) and Size (up and down), cyan pre-delay,
// blue damping, orange shimmer amount (on the curve), pink mix on the right.
class DecayView : public juce::Component
{
public:
    explicit DecayView(ShimmerstuffProcessor& p) : proc(p) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void tick();
    float rt60Seconds() const;
    float axis() const { return rt60Seconds() > 9.f ? 24.f : 12.f; }   // time axis: 12 s normally, 24 s for very long tails
    bool shimmerOn() const { return proc.get("shim_on") > .5f; }
private:
    void measure();
    juce::Rectangle<float> plot() const;
    float curveY(float frac) const;
    juce::Point<float> handle(int i) const;
    int hit(juce::Point<float>) const;
    float signature() const;
    ShimmerstuffProcessor& proc;
    std::vector<float> edc;
    float lastSig = -1.f; int stable = 0; bool dirty = true; int pk = 0;
    int drag = -1; float startA = 0, startB = 0; juce::Point<float> startPos, startHandle, curPos;
    static constexpr int kPts = 240; static constexpr float kSeconds = 24.f;
};

// Tone view: live spectrum of the plugin input and output with draggable filter lines. mode 0 reverb, 1 delay, 2 FX, 3 modulation (no lines).
class ToneView : public juce::Component
{
public:
    ToneView(ShimmerstuffProcessor& p, int mode);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void tick();
private:
    struct Line { const char* id; const char* name; juce::Colour c; };
    std::vector<Line> lines;
    juce::Rectangle<float> plot() const;
    float fx(float hz) const; float hzAt(float x) const;
    ShimmerstuffProcessor& proc;
    static constexpr int N = 2048;
    std::vector<float> magIn, magOut; bool any = false;
    int drag = -1;
    void spectrum(bool input, std::vector<float>& out);
};

// Movement view: the low frequency oscillator of the effect being edited on the Modulation tab, left (solid) and right (dashed).
class ModView : public juce::Component
{
public:
    explicit ModView(ShimmerstuffProcessor& p) : proc(p) {}
    void paint(juce::Graphics&) override;
private:
    ShimmerstuffProcessor& proc;
};

class EchoView : public juce::Component
{
public:
    explicit EchoView(ShimmerstuffProcessor& p) : proc(p) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
private:
    juce::Rectangle<float> plot() const;
    ShimmerstuffProcessor& proc; int drag = -1;
};

class ShapeView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ShapeView(ShimmerstuffProcessor& p) : proc(p) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
private:
    ShimmerstuffProcessor& proc;
};

// Routing graph: IN, the five processors and OUT. Drag a box between two columns (series) or onto a column (parallel); power icon bypasses it,
// arrow icon jumps to its controls; hold a number and drag up or down to change it (Shift = fine, double-click = reset).
class RouteView : public juce::Component
{
public:
    RouteView(ShimmerstuffProcessor& p, std::function<void(int)> openTab) : proc(p), openTab(std::move(openTab)) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
private:
    struct Box { int node; juce::Rectangle<float> r; };
    std::vector<Box> boxes() const;
    juce::Rectangle<float> boxRect(int stage, int idx, int count) const;
    float colX(int stage) const;
    int stageCount() const;
    static const char* nodeName(int n); static const char* nodeSub(int n); static const char* onId(int n); const char* rowId(int n, int i) const;
    static const char* rowName(int n, int i);
    int rowAt(const Box&, juce::Point<float>) const;
    ShimmerstuffProcessor& proc; std::function<void(int)> openTab;
    int dragNode = -1, valNode = -1, valRow = -1; bool moving = false; juce::Point<float> down, cur; float valStart = 0;
};

class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter(ShimmerstuffProcessor& p) : proc(p) {}
    void paint(juce::Graphics&) override;
    void tick();
    void resetPeaks() { inMax = outMax = 0.f; inHold = outHold = 0.f; clip = false; }
    int tab = 0; float rt60 = 0;
private:
    ShimmerstuffProcessor& proc; float inV = 0, outV = 0, inHold = 0, outHold = 0, inMax = 0, outMax = 0; bool clip = false;
};

// Big tab button with a title and a small subtitle
struct TabButton : public juce::Button
{
    TabButton() : juce::Button({}) { setClickingTogglesState(false); }
    juce::String title, subtitle;
    void paintButton(juce::Graphics&, bool hi, bool down) override;
};

class Body : public juce::Component
{
public:
    explicit Body(ShimmerstuffProcessor&);
    ~Body() override { setLookAndFeel(nullptr); }
    static constexpr int IW = 1300, IH = 924;
    void paint(juce::Graphics&) override;
    void setTab(int t);
    void tickFrame();
    juce::TooltipWindow* tooltipWindow = nullptr;
    int winPct = 100;
private:
    void layoutTab();
    void refreshTabPresets();
    void refreshMainPresets();
    void tabPresetChosen();
    void stepTabPreset(int d);
    void mainPresetChosen();
    void saveTabPreset();
    void saveMainPreset();
    void showSettings(); void showAbout(); void showGuide(); void showMore();
    juce::File presetRoot() const; juce::File tabDir(int t) const;
    std::vector<float> snapshot(int tab) const;
    int matchFactory(int tab) const;
    void updateTabComboText();
    ShimmerstuffProcessor& proc;
    ShimLook laf;
    int tab = 0, sub[4] = { 0, 0, 0, 0 }, editFx = 0;
    struct Pn { juce::Rectangle<int> r; int num; juce::String title, sub; };
    std::vector<Pn> panels;
    TabButton tabBtn[4]; juce::TextButton subBtn[4][2], pPrev { "<" }, pNext { ">" }, pSave { "SAVE" }, pMore { "..." }, bGuide { "GUIDE" }, bSet { "SETTINGS" }, bAbout { "ABOUT & SUPPORT" }, mainSave { "SAVE" }, turnOn { "TURN IT ON" }, peakReset { "PEAK RESET" };
    juce::ComboBox tabCombo, mainCombo;
    std::vector<int> comboIds; std::vector<juce::File> userFiles, userMain;
    std::vector<float> selSnap[4]; juce::String selLabel[4];
    DecayView decay; ToneView toneRev, toneDel, toneFx, toneMod; ModView modv; EchoView echo; RouteView route; ShapeView shape; LevelMeter meter;
    Combo rType, rMode, lay, dMode, dSrc, dRatio, dType, mType;
    Knob rk[24], dk[21], ik[4], wk[6], dukn[3], mk[15], mMix, mLevel;
    juce::TextButton shimTog { "SHIMMER ON" }, onBtn[5], revOn { "ON" }, delOn { "ON" }, holdTog { "HOLD" }, limTog { "LIMITER" }, modOn { "ON" }, chip[8];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aOn[5], aRev, aDel, aShim, aHold, aLim, aModOn, aChip[8];
    void syncEdit();                 // Modulation tab: aim the knobs at the effect chosen in "Edit effect"
    int shownEdit = -1;
    int slow = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Body)
};
}

class ShimmerstuffEditor : public juce::AudioProcessorEditor
{
public:
    explicit ShimmerstuffEditor(ShimmerstuffProcessor&);
    ~ShimmerstuffEditor() override { gate.stop(); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDoubleClick(const juce::MouseEvent&) override;   // on a wood cheek: back to 100 %
    static constexpr int cheek = 30, baseW = ss::Body::IW + 2 * cheek, baseH = ss::Body::IH;
private:
    ShimmerstuffProcessor& proc;
    catui::RenderGate gate { *this };
    ss::Body body;
    juce::TooltipWindow tooltip { this, 1000 };
    juce::ComponentBoundsConstrainer constrainer;
    juce::ResizableBorderComponent border { this, &constrainer };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShimmerstuffEditor)
};
