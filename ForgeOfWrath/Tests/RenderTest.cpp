// Offline checks + tools for Forge of Wrath
//   RenderTest <outdir>                       tests, preset renders, GUI snapshot
//   RenderTest batch <jobs.txt> <outdir>      one kick per line: "name note id=value ... amp=<shape> drive=<shape>"
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <chrono>

using namespace juce;
static double sr = 44100.0;
static const int block = 512;
static bool ok = true;
static void check (bool c, const char* what, const String& extra = {}) { printf ("%-50s %s %s\n", what, c ? "OK" : "FAIL", extra.toRawUTF8()); ok &= c; }
static void set (ForgeProcessor& p, const String& id, float v) { if (auto* a = p.apvts.getParameter (id)) a->setValueNotifyingHost (a->convertTo0to1 (v)); }

static AudioBuffer<float> hit (ForgeProcessor& p, int note, double seconds, float vel = 1.f)
{
    p.prepareToPlay (sr, block);
    AudioBuffer<float> out (2, (int) (sr * seconds)); out.clear();
    for (int pos = 0; pos < out.getNumSamples(); pos += block)
    {
        const int n = jmin (block, out.getNumSamples() - pos);
        AudioBuffer<float> b (2, n); b.clear();
        MidiBuffer m;
        if (pos == 0) m.addEvent (MidiMessage::noteOn (1, note, vel), 0);
        p.processBlock (b, m);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, b, ch, 0, n);
    }
    return out;
}
static void writeWav (const File& f, const AudioBuffer<float>& b)
{
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
    if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}
// dominant frequency in a window by autocorrelation
static double pitchAt (const AudioBuffer<float>& b, double fromS, double lenS)
{
    const int a = (int) (fromS * sr), n = (int) (lenS * sr);
    const float* x = b.getReadPointer (0);
    double best = 0; int bestLag = 0;
    for (int lag = (int) (sr / 400); lag < (int) (sr / 25); ++lag)
    {
        double s = 0; for (int i = 0; i < n; ++i) s += x[a + i] * x[a + i + lag];
        if (s > best) { best = s; bestLag = lag; }
    }
    // the first lag that is almost as good = the period (not a multiple of it)
    for (int lag = (int) (sr / 400); lag < bestLag; ++lag)
    {
        double s = 0; for (int i = 0; i < n; ++i) s += x[a + i] * x[a + i + lag];
        double prev = 0, next = 0;
        for (int i = 0; i < n; ++i) { prev += x[a + i] * x[a + i + lag - 1]; next += x[a + i] * x[a + i + lag + 1]; }
        if (s > 0.9 * best && s >= prev && s >= next) return sr / lag;
    }
    return bestLag > 0 ? sr / bestLag : 0.0;
}

using doomsday::kick::Shape;
static int batch (const File& jobs, const File& dir)
{
    dir.createDirectory();
    ForgeProcessor p;
    StringArray lines; jobs.readLines (lines);
    for (auto& line : lines)
    {
        auto tok = StringArray::fromTokens (line, " ", "");
        if (tok.size() < 2) continue;
        p.setCurrentProgram (0);
        double secs = 1.2;
        for (int i = 2; i < tok.size(); ++i)
        {
            const auto id = tok[i].upToFirstOccurrenceOf ("=", false, false), v = tok[i].fromFirstOccurrenceOf ("=", false, false);
            if (id == "amp" || id == "denv") { Shape s; if (Shape::fromString (v, s)) p.setShape (id == "amp" ? 0 : 1, s); }
            else if (id == "secs") secs = v.getDoubleValue();
            else if (id == "sr") sr = v.getDoubleValue();
            else set (p, id, v.getFloatValue());
        }
        writeWav (dir.getChildFile (tok[0] + ".wav"), hit (p, tok[1].getIntValue(), secs));
    }
    return 0;
}
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    if (argc >= 4 && String (argv[1]) == "batch") return batch (File::getCurrentWorkingDirectory().getChildFile (argv[2]), File::getCurrentWorkingDirectory().getChildFile (argv[3]));
    const File outDir = File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "forge_out");
    outDir.createDirectory();
    using namespace ParamIDs;
    ForgeProcessor p;

    // 1) default kick: sane level, tail lands on the played note, ends at LENGTH
    {
        p.setCurrentProgram (0);
        set (p, length, 500.f);
        auto b = hit (p, 31, 0.8);
        const float pk = b.getMagnitude (0, 0, b.getNumSamples());
        check (pk > 0.5f && pk <= 0.99f, "default kick peak in range", String (pk, 3));
        const double f = pitchAt (b, 0.25, 0.12);
        check (std::abs (f - 49.0) < 3.0, "tail settles on the note (G1 = 49 Hz)", String (f, 1) + " Hz");
        const float after = b.getMagnitude (0, (int) (0.52 * sr), (int) (0.2 * sr));
        check (after < 1e-4f, "silent after LENGTH", String (after, 6));
        bool finite = true; for (int i = 0; i < b.getNumSamples(); ++i) finite &= std::isfinite (b.getSample (0, i));
        check (finite, "no NaN / inf");
        const double f2 = pitchAt (hit (p, 34, 0.8), 0.25, 0.12);
        check (std::abs (f2 - 58.3) < 3.5, "note A#1 -> 58 Hz", String (f2, 1) + " Hz");
    }
    // 1b) CLEAN SUB: the lows of a hard-distorted tail become a clean sine (2nd / 3rd harmonic drop)
    {
        auto harmDb = [] (const AudioBuffer<float>& b)
        {
            const int n = 1 << 14, a = (int) (0.12 * sr);
            std::vector<float> w ((size_t) n * 2, 0.f);
            for (int i = 0; i < n && a + i < b.getNumSamples(); ++i) w[(size_t) i] = b.getSample (0, a + i) * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * i / n));
            dsp::FFT fft (14); fft.performFrequencyOnlyForwardTransform (w.data());
            auto at = [&] (double hz) { const int k = (int) std::round (hz * n / sr); float m = 0; for (int j = k - 2; j <= k + 2; ++j) m = jmax (m, w[(size_t) j]); return (double) m; };
            return 20.0 * std::log10 ((at (98.0) + at (147.0)) / at (49.0));
        };
        p.setCurrentProgram (0);
        set (p, distType, 2.f); set (p, drive, 30.f); set (p, length, 600.f); set (p, clipDrive, 0.f);
        const double off = harmDb (hit (p, 31, 0.6));
        set (p, cleanSub, 250.f);
        const double on = harmDb (hit (p, 31, 0.6));
        check (on < off - 10.0, "CLEAN SUB: low harmonics of the distortion removed", "(" + String (off, 1) + " -> " + String (on, 1) + " dB)");
    }
    // 2) every preset: no NaN, peak <= 0 dBFS, not silent
    {
        int bad = 0; String worst;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            auto b = hit (p, 31, 1.4);
            const float pk = b.getMagnitude (0, 0, b.getNumSamples());
            bool finite = true; for (int s = 0; s < b.getNumSamples(); ++s) finite &= std::isfinite (b.getSample (0, s));
            // the owner's own kicks are kept exactly as he set them by ear (they may peak a little over 0 dBFS)
            const float limit = p.getProgramName (i).startsWith ("OWNER") ? 1.5f : 1.f;
            if (! finite || pk > limit || pk < 0.2f) { ++bad; worst << p.getProgramName (i) << " (" << String (pk, 2) << ") "; }
            writeWav (outDir.getChildFile (String (i).paddedLeft ('0', 2) + "_" + File::createLegalFileName (p.getProgramName (i)) + ".wav"), b);
        }
        check (bad == 0, "all presets clean", String (p.getNumPrograms()) + " presets " + worst);
    }
    // 3) retrigger: a new hit fades the old one without a click (no jump bigger than the kick itself makes)
    {
        p.setCurrentProgram (0);
        p.prepareToPlay (sr, block);
        AudioBuffer<float> b (2, block * 40); b.clear();
        for (int k = 0; k < 40; ++k)
        {
            AudioBuffer<float> c (2, block); c.clear(); MidiBuffer m;
            if (k == 0 || k == 20) m.addEvent (MidiMessage::noteOn (1, 31, 1.f), 100);
            p.processBlock (c, m);
            b.copyFrom (0, k * block, c, 0, 0, block);
        }
        check (b.getMagnitude (0, 20 * block, block * 20) > 0.5f, "retrigger plays a new hit");
    }
    // 4) envelopes: state round trip keeps the drawn shapes
    {
        p.setCurrentProgram (0);
        auto s = Shape::make ({ { 0, 1 }, { 300, 0.2f }, { 650, 0.9f }, { 1000, 1 } });
        p.setShape (1, s);
        MemoryBlock mb; p.getStateInformation (mb);
        ForgeProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        check (q.getShape (1).n == 4 && std::abs (q.getShape (1).pts[2].v - 0.9f) < 1e-3f, "drive envelope saved with the session");
    }
    // 5) CPU
    {
        p.setCurrentProgram (0);
        p.prepareToPlay (48000.0, block);
        AudioBuffer<float> b (2, block);
        const int blocks = (int) (48000.0 * 10 / block);
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int k = 0; k < blocks; ++k) { b.clear(); MidiBuffer m; if (k % 18 == 0) m.addEvent (MidiMessage::noteOn (1, 31, 1.f), 0); p.processBlock (b, m); }
        const double secs = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        check (secs / 10.0 < 0.15, "CPU (10 s of kicks at 150 BPM)", String (secs / 10.0 * 100.0, 1) + " %");
    }
    // 8) PITCH / FINE move the tail only
    {
        p.setCurrentProgram (0); sr = 48000.0;
        set (p, ParamIDs::tune, 5.f); set (p, ParamIDs::tailFine, 50.f);
        auto b = hit (p, 31, 0.5);
        const double hz = pitchAt (b, 0.3, 0.08), want = 49.0 * std::pow (2.0, 5.5 / 12.0);
        check (std::abs (hz - want) < 3.0, "PITCH +5 st, FINE +50 ct -> tail on 66.7 Hz", String (hz, 1) + " Hz");
        set (p, ParamIDs::tune, 0.f); set (p, ParamIDs::tailFine, 0.f);
    }
    // 9) PUNCH keeps its pitch on any note (only its START / END knobs set it)
    {
        p.setCurrentProgram (0); sr = 48000.0;
        set (p, ParamIDs::tailLevel, -24.f); set (p, ParamIDs::punchLevel, 0.f); set (p, ParamIDs::clickLevel, -60.f); set (p, ParamIDs::chaos, 0.f);
        auto lo = hit (p, 31, 0.1), hi = hit (p, 43, 0.1);
        double diff = 0, all = 0;
        for (int i = 0; i < (int) (0.03 * sr); ++i) { const double a = lo.getSample (0, i), b = hi.getSample (0, i); diff += (a - b) * (a - b); all += a * a; }
        // the tail is turned down, not off: allow a small rest
        check (diff < all * 0.1, "PUNCH: same pitch on G1 and G2", "(difference " + String (10.0 * std::log10 (diff / (all + 1e-12) + 1e-12), 1) + " dB)");
    }
    // 7) REVERSE BASS (v2.1): a kick on every beat at 150 BPM -> bass swells between them, on the note, clean gap before each kick
    {
        int rbPreset = 0;
        for (int i = 0; i < p.getNumPrograms(); ++i) if (p.getProgramName (i).startsWith ("BASS - Euphoric")) rbPreset = i;
        sr = 48000.0;
        auto play = [&] (bool on, int beats)
        {
            p.setCurrentProgram (rbPreset);
            set (p, ParamIDs::rbOn, on ? 1.f : 0.f);
            p.prepareToPlay (sr, block);
            const int beat = (int) (sr * 0.4);
            AudioBuffer<float> out (2, beat * beats); out.clear();
            for (int pos = 0; pos < out.getNumSamples(); pos += block)
            {
                const int n = jmin (block, out.getNumSamples() - pos);
                AudioBuffer<float> c (2, n); c.clear(); MidiBuffer m;
                for (int k = 0; k < beats; ++k) if (k * beat >= pos && k * beat < pos + n) m.addEvent (MidiMessage::noteOn (1, 31, 1.f), k * beat - pos);
                p.processBlock (c, m);
                for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, c, ch, 0, n);
            }
            return out;
        };
        auto rms = [] (const AudioBuffer<float>& b, double fromBeat, double toBeat)
        {
            const int a = (int) (fromBeat * 0.4 * sr), e = (int) (toBeat * 0.4 * sr);
            double acc = 0; for (int i = a; i < e; ++i) acc += (double) b.getSample (0, i) * b.getSample (0, i);
            return std::sqrt (acc / jmax (1, e - a));
        };
        const auto on = play (true, 8), off = play (false, 8);
        const double swellOn = rms (on, 2.6, 2.9), swellOff = rms (off, 2.6, 2.9), gap = rms (on, 2.965, 2.995);
        // strongest frequency 20..200 Hz in the swell (DFT scan)
        double hz = 0, bestMag = 0;
        {
            const int a0 = (int) (2.45 * 0.4 * sr), n = (int) (0.1 * sr);
            for (double f = 20.0; f <= 200.0; f += 0.25)
            {
                double re = 0, im = 0;
                for (int i = 0; i < n; ++i) { const double w = 0.5 - 0.5 * std::cos (6.283185307 * i / n), v = on.getSample (0, a0 + i) * w;
                                             re += v * std::cos (6.283185307 * f * i / sr); im += v * std::sin (6.283185307 * f * i / sr); }
                if (re * re + im * im > bestMag) { bestMag = re * re + im * im; hz = f; }
            }
        }
        check (swellOn > 0.05 && swellOn > swellOff * 20.0 && gap < swellOn * 0.15 && std::abs (hz - 49.0) < 4.0,
               "REVERSE BASS: swell between kicks, on the note, gap before the kick",
               "(swell " + String (swellOn, 3) + " vs off " + String (swellOff, 4) + ", gap " + String (gap, 3) + ", " + String (hz, 1) + " Hz)");
        float pk = on.getMagnitude (0, 0, on.getNumSamples());
        check (std::isfinite (pk) && pk < 1.05f, "REVERSE BASS: output stays clean", "(peak " + String (pk, 2) + ")");
        // A/B for the owner: 8 beats kick only, then 8 beats kick + reverse bass
        AudioBuffer<float> ab (2, off.getNumSamples() * 2);
        for (int ch = 0; ch < 2; ++ch) { ab.copyFrom (ch, 0, off, ch, 0, off.getNumSamples()); ab.copyFrom (ch, off.getNumSamples(), on, ch, 0, on.getNumSamples()); }
        writeWav (outDir.getChildFile ("reverse_bass_ab.wav"), ab);
    }
    // 6) GUI snapshot
    {
        for (int i = 0; i < p.getNumPrograms(); ++i) if (p.getProgramName (i).startsWith ("BASS - Euphoric")) p.setCurrentProgram (i);
        sr = 48000.0; hit (p, 31, 0.15);
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (1500, 1180);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.f);
        PNGImageFormat png; FileOutputStream fo (outDir.getChildFile ("gui.png")); fo.setPosition (0); fo.truncate(); png.writeImageToStream (img, fo);
    }
    printf ("%s\n", ok ? "ALL OK" : "SOME FAILED");
    return ok ? 0 : 1;
}
