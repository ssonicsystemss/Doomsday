#include "Randomizer.h"
#include "../PluginProcessor.h"

using namespace juce;

namespace re4ge
{
    namespace
    {
        // a continuous knob: its musical range per style (CLASSIC / MODERN / RAW / EXPERIMENTAL); log = spread in octaves
        struct Knob { const char* id; int group; std::array<std::pair<float, float>, 4> range; bool log = false; };
        // a choice / switch: the allowed values per style
        struct Pick { const char* id; int group; std::array<std::vector<int>, 4> options; };
        using R = Randomizer;
        constexpr std::pair<float, float> same (float a, float b) { return { a, b }; }

        const std::vector<Knob>& knobs()
        {
            static const std::vector<Knob> k {
                // OSC 1 + layers + unison / width
                { "osc_blend", R::GOsc, { same (0.f, 0.5f), same (0.f, 0.8f), same (0.f, 0.6f), same (0.f, 1.f) } },
                { "osc_pw", R::GOsc, { same (0.3f, 0.7f), same (0.2f, 0.8f), same (0.2f, 0.6f), same (0.05f, 0.95f) } },
                { "osc_sine", R::GOsc, { same (-18.f, -3.f), same (-24.f, -6.f), same (-24.f, -9.f), same (-40.f, 0.f) } },
                { "osc_warp_amt", R::GOsc, { same (0.1f, 0.5f), same (0.2f, 0.7f), same (0.2f, 0.6f), same (0.1f, 0.9f) } },
                { "osc_warp_env", R::GOsc, { same (0.2f, 0.8f), same (0.f, 1.f), same (0.f, 0.6f), same (0.f, 1.f) } },
                { "tail_start", R::GOsc, { same (3.f, 5.5f), same (3.f, 6.f), same (4.f, 7.f), same (2.f, 9.f) }, true },
                { "tail_drop", R::GOsc, { same (30.f, 150.f), same (25.f, 120.f), same (25.f, 90.f), same (15.f, 300.f) }, true },
                { "tail_curve", R::GOsc, { same (2.f, 5.f), same (2.f, 5.f), same (2.5f, 6.f), same (1.f, 8.f) } },
                { "tail_drive", R::GOsc, { same (18.f, 34.f), same (22.f, 40.f), same (30.f, 46.f), same (10.f, 48.f) } },
                { "tail_tone", R::GOsc, { same (-0.2f, 0.6f), same (0.f, 0.8f), same (0.2f, 0.95f), same (-1.f, 1.f) } },
                { "tail_scream", R::GOsc, { same (0.f, 0.4f), same (0.1f, 0.6f), same (0.3f, 0.8f), same (0.f, 1.f) } },
                { "tail_scream_hz", R::GOsc, { same (500.f, 1500.f), same (600.f, 1800.f), same (500.f, 1600.f), same (200.f, 4000.f) }, true },
                { "uni_voices", R::GOsc, { same (3.f, 5.f), same (4.f, 7.f), same (3.f, 6.f), same (1.f, 7.f) } },
                { "uni_detune", R::GOsc, { same (8.f, 22.f), same (12.f, 35.f), same (10.f, 30.f), same (0.f, 60.f) } },
                { "st_width", R::GOsc, { same (0.15f, 0.4f), same (0.25f, 0.5f), same (0.15f, 0.45f), same (0.f, 0.8f) } },
                { "st_smear", R::GOsc, { same (0.f, 0.4f), same (0.2f, 0.6f), same (0.f, 0.5f), same (0.f, 1.f) } },
                // PITCH
                { "pitch_amt", R::GPitch, { same (3.f, 12.f), same (5.f, 12.f), same (5.f, 24.f), same (-24.f, 24.f) } },
                { "pitch_time", R::GPitch, { same (30.f, 120.f), same (20.f, 100.f), same (10.f, 80.f), same (5.f, 300.f) }, true },
                // ENV (the owner: the bass starts AFTER the kick's tail - never early)
                { "env_delay_pct", R::GEnv, { same (24.f, 38.f), same (22.f, 36.f), same (25.f, 40.f), same (20.f, 45.f) } },
                { "env_swell", R::GEnv, { same (80.f, 220.f), same (60.f, 180.f), same (40.f, 140.f), same (20.f, 300.f) }, true },
                { "env_curve", R::GEnv, { same (-0.8f, 0.2f), same (-0.6f, 0.6f), same (-0.4f, 0.8f), same (-1.f, 1.f) } },
                { "env_hold_pct", R::GEnv, { same (0.f, 30.f), same (0.f, 60.f), same (20.f, 100.f), same (0.f, 100.f) } },
                { "env_decay_pct", R::GEnv, { same (10.f, 60.f), same (0.f, 50.f), same (0.f, 30.f), same (0.f, 90.f) } },
                { "env_sustain", R::GEnv, { same (0.f, 0.6f), same (0.3f, 1.f), same (0.6f, 1.f), same (0.f, 1.f) } },
                { "env_decay_curve", R::GEnv, { same (1.f, 3.f), same (0.8f, 2.5f), same (1.f, 4.f), same (0.3f, 6.f) } },
                { "env_gate_pct", R::GEnv, { same (88.f, 98.f), same (85.f, 98.f), same (88.f, 98.f), same (80.f, 99.f) } },
                { "env_fade", R::GEnv, { same (12.f, 40.f), same (8.f, 30.f), same (6.f, 25.f), same (3.f, 80.f) }, true },
                // DIST
                { "dist_drive", R::GDist, { same (6.f, 16.f), same (8.f, 20.f), same (14.f, 30.f), same (4.f, 36.f) } },
                { "dist_env", R::GDist, { same (0.2f, 0.8f), same (0.f, 0.7f), same (0.f, 0.5f), same (0.f, 1.f) } },
                { "dist_bias", R::GDist, { same (0.f, 0.2f), same (0.f, 0.3f), same (0.1f, 0.5f), same (-0.6f, 0.6f) } },
                { "dist_mix", R::GDist, { same (0.7f, 1.f), same (0.8f, 1.f), same (0.9f, 1.f), same (0.5f, 1.f) } },
                { "dist2_drive", R::GDist, { same (0.f, 6.f), same (4.f, 14.f), same (8.f, 20.f), same (0.f, 24.f) } },
                { "dist2_mix", R::GDist, { same (0.5f, 1.f), same (0.6f, 1.f), same (0.8f, 1.f), same (0.3f, 1.f) } },
                { "dist1_hard", R::GDist, { same (0.f, 0.5f), same (0.2f, 0.7f), same (0.4f, 1.f), same (0.f, 1.f) } },
                { "dist2_hard", R::GDist, { same (0.f, 0.5f), same (0.2f, 0.7f), same (0.4f, 1.f), same (0.f, 1.f) } },
                { "dist1_tone", R::GDist, { same (-0.3f, 0.3f), same (-0.2f, 0.5f), same (0.f, 0.6f), same (-1.f, 1.f) } },
                // FILTER / EQ (the low end always stays: LOW CUT is never touched)
                { "flt_pre_hz", R::GFilter, { same (800.f, 4000.f), same (1500.f, 8000.f), same (2000.f, 10000.f), same (200.f, 14000.f) }, true },
                { "flt_pre_reso", R::GFilter, { same (0.f, 0.4f), same (0.1f, 0.6f), same (0.2f, 0.7f), same (0.f, 0.9f) } },
                { "flt_pre_env", R::GFilter, { same (0.5f, 2.5f), same (0.f, 2.f), same (0.f, 1.5f), same (-3.f, 4.f) } },
                { "eq_high_cut", R::GFilter, { same (2000.f, 9000.f), same (5000.f, 16000.f), same (4000.f, 14000.f), same (1500.f, 16000.f) }, true },
                { "eq_bell_hz", R::GFilter, { same (1200.f, 2400.f), same (1500.f, 2600.f), same (1000.f, 2500.f), same (300.f, 5000.f) }, true },
                { "eq_bell_gain", R::GFilter, { same (0.f, 6.f), same (2.f, 8.f), same (2.f, 9.f), same (-8.f, 10.f) } },
                { "eq_bell_q", R::GFilter, { same (0.8f, 1.6f), same (0.8f, 2.f), same (1.f, 2.5f), same (0.4f, 4.f) } },
                // SHAPE lanes' depth
                { "shp_pitch_range", R::GShape, { same (3.f, 7.f), same (3.f, 12.f), same (5.f, 12.f), same (2.f, 24.f) } },
                { "shp_harm_depth", R::GShape, { same (0.3f, 0.7f), same (0.3f, 0.9f), same (0.2f, 0.6f), same (0.f, 1.f) } },
                { "shp_phase_depth", R::GShape, { same (0.3f, 0.8f), same (0.3f, 1.f), same (0.2f, 0.8f), same (0.f, 1.f) } } };
            return k;
        }
        const std::vector<Pick>& picks()
        {
            static const std::vector<Pick> p {
                { "src_type", R::GOsc, { std::vector<int> { 0, 0, 1 }, { 0 }, { 1, 0 }, { 0, 1 } } },
                { "osc_warp", R::GOsc, { std::vector<int> { 0, 1 }, { 0, 1, 2 }, { 0, 3 }, { 1, 2, 3, 4 } } },
                { "tail_dist", R::GOsc, { std::vector<int> { 0, 1, 4 }, { 1, 2, 4 }, { 2, 3, 4 }, { 0, 1, 2, 3, 4, 5 } } },
                { "pitch_mode", R::GPitch, { std::vector<int> { 0, 2, 2 }, { 0, 2, 3 }, { 2, 3 }, { 0, 1, 2, 3 } } },
                { "dist_type", R::GDist, { std::vector<int> { 0, 1 }, { 1, 2 }, { 2, 4 }, { 0, 1, 2, 3, 4, 5 } } },
                { "dist2_type", R::GDist, { std::vector<int> { 0, 1 }, { 1, 4 }, { 2, 4 }, { 0, 1, 2, 3, 4, 5 } } },
                { "dist1_lab", R::GDist, { std::vector<int> { 0 }, { 0, 1, 2 }, { 0, 2, 5 }, { 1, 2, 3, 4, 5 } } },
                { "dist2_lab", R::GDist, { std::vector<int> { 0 }, { 0, 1, 3 }, { 0, 3, 5 }, { 1, 2, 3, 4, 5 } } },
                { "flt_pre_type", R::GFilter, { std::vector<int> { 1, 1, 0 }, { 1, 1, 0 }, { 0, 1, 1 }, { 0, 1, 1, 3, 4 } } },   // no HP: the low end stays
                { "shp_pitch_on", R::GShape, { std::vector<int> { 0, 0, 1 }, { 0, 1 }, { 0, 1 }, { 0, 1 } } },
                { "shp_harm_on", R::GShape, { std::vector<int> { 0, 1 }, { 0, 1 }, { 0, 0, 1 }, { 0, 1 } } },
                { "shp_phase_on", R::GShape, { std::vector<int> { 0 }, { 0, 1 }, { 0 }, { 0, 1 } } } };
            return p;
        }
        bool groupMoves (int mode, int g)
        {
            switch (mode)
            {
                case R::Full:    return true;
                case R::Osc:     return g == R::GOsc;
                case R::Pitch:   return g == R::GPitch;
                case R::Dist:    return g == R::GDist;
                case R::Filter:  return g == R::GFilter;
                case R::Mod:     return g == R::GMod;
                default:         return g == R::GEnv || g == R::GPitch || g == R::GShape;     // REVERSE
            }
        }
    }

    Randomizer::Settings Randomizer::read (const ValueTree& s)
    {
        Settings r;
        r.mode = jlimit (0, NumModes - 1, (int) s.getProperty ("rndMode", 0));
        r.style = jlimit (0, NumStyles - 1, (int) s.getProperty ("rndStyle", 0));
        r.amount = jlimit (0.f, 1.f, (float) (double) s.getProperty ("rndAmount", 1.0));
        const int bits = (int) s.getProperty ("rndLocks", 0);
        for (int g = 0; g < NumGroups; ++g) r.lock[(size_t) g] = ((bits >> g) & 1) != 0;
        return r;
    }
    void Randomizer::write (ValueTree& s, const Settings& r)
    {
        int bits = 0; for (int g = 0; g < NumGroups; ++g) if (r.lock[(size_t) g]) bits |= 1 << g;
        s.setProperty ("rndMode", r.mode, nullptr); s.setProperty ("rndStyle", r.style, nullptr);
        s.setProperty ("rndAmount", r.amount, nullptr); s.setProperty ("rndLocks", bits, nullptr);
    }

    float Randomizer::getParam (const char* id) const { auto* a = proc.apvts.getParameter (id); return a != nullptr ? a->convertFrom0to1 (a->getValue()) : 0.f; }
    void Randomizer::setParam (const char* id, float v)
    {
        if (auto* a = proc.apvts.getParameter (id)) setNorm (*a, a->convertTo0to1 (v));
    }
    // the host hears the change (gesture), and the state tree gets it NOW with the undo manager - so the whole click is one undo step
    // (APVTS would copy it to the tree later, on its timer, maybe inside another undo transaction)
    void Randomizer::setNorm (juce::RangedAudioParameter& a, float v01)
    {
        v01 = jlimit (0.f, 1.f, v01);
        a.beginChangeGesture(); a.setValueNotifyingHost (v01); a.endChangeGesture();
        auto node = proc.apvts.state.getChildWithProperty ("id", a.paramID);
        if (node.isValid()) node.setProperty ("value", a.convertFrom0to1 (a.getValue()), &proc.undo);
    }

    float Randomizer::randomize (const Settings& st, Random& rng)
    {
        const float A = jlimit (0.f, 1.f, st.amount);
        if (A <= 0.f) return measureDb();
        // the state tree first gets the values as they are now (APVTS copies them on a timer: a preset loaded a moment ago may not be
        // there yet) - so UNDO goes back to exactly the sound before the click
        for (auto* a : proc.getParameters())
            if (auto* r = dynamic_cast<RangedAudioParameter*> (a))
            {
                auto node = proc.apvts.state.getChildWithProperty ("id", r->paramID);
                if (node.isValid()) node.setProperty ("value", r->convertFrom0to1 (r->getValue()), nullptr);
            }
        proc.undo.beginNewTransaction ("Random");
        // a weak / broken roll (too quiet even after matching) is rolled again from the same start (max 6 tries)
        std::vector<float> start; for (auto* a : proc.getParameters()) start.push_back (a->getValue());
        float db = -999.f;
        for (int attempt = 0; attempt < 6; ++attempt)
        {
            if (attempt > 0)
            {
                int i = 0;
                for (auto* a : proc.getParameters()) { if (auto* r = dynamic_cast<RangedAudioParameter*> (a)) if (r->getValue() != start[(size_t) i]) setNorm (*r, start[(size_t) i]); ++i; }
            }
            db = rollOnce (st, rng);
            if (std::isfinite (db) && db > -17.f && db < -11.f) break;
        }
        return db;
    }

    float Randomizer::rollOnce (const Settings& st, Random& rng)
    {
        const float A = jlimit (0.f, 1.f, st.amount);
        const int s = jlimit (0, NumStyles - 1, st.style);
        auto moves = [&] (int g) { return groupMoves (st.mode, g) && ! st.lock[(size_t) g]; };
        // continuous knobs: from where they are towards a random point of the musical range (in the knob's own 0..1)
        for (const auto& k : knobs())
        {
            if (! moves (k.group)) continue;
            auto* a = proc.apvts.getParameter (k.id); if (a == nullptr) continue;
            const auto [lo, hi] = k.range[(size_t) s];
            const float r = rng.nextFloat();
            const float target = k.log && lo > 0.f ? lo * std::pow (hi / lo, r) : lo + r * (hi - lo);
            const float cur01 = a->getValue(), tgt01 = a->convertTo0to1 (target);
            setNorm (*a, cur01 + A * (tgt01 - cur01));
        }
        // choices switch with the probability AMOUNT
        for (const auto& pk : picks())
        {
            if (! moves (pk.group) || rng.nextFloat() > A) continue;
            const auto& opts = pk.options[(size_t) s];
            setParam (pk.id, (float) opts[(size_t) rng.nextInt ((int) opts.size())]);
        }
        // OSC 2-4: a layer comes / goes / changes (CLASSIC: few layers, MODERN / EXPERIMENTAL: more)
        if (moves (GOsc))
        {
            static const float onChance[4] { 0.3f, 0.65f, 0.4f, 0.75f };
            for (int o = 2; o <= 4; ++o)
            {
                if (rng.nextFloat() > A) continue;
                const bool on = rng.nextFloat() < onChance[s];
                setParam (ParamIDs::oscId (o, "level"), on ? -18.f + 15.f * rng.nextFloat() : -60.f);
                if (! on) continue;
                static const int waves[4][4] { { 2, 2, 0, 1 }, { 2, 4, 5, 2 }, { 2, 3, 5, 4 }, { 0, 1, 3, 5 } };
                setParam (ParamIDs::oscId (o, "wave"), (float) waves[s][rng.nextInt (4)]);
                static const int octs[5] { 1, 1, 2, 0, -1 };
                setParam (ParamIDs::oscId (o, "oct"), (float) octs[rng.nextInt (5)]);
                setParam (ParamIDs::oscId (o, "fine"), (rng.nextFloat() * 2.f - 1.f) * (s == Experimental ? 40.f : 12.f));
                setParam (ParamIDs::oscId (o, "pm"), s == Experimental || s == Raw ? rng.nextFloat() * 0.6f : 0.f);
                setParam (ParamIDs::oscId (o, "width"), 0.3f + 0.6f * rng.nextFloat());
            }
        }
        // MOD: one to three rows from an LFO / the envelope to a destination that suits a reverse bass
        if (moves (GMod))
        {
            static const int dsts[] { re::DstCutoff, re::DstDrive, re::DstWarp, re::DstPm, re::DstWidth, re::DstHard1, re::DstBell };
            const int rows = 1 + rng.nextInt (s == Experimental ? 3 : 2);
            for (int r = 0; r < 16; ++r) setParam (ParamIDs::mtxId (r, "on"), 0.f);
            for (int r = 0; r < rows; ++r)
            {
                setParam (ParamIDs::mtxId (r, "on"), 1.f);
                setParam (ParamIDs::mtxId (r, "src"), (float) (rng.nextFloat() < 0.6f ? re::SrcLfo1 + r : re::SrcAmpEnv));
                setParam (ParamIDs::mtxId (r, "dst"), (float) dsts[rng.nextInt ((int) std::size (dsts))]);
                setParam (ParamIDs::mtxId (r, "amt"), (rng.nextBool() ? 1.f : -1.f) * (0.15f + 0.35f * rng.nextFloat()));
                setParam (ParamIDs::lfoId (r, "wave"), (float) rng.nextInt (s == Experimental ? 8 : 5));
                setParam (ParamIDs::lfoId (r, "sync"), 1.f);
                static const int divs[] { 3, 6, 9, 5, 8 };          // 1/16, 1/8, 1/4, 1/16D, 1/8T
                setParam (ParamIDs::lfoId (r, "div"), (float) divs[rng.nextInt ((int) std::size (divs))]);
            }
        }
        // rules: never a broken / silent / wrong-way sound
        if (getParam (ParamIDs::source) > 1.5f && proc.getEngine().currentSample() == nullptr) setParam (ParamIDs::source, 0.f);   // SAMPLE needs a WAV
        bool anyOsc = getParam (ParamIDs::osc1Level) > -40.f;
        for (int o = 2; o <= 4; ++o) anyOsc |= getParam (ParamIDs::oscId (o, "level")) > -40.f;
        if (! anyOsc) setParam (ParamIDs::osc1Level, 0.f);
        setParam (ParamIDs::lowCut, 20.f);                                 // the owner's rule: the low end stays
        if (getParam (ParamIDs::subHz) < 60.f) setParam (ParamIDs::subHz, 80.f);
        if (getParam (ParamIDs::subLevel) < -12.f) setParam (ParamIDs::subLevel, -3.f);
        if (getParam (ParamIDs::delayPct) < 20.f) setParam (ParamIDs::delayPct, 24.f);     // after the kick's tail
        if (getParam (ParamIDs::gatePct) < 80.f) setParam (ParamIDs::gatePct, 90.f);
        // LOUDNESS: two passes on OUTPUT so every random sound sits at about -14 dB RMS
        float db = measureDb();
        for (int pass = 0; pass < 2 && std::isfinite (db) && db > -80.f; ++pass)
        {
            setParam (ParamIDs::level, jlimit (-36.f, 6.f, getParam (ParamIDs::level) + (-14.f - db)));
            db = measureDb();
        }
        return db;
    }

    float Randomizer::measureDb()
    {
        if (offline == nullptr) offline = std::make_unique<re::Engine>();
        auto p = proc.gather();
        p.qualityRt = 0;                                                    // 2x is enough to measure the level
        auto* smp = const_cast<re::Sample*> (proc.getEngine().currentSample());
        offline->setSampleRaw (smp);
        offline->setParams (p);
        offline->setTempo (150.0);
        const double rate = 48000.0; const int block = 512, beat = (int) (rate * 0.4);
        offline->prepare (rate, block);
        AudioBuffer<float> b (2, block);
        double acc = 0; int cnt = 0;
        for (int pos = 0; pos < 2 * beat; pos += block)
        {
            b.clear(); MidiBuffer m;
            for (int k = 0; k < 2; ++k) if (k * beat >= pos && k * beat < pos + block) m.addEvent (MidiMessage::noteOn (1, 31, 1.f), k * beat - pos);
            offline->setParams (p);
            offline->process (b, m);
            if (pos >= beat) for (int i = 0; i < block; ++i) { const double v = 0.5 * (b.getSample (0, i) + b.getSample (1, i)); if (! std::isfinite (v)) return -999.f; acc += v * v; ++cnt; }
        }
        return (float) (10.0 * std::log10 (acc / jmax (1, cnt) + 1e-12));
    }
}
