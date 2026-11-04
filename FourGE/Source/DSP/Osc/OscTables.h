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
