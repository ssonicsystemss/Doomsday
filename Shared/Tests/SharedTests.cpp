// Checks for the Shared blocks: DoomsdayDrive.h (anti-aliasing, level, oversampling) and DoomsdayMod.h (Shape, LFO, sync, matrix)
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include "DoomsdayDrive.h"
#include "DoomsdayMod.h"
#include "DoomsdayFilter.h"
#include "DoomsdayDSP.h"
#include "DoomsdayOsc.h"
#include "DoomsdayShaper.h"
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
    // ---------------------------------------------------------------- mod batch 2
    {
        using namespace doomsday::mod;
        Lfo c; c.prepare (sr); c.setWave (Lfo::Chaos); c.setRateHz (2.0);
        std::vector<float> a (48000); float mx = 0.f; bool fin = true;
        for (auto& v : a) { v = c.next(); mx = jmax (mx, std::abs (v)); fin &= std::isfinite (v); }
        double diff = 0; for (int i = 0; i < 12000; ++i) diff += std::abs (a[(size_t) i] - a[(size_t) i + 24000]);
        check (fin && mx <= 1.f && mx > 0.3f && diff / 12000 > 0.05, "CHAOS LFO: bounded, moving, never repeats", "(max " + String (mx, 2) + ")");

        Performer pf; pf.setLength (4);
        pf.setStep (0, 0, Performer::RampUp, 1.f); pf.setStep (0, 1, Performer::Hold, 0.5f); pf.setStep (0, 2, Performer::Off, 1.f); pf.setStep (0, 3, Performer::ExpDown, 1.f);
        pf.setStep (1, 0, Performer::RampDown, 1.f);
        check (std::abs (pf.at (0.125, 0.25) - 0.5f) < 1e-4f && std::abs (pf.at (0.3, 0.25) - 0.5f) < 1e-4f && pf.at (0.6, 0.25) == 0.f && std::abs (pf.at (1.125, 0.25) - 0.5f) < 1e-4f,
               "Performer: step curves, heights, loop of 4 steps");
        pf.setMorph (1.f);
        check (std::abs (pf.at (0.05, 0.25) - 0.8f) < 1e-3f, "Performer MORPH plays sequence B");

        KeyScale ks; ks.root = 9; ks.scale = KeyScale::Minor;     // A minor
        check (ks.quantize (61) == 60 && ks.quantize (69) == 69 && ks.quantize (70) == 69 && ks.contains (67) && ! ks.contains (68), "KeyScale A minor: C# -> C, A# -> A, G in, G# out");
        ks.scale = KeyScale::HarmonicMinor;
        check (ks.contains (68) && ! ks.contains (67), "KeyScale A harmonic minor has G#, not G");

        Arp arp; arp.setMode (Arp::Up); arp.setOctaves (2); arp.setRateBeats (0.25); arp.setSteps (16);
        arp.noteOn (64, 1.f); arp.noteOn (60, 0.8f); arp.noteOn (67, 1.f);
        std::vector<int> notes; int ons = 0, offs = 0;
        arp.process (0.0, 150.0, sr, (int) (sr * 60.0 / 150.0 * 1.5), [&] (int, int note, float, bool on) { if (on) { notes.push_back (note); ++ons; } else ++offs; });
        const std::vector<int> expect { 60, 64, 67, 72, 76, 79 };
        check (notes == expect && offs >= ons - 1, "Arp UP x2 octaves: 60 64 67 72 76 79 (1.5 beats of 1/16)");
        RandomPerNote rp; rp.prepare (sr, 0.f); const float r1 = rp.trigger(), r2 = rp.trigger();
        check (r1 != r2 && std::abs (r1) <= 1.f && std::abs (r2) <= 1.f, "RandomPerNote: new value per note");
    }
    // ---------------------------------------------------------------- oscillators
    {
        using namespace doomsday::osc;
        auto spectrumOf = [&] (auto&& gen, std::vector<float>& mag, std::vector<float>& fr)
        {
            const int n = 1 << 14; std::vector<float> w ((size_t) n * 2, 0.f);
            for (int i = 0; i < 2000; ++i) gen();
            for (int i = 0; i < n; ++i) w[(size_t) i] = gen() * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * i / n));
            juce::dsp::FFT fft (14); fft.performFrequencyOnlyForwardTransform (w.data());
            mag.assign (w.begin(), w.begin() + n / 2); fr.resize ((size_t) n / 2); for (int k = 0; k < n / 2; ++k) fr[(size_t) k] = (float) (k * sr / n);
        };
        auto centroid = [&] (auto&& gen) { std::vector<float> m, f; spectrumOf (gen, m, f); double a = 0, b = 0; for (size_t k = 1; k < m.size(); ++k) { a += m[k] * m[k] * f[k]; b += m[k] * m[k]; } return a / b; };
        auto peakHz = [&] (auto&& gen, float lo) { std::vector<float> m, f; spectrumOf (gen, m, f); size_t best = 0; for (size_t k = 1; k < m.size(); ++k) if (f[k] > lo && (best == 0 || m[k] > m[best])) best = k; return (double) f[best]; };
        // share of energy above the 2nd harmonic, in dB
        auto rich = [&] (auto&& gen, double f0) { std::vector<float> m, f; spectrumOf (gen, m, f); double hi = 0, all = 0; for (size_t k = 1; k < m.size(); ++k) { all += m[k] * m[k]; if (f[k] > 2.5 * f0) hi += m[k] * m[k]; } return 10.0 * std::log10 (hi / all + 1e-12); };
        IntensityOsc o; o.prepare (sr);
        o.setMode (IntensityOsc::Spectrum); o.setIntensity (0.15f); const double cDull = rich ([&] { return o.next (220.f); }, 220.0);
        o.setIntensity (0.9f); const double cBright = rich ([&] { return o.next (220.f); }, 220.0);
        check (cBright > cDull + 15.0, "IntensityOsc SPECTRUM: intensity adds harmonics", "(upper harmonics " + String (cDull, 1) + " -> " + String (cBright, 1) + " dB)");
        o.setMode (IntensityOsc::Bend); o.setIntensity (0.f); const double b0 = rich ([&] { return o.next (220.f); }, 220.0);
        o.setIntensity (0.9f); const double b1 = rich ([&] { return o.next (220.f); }, 220.0);
        check (b0 < -60.0 && b1 > -15.0, "IntensityOsc BEND: 0 = sine, up = sharp", "(upper harmonics " + String (b0, 1) + " -> " + String (b1, 1) + " dB)");
        (void) centroid;
        o.setMode (IntensityOsc::Formant); o.setIntensity (0.6f);           // ~1.21 kHz formant
        const double f1 = peakHz ([&] { return o.next (150.f); }, 400.f), f2 = peakHz ([&] { return o.next (200.f); }, 400.f);
        check (std::abs (f1 - f2) < 0.15 * f1 && f1 > 900.0 && f1 < 1500.0, "IntensityOsc FORMANT stays put when the pitch moves", "(" + String (f1, 0) + " / " + String (f2, 0) + " Hz)");

        FmOps<4> fm; fm.prepare (sr);
        const double pure = centroid ([&] { return fm.next (440.f); });
        fm.setOp (1, 2.f, 1.f, 0.f, false); fm.setMod (1, 0, 1.5f);
        std::vector<float> m, fr; spectrumOf ([&] { return fm.next (440.f); }, m, fr);
        auto at = [&] (double hz) { const size_t k = (size_t) std::round (hz / (sr / (1 << 14))); float v = 0; for (size_t j = k - 2; j <= k + 2; ++j) v = jmax (v, m[j]); return v; };
        check (std::abs (pure - 440.0) < 20.0 && at (1320.0) > at (440.0) * 0.1f && at (2200.0) > at (440.0) * 0.02f, "FmOps: pure sine alone, sidebands at 440 + 2x440 x k with modulation");

        check (std::abs (ChordStack::ratio (ChordStack::Major, 1, 4, 1.f) - std::pow (2.f, 4.f / 12.f)) < 1e-5f && ChordStack::semitones (ChordStack::Major, 2, 4, 0.5f) == 3.5f
               && ChordStack::semitones (ChordStack::Minor, 3, 4, 0.f) == 0.f, "ChordStack: major third / half way / unison at 0");
        Slop sl; sl.prepare (sr); auto v3 = sl.forVoice (3, 1.f), v3b = sl.forVoice (3, 1.f), v4 = sl.forVoice (4, 1.f), z = sl.forVoice (3, 0.f);
        check (v3.pitchCents == v3b.pitchCents && v3.pitchCents != v4.pitchCents && std::abs (v3.pitchCents) <= 12.f && z.pitchCents == 0.f && z.cutoffMul == 1.f, "Slop: fixed per voice, bounded, off at 0");
    }
    // ---------------------------------------------------------------- shaper / time FX
    {
        using namespace doomsday::shaper;
        // kick every 0.4 s (150 BPM), with a tail
        const int beat = (int) (sr * 0.4); std::vector<float> kick ((size_t) beat * 4, 0.f);
        for (int b = 0; b < 4; ++b) for (int i = 0; i < beat; ++i) { const float t = i / (float) sr; kick[(size_t) (b * beat + i)] = std::sin (2.f * MathConstants<float>::pi * (55.f + 200.f * std::exp (-t * 40.f)) * t) * std::exp (-t * 5.f) * 0.9f; }
        OnsetDetector od; od.prepare (sr); od.setThresholdDb (-30.f); od.setMinGapMs (60.f); od.setSensitivity (0.5f);
        std::vector<int> hits; for (int i = 0; i < (int) kick.size(); ++i) if (od.process (kick[(size_t) i])) hits.push_back (i);
        bool aligned = hits.size() == 4; for (size_t k = 0; aligned && k < hits.size(); ++k) aligned = std::abs (hits[k] - (int) k * beat) < (int) (0.01 * sr);
        check (aligned, "OnsetDetector: 4 kicks -> 4 hits, on time, no false hits in the tails", "(" + String ((int) hits.size()) + ")");

        VolumeShaper vs; vs.prepare (sr); vs.setCurve (presetCurve (0)); vs.setDepth (1.f, 0.f, 0.f); vs.setCrossovers (150.f, 3500.f); vs.setSynced (true, 1.0);
        std::vector<float> L (24000), R (24000);
        for (int i = 0; i < 24000; ++i) L[(size_t) i] = R[(size_t) i] = 0.5f * std::sin (2.f * MathConstants<float>::pi * 50.f * i / (float) sr) + 0.3f * std::sin (2.f * MathConstants<float>::pi * 6000.f * i / (float) sr);
        auto Lin = L;
        vs.process (L.data(), R.data(), 24000, 0.0, 150.0);
        // at the start of each beat (curve = 0): the 50 Hz is gone, 6 kHz stays
        auto bandPeak = [&] (const std::vector<float>& v, int a, int n, float hz)
        { double re = 0, im = 0; for (int i = a; i < a + n; ++i) { re += v[(size_t) i] * std::cos (2.0 * MathConstants<double>::pi * hz * i / sr); im += v[(size_t) i] * std::sin (2.0 * MathConstants<double>::pi * hz * i / sr); } return std::sqrt (re * re + im * im) / n * 2.0; };
        const double lowAt = bandPeak (L, 19200, 480, 50.f), hiAt = bandPeak (L, 19200, 480, 6000.f), lowLater = bandPeak (L, 19200 + 9600, 1920, 50.f);
        (void) Lin;
        check (lowAt < 0.2 && std::abs (hiAt - 0.3) < 0.06 && lowLater > 0.4, "VolumeShaper LOW only: bass ducked on the beat, highs untouched", "(low " + String (lowAt, 2) + " -> " + String (lowLater, 2) + ", high " + String (hiAt, 2) + ")");

        TapeStop tsp; tsp.prepare (sr);
        std::vector<float> y (48000); int zcEarly = 0, zcLate = 0; float pkEnd = 0.f;
        for (int i = 0; i < 48000; ++i)
        {
            if (i == 4800) tsp.engage (500.f, 0.f);
            float l = std::sin (2.f * MathConstants<float>::pi * 440.f * i / (float) sr), r = l;
            tsp.process (l, r); y[(size_t) i] = l;
            if (i > 0 && y[(size_t) i - 1] < 0.f && l >= 0.f) { if (i > 4800 && i < 9600) ++zcEarly; if (i > 21600 && i < 26400) ++zcLate; }
            if (i > 36000) pkEnd = jmax (pkEnd, std::abs (l));
        }
        check (zcLate < zcEarly / 2 && pkEnd < 0.01f, "TapeStop: pitch falls, then silence", "(" + String (zcEarly) + " -> " + String (zcLate) + " cycles / 0.1 s)");

        Stutter st; st.prepare (sr);
        std::vector<float> in (48000), out (48000);
        Random rng (9); for (auto& v : in) v = rng.nextFloat() * 2.f - 1.f;
        const int slice = 2400;
        for (int i = 0; i < 48000; ++i) { if (i == 24000) st.engage (slice); float l = in[(size_t) i], r = l; st.process (l, r); out[(size_t) i] = l; }
        float err = 0.f;
        for (int i = 24000 + 200; i < 24000 + slice - 200; ++i) err = jmax (err, std::abs (out[(size_t) i] - in[(size_t) (i - slice)]));
        float rep = 0.f; for (int i = 24000 + 200; i < 24000 + slice - 200; ++i) rep = jmax (rep, std::abs (out[(size_t) i] - out[(size_t) (i + slice)]));
        check (err < 1e-5f && rep < 1e-5f, "Stutter: repeats exactly the last slice");

        Reverser rv; rv.prepare (sr); rv.setChunk (4800);
        for (int i = 0; i < 48000; ++i) { float l = in[(size_t) i], r = l; rv.process (l, r); out[(size_t) i] = l; }
        float rerr = 0.f;
        for (int p = 300; p < 4800 - 300; ++p) { const int i = 4800 * 5 + p; rerr = jmax (rerr, std::abs (out[(size_t) i] - in[(size_t) (4800 * 5 - 1 - p)])); }
        check (rerr < 1e-5f, "Reverser: the previous chunk, backwards");
    }
    printf ("%s\n", ok ? "ALL OK" : "SOME FAILED");
    return ok ? 0 : 1;
}
