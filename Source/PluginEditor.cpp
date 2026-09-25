#include "PluginEditor.h"
#include "Components.h"
#include "Presets.h"
#include "UpdateChecker.h"

using namespace gd;
using APVTS = juce::AudioProcessorValueTreeState;

namespace
{
    const juce::StringArray docLines {
        "YOUR KICK AND YOUR BASS NEED TO SIT DOWN AND TALK.",
        "YOU CAN'T GATE WHAT YOU DON'T ACKNOWLEDGE.",
        "THIS MIX IS CRYING OUT FOR BOUNDARIES.",
        "MUDDY LOW END? NOT IN MY STUDIO, FRIEND.",
        "SIXTEENTH NOTES DON'T LIE.",
        "THAT PAD HAS BEEN ENABLING YOU FOR YEARS.",
        "SIDECHAIN IS A TWO-WAY STREET.",
        "HOW'S THAT WORKIN' FOR YA?",
        "GET REAL. GET RHYTHMIC.",
        "THE FIRST STEP IS ADMITTING YOU HAVE A REVERB PROBLEM.",
        "I'VE SEEN TRANSIENTS WITH MORE SELF-RESPECT.",
        "WE DON'T DO 'VIBES' HERE. WE DO GRID.",
        "THIS ISN'T A SESSION. IT'S AN INTERVENTION.",
        "SWING IS JUST STRUCTURE WITH CONFIDENCE.",
        "YOU WANT THE DROP? EARN THE DROP.",
        "NOBODY EVER DANCED TO A COMPROMISE.",
        "I DIDN'T GO TO SYNTH SCHOOL FOR NOTHIN'.",
        "LET'S TALK ABOUT YOUR RELATIONSHIP WITH THE SNARE.",
        "YOU'RE NOT DUCKING. YOU'RE HIDING.",
        "THE PUMP DOESN'T JUDGE. I DO. BUT IT DOESN'T.",
    };

    const juce::StringArray realityLines {
        "REALITY CHECK: THAT WAS YOUR OLD PATTERN. THIS IS YOUR NEW LIFE.",
        "REALITY CHECK DELIVERED. NO REFUNDS.",
        "SOMETIMES YOU GOTTA SHAKE THE TREE, PARTNER.",
        "I RANDOMIZED IT. NOW DEAL WITH IT.",
        "THAT'S WHAT GROWTH SOUNDS LIKE.",
    };

    void addChoiceItems (juce::ComboBox& box, APVTS& s, const juce::String& id)
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
            box.addItemList (c->choices, 1);
    }

    Curve randomCurve (juce::Random& r)
    {
        const int divs = r.nextBool() ? 8 : 16;
        Curve c { { 0.0f, r.nextFloat() < 0.6f ? 0.0f : r.nextFloat(), -0.5f + r.nextFloat() * 0.3f } };
        int pos = 0;
        while (true)
        {
            pos += 1 + r.nextInt (divs / 3);
            if (pos >= divs) break;
            const bool hard = r.nextFloat() < 0.35f;
            const float x = (float) pos / divs;
            if (hard)
            {
                c.push_back ({ x, c.back().y, 0.0f });
                c.push_back ({ x, r.nextFloat() < 0.5f ? 0.0f : 1.0f, 0.0f });
            }
            else
                c.push_back ({ x, r.nextFloat() < 0.5f ? 1.0f : r.nextFloat(), (r.nextFloat() - 0.5f) * 1.4f });
        }
        c.push_back ({ 1.0f, 1.0f, 0.0f });
        return c;
    }
}

//==============================================================================
class GateDaddyContent : public juce::Component, private juce::Timer
{
public:
    GateDaddyContent (GateDaddyProcessor& p, GateDaddyEditor& ed)
        : proc (p), editor (ed), s (p.apvts), shaper (p), gate (p), lfoView1 (p, 0), lfoView2 (p, 1)
    {
        setLookAndFeel (&lnf);
        setOpaque (true);

        //--- Header ------------------------------------------------------------
        addAndMakeVisible (doc);
        doc.say ("WELCOME TO THE SHOW. LET'S FIX YOUR MIX.");
        doc.onPoked = [this] { doc.say (docLines[rng.nextInt (docLines.size())]); lastLineTime = juce::Time::getMillisecondCounter(); };

        auto& presets = factoryPresets();
        for (int i = 0; i < (int) presets.size(); ++i)
            presetBox.addItem (presets[(size_t) i].name, i + 1);
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
        presetBox.onChange = [this] { loadPreset (presetBox.getSelectedId() - 1); };
        presetBox.setTooltip ("FACTORY PRESETS. EACH ONE A SESSION WITH DR. GATE.");
        addAndMakeVisible (presetBox);
        for (auto* b : { &prevBtn, &nextBtn })
            addAndMakeVisible (*b);
        prevBtn.onClick = [this] { stepPreset (-1); };
        nextBtn.onClick = [this] { stepPreset (1); };

        sizeBox.addItemList ({ "SIZE 70%", "SIZE 85%", "SIZE 100%", "SIZE 125%", "SIZE 150%" }, 1);
        {
            const float sc = (float) proc.apvts.state.getProperty ("uiScale", 1.0f);
            const float opts[] = { 0.7f, 0.85f, 1.0f, 1.25f, 1.5f };
            int best = 2;
            for (int i = 0; i < 5; ++i)
                if (std::abs (opts[i] - sc) < 0.01f) best = i;
            sizeBox.setSelectedItemIndex (best, juce::dontSendNotification);
            sizeBox.onChange = [this, opts] {
                const float v = opts[juce::jlimit (0, 4, sizeBox.getSelectedItemIndex())];
                juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<GateDaddyEditor> (&editor), v] {
                    if (safe) safe->setUiScale (v);
                });
            };
        }
        addAndMakeVisible (sizeBox);

        realityBtn.setTooltip ("REALITY CHECK: RANDOMIZES THE SHAPER CURVE, GATE PATTERN AND RATES. PRESS UNTIL IT SLAPS.");
        realityBtn.getProperties().set ("accent", (juce::int64) colours::magenta.getARGB());
        realityBtn.onClick = [this] { realityCheck(); };
        addAndMakeVisible (realityBtn);

        updateBtn.getProperties().set ("accent", (juce::int64) colours::amber.getARGB());
        updateBtn.setToggleState (true, juce::dontSendNotification);
        updateBtn.onClick = [this] {
            if (auto u = UpdateService::available())
            {
                juce::URL (u->downloadUrl).launchInDefaultBrowser();
                doc.say ("GROWTH IS A CHOICE. RUN THE INSTALLER, THEN RESTART ABLETON.");
                lastLineTime = juce::Time::getMillisecondCounter();
            }
        };
        addChildComponent (updateBtn);

        toggle (denialBtn, "denial", "DENIAL MODE: BYPASSES EVERYTHING. PRETEND THE PROBLEM ISN'T THERE.", colours::red);
        bpmSlider.setSliderStyle (juce::Slider::LinearBar);
        bpmSlider.setTextValueSuffix (" BPM");
        bpmSlider.setTooltip ("STANDALONE TEMPO. INSIDE ABLETON, GATE DADDY FOLLOWS THE HOST TEMPO AUTOMATICALLY.");
        bpmSlider.setColour (juce::Slider::trackColourId, colours::dimGreen);
        bpmSlider.setColour (juce::Slider::backgroundColourId, juce::Colours::black);
        bpmSlider.setColour (juce::Slider::textBoxOutlineColourId, colours::midGreen);
        addAndMakeVisible (bpmSlider);
        attachSlider (bpmSlider, "bpm");

        //--- Transients -----------------------------------------------------------
        toggle (transOn, "transOn", "TURN ON THE TRANSIENT SHAPER. RUNS FIRST, BEFORE EVERYTHING ELSE.");
        combo (transProfile, "transProfile", "HOW THE TRANSIENT SHAPER LISTENS. HARD SNAP = CLICKY. SOFT BODY = GENTLE.");
        knob ("AMOUNT", "transAmount", "OVERALL TRANSIENT INTENSITY. SCALES ATTACK + SUSTAIN.");
        knob ("ATTACK", "transAttack", "+ PUNCHES THE FRONT OF EACH HIT. - SOFTENS IT. PICK A LANE.", colours::green, true);
        knob ("SUSTAIN", "transSustain", "+ BRINGS UP THE TAIL. - TIGHTENS IT. LET GO OF WHAT DOESN'T SERVE YOU.", colours::green, true);

        //--- Low duck --------------------------------------------------------------
        toggle (duckOn, "duckOn", "LOW-END DUCK: EXTRA DUCKING FOR THE BASS ONLY, BELOW THE FREQ KNOB.");
        combo (duckSource, "duckSource", "CURVE = DUCK FOLLOWS YOUR DRAWN SHAPE. SIDECHAIN = DUCK FOLLOWS THE SIDECHAIN INPUT (YOUR KICK).");
        knob ("FREQ", "duckFreq", "EVERYTHING BELOW THIS GETS DUCKED. 100-200 HZ FOR KICK/BASS CONFLICTS.");
        knob ("AMOUNT", "duckAmount", "HOW HARD THE LOWS GET OUT OF THE WAY.");

        //--- Filter ----------------------------------------------------------------
        toggle (filtOn, "filtOn", "FILTER YOUR FEELINGS. MOOD SWINGS CAN MODULATE THE CUTOFF.");
        combo (filtType, "filtType", "LOW PASS, HIGH PASS OR BAND PASS.");
        knob ("CUTOFF", "cutoff", "FILTER CUTOFF FREQUENCY.");
        knob ("RESO", "reso", "RESONANCE. CRANK IT FOR THAT 1986 SQUELCH.");

        //--- Shaper ----------------------------------------------------------------
        addAndMakeVisible (shaper);
        shaper.onRightClick = [this] { shaperMenu(); };
        toggle (shaperOn, "shaperOn", "VOLUME SHAPER ON/OFF. CLICK EMPTY SPACE TO ADD POINTS, DOUBLE-CLICK TO DELETE, DRAG DIAMONDS TO BEND.");
        combo (shaperMode, "shaperMode", "SYNC = LOCKED TO TEMPO. FREE HZ = UNSYNCED. INPUT TRIG = RESTARTS ON EVERY HIT OF THIS TRACK. SC TRIG = RESTARTS ON EVERY SIDECHAIN HIT (YOUR KICK).");
        combo (shaperRate, "shaperRate", "HOW LONG ONE PASS OF THE CURVE TAKES.");
        hzSlider.setSliderStyle (juce::Slider::LinearBar);
        hzSlider.setTextValueSuffix (" Hz");
        hzSlider.setColour (juce::Slider::trackColourId, colours::dimGreen);
        hzSlider.setTooltip ("FREE-RUNNING SPEED FOR FREE HZ MODE.");
        addChildComponent (hzSlider);
        attachSlider (hzSlider, "shaperHz");
        snapBox.addItemList ({ "SNAP OFF", "SNAP 1/4", "SNAP 1/8", "SNAP 1/16", "SNAP 1/32" }, 1);
        snapBox.setSelectedItemIndex (3, juce::dontSendNotification);
        snapBox.setTooltip ("GRID SNAP FOR POINTS. HOLD CMD WHILE DRAGGING FOR FREEDOM.");
        snapBox.onChange = [this] {
            const int d[] = { 0, 4, 8, 16, 32 };
            shaper.snapDivs = d[juce::jlimit (0, 4, snapBox.getSelectedItemIndex())];
        };
        addAndMakeVisible (snapBox);
        drawBtn.setClickingTogglesState (true);
        drawBtn.setTooltip ("PENCIL MODE: PAINT A STEPPED SHAPE ON THE SNAP GRID.");
        drawBtn.onClick = [this] { shaper.drawMode = drawBtn.getToggleState(); };
        addAndMakeVisible (drawBtn);
        toggle (holdBtn, "shaperHold", "HOLD END: IN TRIGGER MODES, PLAY THE SHAPE ONCE AND HOLD THE LAST VALUE UNTIL THE NEXT HIT. OFF = LOOP.");
        combo (bandMode, "bandMode", "SHAPE THE FULL SIGNAL, ONLY THE LOWS, OR ONLY THE HIGHS (SPLIT AT XOVER).");
        knob ("DEPTH", "shaperDepth", "VOLUME MIX: HOW HARD THE CURVE HITS. 100% = FULL TOUGH LOVE.");
        knob ("SMOOTH", "shaperSmooth", "SMOOTHS SHARP EDGES SO NOTHING CLICKS. UNLESS YOU WANT CLICKS. NO JUDGMENT. SOME JUDGMENT.");
        knob ("OFFSET", "shaperOffset", "SHIFTS THE CURVE IN TIME. NUDGE IT TO LAND EXACTLY ON THE KICK.");
        knob ("TRIG THR", "trigThresh", "TRIGGER THRESHOLD FOR INPUT/SC TRIG MODES. HITS LOUDER THAN THIS RESTART THE SHAPE.", colours::amber);
        knob ("XOVER", "xover", "CROSSOVER FREQ WHEN BAND IS SET TO LOWS ONLY / HIGHS ONLY.");

        const std::pair<const char*, std::function<Curve()>> shapes[] = {
            { "PUMP", curves::pump }, { "HARD PUMP", curves::hardPump }, { "RAMP", curves::sawUp },
            { "CHOP", curves::square }, { "SINE", curves::sine }, { "STUTTER", curves::stutter },
            { "DUCK TAIL", curves::duckTail }, { "TANTRUM", nullptr } };
        for (auto& [name, fn] : shapes)
        {
            auto* b = shapeBtns.add (new juce::TextButton (name));
            auto f = fn;
            b->onClick = [this, f, n = juce::String (name)] {
                shaper.setCurve (f ? f() : randomCurve (rng));
                if (! f) doc.say ("A TANTRUM? FINE. EXPRESS YOURSELF.");
            };
            if (! fn)
            {
                b->getProperties().set ("accent", (juce::int64) colours::magenta.getARGB());
                b->setTooltip ("A RANDOM CURVE. EVERY TIME. JUST LIKE YOUR EX.");
            }
            addAndMakeVisible (b);
        }

        //--- Gate ------------------------------------------------------------------
        addAndMakeVisible (gate);
        gate.setTooltip ("CLICK/DRAG TO PAINT STEPS ON/OFF. ALT-DRAG OR RIGHT-DRAG TO SET STEP LEVEL.");
        toggle (gateOn, "gateOn", "TRANCE GATE ON/OFF. CHOPS AUDIO INTO RHYTHMIC STEPS.");
        combo (gateRate, "gateRate", "GATE RATE: HOW LONG EACH STEP IS. 1/16 = CLASSIC TRANCE GATE.");
        knob ("STEPS", "gateSteps", "PATTERN LENGTH. 16 = ONE BAR AT 1/16. ODD NUMBERS = POLYRHYTHMIC CHAOS.");
        knob ("LENGTH", "gateLength", "HOW MUCH OF EACH STEP IS OPEN. LOWER = CHOPPIER.");
        knob ("SWING", "gateSwing", "DELAYS EVERY OTHER STEP. THAT'S NOT LATE, THAT'S GROOVE.");
        knob ("ATTACK", "gateAttack", "FADE-IN PER STEP. LOW = SNAPPY, HIGH = SOFT.");
        knob ("RELEASE", "gateRelease", "FADE-OUT PER STEP. SMOOTH IT OR SNAP IT.");
        knob ("GATE MIX", "gateDepth", "GATE DEPTH. 100% = FULL SILENCE BETWEEN STEPS.");

        const std::pair<const char*, std::array<float, 16>> patterns[] = {
            { "4 FLOOR", { 1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0 } },
            { "TRANCE",  { 1,0,1,1, 0,1,1,0, 1,0,1,1, 0,1,1,1 } },
            { "OFFBEAT", { 0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0 } },
            { "GALLOP",  { 1,0,1,1, 1,0,1,1, 1,0,1,1, 1,0,1,1 } },
            { "CHOP",    { 1,1,0,1, 1,0,1,0, 1,1,0,1, 0,1,1,0 } },
            { "ALL ON",  { 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1 } },
            { "CHAOS",   {} } };
        for (auto& [name, pat] : patterns)
        {
            auto* b = patternBtns.add (new juce::TextButton (name));
            const bool isRandom = juce::String (name) == "CHAOS";
            b->onClick = [this, pat, isRandom] {
                for (int i = 0; i < 16; ++i)
                    gate.setStep (i, isRandom ? (rng.nextFloat() < 0.6f ? 0.4f + 0.6f * rng.nextFloat() : 0.0f) : pat[(size_t) i]);
                if (isRandom) doc.say ("CHAOS. THAT'S NOT A PATTERN, THAT'S A LIFESTYLE.");
            };
            if (isRandom)
            {
                b->getProperties().set ("accent", (juce::int64) colours::magenta.getARGB());
                b->setTooltip ("RANDOM PATTERN WITH RANDOM LEVELS.");
            }
            addAndMakeVisible (b);
        }

        //--- LFOs ("Mood Swings") ----------------------------------------------------
        for (int i = 0; i < 2; ++i)
        {
            auto& L = lfo[i];
            const juce::String n = "lfo" + juce::String (i + 1);
            const auto accent = i == 0 ? colours::cyan : colours::magenta;
            toggle (L.on, n + "On", "MOOD SWING " + juce::String (i + 1) + " ON/OFF. AN LFO THAT MODULATES THE TARGET.", accent);
            combo (L.shape, n + "Shape", "LFO SHAPE. DRIFT = SMOOTH RANDOM. CURVE = FOLLOWS YOUR DRAWN SHAPER CURVE.");
            combo (L.target, n + "Target", "WHAT THIS MOOD SWING MESSES WITH.");
            combo (L.rate, n + "Rate", "LFO SPEED IN NOTE VALUES (WHEN SYNC IS ON).");
            toggle (L.sync, n + "Sync", "SYNC TO TEMPO. OFF = FREE-RUNNING HZ.", accent);
            L.depth = knob ("DEPTH", n + "Depth", "HOW MUCH IT MODULATES. NEGATIVE = INVERTED. MOODY.", accent, true);
            L.hz = knob ("HZ", n + "Hz", "FREE-RUNNING SPEED (WHEN SYNC IS OFF).", accent);
        }
        addAndMakeVisible (lfoView1);
        addAndMakeVisible (lfoView2);

        //--- Sidechain -------------------------------------------------------------
        toggle (scOn, "scOn", "SIDECHAIN DUCKING: TURNS THIS TRACK DOWN WHEN THE SIDECHAIN (E.G. YOUR KICK) HITS.");
        combo (scSource, "scSource", "EXTERNAL = ABLETON'S SIDECHAIN INPUT. SELF = REACTS TO THIS TRACK'S OWN LEVEL.");
        knob ("THRESH", "scThresh", "HOW LOUD THE SIDECHAIN MUST BE TO START DUCKING.");
        knob ("AMOUNT", "scAmount", "HOW FAR IT DUCKS. 100% = FULL SILENCE ON EVERY HIT.");
        knob ("ATTACK", "scAttack", "HOW FAST IT DUCKS.");
        knob ("RELEASE", "scRelease", "HOW FAST IT COMES BACK. LONGER = MORE PUMP.");

        //--- Output ("The Verdict") -------------------------------------------------
        knob ("DRIVE", "drive", "TAPE-STYLE SATURATION. ATTITUDE IN A KNOB.", colours::amber);
        knob ("WIDTH", "width", "STEREO WIDTH. 0% = MONO, 200% = VERY WIDE, VERY DRAMATIC.");
        knob ("MIX", "mix", "DRY/WET. PARALLEL PROCESSING FOR THE EMOTIONALLY BALANCED.");
        knob ("OUTPUT", "outGain", "OUTPUT LEVEL.");
        meters.add (new Meter ("IN", [this] { return proc.uiInLevel.load(); }, colours::green));
        meters.add (new Meter ("OUT", [this] { return proc.uiOutLevel.load(); }, colours::cyan));
        meters.add (new Meter ("PAIN", [this] { return proc.uiDuck.load(); }, colours::magenta, true));
        meters.getLast()->setTooltip ("PAIN METER: HOW MUCH GATE DADDY IS TURNING YOUR SIGNAL DOWN RIGHT NOW.");
        for (auto* m : meters)
            addAndMakeVisible (m);

        rain.setBounds (0, 0, GateDaddyEditor::baseWidth, GateDaddyEditor::baseHeight);
        setSize (GateDaddyEditor::baseWidth, GateDaddyEditor::baseHeight);
        startTimerHz (15);
    }

    ~GateDaddyContent() override
    {
        stopTimer();
        setLookAndFeel (nullptr);
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        using namespace colours;
        rain.paint (g);

        // Logo with chromatic aberration
        {
            auto logo = juce::Rectangle<float> (18, 10, 380, 50);
            g.setFont (mono (40.0f, true));
            const float wob = std::sin ((float) frame * 0.4f) * (glitch > 0 ? 3.0f : 0.8f);
            g.setColour (magenta.withAlpha (0.7f));
            g.drawText ("GATE DADDY", logo.translated (-2.0f - wob, 0), juce::Justification::left);
            g.setColour (cyan.withAlpha (0.7f));
            g.drawText ("GATE DADDY", logo.translated (2.0f + wob, 0), juce::Justification::left);
            g.setColour (green.withAlpha (0.3f));
            g.drawText ("GATE DADDY", logo.expanded (1.0f), juce::Justification::left);
            g.setColour (juce::Colour (0xffd8ffe0));
            g.drawText ("GATE DADDY", logo, juce::Justification::left);
            g.setFont (mono (10.5f, true));
            g.setColour (magenta);
            g.drawText ("TOUGH LOVE RHYTHM THERAPY // EST. 1986", 20, 60, 380, 14, juce::Justification::left);
            g.setColour (green.withAlpha (0.6f));
            g.drawText ("NO REFUNDS. ONLY RESULTS. v" JucePlugin_VersionString, 20, 74, 380, 12, juce::Justification::left);
        }

        auto on = [this] (const char* id) { return s.getRawParameterValue (id)->load() > 0.5f; };
        drawPanel (g, transPanel, "TRANSIENTS", "FIRST IN CHAIN // GET TO THE POINT", green, on ("transOn"));
        drawPanel (g, duckPanel, "LOW-END DUCK", "BASS INTERVENTION", amber, on ("duckOn"));
        drawPanel (g, filtPanel, "FILTER", "FILTER YOUR FEELINGS", cyan, on ("filtOn"));
        drawPanel (g, shaperPanel, "VOLUME SHAPER", "DRAW IT. PUMP IT. OWN IT.", green, on ("shaperOn"));
        drawPanel (g, gatePanel, "TRANCE GATE", "HEALTHY BOUNDARIES, 16 AT A TIME", green, on ("gateOn"));
        drawPanel (g, lfoPanel[0], "MOOD SWING 1", "LFO", cyan, on ("lfo1On"));
        drawPanel (g, lfoPanel[1], "MOOD SWING 2", "LFO", magenta, on ("lfo2On"));
        drawPanel (g, scPanel, "SIDECHAIN", "WHO'S KNOCKING?", red, on ("scOn"));
        drawPanel (g, outPanel, "THE VERDICT", "OUTPUT", green, ! on ("denial"));

        // Sidechain status + meter
        {
            const bool connected = proc.uiSidechainConnected;
            const bool external = s.getRawParameterValue ("scSource")->load() < 0.5f;
            juce::String status = ! external ? "LISTENING TO ITSELF. VERY SELF-AWARE."
                                 : connected ? "SIDECHAIN CONNECTED. SOMEONE CARES."
                                             : "NOT ROUTED. SET IT IN ABLETON'S SIDECHAIN MENU.";
            g.setFont (mono (10.0f, true));
            g.setColour (connected || ! external ? green : amber);
            g.drawText (status, scStatus, juce::Justification::centredLeft);

            auto m = scMeter;
            g.setColour (juce::Colours::black);
            g.fillRect (m);
            g.setColour (dimGreen);
            g.drawRect (m, 1.0f);
            const float lvl = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (proc.uiSidechainLevel.load(), -60.0f) + 60.0f) / 60.0f);
            g.setColour (red.withAlpha (0.8f));
            g.fillRect (m.reduced (2.0f).withWidth ((m.getWidth() - 4.0f) * lvl));
            const float thr = (s.getRawParameterValue ("scThresh")->load() + 60.0f) / 60.0f;
            g.setColour (juce::Colours::white);
            g.drawVerticalLine ((int) (m.getX() + 2.0f + (m.getWidth() - 4.0f) * thr), m.getY(), m.getBottom());
            g.setColour (green.withAlpha (0.7f));
            g.drawText ("SC LEVEL / THRESH", m.translated (0, 12).withHeight (12), juce::Justification::centredLeft);
        }

        // Verdict readout
        {
            g.setFont (mono (11.0f, true));
            const float duckDb = juce::Decibels::gainToDecibels (juce::jmax (0.001f, proc.uiTotalGain.load()));
            juce::String line1 = juce::String (proc.uiBpm.load(), 1) + " BPM " + (proc.uiHostSynced ? "(HOST SYNCED)" : "(STANDALONE CLOCK)");
            juce::String line2 = "GAIN MOVEMENT: " + juce::String (duckDb, 1) + " dB";
            juce::String line3 = "DIAGNOSIS: " + diagnosis;
            g.setColour (green);
            g.drawText (line1, readout.withHeight (16), juce::Justification::centredLeft);
            g.drawText (line2, readout.withHeight (16).translated (0, 18), juce::Justification::centredLeft);
            g.setColour (magenta);
            g.drawFittedText (line3, readout.withTrimmedTop (38).toNearestInt(), juce::Justification::topLeft, 2);
        }

        // Signal chain footer
        {
            struct Stage { const char* name; const char* id; };
            const Stage chain[] = { { "TRANSIENTS", "transOn" }, { "GATE", "gateOn" }, { "SHAPER", "shaperOn" },
                                    { "LOW DUCK", "duckOn" }, { "SIDECHAIN", "scOn" }, { "MOOD SWINGS", nullptr },
                                    { "FILTER", "filtOn" }, { "DRIVE", nullptr }, { "MIX", nullptr } };
            g.setFont (mono (10.5f, true));
            float x = 18.0f;
            const float y = (float) GateDaddyEditor::baseHeight - 20.0f;
            g.setColour (green.withAlpha (0.6f));
            g.drawText ("SIGNAL CHAIN:", (int) x, (int) y, 110, 14, juce::Justification::left);
            x += 106.0f;
            for (int i = 0; i < (int) std::size (chain); ++i)
            {
                bool active = chain[i].id == nullptr ? (i == 5 ? (on ("lfo1On") || on ("lfo2On"))
                                                              : (i == 7 ? s.getRawParameterValue ("drive")->load() > 0.0f : true))
                                                     : on (chain[i].id);
                const juce::String txt = chain[i].name;
                const float w = textWidth (mono (10.5f, true), txt);
                g.setColour (active ? green : dimGreen.brighter (0.5f));
                g.drawText (txt, (int) x, (int) y, (int) w + 2, 14, juce::Justification::left);
                x += w + 4.0f;
                if (i < (int) std::size (chain) - 1)
                {
                    g.setColour (magenta.withAlpha (0.8f));
                    g.drawText (">", (int) x, (int) y, 12, 14, juce::Justification::left);
                    x += 14.0f;
                }
            }
        }

        // CRT scanlines
        g.setColour (juce::Colours::black.withAlpha (0.12f));
        for (int y = 0; y < getHeight(); y += 3)
            g.drawHorizontalLine (y, 0.0f, (float) getWidth());
    }

    //==========================================================================
    void resized() override
    {
        using R = juce::Rectangle<int>;
        doc.setBounds (400, 6, 400, 86);

        prevBtn.setBounds (812, 10, 28, 26);
        presetBox.setBounds (844, 10, 310, 26);
        nextBtn.setBounds (1158, 10, 28, 26);
        sizeBox.setBounds (812, 42, 104, 24);
        realityBtn.setBounds (922, 42, 140, 24);
        updateBtn.setBounds (262, 18, 132, 24);
        denialBtn.setBounds (1068, 42, 118, 24);
        bpmSlider.setBounds (812, 70, 374, 18);

        // Row A
        transPanel = { 12, 100, 548, 92 };
        duckPanel = { 568, 100, 262, 92 };
        filtPanel = { 838, 100, 350, 92 };
        {
            auto r = transPanel.toNearestInt().reduced (10, 0).withTrimmedTop (22);
            transOn.setBounds (r.getX(), r.getY() + 4, 44, 22);
            transProfile.setBounds (r.getX() + 52, r.getY() + 4, 170, 22);
            place ("transAmount", R (r.getX() + 236, r.getY() - 6, 90, 72));
            place ("transAttack", R (r.getX() + 332, r.getY() - 6, 90, 72));
            place ("transSustain", R (r.getX() + 428, r.getY() - 6, 90, 72));
        }
        {
            auto r = duckPanel.toNearestInt().reduced (10, 0).withTrimmedTop (22);
            duckOn.setBounds (r.getX(), r.getY() + 4, 44, 22);
            duckSource.setBounds (r.getX(), r.getY() + 32, 104, 22);
            place ("duckFreq", R (r.getX() + 110, r.getY() - 6, 66, 72));
            place ("duckAmount", R (r.getX() + 176, r.getY() - 6, 66, 72));
        }
        {
            auto r = filtPanel.toNearestInt().reduced (10, 0).withTrimmedTop (22);
            filtOn.setBounds (r.getX(), r.getY() + 4, 44, 22);
            filtType.setBounds (r.getX() + 52, r.getY() + 4, 120, 22);
            place ("cutoff", R (r.getX() + 180, r.getY() - 6, 74, 72));
            place ("reso", R (r.getX() + 256, r.getY() - 6, 74, 72));
        }

        // Shaper
        shaperPanel = { 12, 200, 818, 292 };
        {
            auto r = shaperPanel.toNearestInt().reduced (10, 0);
            int x = r.getX(), y = r.getY() + 22;
            auto next = [&] (juce::Component& c, int w) { c.setBounds (x, y, w, 24); x += w + 6; };
            next (shaperOn, 44);
            next (shaperMode, 118);
            shaperRate.setBounds (x, y, 84, 24);
            hzSlider.setBounds (x, y, 84, 24);
            x += 90;
            next (snapBox, 98);
            next (drawBtn, 70);
            next (holdBtn, 88);
            next (bandMode, 124);
            const int strip = 150;
            shaper.setBounds (r.getX(), y + 30, r.getWidth() - strip - 8, 196);
            const int bw = (shaper.getWidth() - 7 * 5) / 8;
            for (int i = 0; i < shapeBtns.size(); ++i)
                shapeBtns[i]->setBounds (r.getX() + i * (bw + 5), shaper.getBottom() + 6, bw, 22);
            const int sx = r.getRight() - strip;
            place ("shaperDepth", R (sx + 20, y + 26, 110, 96));
            place ("shaperSmooth", R (sx, y + 124, 75, 66));
            place ("shaperOffset", R (sx + 75, y + 124, 75, 66));
            place ("trigThresh", R (sx, y + 192, 75, 66));
            place ("xover", R (sx + 75, y + 192, 75, 66));
        }

        // Gate
        gatePanel = { 12, 500, 818, 178 };
        {
            auto r = gatePanel.toNearestInt().reduced (10, 0);
            const int y = r.getY() + 22;
            gateOn.setBounds (r.getX(), y, 44, 22);
            gateRate.setBounds (r.getX() + 50, y, 84, 22);
            const int knobsW = 246;
            const int gx = r.getX() + 142, gw = r.getWidth() - knobsW - 150;
            const int pbw = (gw - 6 * 4) / 7;
            for (int i = 0; i < patternBtns.size(); ++i)
                patternBtns[i]->setBounds (gx + i * (pbw + 4), y, pbw, 22);
            gate.setBounds (r.getX(), y + 28, r.getWidth() - knobsW - 8, 118);
            const int kx = r.getRight() - knobsW;
            const char* ids[] = { "gateSteps", "gateLength", "gateSwing", "gateAttack", "gateRelease", "gateDepth" };
            for (int i = 0; i < 6; ++i)
                place (ids[i], R (kx + (i % 3) * 82, y - 2 + (i / 3) * 74, 82, 70));
        }

        // Right column
        lfoPanel[0] = { 838, 200, 350, 144 };
        lfoPanel[1] = { 838, 350, 350, 142 };
        for (int i = 0; i < 2; ++i)
        {
            auto r = lfoPanel[i].toNearestInt().reduced (8, 0);
            auto& L = lfo[i];
            const int y = r.getY() + 22;
            L.on.setBounds (r.getX(), y, 40, 22);
            L.shape.setBounds (r.getX() + 46, y, 100, 22);
            L.target.setBounds (r.getX() + 152, y, 110, 22);
            L.sync.setBounds (r.getX() + 268, y, r.getRight() - (r.getX() + 268), 22);
            auto& view = i == 0 ? lfoView1 : lfoView2;
            view.setBounds (r.getX(), y + 28, 180, r.getBottom() - (y + 28) - 8);
            L.rate.setBounds (r.getX() + 190, y + 28, 144, 22);
            L.depth->setBounds (r.getX() + 190, y + 52, 70, 64);
            L.hz->setBounds (r.getX() + 264, y + 52, 70, 64);
        }
        scPanel = { 838, 500, 350, 178 };
        {
            auto r = scPanel.toNearestInt().reduced (8, 0);
            const int y = r.getY() + 22;
            scOn.setBounds (r.getX(), y, 40, 22);
            scSource.setBounds (r.getX() + 46, y, 110, 22);
            scStatus = juce::Rectangle<float> ((float) r.getX(), (float) y + 26, (float) r.getWidth(), 14);
            const char* ids[] = { "scThresh", "scAmount", "scAttack", "scRelease" };
            for (int i = 0; i < 4; ++i)
                place (ids[i], R (r.getX() + i * 84, y + 42, 80, 68));
            scMeter = juce::Rectangle<float> ((float) r.getX(), (float) y + 114, (float) r.getWidth(), 12);
        }

        // Output
        outPanel = { 12, 686, 1176, 118 };
        {
            auto r = outPanel.toNearestInt().reduced (12, 0);
            const int y = r.getY() + 20;
            const char* ids[] = { "drive", "width", "mix", "outGain" };
            for (int i = 0; i < 4; ++i)
                place (ids[i], R (r.getX() + i * 92, y, 88, 92));
            for (int i = 0; i < meters.size(); ++i)
                meters[i]->setBounds (r.getX() + 380 + i * 36, y + 2, 32, 90);
            readout = juce::Rectangle<float> ((float) r.getX() + 500, (float) y + 6, (float) r.getRight() - (r.getX() + 500), 80);
        }
    }

private:
    //==========================================================================
    void timerCallback() override
    {
        ++frame;
        if (glitch > 0) --glitch;

        // Show the right tempo control
        const bool synced = proc.uiHostSynced;
        bpmSlider.setEnabled (! synced);
        bpmSlider.setTextValueSuffix (synced ? " BPM (HOST)" : " BPM");
        if (synced && std::abs ((float) bpmSlider.getValue() - proc.uiBpm.load()) > 0.05f)
            bpmSlider.setValue (proc.uiBpm.load(), juce::dontSendNotification);

        const int mode = (int) s.getRawParameterValue ("shaperMode")->load();
        hzSlider.setVisible (mode == 1);
        shaperRate.setVisible (mode != 1);
        for (int i = 0; i < 2; ++i)
        {
            const bool sync = s.getRawParameterValue ("lfo" + juce::String (i + 1) + "Sync")->load() > 0.5f;
            lfo[i].rate.setEnabled (sync);
            lfo[i].hz->setEnabled (! sync);
        }

        if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
            presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);

        if (! updateBtn.isVisible())
        {
            if (auto u = UpdateService::available())
            {
                updateBtn.setButtonText ("UPDATE v" + u->version);
                updateBtn.setTooltip ("A NEWER GATE DADDY (v" + u->version + ") IS OUT. CLICK TO DOWNLOAD THE INSTALLER, THEN RESTART ABLETON.");
                updateBtn.setVisible (true);
                doc.say ("THERE'S A NEW ME. v" + u->version + ". CHANGE IS HARD. CLICK UPDATE ANYWAY.");
                lastLineTime = juce::Time::getMillisecondCounter();
            }
        }

        updateDoc();
        repaint();
    }

    void updateDoc()
    {
        auto on = [this] (const char* id) { return s.getRawParameterValue (id)->load() > 0.5f; };
        const bool externalSc = s.getRawParameterValue ("scSource")->load() < 0.5f;
        const int mode = (int) s.getRawParameterValue ("shaperMode")->load();
        const bool wantsSc = (on ("scOn") && externalSc) || (on ("shaperOn") && mode == 3)
                             || (on ("duckOn") && s.getRawParameterValue ("duckSource")->load() > 0.5f);

        juce::String alert;
        if (on ("denial"))
            alert = "DENIAL MODE. WE'RE JUST GONNA PRETEND EVERYTHING'S FINE?";
        else if (proc.uiOutLevel.load() > 1.0f)
            alert = "EASY, SPARKY. YOU'RE CLIPPING. THAT'S A CRY FOR HELP.";
        else if (wantsSc && ! proc.uiSidechainConnected)
            alert = "YOU WANT A SIDECHAIN BUT NOTHING'S ROUTED. WHO ARE YOU WAITING FOR?";
        else if (! on ("shaperOn") && ! on ("gateOn") && ! on ("scOn") && ! on ("transOn") && ! on ("duckOn")
                 && ! on ("filtOn") && ! on ("lfo1On") && ! on ("lfo2On"))
            alert = "YOU PUT ME ON A TRACK TO DO NOTHING? BOLD MOVE.";
        else if (proc.uiDuck.load() > 0.75f)
            alert = "NOW THAT'S WHAT I CALL ACCOUNTABILITY.";

        diagnosis = on ("denial") ? "IN DENIAL" : proc.uiOutLevel.load() > 1.0f ? "CLIPPING (SEEK HELP)"
                  : proc.uiDuck.load() > 0.5f ? "PUMPING WITH PURPOSE" : proc.uiDuck.load() > 0.15f ? "HEALTHY MOVEMENT"
                  : "STABLE. SUSPICIOUSLY STABLE.";

        const auto now = juce::Time::getMillisecondCounter();
        if (alert.isNotEmpty() && alert != lastAlert)
        {
            doc.say (alert);
            lastLineTime = now;
        }
        else if (now - lastLineTime > 9000)
        {
            doc.say (docLines[rng.nextInt (docLines.size())]);
            lastLineTime = now;
        }
        lastAlert = alert;
    }

    void loadPreset (int index)
    {
        if (index < 0) return;
        proc.loadPreset (index);
        glitch = 8;
        doc.say ("NOW SERVING: " + juce::String (factoryPresets()[(size_t) index].name) + ".");
        lastLineTime = juce::Time::getMillisecondCounter();
    }

    void stepPreset (int delta)
    {
        const int n = (int) factoryPresets().size();
        const int next = (proc.getCurrentProgram() + delta + n) % n;
        presetBox.setSelectedId (next + 1, juce::dontSendNotification);
        loadPreset (next);
    }

    void realityCheck()
    {
        shaper.setCurve (randomCurve (rng));
        for (int i = 0; i < 16; ++i)
            gate.setStep (i, rng.nextFloat() < 0.65f ? (rng.nextFloat() < 0.7f ? 1.0f : 0.5f) : 0.0f);
        auto setChoice = [this] (const char* id, const char* rateName)
        {
            auto* prm = s.getParameter (id);
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) rateIndex (rateName)));
        };
        const char* shaperRates[] = { "1/4", "1/2", "1/8", "1/4D", "1/1" };
        const char* gateRates[] = { "1/16", "1/8", "1/16T", "1/32", "1/8T" };
        setChoice ("shaperRate", shaperRates[rng.nextInt (5)]);
        setChoice ("gateRate", gateRates[rng.nextInt (5)]);
        if (auto* sw = s.getParameter ("gateSwing"))
            sw->setValueNotifyingHost (sw->convertTo0to1 (rng.nextFloat() < 0.5f ? 0.0f : rng.nextFloat() * 50.0f));
        glitch = 10;
        doc.say (realityLines[rng.nextInt (realityLines.size())]);
        lastLineTime = juce::Time::getMillisecondCounter();
    }

    void shaperMenu()
    {
        juce::PopupMenu m;
        m.addSectionHeader ("CURVE THERAPY");
        m.addItem (1, "INVERT (FLIP UPSIDE DOWN)");
        m.addItem (2, "REVERSE (PLAY IT BACKWARDS)");
        m.addItem (3, "DOUBLE (REPEAT TWICE)");
        m.addItem (4, "RESET (FLAT, NO EFFECT)");
        m.addItem (5, "SMOOTH ALL CORNERS");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&shaper), [this] (int r) {
            auto c = shaper.getCurve();
            if (r == 1) for (auto& pt : c) pt.y = 1.0f - pt.y;
            else if (r == 2)
            {
                Curve rev;
                for (int i = (int) c.size() - 1; i >= 0; --i)
                    rev.push_back ({ 1.0f - c[(size_t) i].x, c[(size_t) i].y, i > 0 ? -c[(size_t) i - 1].tension : 0.0f });
                c = rev;
            }
            else if (r == 3)
            {
                Curve d;
                for (auto& pt : c) d.push_back ({ pt.x * 0.5f, pt.y, pt.tension });
                for (auto& pt : c) d.push_back ({ 0.5f + pt.x * 0.5f, pt.y, pt.tension });
                c = d;
            }
            else if (r == 4) c = curves::flat();
            else if (r == 5) for (auto& pt : c) pt.tension = pt.tension == 0.0f ? -0.35f : pt.tension;
            else return;
            shaper.setCurve (c);
        });
    }

    //--- widget helpers -----------------------------------------------------------
    Knob* knob (const juce::String& caption, const juce::String& id, const juce::String& tip,
                juce::Colour accent = colours::green, bool bipolar = false)
    {
        auto* k = knobs.add (new Knob (caption, s, id, tip, accent, bipolar));
        k->setName (id);
        addAndMakeVisible (k);
        return k;
    }

    void place (const juce::String& id, juce::Rectangle<int> r)
    {
        for (auto* k : knobs)
            if (k->getName() == id)
                k->setBounds (r);
    }

    void toggle (juce::TextButton& b, const juce::String& id, const juce::String& tip, juce::Colour accent = colours::green)
    {
        if (b.getButtonText().isEmpty())
            b.setButtonText (id.endsWith ("Sync") ? "SYNC" : "ON");
        b.setClickingTogglesState (true);
        b.setTooltip (tip);
        b.getProperties().set ("accent", (juce::int64) accent.getARGB());
        addAndMakeVisible (b);
        buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (s, id, b));
    }

    void combo (juce::ComboBox& box, const juce::String& id, const juce::String& tip)
    {
        addChoiceItems (box, s, id);
        box.setTooltip (tip);
        addAndMakeVisible (box);
        comboAtts.push_back (std::make_unique<APVTS::ComboBoxAttachment> (s, id, box));
    }

    void attachSlider (juce::Slider& sl, const juce::String& id)
    {
        sliderAtts.push_back (std::make_unique<APVTS::SliderAttachment> (s, id, sl));
    }

    //==========================================================================
    GateDaddyProcessor& proc;
    GateDaddyEditor& editor;
    APVTS& s;
    MatrixLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 500 };
    MatrixRain rain;
    DocGate doc;
    juce::Random rng;

    juce::ComboBox presetBox, sizeBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, realityBtn { "REALITY CHECK" }, denialBtn { "DENIAL MODE" }, updateBtn { "UPDATE" };
    juce::SharedResourcePointer<UpdateService> updates;
    juce::Slider bpmSlider;

    juce::TextButton transOn, duckOn, filtOn, shaperOn, gateOn, scOn;
    juce::ComboBox transProfile, duckSource, filtType, shaperMode, shaperRate, bandMode, gateRate, scSource, snapBox;
    juce::Slider hzSlider;
    juce::TextButton drawBtn { "PENCIL" }, holdBtn { "HOLD END" };

    ShaperEditor shaper;
    GateGrid gate;
    LfoView lfoView1, lfoView2;
    juce::OwnedArray<juce::TextButton> shapeBtns, patternBtns;

    struct LfoUi
    {
        juce::TextButton on, sync;
        juce::ComboBox shape, target, rate;
        Knob* depth = nullptr;
        Knob* hz = nullptr;
    } lfo[2];

    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<Meter> meters;
    std::vector<std::unique_ptr<APVTS::ButtonAttachment>> buttonAtts;
    std::vector<std::unique_ptr<APVTS::ComboBoxAttachment>> comboAtts;
    std::vector<std::unique_ptr<APVTS::SliderAttachment>> sliderAtts;

    juce::Rectangle<float> transPanel, duckPanel, filtPanel, shaperPanel, gatePanel, lfoPanel[2], scPanel, outPanel;
    juce::Rectangle<float> scStatus, scMeter, readout;
    juce::String diagnosis = "PENDING", lastAlert;
    juce::uint32 lastLineTime = 0;
    int frame = 0, glitch = 0;
};

//==============================================================================
GateDaddyEditor::GateDaddyEditor (GateDaddyProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    content = std::make_unique<GateDaddyContent> (p, *this);
    addAndMakeVisible (*content);
    const float sc = (float) proc.apvts.state.getProperty ("uiScale", 1.0f);
    setUiScale (juce::jlimit (0.5f, 2.0f, sc));
}

GateDaddyEditor::~GateDaddyEditor() = default;

void GateDaddyEditor::setUiScale (float scale)
{
    proc.apvts.state.setProperty ("uiScale", scale, nullptr);
    content->setTransform (juce::AffineTransform::scale (scale));
    setSize (juce::roundToInt (baseWidth * scale), juce::roundToInt (baseHeight * scale));
}

void GateDaddyEditor::resized()
{
    content->setBounds (0, 0, baseWidth, baseHeight);
}
