#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP.h"

class GateDaddyProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numGateSteps = 16;
    static constexpr int scopeBins = 256;

    GateDaddyProcessor();
    ~GateDaddyProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Gate Daddy"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Shaper curve (edited on the message thread, read by audio via the table)
    gd::Curve getCurve() const;
    void setCurve (const gd::Curve& c);
    std::atomic<int> curveVersion { 0 }; // bumps whenever the curve changes

    void loadPreset (int index);

    juce::AudioProcessorValueTreeState apvts;
    gd::CurveTable curveTable;

    // Live values for the GUI
    std::atomic<float> uiShaperPhase { 0.0f }, uiGateStep { -1.0f }, uiGateGain { 1.0f };
    std::atomic<float> uiLfoPhase[2] {}, uiLfoValue[2] {};
    std::atomic<float> uiSidechainLevel { 0.0f }, uiDuck { 0.0f }, uiTotalGain { 1.0f };
    std::atomic<float> uiInLevel { 0.0f }, uiOutLevel { 0.0f }, uiBpm { 120.0f };
    std::atomic<bool> uiHostSynced { false }, uiSidechainConnected { false };
    std::array<std::atomic<float>, scopeBins> scope {}, scopeOut {}, scopeSc {};
    std::atomic<float> uiTrigFlash { 0.0f };
    std::atomic<int> uiShaperMode { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct Params;
    std::unique_ptr<Params> p;

    mutable juce::CriticalSection curveLock;
    gd::Curve curve = gd::curves::pump();
    int currentPreset = 0;

    double sr = 44100.0;
    double internalPpq = 0.0;
    bool wasPlaying = false;

    float shaperGain = 1.0f, gateGain = 1.0f, scEnv = 0.0f, lfoVolGain = 1.0f;
    float smoothMix = 1.0f, smoothOut = 1.0f;
    gd::Lfo lfos[2];
    gd::Svf filters[2];
    juce::dsp::LinkwitzRileyFilter<float> xoverLow, xoverHigh, xoverLowDry, xoverHighDry, duckLow, duckHigh;
    float tEnvFastA = 0, tEnvSlowA = 0, tEnvFastR = 0, tEnvSlowR = 0;
    float trigEnv = 0.0f, trigFlash = 0.0f, duckGain = 1.0f;
    bool trigArmed = true;
    int trigHoldoff = 0;
    double trigPhase = 1.0, freeShaperPhase = 0.0;
    int scopeIndex = -1;
    float scopePeak = 0.0f, scopePeakOut = 0.0f, scopePeakSc = 0.0f;
    int filterUpdateCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GateDaddyProcessor)
};
