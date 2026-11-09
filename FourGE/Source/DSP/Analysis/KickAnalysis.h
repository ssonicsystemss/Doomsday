#pragma once
// =====================================================================
//  4GE module 16 - KICK ANALYSIS ENGINE (pure DSP on buffers: no files, no GUI; runs on a background thread)
//   One kick (a 4GE render or an imported reference WAV) -> a REPORT:
//    WAVEFORM (min / max), ENVELOPE (5 ms RMS), PITCH CURVE (normalised autocorrelation every 5 ms: start / end Hz, the note,
//    DROP TIME, a held TOK), SPECTRUM (averaged, log bins) + HARMONICS of the tail note, TRANSIENT (attack, strength, click
//    centroid), LENGTH (-40 dB), BANDS (SUB 20-60 / LOW 60-150 / BODY 150-400 / MID 400-1.5k / PRESENCE 1.5-5k / AIR 5-16k,
//    share of the energy), STEREO & PHASE (correlation, side / mid, low-end correlation, mono loss), PEAK / TRUE PEAK / RMS /
//    CREST / LUFS, DC, CLIPPING, and the detected FEATURES in plain words.
//   Information only - the suggestions (Presets/AnalysisTools.h) change nothing until the user presses APPLY.
// =====================================================================
#include <juce_dsp/juce_dsp.h>
#include "../Meter/MasterMeter.h"
#include <array>
#include <vector>

namespace fge::dsp::analysis
{
    constexpr int numBands = 6;
    inline const juce::StringArray& bandNames() { static const juce::StringArray n { "SUB 20-60", "LOW 60-150", "BODY 150-400", "MID 400-1.5k", "PRESENCE 1.5-5k", "AIR 5-16k" }; return n; }
    constexpr float bandEdges[numBands + 1] { 20.f, 60.f, 150.f, 400.f, 1500.f, 5000.f, 16000.f };
    constexpr int specBins = 96;                                       // 20 Hz .. 20 kHz, log
    inline float specHz (int k) { return 20.f * std::pow (1000.f, (float) k / (float) (specBins - 1)); }

    struct Report
    {
        bool valid = false;
        double rate = 44100.0, seconds = 0.0;
        std::vector<float> wmin, wmax;                                 // 512 bins of the mono sum
        std::vector<float> env;                                        // dB re the peak, every 5 ms
        std::vector<float> pitch, pitchConf;                           // Hz (0 = none) every 5 ms
        float startHz = 0.f, endHz = 0.f, dropMs = 0.f, tokMs = 0.f;   // pitch drop: start, the settled note, how long, a held tok before it
        int endNote = -1;
        std::array<float, specBins> spec {};                           // dB re the loudest bin
        std::array<float, 10> harm {};                                 // harmonics 1..10 of endHz in the tail, dB re the fundamental
        float attackMs = 0.f, transientDb = 0.f, clickHz = 0.f;       // transient
        float lengthMs = 0.f;
        std::array<float, numBands> bandDb {};                         // share of energy per band (dB re the total)
        float subShare = 0.f;                                           // % of the energy under 100 Hz
        float corr = 1.f, sideDb = -120.f, lowCorr = 1.f, monoLossDb = 0.f;
        bool stereo = false;
        meter::Measure m;                                               // peak / true peak / RMS / LUFS / DC / clip
        float crestDb = 0.f;
        int clipped = 0;
        juce::StringArray features;
    };

    inline juce::String noteName (int n) { static const char* nm[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return n < 0 ? juce::String ("-") : juce::String (nm[n % 12]) + juce::String (n / 12 - 1); }

    // normalised autocorrelation pitch (McLeod-like): the first strong peak, 25 .. 1500 Hz
    // "G1 +40 ct" (the cents only when > 10 off the note)
    inline juce::String noteText (float hz)
    {
        if (hz <= 0.f) return "-";
        const double m = 69.0 + 12.0 * std::log2 (hz / 440.0); const int nn = (int) std::lround (m); const int ct = (int) std::lround ((m - nn) * 100.0);
        return noteName (nn) + (std::abs (ct) > 10 ? (ct > 0 ? " +" : " ") + juce::String (ct) + " ct" : juce::String());
    }
    inline float pitchAt (const float* x, int n, double rate, float& conf)
    {
        conf = 0.f;
        const int minLag = juce::jmax (2, (int) (rate / 1500.0)), maxLag = juce::jmin (n / 2, (int) (rate / 25.0));
        if (maxLag <= minLag + 2) return 0.f;
        std::vector<float> nsdf ((size_t) maxLag + 1, 0.f);
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double ac = 0, e = 0;
            for (int i = 0; i + lag < n; ++i) { ac += (double) x[i] * x[i + lag]; e += (double) x[i] * x[i] + (double) x[i + lag] * x[i + lag]; }
            nsdf[(size_t) lag] = e > 1.0e-12 ? (float) (2.0 * ac / e) : 0.f;
        }
        float best = 0.f; for (int lag = minLag; lag <= maxLag; ++lag) best = juce::jmax (best, nsdf[(size_t) lag]);
        if (best < 0.3f) return 0.f;
        for (int lag = minLag + 1; lag < maxLag; ++lag)
            if (nsdf[(size_t) lag] > 0.85f * best && nsdf[(size_t) lag] >= nsdf[(size_t) lag - 1] && nsdf[(size_t) lag] >= nsdf[(size_t) lag + 1])
            {
                const float a = nsdf[(size_t) lag - 1], b = nsdf[(size_t) lag], c = nsdf[(size_t) lag + 1];
                const float den = a - 2.f * b + c, shift = std::abs (den) > 1.0e-9f ? 0.5f * (a - c) / den : 0.f;
                conf = b;
                return (float) (rate / ((double) lag + shift));
            }
        return 0.f;
    }

    inline Report analyse (const juce::AudioBuffer<float>& in, double rate)
    {
        Report r; r.rate = rate;
        const int n = in.getNumSamples(), ch = in.getNumChannels();
        if (n < (int) (0.02 * rate) || ch < 1) return r;
        r.seconds = n / rate;
        const float* L = in.getReadPointer (0); const float* R = in.getReadPointer (ch > 1 ? 1 : 0);
        std::vector<float> mono ((size_t) n);
        for (int i = 0; i < n; ++i) mono[(size_t) i] = 0.5f * (L[i] + R[i]);
        r.m = meter::measure (in, rate);
        if (r.m.samplePeakDb < -90.f) return r;
        r.crestDb = r.m.samplePeakDb - r.m.rmsDb;
        for (int i = 0; i < n; ++i) if (std::abs (L[i]) >= 0.999f || std::abs (R[i]) >= 0.999f) ++r.clipped;
        // waveform
        r.wmin.assign (512, 0.f); r.wmax.assign (512, 0.f);
        for (int k = 0; k < 512; ++k)
        {
            const int a = (int) ((int64_t) k * n / 512), e = juce::jmax (a + 1, (int) ((int64_t) (k + 1) * n / 512));
            float lo = 0.f, hi = 0.f; for (int i = a; i < e && i < n; ++i) { lo = juce::jmin (lo, mono[(size_t) i]); hi = juce::jmax (hi, mono[(size_t) i]); }
            r.wmin[(size_t) k] = lo; r.wmax[(size_t) k] = hi;
        }
        // envelope (5 ms RMS) + length + transient
        const int hop = juce::jmax (1, (int) (0.005 * rate));
        float envPk = 0.f; std::vector<float> lin;
        for (int a = 0; a < n; a += hop) { double q = 0; const int e = juce::jmin (n, a + hop); for (int i = a; i < e; ++i) q += (double) mono[(size_t) i] * mono[(size_t) i]; lin.push_back ((float) std::sqrt (q / juce::jmax (1, e - a))); envPk = juce::jmax (envPk, lin.back()); }
        for (auto v : lin) r.env.push_back (juce::Decibels::gainToDecibels (v / juce::jmax (1.0e-9f, envPk), -120.f));
        int last = 0; for (int k = 0; k < (int) r.env.size(); ++k) if (r.env[(size_t) k] > -40.f) last = k;
        r.lengthMs = (float) ((last + 1) * 5);
        // loudness / crest over the KICK only (to its -40 dB end): trailing silence in a render or a file must not make it look "dynamic"
        {
            const int nk = juce::jlimit (juce::jmin (n, 64), n, (int) (r.lengthMs * 0.001 * rate));
            juce::AudioBuffer<float> kick (const_cast<float* const*> (in.getArrayOfReadPointers()), ch, nk);
            r.m = meter::measure (kick, rate);
            r.crestDb = r.m.samplePeakDb - r.m.rmsDb;
        }
        {
            float pk = 0.f; int pkAt = 0; for (int i = 0; i < juce::jmin (n, (int) (0.1 * rate)); ++i) if (std::abs (mono[(size_t) i]) > pk) { pk = std::abs (mono[(size_t) i]); pkAt = i; }
            int a10 = 0, a90 = pkAt; for (int i = 0; i <= pkAt; ++i) { if (a10 == 0 && std::abs (mono[(size_t) i]) > 0.1f * pk) a10 = i; if (std::abs (mono[(size_t) i]) > 0.9f * pk) { a90 = i; break; } }
            r.attackMs = (float) ((a90 - a10) * 1000.0 / rate);
            auto rmsOf = [&] (double t0, double t1) { double q = 0; int c = 0; for (int i = (int) (t0 * rate); i < juce::jmin (n, (int) (t1 * rate)); ++i) { q += (double) mono[(size_t) i] * mono[(size_t) i]; ++c; } return std::sqrt (q / juce::jmax (1, c)); };
            r.transientDb = (float) juce::Decibels::gainToDecibels (rmsOf (0.0, 0.01) / juce::jmax (1.0e-9, rmsOf (0.03, 0.08)), -60.0);
            // click centroid: the first 5 ms (zero crossings rate as a robust centroid estimate)
            int zc = 0; const int e = juce::jmin (n, (int) (0.005 * rate)); for (int i = 1; i < e; ++i) if ((mono[(size_t) i] >= 0.f) != (mono[(size_t) i - 1] >= 0.f)) ++zc;
            r.clickHz = (float) (zc * 0.5 / juce::jmax (1.0e-3, e / rate));
        }
        // pitch curve (5 ms hop, 30 ms window) on a low-passed, decimated copy (~11 kHz: the pitch of a kick is far below)
        {
            const int dec = juce::jmax (1, (int) std::lround (rate / 11025.0));
            const double lr = rate / dec;
            std::vector<float> low, deep;      // 2 kHz (the high drop) and 250 Hz (the low tail under a screaming mid) low-passed copies
            { const double k = 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 2000.0 / rate), kd = 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 250.0 / rate);
              double y1 = 0, y2 = 0, d1 = 0, d2 = 0, d3 = 0, d4 = 0;
              for (int i = 0; i < n; ++i)
              {
                  y1 += k * (mono[(size_t) i] - y1); y2 += k * (y1 - y2);
                  d1 += kd * (mono[(size_t) i] - d1); d2 += kd * (d1 - d2); d3 += kd * (d2 - d3); d4 += kd * (d3 - d4);
                  if (i % dec == 0) { low.push_back ((float) y2); deep.push_back ((float) d4); }
              } }
            auto energy = [] (const float* x, int len) { double q = 0; for (int i = 0; i < len; ++i) q += (double) x[i] * x[i]; return q; };
            // a short window (20 ms) for the fast high drop, a long one (70 ms, down to ~30 Hz) when the short one finds nothing clear;
            // when most of the energy sits under 250 Hz and the 2 kHz copy locks on a mid resonance / harmonic, the 250 Hz copy wins
            const int ws = (int) (0.02 * lr), wl = (int) (0.07 * lr), hp = juce::jmax (1, (int) (0.005 * lr)), nl = (int) low.size();
            for (int a = 0; a + ws < nl && a < (int) (r.lengthMs * 0.001 * lr); a += hp)
            {
                float conf; float hz = pitchAt (low.data() + a, ws, lr, conf);
                if ((hz <= 0.f || conf < 0.8f || hz < 110.f) && a + wl < nl) { float c2; const float h2 = pitchAt (low.data() + a, wl, lr, c2); if (h2 > 0.f && c2 >= conf) { hz = h2; conf = c2; } }
                if (a + wl < nl && (hz <= 0.f || hz > 150.f))
                {
                    const double eA = energy (low.data() + a, wl), eB = energy (deep.data() + a, wl);
                    float cd; const float hd = pitchAt (deep.data() + a, wl, lr, cd);
                    if (hd >= 25.f && hd < 200.f && cd >= 0.85f && eB > 0.5 * eA && (hz <= 0.f || hz > hd * 1.4f)) { hz = hd; conf = cd; }
                }
                r.pitch.push_back (hz); r.pitchConf.push_back (conf);
            }
            // the tail note = the loudness-weighted median of the confident pitch over the second half (a quiet screech after the sub has ended does not win)
            std::vector<std::pair<float, double>> good;
            const int np = (int) r.pitch.size();
            for (int k = np / 2; k < np; ++k)
                if (r.pitch[(size_t) k] > 0.f && r.pitchConf[(size_t) k] > 0.6f) { const double w = lin[(size_t) juce::jmin (k, (int) lin.size() - 1)]; good.push_back ({ r.pitch[(size_t) k], w * w }); }
            if (! good.empty())
            {
                std::sort (good.begin(), good.end()); double tot = 0, acc = 0; for (auto& g : good) tot += g.second;
                for (auto& g : good) { acc += g.second; if (acc >= 0.5 * tot) { r.endHz = g.first; break; } }
            }
            // the start of the drop = the highest clear pitch in the first 30 ms (the click frame alone is not a pitch)
            for (int k = 0; k < juce::jmin (np, 6); ++k) if (r.pitch[(size_t) k] > 0.f && r.pitchConf[(size_t) k] >= 0.8f) r.startHz = juce::jmax (r.startHz, r.pitch[(size_t) k]);
            if (r.startHz <= 0.f) for (int k = 0; k < juce::jmin (np, 4); ++k) if (r.pitch[(size_t) k] > 0.f) { r.startHz = r.pitch[(size_t) k]; break; }
            if (r.endHz > 0.f)
            {
                r.endNote = juce::roundToInt (69.0 + 12.0 * std::log2 (r.endHz / 440.0));
                for (int k = 0; k < np; ++k)
                    if (r.pitch[(size_t) k] > 0.f && std::abs (12.f * std::log2 (r.pitch[(size_t) k] / r.endHz)) < 1.f)
                    {
                        bool stays = true; for (int j = k; j < juce::jmin (np, k + 6); ++j) stays &= r.pitch[(size_t) j] <= 0.f || std::abs (12.f * std::log2 (r.pitch[(size_t) j] / r.endHz)) < 1.5f;
                        if (stays) { r.dropMs = (float) (k * 5); break; }
                    }
                // a held TOK: the pitch stays (within 1 st) above the end note for > 30 ms before the drop
                int run = 0, bestRun = 0;
                for (int k = 1; k < np && k * 5 < r.dropMs; ++k)
                {
                    const float a = r.pitch[(size_t) k], b = r.pitch[(size_t) k - 1];
                    if (a > r.endHz * 1.3f && b > 0.f && std::abs (12.f * std::log2 (a / b)) < 1.f) bestRun = juce::jmax (bestRun, ++run); else run = 0;
                }
                r.tokMs = (float) (bestRun * 5);
            }
        }
        // spectrum + bands + harmonics (Hann 8192, 50 % overlap over the kick)
        {
            constexpr int order = 13, N = 1 << order;
            juce::dsp::FFT fft (order);
            std::vector<float> buf ((size_t) (2 * N)), power ((size_t) (N / 2), 0.f), tailP ((size_t) (N / 2), 0.f);
            const int len = juce::jmax (1, juce::jmin (n, (int) ((r.lengthMs + 20.f) * 0.001 * rate)));
            for (int start = 0; start < len; start += N / 2)
            {
                std::fill (buf.begin(), buf.end(), 0.f);
                for (int i = 0; i < N && start + i < n; ++i) buf[(size_t) i] = mono[(size_t) (start + i)] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) N));
                fft.performFrequencyOnlyForwardTransform (buf.data());
                const bool tail = start >= (int) (r.dropMs * 0.001 * rate);
                for (int b = 0; b < N / 2; ++b) { power[(size_t) b] += buf[(size_t) b] * buf[(size_t) b]; if (tail) tailP[(size_t) b] += buf[(size_t) b] * buf[(size_t) b]; }
                if (start + N >= len) break;
            }
            double band[numBands] {}, total = 0, sub = 0;
            std::array<double, specBins> sp {}; std::array<int, specBins> cnt {};
            for (int b = 1; b < N / 2; ++b)
            {
                const double hz = b * rate / N, p = power[(size_t) b];
                for (int k = 0; k < numBands; ++k) if (hz >= bandEdges[k] && hz < bandEdges[k + 1]) band[k] += p;
                if (hz >= 20.0 && hz < 16000.0) total += p;
                if (hz >= 20.0 && hz < 100.0) sub += p;
                if (hz >= 20.0 && hz <= 20000.0) { const int k = juce::jlimit (0, specBins - 1, (int) std::lround (std::log (hz / 20.0) / std::log (1000.0) * (specBins - 1))); sp[(size_t) k] += p; ++cnt[(size_t) k]; }
            }
            // power per log band = the mean density x the band width (low bands with no FFT bin take their neighbours' density: no comb)
            {
                std::array<double, specBins> dens {};
                for (int k = 0; k < specBins; ++k) dens[(size_t) k] = cnt[(size_t) k] > 0 ? sp[(size_t) k] / cnt[(size_t) k] : -1.0;
                for (int k = 0; k < specBins; ++k)
                    if (dens[(size_t) k] < 0.0)
                    {
                        int a = k - 1, b = k + 1; while (a >= 0 && dens[(size_t) a] < 0.0) --a; while (b < specBins && dens[(size_t) b] < 0.0) ++b;
                        const double va = a >= 0 ? dens[(size_t) a] : 0.0, vb = b < specBins ? dens[(size_t) b] : va;
                        dens[(size_t) k] = a >= 0 && b < specBins ? va + (vb - va) * (k - a) / (double) (b - a) : (a >= 0 ? va : vb);
                    }
                for (int k = 0; k < specBins; ++k) sp[(size_t) k] = juce::jmax (0.0, dens[(size_t) k]) * specHz (k);   // (width of a log band ~ its frequency)
            }
            if (total > 0)
            {
                for (int k = 0; k < numBands; ++k) r.bandDb[(size_t) k] = (float) (10.0 * std::log10 (band[k] / total + 1.0e-9));
                r.subShare = (float) (100.0 * sub / total);
            }
            double spMax = 1.0e-20; for (auto v : sp) spMax = juce::jmax (spMax, v);
            for (int k = 0; k < specBins; ++k) r.spec[(size_t) k] = (float) (10.0 * std::log10 (sp[(size_t) k] / spMax + 1.0e-12));
            if (r.endHz > 0.f)
            {
                auto at = [&] (double hz) { const int b = (int) std::lround (hz * N / rate); double m = 0; for (int d = -2; d <= 2; ++d) if (b + d > 0 && b + d < N / 2) m = juce::jmax (m, (double) tailP[(size_t) (b + d)]); return m; };
                const double f1 = at (r.endHz);
                for (int h = 1; h <= 10; ++h) r.harm[(size_t) h - 1] = (float) (10.0 * std::log10 (at (r.endHz * h) / juce::jmax (1.0e-20, f1) + 1.0e-12));
            }
        }
        // stereo & phase
        {
            double ll = 0, rr = 0, lr = 0, mm = 0, ss = 0, lll = 0, lrr = 0, llr = 0;
            const double k = 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 150.0 / rate);
            double fl = 0, fr = 0;
            for (int i = 0; i < n; ++i)
            {
                const double a = L[i], b = R[i];
                ll += a * a; rr += b * b; lr += a * b; mm += 0.25 * (a + b) * (a + b); ss += 0.25 * (a - b) * (a - b);
                fl += k * (a - fl); fr += k * (b - fr); lll += fl * fl; lrr += fr * fr; llr += fl * fr;
            }
            r.corr = ll * rr > 1.0e-18 ? (float) (lr / std::sqrt (ll * rr)) : 1.f;
            r.lowCorr = lll * lrr > 1.0e-18 ? (float) (llr / std::sqrt (lll * lrr)) : 1.f;
            r.sideDb = (float) (10.0 * std::log10 (ss / juce::jmax (1.0e-18, mm) + 1.0e-12));
            r.monoLossDb = (float) (10.0 * std::log10 (mm / juce::jmax (1.0e-18, 0.5 * (ll + rr)) + 1.0e-12));
            r.stereo = r.sideDb > -40.f;
        }
        // features in plain words
        auto& f = r.features;
        if (r.tokMs >= 30.f) f.add ("TONAL TOK held ~" + juce::String (juce::roundToInt (r.tokMs)) + " ms before the drop");
        if (r.endHz > 0.f) f.add ("TAIL NOTE " + noteText (r.endHz) + " (" + juce::String (r.endHz, 1) + " Hz), pitch drop " + juce::String (juce::roundToInt (r.dropMs)) + " ms");
        if (r.transientDb > 6.f) f.add ("STRONG TRANSIENT (+" + juce::String (r.transientDb, 1) + " dB over the body)");
        else if (r.transientDb < 1.f) f.add ("SOFT START (no clear transient)");
        const float hiShare = r.bandDb[4] > -60.f ? r.bandDb[4] : -60.f;
        if (r.crestDb < 5.f && hiShare > -18.f) f.add ("HEAVY DISTORTION (crest " + juce::String (r.crestDb, 1) + " dB, a lot of presence)");
        else if (r.crestDb > 8.f) f.add ("DYNAMIC / CLEAN (crest " + juce::String (r.crestDb, 1) + " dB)");
        if (r.bandDb[4] > -12.f || r.bandDb[5] > -18.f) f.add ("BRIGHT / SCREECHY TOP");
        if (r.subShare > 85.f) f.add ("SUB-HEAVY (" + juce::String (juce::roundToInt (r.subShare)) + " % under 100 Hz)");
        if (r.stereo) f.add ("STEREO (side " + juce::String (r.sideDb, 1) + " dB)");
        if (r.lowCorr < 0.9f) f.add ("WARNING: the low end is not mono (corr " + juce::String (r.lowCorr, 2) + ")");
        if (r.corr < 0.f) f.add ("WARNING: PHASE PROBLEM (correlation " + juce::String (r.corr, 2) + ")");
        if (r.monoLossDb < -3.f) f.add ("WARNING: loses " + juce::String (-r.monoLossDb, 1) + " dB in mono");
        if (r.m.dc) f.add ("WARNING: DC OFFSET");
        if (r.clipped > 8) f.add ("CLIPPED SAMPLES (" + juce::String (r.clipped) + ")");
        r.valid = true;
        return r;
    }
}
