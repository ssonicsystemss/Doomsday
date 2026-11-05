#pragma once
// =====================================================================
//  4GE - background renders for the GUI (module 04 displays): a list of JOBS, each = the current parameters changed by a
//  small function (e.g. "only layer 3", "only the noise"), rendered with fge::renderKick on this thread and reduced to a
//  min / max envelope for drawing. Also runs the PHASE ALIGN analysis. GUI never runs DSP on the message thread.
// =====================================================================
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Parameters/ParamFrame.h"
#include "../DSP/Layers/PhaseAlign.h"
#include <functional>
#include <mutex>

namespace fge::gui
{
    class KickPreview : private juce::Thread, private juce::AsyncUpdater
    {
    public:
        struct Job { std::function<void (ParamFrame&)> edit; float seconds = 0.4f; };
        struct Wave { std::vector<float> lo, hi; float peak = 0.f; bool silent = true; };
        using Tables = std::array<dsp::TablePtr, 8>;

        KickPreview() : juce::Thread ("4GE previews") { startThread (juce::Thread::Priority::low); }
        ~KickPreview() override { signalThreadShouldExit(); wake.signal(); stopThread (4000); }
        void setJobs (std::vector<Job> j) { std::lock_guard<std::mutex> l (lock); jobs = std::move (j); dirty = true; wake.signal(); }
        // re-render when the parameters / shapes / tables change
        void update (const ParamFrame& f, const env::Bank& bank, uint32_t bankVersion, const Tables& t)
        {
            if (bankVersion == lastBank && t == lastTables && std::equal (f.v.begin(), f.v.end(), lastF.v.begin())) return;
            lastBank = bankVersion; lastTables = t; lastF = f;
            std::lock_guard<std::mutex> l (lock); frame = f; tables = t; if (! bankCopy) bankCopy = std::make_unique<env::Bank>(); *bankCopy = bank; dirty = true; wake.signal();
        }
        std::vector<Wave> waves() const { std::lock_guard<std::mutex> l (lock); return result; }
        // PHASE ALIGN: analysed here; 'done' is called on the message thread
        void analyse (int ref, int target, float fromMs, float lenMs, float search, int note, std::function<void (const dsp::AlignResult&)> done)
        {
            std::lock_guard<std::mutex> l (lock); align = { true, ref, target, fromMs, lenMs, search, note }; onAligned = std::move (done); wake.signal();
        }
        std::function<void()> onRendered;
        static constexpr int points = 160;

    private:
        void run() override
        {
            while (! threadShouldExit())
            {
                wake.wait (250);
                std::vector<Job> j; ParamFrame f; Tables t; env::Bank b; bool doRender = false; AlignReq a;
                {
                    std::lock_guard<std::mutex> l (lock);
                    if (dirty && bankCopy) { j = jobs; f = frame; t = tables; b = *bankCopy; dirty = false; doRender = true; }
                    a = align; align.on = false;
                    if (a.on && bankCopy) { f = frame; t = tables; b = *bankCopy; }
                }
                dsp::OscTables ot; for (size_t i = 0; i < t.size(); ++i) ot.t[i] = t[i].get();
                if (a.on)
                {
                    auto r = dsp::analysePhase (f, a.ref, a.target, a.fromMs, a.lenMs, a.search, a.note, &b, &ot);
                    std::function<void (const dsp::AlignResult&)> cb;
                    { std::lock_guard<std::mutex> l (lock); cb = onAligned; }
                    juce::MessageManager::callAsync ([cb, r] { if (cb) cb (r); });
                }
                if (! doRender) continue;
                std::vector<Wave> out;
                for (auto& job : j)
                {
                    if (threadShouldExit()) return;
                    ParamFrame g = f; if (job.edit) job.edit (g);
                    auto x = renderKick (g, 31, 44100.0, job.seconds, -1, 1.f, &b, 0.0, -1.0, &ot);
                    Wave w; w.lo.assign (points, 0.f); w.hi.assign (points, 0.f);
                    const int n = x.getNumSamples();
                    for (int k = 0; k < points; ++k)
                    {
                        float lo = 0.f, hi = 0.f;
                        for (int i = k * n / points; i < (k + 1) * n / points; ++i) { const float v = 0.5f * (x.getSample (0, i) + x.getSample (1, i)); lo = juce::jmin (lo, v); hi = juce::jmax (hi, v); }
                        w.lo[(size_t) k] = lo; w.hi[(size_t) k] = hi; w.peak = juce::jmax (w.peak, -lo, hi);
                    }
                    w.silent = w.peak < 1.0e-5f;
                    out.push_back (std::move (w));
                }
                { std::lock_guard<std::mutex> l (lock); result = std::move (out); }
                triggerAsyncUpdate();
            }
        }
        void handleAsyncUpdate() override { if (onRendered) onRendered(); }
        struct AlignReq { bool on = false; int ref = 0, target = 1; float fromMs = 0.f, lenMs = 50.f, search = 360.f; int note = 31; };
        mutable std::mutex lock;
        std::vector<Job> jobs;
        ParamFrame frame, lastF; Tables tables, lastTables; uint32_t lastBank = 0;
        std::unique_ptr<env::Bank> bankCopy;
        bool dirty = false;
        std::vector<Wave> result;
        AlignReq align;
        std::function<void (const dsp::AlignResult&)> onAligned;
        juce::WaitableEvent wake;
    };
}
