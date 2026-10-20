// Offline checks for Throne of Titans: flat crossover, limiter ceiling (true peak), LUFS accuracy, latency,
// upward / downward compression, band width, MATCH, presets on a hardstyle test mix, CPU, GUI snapshot.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <chrono>

using namespace juce;
static const double sr = 48000.0;
static const int block = 512;
static bool ok = true;
static void check (bool c, const char* what, const String& extra = {}) { printf ("%-50s %s %s\n", what, c ? "OK" : "FAIL", extra.toRawUTF8()); ok &= c; }
static void set (TitanProcessor& p, const String& id, float v) { if (auto* a = p.apvts.getParameter (id)) a->setValueNotifyingHost (a->convertTo0to1 (v)); }

static void neutral (TitanProcessor& p)
{
    using namespace ParamIDs;
    p.setCurrentProgram (0);
    for (int b = 0; b < 3; ++b)
    {
        set (p, band (b, ratio), 1.f); set (p, band (b, up), 1.f); set (p, band (b, sat), 0.f); set (p, band (b, width), 1.f);
        set (p, band (b, pre), 0.f); set (p, band (b, post), 0.f); set (p, band (b, solo), 0.f); set (p, band (b, on), 1.f);
    }
    set (p, limit, 0.f); set (p, clip, 0.f); set (p, ceiling, 0.f); set (p, tilt, 0.f); set (p, monoBass, 0.f); set (p, input, 0.f); set (p, match, 0.f);
    p.getEngine().reset();
}
static AudioBuffer<float> run (TitanProcessor& p, const AudioBuffer<float>& in)
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
// hardstyle-ish test mix: kick every beat (150 BPM), detuned saw riff, hats; premaster level
static AudioBuffer<float> testMix (double seconds)
{
    AudioBuffer<float> b (2, (int) (sr * seconds)); b.clear();
    Random rng (5);
    const int beat = (int) (sr * 60.0 / 150.0);
    const int notes[8] = { 62, 65, 69, 67, 74, 72, 69, 65 };
    double ph[3] = { 0, 0.3, 0.6 }, kph = 0;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const int inBeat = i % beat; const double t = inBeat / sr;
        // kick: pitch drop 300 -> 55 Hz, saturated, tail
        const double f = 55.0 + 245.0 * std::exp (-t * 40.0);
        kph += f / sr;
        const double kick = std::tanh (3.0 * std::sin (2.0 * MathConstants<double>::pi * kph)) * std::exp (-t * 3.5) * 0.55;
        // riff: eighth notes, three detuned saws, stereo
        const int eighth = (i / (beat / 2)) % 8;
        const double hz = 440.0 * std::pow (2.0, (notes[eighth] - 69) / 12.0);
        double sL = 0, sR = 0;
        for (int k = 0; k < 3; ++k)
        {
            ph[k] += hz * (1.0 + 0.007 * (k - 1)) / sr; ph[k] -= std::floor (ph[k]);
            const double s = 2.0 * ph[k] - 1.0;
            sL += s * (k == 2 ? 0.4 : 1.0); sR += s * (k == 0 ? 0.4 : 1.0);
        }
        const double gate = (i % (beat / 2)) < (int) (beat * 0.42) ? 1.0 : 0.0;
        const double lead = 0.12 * gate;
        // hats on the off-beat
        const double ht = std::fmod ((double) i - beat / 2, (double) beat) / sr;
        const double hat = (ht >= 0 && ht < 0.04) ? (rng.nextFloat() * 2 - 1) * std::exp (-ht * 90.0) * 0.12 : 0.0;
        b.setSample (0, i, (float) (kick + lead * std::tanh (sL) + hat));
        b.setSample (1, i, (float) (kick + lead * std::tanh (sR) + hat * 0.8));
    }
    return b;
}
static double lufsOf (const AudioBuffer<float>& b, double from = 0.0)
{
    doomsday::dyn::Loudness L; L.prepare (sr);
    for (int i = (int) (from * sr); i < b.getNumSamples(); ++i) L.push (b.getSample (0, i), b.getSample (1, i));
    return L.integratedLufs();
}
static double truePeakDb (const AudioBuffer<float>& b)
{
    dsp::Oversampling<float> os (2, 3, dsp::Oversampling<float>::filterHalfBandFIREquiripple, true);
    os.initProcessing ((size_t) b.getNumSamples());
    AudioBuffer<float> c (b);
    dsp::AudioBlock<float> blk (c);
    auto up = os.processSamplesUp (blk);
    float pk = 0.f;
    for (size_t ch = 0; ch < up.getNumChannels(); ++ch) for (size_t i = 0; i < up.getNumSamples(); ++i) pk = jmax (pk, std::abs (up.getSample ((int) ch, (int) i)));
    return 20.0 * std::log10 (pk + 1e-12);
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File out = argc > 1 ? File (argv[1]) : File::getCurrentWorkingDirectory();
    TitanProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.prepareToPlay (sr, block);
    using namespace ParamIDs;
    const int lat = proc.getLatencySamples();

    // demo: RenderTest <dir> demo <prog,prog,...>  -> test mix dry, then each preset
    if (argc > 3 && String (argv[2]) == "demo")
    {
        auto mix = testMix (6.4);
        auto progs = StringArray::fromTokens (argv[3], ",", "");
        AudioBuffer<float> all (2, mix.getNumSamples() * (progs.size() + 1)); all.clear();
        for (int ch = 0; ch < 2; ++ch) all.copyFrom (ch, 0, mix, ch, 0, mix.getNumSamples());
        for (int k = 0; k < progs.size(); ++k)
        {
            proc.setCurrentProgram (progs[k].getIntValue()); proc.getEngine().reset();
            run (proc, mix);                                   // settle
            auto o = run (proc, mix);
            for (int ch = 0; ch < 2; ++ch) all.copyFrom (ch, mix.getNumSamples() * (k + 1), o, ch, 0, o.getNumSamples());
            printf ("%-30s %.1f LUFS  TP %.2f dB\n", proc.getProgramName (progs[k].getIntValue()).toRawUTF8(), lufsOf (o, 0.5), truePeakDb (o));
        }
        auto f = out.getChildFile ("titans_demo.wav"); f.deleteFile();
        if (auto w = std::unique_ptr<AudioFormatWriter> (WavAudioFormat().createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0)))
            w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
        return 0;
    }

    // ---- latency: an impulse comes out exactly 'latency' samples later
    {
        neutral (proc);
        for (int b = 0; b < 3; ++b) set (proc, band (b, on), 0.f);
        AudioBuffer<float> in (2, 4096); in.clear(); in.setSample (0, 100, 0.5f); in.setSample (1, 100, 0.5f);
        { AudioBuffer<float> z (2, 4096); z.clear(); run (proc, z); }
        auto o = run (proc, in);
        int at = 0; float pk = 0; for (int i = 0; i < o.getNumSamples(); ++i) if (std::abs (o.getSample (0, i)) > pk) { pk = std::abs (o.getSample (0, i)); at = i; }
        check (std::abs ((at - 100) - lat) <= 3, "reported latency matches", "(" + String (lat) + " reported, " + String (at - 100) + " measured)");
    }
    // ---- crossover sums flat (magnitude) with neutral bands
    {
        neutral (proc);
        AudioBuffer<float> in (2, 16384); in.clear(); in.setSample (0, 64, 0.25f); in.setSample (1, 64, 0.25f);
        { AudioBuffer<float> z (2, 8192); z.clear(); run (proc, z); }
        auto o = run (proc, in);
        dsp::FFT fft (14); std::vector<float> d (32768, 0.f);
        for (int i = 0; i < 16384; ++i) d[(size_t) i] = o.getSample (0, i) / 0.25f;
        fft.performFrequencyOnlyForwardTransform (d.data());
        float lo = 100.f, hi = -100.f;
        for (int k = (int) (30.0 * 16384 / sr); k < (int) (18000.0 * 16384 / sr); ++k) { const float dbv = 20.f * std::log10 (d[(size_t) k] + 1e-9f); lo = jmin (lo, dbv); hi = jmax (hi, dbv); }
        check (hi - lo < 0.5f && std::abs (hi) < 0.5f, "3-band split sums flat (30 Hz - 18 kHz)", "(" + String (lo, 2) + " .. " + String (hi, 2) + " dB)");
    }
    // ---- LUFS: 1 kHz sine at -20 dBFS on both channels = -20 LUFS
    {
        AudioBuffer<float> s (2, (int) (sr * 6)); const float a = std::pow (10.f, -20.f / 20.f);
        for (int i = 0; i < s.getNumSamples(); ++i) { const float v = a * std::sin (MathConstants<float>::twoPi * 1000.f * (float) i / (float) sr); s.setSample (0, i, v); s.setSample (1, i, v); }
        const double l = lufsOf (s);
        check (std::abs (l + 20.0) < 0.2, "LUFS meter: 1 kHz at -20 dBFS -> -20 LUFS", "(" + String (l, 2) + ")");
    }
    // ---- limiter: pushed hard, true peak stays under the ceiling
    {
        neutral (proc); set (proc, limit, 14.f); set (proc, ceiling, -1.f);
        auto mix = testMix (4.0);
        run (proc, mix);
        auto o = run (proc, mix);
        const double tp = truePeakDb (o), l = lufsOf (o, 0.5);
        check (tp < -1.f + 0.05, "limiter +14 dB, ceiling -1 dBTP: true peak held", "(TP " + String (tp, 2) + " dB, " + String (l, 1) + " LUFS)");
    }
    // ---- downward compression lowers loud, upward lifts quiet
    {
        auto toneAt = [] (float db)
        {
            AudioBuffer<float> s (2, (int) (sr * 1.5)); const float a = std::pow (10.f, db / 20.f);
            for (int i = 0; i < s.getNumSamples(); ++i) { const float v = a * std::sin (MathConstants<float>::twoPi * 800.f * (float) i / (float) sr); s.setSample (0, i, v); s.setSample (1, i, v); }
            return s;
        };
        auto gainFor = [&] (float db, float ratio_, float up_)
        {
            neutral (proc); set (proc, band (1, thresh), -20.f); set (proc, band (1, ratio), ratio_); set (proc, band (1, up), up_); set (proc, band (1, knee), 0.f);
            auto in = toneAt (db); auto o = run (proc, in);
            double ei = 0, eo = 0; for (int i = (int) sr; i < in.getNumSamples() - lat; ++i) { ei += in.getSample (0, i) * in.getSample (0, i); eo += o.getSample (0, i + lat) * o.getSample (0, i + lat); }
            return 10.0 * std::log10 (eo / ei);
        };
        const double down = gainFor (-8.f, 4.f, 1.f), upq = gainFor (-36.f, 1.f, 2.f);
        check (down < -7.f && down > -11.f, "RATIO 4:1, 12 dB over -> about -9 dB", "(" + String (down, 1) + " dB)");
        check (upq > 6.f && upq < 10.f, "UPWARD 2:1, 16 dB under -> about +8 dB", "(" + String (upq, 1) + " dB)");
    }
    // ---- LOW band width 0 -> low end mono
    {
        neutral (proc); set (proc, band (0, width), 0.f);
        AudioBuffer<float> in (2, (int) sr);
        for (int i = 0; i < in.getNumSamples(); ++i) { const float v = 0.3f * std::sin (MathConstants<float>::twoPi * 60.f * (float) i / (float) sr); in.setSample (0, i, v); in.setSample (1, i, -v); }
        run (proc, in); auto o = run (proc, in);
        double side = 0; for (int i = 4800; i < o.getNumSamples(); ++i) { const double s = 0.5 * (o.getSample (0, i) - o.getSample (1, i)); side += s * s; }
        const double rel = 10.0 * std::log10 (side / (o.getNumSamples() - 4800) / 0.045);
        check (rel < -20.0, "LOW width 0 % -> 60 Hz sides gone", "(" + String (rel, 1) + " dB)");
    }
    // ---- MATCH: output follows the input loudness
    {
        neutral (proc); set (proc, limit, 10.f); set (proc, match, 1.f);
        auto mix = testMix (12.0);
        auto o = run (proc, mix);
        AudioBuffer<float> tailIn (2, (int) (sr * 6)), tailOut (2, (int) (sr * 6));
        for (int ch = 0; ch < 2; ++ch) { tailIn.copyFrom (ch, 0, mix, ch, (int) (sr * 6), (int) (sr * 6) - lat); tailOut.copyFrom (ch, 0, o, ch, (int) (sr * 6) + lat, (int) (sr * 6) - lat); }
        const double a = lufsOf (tailIn), b = lufsOf (tailOut);
        check (b <= a + 0.3 && b > a - 1.0, "MATCH: processed output at the input loudness", "(in " + String (a, 1) + ", out " + String (b, 1) + " LUFS)");
        set (proc, match, 0.f);
    }
    // ---- presets on the test mix: clean, under the ceiling, loudness
    {
        auto mix = testMix (6.4);
        printf ("  test mix: %.1f LUFS, TP %.1f dB\n", lufsOf (mix, 0.5), truePeakDb (mix));
        for (int pg = 0; pg < proc.getNumPrograms(); ++pg)
        {
            proc.setCurrentProgram (pg); proc.getEngine().reset();
            run (proc, mix); auto o = run (proc, mix);
            bool nan = false; for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < o.getNumSamples(); ++i) nan |= ! std::isfinite (o.getSample (ch, i));
            const double tp = truePeakDb (o), l = lufsOf (o, 0.5);
            const double ceil = proc.apvts.getRawParameterValue (ceiling)->load();
            const bool good = ! nan && tp < ceil + 0.05;
            printf ("  %-30s %6.1f LUFS  TP %5.2f  %s\n", proc.getProgramName (pg).toRawUTF8(), l, tp, good ? "OK" : "FAIL");
            ok &= good;
        }
        check (true, "all presets: clean + true peak under the ceiling");
    }
    // ---- state round trip
    {
        proc.setCurrentProgram (2);
        MemoryBlock mb; proc.getStateInformation (mb);
        TitanProcessor p2; p2.setStateInformation (mb.getData(), (int) mb.getSize());
        check (std::abs (p2.apvts.getRawParameterValue (limit)->load() - proc.apvts.getRawParameterValue (limit)->load()) < 1e-4f
               && p2.currentPresetName() == proc.currentPresetName(), "state round trip");
    }
    // ---- CPU
    {
        proc.setCurrentProgram (1); proc.getEngine().reset();
        auto mix = testMix (10.0);
        const auto t0 = std::chrono::steady_clock::now();
        run (proc, mix);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        printf ("CPU: 10 s of audio in %.0f ms (%.1f%% of real time)\n", secs * 1000.0, secs * 10.0);
        check (secs < 2.0, "CPU budget");
    }
    // ---- GUI snapshot
    {
        proc.setCurrentProgram (1); proc.getEngine().reset(); proc.getEngine().resetMeters();
        run (proc, testMix (3.0));
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (Canvas::W, Canvas::H);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = out.getChildFile ("titans_gui.png"); f.deleteFile();
        FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
    }
    printf (ok ? "ALL OK\n" : "SOME CHECKS FAILED\n");
    return ok ? 0 : 1;
}
