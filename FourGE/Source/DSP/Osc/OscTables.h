#pragma once
// =====================================================================
//  4GE module 03 - the oscillator DATA that is not a parameter (saved in the state child OSCS) and the table builder:
//  CUSTOM = a drawn cycle (env::Shape, -1..1, same editor as the envelopes) or an imported cycle,
//  HARMONIC = 64 harmonics (amplitude / phase / on) shaped by the HARMONIC parameters (amount, odd, even, roll-off, tilt,
//  spread, phase, random), WAVETABLE = a built-in table or the USER table (frames imported from a WAV).
//  buildOscTable() is pure (message thread or offline render) - the realtime path and the export use the same tables.
// =====================================================================
#include "Wavetable.h"
#include "OscBank.h"
#include "../Env/MultiEnv.h"
#include "../../Parameters/ParamFrame.h"

namespace fge::dsp
{
    constexpr int numHarmonics = 64;

    struct OscData
    {
        struct One
        {
            env::Shape custom = defaultCustom();                 // drawn CUSTOM cycle
            std::vector<float> importedCycle;                    // CUSTOM from a WAV (overrides the drawing when not empty)
            std::array<float, numHarmonics> hAmp {}, hPhase {};  // HARMONIC profile
            std::array<bool, numHarmonics> hOn {};
            std::vector<std::vector<float>> userFrames;          // USER wavetable
            One() { for (int h = 0; h < numHarmonics; ++h) { hAmp[(size_t) h] = 1.f / (float) (h + 1); hOn[(size_t) h] = true; } }
        };
        std::array<One, maxOsc> osc;

        static env::Shape defaultCustom()                        // a rounded hardstyle-ish cycle: up fast, long fall, dip
        {
            auto s = env::Shape::make ({ { 0.f, 0.f }, { 0.12f, 1.f }, { 0.5f, 0.15f }, { 0.62f, -1.f }, { 1.f, 0.f } }, env::EaseInOut);
            return s;
        }
    };

inline void cookOscBank (const ParamFrame& f, OscBankSettings& ob)
{
        auto db = [] (float v) { return std::pow (10.f, v / 20.f); };
        ob.any = false; ob.tailStereo = false;
        const int stride = P::osc2On - P::osc1On;
        for (int i = 0; i < numOsc; ++i)
        {
            auto& o = ob.o[(size_t) i];
            auto at = [&] (int first) { return first + i * stride; };
            o.on = f.on (at (P::osc1On));
            if (! o.on) continue;
            o.wave = f.idx (at (P::osc1Wave)); o.noise = f.idx (at (P::osc1Noise)); o.track = f.idx (at (P::osc1Track));
            o.fixedHz = f[at (P::osc1Freq)];
            const float ratio = juce::jmax (0.05f, f[at (P::osc1Ratio)] + f[at (P::osc1RatioFine)]);
            o.mul = std::exp2 (std::round (f[at (P::osc1Oct)]) + std::round (f[at (P::osc1Semi)]) / 12.f + f[at (P::osc1Fine)] / 1200.f) * ratio;
            const float lvl = f[at (P::osc1Level)] <= -59.9f ? 0.f : db (f[at (P::osc1Level)]);
            o.gain = lvl * (f.on (at (P::osc1Invert)) ? -1.f : 1.f);
            const float a = (juce::jlimit (-1.f, 1.f, f[at (P::osc1Pan)]) + 1.f) * juce::MathConstants<float>::pi * 0.25f;
            o.panL = std::cos (a) * 1.41421356f; o.panR = std::sin (a) * 1.41421356f;
            o.phase01 = f[at (P::osc1Phase)] / 360.f; o.phaseMode = f.idx (at (P::osc1PhMode)); o.phaseRand = f[at (P::osc1PhRand)];
            o.phaseSrc = f.idx (at (P::osc1PhSrc));
            o.pw = f[at (P::osc1Pw)];
            o.uni = juce::jlimit (1, maxUnison, (int) std::lround (f[at (P::osc1Uni)]));
            o.detuneCt = f[at (P::osc1Detune)] * 100.f; o.spread = f[at (P::osc1Spread)]; o.blend = f[at (P::osc1Blend)];
            o.uniMode = f.idx (at (P::osc1UniMode)); o.uniPhase = f.idx (at (P::osc1UniPh));
            o.seed = (uint32_t) std::lround (f[at (P::osc1Seed)]);
            layoutUnison (o);
            o.fmFb = f[at (P::osc1FmFb)]; o.fmEnv = f[at (P::osc1FmEnv)]; o.decayMs = f[at (P::osc1ModDecay)];
            o.ringSrc = f.idx (at (P::osc1RingSrc)); o.ringAmt = f[at (P::osc1RingAmt)]; o.ringMix = f[at (P::osc1RingMix)];
            o.ringHz = f[at (P::osc1RingHz)]; o.ringEnv = f[at (P::osc1RingEnv)];
            o.syncSrc = f.idx (at (P::osc1SyncSrc)); o.syncMode = f.idx (at (P::osc1SyncMode));
            o.syncRatio = 1.f + 7.f * f[at (P::osc1SyncAmt)];
            const int to[4] { P::osc1ToPunch, P::osc1ToBody, P::osc1ToTail, P::osc1ToSub };
            for (int d = 0; d < 4; ++d) o.route[(size_t) d] = f[at (to[d])] <= -59.9f ? 0.f : db (f[at (to[d])]);
            o.wtMode = f.idx (at (P::osc1WtMode)); o.wtPos = f[at (P::osc1WtPos)]; o.wtMorph = f[at (P::osc1WtMorph)];
            o.wtStart = f[at (P::osc1WtStart)]; o.wtEnd = f[at (P::osc1WtEnd)]; o.wtRateMs = f[at (P::osc1WtRate)];
            const bool routed = o.route[0] + o.route[1] + o.route[2] + o.route[3] > 0.f && lvl > 0.f;
            ob.any = true; juce::ignoreUnused (routed);       // (an unrouted oscillator can still be an FM / ring / sync source)
        }
        // MUTE: silent, still a modulation source; SOLO: only the soloed oscillators are heard
        const int muteIds[numOsc] { P::osc1Mute, P::osc2Mute, P::osc3Mute, P::osc4Mute }, soloIds[numOsc] { P::osc1Solo, P::osc2Solo, P::osc3Solo, P::osc4Solo };
        bool anySolo = false;
        for (int i = 0; i < numOsc; ++i) anySolo |= ob.o[(size_t) i].on && f.on (soloIds[i]);
        for (int i = 0; i < numOsc; ++i)
        {
            auto& o = ob.o[(size_t) i];
            o.audible = o.on && ! f.on (muteIds[i]) && (! anySolo || f.on (soloIds[i]));
            if (o.audible && o.route[2] > 0.f && (std::abs (f[P::osc1Pan + i * (P::osc2On - P::osc1On)]) > 0.001f || (o.uni > 1 && o.spread > 0.001f))) ob.tailStereo = true;
        }
        const int fmIds[numOsc][numOsc] { { -1, P::fm1to2, P::fm1to3, P::fm1to4 }, { P::fm2to1, -1, P::fm2to3, P::fm2to4 },
                                          { P::fm3to1, P::fm3to2, -1, P::fm3to4 }, { P::fm4to1, P::fm4to2, P::fm4to3, -1 } };
        for (int a2 = 0; a2 < numOsc; ++a2)
            for (int b2 = 0; b2 < numOsc; ++b2)
                ob.fm[a2][b2] = fmIds[a2][b2] < 0 || ! ob.o[(size_t) a2].on ? 0.f : f[fmIds[a2][b2]];
    }


    namespace osctables
    {
        inline int idx (int osc, int first) { return first + osc * (P::osc2On - P::osc1On); }

        // the HARMONIC engine: profile x parameters -> amplitudes / phases of 64 harmonics
        inline void harmonicSpectrum (const OscData::One& d, const ParamFrame& f, int osc, float* amp, float* ph)
        {
            const float amount = f[idx (osc, P::osc1HAmount)], odd = f[idx (osc, P::osc1HOdd)], even = f[idx (osc, P::osc1HEven)];
            const float roll = f[idx (osc, P::osc1HRoll)], tilt = f[idx (osc, P::osc1HTilt)], spread = f[idx (osc, P::osc1HSpread)];
            const float hph = f[idx (osc, P::osc1HPhase)], rnd = f[idx (osc, P::osc1HRandom)];
            Rng r; r.seed ((uint32_t) f[idx (osc, P::osc1Seed)] * 977u + 3u);
            float raw[numHarmonics];
            for (int h = 0; h < numHarmonics; ++h)
            {
                const int n = h + 1;
                float a = d.hOn[(size_t) h] ? d.hAmp[(size_t) h] : 0.f;
                if (n > 1) a *= amount * (n % 2 == 1 ? odd : even);
                a *= std::pow ((float) n, -2.f * roll);                     // ROLL-OFF: up to 1 / n^2 extra
                a *= std::pow ((float) n, tilt);                            // TILT: brighter (+) / darker (-)
                const float rv = r.next01(), rp = r.next01();
                a *= 1.f + rnd * (rv * 2.f - 1.f);                           // RANDOM: seeded amplitude variation
                raw[h] = juce::jmax (0.f, a);
                ph[h] = d.hPhase[(size_t) h] + hph * juce::MathConstants<float>::pi * (float) h + rnd * rp * juce::MathConstants<float>::twoPi;
            }
            for (int h = 0; h < numHarmonics; ++h)                          // SPREAD: energy leaks to the neighbouring harmonics
            {
                const float l = h > 0 ? raw[h - 1] : 0.f, rr = h + 1 < numHarmonics ? raw[h + 1] : 0.f;
                amp[h] = raw[h] * (1.f - spread * 0.6f) + (l + rr) * spread * 0.3f;
            }
        }

        inline TablePtr builtinCached (int which)
        {
            static const std::array<TablePtr, tables::NumBuiltin> b = [] { std::array<TablePtr, tables::NumBuiltin> a; for (int i = 0; i < tables::NumBuiltin; ++i) a[(size_t) i] = tables::builtin (i); return a; }();
            return b[(size_t) juce::jlimit (0, tables::NumBuiltin - 1, which)];
        }
        // the table oscillator 'osc' reads now (nullptr = a classic waveform, no table)
        inline TablePtr build (const ParamFrame& f, const OscData& data, int osc)
        {
            const int wave = f.idx (idx (osc, P::osc1Wave));
            const auto& d = data.osc[(size_t) osc];
            if (wave == WCustom)
            {
                if (! d.importedCycle.empty()) return tables::fromCycles ({ d.importedCycle });
                return tables::fromCycles ({ tables::cycleOf ([&] (float x) { return d.custom.eval (x); }) });
            }
            if (wave == WHarmonic)
            {
                float a[numHarmonics], p[numHarmonics];
                harmonicSpectrum (d, f, osc, a, p);
                return tables::fromCycles ({ tables::additive (a, p, numHarmonics) });
            }
            if (wave == WWavetable)
            {
                const int t = f.idx (idx (osc, P::osc1WtTable));
                if (t == tables::NumBuiltin)                                  // USER
                    return d.userFrames.empty() ? tables::fromCycles ({ tables::cycleOf ([&] (float x) { return d.custom.eval (x); }) })   // nothing imported: your drawn cycle
                                               : tables::fromCycles (d.userFrames);
                return builtinCached (t);
            }
            return nullptr;
        }
        // what a table depends on (rebuild only when this changes)
        inline juce::String keyOf (const ParamFrame& f, int osc, uint32_t dataVersion)
        {
            const int wave = f.idx (idx (osc, P::osc1Wave));
            juce::String k (wave);
            if (wave == WWavetable) k << "|" << f.idx (idx (osc, P::osc1WtTable));
            if (wave == WHarmonic)
                for (int id : { P::osc1HAmount, P::osc1HOdd, P::osc1HEven, P::osc1HRoll, P::osc1HTilt, P::osc1HSpread, P::osc1HPhase, P::osc1HRandom, P::osc1Seed })
                    k << "|" << juce::String (f[idx (osc, id)], 4);
            if (wave >= WCustom) k << "|v" << (int) dataVersion;
            return k;
        }
    }
}

namespace fge::dsp
{
    // lock-free hand-over of a small struct to the audio thread (triple buffer, like env::BankExchange)
    template <typename T>
    class Exchange
    {
    public:
        T& back() { return buf[(size_t) backIdx]; }
        void publish() { backIdx = mid.exchange (backIdx | dirty) & ~dirty; }
        const T& front()
        {
            if (mid.load() & dirty) frontIdx = mid.exchange (frontIdx) & ~dirty;
            return buf[(size_t) frontIdx];
        }
    private:
        static constexpr int dirty = 4;
        std::array<T, 3> buf {};
        std::atomic<int> mid { 1 };
        int backIdx = 0, frontIdx = 2;
    };

    // OscData <-> ValueTree text (the state child OSCS: one child per oscillator)
    namespace oscstate
    {
        inline juce::String floatsToText (const float* x, size_t n)
        {
            juce::MemoryBlock mb (x, n * sizeof (float));
            return mb.toBase64Encoding();
        }
        inline std::vector<float> textToFloats (const juce::String& s)
        {
            juce::MemoryBlock mb;
            if (s.isEmpty() || ! mb.fromBase64Encoding (s)) return {};
            std::vector<float> v (mb.getSize() / sizeof (float));
            if (! v.empty()) std::memcpy (v.data(), mb.getData(), v.size() * sizeof (float));
            for (auto& f : v) if (! std::isfinite (f)) f = 0.f;
            return v;
        }
        inline OscData::One fromTree (const juce::ValueTree& t)
        {
            OscData::One o;
            if (! t.isValid()) return o;
            env::Shape s;
            if (env::Shape::fromString (t.getProperty ("custom").toString(), s, -1.f, 1.f)) o.custom = s;
            o.importedCycle = textToFloats (t.getProperty ("cycle").toString());
            if (! o.importedCycle.empty() && (int) o.importedCycle.size() != Table::N) o.importedCycle.clear();
            const auto a = textToFloats (t.getProperty ("hAmp").toString()), p = textToFloats (t.getProperty ("hPhase").toString()),
                       e = textToFloats (t.getProperty ("hOn").toString());
            for (int h = 0; h < numHarmonics; ++h)
            {
                if ((int) a.size() == numHarmonics) o.hAmp[(size_t) h] = juce::jlimit (0.f, 1.f, a[(size_t) h]);
                if ((int) p.size() == numHarmonics) o.hPhase[(size_t) h] = p[(size_t) h];
                if ((int) e.size() == numHarmonics) o.hOn[(size_t) h] = e[(size_t) h] > 0.5f;
            }
            const auto fr = textToFloats (t.getProperty ("frames").toString());
            for (size_t k = 0; k + Table::N <= fr.size() && o.userFrames.size() < 64; k += Table::N)
                o.userFrames.emplace_back (fr.begin() + (long) k, fr.begin() + (long) (k + Table::N));
            return o;
        }
        // the text of one property (so the GUI writes exactly one undoable change)
        inline juce::String harmonicsText (const OscData::One& o, int which)
        {
            float v[numHarmonics];
            for (int h = 0; h < numHarmonics; ++h) v[h] = which == 0 ? o.hAmp[(size_t) h] : which == 1 ? o.hPhase[(size_t) h] : (o.hOn[(size_t) h] ? 1.f : 0.f);
            return floatsToText (v, numHarmonics);
        }
        inline juce::String framesText (const std::vector<std::vector<float>>& frames)
        {
            std::vector<float> all;
            for (auto& f : frames) all.insert (all.end(), f.begin(), f.end());
            return all.empty() ? juce::String() : floatsToText (all.data(), all.size());
        }
    }
}

namespace fge::dsp
{
    // ---- preview for the GUI (runs on the GUI's background thread): the oscillators at a steady pitch, no part chain.
    //      only = -1 the audible sum, 0..3 one oscillator (its raw output); post = false switches FM / ring / sync off.
    inline std::vector<float> renderOscPreview (const ParamFrame& f, const OscTables& tb, int only, bool post, float hz, int n, double rate)
    {
        OscBankSettings s;
        cookOscBank (f, s);
        if (! post)
        {
            for (auto& o : s.o) { o.ringSrc = 0; o.syncSrc = 0; o.fmFb = 0.f; }
            for (auto& row : s.fm) for (auto& v : row) v = 0.f;
        }
        for (auto& o : s.o) { o.route = { 0.f, 0.f, 1.f, 0.f }; o.panL = o.panR = 1.f; }
        auto v = std::make_unique<OscBankVoice>();
        const std::array<float, 4> partHz { hz, hz, hz, hz };
        v->start (s, partHz, 0.0, 0);
        std::vector<float> out ((size_t) n, 0.f);
        const float inv = (float) (1.0 / rate);
        for (int i = 0; i < n; ++i)
        {
            std::array<float, 4> dest {}; float l = 0.f, r = 0.f;
            v->tick (s, tb, partHz, 1000.f * (float) i / (float) rate, inv, dest, l, r);
            if (only < 0) out[(size_t) i] = dest[2];
            else { const auto& o = s.o[(size_t) only]; out[(size_t) i] = o.on ? v->unitOf (only).out * std::abs (o.gain) * (o.gain < 0.f ? -1.f : 1.f) : 0.f; }
        }
        return out;
    }
}
