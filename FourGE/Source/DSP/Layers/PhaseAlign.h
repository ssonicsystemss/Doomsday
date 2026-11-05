#pragma once
// =====================================================================
//  4GE module 04 - PHASE ALIGNMENT analysis (offline, never on the audio thread): renders the REFERENCE layer and the
//  TARGET layer alone (everything else silent) over a time window and measures their correlation for target phases
//  across PHASE SEARCH. Nothing is changed here - the GUI shows the curve and the user decides (APPLY).
// =====================================================================
#include "../Engine.h"

namespace fge::dsp
{
    struct AlignResult { std::vector<float> phaseDeg, corr; float bestDeg = 0.f, bestCorr = 0.f, nowCorr = 0.f; };

    inline float correlation (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int from, int n)
    {
        double ab = 0.0, aa = 0.0, bb = 0.0;
        for (int i = from; i < from + n && i < a.getNumSamples() && i < b.getNumSamples(); ++i)
        {
            const double x = a.getSample (0, i) + a.getSample (1, i), y = b.getSample (0, i) + b.getSample (1, i);
            ab += x * y; aa += x * x; bb += y * y;
        }
        return aa > 1.0e-12 && bb > 1.0e-12 ? (float) (ab / std::sqrt (aa * bb)) : 0.f;
    }

    // frame: the current parameters; ref / target: layer index 0..7; window in ms; search 0..360 degrees
    inline AlignResult analysePhase (ParamFrame f, int ref, int target, float fromMs, float lenMs, float searchDeg, int note,
                                     const env::Bank* bank, const OscTables* tables, int steps = 48)
    {
        AlignResult r;
        const int ls = P::layer2On - P::layer1On;
        for (int id : { P::mixPunchMute, P::mixBodyMute, P::mixTailMute, P::mixSubMute }) f[id] = 1.f;
        f[P::noiseNAOn] = f[P::noiseNBOn] = 0.f; f[P::trOn] = 0.f; f[P::shOn] = 0.f; f[P::clipDrive] = 0.f;
        for (int i = 0; i < numLayers; ++i) f[P::layer1Solo + i * ls] = 0.f;
        const double rate = 44100.0;
        const double secs = (fromMs + lenMs) / 1000.0 + 0.02;
        auto only = [&] (int layer, float phaseDeg)
        {
            ParamFrame g = f;
            for (int i = 0; i < numLayers; ++i) g[P::layer1On + i * ls] = i == layer ? 1.f : 0.f;
            g[P::layer1Phase + layer * ls] = std::fmod (phaseDeg + 720.f, 360.f);
            return renderKick (g, note, rate, secs, -1, 1.f, bank, 0.0, -1.0, tables);
        };
        const auto a = only (ref, f[P::layer1Phase + ref * ls]);
        const int from = (int) (fromMs * 0.001 * rate), n = (int) (lenMs * 0.001 * rate);
        const float now = f[P::layer1Phase + target * ls];
        r.nowCorr = correlation (a, only (target, now), from, n);
        r.bestCorr = -2.f;
        for (int k = 0; k <= steps; ++k)
        {
            const float off = -0.5f * searchDeg + searchDeg * (float) k / (float) steps;
            const float c = correlation (a, only (target, now + off), from, n);
            r.phaseDeg.push_back (std::fmod (now + off + 720.f, 360.f)); r.corr.push_back (c);
            if (c > r.bestCorr) { r.bestCorr = c; r.bestDeg = r.phaseDeg.back(); }
        }
        return r;
    }
}
