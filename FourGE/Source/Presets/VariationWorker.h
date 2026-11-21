#pragma once
// =====================================================================
//  4GE - KICK VARIATION worker (VARIATION_DNA.md): every candidate is RENDERED, MEASURED (module 16 KickAnalysis), CHECKED, SCORED,
//  and 7 that differ from each other are chosen. A background thread (renders never on the message / audio thread); no GUI.
//   checks: hard rules (the note, a mono low end, the sub, no clip / silence, the transient), KEEP anchors (a kept part must SOUND the same),
//           directions (BRIGHTER must measure brighter). Too few survivors -> the tolerances are relaxed once (shown as "relaxed").
// =====================================================================
#include "KickVariation.h"
#include "../DSP/Analysis/KickAnalysis.h"
#include <mutex>
#include <thread>
#include <atomic>

namespace fge::kv
{
    namespace ka = fge::dsp::analysis;

    // ---- what each part's measurement is (vs the BASE), its threshold for a DIRECTION and its tolerance for KEEP
    struct Probe
    {
        ka::Report r; float rms = 0.f;
        float harmHi() const { float s = 0.f; for (int i = 2; i < 10; ++i) s += r.harm[(size_t) i]; return s / 8.f; }
        float early() const { float s = 0.f; int n = 0; for (size_t i = 0; i < r.env.size() && i < 6; ++i) { s += r.env[i]; ++n; } return n ? s / (float) n : 0.f; }
    };
    // the pitch curve, frame by frame (5 ms): the mean difference in cents over [from, to) where both are confident
    inline float pitchCents (const Probe& a, const Probe& b, int from, int to)
    {
        double s = 0.0; int n = 0;
        for (int k = from; k < to && k < (int) a.r.pitch.size() && k < (int) b.r.pitch.size(); ++k)
        {
            const float x = a.r.pitch[(size_t) k], y = b.r.pitch[(size_t) k];
            if (x > 0.f && y > 0.f && a.r.pitchConf[(size_t) k] > 0.5f && b.r.pitchConf[(size_t) k] > 0.5f) { s += 1200.0 * std::log2 (x / y); ++n; }
        }
        return n ? (float) (s / n) : 0.f;
    }
    inline float partMetric (int gene, const Probe& a, const Probe& b)       // a - b, + = the UP direction of that part
    {
        switch (gene)
        {
            case GPitch:   return pitchCents (a, b, 3, 24);                                   // the drop 15-120 ms (the punch's first ms are the PUNCH's)
            case GPunch:   return a.r.transientDb - b.r.transientDb;
            case GBody:    return a.r.bandDb[2] - b.r.bandDb[2];
            case GTail:    return 0.5f * (a.r.bandDb[3] + a.r.bandDb[4]) - a.r.bandDb[1] - (0.5f * (b.r.bandDb[3] + b.r.bandDb[4]) - b.r.bandDb[1]);
            case GWeight:  return a.r.bandDb[0] - b.r.bandDb[0];
            case GDirt:    return a.harmHi() - b.harmHi();
            case GLength:  return b.r.lengthMs > 0.f ? 100.f * (a.r.lengthMs / b.r.lengthMs - 1.f) : 0.f;
            case GSpace:   return a.r.sideDb - b.r.sideDb;
            case GScreech: return a.r.bandDb[4] - b.r.bandDb[4];
            default:       return 0.f;
        }
    }
    constexpr float dirThreshold[NumGenes] { 40.f, 0.6f, 0.7f, 0.6f, 0.7f, 0.7f, 4.f, 1.5f, 1.f };       // cents, dB, dB, dB, dB, dB, %, dB, dB
    constexpr float keepTolerance[NumGenes] { 50.f, 1.f, 1.5f, 1.5f, 1.f, 1.5f, 3.f, 2.f, 2.f };
    // extra KEEP anchors beyond the main metric
    inline bool keepExtra (int gene, const Probe& a, const Probe& b, float relax)
    {
        switch (gene)
        {
            case GPitch: return std::abs (pitchCents (a, b, 24, 60)) <= 30.f * relax;          // (and the settled part 120-300 ms)
            case GPunch: return std::abs (a.r.attackMs - b.r.attackMs) <= 1.f * relax && std::abs (a.early() - b.early()) <= 1.f * relax && std::abs (a.r.bandDb[4] - b.r.bandDb[4]) <= 1.5f * relax;
            case GTail:  return std::abs (a.r.bandDb[3] - b.r.bandDb[3]) <= 1.5f * relax && std::abs (a.harmHi() - b.harmHi()) <= 1.5f * relax;
            case GWeight:return std::abs (a.r.subShare - b.r.subShare) <= 3.f * relax;
            case GDirt:  return std::abs (a.r.crestDb - b.r.crestDb) <= 1.f * relax && std::abs (a.r.bandDb[5] - b.r.bandDb[5]) <= 1.5f * relax;
            case GSpace: return std::abs (a.r.corr - b.r.corr) <= 0.05f * relax;
            default:     return true;
        }
    }
    // HARD RULES: a hardstyle kick stays a hardstyle kick
    inline juce::String hardRule (const Probe& a, const Probe& b)
    {
        if (a.r.m.samplePeakDb < -60.f) return "silent";
        if (a.r.endHz > 0.f && b.r.endHz > 0.f && std::abs (1200.f * std::log2 (a.r.endHz / b.r.endHz)) > 25.f) return "note moved";
        if (b.r.lowCorr >= 0.9f && a.r.lowCorr < 0.9f) return "low end not mono";
        if (a.r.subShare < 0.6f * b.r.subShare) return "sub lost";
        if (a.r.m.dc) return "DC";
        if (a.r.clipped > juce::jmax (8, 2 * b.r.clipped)) return "clipping";
        if (a.r.transientDb < b.r.transientDb - 6.f) return "transient lost";
        if (a.r.crestDb < juce::jmin (3.f, b.r.crestDb - 4.f)) return "over-driven";
        return {};
    }
    // a distance between two kicks as heard (each feature in units of "clearly audible")
    inline float heardDistance (const Probe& a, const Probe& b)
    {
        double e = 0.0; int n = 0;
        auto add = [&] (float d) { e += (double) d * d; ++n; };
        for (int g = 0; g < NumGenes; ++g) add (partMetric (g, a, b) / dirThreshold[g]);
        add ((a.r.attackMs - b.r.attackMs) / 1.f);
        add ((a.r.crestDb - b.r.crestDb) / 1.5f);
        if (b.r.dropMs > 0.f && a.r.dropMs > 0.f) add (10.f * std::log (a.r.dropMs / b.r.dropMs));
        return (float) std::sqrt (e / juce::jmax (1, n));
    }
    inline juce::String labelFor (const Probe& a, const Probe& b, const Dna& dna)
    {
        std::vector<std::pair<float, juce::String>> l;
        for (int g = 0; g < NumGenes; ++g)
        {
            if (dna[(size_t) g] == Keep) continue;
            const float x = partMetric (g, a, b) / dirThreshold[g];
            if (std::abs (x) >= 1.f) l.push_back ({ std::abs (x), dirName (g, x > 0.f).toLowerCase() });
        }
        std::sort (l.begin(), l.end(), [] (auto& x, auto& y) { return x.first > y.first; });
        juce::StringArray s; for (size_t i = 0; i < l.size() && i < 3; ++i) s.add (l[i].second);
        return s.isEmpty() ? juce::String ("subtle") : s.joinIntoString (", ");
    }

    class VariationWorker : private juce::Thread, private juce::AsyncUpdater
    {
    public:
        using Tables = std::array<dsp::TablePtr, 8>;
        struct Wave { std::vector<float> lo, hi; float peak = 0.f; };
        struct Request
        {
            uint32_t tag = 0;
            ParamFrame base; std::vector<ParamFrame> cands; std::vector<Dirs> signs;   // per candidate: the direction of each part (0 = none)
            Dna dna {}; float variation = 0.5f; bool fresh = false;
            bool hasStyleRef = false; ParamFrame styleRef;
            env::Bank bank; Tables tables; dsp::dist::DistData dist;
            int note = 31; double seconds = 0.5;
        };
        struct Result
        {
            uint32_t tag = 0;
            std::vector<ParamFrame> picked; juce::StringArray labels; std::vector<Wave> waves;   // waves[0] = the BASE
            int tried = 0, passed = 0; bool relaxed = false; juce::StringArray rejects;
        };
        VariationWorker() : juce::Thread ("4GE variation") { startThread (juce::Thread::Priority::low); }
        ~VariationWorker() override { cancelPendingUpdate(); signalThreadShouldExit(); wake.signal(); stopThread (6000); }
        void submit (std::unique_ptr<Request> r) { std::lock_guard<std::mutex> l (lock); pending = std::move (r); progressDone = 0; progressOf = 1; wake.signal(); }
        std::unique_ptr<Result> take() { std::lock_guard<std::mutex> l (lock); return std::move (done); }
        bool busy() const { std::lock_guard<std::mutex> l (lock); return working || pending != nullptr; }
        float progress() const { return (float) progressDone.load() / (float) juce::jmax (1, progressOf.load()); }
        std::function<void()> onDone;

        // offline (tests): the whole job on the calling thread
        static Result process (const Request& q, const std::atomic<bool>* cancel = nullptr, std::atomic<int>* prog = nullptr)
        {
            Result out; out.tag = q.tag;
            dsp::OscTables ot; for (size_t i = 0; i < q.tables.size(); ++i) ot.t[i] = q.tables[i].get();
            auto render = [&] (const ParamFrame& f) { return renderKick (f, q.note, 44100.0, q.seconds, -1, 1.f, &q.bank, 0.0, -1.0, &ot, nullptr, &q.dist); };
            auto probe = [] (const juce::AudioBuffer<float>& x)
            {
                Probe p; p.r = ka::analyse (x, 44100.0);
                double e = 0.0; for (int c = 0; c < x.getNumChannels(); ++c) for (int i = 0; i < x.getNumSamples(); ++i) e += (double) x.getSample (c, i) * x.getSample (c, i);
                p.rms = (float) std::sqrt (e / juce::jmax (1, x.getNumSamples() * x.getNumChannels()));
                return p;
            };
            auto wave = [] (const juce::AudioBuffer<float>& x)
            {
                Wave w; constexpr int pts = 160; w.lo.assign (pts, 0.f); w.hi.assign (pts, 0.f); const int n = x.getNumSamples();
                for (int k = 0; k < pts; ++k)
                {
                    float lo = 0.f, hi = 0.f;
                    for (int i = k * n / pts; i < (k + 1) * n / pts; ++i) { const float v = 0.5f * (x.getSample (0, i) + x.getSample (1, i)); lo = juce::jmin (lo, v); hi = juce::jmax (hi, v); }
                    w.lo[(size_t) k] = lo; w.hi[(size_t) k] = hi; w.peak = juce::jmax (w.peak, -lo, hi);
                }
                return w;
            };
            const auto bx = render (q.base); const Probe B = probe (bx);
            out.waves.push_back (wave (bx));
            Probe S; if (q.hasStyleRef) S = probe (render (q.styleRef));
            struct Cand { ParamFrame f; Probe p; Wave w; bool ok = false, okRelaxed = false; float score = 0.f; size_t idx = 0; };
            std::vector<Cand> c;
            c.resize (q.cands.size());
            std::atomic<size_t> next { 0 };
            std::mutex rej;
            auto work = [&]
            {
                for (size_t i = next++; i < q.cands.size(); i = next++)
                {
                    if (cancel != nullptr && cancel->load()) return;
                    Cand& x = c[i]; x.f = q.cands[i]; x.idx = i;
                    // level-matched to the BASE (louder is not better): CORE MASTER is a plain gain after every stage, so the render is scaled
                    auto rx = render (x.f);
                    const float rms1 = probe (rx).rms;
                    if (rms1 > 1.0e-6f && B.rms > 1.0e-6f)
                    {
                        const float off = juce::jlimit (-12.f, 6.f, 20.f * std::log10 (B.rms / rms1));
                        const float nm = juce::jlimit (def (P::coreMaster).min, def (P::coreMaster).max, x.f[P::coreMaster] + off);
                        rx.applyGain (juce::Decibels::decibelsToGain (nm - x.f[P::coreMaster]));
                        x.f[P::coreMaster] = nm;
                    }
                    x.p = probe (rx); x.w = wave (rx);
                    juce::StringArray why;
                    auto checks = [&] (float relax)
                    {
                        if (const auto h = hardRule (x.p, B); h.isNotEmpty()) { if (relax == 1.f) why.add (h); return false; }
                        for (int g = 0; g < NumGenes; ++g)
                        {
                            const int m = q.dna[(size_t) g];
                            const float met = partMetric (g, x.p, B);
                            if (m == Keep && (std::abs (met) > keepTolerance[g] * relax || ! keepExtra (g, x.p, B, relax))) { if (relax == 1.f) why.add ("KEEP " + geneNames()[g]); return false; }
                            const int sg = i < q.signs.size() ? q.signs[i][(size_t) g] : 0;
                            if (! q.fresh && m == Evolve && sg != 0 && met * (float) sg < dirThreshold[g] / relax) { if (relax == 1.f) why.add (dirName (g, sg > 0).toLowerCase() + " not reached"); return false; }
                        }
                        return true;
                    };
                    x.ok = checks (1.f); x.okRelaxed = x.ok || checks (2.f);
                    const float away = distance (q.base, x.f);
                    x.score = std::abs (away - 60.f * q.variation) / 20.f + (q.hasStyleRef ? 0.3f * heardDistance (x.p, S) : 0.f);
                    if (q.fresh) x.score = q.hasStyleRef ? heardDistance (x.p, S) : 0.f;
                    { std::lock_guard<std::mutex> l (rej); out.rejects.addArray (why); }
                    if (prog != nullptr) ++(*prog);
                }
            };
            {   // (3 renders at a time: each one is a whole private engine)
                std::vector<std::thread> th; for (int t = 0; t < 2; ++t) th.emplace_back (work);
                work();
                for (auto& t : th) t.join();
            }
            if (cancel != nullptr && cancel->load()) return out;
            out.tried = (int) c.size();
            std::vector<Cand*> pool; for (auto& x : c) if (x.ok) pool.push_back (&x);
            out.passed = (int) pool.size();
            if (pool.size() < 7) { out.relaxed = true; pool.clear(); for (auto& x : c) if (x.okRelaxed) pool.push_back (&x); }
            if (pool.size() < 7) { pool.clear(); for (auto& x : c) pool.push_back (&x); }     // (never an empty page: the best we have)
            std::sort (pool.begin(), pool.end(), [] (Cand* a, Cand* b) { return a->score < b->score; });
            pool.resize (juce::jmax (std::min<size_t> (7, pool.size()), (pool.size() + 1) / 2));
            // 7 that differ from each other: farthest-point, starting with the best score
            std::vector<Cand*> pick;
            if (! pool.empty()) pick.push_back (pool.front());
            while (pick.size() < 7 && pick.size() < pool.size())
            {
                Cand* best = nullptr; float bestD = -1.f;
                for (auto* x : pool)
                {
                    if (std::find (pick.begin(), pick.end(), x) != pick.end()) continue;
                    float dmin = 1.0e9f; for (auto* y : pick) dmin = juce::jmin (dmin, heardDistance (x->p, y->p));
                    dmin -= 0.15f * x->score;
                    if (dmin > bestD) { bestD = dmin; best = x; }
                }
                if (best == nullptr) break;
                pick.push_back (best);
            }
            for (auto* x : pick) { out.picked.push_back (x->f); out.labels.add (labelFor (x->p, B, q.dna)); out.waves.push_back (x->w); }
            return out;
        }

    private:
        void run() override
        {
            while (! threadShouldExit())
            {
                wake.wait (250);
                std::unique_ptr<Request> q;
                { std::lock_guard<std::mutex> l (lock); q = std::move (pending); if (q) working = true; }
                if (! q) continue;
                progressDone = 0; progressOf = (int) q->cands.size();
                cancelNow = false;
                auto r = std::make_unique<Result> (process (*q, &cancelNow, &progressDone));
                {
                    std::lock_guard<std::mutex> l (lock);
                    working = false;
                    if (pending != nullptr) continue;          // (a newer request came: drop this one)
                    done = std::move (r);
                }
                triggerAsyncUpdate();
            }
        }
        void handleAsyncUpdate() override { if (onDone) onDone(); }
        mutable std::mutex lock;
        std::unique_ptr<Request> pending;
        std::unique_ptr<Result> done;
        bool working = false;
        std::atomic<bool> cancelNow { false };
        std::atomic<int> progressDone { 0 }, progressOf { 1 };
        juce::WaitableEvent wake;
    };

    // ---- the requests (message thread) and taking the result into the workspace
    // ANY: how much a part is outside the STYLE (-1 = far below its usual values, +1 = far above), along its direction axis
    inline float outsideStyle (const ParamFrame& f, int gene, int style)
    {
        float s = 0.f; int n = 0;
        for (auto& [id, w] : dirIds (gene))
        {
            const int k = fge::modstate::indexOf (id);
            const Range rg = rangeFor (style, id);
            if (k < 0 || ! rg.has || ! rg.choices.empty() || def (k).kind != Kind::Float) continue;
            const float lo = norm (k, rg.lo), hi = norm (k, rg.hi), c = 0.5f * (lo + hi), half = 0.5f * std::abs (hi - lo) + 0.05f;
            s += juce::jlimit (-1.f, 1.f, (norm (k, f[k]) - c) / half) * (w > 0.f ? 1.f : -1.f); ++n;
        }
        return n ? juce::jlimit (-1.f, 1.f, s / (float) n) : 0.f;
    }
    constexpr int candidates = 24;
    inline std::unique_ptr<VariationWorker::Request> makeRequest (GeProcessor& p, bool fresh)
    {
        auto& w = p.kvWork;
        const auto cur = p.frame();
        if (! w.hasBase || w.selected == -1) { w.base = cur; w.hasBase = true; }      // on the base: your latest edits ARE the base
        auto s = load (p); s.seed = s.seed % 99991 + 1; save (p, s);
        const int style = styleOf (p);
        auto q = std::make_unique<VariationWorker::Request>();
        q->tag = ++w.generation; q->base = w.base; q->variation = s.variation; q->fresh = fresh;
        q->dna = s.mode;
        if (fresh) { q->dna.fill (Free); q->dna[GLength] = Keep; }
        q->hasStyleRef = true; q->styleRef = fge::style::onFrame (w.base, style);
        juce::Random r ((juce::int64) s.seed * 104729 + 7);
        // ANY: deal the directions so the set covers both ways (about 3 -, 3 +, 1 none per 7), pulled back toward the STYLE
        std::array<std::vector<int>, NumGenes> deck;
        for (int i = 0; i < candidates; ++i)
        {
            Settings si = s;
            Dirs sg {};
            for (int g = 0; g < NumGenes; ++g)
            {
                if (fresh || s.mode[(size_t) g] != Evolve) continue;
                if (s.dir[(size_t) g] != 0) { sg[(size_t) g] = s.dir[(size_t) g]; continue; }
                if (deck[(size_t) g].empty())
                {
                    const float pUp = 0.5f - 0.35f * outsideStyle (w.base, g, style);    // (above the style -> more versions go down)
                    const int ups = juce::jlimit (0, 6, juce::roundToInt (6.f * pUp));
                    for (int k = 0; k < 6; ++k) deck[(size_t) g].push_back (k < ups ? 1 : -1);
                    deck[(size_t) g].push_back (0);
                    for (int k = 6; k > 0; --k) std::swap (deck[(size_t) g][(size_t) k], deck[(size_t) g][(size_t) r.nextInt (k + 1)]);
                }
                sg[(size_t) g] = deck[(size_t) g].back(); deck[(size_t) g].pop_back();
            }
            si.dir = sg;
            q->cands.push_back (fresh ? freshOne (cur, style, (uint32_t) s.seed * 131u + (uint32_t) i) : mutateOne (w.base, si, style, (uint32_t) s.seed * 31u + (uint32_t) i));
            q->signs.push_back (sg);
        }
        q->bank = p.shapes(); q->tables = p.currentTables(); q->dist = p.distData(); q->note = p.auditionNote;
        q->seconds = juce::jlimit (0.25, 1.5, (fge::kickLengthMs (w.base, 150.0) + 60.0) / 1000.0);
        w.fresh = fresh; w.selected = -1; w.morphB = -1; w.morph = 0.f;
        return q;
    }
    inline bool accept (GeProcessor& p, const VariationWorker::Result& r)
    {
        auto& w = p.kvWork;
        if (r.tag != w.generation) return false;                                      // (an older job)
        w.versions = r.picked; w.labels = r.labels; w.away.clear();
        for (auto& v : w.versions) w.away.push_back (distance (v, w.base));
        w.waveLo.clear(); w.waveHi.clear();
        for (auto& x : r.waves) { w.waveLo.push_back (x.lo); w.waveHi.push_back (x.hi); }
        w.tried = r.tried; w.passed = r.passed; w.relaxed = r.relaxed;
        return true;
    }
}
