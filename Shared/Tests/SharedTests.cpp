// Checks for the Shared blocks: DoomsdayDrive.h (anti-aliasing, level, oversampling) and DoomsdayMod.h (Shape, LFO, sync, matrix)
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include "DoomsdayDrive.h"
#include "DoomsdayMod.h"
#include "DoomsdayFilter.h"
#include "DoomsdayDSP.h"
#include <cstdio>
#include <vector>
#include <set>

using namespace juce;
static bool ok = true;
static void check (bool c, const char* what, const String& extra = {}) { printf ("%-56s %s %s\n", what, c ? "OK" : "FAIL", extra.toRawUTF8()); ok &= c; }

// energy that is NOT on a harmonic of f0 (= aliasing), relative to the total, in dB
static double aliasDb (const std::vector<float>& x, double sr, double f0)
{
    const int order = 15, n = 1 << order;
    std::vector<float> w ((size_t) n * 2, 0.f);
    for (int i = 0; i < n; ++i) w[(size_t) i] = x[(size_t) (x.size() - n + i)] * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * i / n));
    juce::dsp::FFT fft (order); fft.performFrequencyOnlyForwardTransform (w.data());
    double harm = 0, other = 0;
    for (int k = 1; k < n / 2; ++k)
    {
        const double hz = k * sr / n, h = hz / f0, e = (double) w[(size_t) k] * w[(size_t) k];
        if (std::abs (h - std::round (h)) * f0 < 3.0 * sr / n) harm += e; else other += e;
    }
    return 10.0 * std::log10 (other / (harm + other) + 1e-30);
}

int main()
{
    using namespace doomsday;
    const double sr = 48000.0;
    // ---------------------------------------------------------------- drive
    {
        const double f0 = 2637.0;           // E7: a high screech note, harmonics fold back hard
        const int len = 512 * 94;
        std::vector<float> in ((size_t) len);
        for (int i = 0; i < len; ++i) in[(size_t) i] = 0.9f * (float) std::sin (2.0 * MathConstants<double>::pi * f0 * i / sr);
        for (int c : { drive::Hard, drive::Soft, drive::Tube })
        {
            const float g = 8.f;
            std::vector<float> naive ((size_t) len), adaa ((size_t) len), os2 ((size_t) len);
            drive::Adaa1 a;
            for (int i = 0; i < len; ++i) { naive[(size_t) i] = drive::plain (in[(size_t) i] * g, c); adaa[(size_t) i] = a.process (in[(size_t) i] * g, c); }
            // ADAA + 2x oversampling
            drive::OverSampler os; os.prepare (1, 512); os.setFactorIndex (1);
            drive::Adaa1 a2;
            AudioBuffer<float> buf (1, 512);
            for (int pos = 0; pos < len; pos += 512)
            {
                for (int i = 0; i < 512; ++i) buf.setSample (0, i, in[(size_t) (pos + i)]);
                juce::dsp::AudioBlock<float> blk (buf);
                os.process (blk, [&] (int, float x) { return a2.process (x * g, c); });
                for (int i = 0; i < 512; ++i) os2[(size_t) (pos + i)] = buf.getSample (0, i);
            }
            const double an = aliasDb (naive, sr, f0), aa = aliasDb (adaa, sr, f0), ao = aliasDb (os2, sr, f0);
            check (aa < an - 6.0 && ao < aa - 3.0, (String ("ADAA less aliasing than plain: ") + drive::curveName (c)).toRawUTF8(),
                   "(plain " + String (an, 1) + " dB, ADAA " + String (aa, 1) + " dB, ADAA+2x " + String (ao, 1) + " dB)");
        }
        // quiet signal passes at unity (no level jump)
        for (int c = 0; c < drive::numCurves; ++c)
        {
            drive::Adaa1 a; double e1 = 0, e2 = 0;
            for (int i = 0; i < 4800; ++i) { const float x = 0.01f * (float) std::sin (2.0 * MathConstants<double>::pi * 100.0 * i / sr); const float y = a.process (x, c); if (i > 100) { e1 += x * x; e2 += y * y; } }
            const double db = 10.0 * std::log10 (e2 / e1);
            check (std::abs (db) < (c == drive::Fold ? 4.5 : 0.3), (String ("unity for quiet signals: ") + drive::curveName (c)).toRawUTF8(), "(" + String (db, 2) + " dB)");
        }
        drive::OverSampler os; os.prepare (2, 256);
        os.setFactorIndex (2);
        check (os.ratio() == 4 && os.latencySamples() > 0.f, "OverSampler 4x reports latency", "(" + String (os.latencySamples(), 2) + " samples)");
    }
    // ---------------------------------------------------------------- mod
    {
        using mod::Shape;
        auto s = Shape::make ({ { 0, 0 }, { 500, 1, 0.8f }, { 1000, 0 } });
        check (std::abs (s.at (250) - 0.5f) < 0.01f && s.at (750) > 0.85f, "Shape bend bends only the segment after its dot", "(" + String (s.at (250), 3) + ", " + String (s.at (750), 3) + ")");
        Shape r; Shape::fromString (s.toString(), r);
        check (r == s, "Shape string round trip (with bend)");
        Shape old; Shape::fromString ("0.00:1.0000;800.00:0.9200;1000.00:0.0000", old);
        check (old.n == 3 && old.pts[1].bend == 0.f && std::abs (old.at (400) - 0.96f) < 1e-3f, "old \"ms:v\" strings still load straight");

        mod::Lfo l; l.prepare (sr); l.setWave (mod::Lfo::Sine); l.setRateHz (2.0);
        int crossings = 0; float prev = l.next();
        for (int i = 1; i < (int) sr; ++i) { const float v = l.next(); if (prev > 0.f && v <= 0.f) ++crossings; prev = v; }
        check (crossings == 2, "LFO 2 Hz -> 2 cycles per second", "(" + String (crossings) + ")");
        mod::Lfo ls; ls.prepare (sr); ls.setWave (mod::Lfo::SawUp); ls.setSynced (mod::syncBeats (9), 150.0);   // 1/4 at 150 BPM = 0.4 s
        ls.syncTo (2.5);                                                                                         // halfway through a beat
        check (std::abs (ls.getPhase() - 0.5) < 1e-9, "LFO syncTo (ppq) locks the phase to the song", "(" + String (ls.getPhase(), 3) + ")");
        float mx = -2, mn = 2; for (int i = 0; i < (int) (sr * 0.4); ++i) { const float v = ls.next(); mx = jmax (mx, v); mn = jmin (mn, v); }
        check (mx > 0.99f && mn < -0.99f, "synced 1/4 at 150 BPM covers one full cycle in 0.4 s");
        auto drawn = Shape::make ({ { 0, 1 }, { 300, 0 }, { 1000, 1 } });
        mod::Lfo ld; ld.prepare (sr); ld.setWave (mod::Lfo::Drawn); ld.setShape (&drawn); ld.setRateHz (1.0);
        float first = ld.next(); for (int i = 1; i < (int) (sr * 0.3); ++i) ld.next();
        check (first > 0.99f && ld.next() < -0.98f, "DRAWN LFO plays the Shape as one cycle (sidechain curve)");
        mod::Matrix<3, 2, 4> m;
        m.setSlot (0, 0, 1, 0.5f); m.setSlot (1, 1, 1, -1.f, 2);
        m.setSource (0, 1.f); m.setSource (1, 0.8f); m.setSource (2, 0.25f); m.compute();
        check (std::abs (m.dest (1) - (0.5f - 0.2f)) < 1e-6f && m.dest (0) == 0.f, "Matrix sums slots per destination (with via)", "(" + String (m.dest (1), 3) + ")");
    }
    // ---------------------------------------------------------------- filter
    {
        using doomsday::filter::Ladder;
        auto gainAt = [&] (Ladder f, double hz)
        {
            f.reset(); double e1 = 0, e2 = 0;
            for (int i = 0; i < 24000; ++i)
            {
                const float x = 0.05f * (float) std::sin (2.0 * MathConstants<double>::pi * hz * i / sr);
                const float y = f.process (x);
                if (i > 12000) { e1 += x * x; e2 += y * y; }
            }
            return 10.0 * std::log10 (e2 / e1);
        };
        Ladder f; f.prepare (sr); f.setCutoff (1000.f); f.setResonance (0.f); f.setDrive (0.f); f.setBassComp (0.f);
        const double g100 = gainAt (f, 100.0), g1k = gainAt (f, 1000.0), g4k = gainAt (f, 4000.0), g8k = gainAt (f, 8000.0);
        check (std::abs (g100) < 0.5 && std::abs (g1k + 12.0) < 1.5 && g8k < g4k - 18.0, "ladder LP24: flat below, -12 dB at cutoff, 24 dB/oct",
               "(100 Hz " + String (g100, 1) + ", 1k " + String (g1k, 1) + ", 4k " + String (g4k, 1) + ", 8k " + String (g8k, 1) + ")");
        f.setMode (doomsday::filter::HP24);
        const double h100 = gainAt (f, 100.0), h10k = gainAt (f, 10000.0);
        check (h10k > -1.0 && h100 < -60.0, "ladder HP24: passes highs, removes lows", "(10k " + String (h10k, 1) + ", 100 Hz " + String (h100, 1) + ")");
        f.setMode (doomsday::filter::LP12);
        check (std::abs (gainAt (f, 1000.0) + 6.0) < 1.5, "ladder LP12: -6 dB at cutoff");
        // self-oscillation: a kick of noise, then silence -> it keeps singing at the cutoff
        {
            Ladder o; o.prepare (sr); o.setCutoff (500.f); o.setResonance (1.05f); o.setDrive (0.f);
            std::vector<float> y (48000);
            for (int i = 0; i < 48000; ++i) y[(size_t) i] = o.process (i < 10 ? 0.5f : 0.f);
            int zc = 0; float pk = 0.f;
            for (int i = 24000; i < 48000; ++i) { if (y[(size_t) i - 1] < 0.f && y[(size_t) i] >= 0.f) ++zc; pk = jmax (pk, std::abs (y[(size_t) i])); }
            const double hz = zc / 0.5;
            check (pk > 0.05f && std::abs (hz - 500.0) < 50.0, "ladder self-oscillates at the cutoff", "(" + String (hz, 0) + " Hz, peak " + String (pk, 2) + ")");
        }
        // stability: wild cutoff modulation every sample + loud noise at max resonance
        {
            Ladder w; w.prepare (sr); w.setResonance (1.05f); w.setDrive (18.f);
            Random rng (3); bool fine = true; float pk = 0.f;
            for (int i = 0; i < 96000; ++i)
            {
                w.setCutoff (20.f * std::pow (1000.f, rng.nextFloat()));
                const float y = w.process ((rng.nextFloat() * 2.f - 1.f) * 2.f);
                fine &= std::isfinite (y); pk = jmax (pk, std::abs (y));
            }
            check (fine && pk < 4.f, "ladder stable under wild modulation", "(peak " + String (pk, 2) + ")");
        }
        // drive: more harmonics when pushed
        {
            auto thd = [&] (float driveDb)
            {
                Ladder d; d.prepare (sr); d.setCutoff (8000.f); d.setDrive (driveDb);
                const int n = 1 << 14; std::vector<float> w ((size_t) n * 2, 0.f);
                for (int i = 0; i < n + 4800; ++i)
                {
                    const float y = d.process (0.5f * (float) std::sin (2.0 * MathConstants<double>::pi * 187.5 * i / sr));
                    if (i >= 4800) w[(size_t) (i - 4800)] = y * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (i - 4800) / n));
                }
                juce::dsp::FFT fft (14); fft.performFrequencyOnlyForwardTransform (w.data());
                const int b = (int) std::round (187.5 * n / sr);
                double h = 0; for (int k = 2; k <= 8; ++k) h += (double) w[(size_t) (b * k)] * w[(size_t) (b * k)];
                return 10.0 * std::log10 (h / ((double) w[(size_t) b] * w[(size_t) b]) + 1e-20);
            };
            const double t0 = thd (0.f), t24 = thd (24.f);
            check (t24 > t0 + 15.0, "ladder DRIVE adds warmth (harmonics)", "(0 dB " + String (t0, 1) + " -> +24 dB " + String (t24, 1) + ")");
        }
    }
    // ---------------------------------------------------------------- unison: side-voice level (supersaw MIX)
    {
        doomsday::dsp::UnisonBank::Table a, b;
        a.setup (7, 2, 40.f, 1.f, 0.f, 1.f);
        b.setup (7, 2, 40.f, 1.f, 0.f, 1.f, 0.3f);
        check (std::abs ((b.gl[3] + b.gr[3]) - (a.gl[3] + a.gr[3])) < 1e-6f && (b.gl[0] + b.gr[0]) < (a.gl[0] + a.gr[0]) * 0.5f, "unison SIDES level: centre kept, detuned voices turned down");
    }
    // ---------------------------------------------------------------- drive batch 2
    {
        using namespace doomsday::drive;
        Bitcrusher bc; bc.prepare (sr); bc.set (sr, 3.f, 1.f);
        std::set<float> lv; for (int i = 0; i < 2000; ++i) lv.insert (bc.process (std::sin (i * 0.01f) * 0.99f));
        check (lv.size() <= 9, "Bitcrusher 3 bits -> at most 9 levels", "(" + String ((int) lv.size()) + ")");
        bc.set (4000.0f, 24.f, 1.f); int changes = 0; float prev = 0.f;
        for (int i = 0; i < 4800; ++i) { const float y = bc.process (std::sin (i * 0.05f)); if (y != prev) ++changes; prev = y; }
        check (std::abs (changes - 400) < 10, "Bitcrusher 4 kHz -> 400 new values per 0.1 s", "(" + String (changes) + ")");

        DrawnShaper ds; ds.setShape (DrawnShaper::identity(), false);
        float err = 0.f; for (float x = -1.f; x <= 1.f; x += 0.01f) err = jmax (err, std::abs (ds.process (x) - x));
        check (err < 1e-3f, "DrawnShaper identity line passes the signal");
        ds.setShape (doomsday::mod::Shape::make ({ { 0, 0.5f }, { 500, 0.5f }, { 600, 1.f }, { 1000, 1.f } }), true);
        check (std::abs (ds.process (0.5f) - 1.f) < 1e-3f && std::abs (ds.process (-0.5f) + 1.f) < 1e-3f && std::abs (ds.process (0.05f)) < 0.6f,
               "DrawnShaper symmetric hard curve");

        // kick-like burst: silence, a hit, a ring-out
        std::vector<float> kick (24000, 0.f);
        for (int i = 4800; i < 24000; ++i) { const float t = (i - 4800) / (float) sr; kick[(size_t) i] = std::sin (2.f * MathConstants<float>::pi * 60.f * t) * std::exp (-t * 6.f) * 0.8f; }
        auto peakIn = [&] (const std::vector<float>& v, int a, int b) { float m = 0; for (int i = a; i < b; ++i) m = jmax (m, std::abs (v[(size_t) i])); return m; };
        TransientShaper ts; ts.prepare (sr);
        std::vector<float> o (kick.size()); for (size_t i = 0; i < kick.size(); ++i) o[i] = ts.process (kick[i], 9.f, 0.f);
        const float hitGain = peakIn (o, 4800, 5300) / peakIn (kick, 4800, 5300), tailGain = peakIn (o, 18000, 20000) / peakIn (kick, 18000, 20000);
        check (hitGain > 1.4f && std::abs (tailGain - 1.f) < 0.15f, "TransientShaper ATTACK +9 dB lifts the hit, not the tail", "(hit x" + String (hitGain, 2) + ", tail x" + String (tailGain, 2) + ")");
        ts.reset(); for (size_t i = 0; i < kick.size(); ++i) o[i] = ts.process (kick[i], 0.f, -12.f);
        check (peakIn (o, 18000, 20000) < peakIn (kick, 18000, 20000) * 0.7f, "TransientShaper SUSTAIN -12 dB shortens the ring-out");

        DynamicDrive dd; dd.prepare (sr); float onHit = 1.f, later = 0.f;
        for (int i = 0; i < 24000; ++i) { const float m = dd.process (kick[(size_t) i], 1.f); if (i > 4800 && i < 5300) onHit = jmin (onHit, m); if (i == 15000) later = m; }
        check (onHit < 0.5f && later > 0.9f, "DynamicDrive: less drive on the hit, full drive on the tail", "(" + String (onHit, 2) + " / " + String (later, 2) + ")");

        AutoGain ag; ag.prepare (sr); double ein = 0, eout = 0;
        for (int i = 0; i < 96000; ++i) { const float x = 0.2f * std::sin (i * 0.03f); const float y = ag.process (x, std::tanh (x * 8.f)); if (i > 48000) { ein += x * x; eout += y * y; } }
        check (std::abs (10.0 * std::log10 (eout / ein)) < 1.0, "AutoGain keeps the loudness of the input", "(" + String (10.0 * std::log10 (eout / ein), 2) + " dB)");
    }
    printf ("%s\n", ok ? "ALL OK" : "SOME FAILED");
    return ok ? 0 : 1;
}
