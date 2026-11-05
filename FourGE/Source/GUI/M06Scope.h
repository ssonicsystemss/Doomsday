#pragma once
// =====================================================================
//  4GE module 06 - the page's displays, computed on a background thread from the REAL DSP code (never on the message thread):
//   FILTER RESPONSE    - an impulse through the selected filter instance (its own algorithm / state), FFT -> dB per log bin,
//                        at the start of the kick (envelope peak) and later (envelope at rest) - you see the envelope move it;
//   ENVELOPE           - the filter / resonator pitch envelope sampled with m06::envValue (the function the engine runs);
//   RESONANCE          - an impulse through the selected resonator (+ its pitch over time);
//   SCREECH            - the kick rendered with the screech alone (waveform min / max + spectrum).
// =====================================================================
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../DSP/Engine.h"
#include <mutex>

namespace fge::gui
{
    class M06Scope : private juce::Thread, private juce::AsyncUpdater
    {
    public:
        using Tables = std::array<dsp::TablePtr, 8>;
        struct Request { ParamFrame f; env::Bank bank; Tables tables; int filter = 0, res = 0; };
        struct Result
        {
            std::vector<float> fltStart, fltLater, resResp;        // dB per log bin (30 Hz .. 20 kHz)
            std::vector<float> fltEnv, resPitch;                   // 0..1 / Hz over envMs
            float envMs = 1000.f;
            std::vector<float> scrLo, scrHi, scrSpec; float scrPeak = 0.f, scrStartMs = 0.f, scrEndMs = 0.f;
            bool valid = false;
        };
        static constexpr int bins = 200, columns = 300;
        static float binHz (int k) { return 30.f * std::pow (20000.f / 30.f, (float) k / (float) (bins - 1)); }

        M06Scope() : juce::Thread ("4GE m06 scope") { startThread (juce::Thread::Priority::low); }
        ~M06Scope() override { signalThreadShouldExit(); wake.signal(); stopThread (4000); }
        void ask (std::unique_ptr<Request> r) { std::lock_guard<std::mutex> l (lock); pending = std::move (r); wake.signal(); }
        Result result() const { std::lock_guard<std::mutex> l (lock); return shown; }
        std::function<void()> onReady;

        static std::vector<float> spectrum (const std::vector<float>& x, double rate)
        {
            constexpr int order = 14, n = 1 << order;
            juce::dsp::FFT fft (order); std::vector<float> buf ((size_t) n * 2, 0.f);
            for (int i = 0; i < juce::jmin (n, (int) x.size()); ++i) buf[(size_t) i] = x[(size_t) i];
            fft.performFrequencyOnlyForwardTransform (buf.data());
            std::vector<float> o ((size_t) bins);
            for (int k = 0; k < bins; ++k) o[(size_t) k] = juce::Decibels::gainToDecibels (buf[(size_t) juce::jlimit (1, n / 2 - 1, (int) (binHz (k) * n / rate))], -120.f);
            return o;
        }
        static Result analyse (const Request& q)
        {
            namespace m6 = dsp::m06;
            Result r;
            const double rate = 44100.0 * 4.0;
            auto bank = std::make_unique<m6::Bank>();
            ParamFrame g = q.f;
            const int fi = juce::jlimit (0, m6::numFilters - 1, q.filter), ri = juce::jlimit (0, m6::numRes - 1, q.res);
            g[P::flt1On + fi * (P::flt2On - P::flt1On)] = 1.f;
            g[P::res1On + ri * (P::res2On - P::res1On)] = 1.f;
            m6::cookBank (g, q.bank, *bank, rate);
            // ---- filter response at the start (envelope peak) and at rest
            auto filterAt = [&] (float ms)
            {
                auto st = std::make_unique<m6::State>(); st->reset();
                m6::Ctx cx; cx.ms = ms; cx.lengthMs = q.f[P::coreLength]; cx.vel = 1.f;
                const auto& fs = bank->f[(size_t) fi];
                std::vector<float> x (1 << 14);
                for (size_t i = 0; i < x.size(); ++i) { float l = i == 0 ? 1.f : 0.f, rr = l; auto& s = st->f[(size_t) fi]; if ((s.tick++ & 15) == 0) s.update (fs, cx, rate); s.process (fs, l, rr); x[i] = l; }
                return spectrum (x, rate);
            };
            const auto& fs = bank->f[(size_t) fi];
            float peakMs = fs.e.a; if (fs.e.sync) peakMs *= q.f[P::coreLength] / 450.f;
            r.fltStart = filterAt (peakMs); r.fltLater = filterAt (juce::jmax (fs.e.a + fs.e.d, 1.f) * 3.f + 5.f);
            // ---- envelope curves
            r.envMs = juce::jlimit (100.f, 5000.f, q.f[P::coreLength] + 100.f);
            for (int k = 0; k < columns; ++k)
            {
                m6::Ctx cx; cx.ms = r.envMs * (float) k / (float) (columns - 1); cx.lengthMs = q.f[P::coreLength];
                r.fltEnv.push_back (m6::envValue (fs.e, cx));
                m6::ResState rs; r.resPitch.push_back (rs.baseHz (bank->r[(size_t) ri], cx));
            }
            // ---- resonator: an impulse through it (internal sources: its own excitation)
            {
                auto st = std::make_unique<m6::State>(); st->reset();
                std::vector<float> x (1 << 14);
                m6::Ctx cx; cx.lengthMs = q.f[P::coreLength];
                const auto& rs = bank->r[(size_t) ri];
                for (size_t i = 0; i < x.size(); ++i)
                {
                    cx.ms = (float) (i * 1000.0 / rate);
                    float l = i == 0 ? 1.f : 0.f, rr = l;
                    auto& s = st->r[(size_t) ri];
                    if ((s.tick++ & 15) == 0) s.update (rs, cx, rate);
                    s.process (rs, l, rr, cx, rate);
                    x[i] = l;
                }
                r.resResp = spectrum (x, rate);
            }
            // ---- the screech alone (the kick parts muted, the screech on TEXTURE so the part faders do not matter)
            if (q.f.on (P::scrOn))
            {
                ParamFrame s = q.f;
                for (int id : { P::mixPunchMute, P::mixBodyMute, P::mixTailMute, P::mixSubMute }) s[id] = 1.f;
                for (int i = 0; i < 8; ++i) s[P::layer1On + i * (P::layer2On - P::layer1On)] = 0.f;
                s[P::noiseNAOn] = s[P::noiseNBOn] = s[P::trOn] = 0.f; s[P::scrOutput] = 1.f;
                dsp::OscTables ot; for (size_t i = 0; i < q.tables.size(); ++i) ot.t[i] = q.tables[i].get();
                const double secs = juce::jlimit (0.3, 3.0, (double) (q.f[P::coreLength] + 300.f) / 1000.0);
                auto x = renderKick (s, 31, 44100.0, secs, -1, 1.f, &q.bank, 0.0, -1.0, &ot);
                const int n = x.getNumSamples();
                r.scrLo.assign (columns, 0.f); r.scrHi.assign (columns, 0.f);
                for (int k = 0; k < columns; ++k)
                    for (int i = k * n / columns; i < (k + 1) * n / columns; ++i) { const float v = 0.5f * (x.getSample (0, i) + x.getSample (1, i)); r.scrLo[(size_t) k] = juce::jmin (r.scrLo[(size_t) k], v); r.scrHi[(size_t) k] = juce::jmax (r.scrHi[(size_t) k], v); r.scrPeak = juce::jmax (r.scrPeak, std::abs (v)); }
                r.scrEndMs = (float) (secs * 1000.0);
                std::vector<float> m ((size_t) n); for (int i = 0; i < n; ++i) m[(size_t) i] = x.getSample (0, i) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) n));
                r.scrSpec = spectrum (m, 44100.0);
                for (auto& v : r.scrSpec) v -= 20.f * std::log10 ((float) n * 0.25f);
            }
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
