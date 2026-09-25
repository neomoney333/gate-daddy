#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace gd
{

//==============================================================================
// Tempo-synced note values. Length is in quarter notes (beats).
struct NoteRate
{
    const char* name;
    double beats;
};

inline const std::array<NoteRate, 18>& noteRates()
{
    static const std::array<NoteRate, 18> rates {{
        { "4 BAR", 16.0 },  { "2 BAR", 8.0 },    { "1/1", 4.0 },
        { "1/2D", 3.0 },    { "1/2", 2.0 },      { "1/2T", 4.0 / 3.0 },
        { "1/4D", 1.5 },    { "1/4", 1.0 },      { "1/4T", 2.0 / 3.0 },
        { "1/8D", 0.75 },   { "1/8", 0.5 },      { "1/8T", 1.0 / 3.0 },
        { "1/16D", 0.375 }, { "1/16", 0.25 },    { "1/16T", 1.0 / 6.0 },
        { "1/32", 0.125 },  { "1/32T", 1.0 / 12.0 }, { "1/64", 0.0625 },
    }};
    return rates;
}

inline juce::StringArray noteRateNames()
{
    juce::StringArray s;
    for (auto& r : noteRates())
        s.add (r.name);
    return s;
}

inline int rateIndex (const char* name)
{
    auto& r = noteRates();
    for (int i = 0; i < (int) r.size(); ++i)
        if (juce::String (r[(size_t) i].name) == name)
            return i;
    return 7;
}

//==============================================================================
// Drawable shaper curve: a list of breakpoints. Each point's `tension` bends
// the segment that starts at that point (-1 = fast then slow, +1 = slow then fast).
struct CurvePoint
{
    float x = 0.0f, y = 0.0f, tension = 0.0f;
};

using Curve = std::vector<CurvePoint>;

inline float warp (float t, float tension)
{
    if (std::abs (tension) < 1.0e-3f)
        return t;
    const float k = tension * 7.0f;
    return (std::exp (k * t) - 1.0f) / (std::exp (k) - 1.0f);
}

inline float evalCurve (const Curve& c, float x)
{
    if (c.empty())
        return 1.0f;
    if (x <= c.front().x)
        return c.front().y;
    for (size_t i = 0; i + 1 < c.size(); ++i)
    {
        auto& a = c[i];
        auto& b = c[i + 1];
        if (x <= b.x)
        {
            const float span = b.x - a.x;
            if (span <= 1.0e-6f)
                return b.y;
            const float t = (x - a.x) / span;
            return a.y + (b.y - a.y) * warp (t, a.tension);
        }
    }
    return c.back().y;
}

inline juce::String curveToString (const Curve& c)
{
    juce::StringArray parts;
    for (auto& p : c)
        parts.add (juce::String (p.x, 4) + "," + juce::String (p.y, 4) + "," + juce::String (p.tension, 4));
    return parts.joinIntoString (";");
}

inline Curve curveFromString (const juce::String& s)
{
    Curve c;
    for (auto& part : juce::StringArray::fromTokens (s, ";", ""))
    {
        auto v = juce::StringArray::fromTokens (part, ",", "");
        if (v.size() >= 3)
            c.push_back ({ juce::jlimit (0.0f, 1.0f, v[0].getFloatValue()),
                           juce::jlimit (0.0f, 1.0f, v[1].getFloatValue()),
                           juce::jlimit (-1.0f, 1.0f, v[2].getFloatValue()) });
    }
    if (c.size() < 2)
        return {};
    std::sort (c.begin(), c.end(), [] (auto& a, auto& b) { return a.x < b.x; });
    c.front().x = 0.0f;
    c.back().x = 1.0f;
    return c;
}

// Factory curve shapes (y = volume, 1 = full level).
namespace curves
{
    inline Curve pump()       { return { { 0.0f, 0.0f, -0.55f }, { 0.42f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }; }
    inline Curve hardPump()   { return { { 0.0f, 0.0f, 0.0f }, { 0.12f, 0.0f, -0.7f }, { 0.5f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }; }
    inline Curve sawDown()    { return { { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } }; }
    inline Curve sawUp()      { return { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }; }
    inline Curve triangle()   { return { { 0.0f, 0.0f, 0.0f }, { 0.5f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } }; }
    inline Curve square()     { return { { 0.0f, 1.0f, 0.0f }, { 0.5f, 1.0f, 0.0f }, { 0.5f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } }; }
    inline Curve sine()       { return { { 0.0f, 0.0f, 0.45f }, { 0.25f, 0.5f, -0.45f }, { 0.5f, 1.0f, 0.45f }, { 0.75f, 0.5f, -0.45f }, { 1.0f, 0.0f, 0.0f } }; }
    inline Curve duckTail()   { return { { 0.0f, 1.0f, 0.0f }, { 0.02f, 0.05f, -0.4f }, { 0.6f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }; }
    inline Curve stutter()    { return { { 0.0f, 1.0f, 0.0f }, { 0.2f, 1.0f, 0.0f }, { 0.2f, 0.0f, 0.0f }, { 0.25f, 0.0f, 0.0f }, { 0.25f, 1.0f, 0.0f },
                                         { 0.45f, 1.0f, 0.0f }, { 0.45f, 0.0f, 0.0f }, { 0.5f, 0.0f, 0.0f }, { 0.5f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }; }
    inline Curve flat()       { return { { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }; }
}

//==============================================================================
// Lock-free lookup table the GUI writes into and the audio thread reads.
struct CurveTable
{
    static constexpr int size = 1024;
    std::array<std::atomic<float>, size + 1> values;

    CurveTable() { fill (curves::pump()); }

    void fill (const Curve& c)
    {
        for (int i = 0; i <= size; ++i)
            values[(size_t) i].store (evalCurve (c, (float) i / (float) size), std::memory_order_relaxed);
    }

    float lookup (float phase) const noexcept
    {
        const float pos = juce::jlimit (0.0f, 1.0f, phase) * (float) size;
        const int i = (int) pos;
        const float frac = pos - (float) i;
        const float a = values[(size_t) i].load (std::memory_order_relaxed);
        const float b = values[(size_t) juce::jmin (i + 1, size)].load (std::memory_order_relaxed);
        return a + (b - a) * frac;
    }
};

//==============================================================================
enum class LfoShape { sine, triangle, sawUp, sawDown, square, sampleHold, smoothRandom, shaperCurve };

inline juce::StringArray lfoShapeNames()
{
    return { "SINE", "TRIANGLE", "SAW UP", "SAW DOWN", "SQUARE", "S&H", "DRIFT", "CURVE" };
}

enum class LfoTarget { cutoff, resonance, pan, volume, drive, width };

inline juce::StringArray lfoTargetNames()
{
    return { "CUTOFF", "RESONANCE", "PAN", "VOLUME", "DRIVE", "WIDTH" };
}

// Bipolar output in [-1, 1].
struct Lfo
{
    juce::Random rng { 1986 };
    float held = 0.0f, prevHeld = 0.0f;
    double freePhase = 0.0;
    double lastPhase = 0.0;

    // Deterministic shape used by both the audio engine and the GUI preview.
    static float shapeValue (LfoShape s, float p)
    {
        switch (s)
        {
            case LfoShape::sine:      return std::sin (juce::MathConstants<float>::twoPi * p);
            case LfoShape::triangle:  return p < 0.25f ? p * 4.0f : (p < 0.75f ? 2.0f - p * 4.0f : p * 4.0f - 4.0f);
            case LfoShape::sawUp:     return p * 2.0f - 1.0f;
            case LfoShape::sawDown:   return 1.0f - p * 2.0f;
            case LfoShape::square:    return p < 0.5f ? 1.0f : -1.0f;
            default:                  return 0.0f;
        }
    }

    float process (LfoShape s, double phase, const CurveTable& table)
    {
        const bool wrapped = phase < lastPhase;
        lastPhase = phase;
        if (wrapped)
        {
            prevHeld = held;
            held = rng.nextFloat() * 2.0f - 1.0f;
        }
        const auto p = (float) phase;
        switch (s)
        {
            case LfoShape::sampleHold:   return held;
            case LfoShape::smoothRandom:
            {
                const float t = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * p);
                return prevHeld + (held - prevHeld) * t;
            }
            case LfoShape::shaperCurve:  return table.lookup (p) * 2.0f - 1.0f;
            default:                     return shapeValue (s, p);
        }
    }
};

//==============================================================================
// Zero-delay-feedback state variable filter (Cytomic / Simper topology).
struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.0f;

    void set (float cutoff, float q, double sr)
    {
        cutoff = juce::jlimit (20.0f, (float) (sr * 0.45), cutoff);
        const float g = std::tan (juce::MathConstants<float>::pi * cutoff / (float) sr);
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    // mode: 0 = low pass, 1 = high pass, 2 = band pass
    float process (float v0, int mode)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        if (mode == 0) return v2;
        if (mode == 1) return v0 - k * v1 - v2;
        return v1;
    }

    void reset() { ic1 = ic2 = 0.0f; }
};

} // namespace gd
