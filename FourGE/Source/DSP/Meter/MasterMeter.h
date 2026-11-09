#pragma once
// =====================================================================
//  4GE module 12 - MASTER METERING (pure DSP, no GUI / files):
//   LiveMeter (audio thread push, GUI reads atomics): sample PEAK L / R (display + MAX since reset + 2 s HOLD + CLIP),
//   TRUE PEAK (8x interpolation, BS.1770 style, its own value - never the sample peak), RMS (300 ms), LUFS momentary / short-term /
//   integrated since reset (BS.1770 gated), DC OFFSET detection (1 s mean). RESET = a flag the audio thread acts on.
//   measure(): the same numbers for a finished render (export) - integrated LUFS when the render is long enough (>= 400 ms).
// =====================================================================
#include "DoomsdayDynamics.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

namespace fge::dsp::meter
{
    inline float toDb (double g) { return g > 1.0e-9 ? (float) (20.0 * std::log10 (g)) : -180.f; }

    struct Measure
    {
        float samplePeakDb = -180.f, truePeakDb = -180.f, rmsDb = -180.f, lufs = -70.f, dcL = 0.f, dcR = 0.f;
        bool lufsValid = false, dc = false, clipped = false;
        double seconds = 0.0;
    };
    // the numbers of a finished buffer (offline: export / preview)
    inline Measure measure (const juce::AudioBuffer<float>& b, double rate)
    {
        Measure m;
        const int n = b.getNumSamples(), ch = b.getNumChannels();
        if (n <= 0 || ch <= 0) return m;
        m.seconds = (double) n / rate;
        double pk = 0.0, tp = 0.0, sq = 0.0;
        std::array<double, 2> mean {};
        std::array<doomsday::dyn::TruePeak, 2> tpd;
        for (int c = 0; c < juce::jmin (2, ch); ++c)
        {
            const float* x = b.getReadPointer (c);
            for (int i = 0; i < n; ++i)
            {
                pk = juce::jmax (pk, (double) std::abs (x[i])); sq += (double) x[i] * x[i]; mean[(size_t) c] += x[i];
                tp = juce::jmax (tp, (double) tpd[(size_t) c].push (x[i]));
            }
            for (int i = 0; i < doomsday::dyn::TruePeak::taps; ++i) tp = juce::jmax (tp, (double) tpd[(size_t) c].push (0.f));   // (flush the delay)
        }
        tp = juce::jmax (tp, pk);                                          // (a true peak is never below the sample peak)
        m.samplePeakDb = toDb (pk); m.truePeakDb = toDb (tp); m.clipped = pk > 1.0;
        m.rmsDb = toDb (std::sqrt (sq / (double) (n * juce::jmin (2, ch))));
        m.dcL = (float) (mean[0] / n); m.dcR = (float) (ch > 1 ? mean[1] / n : mean[0] / n);
        m.dc = std::abs (m.dcL) > 0.003f || std::abs (m.dcR) > 0.003f;
        if (m.seconds >= 0.4)
        {
            doomsday::dyn::Loudness lu; lu.prepare (rate);
            const float* l = b.getReadPointer (0); const float* r = b.getReadPointer (ch > 1 ? 1 : 0);
            for (int i = 0; i < n; ++i) lu.push (l[i], r[i]);
            m.lufs = lu.integratedLufs(); m.lufsValid = m.lufs > -69.9f;
        }
        return m;
    }

    class LiveMeter
    {
    public:
        void prepare (double rate)
        {
            sr = rate; lu.prepare (rate); for (auto& t : tpd) t.reset();
            rmsK = 1.0 - std::exp (-1.0 / (0.3 * rate)); dcK = 1.0 - std::exp (-1.0 / (1.0 * rate));
            holdN = (int) (2.0 * rate); fall = (float) std::exp (-1.0 / (0.3 * rate));
            doReset();
        }
        // audio thread
        void push (const float* l, const float* r, int n)
        {
            if (resetReq.exchange (false)) doReset();
            if (r == nullptr) r = l;
            float maxNow = 0.f;
            for (int i = 0; i < n; ++i)
            {
                const float al = std::abs (l[i]), ar = std::abs (r[i]);
                dispL = juce::jmax (al, dispL * fall); dispR = juce::jmax (ar, dispR * fall);
                maxNow = juce::jmax (maxNow, al, ar);
                if (al > 1.f || ar > 1.f) clip.store (true, std::memory_order_relaxed);
                // true peak: the full interpolation only near the current maximum (cheap while quiet)
                const float skip = (float) (tpMax * 0.7);
                tpMax = juce::jmax (tpMax, (double) tpd[0].push (l[i], skip), (double) tpd[1].push (r[i], skip));
                ms += rmsK * (0.5 * ((double) l[i] * l[i] + (double) r[i] * r[i]) - ms);
                dcL += dcK * ((double) l[i] - dcL); dcR += dcK * ((double) r[i] - dcR);
                lu.push (l[i], r[i]);
            }
            if (maxNow >= holdV || (holdCount -= n) <= 0) { holdV = maxNow; holdCount = holdN; }
            maxPk = juce::jmax (maxPk, (double) maxNow);
            peakL.store (dispL, std::memory_order_relaxed); peakR.store (dispR, std::memory_order_relaxed);
            peakMax.store ((float) maxPk, std::memory_order_relaxed); peakHold.store (holdV, std::memory_order_relaxed);
            truePeak.store ((float) juce::jmax (tpMax, maxPk), std::memory_order_relaxed);
            rms.store ((float) std::sqrt (ms), std::memory_order_relaxed);
            lufsM.store (lu.momentaryLufs()); lufsS.store (lu.shortTermLufs()); lufsI.store (lu.integratedLufs());
            dcOffset.store ((float) juce::jmax (std::abs (dcL), std::abs (dcR)), std::memory_order_relaxed);
        }
        void requestReset() { resetReq.store (true); }
        // GUI side
        std::atomic<float> peakL { 0 }, peakR { 0 }, peakMax { 0 }, peakHold { 0 }, truePeak { 0 }, rms { 0 }, lufsM { -70 }, lufsS { -70 }, lufsI { -70 }, dcOffset { 0 };
        std::atomic<bool> clip { false };
        bool dcWarning() const { return dcOffset.load() > 0.003f; }
    private:
        void doReset()
        {
            for (auto& t : tpd) t.reset();
            tpMax = maxPk = 0.0; holdV = 0.f; holdCount = 0; dispL = dispR = 0.f; lu.reset(); clip.store (false);
            peakMax.store (0.f); truePeak.store (0.f);
        }
        double sr = 44100.0, rmsK = 0.001, dcK = 0.0001, ms = 0.0, dcL = 0.0, dcR = 0.0, tpMax = 0.0, maxPk = 0.0;
        float dispL = 0.f, dispR = 0.f, holdV = 0.f, fall = 0.999f;
        int holdN = 88200, holdCount = 0;
        std::array<doomsday::dyn::TruePeak, 2> tpd;
        doomsday::dyn::Loudness lu;
        std::atomic<bool> resetReq { false };
    };
}
