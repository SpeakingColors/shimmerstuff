#pragma once
#include "PluginProcessor.h"
#include <catui/CatLookAndFeel.h>
#include <catui/RenderGate.h>
#include <catui/Logo.h>
#include <catui/SodaPop.h>

namespace ss
{
namespace col
{
const juce::Colour ground(0xff0b0b0d), panel(0xff15161a), panel2(0xff1c1d22), line(0xff2b2d34), ink(0xfff4f5f7), dim(0xffa9adb8), disp(0xff06080a),
    mint(0xff02EBBA), pink(0xffFF1383), purple(0xffA43DFF), lilac(0xffCD92FE), yellow(0xffFFD100), blue(0xff6CC9F5), orange(0xffF5A66E);
}

struct Knob
{
    juce::Slider s { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label l;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a;
    void init(juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& name,
              const juce::String& suffix, const juce::String& tip, bool pct = false);
    void place(juce::Rectangle<int> r) { l.setBounds(r.removeFromTop(16)); s.setBounds(r); }
    void show(bool v) { s.setVisible(v); l.setVisible(v); }
};

struct Combo
{
    juce::ComboBox c; juce::Label l;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> a;
    void init(juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& name, const juce::StringArray& items, const juce::String& tip);
    void place(juce::Rectangle<int> r) { l.setBounds(r.removeFromTop(16)); c.setBounds(r.removeFromTop(26)); }
    void show(bool v) { c.setVisible(v); l.setVisible(v); }
};

// Decay view: the measured energy decay curve of the real reverb (purple), a yellow handle (drag sideways for Decay, up and down for Size)
// and three tracks underneath with handles for pre-delay (cyan), damping (blue) and mix (pink).
class DecayView : public juce::Component, private juce::Timer
{
public:
    explicit DecayView(ShimmerstuffProcessor& p);
    ~DecayView() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    void measure();
    juce::Rectangle<float> plot() const;
    juce::Rectangle<float> track(int i) const;
    float rt60() const;
    ShimmerstuffProcessor& proc;
    std::vector<float> edc;              // dB, 0 .. -80, kPts points over kSeconds
    float lastSig = -1.f; int stable = 0; bool dirty = true;
    int drag = -1; float startDecay = 0, startSize = 0; juce::Point<float> startPos;
    juce::String dragId;
    static constexpr int kPts = 240; static constexpr float kSeconds = 10.f;
    float signature() const;
};

class EchoView : public juce::Component
{
public:
    explicit EchoView(ShimmerstuffProcessor& p) : proc(p) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    ShimmerstuffProcessor& proc; int drag = -1;
};

class ShapeView : public juce::Component, public juce::SettableTooltipClient   // eight small thumbnails of the duck shapes; click one to choose it
{
public:
    explicit ShapeView(ShimmerstuffProcessor& p) : proc(p) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
private:
    ShimmerstuffProcessor& proc;
};

// Routing graph: In, the four processors and Out. Drag a box to another place (between two columns for series, onto a column for parallel),
// click the power corner to switch it on or off, click the tab corner to jump to its controls, drag the value line up or down to change it.
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
    static const char* nodeName(int n);
    static const char* onId(int n);
    static const char* valId(int n);
    static const char* valLabel(int n);
    ShimmerstuffProcessor& proc; std::function<void(int)> openTab;
    int dragNode = -1, valNode = -1; bool moving = false; juce::Point<float> down, cur; float valStart = 0;
};

class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter(ShimmerstuffProcessor& p) : proc(p) {}
    void paint(juce::Graphics&) override;
private:
    ShimmerstuffProcessor& proc; float inV = 0, outV = 0, inHold = 0, outHold = 0;
};
}

class ShimmerstuffEditor : public juce::AudioProcessorEditor
{
public:
    explicit ShimmerstuffEditor(ShimmerstuffProcessor&);
    ~ShimmerstuffEditor() override { setLookAndFeel(nullptr); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDoubleClick(const juce::MouseEvent&) override;   // on a wood cheek: back to 100 %
    static constexpr int baseW = 1180, baseH = 780, cheek = 24;
private:
    void setTab(int t);
    void refreshPresets();
    void presetChosen();
    void savePreset();
    juce::File presetDir() const;
    ShimmerstuffProcessor& proc;
    catui::CatLookAndFeel laf;
    catui::RenderGate gate { *this };
    int tab = 0;
    juce::TextButton tabBtn[3];
    juce::ComboBox presets; juce::TextButton save { "Save" }, bypass { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aByp;
    ss::DecayView decay; ss::EchoView echo; ss::RouteView route; ss::ShapeView shape; ss::LevelMeter meter;
    // reverb knobs
    ss::Knob rk[13]; ss::Combo rType;
    // delay knobs
    ss::Knob dk[6];
    // fx knobs
    ss::Knob ik[4], wk[6], dukn[3]; ss::Combo dMode, dSrc, dRatio;
    juce::ToggleButton onBtn[4]; std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aOn[4];
    juce::Label hint;
    juce::TooltipWindow tooltip { this, 1000 };
    juce::ComponentBoundsConstrainer constrainer;
    juce::ResizableBorderComponent border;
    std::vector<juce::File> userFiles;
    juce::Rectangle<int> displayR, knobR, meterR;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShimmerstuffEditor)
};
