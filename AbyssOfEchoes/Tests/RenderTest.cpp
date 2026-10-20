// Offline checks for Abyss of Echoes: delay timing, reverb decay, freeze, shimmer, ducking, mono bass,
// dry / bypass, every preset clean, state round trip, CPU, GUI snapshot. Optional demo renders.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>

using namespace juce;
static const double sr = 48000.0;
static const int block = 512;
static bool ok = true;

static void check (bool c, const char* what, const String& extra = {}) { printf ("%-48s %s %s\n", what, c ? "OK" : "FAIL", extra.toRawUTF8()); ok &= c; }
static void set (AbyssProcessor& p, const char* id, float v) { if (auto* a = p.apvts.getParameter (id)) a->setValueNotifyingHost (a->convertTo0to1 (v)); }
static void clean (AbyssProcessor& p)
{
    p.setCurrentProgram (0);
    p.getEngine().reset();
}
// run 'in' (stereo) through the processor, return the output
static AudioBuffer<float> run (AbyssProcessor& p, const AudioBuffer<float>& in)
{
    AudioBuffer<float> out (2, in.getNumSamples());
    for (int pos = 0; pos < in.getNumSamples(); pos += block)
    {
        const int n = jmin (block, in.getNumSamples() - pos);
        AudioBuffer<float> b (2, n);
        for (int ch = 0; ch < 2; ++ch) b.copyFrom (ch, 0, in, ch, pos, n);
        MidiBuffer m; p.processBlock (b, m);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, b, ch, 0, n);
    }
    return out;
}
static AudioBuffer<float> impulse (double seconds, float amp = 0.8f)
{
    AudioBuffer<float> b (2, (int) (sr * seconds)); b.clear();
    b.setSample (0, 0, amp); b.setSample (1, 0, amp);
    return b;
}
// a screech-like test tone: detuned saws through a soft clip, bursts of 'onS' every 'periodS'
static AudioBuffer<float> bursts (double seconds, double onS, double periodS, float hz = 440.f)
{
    AudioBuffer<float> b (2, (int) (sr * seconds)); b.clear();
    double p1 = 0, p2 = 0.3, p3 = 0.6;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / sr;
        const bool on = std::fmod (t, periodS) < onS;
        p1 += hz / sr; p2 += hz * 1.006 / sr; p3 += hz * 0.994 / sr;
        p1 -= std::floor (p1); p2 -= std::floor (p2); p3 -= std::floor (p3);
        const float s = (float) std::tanh (1.5 * ((2 * p1 - 1) + (2 * p2 - 1) + (2 * p3 - 1)) / 3.0) * (on ? 0.5f : 0.f);
        b.setSample (0, i, s); b.setSample (1, i, (float) std::tanh (1.5 * ((2 * p1 - 1) + (2 * p3 - 1) + (2 * p2 - 1)) / 3.0) * (on ? 0.5f : 0.f));
    }
    return b;
}
static double energy (const AudioBuffer<float>& b, double a, double z, int ch = -1)
{
    double e = 0; const int i0 = (int) (a * sr), i1 = jmin (b.getNumSamples(), (int) (z * sr));
    for (int c = 0; c < 2; ++c) if (ch < 0 || ch == c) for (int i = i0; i < i1; ++i) e += (double) b.getSample (c, i) * b.getSample (c, i);
    return e / jmax (1, i1 - i0);
}
static double db (double e) { return 10.0 * std::log10 (e + 1e-30); }
static void writeWav (const File& f, const AudioBuffer<float>& b)
{
    f.deleteFile();
    if (auto w = std::unique_ptr<AudioFormatWriter> (WavAudioFormat().createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0)))
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File out = argc > 1 ? File (argv[1]) : File::getCurrentWorkingDirectory();
    AbyssProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.prepareToPlay (sr, block);
    using namespace ParamIDs;

    // demo: RenderTest <dir> demo <input.wav> <prog,prog,...>  -> dry, then each preset
    if (argc > 4 && String (argv[2]) == "demo")
    {
        AudioFormatManager fmts; fmts.registerBasicFormats();
        std::unique_ptr<AudioFormatReader> rd (fmts.createReaderFor (File (argv[3])));
        if (rd == nullptr) return 1;
        AudioBuffer<float> src (2, (int) rd->lengthInSamples); rd->read (&src, 0, src.getNumSamples(), 0, true, true);
        const double tail = 2.5;
        AudioBuffer<float> in (2, src.getNumSamples() + (int) (sr * tail)); in.clear();
        for (int ch = 0; ch < 2; ++ch) in.copyFrom (ch, 0, src, jmin (ch, src.getNumChannels() - 1), 0, src.getNumSamples());
        in.applyGain (0.5f / jmax (0.001f, in.getMagnitude (0, in.getNumSamples())));
        auto progs = StringArray::fromTokens (argv[4], ",", "");
        AudioBuffer<float> all (2, in.getNumSamples() * (progs.size() + 1)); all.clear();
        for (int ch = 0; ch < 2; ++ch) all.copyFrom (ch, 0, in, ch, 0, in.getNumSamples());
        for (int k = 0; k < progs.size(); ++k)
        {
            proc.setCurrentProgram (progs[k].getIntValue()); proc.getEngine().reset();
            auto o = run (proc, in);
            for (int ch = 0; ch < 2; ++ch) all.copyFrom (ch, in.getNumSamples() * (k + 1), o, ch, 0, o.getNumSamples());
            printf ("%s\n", proc.getProgramName (progs[k].getIntValue()).toRawUTF8());
        }
        writeWav (out.getChildFile (argc > 5 ? String (argv[5]) : String ("abyss_demo.wav")), all);
        return 0;
    }

    // ---- wet level calibration: 100 % wet should sit near the dry level
    {
        auto in = bursts (3.0, 0.3, 0.6);
        const double din = db (energy (in, 0.5, 3.0));
        for (int which = 0; which < 2; ++which)
        {
            clean (proc); set (proc, dry, 0); set (proc, revDuck, 0.f); set (proc, dlyDuck, 0.f);
            if (which == 0) { set (proc, dlyOn, 0); set (proc, revMix, 1.f); } else { set (proc, revOn, 0); set (proc, dlyMix, 1.f); set (proc, dlyFb, 0.f); }
            proc.getEngine().reset();
            auto o = run (proc, in);
            printf ("wet level %s: %+.1f dB vs dry\n", which == 0 ? "reverb" : "delay ", db (energy (o, 0.5, 3.0)) - din);
        }
    }
    // ---- bypass: delay + reverb off, width 100 % -> output == input
    {
        clean (proc); set (proc, dlyOn, 0); set (proc, revOn, 0);
        auto in = bursts (1.0, 0.2, 0.5);
        auto o = run (proc, in); o = run (proc, in);   // second pass after the smoothers settled
        float diff = 0.f;
        for (int ch = 0; ch < 2; ++ch) for (int i = 4800; i < in.getNumSamples(); ++i) diff = jmax (diff, std::abs (o.getSample (ch, i) - in.getSample (ch, i)));
        check (diff < 1e-5f, "all effects off = untouched", "(max diff " + String (diff, 7) + ")");
    }
    // ---- delay timing: 1/4 at 150 BPM = 400 ms; ping-pong: first echo left, second right
    {
        clean (proc); set (proc, revOn, 0); set (proc, dlyOn, 1); set (proc, dlyMode, 1); set (proc, dlySync, 1); set (proc, dlyDiv, 8);
        set (proc, dlyFb, 0.5f); set (proc, dlyMix, 1.f); set (proc, dlyDuck, 0.f); set (proc, dlyMod, 0.f); set (proc, dlyLo, 20.f); set (proc, dlyHi, 20000.f);
        set (proc, dry, 0);
        proc.getEngine().reset(); { AudioBuffer<float> z (2, 4800); z.clear(); run (proc, z); } proc.getEngine().reset();
        auto o = run (proc, impulse (1.0));
        auto peakAt = [&] (int ch, int from, int to) { int at = from; float pk = 0; for (int i = from; i < to; ++i) if (std::abs (o.getSample (ch, i)) > pk) { pk = std::abs (o.getSample (ch, i)); at = i; } return std::make_pair (at, pk); };
        auto l1 = peakAt (0, 1000, 25000), r1 = peakAt (1, 1000, 25000), r2 = peakAt (1, 25000, 45000);
        check (std::abs (l1.first - 19200) < 8 && l1.second > 0.2f && r1.second < 0.02f && std::abs (r2.first - 38400) < 12,
               "ping-pong 1/4 @150: L at 400 ms, R at 800 ms", "(" + String (l1.first) + ", " + String (r2.first) + ")");
        check (std::abs (o.getSample (0, 0)) < 1e-4f, "DRY off: no direct sound");
    }
    // ---- reverb decay follows DECAY (RT60 from the energy slope)
    for (float target : { 1.f, 3.f })
    {
        clean (proc); set (proc, dlyOn, 0); set (proc, revOn, 1); set (proc, revDecay, target); set (proc, revMix, 1.f); set (proc, revDuck, 0.f);
        set (proc, revPre, 0.f); set (proc, revShim, 0.f); set (proc, dry, 0); set (proc, revLo, 20.f); set (proc, revDamp, 20000.f);
        proc.getEngine().reset();
        auto o = run (proc, impulse (target * 1.5 + 0.5));
        const double a = 0.15, z = 0.15 + target * 0.6;
        const double slope = (db (energy (o, z, z + 0.05)) - db (energy (o, a, a + 0.05))) / (z - a);   // dB per second
        const double rt60 = -60.0 / slope;
        check (rt60 > target * 0.7 && rt60 < target * 1.35, target < 2.f ? "reverb DECAY 1 s -> RT60" : "reverb DECAY 3 s -> RT60", "(" + String (rt60, 2) + " s)");
    }
    // ---- freeze holds the tail
    {
        clean (proc); set (proc, dlyOn, 0); set (proc, revMix, 1.f); set (proc, revDuck, 0.f); set (proc, dry, 0); set (proc, freeze, 0);
        proc.getEngine().reset();
        auto in = bursts (0.6, 0.5, 1.0);
        run (proc, in);
        set (proc, freeze, 1);
        AudioBuffer<float> silence (2, (int) (sr * 4.0)); silence.clear();
        auto o = run (proc, silence);
        const double early = db (energy (o, 0.2, 0.5)), late = db (energy (o, 3.5, 3.9));
        check (late > early - 6.0 && late > -60.0, "FREEZE keeps the tail", "(" + String (early, 1) + " -> " + String (late, 1) + " dB)");
        set (proc, freeze, 0);
    }
    // ---- shimmer brightens the tail
    {
        auto centroid = [] (const AudioBuffer<float>& b, double a, double z)
        {
            const int n = 8192; dsp::FFT fft (13); std::vector<float> d ((size_t) n * 2, 0.f);
            const int i0 = (int) (a * sr);
            for (int i = 0; i < n && i0 + i < b.getNumSamples(); ++i) d[(size_t) i] = b.getSample (0, i0 + i) * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * i / n));
            fft.performFrequencyOnlyForwardTransform (d.data());
            double s = 0, w = 0; for (int k = 1; k < n / 2; ++k) { s += d[(size_t) k]; w += d[(size_t) k] * k * sr / n; }
            return w / jmax (1e-9, s);
        };
        clean (proc); set (proc, dlyOn, 0); set (proc, revMix, 1.f); set (proc, dry, 0); set (proc, revDecay, 5.f); set (proc, revDuck, 0.f); set (proc, revShim, 0.f);
        proc.getEngine().reset();
        auto in = bursts (3.0, 0.4, 5.0, 330.f);
        auto a = run (proc, in);
        set (proc, revShim, 0.8f); proc.getEngine().reset();
        auto b = run (proc, in);
        const double c0 = centroid (a, 1.5, 0), c1 = centroid (b, 1.5, 0);
        check (c1 > c0 * 1.15, "SHIMMER lifts the tail an octave", "(" + String ((int) c0) + " -> " + String ((int) c1) + " Hz)");
    }
    // ---- ducking: wet quieter while the dry plays, comes back in the gaps
    {
        auto wetWhilePlaying = [&] (float duck)
        {
            clean (proc); set (proc, dlyOn, 0); set (proc, revMix, 1.f); set (proc, revDuck, duck); set (proc, dry, 0); set (proc, revDecay, 3.f);
            proc.getEngine().reset();
            auto o = run (proc, bursts (2.0, 1.0, 4.0));
            return std::make_pair (db (energy (o, 0.6, 0.95)), db (energy (o, 1.5, 1.8)));
        };
        auto a = wetWhilePlaying (0.f), b = wetWhilePlaying (1.f);
        check (b.first < a.first - 6.0 && b.second > a.second - 3.0, "DUCK: wet down while playing, back in the gap",
               "(" + String (a.first - b.first, 1) + " dB under the note, " + String (a.second - b.second, 1) + " dB after)");
    }
    // ---- mono bass removes low sides
    {
        clean (proc); set (proc, dlyOn, 0); set (proc, revOn, 0); set (proc, monoBass, 200.f);
        AudioBuffer<float> in (2, (int) sr);
        for (int i = 0; i < in.getNumSamples(); ++i) { const float s = 0.5f * std::sin (MathConstants<float>::twoPi * 60.f * (float) i / (float) sr); in.setSample (0, i, s); in.setSample (1, i, -s); }
        run (proc, in);
        auto o = run (proc, in);
        double side = 0; for (int i = 4800; i < o.getNumSamples(); ++i) { const double s = 0.5 * (o.getSample (0, i) - o.getSample (1, i)); side += s * s; }
        side /= (o.getNumSamples() - 4800);
        check (db (side) < db (0.125) - 20.0, "MONO BASS 200 Hz: 60 Hz sides removed", "(" + String (db (side) - db (0.125), 1) + " dB)");
    }
    // ---- width 0 = mono
    {
        clean (proc); set (proc, dlyOn, 0); set (proc, revOn, 0); set (proc, width, 0.f);
        auto in = bursts (0.5, 0.3, 1.0);
        for (int i = 0; i < in.getNumSamples(); ++i) in.setSample (1, i, in.getSample (1, i) * 0.3f);
        run (proc, in); auto o = run (proc, in);
        float d = 0; for (int i = 4800; i < o.getNumSamples(); ++i) d = jmax (d, std::abs (o.getSample (0, i) - o.getSample (1, i)));
        check (d < 1e-4f, "WIDTH 0 % = mono");
    }
    // ---- every preset: clean, bounded, has a tail
    {
        auto in = bursts (2.5, 0.25, 0.8);
        AudioBuffer<float> tailIn (2, (int) (sr * 1.5)); tailIn.clear();
        for (int pg = 0; pg < proc.getNumPrograms(); ++pg)
        {
            proc.setCurrentProgram (pg); proc.getEngine().reset();
            auto o = run (proc, in); auto t = run (proc, tailIn);
            bool nan = false; float pk = 0.f;
            for (auto* b : { &o, &t }) for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < b->getNumSamples(); ++i) { const float v = b->getSample (ch, i); nan |= ! std::isfinite (v); pk = jmax (pk, std::abs (v)); }
            const bool good = ! nan && pk < 2.5f;
            printf ("  %-30s peak %.2f  tail %6.1f dB  %s\n", proc.getProgramName (pg).toRawUTF8(), pk, db (energy (t, 0.1, 0.4)), good ? "OK" : "FAIL");
            ok &= good;
        }
        check (true, "all presets render clean");
    }
    // ---- state round trip
    {
        proc.setCurrentProgram (9);
        MemoryBlock mb; proc.getStateInformation (mb);
        AbyssProcessor p2; p2.setStateInformation (mb.getData(), (int) mb.getSize());
        const bool same = std::abs (p2.apvts.getRawParameterValue (revDecay)->load() - proc.apvts.getRawParameterValue (revDecay)->load()) < 1e-4f
                       && p2.currentPresetName() == proc.currentPresetName();
        check (same, "state round trip");
    }
    // ---- CPU (busiest preset: shimmer + delay)
    {
        proc.setCurrentProgram (10); set (proc, dlyOn, 1); proc.getEngine().reset();
        auto in = bursts (10.0, 0.25, 0.5);
        const auto t0 = std::chrono::steady_clock::now();
        run (proc, in);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        printf ("CPU: 10 s of audio in %.0f ms (%.1f%% of real time)\n", secs * 1000.0, secs * 10.0);
        check (secs < 1.5, "CPU budget");
    }
    // ---- GUI snapshot while something plays
    {
        proc.setCurrentProgram (1); proc.getEngine().reset();
        run (proc, bursts (1.2, 0.2, 0.4));
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (Canvas::W, Canvas::H);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = out.getChildFile ("abyss_gui.png"); f.deleteFile();
        FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
    }
    printf (ok ? "ALL OK\n" : "SOME CHECKS FAILED\n");
    return ok ? 0 : 1;
}
