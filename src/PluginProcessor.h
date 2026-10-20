#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Fx.h"
#include "Presets.h"
#include <array>

// shimmerstuff: shimmer reverb, shimmer delay, input FX, wet FX and a free routing graph in one plugin.
class ShimmerstuffProcessor : public juce::AudioProcessor
{
public:
    ShimmerstuffProcessor();
    void prepareToPlay(double sampleRate, int blockSize) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "shimmerstuff"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return program; }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    float get(const juce::String& id) const { return apvts.getRawParameterValue(id)->load(); }
    void setParam(const juce::String& id, float v);

    // routing graph: the host-automatable "route" choice picks one of the eight ready-made layouts or "Custom"; a custom graph is part of the saved state
    static constexpr int kLayouts = 8;
    static const char* layoutGraph(int i);
    static const juce::StringArray& layoutNames();
    juce::String getGraph() const;              // the graph in effect (layout or custom)
    void setGraph(const juce::String&);         // picks the matching layout, or stores a custom graph and selects Custom

    // meters, written by the audio thread
    std::atomic<float> inPk { 0.f }, outPk { 0.f }, duckGain { 1.f };
    std::atomic<int> sidechainOn { 0 };

    static const juce::StringArray& typeNames();
    static const juce::StringArray& ratioNames();
    static constexpr float kRatioBeats[7] = { 0.25f, 0.5f, 1.f, 2.f, 4.f, 8.f, 16.f };
    // presets. Tab presets set only the controls of their tab (0 reverb, 1 delay, 2 FX and routing); main presets set the whole plugin.
    int numTabPresets(int tab) const;
    juce::String tabPresetName(int tab, int i) const;
    void applyTabPreset(int tab, int i);
    int numMainPresets() const;
    juce::String mainPresetName(int i) const;
    void applyMainPreset(int i);
    void applyUserPresetXml(const juce::XmlElement&);
    juce::XmlElement* makePresetXml(int tab = -1) const;     // tab -1 = the whole plugin
    int lastTabPreset[3] = { 0, 0, 0 };
    // audio taps for the spectrum view: the last 4096 mono samples of the input and the output
    static constexpr int kTap = 4096;
    std::array<std::atomic<float>, kTap> tapIn, tapOut;
    std::atomic<int> tapPos { 0 };
    std::atomic<float> rms { 0.f };
    bool bubbles() const { return (bool) apvts.state.getProperty("bubbles", true); }
    void setBubbles(bool b) { apvts.state.setProperty("bubbles", b, nullptr); }
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    wm::fx::Engine eng;
    std::atomic<uint64_t> graphCode { 0 };
    uint64_t layoutCode[kLayouts] = {};
    juce::String graphStr = "ifx>rev>del>wfx";
    wm::fx::Graph graph;
    void applyKeys(const std::vector<const char*>&, const ssp::Pre*, bool onOff);
    void resetKeys(const std::vector<const char*>&, bool onOff);
    int program = 0;
    double fs = 48000.0;
    float sm[12] = {};          // smoothed values
    float smDt = 450.f;
    bool smInit = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShimmerstuffProcessor)
};
