#pragma once

#include "DSP.h"
#include <utility>

namespace gd
{

struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values; // real-world units, choices as indices
    Curve curve;
    std::array<float, 16> steps;
};

inline float rate (const char* n) { return (float) rateIndex (n); }

inline const std::vector<Preset>& factoryPresets()
{
    static const std::array<float, 16> allOn     { 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1 };
    static const std::array<float, 16> trance   { 1,0,1,1, 0,1,1,0, 1,0,1,1, 0,1,1,1 };
    static const std::array<float, 16> offbeat   { 0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0 };
    static const std::array<float, 16> chop      { 1,1,0,1, 1,0,1,0, 1,1,0,1, 0,1,1,0 };
    static const std::array<float, 16> accents   { 1,.35f,.6f,.35f, 1,.35f,.6f,.35f, 1,.35f,.6f,.35f, 1,.35f,.6f,1 };
    static const std::array<float, 16> stutter   { 1,1,1,1, 0,0,0,0, 1,0,1,0, 1,1,1,1 };

    static const std::vector<Preset> presets {
        { "INIT // CLEAN SLATE", {}, curves::pump(), allOn },

        { "GET REAL PUMP",
          { { "shaperOn", 1 }, { "shaperRate", rate ("1/4") }, { "shaperDepth", 100 } },
          curves::pump(), allOn },

        { "HERE'S THE DEAL (1/16 GATE)",
          { { "shaperOn", 0 }, { "gateOn", 1 }, { "gateRate", rate ("1/16") }, { "gateLength", 70 },
            { "gateAttack", 1 }, { "gateRelease", 25 } },
          curves::pump(), trance },

        { "HOW'S THAT WORKIN' FOR YA",
          { { "shaperOn", 0 }, { "gateOn", 1 }, { "gateRate", rate ("1/8") }, { "gateSwing", 45 },
            { "gateLength", 60 }, { "gateRelease", 40 } },
          curves::pump(), chop },

        { "TOUGH LOVE TRANCE",
          { { "shaperOn", 1 }, { "shaperRate", rate ("1/4") }, { "shaperDepth", 60 },
            { "gateOn", 1 }, { "gateRate", rate ("1/16") }, { "gateLength", 55 },
            { "filtOn", 1 }, { "cutoff", 2500 }, { "reso", 2.5f },
            { "lfo1On", 1 }, { "lfo1Shape", 0 }, { "lfo1Rate", rate ("2 BAR") }, { "lfo1Depth", 45 }, { "lfo1Target", 0 } },
          curves::pump(), trance },

        { "THE MOUSTACHE WOBBLE",
          { { "shaperOn", 0 }, { "filtOn", 1 }, { "cutoff", 600 }, { "reso", 5 }, { "drive", 35 },
            { "lfo1On", 1 }, { "lfo1Shape", 0 }, { "lfo1Rate", rate ("1/8T") }, { "lfo1Depth", 80 }, { "lfo1Target", 0 } },
          curves::pump(), allOn },

        { "SIDECHAIN INTERVENTION",
          { { "shaperOn", 0 }, { "scOn", 1 }, { "scSource", 0 }, { "scThresh", -30 }, { "scAmount", 90 },
            { "scAttack", 2 }, { "scRelease", 180 } },
          curves::pump(), allOn },

        { "LOWS-ONLY ACCOUNTABILITY",
          { { "shaperOn", 1 }, { "shaperRate", rate ("1/4") }, { "bandMode", 1 }, { "xover", 180 } },
          curves::hardPump(), allOn },

        { "DAYTIME TV STUTTER",
          { { "shaperOn", 0 }, { "gateOn", 1 }, { "gateRate", rate ("1/32") }, { "gateLength", 80 },
            { "gateAttack", 0.5f }, { "gateRelease", 8 } },
          curves::pump(), stutter },

        { "STUDIO AUDIENCE PAN",
          { { "shaperOn", 0 },
            { "lfo1On", 1 }, { "lfo1Shape", 1 }, { "lfo1Rate", rate ("1/8") }, { "lfo1Depth", 85 }, { "lfo1Target", 2 },
            { "lfo2On", 1 }, { "lfo2Shape", 6 }, { "lfo2Rate", rate ("1/1") }, { "lfo2Depth", 40 }, { "lfo2Target", 5 } },
          curves::pump(), allOn },

        { "COUCH SESSION CHILL",
          { { "shaperOn", 1 }, { "shaperRate", rate ("1/2") }, { "shaperDepth", 55 }, { "shaperSmooth", 8 },
            { "filtOn", 1 }, { "cutoff", 5000 },
            { "lfo1On", 1 }, { "lfo1Shape", 6 }, { "lfo1Rate", rate ("4 BAR") }, { "lfo1Depth", 50 }, { "lfo1Target", 0 } },
          curves::sine(), allOn },

        { "1986 TAPE CHOP",
          { { "shaperOn", 0 }, { "gateOn", 1 }, { "gateRate", rate ("1/8T") }, { "gateLength", 50 }, { "drive", 55 },
            { "filtOn", 1 }, { "cutoff", 7000 }, { "width", 70 } },
          curves::pump(), accents },

        { "BOUNDARIES (OFFBEAT GATE)",
          { { "shaperOn", 0 }, { "gateOn", 1 }, { "gateRate", rate ("1/16") }, { "gateLength", 100 },
            { "gateAttack", 3 }, { "gateRelease", 60 } },
          curves::pump(), offbeat },

        { "SQUARE WITH ME",
          { { "shaperOn", 1 }, { "shaperRate", rate ("1/8") }, { "shaperSmooth", 2 },
            { "lfo1On", 1 }, { "lfo1Shape", 7 }, { "lfo1Rate", rate ("1/8") }, { "lfo1Depth", 35 }, { "lfo1Target", 0 },
            { "filtOn", 1 }, { "cutoff", 3000 }, { "reso", 3 } },
          curves::square(), allOn },

        { "STUTTER STEP PROGRAM",
          { { "shaperOn", 1 }, { "shaperRate", rate ("1/1") } },
          curves::stutter(), allOn },
    };
    return presets;
}

} // namespace gd
