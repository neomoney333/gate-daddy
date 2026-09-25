#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace gd
{

//==============================================================================
namespace colours
{
    const juce::Colour bg        { 0xff020604 };
    const juce::Colour panel     { 0xe6040d07 };
    const juce::Colour green     { 0xff00ff41 };
    const juce::Colour midGreen  { 0xff00a82b };
    const juce::Colour dimGreen  { 0xff0b3d1a };
    const juce::Colour darkGreen { 0xff061a0c };
    const juce::Colour magenta   { 0xffff2bd6 };
    const juce::Colour cyan      { 0xff00e5ff };
    const juce::Colour amber     { 0xffffb000 };
    const juce::Colour red       { 0xffff3355 };
}

#if JUCE_WINDOWS
inline const char* monoFontName = "Consolas";
#else
inline const char* monoFontName = "Menlo";
#endif

inline juce::Font mono (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (monoFontName, size, bold ? juce::Font::bold : juce::Font::plain));
}

inline float textWidth (const juce::Font& f, const juce::String& t)
{
    return juce::GlyphArrangement::getStringWidth (f, t);
}

//==============================================================================
class MatrixLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MatrixLookAndFeel()
    {
        using namespace colours;
        setColour (juce::ResizableWindow::backgroundColourId, bg);
        setColour (juce::Label::textColourId, green);
        setColour (juce::Slider::textBoxTextColourId, green);
        setColour (juce::Slider::textBoxOutlineColourId, dimGreen);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::black);
        setColour (juce::Slider::textBoxHighlightColourId, magenta.withAlpha (0.4f));
        setColour (juce::ComboBox::backgroundColourId, juce::Colours::black);
        setColour (juce::ComboBox::textColourId, green);
        setColour (juce::ComboBox::outlineColourId, midGreen);
        setColour (juce::ComboBox::arrowColourId, green);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf0020a05));
        setColour (juce::PopupMenu::textColourId, green);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, green.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::PopupMenu::headerTextColourId, magenta);
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf0020a05));
        setColour (juce::TooltipWindow::textColourId, green);
        setColour (juce::TooltipWindow::outlineColourId, magenta);
        setColour (juce::TextEditor::backgroundColourId, juce::Colours::black);
        setColour (juce::TextEditor::textColourId, green);
        setColour (juce::TextEditor::highlightColourId, magenta.withAlpha (0.4f));
        setColour (juce::CaretComponent::caretColourId, magenta);
        setDefaultSansSerifTypefaceName (monoFontName);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace colours;
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const bool bipolar = s.getProperties()["bipolar"];
        const auto accent = s.isEnabled() ? (s.getProperties()["accent"].isVoid() ? green : juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["accent"])) : dimGreen;
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float track = juce::jmax (2.5f, r * 0.14f);

        // body
        g.setColour (juce::Colours::black);
        g.fillEllipse (c.x - r * 0.72f, c.y - r * 0.72f, r * 1.44f, r * 1.44f);
        g.setColour (darkGreen);
        g.drawEllipse (c.x - r * 0.72f, c.y - r * 0.72f, r * 1.44f, r * 1.44f, 1.0f);

        // track
        juce::Path bgArc;
        bgArc.addCentredArc (c.x, c.y, r - track, r - track, 0, startAngle, endAngle, true);
        g.setColour (dimGreen);
        g.strokePath (bgArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        // value arc with glow
        const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, r - track, r - track, 0, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (arc, juce::PathStrokeType (track * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        // pointer
        const auto tip = c.getPointOnCircumference (r * 0.62f, angle);
        const auto base = c.getPointOnCircumference (r * 0.2f, angle);
        g.setColour (s.isMouseOverOrDragging() ? juce::Colours::white : accent);
        g.drawLine ({ base, tip }, 2.0f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        using namespace colours;
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        const auto accent = b.getProperties()["accent"].isVoid() ? green : juce::Colour ((juce::uint32) (juce::int64) b.getProperties()["accent"]);
        juce::Path p;
        const float cut = juce::jmin (6.0f, r.getHeight() * 0.3f);
        p.startNewSubPath (r.getX() + cut, r.getY());
        p.lineTo (r.getRight(), r.getY());
        p.lineTo (r.getRight(), r.getBottom() - cut);
        p.lineTo (r.getRight() - cut, r.getBottom());
        p.lineTo (r.getX(), r.getBottom());
        p.lineTo (r.getX(), r.getY() + cut);
        p.closeSubPath();

        if (on)
        {
            g.setColour (accent.withAlpha (0.18f));
            g.fillRect (r.expanded (2.0f));
            g.setColour (accent.withAlpha (down ? 0.7f : 0.85f));
        }
        else
            g.setColour (over ? darkGreen.brighter (0.4f) : juce::Colours::black.withAlpha (0.8f));
        g.fillPath (p);
        g.setColour (on ? accent : (over ? accent : midGreen));
        g.strokePath (p, juce::PathStrokeType (1.0f));
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont (mono (juce::jmin (12.0f, b.getHeight() * 0.55f), true));
        g.setColour (b.getToggleState() ? juce::Colours::black : (b.isEnabled() ? colours::green : colours::dimGreen));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 0), juce::Justification::centred, 1, 0.7f);
    }

    juce::Font getComboBoxFont (juce::ComboBox& c) override { return mono (juce::jmin (12.0f, c.getHeight() * 0.55f), true); }
    juce::Font getPopupMenuFont() override { return mono (13.0f); }
    juce::Font getLabelFont (juce::Label& l) override { return mono (juce::jmin (11.0f, (float) l.getHeight() * 0.8f)); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return mono (juce::jmin (12.0f, h * 0.55f), true); }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace colours;
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.fillRect (r);
        g.setColour (box.isMouseOver (true) ? green : midGreen);
        g.drawRect (r, 1.0f);
        juce::Path arrow;
        const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour (box.isEnabled() ? green : dimGreen);
        g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (4, 1, box.getWidth() - 22, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
    {
        g.fillAll (findColour (juce::TooltipWindow::backgroundColourId));
        g.setColour (colours::magenta);
        g.drawRect (0, 0, w, h, 1);
        g.setColour (colours::green);
        g.setFont (mono (12.0f));
        g.drawFittedText (text, 6, 4, w - 12, h - 8, juce::Justification::centredLeft, 6);
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override
    {
        const int w = juce::jmin (340, 20 + (int) textWidth (mono (12.0f), text));
        const int lines = 1 + (int) (textWidth (mono (12.0f), text) / 320.0f);
        const int h = 10 + lines * 16;
        return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 24,
                                     pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h)
            .constrainedWithin (parent);
    }
};

//==============================================================================
// Draws a module panel with notched corners and a title tab.
inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title,
                       const juce::String& subtitle, juce::Colour accent, bool active = true)
{
    using namespace colours;
    const float cut = 10.0f;
    juce::Path p;
    p.startNewSubPath (r.getX() + cut, r.getY());
    p.lineTo (r.getRight() - cut, r.getY());
    p.lineTo (r.getRight(), r.getY() + cut);
    p.lineTo (r.getRight(), r.getBottom());
    p.lineTo (r.getX() + cut, r.getBottom());
    p.lineTo (r.getX(), r.getBottom() - cut);
    p.lineTo (r.getX(), r.getY() + cut);
    p.closeSubPath();
    g.setColour (panel);
    g.fillPath (p);
    g.setColour ((active ? accent : midGreen).withAlpha (active ? 0.9f : 0.5f));
    g.strokePath (p, juce::PathStrokeType (1.0f));

    // title
    g.setFont (mono (11.5f, true));
    const float tw = textWidth (mono (11.5f, true), title) + 16.0f;
    auto tab = juce::Rectangle<float> (r.getX() + cut, r.getY(), tw, 16.0f);
    g.setColour (active ? accent : midGreen);
    g.fillRect (tab);
    g.setColour (juce::Colours::black);
    g.drawText (title, tab, juce::Justification::centred);
    if (subtitle.isNotEmpty())
    {
        g.setColour ((active ? accent : midGreen).withAlpha (0.65f));
        g.setFont (mono (10.0f));
        g.drawText (subtitle, juce::Rectangle<float> (tab.getRight() + 6.0f, r.getY(), r.getWidth() - tw - 40.0f, 16.0f),
                    juce::Justification::centredLeft);
    }
}

//==============================================================================
// Knob with a caption above and a value box below.
struct Knob : public juce::Component
{
    Knob (const juce::String& caption, juce::AudioProcessorValueTreeState& s, const juce::String& paramID,
          const juce::String& tip, juce::Colour accent = colours::green, bool bipolar = false)
    {
        name.setText (caption, juce::dontSendNotification);
        name.setJustificationType (juce::Justification::centred);
        name.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (name);
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 15);
        slider.getProperties().set ("accent", (juce::int64) accent.getARGB());
        slider.getProperties().set ("bipolar", bipolar);
        slider.setTooltip (tip);
        slider.setPopupDisplayEnabled (false, false, nullptr);
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, paramID, slider);
        if (auto* prm = s.getParameter (paramID); prm != nullptr && prm->getLabel().isNotEmpty())
            slider.setTextValueSuffix (" " + prm->getLabel());
        if (paramID == "gateSteps")
            slider.setNumDecimalPlacesToDisplay (0);
        slider.setDoubleClickReturnValue (true, s.getParameter (paramID)->convertFrom0to1 (s.getParameter (paramID)->getDefaultValue()));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        name.setBounds (r.removeFromTop (13));
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmin (64, r.getWidth()), 15);
        slider.setBounds (r);
    }

    juce::Label name;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
// Falling-code background. Deliberately cheap: ~15 fps, a handful of glyphs per column.
class MatrixRain : public juce::Component, private juce::Timer
{
public:
    MatrixRain()
    {
        setInterceptsMouseClicks (false, false);
        setOpaque (true);
        startTimerHz (15);
    }

    void resized() override
    {
        drops.clear();
        const int cols = getWidth() / colW + 1;
        for (int i = 0; i < cols; ++i)
            drops.push_back ({ rng.nextFloat() * (float) getHeight(), 3.0f + rng.nextFloat() * 7.0f, rng.nextInt (1000) });
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::bg);
        g.setFont (mono (12.0f, true));
        static const juce::String glyphs ("01GATEDADY$#%&*+=<>/\\|{}[]7X9ZQ");
        static const juce::String secret ("GATEDADDY");
        for (int i = 0; i < (int) drops.size(); ++i)
        {
            auto& d = drops[(size_t) i];
            const int x = i * colW;
            for (int k = 0; k < trail; ++k)
            {
                const float y = d.y - (float) (k * colW);
                if (y < -colW || y > getHeight())
                    continue;
                const float a = k == 0 ? 0.55f : 0.28f * (1.0f - (float) k / trail);
                g.setColour ((k == 0 ? juce::Colour (0xffb8ffc8) : colours::green).withAlpha (a));
                const int seed = d.seed + (int) (y / colW);
                const juce::juce_wchar ch = (seed % 37 == 0) ? secret[(seed / 37) % secret.length()]
                                                            : glyphs[(seed * 7 + frame / 4 * (k % 3 == 0 ? 1 : 0)) % glyphs.length()];
                g.drawSingleLineText (juce::String::charToString (ch), x, (int) y);
            }
        }
    }

private:
    void timerCallback() override
    {
        ++frame;
        for (auto& d : drops)
        {
            d.y += d.speed;
            if (d.y - trail * colW > getHeight())
            {
                d.y = -rng.nextFloat() * 200.0f;
                d.speed = 3.0f + rng.nextFloat() * 7.0f;
                d.seed = rng.nextInt (1000);
            }
        }
        repaint();
    }

    struct Drop { float y, speed; int seed; };
    std::vector<Drop> drops;
    juce::Random rng;
    int frame = 0;
    static constexpr int colW = 16, trail = 14;
};

//==============================================================================
// Dr. Gate: pixel-art host of the show, with a typewriter speech bubble.
class DocGate : public juce::Component, private juce::Timer
{
public:
    DocGate() { startTimerHz (30); }

    void say (const juce::String& line)
    {
        if (line == target)
            return;
        target = line;
        shown = 0;
    }

    std::function<void()> onPoked;

    void paint (juce::Graphics& g) override
    {
        using namespace colours;
        static const char* sprite[] = {
            ".....ssssss.....",
            "....ssssssss....",
            "...ssssssssss...",
            ".hhsssssssssshh.",
            ".hhkkkkkkkkkkhh.",
            ".hhkckkkkkckkhh.",
            "...ssssssssss...",
            "...ssmmmmmmss...",
            "...smmmmmmmms...",
            "....sssrrsss....",
            ".....ssssss.....",
            "......ssss......",
            "...jjjwtwjjj....",
            "..jjjjwttwjjjj..",
            ".jjjjjwttwjjjjj.",
            ".jjjjjjttjjjjjj.",
        };
        const float px = 4.0f;
        const float ox = 10.0f, oy = 4.0f;
        const bool talking = shown < target.length();
        for (int row = 0; row < 16; ++row)
        {
            for (int col = 0; col < 16; ++col)
            {
                char c = sprite[row][col];
                juce::Colour colour;
                switch (c)
                {
                    case 's': colour = juce::Colour (0xffe0a071); break;
                    case 'k': colour = juce::Colour (0xff0a0a0a); break;
                    case 'c': colour = cyan; break;
                    case 'm': colour = juce::Colour (0xffd8d8d8); break;
                    case 'h': colour = magenta; break;
                    case 'r': colour = (talking && (frame / 3) % 2) ? juce::Colour (0xff3a0808) : juce::Colour (0xffe0a071); break;
                    case 'j': colour = juce::Colour (0xffb0127f); break;
                    case 'w': colour = juce::Colours::white; break;
                    case 't': colour = cyan; break;
                    default: continue;
                }
                float dx = 0.0f;
                if ((c == 'm') && (talking || wiggle > 0) && (frame / 2) % 2)
                    dx = px * 0.5f;
                g.setColour (colour);
                g.fillRect (ox + col * px + dx, oy + row * px, px, px);
            }
        }
        g.setColour (green);
        g.setFont (mono (9.5f, true));
        g.drawText ("DR. GATE", juce::Rectangle<float> (0, oy + 16 * px + 2, 16 * px + 16, 12), juce::Justification::centred);

        // bubble
        auto b = getLocalBounds().toFloat().withTrimmedLeft (16 * px + 22).reduced (2.0f, 6.0f);
        juce::Path bubble;
        bubble.addRoundedRectangle (b, 6.0f);
        bubble.addTriangle (b.getX(), b.getCentreY() - 6, b.getX(), b.getCentreY() + 6, b.getX() - 10, b.getCentreY() + 2);
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.fillPath (bubble);
        g.setColour (green);
        g.strokePath (bubble, juce::PathStrokeType (1.2f));
        g.setFont (mono (12.5f, true));
        juce::String txt = target.substring (0, shown);
        if ((frame / 8) % 2 == 0)
            txt << "_";
        g.drawFittedText (txt, b.reduced (10, 4).toNearestInt(), juce::Justification::centredLeft, 3, 0.8f);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        wiggle = 30;
        if (onPoked)
            onPoked();
    }

private:
    void timerCallback() override
    {
        ++frame;
        if (wiggle > 0)
            --wiggle;
        if (shown < target.length())
        {
            shown = juce::jmin (target.length(), shown + 1);
            repaint();
        }
        else if (frame % 8 == 0 || wiggle > 0)
            repaint();
    }

    juce::String target;
    int shown = 0, frame = 0, wiggle = 0;
};

//==============================================================================
// ShaperBox-style breakpoint editor with live IN / SC / OUT scope.
class ShaperEditor : public juce::Component, private juce::Timer
{
public:
    explicit ShaperEditor (GateDaddyProcessor& p) : proc (p)
    {
        curve = proc.getCurve();
        seenVersion = proc.curveVersion.load();
        setRepaintsOnMouseActivity (true);
        startTimerHz (30);
    }

    int snapDivs = 16;
    bool drawMode = false; // freehand pencil mode
    std::function<void()> onEdited;

    void setCurve (const Curve& c)
    {
        curve = c;
        commit();
    }

    const Curve& getCurve() const { return curve; }

    void paint (juce::Graphics& g) override
    {
        using namespace colours;
        const auto a = area();
        g.setColour (juce::Colours::black.withAlpha (0.9f));
        g.fillRect (getLocalBounds());

        // grid
        for (int i = 0; i <= 16; ++i)
        {
            const float x = a.getX() + a.getWidth() * i / 16.0f;
            g.setColour (i % 4 == 0 ? dimGreen.brighter (0.3f) : dimGreen.withAlpha (0.6f));
            g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        }
        for (int i = 0; i <= 4; ++i)
        {
            g.setColour (dimGreen.withAlpha (i == 2 ? 0.9f : 0.5f));
            g.drawHorizontalLine ((int) (a.getY() + a.getHeight() * i / 4.0f), a.getX(), a.getRight());
        }

        // scope (auto-scaled mirrored waveforms, centered)
        float maxv = 0.05f;
        for (int i = 0; i < GateDaddyProcessor::scopeBins; ++i)
            maxv = juce::jmax (maxv, proc.scope[(size_t) i].load(), proc.scopeOut[(size_t) i].load());
        const float cy = a.getCentreY(), half = a.getHeight() * 0.45f;
        auto drawScope = [&] (const std::array<std::atomic<float>, GateDaddyProcessor::scopeBins>& data, juce::Colour c, bool fill)
        {
            juce::Path path;
            const int N = GateDaddyProcessor::scopeBins;
            for (int i = 0; i < N; ++i)
            {
                const float x = a.getX() + a.getWidth() * (i + 0.5f) / N;
                const float v = juce::jmin (1.0f, data[(size_t) i].load() / maxv) * half;
                if (i == 0) path.startNewSubPath (x, cy - v); else path.lineTo (x, cy - v);
            }
            for (int i = N - 1; i >= 0; --i)
            {
                const float x = a.getX() + a.getWidth() * (i + 0.5f) / N;
                const float v = juce::jmin (1.0f, data[(size_t) i].load() / maxv) * half;
                path.lineTo (x, cy + v);
            }
            path.closeSubPath();
            g.setColour (c.withAlpha (fill ? 0.22f : 0.12f));
            g.fillPath (path);
            g.setColour (c.withAlpha (0.5f));
            g.strokePath (path, juce::PathStrokeType (0.8f));
        };
        if (proc.uiSidechainConnected)
            drawScope (proc.scopeSc, red, false);
        drawScope (proc.scope, juce::Colours::white, false);
        drawScope (proc.scopeOut, cyan, true);

        // curve
        juce::Path line, fill;
        const int res = juce::jmax (64, (int) a.getWidth());
        for (int i = 0; i <= res; ++i)
        {
            const float x = (float) i / res;
            const auto pt = toScreen (x, evalCurve (curve, x));
            if (i == 0) line.startNewSubPath (pt); else line.lineTo (pt);
        }
        fill = line;
        fill.lineTo (a.getRight(), a.getBottom());
        fill.lineTo (a.getX(), a.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (green.withAlpha (0.18f), 0, a.getY(), green.withAlpha (0.0f), 0, a.getBottom(), false));
        g.fillPath (fill);
        g.setColour (green.withAlpha (0.25f));
        g.strokePath (line, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (green);
        g.strokePath (line, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // tension handles
        for (size_t i = 0; i + 1 < curve.size(); ++i)
        {
            if (curve[i + 1].x - curve[i].x < 0.02f || std::abs (curve[i + 1].y - curve[i].y) < 0.01f)
                continue;
            const auto h = handlePos ((int) i);
            g.setColour ((int) i == hoverHandle ? magenta : magenta.withAlpha (0.6f));
            juce::Path d;
            d.addPolygon (h, 4, 4.5f, juce::MathConstants<float>::pi / 4.0f);
            g.strokePath (d, juce::PathStrokeType (1.2f));
        }

        // points
        for (size_t i = 0; i < curve.size(); ++i)
        {
            const auto pt = toScreen (curve[i].x, curve[i].y);
            const bool hot = (int) i == hoverPoint || (int) i == dragIndex;
            g.setColour (juce::Colours::black);
            g.fillEllipse (pt.x - 5, pt.y - 5, 10, 10);
            g.setColour (hot ? juce::Colours::white : green);
            g.drawEllipse (pt.x - 5, pt.y - 5, 10, 10, 2.0f);
        }

        // playhead
        const float ph = proc.uiShaperPhase.load();
        const float px = a.getX() + a.getWidth() * ph;
        g.setColour (magenta.withAlpha (0.9f));
        g.drawVerticalLine ((int) px, a.getY(), a.getBottom());
        g.fillEllipse (px - 4, toScreen (ph, evalCurve (curve, ph)).y - 4, 8, 8);

        // legend
        g.setFont (mono (10.0f, true));
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.drawText ("IN", 8, 4, 20, 12, juce::Justification::left);
        g.setColour (proc.uiSidechainConnected ? red : red.withAlpha (0.8f));
        g.drawText (proc.uiSidechainConnected ? "SC: CONNECTED" : "SC: NOT ROUTED (IT'S NOT YOU, IT'S YOUR ROUTING)", 30, 4, 360, 12, juce::Justification::left);
        g.setColour (cyan);
        g.drawText ("OUT", getWidth() - 110, 4, 30, 12, juce::Justification::left);
        g.setColour (green.withAlpha (0.7f));
        g.drawText (drawMode ? "PENCIL" : "AUTO SCALE", getWidth() - 80, 4, 74, 12, juce::Justification::right);

        const float flash = proc.uiTrigFlash.load();
        if (proc.uiShaperMode >= 2 && flash > 0.05f)
        {
            g.setColour (amber.withAlpha (flash));
            g.drawText ("TRIG!", getWidth() - 110, getHeight() - 16, 100, 12, juce::Justification::right);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        hoverPoint = hitPoint (e.position);
        hoverHandle = hoverPoint < 0 ? hitHandle (e.position) : -1;
        setMouseCursor (hoverPoint >= 0 || hoverHandle >= 0 ? juce::MouseCursor::DraggingHandCursor
                                                            : (drawMode ? juce::MouseCursor::CrosshairCursor : juce::MouseCursor::NormalCursor));
    }

    void mouseExit (const juce::MouseEvent&) override { hoverPoint = hoverHandle = -1; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            if (onRightClick)
                onRightClick();
            return;
        }
        if (drawMode)
        {
            lastDrawX = -1.0f;
            pencil (e.position);
            return;
        }
        dragIndex = hitPoint (e.position);
        if (dragIndex >= 0)
        {
            mode = Mode::point;
            return;
        }
        const int h = hitHandle (e.position);
        if (h >= 0)
        {
            mode = Mode::tension;
            dragIndex = h;
            startTension = curve[(size_t) h].tension;
            return;
        }
        // add a new point and start dragging it
        const float x = snapX (xFrom (e.position.x), e.mods);
        const float y = yFrom (e.position.y);
        auto it = std::upper_bound (curve.begin(), curve.end(), x, [] (float v, const CurvePoint& p) { return v < p.x; });
        if (it == curve.begin()) ++it;
        if (it == curve.end()) --it;
        it = curve.insert (it, { x, y, 0.0f });
        dragIndex = (int) std::distance (curve.begin(), it);
        mode = Mode::point;
        commit();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drawMode)
        {
            pencil (e.position);
            return;
        }
        if (dragIndex < 0)
            return;
        auto& pt = curve[(size_t) dragIndex];
        if (mode == Mode::point)
        {
            const bool isEnd = dragIndex == 0 || dragIndex == (int) curve.size() - 1;
            if (! isEnd)
                pt.x = juce::jlimit (curve[(size_t) dragIndex - 1].x, curve[(size_t) dragIndex + 1].x, snapX (xFrom (e.position.x), e.mods));
            pt.y = yFrom (e.position.y);
            if (e.mods.isShiftDown())
                pt.y = std::round (pt.y * 8.0f) / 8.0f;
        }
        else if (mode == Mode::tension)
        {
            const auto& next = curve[(size_t) dragIndex + 1];
            const float dir = next.y >= pt.y ? 1.0f : -1.0f;
            pt.tension = juce::jlimit (-1.0f, 1.0f, startTension + dir * (float) e.getDistanceFromDragStartY() / 90.0f);
        }
        commit();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        mode = Mode::none;
        dragIndex = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (drawMode)
            return;
        const int i = hitPoint (e.position);
        if (i > 0 && i < (int) curve.size() - 1)
        {
            curve.erase (curve.begin() + i);
            commit();
            return;
        }
        const int h = hitHandle (e.position);
        if (h >= 0)
        {
            curve[(size_t) h].tension = 0.0f;
            commit();
        }
    }

    std::function<void()> onRightClick;

private:
    enum class Mode { none, point, tension };

    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (10.0f, 18.0f); }

    juce::Point<float> toScreen (float x, float y) const
    {
        const auto a = area();
        return { a.getX() + x * a.getWidth(), a.getBottom() - y * a.getHeight() };
    }
    float xFrom (float px) const { const auto a = area(); return juce::jlimit (0.0f, 1.0f, (px - a.getX()) / a.getWidth()); }
    float yFrom (float py) const { const auto a = area(); return juce::jlimit (0.0f, 1.0f, (a.getBottom() - py) / a.getHeight()); }

    float snapX (float x, const juce::ModifierKeys& mods) const
    {
        if (snapDivs <= 0 || mods.isCommandDown())
            return x;
        return std::round (x * snapDivs) / (float) snapDivs;
    }

    juce::Point<float> handlePos (int i) const
    {
        const float mx = 0.5f * (curve[(size_t) i].x + curve[(size_t) i + 1].x);
        return toScreen (mx, evalCurve (curve, mx));
    }

    int hitPoint (juce::Point<float> pos) const
    {
        for (int i = (int) curve.size() - 1; i >= 0; --i)
            if (toScreen (curve[(size_t) i].x, curve[(size_t) i].y).getDistanceFrom (pos) < 8.0f)
                return i;
        return -1;
    }

    int hitHandle (juce::Point<float> pos) const
    {
        for (int i = 0; i + 1 < (int) curve.size(); ++i)
            if (handlePos (i).getDistanceFrom (pos) < 7.0f)
                return i;
        return -1;
    }

    // Pencil: paints a stepped shape on the snap grid (or 1/64 when snap is off).
    void pencil (juce::Point<float> pos)
    {
        const int divs = snapDivs > 0 ? snapDivs : 64;
        const int cell = juce::jlimit (0, divs - 1, (int) (xFrom (pos.x) * divs));
        const float y = yFrom (pos.y);
        std::vector<float> levels ((size_t) divs);
        for (int i = 0; i < divs; ++i)
            levels[(size_t) i] = evalCurve (curve, (i + 0.5f) / divs);
        levels[(size_t) cell] = y;
        Curve c;
        for (int i = 0; i < divs; ++i)
        {
            const float x0 = (float) i / divs, x1 = (float) (i + 1) / divs;
            c.push_back ({ x0, levels[(size_t) i], 0.0f });
            c.push_back ({ x1, levels[(size_t) i], 0.0f });
        }
        // merge identical neighbours so the curve stays editable
        Curve merged;
        for (auto& p : c)
            if (merged.size() < 2 || ! (juce::approximatelyEqual (merged.back().y, p.y) && juce::approximatelyEqual (merged[merged.size() - 2].y, p.y)))
                merged.push_back (p);
            else
                merged.back().x = p.x;
        curve = merged;
        commit();
    }

    void commit()
    {
        proc.setCurve (curve);
        seenVersion = proc.curveVersion.load();
        repaint();
        if (onEdited)
            onEdited();
    }

    void timerCallback() override
    {
        if (! isShowing())
            return;
        const int v = proc.curveVersion.load();
        if (v != seenVersion && mode == Mode::none)
        {
            seenVersion = v;
            curve = proc.getCurve();
        }
        repaint();
    }

    GateDaddyProcessor& proc;
    Curve curve;
    int seenVersion = 0, dragIndex = -1, hoverPoint = -1, hoverHandle = -1;
    Mode mode = Mode::none;
    float startTension = 0.0f, lastDrawX = -1.0f;
};

//==============================================================================
// 16-step trance gate grid. Click/drag paints on/off, Alt/Cmd-drag sets level.
class GateGrid : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit GateGrid (GateDaddyProcessor& p) : proc (p)
    {
        for (int i = 0; i < GateDaddyProcessor::numGateSteps; ++i)
            steps[(size_t) i] = proc.apvts.getParameter ("step" + juce::String (i + 1));
        numSteps = proc.apvts.getRawParameterValue ("gateSteps");
        gateOn = proc.apvts.getRawParameterValue ("gateOn");
        setTooltip ("");
        startTimerHz (30);
    }

    void setStep (int i, float v)
    {
        auto* prm = steps[(size_t) i];
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
        prm->endChangeGesture();
    }

    float getStep (int i) const { return steps[(size_t) i]->getValue(); }

    void paint (juce::Graphics& g) override
    {
        using namespace colours;
        g.setColour (juce::Colours::black.withAlpha (0.9f));
        g.fillRect (getLocalBounds());
        const int n = GateDaddyProcessor::numGateSteps;
        const int active = (int) numSteps->load();
        const int playing = (int) proc.uiGateStep.load();
        const bool on = gateOn->load() > 0.5f;
        auto a = getLocalBounds().toFloat().reduced (6.0f, 6.0f);
        const float w = a.getWidth() / n;
        for (int i = 0; i < n; ++i)
        {
            auto cell = juce::Rectangle<float> (a.getX() + i * w, a.getY(), w, a.getHeight()).reduced (2.0f, 0.0f);
            const bool inRange = i < active;
            const float lvl = getStep (i);
            g.setColour (i % 4 == 0 ? dimGreen.brighter (0.2f) : dimGreen.withAlpha (0.7f));
            g.drawRect (cell, 1.0f);
            if (lvl > 0.001f)
            {
                auto bar = cell.reduced (2.0f).withTrimmedTop ((cell.getHeight() - 4.0f) * (1.0f - lvl));
                const auto col = ! inRange ? dimGreen : (i == playing && on ? juce::Colours::white : (on ? green : midGreen));
                g.setColour (col.withAlpha (i == playing && on ? 0.35f : 0.18f));
                g.fillRect (bar.expanded (1.5f));
                g.setColour (col);
                g.fillRect (bar);
            }
            if (i == playing && on)
            {
                g.setColour (magenta);
                g.fillRect (cell.getX(), cell.getBottom() + 1.0f, cell.getWidth(), 3.0f);
            }
        }
        if (! on)
        {
            g.setColour (green.withAlpha (0.6f));
            g.setFont (mono (12.0f, true));
            g.drawText ("GATE IS OFF. NO BOUNDARIES. CHAOS REIGNS.", getLocalBounds(), juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = cellAt (e.position.x);
        if (i < 0) return;
        levelMode = e.mods.isAltDown() || e.mods.isCommandDown() || e.mods.isPopupMenu();
        if (levelMode)
            setStep (i, levelAt (e.position.y));
        else
        {
            paintValue = getStep (i) > 0.001f ? 0.0f : 1.0f;
            setStep (i, paintValue);
        }
        lastCell = i;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int i = cellAt (e.position.x);
        if (i < 0) return;
        if (levelMode)
            setStep (i, levelAt (e.position.y));
        else if (i != lastCell)
            setStep (i, paintValue);
        lastCell = i;
    }

private:
    int cellAt (float x) const
    {
        auto a = getLocalBounds().toFloat().reduced (6.0f, 6.0f);
        const int i = (int) ((x - a.getX()) / (a.getWidth() / GateDaddyProcessor::numGateSteps));
        return juce::isPositiveAndBelow (i, GateDaddyProcessor::numGateSteps) ? i : -1;
    }
    float levelAt (float y) const
    {
        auto a = getLocalBounds().toFloat().reduced (6.0f, 6.0f);
        return juce::jlimit (0.0f, 1.0f, (a.getBottom() - y) / a.getHeight());
    }
    void timerCallback() override
    {
        if (isShowing())
            repaint();
    }

    GateDaddyProcessor& proc;
    std::array<juce::RangedAudioParameter*, GateDaddyProcessor::numGateSteps> steps {};
    std::atomic<float>* numSteps = nullptr;
    std::atomic<float>* gateOn = nullptr;
    bool levelMode = false;
    float paintValue = 1.0f;
    int lastCell = -1;
};

//==============================================================================
class LfoView : public juce::Component, private juce::Timer
{
public:
    LfoView (GateDaddyProcessor& p, int idx) : proc (p), index (idx)
    {
        const juce::String n = "lfo" + juce::String (idx + 1);
        shape = proc.apvts.getRawParameterValue (n + "Shape");
        on = proc.apvts.getRawParameterValue (n + "On");
        depth = proc.apvts.getRawParameterValue (n + "Depth");
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace colours;
        g.setColour (juce::Colours::black.withAlpha (0.9f));
        g.fillRect (getLocalBounds());
        auto a = getLocalBounds().toFloat().reduced (4.0f, 6.0f);
        g.setColour (dimGreen);
        g.drawHorizontalLine ((int) a.getCentreY(), a.getX(), a.getRight());
        const bool active = on->load() > 0.5f;
        const auto s = (LfoShape) (int) shape->load();
        const auto col = active ? (index == 0 ? cyan : magenta) : midGreen;

        juce::Path path;
        juce::Random r (42);
        float held = r.nextFloat() * 2 - 1, prev = held;
        for (int i = 0; i <= 128; ++i)
        {
            const float ph = i / 128.0f;
            float v;
            if (s == LfoShape::sampleHold || s == LfoShape::smoothRandom)
            {
                static const float rnd[] = { -0.2f, 0.75f, -0.8f, 0.35f, 0.95f };
                const int seg = juce::jmin (3, (int) (ph * 4.0f));
                const float cur = rnd[seg + 1];
                prev = rnd[seg];
                const float t = ph * 4.0f - seg;
                v = s == LfoShape::sampleHold ? cur : prev + (cur - prev) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * t));
            }
            else if (s == LfoShape::shaperCurve)
                v = proc.curveTable.lookup (ph) * 2.0f - 1.0f;
            else
                v = Lfo::shapeValue (s, ph);
            const auto pt = juce::Point<float> (a.getX() + ph * a.getWidth(), a.getCentreY() - v * a.getHeight() * 0.45f);
            if (i == 0) path.startNewSubPath (pt); else path.lineTo (pt);
        }
        juce::ignoreUnused (held);
        g.setColour (col.withAlpha (0.25f));
        g.strokePath (path, juce::PathStrokeType (5.0f));
        g.setColour (col);
        g.strokePath (path, juce::PathStrokeType (1.6f));

        if (active)
        {
            const float ph = proc.uiLfoPhase[index].load();
            const float v = proc.uiLfoValue[index].load();
            const auto dot = juce::Point<float> (a.getX() + ph * a.getWidth(), a.getCentreY() - v * a.getHeight() * 0.45f);
            g.setColour (juce::Colours::white);
            g.fillEllipse (dot.x - 3.5f, dot.y - 3.5f, 7, 7);
        }
    }

private:
    void timerCallback() override
    {
        if (isShowing())
            repaint();
    }

    GateDaddyProcessor& proc;
    int index;
    std::atomic<float>* shape;
    std::atomic<float>* on;
    std::atomic<float>* depth;
};

//==============================================================================
class Meter : public juce::Component, public juce::SettableTooltipClient
{
public:
    Meter (juce::String caption, std::function<float()> src, juce::Colour c, bool reduction = false)
        : label (std::move (caption)), source (std::move (src)), colour (c), isReduction (reduction) {}

    void paint (juce::Graphics& g) override
    {
        using namespace colours;
        auto r = getLocalBounds().toFloat();
        auto cap = r.removeFromBottom (13.0f);
        g.setColour (green.withAlpha (0.8f));
        g.setFont (mono (9.5f, true));
        g.drawText (label, cap, juce::Justification::centred);
        r = r.reduced (2.0f, 1.0f);
        g.setColour (juce::Colours::black);
        g.fillRect (r);
        g.setColour (dimGreen);
        g.drawRect (r, 1.0f);
        const float v = source();
        float norm;
        if (isReduction)
            norm = juce::jlimit (0.0f, 1.0f, v);
        else
            norm = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (v, -60.0f) + 60.0f) / 66.0f);
        const int segs = 20;
        const float sh = (r.getHeight() - 4.0f) / segs;
        for (int i = 0; i < segs; ++i)
        {
            const float t = (i + 1.0f) / segs;
            if (t > norm + 0.001f) break;
            auto c = colour;
            if (! isReduction && t > 0.9f) c = red;
            else if (! isReduction && t > 0.75f) c = amber;
            g.setColour (c);
            g.fillRect (r.getX() + 3.0f, r.getBottom() - 2.0f - (i + 1) * sh + 1.0f, r.getWidth() - 6.0f, sh - 1.5f);
        }
    }

private:
    juce::String label;
    std::function<float()> source;
    juce::Colour colour;
    bool isReduction;
};

} // namespace gd
