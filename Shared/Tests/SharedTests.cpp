// Checks for the Shared blocks: DoomsdayDrive.h (anti-aliasing, level, oversampling) and DoomsdayMod.h (Shape, LFO, sync, matrix)
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include "DoomsdayDrive.h"
#include "DoomsdayMod.h"
#include <cstdio>
#include <vector>

using namespace juce;
static bool ok = true;
static void check (bool c, const char* what, const String& extra = {}) { printf ("%-56s %s %s\n", what, c ? "OK" : "FAIL", extra.toRawUTF8()); ok &= c; }

// energy that is NOT on a harmonic of f0 (= aliasing), relative to the total, in dB
static double aliasDb (const std::vector<float>& x, double sr, double f0)
{
    const int order = 15, n = 1 << order;
    std::vector<float> w ((size_t) n * 2, 0.f);
    for (int i = 0; i < n; ++i) w[(size_t) i] = x[(size_t) (x.size() - n + i)] * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * i / n));
    dsp::FFT fft (order); fft.performFrequencyOnlyForwardTransform (w.data());
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
                dsp::AudioBlock<float> blk (buf);
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
    printf ("%s\n", ok ? "ALL OK" : "SOME FAILED");
    return ok ? 0 : 1;
}
