// 4TUNE offline test: frozen parameter IDs, every control changes the sound (min vs max), FORTUNE transformation,
// SCREAM gain compensation, presets, polyphony / mono, state round trip, CPU, GUI snapshots, demo WAVs.
//   RenderTest <outdir>                                       all checks
//   RenderTest <out.wav> --midi <program | name> <file.mid> [bpm] [id=value ...]   a MIDI melody through 4TUNE (demos)
//   RenderTest <outdir> --write-ids                           (once) write Tests/param_ids.txt
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/Melody/MelodyFile.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>

using namespace juce;
using namespace tune;

static const double sr = 48000.0;
static const int block = 512;
static bool ok = true;

static void check (bool cond, const String& what, const String& extra = {})
{
    printf ("%-58s %s %s\n", what.toRawUTF8(), cond ? "OK" : "FAIL", extra.toRawUTF8());
    ok &= cond;
}
static void set (TuneProcessor& p, const String& id, float v)
{
    if (auto* prm = p.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    else { printf ("!! unknown parameter %s\n", id.toRawUTF8()); ok = false; }
}

struct Note { double beat, len; int note; int vel = 110; };

static AudioBuffer<float> render (TuneProcessor& p, const std::vector<Note>& notes, double beats, double bendAtBeat = -1.0)
{
    const double spb = sr * 60.0 / 150.0;
    const int total = (int) (beats * spb);
    AudioBuffer<float> out (2, total); out.clear();
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = jmin (block, total - pos);
        AudioBuffer<float> buf (2, n); buf.clear();
        MidiBuffer midi;
        for (auto& e : notes)
        {
            const int on = (int) (e.beat * spb), off = (int) ((e.beat + e.len) * spb);
            if (on >= pos && on < pos + n) midi.addEvent (MidiMessage::noteOn (1, e.note, (uint8) e.vel), on - pos);
            if (off >= pos && off < pos + n) midi.addEvent (MidiMessage::noteOff (1, e.note), off - pos);
        }
        if (bendAtBeat >= 0.0) { const int b = (int) (bendAtBeat * spb); if (b >= pos && b < pos + n) midi.addEvent (MidiMessage::pitchWheel (1, 12000), b - pos); }
        p.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, buf, ch, 0, n);
    }
    return out;
}
static float peakOf (const AudioBuffer<float>& b) { return jmax (b.getMagnitude (0, 0, b.getNumSamples()), b.getMagnitude (1, 0, b.getNumSamples())); }
static double rmsOf (const AudioBuffer<float>& b, int from = 0, int to = -1)
{
    if (to < 0) to = b.getNumSamples();
    double s = 0; for (int ch = 0; ch < 2; ++ch) for (int i = from; i < to; ++i) s += (double) b.getSample (ch, i) * b.getSample (ch, i);
    return std::sqrt (s / jmax (1, 2 * (to - from)));
}
static bool hasNaN (const AudioBuffer<float>& b)
{
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (ch, i))) return true;
    return false;
}
// brightness = spectral centroid of the mid signal over [from, to)
static double centroid (const AudioBuffer<float>& b, int from, int len)
{
    const int order = 14, N = 1 << order; juce::dsp::FFT fft (order);
    std::vector<float> d ((size_t) 2 * N, 0.f);
    for (int i = 0; i < jmin (N, len); ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) N);
        d[(size_t) i] = 0.5f * (b.getSample (0, from + i) + b.getSample (1, from + i)) * w;
    }
    fft.performFrequencyOnlyForwardTransform (d.data());
    double num = 0, den = 0;
    for (int k = 1; k < N / 2; ++k) { const double f = k * sr / N; num += f * d[(size_t) k]; den += d[(size_t) k]; }
    return den > 0 ? num / den : 0.0;
}
static double sideRatio (const AudioBuffer<float>& b, int from, int to)
{
    double m = 0, s = 0;
    for (int i = from; i < to; ++i) { const double l = b.getSample (0, i), r = b.getSample (1, i); m += (l + r) * (l + r); s += (l - r) * (l - r); }
    return m > 0 ? std::sqrt (s / m) : 0.0;
}
static void writeWav (const File& f, const AudioBuffer<float>& b)
{
    f.deleteFile(); WavAudioFormat wf;
    if (auto w = std::unique_ptr<AudioFormatWriter> (wf.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0))) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

// the test phrase: a short arpeggio (glide / retrigger / mono) + an A minor chord, a pitch-bend in the middle
static std::vector<Note> phrase()
{
    std::vector<Note> v { { 0.0, 0.3, 69, 100 }, { 0.375, 0.3, 72, 120 }, { 0.75, 0.3, 76, 90 }, { 1.125, 0.3, 74, 127 } };
    for (int n : { 57, 60, 64, 69 }) v.push_back ({ 1.5, 1.5, n, 110 });
    return v;
}

// fresh processor (deterministic random phases) with a context and one value
static AudioBuffer<float> renderWith (const std::vector<std::pair<String, float>>& values, int program = 0)
{
    TuneProcessor p; p.setPlayConfigDetails (0, 2, sr, block); p.prepareToPlay (sr, block);
    p.setCurrentProgram (program);
    for (auto& [id, v] : values) set (p, id, v);
    return render (p, phrase(), 4.0, 2.2);
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outArg = argc > 1 ? File (argv[1]) : File::getCurrentWorkingDirectory();
    const File idFile = File (__FILE__).getParentDirectory().getChildFile ("param_ids.txt");

    // ---------------------------------------------------------------- demo mode
    if (argc > 4 && String (argv[2]) == "--midi")
    {
        TuneProcessor p; p.setPlayConfigDetails (0, 2, sr, block); p.prepareToPlay (sr, block);
        const String progArg (argv[3]);
        int prog = progArg.containsOnly ("0123456789") ? progArg.getIntValue() : -1;
        if (prog < 0) for (int i = 0; i < p.getNumPrograms(); ++i) if (p.getProgramName (i).containsIgnoreCase (progArg)) { prog = i; break; }
        if (prog >= 0) p.setCurrentProgram (prog);
        double bpm = 150.0; int firstKv = 5;
        if (argc > 5 && ! String (argv[5]).contains ("=")) { bpm = String (argv[5]).getDoubleValue(); firstKv = 6; }
        for (int a = firstKv; a < argc; ++a) { const String kv (argv[a]); set (p, kv.upToFirstOccurrenceOf ("=", false, false), kv.fromFirstOccurrenceOf ("=", false, false).getFloatValue()); }
        FileInputStream in ((File (argv[4]))); MidiFile mf;
        if (! in.openedOk() || ! mf.readFrom (in)) { printf ("!! midi not read\n"); return 1; }
        const double tpq = (double) mf.getTimeFormat(); std::vector<std::pair<int, MidiMessage>> ev; double endS = 0;
        for (int t = 0; t < mf.getNumTracks(); ++t) for (auto* e : *mf.getTrack (t)) if (e->message.isNoteOnOrOff())
        { const double sec = e->message.getTimeStamp() / tpq * 60.0 / bpm; ev.push_back ({ (int) (sec * sr), e->message }); endS = std::max (endS, sec); }
        std::stable_sort (ev.begin(), ev.end(), [] (auto& a, auto& b) { return a.first < b.first; });
        const int total = (int) ((endS + 2.0) * sr); AudioBuffer<float> out (2, total); out.clear();
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = jmin (block, total - pos); AudioBuffer<float> b (2, n); b.clear(); MidiBuffer m;
            for (auto& [at, msg] : ev) if (at >= pos && at < pos + n) m.addEvent (msg, at - pos);
            p.processBlock (b, m); for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, b, ch, 0, n);
        }
        writeWav (outArg, out);
        printf ("rendered %s (%d events, %.1f s, %s) peak %.3f nan %d\n", outArg.getFileName().toRawUTF8(), (int) ev.size(), endS + 2.0,
                p.currentPresetName().toRawUTF8(), peakOf (out), (int) hasNaN (out));
        return 0;
    }
    // RenderTest <out.mid> --melody [mode] [seed] [key] [scale] [bars]   a generated melody as a MIDI file (demos)
    if (argc > 2 && String (argv[2]) == "--melody")
    {
        melody::Settings m;
        if (argc > 3) m.mode = String (argv[3]).getIntValue();
        if (argc > 4) m.seed = (uint32_t) String (argv[4]).getIntValue();
        if (argc > 5) m.key = String (argv[5]).getIntValue();
        if (argc > 6) m.scale = String (argv[6]).getIntValue();
        if (argc > 7) m.bars = String (argv[7]).getIntValue();
        melody::Generator g; auto v = g.generate (m);
        melody::writeMidi (v, 150.0, outArg);
        printf ("melody %s %s %s, %d notes -> %s\n", melody::modeName (m.mode), melody::keyName (m.key), melody::scaleName (m.scale), (int) v.size(), outArg.getFileName().toRawUTF8());
        return 0;
    }
    if (argc > 2 && String (argv[2]) == "--write-ids")
    {
        StringArray ids; for (auto* id : paramIds()) ids.add (id);
        idFile.replaceWithText (ids.joinIntoString ("\n") + "\n");
        printf ("wrote %d ids to %s\n", ids.size(), idFile.getFullPathName().toRawUTF8());
        return 0;
    }
    const File outDir = outArg; outDir.createDirectory();
    const bool quick = SystemStats::getEnvironmentVariable ("FTN_QUICK", "0") == "1";

    // ---------------------------------------------------------------- 1. frozen IDs (append only = FL automation index)
    {
        StringArray frozen; frozen.addLines (idFile.loadFileAsString()); frozen.removeEmptyStrings();
        const auto& ids = paramIds();
        bool same = frozen.size() > 0 && frozen.size() <= (int) ids.size();
        for (int i = 0; same && i < frozen.size(); ++i) same = frozen[i] == ids[(size_t) i];
        check (same, "parameter IDs = frozen list (append only)", String ((int) ids.size()) + " params, " + String (frozen.size()) + " frozen");
        TuneProcessor p;
        check ((int) p.getParameters().size() == (int) ids.size(), "APVTS has every table parameter");
    }

    // ---------------------------------------------------------------- 2. INIT pluck + presets play
    {
        auto a = renderWith ({});
        check (! hasNaN (a) && peakOf (a) > 0.05f && peakOf (a) <= 1.0f, "default sound (euphoric pluck) plays", "peak " + String (peakOf (a), 3));
        writeWav (outDir.getChildFile ("default_phrase.wav"), a);
        TuneProcessor p; p.setPlayConfigDetails (0, 2, sr, block); p.prepareToPlay (sr, block);
        AudioBuffer<float> reel (2, (int) (sr * 2.0 * p.getNumPrograms())); reel.clear();
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            auto o = render (p, phrase(), 5.0);
            const float pk = peakOf (o);
            check (! hasNaN (o) && pk > 0.03f && pk <= 1.0f, "preset " + p.getProgramName (i), "peak " + String (pk, 3));
            for (int ch = 0; ch < 2; ++ch) reel.copyFrom (ch, (int) (i * sr * 2.0), o, ch, (int) (sr * 0.6), jmin ((int) (sr * 2.0), o.getNumSamples() - (int) (sr * 0.6)));
        }
        writeWav (outDir.getChildFile ("presets_reel.wav"), reel);
    }

    // ---------------------------------------------------------------- 3. FORTUNE: one coherent transformation (spec 12-19, 31)
    {
        double cen[3], wid[3], lvl[3];
        const float fv[3] { 0.f, 0.5f, 1.f };
        for (int k = 0; k < 3; ++k)
        {
            auto o = renderWith ({ { "fortune", fv[k] }, { "dly_on", 0 }, { "rvb_on", 0 } });
            const int a = (int) (sr * 1.5 * 0.4), b = (int) (sr * 2.4 * 0.4 + sr * 0.6);
            cen[k] = centroid (o, (int) (1.5 * 0.4 * sr), (int) (0.35 * sr)); wid[k] = sideRatio (o, a, b); lvl[k] = 20.0 * std::log10 (rmsOf (o, a, b) + 1e-9);
            if (k == 2) writeWav (outDir.getChildFile ("fortune_100.wav"), o);
            if (k == 0) writeWav (outDir.getChildFile ("fortune_0.wav"), o);
        }
        check (cen[0] < cen[1] && cen[1] < cen[2], "FORTUNE: brighter 0 -> 50 -> 100 %", String ((int) cen[0]) + " / " + String ((int) cen[1]) + " / " + String ((int) cen[2]) + " Hz");
        check (wid[0] < wid[1] && wid[1] < wid[2], "FORTUNE: wider 0 -> 50 -> 100 %", String (wid[0], 3) + " / " + String (wid[1], 3) + " / " + String (wid[2], 3));
        check (std::abs (lvl[2] - lvl[0]) < 6.0, "FORTUNE: not just louder (level within 6 dB)", String (lvl[0], 1) + " / " + String (lvl[1], 1) + " / " + String (lvl[2], 1) + " dB");
        // manual values stay valid: FORTUNE adds on top of the cutoff knob (a darker knob stays darker at the same FORTUNE)
        auto dark = renderWith ({ { "fortune", 1.f }, { "flt_cut", 300.f }, { "dly_on", 0 }, { "rvb_on", 0 } });
        auto bright = renderWith ({ { "fortune", 1.f }, { "flt_cut", 3000.f }, { "dly_on", 0 }, { "rvb_on", 0 } });
        const int f0 = (int) (1.5 * 0.4 * sr);
        check (centroid (dark, f0, (int) (0.35 * sr)) < centroid (bright, f0, (int) (0.35 * sr)), "FORTUNE keeps the manual cutoff (BASE + FORTUNE)");
    }

    // ---------------------------------------------------------------- 4. SCREAM: character, not loudness (spec 11)
    {
        double lv[3]; const float sv[3] { 0.1f, 0.5f, 1.f };
        for (int k = 0; k < 3; ++k)
        {
            auto o = renderWith ({ { "scr_amt", sv[k] }, { "dly_on", 0 }, { "rvb_on", 0 }, { "lim_on", 0 } });
            lv[k] = 20.0 * std::log10 (rmsOf (o, (int) (0.6 * sr), (int) (1.6 * sr)) + 1e-9);
            if (k == 2) writeWav (outDir.getChildFile ("scream_100.wav"), o);
        }
        check (std::abs (lv[2] - lv[0]) < 3.5 && std::abs (lv[1] - lv[0]) < 3.5, "SCREAM gain compensation (10 / 50 / 100 % within 3.5 dB)",
               String (lv[0], 1) + " / " + String (lv[1], 1) + " / " + String (lv[2], 1) + " dB");
    }

    // ---------------------------------------------------------------- 5. every control changes the sound (min vs max, in a context)
    if (! quick)
    {
        auto context = [] (const String& id) -> std::vector<std::pair<String, float>>
        {
            std::vector<std::pair<String, float>> c;
            if (id.startsWith ("o4_")) c.push_back ({ "o4_on", 1 });
            if (id.startsWith ("o2_") && (id.endsWith ("detune") || id.endsWith ("uspread") || id.endsWith ("umix") || id.endsWith ("width"))) c.push_back ({ "o2_voices", 5 });
            if (id.startsWith ("o2_") && id.endsWith ("width")) c.push_back ({ "o2_uspread", 0.6f });   // SPREAD widens what the unison already spreads
            if (id.endsWith ("_wt")) c.push_back ({ id.upToFirstOccurrenceOf ("_", false, false) + "_wave", 5 });
            if (id.endsWith ("_phase")  && id.startsWith ("o")) c.push_back ({ id.upToFirstOccurrenceOf ("_", false, false) + "_phrand", 0 });
            if (id.startsWith ("me_")) { c.push_back ({ "mm3_src", 3 }); c.push_back ({ "mm3_dst", 9 }); c.push_back ({ "mm3_amt", 0.6f }); }
            if (id.startsWith ("lfo"))
            {
                c.push_back ({ "mm3_src", id.startsWith ("lfo1") ? 4.f : 5.f }); c.push_back ({ "mm3_dst", 9 }); c.push_back ({ "mm3_amt", 0.6f });
                if (id.endsWith ("_div")) c.push_back ({ id.upToFirstOccurrenceOf ("_", false, false) + "_sync", 1 });
                if (id.endsWith ("_retrig")) c.push_back ({ "play_mode", 1 });
                c.push_back ({ id.upToFirstOccurrenceOf ("_", false, false) + "_rate", 3.f });
            }
            if (id.startsWith ("mm"))
            {
                const String s = id.upToFirstOccurrenceOf ("_", false, false);
                c.push_back ({ s + "_src", 4 }); c.push_back ({ s + "_dst", 9 }); c.push_back ({ s + "_amt", 0.6f }); c.push_back ({ "fortune", 0.5f });
            }
            if (id.startsWith ("ft_")) c.push_back ({ "fortune", 0.8f });
            if (id == "eq_on") c.push_back ({ "eq_low", 6 });
            if (id.startsWith ("dist_")) c.push_back ({ "dist_on", 1 });
            if (id.startsWith ("cho_")) c.push_back ({ "cho_on", 1 });
            if (id.startsWith ("lim_") && id != "lim_input") c.push_back ({ "lim_input", 14 });
            if (id == "glide") c.push_back ({ "play_mode", 2 });
            return c;
        };
        int tested = 0, dead = 0;
        TuneProcessor list;
        for (auto* prm : list.getParameters())
        {
            auto* rp = dynamic_cast<RangedAudioParameter*> (prm);
            if (rp == nullptr) continue;
            const String id = rp->paramID;
            auto ctx = context (id);
            auto lo = ctx; lo.push_back ({ id, rp->convertFrom0to1 (0.f) });
            auto hi = ctx; hi.push_back ({ id, rp->convertFrom0to1 (1.f) });
            if (id.startsWith ("mm") && id.endsWith ("_src")) hi.back().second = (float) tune::dsp::SFortune;
            if (id.endsWith ("_phase")) hi.back().second = 0.5f;          // 0 and 100 % are the same point of the cycle
            auto a = renderWith (lo), b = renderWith (hi);
            double diff = 0, ref = 0;
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < a.getNumSamples(); ++i) { const double d = a.getSample (ch, i) - b.getSample (ch, i); diff += d * d; ref += (double) a.getSample (ch, i) * a.getSample (ch, i); }
            const double rel = std::sqrt (diff / jmax (1e-12, ref));
            ++tested;
            if (rel < 1e-3) { ++dead; check (false, "control changes the sound: " + id, "rel " + String (rel, 6)); }
        }
        check (dead == 0, "every control changes the sound", String (tested) + " tested, " + String (dead) + " dead");
    }

    // ---------------------------------------------------------------- 6. polyphony / mono / legato / state
    {
        TuneProcessor p; p.setPlayConfigDetails (0, 2, sr, block); p.prepareToPlay (sr, block);
        std::vector<Note> sixteen; for (int i = 0; i < 20; ++i) sixteen.push_back ({ 0.0, 1.5, 40 + i * 2 });
        auto o = render (p, sixteen, 3.0);
        check (! hasNaN (o) && peakOf (o) <= 1.0f, "20 notes at once (voice stealing, limiter)", "peak " + String (peakOf (o), 3));
        set (p, "play_mode", 2); set (p, "glide", 120);
        auto m = render (p, phrase(), 4.0);
        check (! hasNaN (m) && peakOf (m) > 0.05f, "LEGATO + glide plays");
        MemoryBlock st; p.getStateInformation (st);
        TuneProcessor q; q.setStateInformation (st.getData(), (int) st.getSize());
        bool same = true;
        for (int i = 0; i < P::count; ++i) same &= std::abs (p.param ((P::Id) i) - q.param ((P::Id) i)) < 1e-4f;
        check (same, "state round trip (project reopen restores everything)");
        p.smartRandom(); auto r1 = render (p, phrase(), 4.0);
        check (! hasNaN (r1) && peakOf (r1) > 0.02f && peakOf (r1) <= 1.0f, "SMART RANDOM plays", p.currentPresetName());
        p.fullRandom(); auto r2 = render (p, phrase(), 4.0);
        check (! hasNaN (r2) && peakOf (r2) <= 1.0f, "FULL RANDOM stays finite and limited", p.currentPresetName());
    }

    // ---------------------------------------------------------------- 7. CPU: the festival supersaw, 8 notes
    {
        TuneProcessor p; p.setPlayConfigDetails (0, 2, sr, block); p.prepareToPlay (sr, block);
        for (int i = 0; i < p.getNumPrograms(); ++i) if (p.getProgramName (i).contains ("Festival Supersaw")) p.setCurrentProgram (i);
        std::vector<Note> chord; for (int n : { 45, 52, 57, 60, 64, 69, 72, 76 }) chord.push_back ({ 0.0, 9.0, n });
        const auto t0 = std::chrono::steady_clock::now();
        auto o = render (p, chord, 25.0);          // 10 s
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        const double load = secs / (25.0 * 60.0 / 150.0) * 100.0;
        check (load < 60.0, "CPU: 8-note festival supersaw (" + String (load, 1) + " % of one core)");
    }

    // ---------------------------------------------------------------- 9. MELODY ENGINE (spec 21-24)
    {
        double meanIv[melody::numModes] {}; int leadCount[melody::numModes] {};
        bool allOk = true;
        for (int mo = 0; mo < melody::numModes; ++mo)
            for (int sc : { (int) melody::Minor, (int) melody::Major, (int) melody::HarmonicMinor, (int) melody::Phrygian })
                for (uint32_t seed = 1; seed <= 12; ++seed)
                    for (int bars : { 1, 2, 4, 8 })
                    {
                        melody::Settings m; m.mode = mo; m.scale = sc; m.seed = seed; m.bars = bars; m.key = (int) (seed % 12);
                        melody::Generator g; auto v = g.generate (m);
                        double end = 0; int prev = -1; double iv = 0; int lead = 0;
                        for (auto& n : v)
                        {
                            allOk &= melody::inScale (m, n.pitch) && n.len > 0 && n.start >= 0 && n.vel > 0 && n.vel <= 127;
                            allOk &= n.pitch >= 12 * (m.octLow + 1) - 26 && n.pitch <= 12 * (m.octHigh + 2) - 1;
                            end = jmax (end, n.start + n.len);
                            if (n.pitch >= 12 * (m.octLow + 1)) { if (prev >= 0) iv += std::abs (n.pitch - prev); prev = n.pitch; ++lead; }
                        }
                        allOk &= ! v.empty() && end <= bars * 4.0 + 1e-6 && end > bars * 4.0 - 1.01;
                        if (sc == melody::Minor && bars == 4) { meanIv[mo] += lead > 1 ? iv / (lead - 1) : 0; leadCount[mo] += lead; }
                    }
        check (allOk, "MELODY: every mode / scale / length: in key, in range, the whole phrase");
        melody::Settings m; m.seed = 77;
        melody::Generator g1, g2; auto a = g1.generate (m), b = g2.generate (m);
        bool same = a.size() == b.size(); for (size_t i = 0; same && i < a.size(); ++i) same = a[i].pitch == b[i].pitch && a[i].start == b[i].start;
        m.seed = 78; auto c = g1.generate (m);
        bool differ = c.size() != a.size(); for (size_t i = 0; ! differ && i < a.size(); ++i) differ = a[i].pitch != c[i].pitch || a[i].start != c[i].start;
        check (same && differ, "MELODY: same seed = same melody, new seed = new melody");
        check (meanIv[melody::Euphoric] > meanIv[melody::Classic] && meanIv[melody::Euphoric] > meanIv[melody::Minimal]
               && leadCount[melody::Minimal] < leadCount[melody::Anthem],
               "MELODY: the modes follow their own rules",
               "interval euphoric " + String (meanIv[melody::Euphoric] / 12.0, 2) + " / classic " + String (meanIv[melody::Classic] / 12.0, 2)
               + " / minimal " + String (meanIv[melody::Minimal] / 12.0, 2));
        // MIDI file round trip (DRAG MIDI keeps pitch, length, timing, velocity)
        const File mid = outDir.getChildFile ("melody_roundtrip.mid");
        melody::writeMidi (a, 150.0, mid);
        auto r = melody::readMidi (mid);
        bool rt = r.size() == a.size();
        for (size_t i = 0; rt && i < a.size(); ++i) rt = r[i].pitch == a[i].pitch && std::abs (r[i].start - a[i].start) < 0.002 && std::abs (r[i].len - a[i].len) < 0.004 && r[i].vel == a[i].vel;
        check (rt, "MELODY: MIDI file keeps pitch / length / timing / velocity", String ((int) r.size()) + " notes");

        // the processor: state, undo, presets, preview
        TuneProcessor p; p.setPlayConfigDetails (0, 2, sr, block); p.prepareToPlay (sr, block);
        p.setMelodySetting ("mode", (int) melody::Festival); p.setMelodySetting ("seed", 5); p.generateMelody (false);
        const auto before = p.melodyNotes();
        p.generateMelody (true);
        const auto after = p.melodyNotes();
        p.undo.undo();
        const auto undone = p.melodyNotes();
        bool u = undone.size() == before.size(); for (size_t i = 0; u && i < before.size(); ++i) u = undone[i].pitch == before[i].pitch && undone[i].start == before[i].start;
        check (u && after.size() > 0, "MELODY: GENERATE is one undo step");
        MemoryBlock st; p.getStateInformation (st);
        TuneProcessor q; q.setStateInformation (st.getData(), (int) st.getSize());
        const auto restored = q.melodyNotes();
        bool rs = restored.size() == undone.size(); for (size_t i = 0; rs && i < undone.size(); ++i) rs = restored[i].pitch == undone[i].pitch && restored[i].start == undone[i].start;
        check (rs && (int) q.melodySettings().mode == melody::Festival, "MELODY: saved with the project (notes + settings)");
        p.setCurrentProgram (3);
        check (p.melodyNotes().size() == undone.size(), "MELODY: a sound preset keeps the melody");
        // PREVIEW: no MIDI from the host, the melody plays the synth; off = silence
        auto silent = render (p, {}, 8.0);
        p.melodyPreview = true;
        auto played = render (p, {}, 8.0);
        p.melodyPreview = false;
        auto after2 = render (p, {}, 8.0);
        check (peakOf (silent) < 1e-4f && peakOf (played) > 0.05f && rmsOf (after2, after2.getNumSamples() / 2) < 1e-3,
               "MELODY: PREVIEW plays the melody (own clock), stops cleanly", "peak " + String (peakOf (played), 3));
        writeWav (outDir.getChildFile ("melody_preview.wav"), played);
    }

    // ---------------------------------------------------------------- 8. GUI snapshots
    {
        TuneProcessor p;
        std::unique_ptr<TuneEditor> ed (dynamic_cast<TuneEditor*> (p.createEditor()));
        ed->setSize (TuneCanvas::W, TuneCanvas::H);
        static const char* names[] { "synth", "mod", "fx", "melody" };
        for (int pg = 0; pg < 4; ++pg)
        {
            ed->canvas.showPage (pg); ed->canvas.refreshNow();
            auto img = ed->canvas.createComponentSnapshot (ed->canvas.getLocalBounds(), true, 1.f);
            File f = outDir.getChildFile (String ("gui_") + names[pg] + ".png"); f.deleteFile();
            FileOutputStream os (f); PNGImageFormat png; png.writeImageToStream (img, os);
        }
        int knobs = 0; for (auto* pg : ed->canvas.pages()) knobs += (int) pg->knobs.size();
        check (knobs > 120, "GUI built", String (knobs) + " knobs");
    }

    printf ("\n%s\n", ok ? "ALL OK" : "SOME CHECKS FAILED");
    return ok ? 0 : 1;
}
