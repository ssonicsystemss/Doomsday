#pragma once
// =====================================================================
//  4GE module 08 - the DYNAMICS page's displays, computed on a background thread from the REAL DSP code (never on the message thread):
//   CURVE    - the static curve of the selected processor: compressor dB in -> dB out (dyn::cmpGainDb), clipper / limiter
//              linear in -> out (dyn::clipCurve / the ceiling), transient shaper = none (its gain over time is the history);
//   HISTORY  - the kick rendered with a Probe on that processor: gain reduction (or the transient gain), input and output envelopes
//              over the first 600 ms;
//   CREST    - the whole kick: peak, RMS, crest factor, short-term crest over time, and the same of the kick with module 08 off
//              (a level jump warning when the processed kick is much louder / quieter).
// =====================================================================
#include <juce_gui_basics/juce_gui_basics.h>
#include "../DSP/Engine.h"
#include "../DSP/Dyn/DynCook.h"
#include <mutex>

namespace fge::gui
{
    class DynScope : private juce::Thread, private juce::AsyncUpdater
    {
    public:
        using Tables = std::array<dsp::TablePtr, 8>;
        struct Request { ParamFrame f; env::Bank bank; Tables tables; dsp::dist::DistData dist; int kind = 0, index = 0, note = 31; };
        static constexpr int histN = 300, curveN = 121;
        struct Result
        {
            std::vector<float> curveX, curveY;                        // (compressor: dB, clipper / limiter: linear -2..2)
            std::vector<float> gr, in, out;                           // dB per history point (600 ms)
            float peak = -120.f, rms = -120.f, crest = 0.f, dryPeak = -120.f, dryRms = -120.f, dryCrest = 0.f, maxGr = 0.f;
            std::vector<float> crestHist;                             // short-term crest (dB) per history point
            int kind = -1, index = -1; bool valid = false;
        };
        static constexpr double histMs = 600.0;

        DynScope() : juce::Thread ("4GE dyn scope") { startThread (juce::Thread::Priority::low); }
        ~DynScope() override { signalThreadShouldExit(); wake.signal(); stopThread (4000); }
        void ask (std::unique_ptr<Request> r) { std::lock_guard<std::mutex> l (lock); pending = std::move (r); wake.signal(); }
        Result result() const { std::lock_guard<std::mutex> l (lock); return shown; }
        std::function<void()> onReady;

        // the ids that switch every module 08 processor off (the "dry" comparison)
        static ParamFrame withoutDyn (ParamFrame f)
        {
            for (int i = 0; i < 4; ++i) f[P::cmp1On + i * (P::cmp2On - P::cmp1On)] = 0.f;
            for (int i = 0; i < 5; ++i) f[P::ts1On + i * (P::ts2On - P::ts1On)] = 0.f;
            for (int i = 0; i < 2; ++i) { f[P::clp1On + i * (P::clp2On - P::clp1On)] = 0.f; f[P::lim1On + i * (P::lim2On - P::lim1On)] = 0.f; }
            f[P::crOn] = 0.f; f[P::dynAuto] = 0.f; f[P::dynMatch] = 0.f;
            return f;
        }
        static Result analyse (const Request& q)
        {
            namespace dy = dsp::dyn;
            Result r; r.kind = q.kind; r.index = q.index;
            const double rate = 44100.0 * 4.0;
            auto bank = std::make_unique<dy::Bank>(); dy::cookBank (q.f, &q.dist, *bank, rate, 44100.0);
            // ---- the static curve
            if (q.kind == 0)
            {
                const auto& s = bank->cmp[(size_t) q.index];
                for (int k = 0; k < curveN; ++k) { const float in = -60.f + 60.f * (float) k / (float) (curveN - 1); r.curveX.push_back (in); r.curveY.push_back (in + dy::cmpGainDb (s, in) + juce::Decibels::gainToDecibels (s.outG)); }
            }
            else if (q.kind == 2 || q.kind == 3)
            {
                for (int k = 0; k < curveN; ++k)
                {
                    const float x = q.kind == 2 ? -1.f + 2.f * (float) k / (float) (curveN - 1) : -2.f + 4.f * (float) k / (float) (curveN - 1);   // (clipper: the editor's -1..1)
                    float y;
                    if (q.kind == 2) y = dy::clipCurve (bank->clp[(size_t) q.index], x);
                    else { const auto& s = bank->lim[(size_t) q.index]; const float a = std::abs (x * s.inG); y = std::copysign (juce::jmin (a, s.ceil), x); }
                    r.curveX.push_back (x); r.curveY.push_back (y);
                }
            }
            // ---- history (the real render with a probe on the processor) + crest
            dsp::OscTables ot; for (size_t i = 0; i < q.tables.size(); ++i) ot.t[i] = q.tables[i].get();
            dy::Probe probe; probe.kind = q.kind; probe.index = q.index;
            const double secs = 0.6;
            auto post = renderKick (q.f, q.note, 44100.0, secs, -1, 1.f, &q.bank, 0.0, -1.0, &ot, nullptr, &q.dist, &probe);
            auto dry = renderKick (withoutDyn (q.f), q.note, 44100.0, secs, -1, 1.f, &q.bank, 0.0, -1.0, &ot, nullptr, &q.dist);
            if (! probe.gr.empty())
            {
                // the probe runs at the place's rate (engine 4x, MASTER = the host rate): resample its whole length onto the history
                const int loc = q.kind == 0 ? bank->cmp[(size_t) q.index].loc : q.kind == 1 ? bank->ts[(size_t) q.index].loc : q.kind == 2 ? bank->clp[(size_t) q.index].loc
                              : q.kind == 3 ? bank->lim[(size_t) q.index].loc : bank->crest.loc;
                const double placeRate = loc == dy::LMaster ? 44100.0 : rate;
                const int per = juce::jmax (1, (int) (placeRate * histMs / 1000.0 / histN));
                for (int h = 0; h < histN; ++h)
                {
                    float g = 0.f, a = 0.f, b = 0.f; bool any = false;
                    for (int k = h * per; k < (h + 1) * per && k < (int) probe.gr.size(); ++k)
                    { g = any ? (q.kind == 1 ? (std::abs (probe.gr[(size_t) k]) > std::abs (g) ? probe.gr[(size_t) k] : g) : juce::jmin (g, probe.gr[(size_t) k])) : probe.gr[(size_t) k];
                      a = juce::jmax (a, std::abs (probe.in[(size_t) k])); b = juce::jmax (b, std::abs (probe.out[(size_t) k])); any = true; }
                    r.gr.push_back (g);
                    r.in.push_back (juce::Decibels::gainToDecibels (a, -90.f)); r.out.push_back (juce::Decibels::gainToDecibels (b, -90.f));
                    r.maxGr = q.kind == 1 ? (std::abs (g) > std::abs (r.maxGr) ? g : r.maxGr) : juce::jmin (r.maxGr, g);
                }
            }
            auto stats = [] (const juce::AudioBuffer<float>& x, float& pk, float& rm, float& cr)
            {
                double p = 0, ms = 0; const int n = x.getNumSamples();
                for (int i = 0; i < n; ++i) { const double v = 0.5 * (x.getSample (0, i) + x.getSample (1, i)); p = juce::jmax (p, std::abs (v)); ms += v * v; }
                pk = juce::Decibels::gainToDecibels ((float) p, -120.f); rm = juce::Decibels::gainToDecibels ((float) std::sqrt (ms / juce::jmax (1, n)), -120.f); cr = pk - rm;
            };
            stats (post, r.peak, r.rms, r.crest); stats (dry, r.dryPeak, r.dryRms, r.dryCrest);
            const int per = juce::jmax (1, (int) (44100.0 * histMs / 1000.0 / histN)), win = (int) (0.01 * 44100.0);
            for (int h = 0; h < histN; ++h)
            {
                const int c = h * per; double p = 0, ms = 0; int n = 0;
                for (int i = juce::jmax (0, c - win / 2); i < juce::jmin (post.getNumSamples(), c + win / 2); ++i) { const double v = 0.5 * (post.getSample (0, i) + post.getSample (1, i)); p = juce::jmax (p, std::abs (v)); ms += v * v; ++n; }
                r.crestHist.push_back (n > 0 && ms > 1.0e-12 ? (float) (20.0 * std::log10 (p / std::sqrt (ms / n))) : 0.f);
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
