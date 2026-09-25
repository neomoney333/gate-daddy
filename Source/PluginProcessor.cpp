#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
// Cached raw parameter pointers so the audio thread never does string lookups.
struct GateDaddyProcessor::Params
{
    explicit Params (APVTS& s)
    {
        auto get = [&s] (const juce::String& id) { return s.getRawParameterValue (id); };
        mix = get ("mix"); outGain = get ("outGain"); drive = get ("drive"); width = get ("width");
        bandMode = get ("bandMode"); xover = get ("xover"); bpm = get ("bpm"); bypass = get ("denial");
        shaperOn = get ("shaperOn"); shaperRate = get ("shaperRate"); shaperDepth = get ("shaperDepth");
        shaperSmooth = get ("shaperSmooth"); shaperOffset = get ("shaperOffset");
        shaperMode = get ("shaperMode"); shaperHz = get ("shaperHz"); shaperHold = get ("shaperHold");
        trigThresh = get ("trigThresh");
        transOn = get ("transOn"); transProfile = get ("transProfile"); transAmount = get ("transAmount");
        transAttack = get ("transAttack"); transSustain = get ("transSustain");
        duckOn = get ("duckOn"); duckFreq = get ("duckFreq"); duckAmount = get ("duckAmount"); duckSource = get ("duckSource");
        gateOn = get ("gateOn"); gateRate = get ("gateRate"); gateSteps = get ("gateSteps");
        gateLength = get ("gateLength"); gateSwing = get ("gateSwing"); gateAttack = get ("gateAttack");
        gateRelease = get ("gateRelease"); gateDepth = get ("gateDepth");
        for (int i = 0; i < numGateSteps; ++i)
            steps[(size_t) i] = get ("step" + juce::String (i + 1));
        scOn = get ("scOn"); scSource = get ("scSource"); scThresh = get ("scThresh");
        scAmount = get ("scAmount"); scAttack = get ("scAttack"); scRelease = get ("scRelease");
        filtOn = get ("filtOn"); filtType = get ("filtType"); cutoff = get ("cutoff"); reso = get ("reso");
        for (int i = 0; i < 2; ++i)
        {
            const juce::String n = "lfo" + juce::String (i + 1);
            lfo[i].on = get (n + "On"); lfo[i].shape = get (n + "Shape"); lfo[i].sync = get (n + "Sync");
            lfo[i].rate = get (n + "Rate"); lfo[i].hz = get (n + "Hz"); lfo[i].depth = get (n + "Depth");
            lfo[i].target = get (n + "Target");
        }
    }

    using P = std::atomic<float>*;
    P mix, outGain, drive, width, bandMode, xover, bpm, bypass;
    P shaperOn, shaperRate, shaperDepth, shaperSmooth, shaperOffset, shaperMode, shaperHz, shaperHold, trigThresh;
    P transOn, transProfile, transAmount, transAttack, transSustain;
    P duckOn, duckFreq, duckAmount, duckSource;
    P gateOn, gateRate, gateSteps, gateLength, gateSwing, gateAttack, gateRelease, gateDepth;
    std::array<P, numGateSteps> steps;
    P scOn, scSource, scThresh, scAmount, scAttack, scRelease;
    P filtOn, filtType, cutoff, reso;
    struct { P on, shape, sync, rate, hz, depth, target; } lfo[2];
};

//==============================================================================
APVTS::ParameterLayout GateDaddyProcessor::createLayout()
{
    using namespace juce;
    APVTS::ParameterLayout layout;

    auto pct = [] (const String& id, const String& name, float lo, float hi, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.1f), def,
                                                      AudioParameterFloatAttributes().withLabel ("%"));
    };
    auto ms = [] (const String& id, const String& name, float lo, float hi, float def)
    {
        NormalisableRange<float> r (lo, hi, 0.01f);
        r.setSkewForCentre (lo + (hi - lo) * 0.12f);
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                                                      AudioParameterFloatAttributes().withLabel ("ms"));
    };
    auto onOff = [] (const String& id, const String& name, bool def)
    {
        return std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def);
    };
    auto choice = [] (const String& id, const String& name, const StringArray& items, int def)
    {
        return std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def);
    };

    // Global
    layout.add (pct ("mix", "Mix", 0, 100, 100));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "outGain", 1 }, "Output",
                                                       NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (pct ("drive", "Drive", 0, 100, 0));
    layout.add (pct ("width", "Width", 0, 200, 100));
    layout.add (choice ("bandMode", "Band", { "FULL RANGE", "LOWS ONLY", "HIGHS ONLY" }, 0));
    {
        NormalisableRange<float> r (40.0f, 2000.0f, 1.0f);
        r.setSkewForCentre (200.0f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "xover", 1 }, "Crossover", r, 150.0f,
                                                           AudioParameterFloatAttributes().withLabel ("Hz")));
    }
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "bpm", 1 }, "Standalone BPM",
                                                       NormalisableRange<float> (60.0f, 200.0f, 0.1f), 124.0f));
    layout.add (onOff ("denial", "Denial Mode (Bypass)", false));

    // Shaper
    layout.add (onOff ("shaperOn", "Shaper On", true));
    layout.add (choice ("shaperRate", "Shaper Rate", gd::noteRateNames(), gd::rateIndex ("1/4")));
    layout.add (pct ("shaperDepth", "Shaper Depth", 0, 100, 100));
    layout.add (ms ("shaperSmooth", "Shaper Smooth", 0.0f, 20.0f, 1.0f));
    layout.add (pct ("shaperOffset", "Shaper Offset", 0, 100, 0));
    layout.add (choice ("shaperMode", "Shaper Mode", { "SYNC", "FREE HZ", "INPUT TRIG", "SC TRIG" }, 0));
    {
        NormalisableRange<float> hz (0.05f, 30.0f, 0.01f);
        hz.setSkewForCentre (2.0f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "shaperHz", 1 }, "Shaper Hz", hz, 2.0f,
                                                           AudioParameterFloatAttributes().withLabel ("Hz")));
    }
    layout.add (onOff ("shaperHold", "Shaper Hold End", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "trigThresh", 1 }, "Trigger Threshold",
                                                       NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -18.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));

    // Transient shaper (first in chain)
    layout.add (onOff ("transOn", "Transient On", false));
    layout.add (choice ("transProfile", "Transient Profile", { "NEUTRAL PUNCH", "HARD SNAP", "SOFT BODY", "TIGHT TAIL" }, 0));
    layout.add (pct ("transAmount", "Transient Amount", 0, 100, 50));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "transAttack", 1 }, "Transient Attack",
                                                       NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 4.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "transSustain", 1 }, "Transient Sustain",
                                                       NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));

    // Low-end duck
    layout.add (onOff ("duckOn", "Low Duck On", false));
    {
        NormalisableRange<float> r (30.0f, 500.0f, 1.0f);
        r.setSkewForCentre (120.0f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "duckFreq", 1 }, "Low Duck Freq", r, 150.0f,
                                                           AudioParameterFloatAttributes().withLabel ("Hz")));
    }
    layout.add (pct ("duckAmount", "Low Duck Amount", 0, 100, 60));
    layout.add (choice ("duckSource", "Low Duck Source", { "CURVE", "SIDECHAIN" }, 0));

    // Gate
    layout.add (onOff ("gateOn", "Gate On", false));
    layout.add (choice ("gateRate", "Gate Rate", gd::noteRateNames(), gd::rateIndex ("1/16")));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "gateSteps", 1 }, "Gate Steps", 1, numGateSteps, numGateSteps));
    layout.add (pct ("gateLength", "Gate Length", 5, 100, 75));
    layout.add (pct ("gateSwing", "Gate Swing", 0, 75, 0));
    layout.add (ms ("gateAttack", "Gate Attack", 0.1f, 50.0f, 1.0f));
    layout.add (ms ("gateRelease", "Gate Release", 1.0f, 300.0f, 20.0f));
    layout.add (pct ("gateDepth", "Gate Depth", 0, 100, 100));
    for (int i = 0; i < numGateSteps; ++i)
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "step" + String (i + 1), 1 }, "Gate Step " + String (i + 1),
                                                           NormalisableRange<float> (0.0f, 1.0f), 1.0f));

    // Sidechain
    layout.add (onOff ("scOn", "Sidechain On", false));
    layout.add (choice ("scSource", "Sidechain Source", { "EXTERNAL", "SELF" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "scThresh", 1 }, "SC Threshold",
                                                       NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -24.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (pct ("scAmount", "SC Amount", 0, 100, 80));
    layout.add (ms ("scAttack", "SC Attack", 0.1f, 50.0f, 1.0f));
    layout.add (ms ("scRelease", "SC Release", 10.0f, 1000.0f, 150.0f));

    // Filter
    layout.add (onOff ("filtOn", "Filter On", false));
    layout.add (choice ("filtType", "Filter Type", { "LOW PASS", "HIGH PASS", "BAND PASS" }, 0));
    {
        NormalisableRange<float> r (20.0f, 20000.0f, 1.0f);
        r.setSkewForCentre (1000.0f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "cutoff", 1 }, "Cutoff", r, 20000.0f,
                                                           AudioParameterFloatAttributes().withLabel ("Hz")));
        NormalisableRange<float> q (0.5f, 12.0f, 0.01f);
        q.setSkewForCentre (2.0f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "reso", 1 }, "Resonance", q, 0.707f));
    }

    // LFOs
    for (int i = 1; i <= 2; ++i)
    {
        const String n = "lfo" + String (i), N = "LFO " + String (i) + " ";
        layout.add (onOff (n + "On", N + "On", false));
        layout.add (choice (n + "Shape", N + "Shape", gd::lfoShapeNames(), 0));
        layout.add (onOff (n + "Sync", N + "Sync", true));
        layout.add (choice (n + "Rate", N + "Rate", gd::noteRateNames(), gd::rateIndex ("1/1")));
        NormalisableRange<float> hz (0.02f, 20.0f, 0.01f);
        hz.setSkewForCentre (1.0f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { n + "Hz", 1 }, N + "Hz", hz, 1.0f,
                                                           AudioParameterFloatAttributes().withLabel ("Hz")));
        layout.add (pct (n + "Depth", N + "Depth", -100, 100, 50));
        layout.add (choice (n + "Target", N + "Target", gd::lfoTargetNames(), i == 1 ? 0 : 2));
    }

    return layout;
}

//==============================================================================
GateDaddyProcessor::GateDaddyProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GateDaddy", createLayout())
{
    p = std::make_unique<Params> (apvts);
    curveTable.fill (curve);
    xoverLow.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    xoverHigh.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    xoverLowDry.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    xoverHighDry.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    duckLow.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    duckHigh.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
}

GateDaddyProcessor::~GateDaddyProcessor() = default;

bool GateDaddyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto main = layouts.getMainOutputChannelSet();
    if (main != juce::AudioChannelSet::mono() && main != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != main)
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void GateDaddyProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    xoverLow.prepare (spec);
    xoverHigh.prepare (spec);
    for (auto* f : { &xoverLowDry, &xoverHighDry, &duckLow, &duckHigh })
        f->prepare (spec);
    tEnvFastA = tEnvSlowA = tEnvFastR = tEnvSlowR = trigEnv = 0.0f;
    trigPhase = 1.0;
    duckGain = 1.0f;
    for (auto& f : filters)
        f.reset();
    shaperGain = gateGain = lfoVolGain = 1.0f;
    scEnv = 0.0f;
    smoothMix = p->mix->load() * 0.01f;
    smoothOut = juce::Decibels::decibelsToGain (p->outGain->load());
    filterUpdateCounter = 0;
    internalPpq = 0.0;
}

//==============================================================================
namespace
{
    inline float onePoleCoef (float ms, double sr)
    {
        return ms <= 0.0f ? 1.0f : 1.0f - std::exp (-1.0f / (ms * 0.001f * (float) sr));
    }

    inline double frac (double x) { return x - std::floor (x); }
}

void GateDaddyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto main = getBusBuffer (buffer, true, 0);
    const int numCh = juce::jmin (main.getNumChannels(), 2);
    const int n = main.getNumSamples();
    if (numCh == 0 || n == 0)
        return;

    // Sidechain bus (optional)
    juce::AudioBuffer<float> sc;
    bool haveSc = false;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        sc = getBusBuffer (buffer, true, 1);
        haveSc = sc.getNumChannels() > 0;
    }
    uiSidechainConnected = haveSc;

    //--------------------------------------------------------------------------
    // Clock: follow host transport when it's rolling, otherwise free-run.
    double bpm = p->bpm->load();
    bool hostTempo = false, playing = false;
    double blockPpq = internalPpq;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) { bpm = *b; hostTempo = true; }
            playing = pos->getIsPlaying();
            if (playing)
                if (auto q = pos->getPpqPosition())
                    blockPpq = *q;
        }
    }
    bpm = juce::jlimit (20.0, 999.0, bpm);
    const double ppqPerSample = bpm / 60.0 / sr;
    internalPpq = blockPpq + ppqPerSample * n;
    if (internalPpq > 1.0e7)
        internalPpq = 0.0;
    wasPlaying = playing;

    //--------------------------------------------------------------------------
    // Snapshot parameters once per block
    const bool bypassed = p->bypass->load() > 0.5f;
    const bool shaperOn = p->shaperOn->load() > 0.5f;
    const double shaperBeats = gd::noteRates()[(size_t) (int) p->shaperRate->load()].beats;
    const float shaperDepth = p->shaperDepth->load() * 0.01f;
    const float shaperCoef = onePoleCoef (p->shaperSmooth->load(), sr);
    const double shaperOffset = p->shaperOffset->load() * 0.01;

    const int shaperMode = (int) p->shaperMode->load();
    const double shaperHz = p->shaperHz->load();
    const bool shaperHold = p->shaperHold->load() > 0.5f;
    const float trigThresh = p->trigThresh->load();
    const float trigAtk = onePoleCoef (0.2f, sr), trigRel = onePoleCoef (30.0f, sr);
    const int holdoffSamples = (int) (0.045 * sr);

    const bool transOn = p->transOn->load() > 0.5f;
    const float transAmt = p->transAmount->load() * 0.01f;
    const float transAtkDb = p->transAttack->load() * transAmt;
    const float transSusDb = p->transSustain->load() * transAmt;
    static constexpr float profiles[4][4] = { // fastAtk, slowAtk, fastRel, slowRel (ms)
        { 0.5f, 20.0f, 40.0f, 250.0f }, { 0.2f, 10.0f, 25.0f, 150.0f },
        { 2.0f, 40.0f, 60.0f, 400.0f }, { 0.5f, 15.0f, 20.0f, 80.0f } };
    const auto& prof = profiles[juce::jlimit (0, 3, (int) p->transProfile->load())];
    const float tFastA = onePoleCoef (prof[0], sr), tSlowA = onePoleCoef (prof[1], sr);
    const float tFastR = onePoleCoef (prof[2], sr), tSlowR = onePoleCoef (prof[3], sr);
    const float tRel = onePoleCoef (60.0f, sr);

    const bool duckOn = p->duckOn->load() > 0.5f;
    const float duckAmount = p->duckAmount->load() * 0.01f;
    const bool duckFromSc = p->duckSource->load() > 0.5f;
    if (duckOn)
    {
        duckLow.setCutoffFrequency (p->duckFreq->load());
        duckHigh.setCutoffFrequency (p->duckFreq->load());
    }

    const bool gateOn = p->gateOn->load() > 0.5f;
    const double gateBeats = gd::noteRates()[(size_t) (int) p->gateRate->load()].beats;
    const int gateSteps = juce::jlimit (1, numGateSteps, (int) p->gateSteps->load());
    const double gateLen = p->gateLength->load() * 0.01;
    const double swing = p->gateSwing->load() * 0.01;
    // Gate times are "fade fully in/out" times (~40 dB), not one-pole time constants.
    const float gateAtk = onePoleCoef (p->gateAttack->load() / 4.6f, sr);
    const float gateRel = onePoleCoef (p->gateRelease->load() / 4.6f, sr);
    const float gateDepth = p->gateDepth->load() * 0.01f;
    std::array<float, numGateSteps> stepLevels;
    for (int i = 0; i < numGateSteps; ++i)
        stepLevels[(size_t) i] = p->steps[(size_t) i]->load();

    const bool scOn = p->scOn->load() > 0.5f;
    const bool scExternal = p->scSource->load() < 0.5f && haveSc;
    const float scThresh = p->scThresh->load();
    const float scAmount = p->scAmount->load() * 0.01f;
    const float scAtk = onePoleCoef (p->scAttack->load(), sr);
    const float scRel = onePoleCoef (p->scRelease->load(), sr);

    const bool filtOn = p->filtOn->load() > 0.5f;
    const int filtType = (int) p->filtType->load();
    const float baseCutoff = p->cutoff->load();
    const float baseReso = p->reso->load();

    struct LfoSnap { bool on, sync; gd::LfoShape shape; double beats; float hz, depth; int target; };
    LfoSnap ls[2];
    for (int i = 0; i < 2; ++i)
        ls[i] = { p->lfo[i].on->load() > 0.5f, p->lfo[i].sync->load() > 0.5f, (gd::LfoShape) (int) p->lfo[i].shape->load(),
                  gd::noteRates()[(size_t) (int) p->lfo[i].rate->load()].beats, p->lfo[i].hz->load(),
                  p->lfo[i].depth->load() * 0.01f, (int) p->lfo[i].target->load() };

    const float baseDrive = p->drive->load() * 0.01f;
    const float baseWidth = p->width->load() * 0.01f;
    const int bandMode = (int) p->bandMode->load();
    const float targetMix = bypassed ? 0.0f : p->mix->load() * 0.01f;
    const float targetOut = bypassed ? 1.0f : juce::Decibels::decibelsToGain (p->outGain->load());
    const float paramCoef = onePoleCoef (20.0f, sr);
    const float volCoef = onePoleCoef (2.0f, sr);

    if (bandMode != 0)
    {
        xoverLow.setCutoffFrequency (p->xover->load());
        xoverHigh.setCutoffFrequency (p->xover->load());
        xoverLowDry.setCutoffFrequency (p->xover->load());
        xoverHighDry.setCutoffFrequency (p->xover->load());
    }

    float* ch[2] = { main.getWritePointer (0), main.getWritePointer (numCh > 1 ? 1 : 0) };
    const float* scCh[2] = { nullptr, nullptr };
    if (haveSc)
    {
        scCh[0] = sc.getReadPointer (0);
        scCh[1] = sc.getReadPointer (sc.getNumChannels() > 1 ? 1 : 0);
    }

    float inPeak = 0.0f, outPeak = 0.0f, scPeak = 0.0f, gainSum = 0.0f;
    float lastShaperPhase = 0.0f, lastLfoPhase[2] {}, lastLfoVal[2] {};
    int lastStep = -1;

    for (int s = 0; s < n; ++s)
    {
        const double ppq = blockPpq + ppqPerSample * s;

        // --- Transient shaper detectors (stereo-linked) ---------------------------
        const float inAbs = juce::jmax (std::abs (ch[0][s]), std::abs (ch[1][s]));
        float transGain = 1.0f;
        if (transOn)
        {
            tEnvFastA += (inAbs - tEnvFastA) * (inAbs > tEnvFastA ? tFastA : tRel);
            tEnvSlowA += (inAbs - tEnvSlowA) * (inAbs > tEnvSlowA ? tSlowA : tRel);
            tEnvFastR += (inAbs - tEnvFastR) * (inAbs > tEnvFastR ? tFastA : tFastR);
            tEnvSlowR += (inAbs - tEnvSlowR) * (inAbs > tEnvSlowR ? tFastA : tSlowR);
            constexpr float floorDb = -80.0f;
            const float attackDiff = juce::Decibels::gainToDecibels (tEnvFastA, floorDb) - juce::Decibels::gainToDecibels (tEnvSlowA, floorDb);
            const float sustainDiff = juce::Decibels::gainToDecibels (tEnvSlowR, floorDb) - juce::Decibels::gainToDecibels (tEnvFastR, floorDb);
            const float gDb = transAtkDb * juce::jlimit (0.0f, 1.0f, attackDiff / 9.0f)
                            + transSusDb * juce::jlimit (0.0f, 1.0f, sustainDiff / 9.0f);
            transGain = juce::Decibels::decibelsToGain (gDb);
        }

        // --- Trigger detector (INPUT TRIG / SC TRIG) -------------------------------
        if (shaperMode >= 2)
        {
            const float det = (shaperMode == 3 && haveSc) ? juce::jmax (std::abs (scCh[0][s]), std::abs (scCh[1][s])) : inAbs;
            trigEnv += (det - trigEnv) * (det > trigEnv ? trigAtk : trigRel);
            const float envDb = juce::Decibels::gainToDecibels (trigEnv, -100.0f);
            if (trigHoldoff > 0)
                --trigHoldoff;
            if (trigArmed && envDb > trigThresh && trigHoldoff == 0)
            {
                trigPhase = 0.0;
                trigArmed = false;
                trigHoldoff = holdoffSamples;
                trigFlash = 1.0f;
            }
            else if (! trigArmed && envDb < trigThresh - 6.0f)
                trigArmed = true;
        }

        // --- Shaper -----------------------------------------------------------
        float shaperPhase;
        if (shaperMode == 0)
            shaperPhase = (float) frac (ppq / shaperBeats + shaperOffset);
        else if (shaperMode == 1)
        {
            freeShaperPhase = frac (freeShaperPhase + shaperHz / sr);
            shaperPhase = (float) frac (freeShaperPhase + shaperOffset);
        }
        else
        {
            trigPhase += ppqPerSample / shaperBeats;
            if (trigPhase >= 1.0)
                trigPhase = shaperHold ? 1.0 : frac (trigPhase);
            shaperPhase = (float) juce::jmin (1.0, trigPhase);
        }
        const float rawCurve = curveTable.lookup (shaperPhase);
        const float shaperTarget = shaperOn ? 1.0f - shaperDepth * (1.0f - rawCurve) : 1.0f;
        shaperGain += (shaperTarget - shaperGain) * shaperCoef;
        lastShaperPhase = shaperPhase;

        // --- Gate (with swing: odd steps start late) ---------------------------
        float gateOut = 1.0f;
        {
            const double pairLen = gateBeats * 2.0;
            const double pairPos = frac (ppq / pairLen) * pairLen;
            const double evenLen = gateBeats * (1.0 + swing);
            const bool odd = pairPos >= evenLen;
            const double posInStep = odd ? pairPos - evenLen : pairPos;
            const double stepLen = odd ? gateBeats * (1.0 - swing) : evenLen;
            const auto pairIndex = (long long) std::floor (ppq / pairLen);
            const int step = (int) ((((pairIndex * 2 + (odd ? 1 : 0)) % gateSteps) + gateSteps) % gateSteps);
            const float level = stepLevels[(size_t) step];
            const float target = (gateOn && posInStep < stepLen * gateLen) ? level : (gateOn ? 0.0f : 1.0f);
            gateGain += (target - gateGain) * (target > gateGain ? gateAtk : gateRel);
            gateOut = 1.0f - gateDepth * (1.0f - gateGain);
            if (! gateOn) gateOut = 1.0f;
            lastStep = step;
        }

        // --- Sidechain envelope follower ----------------------------------------
        float scGain = 1.0f, scRaw = 0.0f;
        {
            float det;
            if (scExternal)
                det = juce::jmax (std::abs (scCh[0][s]), std::abs (scCh[1][s]));
            else
                det = juce::jmax (std::abs (ch[0][s]), std::abs (ch[1][s]));
            scEnv += (det - scEnv) * (det > scEnv ? scAtk : scRel);
            scPeak = juce::jmax (scPeak, det);
            if (scOn || (duckOn && duckFromSc))
            {
                const float envDb = juce::Decibels::gainToDecibels (scEnv, -100.0f);
                const float over = juce::jlimit (0.0f, 1.0f, (envDb - scThresh) / 12.0f);
                scRaw = over * (2.0f - over); // soft knee
            }
            if (scOn)
                scGain = 1.0f - scAmount * scRaw;
        }

        // --- LFOs ----------------------------------------------------------------
        float modCutoff = 0, modReso = 0, modPan = 0, modDrive = 0, modWidth = 0, volTarget = 1.0f;
        for (int i = 0; i < 2; ++i)
        {
            auto& L = ls[i];
            double ph;
            if (L.sync)
                ph = frac (ppq / L.beats);
            else
            {
                lfos[i].freePhase = frac (lfos[i].freePhase + L.hz / sr);
                ph = lfos[i].freePhase;
            }
            const float v = lfos[i].process (L.shape, ph, curveTable);
            lastLfoPhase[i] = (float) ph;
            lastLfoVal[i] = v;
            if (! L.on)
                continue;
            const float m = v * L.depth;
            switch ((gd::LfoTarget) L.target)
            {
                case gd::LfoTarget::cutoff:    modCutoff += m; break;
                case gd::LfoTarget::resonance: modReso += m; break;
                case gd::LfoTarget::pan:       modPan += m; break;
                case gd::LfoTarget::drive:     modDrive += m; break;
                case gd::LfoTarget::width:     modWidth += m; break;
                case gd::LfoTarget::volume:
                    volTarget *= 1.0f - std::abs (L.depth) * (0.5f - 0.5f * v * (L.depth >= 0 ? 1.0f : -1.0f));
                    break;
            }
        }
        lfoVolGain += (volTarget - lfoVolGain) * volCoef;

        const float totalGain = shaperGain * gateOut * scGain * lfoVolGain;
        const float duckTarget = duckOn ? 1.0f - duckAmount * (duckFromSc ? scRaw : 1.0f - rawCurve) : 1.0f;
        duckGain += (duckTarget - duckGain) * shaperCoef;
        gainSum += totalGain;

        // --- Filter coefficients (every 16 samples is plenty) ------------------
        if (filtOn && filterUpdateCounter-- <= 0)
        {
            filterUpdateCounter = 15;
            const float fc = baseCutoff * std::exp2 (modCutoff * 5.0f);
            const float q = juce::jlimit (0.5f, 15.0f, baseReso * std::exp2 (modReso * 2.5f));
            for (auto& f : filters)
                f.set (fc, q, sr);
        }

        smoothMix += (targetMix - smoothMix) * paramCoef;
        smoothOut += (targetOut - smoothOut) * paramCoef;

        const float driveAmt = juce::jlimit (0.0f, 1.0f, baseDrive + modDrive);
        const float pre = 1.0f + driveAmt * 9.0f;
        const float driveNorm = 1.0f / std::pow (pre, 0.7f);

        float wet[2], dry[2];
        for (int c = 0; c < numCh; ++c)
        {
            const float x = ch[c][s];
            inPeak = juce::jmax (inPeak, std::abs (x));
            const float xt = x * transGain;
            float y;
            if (bandMode == 0)
            {
                dry[c] = x;
                y = xt * totalGain;
            }
            else
            {
                const float lo = xoverLow.processSample (c, xt);
                const float hi = xoverHigh.processSample (c, xt);
                dry[c] = xoverLowDry.processSample (c, x) + xoverHighDry.processSample (c, x); // phase-matched dry
                y = bandMode == 1 ? lo * totalGain + hi : lo + hi * totalGain;
            }
            if (duckOn)
            {
                const float lo = duckLow.processSample (c, y);
                const float hi = duckHigh.processSample (c, y);
                y = lo * duckGain + hi;
            }
            if (filtOn)
                y = filters[c].process (y, filtType);
            if (driveAmt > 0.001f)
                y = std::tanh (y * pre) * driveNorm;
            wet[c] = y;
        }

        if (numCh == 2)
        {
            const float w = juce::jlimit (0.0f, 2.0f, baseWidth + modWidth);
            const float mid = 0.5f * (wet[0] + wet[1]);
            const float side = 0.5f * (wet[0] - wet[1]) * w;
            wet[0] = mid + side;
            wet[1] = mid - side;
            const float pan = juce::jlimit (-1.0f, 1.0f, modPan);
            wet[0] *= pan > 0.0f ? 1.0f - pan : 1.0f;
            wet[1] *= pan < 0.0f ? 1.0f + pan : 1.0f;
        }

        for (int c = 0; c < numCh; ++c)
        {
            const float out = (dry[c] + (wet[c] - dry[c]) * smoothMix) * smoothOut;
            ch[c][s] = out;
            outPeak = juce::jmax (outPeak, std::abs (out));
        }

        // --- Scope: input peak per slice of the shaper cycle --------------------
        const int bin = juce::jlimit (0, scopeBins - 1, (int) (shaperPhase * scopeBins));
        const float mono = numCh > 1 ? 0.5f * (std::abs (dry[0]) + std::abs (dry[1])) : std::abs (dry[0]);
        const float monoOut = numCh > 1 ? 0.5f * (std::abs (ch[0][s]) + std::abs (ch[1][s])) : std::abs (ch[0][s]);
        const float scNow = haveSc ? juce::jmax (std::abs (scCh[0][s]), std::abs (scCh[1][s])) : 0.0f;
        if (bin != scopeIndex)
        {
            if (scopeIndex >= 0)
            {
                scope[(size_t) scopeIndex].store (scopePeak, std::memory_order_relaxed);
                scopeOut[(size_t) scopeIndex].store (scopePeakOut, std::memory_order_relaxed);
                scopeSc[(size_t) scopeIndex].store (scopePeakSc, std::memory_order_relaxed);
            }
            scopeIndex = bin;
            scopePeak = scopePeakOut = scopePeakSc = 0.0f;
        }
        scopePeak = juce::jmax (scopePeak, mono);
        scopePeakOut = juce::jmax (scopePeakOut, monoOut);
        scopePeakSc = juce::jmax (scopePeakSc, scNow);
    }

    // Mono in/out: nothing else to do. Clear any extra output channels.
    for (int c = numCh; c < main.getNumChannels(); ++c)
        main.clear (c, 0, n);

    // --- Publish values for the GUI --------------------------------------------
    uiShaperPhase = lastShaperPhase;
    uiTrigFlash = trigFlash;
    trigFlash *= 0.8f;
    uiShaperMode = shaperMode;
    uiGateStep = gateOn ? (float) lastStep : -1.0f;
    uiGateGain = gateGain;
    for (int i = 0; i < 2; ++i) { uiLfoPhase[i] = lastLfoPhase[i]; uiLfoValue[i] = lastLfoVal[i]; }
    uiSidechainLevel = juce::jmax (scPeak, uiSidechainLevel.load() * 0.9f);
    uiDuck = 1.0f - gainSum / (float) n;
    uiTotalGain = gainSum / (float) n;
    uiInLevel = juce::jmax (inPeak, uiInLevel.load() * 0.9f);
    uiOutLevel = juce::jmax (outPeak, uiOutLevel.load() * 0.9f);
    uiBpm = (float) bpm;
    uiHostSynced = hostTempo;
}

//==============================================================================
gd::Curve GateDaddyProcessor::getCurve() const
{
    const juce::ScopedLock sl (curveLock);
    return curve;
}

void GateDaddyProcessor::setCurve (const gd::Curve& c)
{
    if (c.size() < 2)
        return;
    {
        const juce::ScopedLock sl (curveLock);
        curve = c;
    }
    curveTable.fill (c);
    ++curveVersion;
}

//==============================================================================
int GateDaddyProcessor::getNumPrograms() { return (int) gd::factoryPresets().size(); }

const juce::String GateDaddyProcessor::getProgramName (int index)
{
    auto& ps = gd::factoryPresets();
    return juce::isPositiveAndBelow (index, (int) ps.size()) ? juce::String (ps[(size_t) index].name) : juce::String();
}

void GateDaddyProcessor::setCurrentProgram (int index) { loadPreset (index); }

void GateDaddyProcessor::loadPreset (int index)
{
    auto& ps = gd::factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) ps.size()))
        return;
    auto& preset = ps[(size_t) index];
    currentPreset = index;

    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (rp->getParameterID() != "bpm" && rp->getParameterID() != "denial")
                rp->setValueNotifyingHost (rp->getDefaultValue());

    auto set = [this] (const juce::String& id, float value)
    {
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    };
    for (auto& [id, value] : preset.values)
        set (id, value);
    for (int i = 0; i < numGateSteps; ++i)
        set ("step" + juce::String (i + 1), preset.steps[(size_t) i]);

    setCurve (preset.curve);
}

//==============================================================================
void GateDaddyProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("curve", gd::curveToString (getCurve()), nullptr);
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void GateDaddyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (! xml->hasTagName (apvts.state.getType()))
            return;
        auto state = juce::ValueTree::fromXml (*xml);
        auto c = gd::curveFromString (state.getProperty ("curve").toString());
        currentPreset = state.getProperty ("preset", 0);
        apvts.replaceState (state);
        if (c.size() >= 2)
            setCurve (c);
    }
}

juce::AudioProcessorEditor* GateDaddyProcessor::createEditor() { return new GateDaddyEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GateDaddyProcessor(); }
