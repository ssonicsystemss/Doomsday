#pragma once
// RE4GE - KICK-TO-REVERSE analysis (signature feature 2): splits a kick into TRANSIENT / PUNCH / BODY / TAIL and finds its note.
// Offline, on a mono buffer (called off the audio thread when a WAV is loaded). Pure DSP: no files, no GUI.
//   level      RMS in 2 ms frames (dB)
//   brightness the share of energy above ~2 kHz (a first difference = a cheap high-pass), per frame
//   pitch      autocorrelation f0 (30..500 Hz) in 45 ms windows every 5 ms (4x decimated)
//   TRANSIENT  peak .. where the brightness falls under 35 % of its maximum (the click is gone)
//   PUNCH      .. where the pitch is within x1.6 of the tail's pitch (the fast drop is over)
//   BODY       .. where the pitch settles within 4 % of the tail's pitch
//   TAIL       .. where the level falls 45 dB under the peak (the end)
// Every section may be 0 long. CONFIDENCE 0..1 per boundary: how clearly it was found (a flat pitch = an unclear punch, etc.).
// ROOT = the median pitch of the tail as a MIDI note (with the cents off the nearest note).
#include <vector>
#include <cmath>
#include <algorithm>
#include <juce_core/juce_core.h>

namespace re
{
    struct KickSections
    {
        double transientEnd = 0, punchEnd = 0, bodyEnd = 0, tailEnd = 0;   // ms
        float confTransient = 0.f, confPunch = 0.f, confBody = 0.f, confTail = 0.f;
        double rootHz = 0.0; int rootNote = -1; float rootCents = 0.f;
        bool ok = false;
    };

    namespace kickdetail
    {
        // autocorrelation pitch in [from, from + n), 0 = unvoiced
        inline double f0At (const std::vector<float>& x, int from, int n, double rate)
        {
            if (from < 0 || from + n >= (int) x.size()) return 0.0;
            double e0 = 0; for (int i = 0; i < n; ++i) e0 += (double) x[(size_t) (from + i)] * x[(size_t) (from + i)];
            if (e0 < 1.0e-9) return 0.0;
            const int lagMin = (int) (rate / 500.0), lagMax = juce::jmin (n - 2, (int) (rate / 30.0));
            double best = 0; int bestLag = 0;
            std::vector<double> r ((size_t) lagMax + 2, 0.0);
            for (int lag = lagMin; lag <= lagMax; ++lag)
            {
                double acc = 0, el = 0;
                for (int i = 0; i + lag < n; ++i) { acc += (double) x[(size_t) (from + i)] * x[(size_t) (from + i + lag)]; el += (double) x[(size_t) (from + i + lag)] * x[(size_t) (from + i + lag)]; }
                r[(size_t) lag] = acc / std::sqrt (e0 * el + 1e-18);
            }
            for (int lag = lagMin + 1; lag < lagMax; ++lag)
                if (r[(size_t) lag] > best && r[(size_t) lag] >= r[(size_t) lag - 1] && r[(size_t) lag] >= r[(size_t) lag + 1]) { best = r[(size_t) lag]; bestLag = lag; }
            if (bestLag == 0 || best < 0.5) return 0.0;
            // the first strong peak (avoids octave errors on long lags)
            for (int lag = lagMin + 1; lag < bestLag; ++lag)
                if (r[(size_t) lag] > 0.85 * best && r[(size_t) lag] >= r[(size_t) lag - 1] && r[(size_t) lag] >= r[(size_t) lag + 1]) { bestLag = lag; break; }
            // parabolic refinement
            const double a = r[(size_t) bestLag - 1], b = r[(size_t) bestLag], c = r[(size_t) bestLag + 1], den = a - 2 * b + c;
            const double off = std::abs (den) > 1e-12 ? 0.5 * (a - c) / den : 0.0;
            return rate / (bestLag + off);
        }
    }

    inline KickSections analyseKick (const std::vector<float>& x, double rate)
    {
        KickSections k;
        const int N = (int) x.size();
        if (N < (int) (0.05 * rate)) return k;
        const double msPer = 1000.0 / rate;
        // level + brightness per 2 ms frame
        const int fr = juce::jmax (8, (int) (0.002 * rate));
        const int nf = N / fr;
        std::vector<double> lev ((size_t) nf), bri ((size_t) nf);
        for (int f = 0; f < nf; ++f)
        {
            double e = 0, h = 0;
            for (int i = 0; i < fr; ++i)
            {
                const int j = f * fr + i;
                const double v = x[(size_t) j], d = j > 0 ? v - x[(size_t) j - 1] : 0.0;
                e += v * v; h += d * d;
            }
            lev[(size_t) f] = 10.0 * std::log10 (e / fr + 1e-14);
            bri[(size_t) f] = h / (e + 1e-14);
        }
        const int pk = (int) (std::max_element (lev.begin(), lev.end()) - lev.begin());
        const double peakDb = lev[(size_t) pk];
        // TAIL end: the last frame above -45 dB of the peak
        int endF = nf - 1;
        while (endF > pk && lev[(size_t) endF] < peakDb - 45.0) --endF;
        k.tailEnd = (endF + 1) * fr * msPer;
        k.confTail = (float) juce::jlimit (0.0, 1.0, (peakDb - lev[(size_t) (nf - 1)]) / 45.0);
        // TRANSIENT: brightness falls under 35 % of its max (from the first 30 ms)
        {
            int from = 0; double bmax = 0;
            for (int f = 0; f < juce::jmin (nf, (int) (30.0 / (fr * msPer)) + 1); ++f) if (bri[(size_t) f] > bmax) { bmax = bri[(size_t) f]; from = f; }
            int f = from;
            while (f < endF && bri[(size_t) f] > 0.35 * bmax) ++f;
            k.transientEnd = juce::jmin ((double) f * fr * msPer, k.tailEnd);
            const double later = bri[(size_t) juce::jmin (endF, f + 10)];
            k.confTransient = (float) juce::jlimit (0.0, 1.0, 1.0 - later / (bmax + 1e-12));
        }
        // pitch track every 5 ms (20 ms windows) on a 4x decimated copy (the pitch is < 500 Hz; 16x less work)
        const int dec = rate > 30000.0 ? 4 : 2;
        const double rd = rate / dec;
        std::vector<float> xd ((size_t) (N / dec));
        for (size_t i = 0; i < xd.size(); ++i) { float a = 0; for (int j = 0; j < dec; ++j) a += x[i * (size_t) dec + (size_t) j]; xd[i] = a / (float) dec; }
        const double msPerD = 1000.0 / rd;
        const int win = (int) (0.045 * rd), hop = juce::jmax (1, (int) (0.005 * rd));      // 45 ms: two periods of a 45 Hz tail
        std::vector<double> t, f0;
        for (int s = 0; s + win < (int) (k.tailEnd / msPerD); s += hop)
        {
            const double hz = kickdetail::f0At (xd, s, win, rd);
            if (hz > 0) { t.push_back ((s + win * 0.5) * msPerD); f0.push_back (hz); }
        }
        if (f0.size() < 4) { k.punchEnd = k.bodyEnd = k.transientEnd; k.ok = true; return k; }
        // the tail's pitch: median of the last 40 % of the voiced frames
        std::vector<double> lastPart (f0.begin() + (long) (f0.size() * 6 / 10), f0.end());
        std::nth_element (lastPart.begin(), lastPart.begin() + (long) lastPart.size() / 2, lastPart.end());
        const double tailHz = lastPart[lastPart.size() / 2];
        k.rootHz = tailHz;
        const double note = 69.0 + 12.0 * std::log2 (tailHz / 440.0);
        k.rootNote = (int) std::round (note);
        k.rootCents = (float) ((note - k.rootNote) * 100.0);
        // PUNCH ends when the pitch comes within x1.6 of the tail, BODY when it settles within 4 % (and stays there)
        size_t ip = 0;
        while (ip < f0.size() && f0[ip] > tailHz * 1.6) ++ip;
        size_t ib = ip;
        for (size_t i = ip; i < f0.size(); ++i)
        {
            bool stays = true;
            for (size_t j = i; j < juce::jmin (f0.size(), i + 6); ++j) if (std::abs (f0[j] / tailHz - 1.0) > 0.04) { stays = false; break; }
            if (stays) { ib = i; break; }
            ib = i + 1;
        }
        k.punchEnd = juce::jlimit (k.transientEnd, k.tailEnd, ip < f0.size() ? t[ip] : k.tailEnd);
        k.bodyEnd = juce::jlimit (k.punchEnd, k.tailEnd, ib < f0.size() ? t[ib] : k.tailEnd);
        const double startHz = f0.front();
        k.confPunch = (float) juce::jlimit (0.0, 1.0, (startHz / tailHz - 1.0) / 2.0);       // a big drop = a clear punch
        double dev = 0; int nd = 0;
        for (size_t i = ib; i < f0.size(); ++i) { dev += std::abs (f0[i] / tailHz - 1.0); ++nd; }
        k.confBody = (float) juce::jlimit (0.0, 1.0, 1.0 - (nd > 0 ? dev / nd : 1.0) / 0.04);
        k.ok = true;
        return k;
    }
}
