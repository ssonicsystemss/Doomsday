// Offline test for Mirror of Souls: flat = transparent, bell / cut accuracy, AMOUNT, MID/SIDE, DYNAMIC, MONO BASS,
// display curve == audio, presets, state, GUI snapshot.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

using namespace juce;
static const double sr = 48000.0;
static const int block = 512;
static bool ok = true;
static void check (bool c, const char* what, const String& extra = {}) { printf ("%-48s %s %s\n", what, c ? "OK" : "FAIL", extra.toRawUTF8()); ok &= c; }
static void set (MirrorProcessor& p, const String& id, float v) { if (auto* r = p.apvts.getParameter (id)) r->setValueNotifyingHost (r->convertTo0to1 (v)); }
static void flat (MirrorProcessor& p) { p.setCurrentProgram (0); }

// run a stereo signal through (gen(i, ch)), returns output; first 0.2 s discarded by callers
template <typename Gen>
static AudioBuffer<float> run (MirrorProcessor& p, int len, Gen gen)
{
    AudioBuffer<float> out (2, len);
    MidiBuffer none;
    for (int pos = 0; pos < len; pos += block)
    {
        const int n = jmin (block, len - pos);
        AudioBuffer<float> b (2, n);
        for (int i = 0; i < n; ++i) for (int ch = 0; ch < 2; ++ch) b.setSample (ch, i, gen (pos + i, ch));
        p.processBlock (b, none);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, b, ch, 0, n);
    }
    return out;
}
static double rmsDb (const AudioBuffer<float>& b, int ch, int from) { return Decibels::gainToDecibels ((double) b.getRMSLevel (ch, from, b.getNumSamples() - from), -200.0); }
static double sineGainDb (MirrorProcessor& p, double hz, float amp = 0.25f)
{
    auto o = run (p, (int) sr, [&] (int i, int) { return amp * (float) std::sin (MathConstants<double>::twoPi * hz * i / sr); });
    return rmsDb (o, 0, (int) (0.3 * sr)) - Decibels::gainToDecibels (amp / std::sqrt (2.0));
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File out = argc > 1 ? File (argv[1]) : File::getCurrentWorkingDirectory();
    using namespace ParamIDs;
    MirrorProcessor p;
    p.setPlayConfigDetails (2, 2, sr, block);
    p.prepareToPlay (sr, block);

    // ---- flat = transparent
    {
        flat (p);
        Random r (1);
        std::vector<float> in ((size_t) sr);
        for (auto& x : in) x = r.nextFloat() * 0.5f - 0.25f;
        auto o = run (p, (int) sr, [&] (int i, int) { return in[(size_t) i]; });
        float err = 0.f; for (int i = 0; i < (int) sr; ++i) err = jmax (err, std::abs (o.getSample (0, i) - in[(size_t) i]));
        check (err < 1e-5f, "flat preset is transparent", "(max err " + String (err, 7) + ")");
    }
    // ---- bell +6 dB at 1 kHz, and the display curve says the same
    {
        flat (p);
        set (p, band (2, on), 1); set (p, band (2, type), Choices::Bell); set (p, band (2, freq), 1000); set (p, band (2, gain), 6); set (p, band (2, q), 1.f);
        const double at1k = sineGainDb (p, 1000.0), at100 = sineGainDb (p, 100.0);
        const auto ep = p.gather();
        const auto d = MirrorEngine::design (ep.band[2], sr, 6.f, 1.f);
        const double disp = Decibels::gainToDecibels (d.magnitude (1000.0, sr)), disp100 = Decibels::gainToDecibels (d.magnitude (100.0, sr));
        check (std::abs (at1k - 6.0) < 0.2 && std::abs (at100) < 0.5, "bell +6 dB @ 1 kHz", "(" + String (at1k, 2) + " dB, 100 Hz " + String (at100, 2) + ")");
        check (std::abs (disp - at1k) < 0.2 && std::abs (disp100 - at100) < 0.2, "display curve matches the audio");
        // AMOUNT 0 = flat, 150 % = 9 dB
        set (p, amount, 0.f);   const double a0 = sineGainDb (p, 1000.0);
        set (p, amount, 1.5f);  const double a15 = sineGainDb (p, 1000.0);
        set (p, amount, 1.f);
        check (std::abs (a0) < 0.2 && std::abs (a15 - 9.0) < 0.3, "AMOUNT scales the curve", "(0 %: " + String (a0, 2) + ", 150 %: " + String (a15, 2) + " dB)");
    }
    // ---- low cut 24 dB @ 100 Hz
    {
        flat (p);
        set (p, band (0, on), 1); set (p, band (0, type), Choices::LowCut); set (p, band (0, freq), 100); set (p, band (0, slope), 1);
        const double lo = sineGainDb (p, 30.0), mid = sineGainDb (p, 1000.0), at = sineGainDb (p, 100.0);
        check (lo < -20.0 && std::abs (mid) < 0.2 && std::abs (at + 3.0) < 0.5, "low cut 24 dB @ 100 Hz", "(30 Hz " + String (lo, 1) + ", 100 Hz " + String (at, 1) + ", 1 kHz " + String (mid, 2) + ")");
        set (p, band (0, slope), 2);
        const double lo48 = sineGainDb (p, 30.0);
        check (lo48 < lo - 15.0, "48 dB slope cuts harder", "(" + String (lo48, 1) + " dB)");
    }
    // ---- MID band leaves the sides alone
    {
        flat (p);
        set (p, band (3, on), 1); set (p, band (3, type), Choices::Bell); set (p, band (3, freq), 1000); set (p, band (3, gain), 12); set (p, band (3, mode), 1);
        auto o = run (p, (int) sr, [&] (int i, int ch) { const float s = 0.25f * (float) std::sin (MathConstants<double>::twoPi * 1000.0 * i / sr); return ch == 0 ? s : -s; });
        const double g = rmsDb (o, 0, (int) (0.3 * sr)) - Decibels::gainToDecibels (0.25 / std::sqrt (2.0));
        set (p, band (3, mode), 2);
        auto o2 = run (p, (int) sr, [&] (int i, int ch) { const float s = 0.25f * (float) std::sin (MathConstants<double>::twoPi * 1000.0 * i / sr); return ch == 0 ? s : -s; });
        const double g2 = rmsDb (o2, 0, (int) (0.3 * sr)) - Decibels::gainToDecibels (0.25 / std::sqrt (2.0));
        check (std::abs (g) < 0.2 && std::abs (g2 - 12.0) < 0.3, "MID band ignores side, SIDE band boosts it", "(" + String (g, 2) + " / " + String (g2, 2) + " dB)");
    }
    // ---- DYNAMIC: quiet passes, loud gets cut
    {
        flat (p);
        set (p, band (4, on), 1); set (p, band (4, type), Choices::Bell); set (p, band (4, freq), 3500); set (p, band (4, gain), -9);
        set (p, band (4, q), 2.f); set (p, band (4, dyn), 1); set (p, band (4, thresh), -30);
        const double quiet = sineGainDb (p, 3500.0, 0.003f), loud = sineGainDb (p, 3500.0, 0.5f);
        check (quiet > -1.0 && loud < -7.0, "DYNAMIC band: quiet untouched, loud cut", "(" + String (quiet, 2) + " / " + String (loud, 2) + " dB)");
    }
    // ---- MONO BASS
    {
        flat (p); set (p, monoBass, 200);
        auto o = run (p, (int) sr, [&] (int i, int ch) { const float s = 0.25f * (float) std::sin (MathConstants<double>::twoPi * 60.0 * i / sr); return ch == 0 ? s : -s; });
        check (rmsDb (o, 0, (int) (0.3 * sr)) < -40.0, "MONO BASS removes low side", String (rmsDb (o, 0, (int) (0.3 * sr)), 1) + " dB");
    }
    // ---- every preset on pink-ish noise
    {
        Random r (5);
        float b0 = 0, b1 = 0, b2 = 0;
        std::vector<float> pink ((size_t) sr * 2);
        for (auto& x : pink) { const float w = r.nextFloat() * 2.f - 1.f; b0 = 0.997f * b0 + w * 0.029591f; b1 = 0.985f * b1 + w * 0.032534f; b2 = 0.95f * b2 + w * 0.048056f; x = (b0 + b1 + b2 + w * 0.05f) * 0.6f; }
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            auto o = run (p, (int) sr, [&] (int k, int) { return pink[(size_t) k]; });
            bool bad = false; for (int k = 0; k < o.getNumSamples(); ++k) if (! std::isfinite (o.getSample (0, k))) bad = true;
            if (bad || o.getMagnitude (0, 0, o.getNumSamples()) > 2.f) { printf ("preset %s FAIL\n", p.getProgramName (i).toRawUTF8()); ok = false; }
        }
        check (true, ("presets x" + String (p.getNumPrograms()) + " render clean").toRawUTF8());

        // ---- state
        p.setCurrentProgram (5); set (p, band (5, gain), -4.5f);
        MemoryBlock st; p.getStateInformation (st);
        MirrorProcessor p2; p2.setStateInformation (st.getData(), (int) st.getSize());
        check (std::abs (p2.apvts.getRawParameterValue (band (5, gain))->load() + 4.5f) < 0.01f && p2.currentPresetName() == p.getProgramName (5), "state round trip");

        // ---- GUI with music-like signal through "KICK - Punch Focus"
        p.setCurrentProgram (1);
        set (p, band (4, on), 1); set (p, band (4, type), Choices::Bell); set (p, band (4, freq), 6000); set (p, band (4, gain), -4); set (p, band (4, q), 2.5f);
        set (p, band (4, dyn), 1); set (p, band (4, thresh), -40);
        run (p, (int) sr, [&] (int k, int) { return pink[(size_t) k] + 0.4f * (float) std::sin (MathConstants<double>::twoPi * 55.0 * k / sr) * (float) std::exp (-(k % 24000) / 6000.0); });
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (Canvas::W, Canvas::H);
        for (int t = 0; t < 4; ++t)       // let the analyser fill up
        {
            run (p, 2048, [&] (int k, int) { return pink[(size_t) k + 20000] + 0.4f * (float) std::sin (MathConstants<double>::twoPi * 55.0 * k / sr); });
            ed->getChildComponent (0)->getChildComponent (0)->repaint();
        }
        if (auto* c = dynamic_cast<Canvas*> (ed->getChildComponent (0))) juce::ignoreUnused (c);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = out.getChildFile ("mirror_gui.png"); f.deleteFile();
        FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
    }
    printf (ok ? "ALL OK\n" : "SOME CHECKS FAILED\n");
    return ok ? 0 : 1;
}
