#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/Presets.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

static void setP (GateDaddyProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

struct Signal
{
    double sr; long long n = 0;
    float kick (long long i) const { double t = std::fmod (i / sr, 60.0 / 124.0); return (float) (std::sin (2 * M_PI * (50 + 150 * std::exp (-t * 30)) * t) * std::exp (-t * 8)); }
    float pad (long long i) const { double t = i / sr; return (float) (0.3 * (2 * std::fmod (t * 110, 1.0) - 1)); }
};

// process `seconds` of pad (+ kick in sidechain or main); return per-1/16 RMS list
static int binDiv = 1;
static std::vector<float> run (GateDaddyProcessor& p, double seconds, bool kickInMain, bool kickInSc, float& peak, bool& finite)
{
    const double sr = 48000; const int bs = 512;
    Signal sig { sr };
    juce::AudioBuffer<float> buf (p.getTotalNumInputChannels(), bs);
    juce::MidiBuffer midi;
    const long long total = (long long) (seconds * sr);
    const int sixteenth = (int) (sr * 60.0 / 124.0 / 4.0 / binDiv);
    std::vector<float> rms; double acc = 0; int cnt = 0;
    peak = 0; finite = true;
    for (long long pos = 0; pos < total; pos += bs)
    {
        buf.clear();
        for (int i = 0; i < bs; ++i)
        {
            const long long k = pos + i;
            float main = sig.pad (k) + (kickInMain ? sig.kick (k) : 0.0f);
            buf.setSample (0, i, main); buf.setSample (1, i, main);
            if (buf.getNumChannels() >= 4 && kickInSc) { buf.setSample (2, i, sig.kick (k)); buf.setSample (3, i, sig.kick (k)); }
        }
        p.processBlock (buf, midi);
        for (int i = 0; i < bs; ++i)
        {
            float v = buf.getSample (0, i);
            if (! std::isfinite (v)) finite = false;
            peak = std::max (peak, std::abs (v));
            acc += v * v; if (++cnt == sixteenth) { rms.push_back ((float) std::sqrt (acc / cnt)); acc = 0; cnt = 0; }
        }
    }
    return rms;
}

static void reset (GateDaddyProcessor& p) { p.loadPreset (0); setP (p, "shaperOn", 0); p.prepareToPlay (48000, 512); }

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    GateDaddyProcessor p;
    p.enableAllBuses();
    std::cout << "Channels in: " << p.getTotalNumInputChannels() << " out: " << p.getTotalNumOutputChannels() << "\n";
    p.prepareToPlay (48000, 512);
    float peak; bool finite; int fails = 0;
    auto check = [&] (bool ok, const juce::String& what) { std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << "\n"; if (! ok) ++fails; };
    auto minmax = [] (const std::vector<float>& v, size_t from) { float lo = 1e9f, hi = 0; for (size_t i = from; i < v.size(); ++i) { lo = std::min (lo, v[i]); hi = std::max (hi, v[i]); } return std::make_pair (lo, hi); };

    // 1. Bypass-ish: everything off -> passthrough
    reset (p);
    auto r = run (p, 2, false, false, peak, finite);
    auto [lo0, hi0] = minmax (r, 4);
    check (finite && std::abs (hi0 - lo0) < 0.01f, "All modules off = clean passthrough (rms " + juce::String (lo0, 3) + ".." + juce::String (hi0, 3) + ")");

    // 2. Shaper pump 1/4
    reset (p); setP (p, "shaperOn", 1);
    binDiv = 4; r = run (p, 4, false, false, peak, finite); binDiv = 1;
    { auto [lo, hi] = minmax (r, 8); check (finite && lo < hi * 0.3f, "Shaper pumps volume (quietest 1/16 = " + juce::String (lo / hi * 100, 0) + "% of loudest)"); }

    // 3. Gate 1/16 with pattern 1,0,1,0...
    reset (p); setP (p, "gateOn", 1); setP (p, "gateRate", (float) gd::rateIndex ("1/16")); setP (p, "gateLength", 100);
    for (int i = 0; i < 16; ++i) setP (p, ("step" + juce::String (i + 1)).toRawUTF8(), i % 2 == 0 ? 1.0f : 0.0f);
    r = run (p, 4, false, false, peak, finite);
    { int good = 0, tot = 0; for (size_t i = 16; i + 1 < r.size(); i += 2) { ++tot; if (r[i] > r[i + 1] * 4) ++good; }
      check (finite && good > tot * 0.9, "Gate 1/16 on/off pattern (" + juce::String (good) + "/" + juce::String (tot) + " steps correct)"); }

    // 4. Gate rate 1/8 changes step length
    setP (p, "gateRate", (float) gd::rateIndex ("1/8")); p.prepareToPlay (48000, 512);
    r = run (p, 4, false, false, peak, finite);
    { int good = 0, tot = 0; for (size_t i = 16; i + 3 < r.size(); i += 4) { ++tot; if (r[i] > r[i + 2] * 4 && r[i + 1] > r[i + 2] * 4) ++good; }
      check (finite && good > tot * 0.9, "Gate rate 1/8 = steps twice as long (" + juce::String (good) + "/" + juce::String (tot) + ")"); }

    // 5. External sidechain ducking
    reset (p); setP (p, "scOn", 1); setP (p, "scSource", 0); setP (p, "scAmount", 100);
    r = run (p, 4, false, true, peak, finite);
    { auto [lo, hi] = minmax (r, 8); check (finite && lo < hi * 0.4f && p.uiSidechainConnected, "Sidechain input ducks the pad (" + juce::String (lo / hi * 100, 0) + "%)"); }

    // 6. SC trigger mode on the shaper
    reset (p); setP (p, "shaperOn", 1); setP (p, "shaperMode", 3); setP (p, "shaperRate", (float) gd::rateIndex ("1/8"));
    binDiv = 4; r = run (p, 4, false, true, peak, finite); binDiv = 1;
    { auto [lo, hi] = minmax (r, 32); check (finite && lo < hi * 0.5f, "Shaper retriggers from sidechain kick (" + juce::String (lo / hi * 100, 0) + "%)"); }

    // 7. Transient shaper boosts attack
    reset (p);
    run (p, 2, true, false, peak, finite); const float basePeak = peak;
    setP (p, "transOn", 1); setP (p, "transAmount", 100); setP (p, "transAttack", 12);
    run (p, 2, true, false, peak, finite);
    check (finite && peak > basePeak * 1.2f, "Transient attack boost (peak " + juce::String (basePeak, 2) + " -> " + juce::String (peak, 2) + ")");

    // 8. Low duck + everything on + all presets
    for (int i = 0; i < p.getNumPrograms(); ++i)
    {
        p.loadPreset (i);
        run (p, 3, true, true, peak, finite);
        check (finite && peak < 4.0f, "Preset " + p.getProgramName (i) + " (peak " + juce::String (peak, 2) + ")");
    }

    // 9. Kitchen sink + CPU
    for (auto id : { "transOn", "gateOn", "shaperOn", "duckOn", "scOn", "filtOn", "lfo1On", "lfo2On" }) setP (p, id, 1);
    setP (p, "bandMode", 1); setP (p, "drive", 50); setP (p, "lfo1Shape", 5); setP (p, "lfo2Target", 3);
    auto t0 = juce::Time::getMillisecondCounterHiRes();
    run (p, 60, true, true, peak, finite);
    auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
    check (finite, "Everything on for 60s of audio: " + juce::String (ms, 0) + " ms => " + juce::String (60000.0 / ms, 0) + "x faster than realtime");

    // 10. State save/restore
    juce::MemoryBlock mb; p.getStateInformation (mb);
    GateDaddyProcessor p2; p2.setStateInformation (mb.getData(), (int) mb.getSize());
    check (gd::curveToString (p2.getCurve()) == gd::curveToString (p.getCurve()) && p2.apvts.getRawParameterValue ("drive")->load() == 50.0f, "Save/restore state (curve + params)");

    // UI snapshot
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File out ("/private/tmp/claude-501/-Users-destinrayner/acf5cbe6-9001-45dc-b6ec-ee41d508bc2c/scratchpad/ui.png");
        out.deleteFile();
        juce::FileOutputStream fos (out); juce::PNGImageFormat().writeImageToStream (img, fos);
        std::cout << "UI snapshot: " << img.getWidth() << "x" << img.getHeight() << "\n";
    }
    std::cout << (fails == 0 ? "ALL TESTS PASSED\n" : "SOME TESTS FAILED\n");
    return fails;
}
