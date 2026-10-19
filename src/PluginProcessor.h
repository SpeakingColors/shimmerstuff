#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Fx.h"

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

    // routing graph (not an automatable parameter: it is part of the saved state)
    juce::String getGraph() const;
    void setGraph(const juce::String&);

    // meters, written by the audio thread
    std::atomic<float> inPk { 0.f }, outPk { 0.f }, duckGain { 1.f };
    std::atomic<int> sidechainOn { 0 };

    static const juce::StringArray& typeNames();
    static const juce::StringArray& ratioNames();
    static constexpr float kRatioBeats[7] = { 0.25f, 0.5f, 1.f, 2.f, 4.f, 8.f, 16.f };
    // main presets: whole plugin (reverb, delay, FX and routing together)
    void applyUserPresetXml(const juce::XmlElement&);
    juce::XmlElement* makePresetXml() const;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    wm::fx::Engine eng;
    std::atomic<uint64_t> graphCode { 0 };
    juce::String graphStr = "ifx>rev>del>wfx";
    wm::fx::Graph graph;
    int program = 0;
    double fs = 48000.0;
    float sm[12] = {};          // smoothed values
    float smDt = 450.f;
    bool smInit = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShimmerstuffProcessor)
};
