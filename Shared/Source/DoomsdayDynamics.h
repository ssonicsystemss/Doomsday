#pragma once
// =====================================================================
//  DOOMSDAY shared DYNAMICS blocks (header only, from Throne of Titans)
//    dyn::Crossover3      3-band Linkwitz-Riley 24 dB split (low / mid / high sum back flat in level)
//    dyn::BandComp        compressor with downward ratio + upward ratio (OTT-style), soft knee, attack / release
//    dyn::saturate        soft saturation with drive + mix
//    dyn::TruePeak        8x oversampled peak estimate (polyphase Kaiser-sinc interpolator, ITU-R BS.1770 style)
//    dyn::Limiter         lookahead brickwall limiter on true peaks, smooth attack, program release
//    dyn::Loudness        BS.1770 LUFS: momentary (400 ms), short-term (3 s), integrated (gated, histogram)
//    dyn::Tilt            tilt EQ around 1 kHz (+ = brighter)
//  All memory is allocated in prepare(); process functions are real-time safe.
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include "DoomsdayDSP.h"
#include <cmath>
#include <array>
#include <vector>

namespace doomsday::dyn
{
    using doomsday::dsp::Biquad;
    using doomsday::dsp::BiquadCoefs;

    inline float dbToGain (float db) { return std::pow (10.f, db * 0.05f); }
    inline float gainToDb (float g)  { return 20.f * std::log10 (juce::jmax (1.0e-9f, g)); }

    // ------------------------------------------------------------ 3-band crossover (LR4)
    class Crossover3
    {
    public:
        void prepare (double sampleRate) { sr = sampleRate; set (150.f, 3500.f, true); reset(); }
        void reset() { for (auto& ch : st) for (auto& b : ch) b.reset(); }
        void set (float lowHz, float highHz, bool force = false)
        {
            highHz = juce::jmax (highHz, lowHz * 1.5f);
            if (! force && std::abs (lowHz - f1) < 0.01f && std::abs (highHz - f2) < 0.01f) return;
            f1 = lowHz; f2 = highHz;
            const double q = 0.70710678;
            lp1 = BiquadCoefs::make (BiquadCoefs::LowPass, sr, f1, q);  hp1 = BiquadCoefs::make (BiquadCoefs::HighPass, sr, f1, q);
            lp2 = BiquadCoefs::make (BiquadCoefs::LowPass, sr, f2, q);  hp2 = BiquadCoefs::make (BiquadCoefs::HighPass, sr, f2, q);
        }
        // ch 0 / 1
        inline void split (int ch, float x, float& lo, float& mid, float& hi)
        {
            auto& s = st[(size_t) ch];
            lo = s[1].process (s[0].process (x, lp1), lp1);
            const float rest = s[3].process (s[2].process (x, hp1), hp1);
            mid = s[5].process (s[4].process (rest, lp2), lp2);
            hi = s[7].process (s[6].process (rest, hp2), hp2);
            // keep the low band in phase with mid + high: the same LR4 allpass at f2
            const float apL = s[9].process (s[8].process (lo, lp2), lp2), apH = s[11].process (s[10].process (lo, hp2), hp2);
            lo = apL + apH;
        }
        float lowHz() const { return f1; }
        float highHz() const { return f2; }
    private:
        double sr = 44100.0;
        float f1 = 150.f, f2 = 3500.f;
        BiquadCoefs lp1, hp1, lp2, hp2;
        std::array<std::array<Biquad, 12>, 2> st;
    };

    // ------------------------------------------------------------ compressor (down + up)
    struct CompParams
    {
        float threshDb = -18.f, ratio = 3.f, upRatio = 1.f, kneeDb = 6.f, attackMs = 10.f, releaseMs = 120.f, maxUpDb = 12.f;
    };
    class BandComp
    {
    public:
        void prepare (double sampleRate) { sr = sampleRate; envDb = -120.f; grDb = 0.f; }
        void reset() { envDb = -120.f; grDb = 0.f; }
        // static curve: gain change (dB) for an input level (dB)
        static float curve (float x, const CompParams& p)
        {
            const float over = x - p.threshDb, k = juce::jmax (0.01f, p.kneeDb);
            float g = 0.f;
            const float slope = 1.f / juce::jmax (1.f, p.ratio) - 1.f;
            if (over > k * 0.5f) g = slope * over;
            else if (over > -k * 0.5f) g = slope * (over + k * 0.5f) * (over + k * 0.5f) / (2.f * k);
            if (p.upRatio > 1.001f && over < 0.f)
            {
                // upward: quiet parts lifted towards the threshold (faded out for near-silence)
                const float up = juce::jmin (p.maxUpDb, -over * (1.f - 1.f / p.upRatio));
                const float fade = juce::jlimit (0.f, 1.f, (x + 70.f) / 20.f);
                g += up * fade;
            }
            return g;
        }
        // level: linked detector input (max of |L|, |R|); returns the gain (linear)
        inline float process (float level, const CompParams& p)
        {
            const float x = gainToDb (level);
            const float att = std::exp (-1.f / (juce::jmax (0.05f, p.attackMs) * 0.001f * (float) sr));
            const float rel = std::exp (-1.f / (juce::jmax (1.f, p.releaseMs) * 0.001f * (float) sr));
            envDb = x > envDb ? att * envDb + (1.f - att) * x : rel * envDb + (1.f - rel) * x;
            grDb = curve (envDb, p);
            return dbToGain (grDb);
        }
        float gainReductionDb() const { return grDb; }
    private:
        double sr = 44100.0;
        float envDb = -120.f, grDb = 0.f;
    };

    inline float saturate (float x, float amount)
    {
        if (amount <= 0.001f) return x;
        const float k = 1.f + amount * 5.f;
        const float y = std::tanh (x * k) / std::tanh (k) ;
        // keep small signals at about the same level, mix in by amount
        return x + amount * (y - x);
    }

    // ------------------------------------------------------------ true peak (4x polyphase interpolation)
    class TruePeak
    {
    public:
        TruePeak()
        {
            // phases x 48 taps Kaiser-windowed sinc (beta 7)
            auto bessel0 = [] (double x) { double s = 1.0, t = 1.0; for (int k = 1; k < 30; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; } return s; };
            const double beta = 7.0, half = taps / 2.0 + 0.5;
            for (int ph = 0; ph < phases; ++ph)
                for (int t = 0; t < taps; ++t)
                {
                    const double x = (double) (t - taps / 2) + (double) ph / (double) phases;
                    const double s = std::abs (x) < 1e-9 ? 1.0 : std::sin (juce::MathConstants<double>::pi * x) / (juce::MathConstants<double>::pi * x);
                    const double r = juce::jlimit (-1.0, 1.0, x / half);
                    const double w = bessel0 (beta * std::sqrt (1.0 - r * r)) / bessel0 (beta);
                    coef[(size_t) ph][(size_t) t] = (float) (s * w);
                }
        }
        void reset() { hist.fill (0.f); pos = 0; }
        // push a sample, returns the highest |value| between the previous sample and this one.
        // skipBelow: when a quick upper bound (+3 dB over the neighbouring samples) stays under it,
        // the bound is returned without the full interpolation (callers only care above that level)
        inline float push (float x, float skipBelow = 0.f)
        {
            hist[(size_t) pos] = x; pos = (pos + 1) % taps;
            if (skipBelow > 0.f)
            {
                float q = 0.f;
                for (int t = taps / 2 - 3; t <= taps / 2 + 2; ++t) q = juce::jmax (q, std::abs (hist[(size_t) ((pos + t) % taps)]));
                q *= 1.42f;
                if (q < skipBelow) return q;
            }
            float pk = 0.f;
            for (int ph = 0; ph < phases; ++ph)
            {
                float acc = 0.f;
                for (int t = 0; t < taps; ++t) acc += coef[(size_t) ph][(size_t) t] * hist[(size_t) ((pos + t) % taps)];
                pk = juce::jmax (pk, std::abs (acc));
            }
            return pk;   // note: delayed by taps / 2 samples
        }
        static constexpr int taps = 48, phases = 8;
    private:
        std::array<std::array<float, taps>, phases> coef {};
        std::array<float, taps> hist {};
        int pos = 0;
    };

    // ------------------------------------------------------------ lookahead limiter
    class Limiter
    {
    public:
        void prepare (double sampleRate, float lookaheadMs = 1.5f)
        {
            sr = sampleRate;
            look = juce::jmax (8, (int) std::round (lookaheadMs * 0.001 * sr));
            const int tp = TruePeak::taps / 2;
            latency = look + tp + 1;     // +1: a peak between two samples gets the full reduction on both of them
            for (auto& d : delay) d.assign ((size_t) (latency + 4), 0.f);
            gainBuf.assign ((size_t) (look + 1), 1.f);
            qIdx.assign ((size_t) (look + 4), 0); qVal.assign ((size_t) (look + 4), 1.f);
            reset();
        }
        void reset()
        {
            for (auto& d : delay) std::fill (d.begin(), d.end(), 0.f);
            std::fill (gainBuf.begin(), gainBuf.end(), 1.f);
            dpos = 0; gpos = 0; boxSum = (double) (look + 1); rel = 1.f; n = 0;
            qHead = qTail = 0;
            tpL.reset(); tpR.reset();
            grDb = 0.f;
        }
        int latencySamples() const { return latency; }
        // ceilingDb in dBTP; releaseMs program release
        inline void process (float& l, float& r, float ceilingDb, float releaseMs)
        {
            const float ceil = dbToGain (ceilingDb);
            const float aim = ceil * 0.994f;     // -0.05 dB margin for what the 8x estimate can still miss
            // true peak of the incoming sample (delayed by taps/2 inside TruePeak -> matched by the delay line)
            const float pk = juce::jmax (tpL.push (l, aim), tpR.push (r, aim), 1.0e-9f);
            const float need = juce::jmin (1.f, aim / pk);
            // sliding minimum over the lookahead window (monotonic deque of (index, value))
            ++n;
            const int cap = (int) qIdx.size();
            while (qHead != qTail && qVal[(size_t) ((qTail - 1 + cap) % cap)] >= need) qTail = (qTail - 1 + cap) % cap;
            qIdx[(size_t) qTail] = n; qVal[(size_t) qTail] = need; qTail = (qTail + 1) % cap;
            while (qIdx[(size_t) qHead] <= n - (look + 1)) qHead = (qHead + 1) % cap;
            const float m = qVal[(size_t) qHead];
            // boxcar average over the window -> smooth attack that reaches the minimum exactly at the peak
            boxSum += (double) m - (double) gainBuf[(size_t) gpos];
            gainBuf[(size_t) gpos] = m; gpos = (gpos + 1) % (look + 1);
            const float box = (float) (boxSum / (double) (look + 1));
            // release
            const float rc = std::exp (-1.f / (juce::jmax (5.f, releaseMs) * 0.001f * (float) sr));
            rel = box < rel ? box : box + (rel - box) * rc;
            const float g = juce::jmin (rel, box);
            // delayed audio
            auto& dl = delay[0]; auto& dr = delay[1];
            const int sz = (int) dl.size();
            dl[(size_t) dpos] = l; dr[(size_t) dpos] = r;
            int rp = dpos - latency; if (rp < 0) rp += sz;
            dpos = (dpos + 1) % sz;
            l = dl[(size_t) rp] * g; r = dr[(size_t) rp] * g;
            // last safety: never above the ceiling (sample peak)
            l = juce::jlimit (-ceil, ceil, l); r = juce::jlimit (-ceil, ceil, r);
            grDb = gainToDb (g);
        }
        float gainReductionDb() const { return grDb; }
    private:
        double sr = 44100.0;
        int look = 64, latency = 70, dpos = 0, gpos = 0;
        long long n = 0;
        std::array<std::vector<float>, 2> delay;
        std::vector<float> gainBuf;
        std::vector<long long> qIdx;                         // monotonic queue (ring) for the sliding minimum
        std::vector<float> qVal;
        int qHead = 0, qTail = 0;
        double boxSum = 0.0;
        float rel = 1.f, grDb = 0.f;
        TruePeak tpL, tpR;
    };

    // ------------------------------------------------------------ BS.1770 loudness
    class Loudness
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            // K-weighting (pre-filter shelf + RLB high-pass), coefficients for any rate (as in libebur128)
            {
                const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
                const double K = std::tan (juce::MathConstants<double>::pi * f0 / sr);
                const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
                const double a0 = 1.0 + K / Q + K * K;
                pre.b0 = (Vh + Vb * K / Q + K * K) / a0; pre.b1 = 2.0 * (K * K - Vh) / a0; pre.b2 = (Vh - Vb * K / Q + K * K) / a0;
                pre.a1 = 2.0 * (K * K - 1.0) / a0; pre.a2 = (1.0 - K / Q + K * K) / a0;
            }
            {
                const double f0 = 38.13547087602444, Q = 0.5003270373238773;
                const double K = std::tan (juce::MathConstants<double>::pi * f0 / sr);
                const double a0 = 1.0 + K / Q + K * K;
                rlb.b0 = 1.0; rlb.b1 = -2.0; rlb.b2 = 1.0;
                rlb.a1 = 2.0 * (K * K - 1.0) / a0; rlb.a2 = (1.0 - K / Q + K * K) / a0;
            }
            blockLen = juce::jmax (1, (int) std::round (sr * 0.1));
            reset();
        }
        void reset()
        {
            for (auto& f : f1) f.reset();
            for (auto& f : f2) f.reset();
            blocks.fill (0.0); bpos = 0; filled = 0; acc = 0.0; cnt = 0;
            hist.fill (0); histN = 0;
            momentary = shortTerm = integrated = -70.f;
        }
        inline void push (float l, float r)
        {
            const float kl = f2[0].process (f1[0].process (l, pre), rlb);
            const float kr = f2[1].process (f1[1].process (r, pre), rlb);
            acc += (double) kl * kl + (double) kr * kr;
            if (++cnt >= blockLen) finishBlock();
        }
        float momentaryLufs() const { return momentary; }
        float shortTermLufs() const { return shortTerm; }
        float integratedLufs() const { return integrated; }
        void resetIntegrated() { hist.fill (0); histN = 0; integrated = -70.f; }
    private:
        static float toLufs (double ms) { return (float) (-0.691 + 10.0 * std::log10 (ms + 1e-12)); }
        void finishBlock()
        {
            blocks[(size_t) bpos] = acc / (double) cnt; bpos = (bpos + 1) % (int) blocks.size(); filled = juce::jmin ((int) blocks.size(), filled + 1);
            acc = 0.0; cnt = 0;
            auto meanOf = [this] (int nb) { double s = 0; const int k = juce::jmin (nb, filled); for (int i = 1; i <= k; ++i) s += blocks[(size_t) ((bpos - i + (int) blocks.size()) % (int) blocks.size())]; return k > 0 ? s / k : 0.0; };
            const double m = meanOf (4), st = meanOf (30);
            momentary = juce::jmax (-70.f, toLufs (m));
            shortTerm = juce::jmax (-70.f, toLufs (st));
            // integrated: 400 ms gating blocks (75 % overlap) into a 0.1 LU histogram
            if (filled >= 4)
            {
                const float l = toLufs (m);
                if (l > -70.f)
                {
                    const int bin = juce::jlimit (0, (int) hist.size() - 1, (int) std::round ((l + 70.f) * 10.f));
                    ++hist[(size_t) bin]; ++histN;
                    // relative gate: mean of blocks above -70, then -10 LU below it
                    double s = 0; long long c = 0;
                    for (size_t b = 0; b < hist.size(); ++b) if (hist[b]) { s += hist[b] * std::pow (10.0, (((double) b / 10.0 - 70.0) + 0.691) / 10.0); c += hist[b]; }
                    const double gate = toLufs (s / (double) c) - 10.0;
                    s = 0; c = 0;
                    for (size_t b = 0; b < hist.size(); ++b)
                    {
                        const double lb = (double) b / 10.0 - 70.0;
                        if (hist[b] && lb >= gate) { s += hist[b] * std::pow (10.0, (lb + 0.691) / 10.0); c += hist[b]; }
                    }
                    if (c > 0) integrated = toLufs (s / (double) c);
                }
            }
        }
        double sr = 48000.0;
        BiquadCoefs pre, rlb;
        std::array<Biquad, 2> f1, f2;
        std::array<double, 30> blocks {};
        int bpos = 0, filled = 0, blockLen = 4800, cnt = 0;
        double acc = 0.0;
        std::array<long long, 760> hist {};      // -70 .. +6 LUFS in 0.1 LU
        long long histN = 0;
        float momentary = -70.f, shortTerm = -70.f, integrated = -70.f;
    };

    // ------------------------------------------------------------ tilt EQ (low shelf - / high shelf + around 1 kHz)
    class Tilt
    {
    public:
        void prepare (double sampleRate) { sr = sampleRate; set (0.f, true); for (auto& b : st) b.reset(); }
        void set (float db, bool force = false)
        {
            if (! force && std::abs (db - cur) < 0.001f) return;
            cur = db;
            lo = BiquadCoefs::make (BiquadCoefs::LowShelf, sr, 700.0, 0.6, -db * 0.5);
            hi = BiquadCoefs::make (BiquadCoefs::HighShelf, sr, 1400.0, 0.6, db * 0.5);
        }
        inline float process (int ch, float x)
        {
            if (std::abs (cur) < 0.01f) return x;
            return st[(size_t) ch * 2 + 1].process (st[(size_t) ch * 2].process (x, lo), hi);
        }
    private:
        double sr = 44100.0;
        float cur = 0.f;
        BiquadCoefs lo, hi;
        std::array<Biquad, 4> st;
    };
}
