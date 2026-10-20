#pragma once
// =====================================================================
//  MIRROR OF SOULS - 8-band EQ engine
//   per band: LOW CUT / LOW SHELF / BELL / NOTCH / HIGH SHELF / HIGH CUT, 12/24/48 dB cuts,
//   STEREO / MID / SIDE, DYNAMIC (band works only above its threshold)
//   AMOUNT scales the whole curve (0..150 %), MONO BASS, OUTPUT; analyser taps before and after
// =====================================================================
#include "DoomsdayDSP.h"
#include "Params.h"

struct BandParams
{
    bool on = false; int type = Choices::Bell, slope = 1;
    float freq = 1000.f, gainDb = 0.f, q = 0.9f;
    int mode = 0; bool dyn = false; float threshDb = -24.f;
};

struct EqParams
{
    std::array<BandParams, ParamIDs::numBands> band;
    float amount = 1.f, outDb = 0.f, monoBassHz = 0.f;
};

struct BandDesign
{
    int stages = 0;
    std::array<doomsday::dsp::BiquadCoefs, 8> c;
    double magnitude (double hz, double fs) const { double m = 1.0; for (int i = 0; i < stages; ++i) m *= c[(size_t) i].magnitude (hz, fs); return m; }
};

class MirrorEngine
{
public:
    using BQ = doomsday::dsp::BiquadCoefs;
    static constexpr int nb = ParamIDs::numBands;

    // shared by the engine and the display: the filter for one band at a given effective gain
    static BandDesign design (const BandParams& b, double fs, float gainDb, float amount)
    {
        BandDesign d;
        const float amt = juce::jlimit (0.f, 1.5f, amount);
        const float g = gainDb * amt;
        switch (b.type)
        {
            case Choices::LowCut:
            case Choices::HighCut:
            {
                // AMOUNT slides the cut towards the edge of the spectrum (0 % = no cut)
                const float k = juce::jmin (1.f, amt);
                const double hz = b.type == Choices::LowCut ? 10.0 * std::pow ((double) b.freq / 10.0, (double) k)
                                                            : 22000.0 * std::pow ((double) b.freq / 22000.0, (double) k);
                const int stages = b.slope == 0 ? 1 : b.slope == 1 ? 2 : b.slope == 2 ? 4 : 8;
                const auto qs = doomsday::dsp::butterworthQs (stages);
                d.stages = stages;
                for (int i = 0; i < stages; ++i)
                {
                    const double q = stages == 1 ? (double) b.q : qs[(size_t) i];
                    d.c[(size_t) i] = BQ::make (b.type == Choices::LowCut ? BQ::HighPass : BQ::LowPass, fs, hz, q);
                }
                break;
            }
            case Choices::LowShelf:  d.stages = 1; d.c[0] = BQ::make (BQ::LowShelf, fs, b.freq, juce::jlimit (0.3, 2.0, (double) b.q), g); break;
            case Choices::HighShelf: d.stages = 1; d.c[0] = BQ::make (BQ::HighShelf, fs, b.freq, juce::jlimit (0.3, 2.0, (double) b.q), g); break;
            case Choices::Notch:     d.stages = 1; d.c[0] = amt >= 0.999f ? BQ::make (BQ::Notch, fs, b.freq, juce::jmax (2.0, (double) b.q))      // full notch: the frequency is gone
                                                          : BQ::make (BQ::Bell, fs, b.freq, juce::jmax (2.0, (double) b.q), -48.0 * amt); break;
            default:                 d.stages = 1; d.c[0] = BQ::make (BQ::Bell, fs, b.freq, b.q, g); break;
        }
        return d;
    }
    static bool gainType (int t) { return t == Choices::Bell || t == Choices::LowShelf || t == Choices::HighShelf; }

    doomsday::dsp::AnalyzerTap<8192> preTap, postTap;
    std::array<std::atomic<float>, nb> liveGainDb {};   // effective gain incl. dynamics (for the display)

    void prepare (double sampleRate, int)
    {
        sr = sampleRate;
        for (auto& s : st) s = {};
        for (auto& sv : sGain) { sv.reset (sr, 0.04); }
        for (auto& sv : sFreq) { sv.reset (sr, 0.04); }
        for (auto& sv : sQ)    { sv.reset (sr, 0.04); }
        sOut.reset (sr, 0.03);
        first = true;
        monoHp = {};
    }

    void setParams (const EqParams& np) { p = np; }

    void process (float* L, float* R, int n)
    {
        preTap.push (L, R, n);
        if (first)
        {
            for (int b = 0; b < nb; ++b)
            {
                sGain[(size_t) b].setCurrentAndTargetValue (p.band[(size_t) b].gainDb);
                sFreq[(size_t) b].setCurrentAndTargetValue (p.band[(size_t) b].freq);
                sQ[(size_t) b].setCurrentAndTargetValue (p.band[(size_t) b].q);
            }
            sOut.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p.outDb));
            first = false;
        }
        const float att = 1.f - std::exp (-1.f / (0.002f * (float) sr)), rel = 1.f - std::exp (-1.f / (0.1f * (float) sr));
        for (int b = 0; b < nb; ++b)
        {
            const auto& bp = p.band[(size_t) b];
            auto& S = st[(size_t) b];
            if (bp.mode != S.mode || bp.type != S.type || bp.slope != S.slope || bp.on != S.on)
            {
                for (auto& ch : S.f) for (auto& q : ch) q.reset();
                S.mode = bp.mode; S.type = bp.type; S.slope = bp.slope; S.on = bp.on;
            }
            sGain[(size_t) b].setTargetValue (bp.gainDb);
            sFreq[(size_t) b].setTargetValue (bp.freq);
            sQ[(size_t) b].setTargetValue (bp.q);
            if (! bp.on) { liveGainDb[(size_t) b].store (0.f); continue; }

            const bool dynamic = bp.dyn && gainType (bp.type);
            for (int start = 0; start < n; start += 32)
            {
                const int len = juce::jmin (32, n - start);
                // parameters for this 32-sample slice
                BandParams cur = bp;
                cur.freq = sFreq[(size_t) b].getNextValue(); sFreq[(size_t) b].skip (len - 1);
                cur.q = sQ[(size_t) b].getNextValue();       sQ[(size_t) b].skip (len - 1);
                float g = sGain[(size_t) b].getNextValue();  sGain[(size_t) b].skip (len - 1);
                if (dynamic)
                {
                    // detector: band-pass around the band on the signal the band works on
                    const auto det = BQ::make (BQ::BandPass, sr, cur.freq, juce::jmax (0.5f, cur.q), 0.0);
                    for (int i = start; i < start + len; ++i)
                    {
                        const float x = bp.mode == 2 ? 0.5f * (L[i] - R[i]) : 0.5f * (L[i] + R[i]);
                        const float a = std::abs (S.det.process (x, det));
                        S.env += (a > S.env ? att : rel) * (a - S.env);
                    }
                    const float lvl = juce::Decibels::gainToDecibels (S.env * 1.414f, -100.f);
                    g *= juce::jlimit (0.f, 1.f, (lvl - bp.threshDb) / 12.f);
                }
                const auto d = design (cur, sr, g, p.amount);
                liveGainDb[(size_t) b].store (gainType (bp.type) ? g * p.amount : 0.f, std::memory_order_relaxed);
                for (int i = start; i < start + len; ++i)
                {
                    if (bp.mode == 0)
                    {
                        for (int s = 0; s < d.stages; ++s) { L[i] = S.f[0][(size_t) s].process (L[i], d.c[(size_t) s]); R[i] = S.f[1][(size_t) s].process (R[i], d.c[(size_t) s]); }
                    }
                    else
                    {
                        float m = 0.5f * (L[i] + R[i]), sd = 0.5f * (L[i] - R[i]);
                        float& x = bp.mode == 1 ? m : sd;
                        for (int s = 0; s < d.stages; ++s) x = S.f[0][(size_t) s].process (x, d.c[(size_t) s]);
                        L[i] = m + sd; R[i] = m - sd;
                    }
                }
            }
        }

        // MONO BASS: no side signal below the frequency
        if (p.monoBassHz > 20.f)
        {
            // side through a 24 dB high-pass: the lows become mono
            const auto c1 = BQ::make (BQ::HighPass, sr, p.monoBassHz, 0.54119610), c2 = BQ::make (BQ::HighPass, sr, p.monoBassHz, 1.30656296);
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (L[i] + R[i]);
                const float s = monoHp[1].process (monoHp[0].process (0.5f * (L[i] - R[i]), c1), c2);
                L[i] = m + s; R[i] = m - s;
            }
        }
        sOut.setTargetValue (juce::Decibels::decibelsToGain (p.outDb));
        for (int i = 0; i < n; ++i) { const float g = sOut.getNextValue(); L[i] *= g; R[i] *= g; }
        postTap.push (L, R, n);
    }

    double sampleRate() const { return sr; }

private:
    struct BandState
    {
        std::array<std::array<doomsday::dsp::Biquad, 8>, 2> f;
        doomsday::dsp::Biquad det;
        float env = 0.f;
        int mode = -1, type = -1, slope = -1; bool on = false;
    };
    EqParams p;
    double sr = 48000.0;
    bool first = true;
    std::array<BandState, nb> st;
    std::array<juce::SmoothedValue<float>, nb> sGain, sQ;
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>, nb> sFreq;
    juce::SmoothedValue<float> sOut;
    std::array<doomsday::dsp::Biquad, 2> monoHp;
};
