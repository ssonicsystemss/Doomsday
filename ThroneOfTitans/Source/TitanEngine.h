#pragma once
// =====================================================================
//  THRONE OF TITANS - multiband master dynamics
//
//   in -> INPUT -> TILT -> 3-band LR4 split
//        each band: PRE -> compressor (down RATIO + UP ratio, knee, attack, release) -> SAT -> WIDTH (M/S) -> POST
//        (ON off = band passes untouched, SOLO = hear only soloed bands)
//   -> sum -> MONO BASS -> CLIP (soft clipper into the ceiling) -> LIMIT (drive) -> lookahead true-peak limiter (CEILING)
//   -> MATCH (optional: output turned to the input loudness, for honest A/B)
//   meters: LUFS momentary / short-term / integrated, true peak, band + limiter gain reduction
//  All blocks live in Shared (DoomsdayDynamics.h, DoomsdaySpace.h, DoomsdayDSP.h).
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include "DoomsdayDynamics.h"
#include "DoomsdaySpace.h"
#include "DoomsdayDSP.h"
#include <atomic>

struct TitanBand
{
    bool on = true, solo = false;
    float preDb = 0.f, postDb = 0.f, sat = 0.f, width = 1.f;
    doomsday::dyn::CompParams comp;
};

struct TitanParams
{
    std::array<TitanBand, 3> band;
    float xoLow = 150.f, xoHigh = 3500.f, inputDb = 0.f, tiltDb = 0.f, monoBassHz = 0.f;
    float clip = 0.f, limitDb = 0.f, ceilingDb = -0.3f, limReleaseMs = 120.f;
    bool match = false;
};

class TitanEngine
{
public:
    void prepare (double sampleRate, int)
    {
        sr = sampleRate;
        xo.prepare (sr); tilt.prepare (sr); limiter.prepare (sr, 1.5f);
        for (auto& b : comps) b.prepare (sr);
        inMeter.prepare (sr); outMeter.prepare (sr); matchIn.prepare (sr); matchOut.prepare (sr);
        for (auto* s : { &sIn, &sLimit, &sMatch }) s->reset (sr, 0.05);
        sIn.setCurrentAndTargetValue (doomsday::dyn::dbToGain (p.inputDb));
        sLimit.setCurrentAndTargetValue (doomsday::dyn::dbToGain (p.limitDb));
        sMatch.setCurrentAndTargetValue (1.f);
        for (int b = 0; b < 3; ++b)
        {
            sPre[(size_t) b].reset (sr, 0.05); sPost[(size_t) b].reset (sr, 0.05); sWidth[(size_t) b].reset (sr, 0.05);
            sPre[(size_t) b].setCurrentAndTargetValue (doomsday::dyn::dbToGain (p.band[(size_t) b].preDb));
            sPost[(size_t) b].setCurrentAndTargetValue (doomsday::dyn::dbToGain (p.band[(size_t) b].postDb));
            sWidth[(size_t) b].setCurrentAndTargetValue (p.band[(size_t) b].width);
        }
        mono = {};
        matchDb = 0.f;
    }
    int latencySamples() const { return limiter.latencySamples(); }
    void reset() { xo.reset(); limiter.reset(); for (auto& b : comps) b.reset(); mono = {}; }
    void setParams (const TitanParams& np) { p = np; }

    void process (float* L, float* R, int n)
    {
        using namespace doomsday::dyn;
        xo.set (p.xoLow, p.xoHigh);
        tilt.set (p.tiltDb);
        sIn.setTargetValue (dbToGain (p.inputDb));
        sLimit.setTargetValue (dbToGain (p.limitDb));
        bool anySolo = false;
        for (int b = 0; b < 3; ++b)
        {
            const auto& B = p.band[(size_t) b];
            anySolo |= B.solo;
            sPre[(size_t) b].setTargetValue (B.on ? dbToGain (B.preDb) : 1.f);
            sPost[(size_t) b].setTargetValue (B.on ? dbToGain (B.postDb) : 1.f);
            sWidth[(size_t) b].setTargetValue (B.on ? B.width : 1.f);
        }
        if (resetRequest.exchange (false)) resetMeters();
        preTap.push (L, R, n);
        float pkIn = 0.f, pkOut = 0.f, tpMax = 0.f, clipMax = 0.f;
        std::array<float, 3> grMin { 0.f, 0.f, 0.f }, grMax { 0.f, 0.f, 0.f };
        float limMin = 0.f;
        const float ceil = dbToGain (p.ceilingDb);
        for (int i = 0; i < n; ++i)
        {
            const float inL = L[i], inR = R[i];
            inMeter.push (inL, inR);
            matchIn.push (inL, inR);
            pkIn = juce::jmax (pkIn, std::abs (inL), std::abs (inR));
            const float gi = sIn.getNextValue();
            float l = tilt.process (0, inL * gi), r = tilt.process (1, inR * gi);
            float bl[3], br[3];
            xo.split (0, l, bl[0], bl[1], bl[2]);
            xo.split (1, r, br[0], br[1], br[2]);
            float sl = 0.f, sr_ = 0.f;
            for (int b = 0; b < 3; ++b)
            {
                const auto& B = p.band[(size_t) b];
                float x = bl[b] * sPre[(size_t) b].getNextValue(), y = br[b] * sPre[(size_t) b].getCurrentValue();
                if (B.on)
                {
                    const float g = comps[(size_t) b].process (juce::jmax (std::abs (x), std::abs (y)), B.comp);
                    x *= g; y *= g;
                    const float gr = comps[(size_t) b].gainReductionDb();
                    grMin[(size_t) b] = juce::jmin (grMin[(size_t) b], gr); grMax[(size_t) b] = juce::jmax (grMax[(size_t) b], gr);
                    x = saturate (x, B.sat); y = saturate (y, B.sat);
                }
                const float w = sWidth[(size_t) b].getNextValue();
                const float m = 0.5f * (x + y), s = 0.5f * (x - y) * w;
                const float post = sPost[(size_t) b].getNextValue();
                x = (m + s) * post; y = (m - s) * post;
                if (anySolo && ! B.solo) continue;
                sl += x; sr_ += y;
            }
            l = sl; r = sr_;
            mono.process (l, r, p.monoBassHz, sr);
            // LIMIT drive, then the soft clipper shaves the peaks just under the ceiling, then the limiter catches the rest
            const float gl = sLimit.getNextValue();
            l *= gl; r *= gl;
            if (p.clip > 0.001f)
            {
                const float cl = ceil * std::tanh (l / ceil), cr = ceil * std::tanh (r / ceil);
                const float nl = l + p.clip * (cl - l), nr = r + p.clip * (cr - r);
                // shaved amount in dB at the biggest peak
                if (std::abs (l) > 1e-6f) clipMax = juce::jmax (clipMax, gainToDb (std::abs (l) / juce::jmax (1e-9f, std::abs (nl))));
                if (std::abs (r) > 1e-6f) clipMax = juce::jmax (clipMax, gainToDb (std::abs (r) / juce::jmax (1e-9f, std::abs (nr))));
                l = nl; r = nr;
            }
            limiter.process (l, r, p.ceilingDb, p.limReleaseMs);
            limMin = juce::jmin (limMin, limiter.gainReductionDb());
            // MATCH: turn the output to the input loudness (honest comparison)
            matchOut.push (l, r);
            const float mg = sMatch.getNextValue();
            l *= mg; r *= mg;
            outMeter.push (l, r);
            const float hold = doomsday::dyn::dbToGain (tpHoldDb) * 0.98f;
            const float tp = juce::jmax (tpL.push (l, hold), tpR.push (r, hold));
            tpMax = juce::jmax (tpMax, tp);
            pkOut = juce::jmax (pkOut, std::abs (l), std::abs (r));
            L[i] = l; R[i] = r;
        }
        postTap.push (L, R, n);
        // match gain follows slowly (short-term loudness difference)
        {
            const float a = matchIn.shortTermLufs(), b = matchOut.shortTermLufs();
            if (p.match && a > -50.f && b > -50.f)
                matchDb += 0.05f * (juce::jlimit (-30.f, 0.f, a - b) - matchDb);     // only turns down: the ceiling stays safe
            else if (! p.match) matchDb = 0.f;
            sMatch.setTargetValue (dbToGain (matchDb));
        }
        // meters
        for (int b = 0; b < 3; ++b) { bandGrDb[(size_t) b].store (grMin[(size_t) b]); bandUpDb[(size_t) b].store (grMax[(size_t) b]); }
        limGrDb.store (juce::jmin (limGrDb.load(), limMin));
        clipDb.store (juce::jmax (clipDb.load(), clipMax));
        inPeak.store (juce::jmax (inPeak.load(), pkIn)); outPeak.store (juce::jmax (outPeak.load(), pkOut));
        truePeakDb.store (juce::jmax (truePeakDb.load(), gainToDb (tpMax)));
        tpHoldDb = juce::jmax (tpHoldDb, gainToDb (tpMax));
        truePeakMaxDb.store (tpHoldDb);
        lufsM.store (outMeter.momentaryLufs()); lufsS.store (outMeter.shortTermLufs()); lufsI.store (outMeter.integratedLufs());
        lufsIn.store (inMeter.shortTermLufs());
        matchGainDb.store (matchDb);
    }
    void resetMeters()
    {
        outMeter.resetIntegrated(); tpHoldDb = -120.f; truePeakMaxDb.store (-120.f);
        limGrDb.store (0.f); clipDb.store (0.f); inPeak.store (0.f); outPeak.store (0.f);
    }

    // GUI meters (read + reset by the editor)
    std::array<std::atomic<float>, 3> bandGrDb {}, bandUpDb {};
    std::atomic<float> limGrDb { 0.f }, clipDb { 0.f }, inPeak { 0.f }, outPeak { 0.f }, truePeakDb { -120.f }, truePeakMaxDb { -120.f };
    std::atomic<float> lufsM { -70.f }, lufsS { -70.f }, lufsI { -70.f }, lufsIn { -70.f }, matchGainDb { 0.f };
    doomsday::dsp::AnalyzerTap<8192> preTap, postTap;
    std::atomic<bool> resetRequest { false };

private:
    double sr = 44100.0;
    TitanParams p;
    doomsday::dyn::Crossover3 xo;
    doomsday::dyn::Tilt tilt;
    std::array<doomsday::dyn::BandComp, 3> comps;
    doomsday::dyn::Limiter limiter;
    doomsday::dyn::Loudness inMeter, outMeter, matchIn, matchOut;
    doomsday::dyn::TruePeak tpL, tpR;
    doomsday::space::MonoBass mono;
    juce::SmoothedValue<float> sIn, sLimit, sMatch;
    std::array<juce::SmoothedValue<float>, 3> sPre, sPost, sWidth;
    float matchDb = 0.f, tpHoldDb = -120.f;
};
