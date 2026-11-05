#pragma once
// =====================================================================
//  4GE module 05 - the DISTORTION VISUALIZER data (computed on a background thread, never on the message / audio thread):
//   TRANSFER CURVE - the selected stage's static curve from the SAME cooked tables the audio runs (dist::staticCurve);
//   WAVEFORM       - the real kick with the stage BYPASSED (before) and ACTIVE (after), min / max per column;
//   SPECTRUM       - FFT (Hann, 8192) of both renders, channel L / R / MID / SIDE, log-spaced bins 20 Hz .. 20 kHz.
// =====================================================================
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../DSP/Engine.h"
#include "../DSP/Dist/DistCook.h"
#include <mutex>

namespace fge::gui
{
    class DistScope : private juce::Thread, private juce::AsyncUpdater
    {
    public:
        using Tables = std::array<dsp::TablePtr, 8>;
        struct Request { ParamFrame f; env::Bank bank; Tables tables; dsp::dist::DistData dist; int stage = 0, channel = 0, note = 31; };
        struct Result
        {
            int stage = -1;
            std::vector<float> curveX, curveY;                       // input -> output (incl. drive / bias / output / mix)
            std::vector<float> loB, hiB, loA, hiA;                   // waveform before / after (min / max per column)
            std::vector<float> specB, specA;                         // dB per log bin (before / after)
            float peak = 0.f;
            bool valid = false;
        };
        static constexpr int columns = 300, bins = 256;
        static float binHz (int k) { return 20.f * std::pow (1000.f, (float) k / (float) (bins - 1)); }

        DistScope() : juce::Thread ("4GE dist scope") { startThread (juce::Thread::Priority::low); }
        ~DistScope() override { signalThreadShouldExit(); wake.signal(); stopThread (4000); }
        void ask (std::unique_ptr<Request> r) { std::lock_guard<std::mutex> l (lock); pending = std::move (r); wake.signal(); }
        Result result() const { std::lock_guard<std::mutex> l (lock); return shown; }
        std::function<void()> onReady;

        // the whole analysis (also called by the tests)
        static Result analyse (const Request& q)
        {
            Result r; r.stage = q.stage;
            namespace dd = dsp::dist;
            const int st = juce::jlimit (0, dd::numStages - 1, q.stage);
            // ---- TRANSFER CURVE: cook the stage exactly like the engine does (same tables), read its static curve
            {
                auto bank = std::make_unique<dd::DistBank>();
                auto tabs = std::make_unique<dd::DistTables>(); tabs->allocate();
                ParamFrame g = q.f; g[P::dist1On + st * (P::dist2On - P::dist1On)] = 1.f;
                dd::cookDist (g, &q.dist, q.bank, *bank, *tabs, 44100.0 * 4.0, false);
                const auto& s = bank->st[(size_t) st];
                for (int i = 0; i < columns; ++i)
                {
                    const float x = -1.5f + 3.f * (float) i / (float) (columns - 1);
                    r.curveX.push_back (x); r.curveY.push_back (dd::staticCurve (s, x));
                }
            }
            // ---- the real kick, the stage bypassed / active
            dsp::OscTables ot; for (size_t i = 0; i < q.tables.size(); ++i) ot.t[i] = q.tables[i].get();
            ParamFrame off = q.f; off[P::dist1On + st * (P::dist2On - P::dist1On)] = 0.f;
            const double secs = 0.5;
            auto b = renderKick (off, q.note, 44100.0, secs, -1, 1.f, &q.bank, 0.0, -1.0, &ot, nullptr, &q.dist);
            auto a = renderKick (q.f, q.note, 44100.0, secs, -1, 1.f, &q.bank, 0.0, -1.0, &ot, nullptr, &q.dist);
            auto chan = [&] (const juce::AudioBuffer<float>& x, int i)
            {
                const float L = x.getSample (0, i), R = x.getSample (1, i);
                return q.channel == 1 ? L : q.channel == 2 ? R : q.channel == 3 ? 0.5f * (L - R) : 0.5f * (L + R);   // MID / L / R / SIDE
            };
            auto wave = [&] (const juce::AudioBuffer<float>& x, std::vector<float>& lo, std::vector<float>& hi)
            {
                const int n = x.getNumSamples(); lo.assign (columns, 0.f); hi.assign (columns, 0.f);
                for (int k = 0; k < columns; ++k)
                    for (int i = k * n / columns; i < (k + 1) * n / columns; ++i) { const float v = chan (x, i); lo[(size_t) k] = juce::jmin (lo[(size_t) k], v); hi[(size_t) k] = juce::jmax (hi[(size_t) k], v); r.peak = juce::jmax (r.peak, std::abs (v)); }
            };
            wave (b, r.loB, r.hiB); wave (a, r.loA, r.hiA);
            auto spec = [&] (const juce::AudioBuffer<float>& x, std::vector<float>& out)
            {
                constexpr int order = 13, n = 1 << order;
                juce::dsp::FFT fft (order);
                std::vector<float> buf ((size_t) n * 2, 0.f);
                const int m = juce::jmin (n, x.getNumSamples());
                for (int i = 0; i < m; ++i) buf[(size_t) i] = chan (x, i) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) m));
                fft.performFrequencyOnlyForwardTransform (buf.data());
                out.assign (bins, -140.f);
                for (int k = 0; k < bins; ++k)
                {
                    const float f0 = binHz (k) / std::pow (1000.f, 0.5f / (bins - 1)), f1 = binHz (k) * std::pow (1000.f, 0.5f / (bins - 1));
                    const int i0 = juce::jlimit (1, n / 2 - 1, (int) (f0 * n / 44100.f)), i1 = juce::jlimit (i0, n / 2 - 1, (int) (f1 * n / 44100.f));
                    float e = 0.f; for (int i = i0; i <= i1; ++i) e = juce::jmax (e, buf[(size_t) i]);
                    out[(size_t) k] = juce::Decibels::gainToDecibels (e * 4.f / (float) m, -140.f);
                }
            };
            spec (b, r.specB); spec (a, r.specA);
            r.valid = true;
            return r;
        }

    private:
        void run() override
        {
            while (! threadShouldExit())
            {
                wake.wait (250);
                std::unique_ptr<Request> q;
                { std::lock_guard<std::mutex> l (lock); q = std::move (pending); }
                if (q == nullptr) continue;
                auto r = analyse (*q);
                { std::lock_guard<std::mutex> l (lock); if (pending != nullptr) continue; shown = std::move (r); }
                triggerAsyncUpdate();
            }
        }
        void handleAsyncUpdate() override { if (onReady) onReady(); }
        mutable std::mutex lock;
        std::unique_ptr<Request> pending;
        Result shown;
        juce::WaitableEvent wake;
    };
}
