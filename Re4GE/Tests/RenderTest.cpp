// Offline checks for RE4GE (reverse bass)
//   RenderTest <outdir>       tests, preset renders (with a short kick on top, for listening), GUI snapshots
#include "../Source/PluginProcessor.h"
#include "../Source/GUI/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>

using namespace juce;
static double sr = 48000.0;
static const int block = 512;
static bool ok = true;
static void check (bool c, const char* what, const String& extra = {}) { printf ("%-58s %s %s\n", what, c ? "OK" : "FAIL", extra.toRawUTF8()); ok &= c; }
static void set (ReProcessor& p, const String& id, float v) { if (auto* a = p.apvts.getParameter (id)) a->setValueNotifyingHost (a->convertTo0to1 (v)); }
static float raw (ReProcessor& p, const String& id) { return p.apvts.getRawParameterValue (id)->load(); }

// a fake host with a tempo
struct Host : AudioPlayHead
{
    double bpm = 150.0, ppq = 0.0; bool withPpq = false;
    Optional<PositionInfo> getPosition() const override { PositionInfo i; i.setBpm (bpm); i.setIsPlaying (true); if (withPpq) i.setPpqPosition (ppq); return i; }
};

// notes: (start ms, length ms, note); renders `seconds`
struct Note { double at, len; int note; float vel = 1.f; };
// bufferSize < 0 = random sizes 1..1500 (seeded)
static AudioBuffer<float> play (ReProcessor& p, Host& h, std::vector<Note> notes, double seconds, int bufferSize = block)
{
    p.setPlayHead (&h);
    p.prepareToPlay (sr, bufferSize > 0 ? bufferSize : 1500);
    AudioBuffer<float> out (2, (int) (sr * seconds)); out.clear();
    Random rnd (77);
    for (int pos = 0, n = 0; pos < out.getNumSamples(); pos += n)
    {
        n = jmin (bufferSize > 0 ? bufferSize : 1 + rnd.nextInt (1500), out.getNumSamples() - pos);
        AudioBuffer<float> b (2, n); b.clear(); MidiBuffer m;
        for (auto& nt : notes)
        {
            const int on = (int) (nt.at * 0.001 * sr), off = (int) ((nt.at + nt.len) * 0.001 * sr);
            if (on >= pos && on < pos + n) m.addEvent (MidiMessage::noteOn (1, nt.note, nt.vel), on - pos);
            if (off >= pos && off < pos + n) m.addEvent (MidiMessage::noteOff (1, nt.note), off - pos);
        }
        h.ppq = pos / sr * h.bpm / 60.0;
        p.processBlock (b, m);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, b, ch, 0, n);
    }
    return out;
}
static std::vector<Note> everyBeat (int beats, double bpm, int note = 31) { std::vector<Note> v; for (int i = 0; i < beats; ++i) v.push_back ({ i * 60000.0 / bpm, 100.0, note }); return v; }
static double rms (const AudioBuffer<float>& b, double fromMs, double toMs)
{
    const int a = jmax (0, (int) (fromMs * 0.001 * sr)), e = jmin (b.getNumSamples(), (int) (toMs * 0.001 * sr));
    double acc = 0; for (int i = a; i < e; ++i) acc += (double) b.getSample (0, i) * b.getSample (0, i);
    return std::sqrt (acc / jmax (1, e - a));
}
static int byName (ReProcessor& p, const char* start) { for (int i = 0; i < p.getNumPrograms(); ++i) if (p.getProgramName (i).startsWith (start)) return i; return 0; }
static double diff (const AudioBuffer<float>& a, const AudioBuffer<float>& b)
{
    double d = 0; for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < jmin (a.getNumSamples(), b.getNumSamples()); ++i) d = jmax (d, (double) std::abs (a.getSample (ch, i) - b.getSample (ch, i)));
    return d;
}
static bool finite (const AudioBuffer<float>& b) { for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (0, i))) return false; return true; }
static void writeWav (const File& f, const AudioBuffer<float>& b)
{
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
    if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}
// strongest frequency 20..300 Hz in a window (DFT scan)
static double domHz (const AudioBuffer<float>& b, double fromMs, double lenMs)
{
    double hz = 0, best = 0;
    const int a0 = (int) (fromMs * 0.001 * sr), n = (int) (lenMs * 0.001 * sr);
    for (double f = 20.0; f <= 300.0; f += 0.5)
    {
        double re = 0, im = 0;
        for (int i = 0; i < n; ++i) { const double w = 0.5 - 0.5 * std::cos (6.283185307 * i / n), v = b.getSample (0, a0 + i) * w;
                                     re += v * std::cos (6.283185307 * f * i / sr); im += v * std::sin (6.283185307 * f * i / sr); }
        if (re * re + im * im > best) { best = re * re + im * im; hz = f; }
    }
    return hz;
}
// a short kick on every beat (4GE's voice, default settings, 170 ms) so the owner hears the bass in context
static void addKicks (AudioBuffer<float>& b, double bpm)
{
    ge::Params k; k.lengthMs = 170.f; k.punchIntoDist = true; k.headFixed = true; k.headMs = 110.f;
    k.cook (sr);
    ge::Voice v; v.prepare (sr);
    const int beat = (int) (60.0 / bpm * sr);
    for (int start = 0; start < b.getNumSamples(); start += beat)
    {
        v.trigger (49.f, 1.f, k);
        for (int i = start; i < jmin (b.getNumSamples(), start + beat); ++i) { const float y = ge::masterClip (v.render (k), 1.6f, 0.4f) * 0.8f; for (int ch = 0; ch < 2; ++ch) b.addSample (ch, i, y); }
    }
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outDir = File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "re4ge_out");
    outDir.createDirectory();
    using namespace ParamIDs;
    ReProcessor p; Host host;
    // RenderTest <outdir> --analyse <wav>: KICK-TO-REVERSE sections of a kick
    if (argc > 3 && String (argv[2]) == "--analyse")
    {
        if (! p.loadSample (File::getCurrentWorkingDirectory().getChildFile (argv[3]))) { printf ("cannot load\n"); return 1; }
        const auto& k = p.kickSections();
        printf ("transient %.1f (%.0f%%)  punch %.1f (%.0f%%)  body %.1f (%.0f%%)  tail end %.1f (%.0f%%)  root %.1f Hz = note %d %+.0f ct\n",
                k.transientEnd, k.confTransient * 100.f, k.punchEnd, k.confPunch * 100.f, k.bodyEnd, k.confBody * 100.f, k.tailEnd, k.confTail * 100.f, k.rootHz, k.rootNote, k.rootCents);
        return 0;
    }
    // RenderTest <outdir> --random <out.wav>: Dense Rise, then one random bass per STYLE (FULL, amount 100 %), 4 beats each with a kick
    if (argc > 3 && String (argv[2]) == "--random")
    {
        Random rng (7);
        AudioBuffer<float> all (2, 0);
        for (int sIdx = -1; sIdx < re4ge::Randomizer::NumStyles; ++sIdx)
        {
            p.setCurrentProgram (byName (p, "CLASSIC - Dense Rise"));
            if (sIdx >= 0) { re4ge::Randomizer::Settings st; st.style = std::getenv ("RND_STYLE") != nullptr ? std::atoi (std::getenv ("RND_STYLE")) : sIdx; const float db = p.randomizeWith (st); printf ("%s: %.1f dB\n", re4ge::Randomizer::styleNames()[sIdx].toRawUTF8(), db);
                if (std::getenv ("RND_DUMP") != nullptr) for (auto* a : p.getParameters()) if (auto* w = dynamic_cast<RangedAudioParameter*> (a)) if (std::abs (w->getValue() - w->getDefaultValue()) > 1e-4f) printf ("  %s %s\n", w->paramID.toRawUTF8(), w->getCurrentValueAsText().toRawUTF8()); }
            auto b = play (p, host, everyBeat (4, 150.0), 1.6);
            addKicks (b, 150.0);
            const int at = all.getNumSamples(); all.setSize (2, at + b.getNumSamples(), true);
            for (int ch = 0; ch < 2; ++ch) all.copyFrom (ch, at, b, ch, 0, b.getNumSamples());
        }
        all.applyGain (0.7f);
        writeWav (File::getCurrentWorkingDirectory().getChildFile (argv[3]), all);
        return 0;
    }
    // RenderTest <outdir> --render <settings.txt> <out.wav> [bpm] [beats]: 'preset <name>' / '<id> <value>' lines -> a WAV (for matching)
    if (argc > 4 && String (argv[2]) == "--render")
    {
        StringArray lines; lines.addLines (File::getCurrentWorkingDirectory().getChildFile (argv[3]).loadFileAsString());
        bool kicks = false; int renderNote = 31;
        for (auto& ln : lines)
        {
            auto t = StringArray::fromTokens (ln.trim(), " ", ""); if (t.size() < 2) continue;
            if (t[0] == "kicks") { kicks = t[1].getIntValue() != 0; continue; }                 // 'kicks 1': a kick on every beat (A/B)
            if (t[0] == "note") { renderNote = t[1].getIntValue(); continue; }                       // 'note 26': the MIDI note of the bass
            if (t[0] == "ab") { if (t[1] == "sync") p.morphSync(); else p.morphSelect (t[1].getIntValue()); continue; }   // 'ab 0' / 'ab 1' / 'ab sync': MORPH slots
            if (t[0] == "preset") p.setCurrentProgram (byName (p, ln.fromFirstOccurrenceOf ("preset ", false, false).trim().toRawUTF8()));
            else if (t[0] == "sample") p.loadSample (File (ln.fromFirstOccurrenceOf ("sample ", false, false).trim()));
            else if (t[0] == "detect") p.detectKick (t[1].getIntValue());
            else set (p, t[0], t[1].getFloatValue());
        }
        host.bpm = argc > 5 ? String (argv[5]).getDoubleValue() : 147.0;
        const int beatsN = argc > 6 ? String (argv[6]).getIntValue() : 4;
        host.withPpq = true;                                                   // like FL: the SEQUENCER follows the song grid
        auto b = play (p, host, everyBeat (beatsN, host.bpm, renderNote), beatsN * 60.0 / host.bpm);
        host.withPpq = false;
        if (kicks) addKicks (b, host.bpm);
        writeWav (File::getCurrentWorkingDirectory().getChildFile (argv[4]), b);
        printf ("peak %.2f\n", b.getMagnitude (0, 0, b.getNumSamples()));
        return 0;
    }

    // 1) every preset: clean, audible, not over 0 dBFS; render with kicks for listening
    {
        int bad = 0; String why;
        AudioBuffer<float> all (2, (int) (sr * 1.6) * p.getNumPrograms()); all.clear();
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i); host.bpm = 150.0;
            const bool gate = p.getProgramName (i).startsWith ("GATE");
            auto notes = gate ? std::vector<Note> { { 200.0, 300.0, 31 }, { 600.0, 150.0, 34 }, { 800.0, 400.0, 29 } } : everyBeat (4, 150.0);
            auto b = play (p, host, notes, 1.6);
            const float pk = b.getMagnitude (0, 0, b.getNumSamples());
            printf ("   %-34s peak %.2f  rms %.1f dB\n", p.getProgramName (i).toRawUTF8(), pk, 20.0 * std::log10 (rms (b, 0, 1600) + 1e-9));
            if (! finite (b) || pk > 1.0f || pk < 0.05f) { ++bad; why << p.getProgramName (i) << " (" << String (pk, 2) << ") "; }
            writeWav (outDir.getChildFile ("solo_" + String (i).paddedLeft ('0', 2) + ".wav"), b);       // the bass alone (for measuring)
            if (! gate) addKicks (b, 150.0);
            writeWav (outDir.getChildFile (String (i).paddedLeft ('0', 2) + "_" + File::createLegalFileName (p.getProgramName (i)) + ".wav"), b);
            for (int ch = 0; ch < 2; ++ch) all.copyFrom (ch, i * (int) (sr * 1.6), b, ch, 0, b.getNumSamples());
        }
        writeWav (outDir.getChildFile ("re4ge_presets_with_kick.wav"), all);
        check (bad == 0, "all presets clean and audible", String (p.getNumPrograms()) + " presets " + why);
    }
    // 2) PATTERN timing (PUMP, 1/4): silent until DELAY, sound in the middle, silent before the next kick - at 150 and 172 BPM
    for (double bpm : { 150.0, 172.0 })
    {
        p.setCurrentProgram (0); host.bpm = bpm;
        auto b = play (p, host, everyBeat (4, bpm), 4 * 60.0 / bpm);
        const double beatMs = 60000.0 / bpm, delay = 0.36 * beatMs, gate = 0.98 * beatMs;
        // onset: the first sample above -40 dB of the bass's peak in beat 2
        const float pk = b.getMagnitude (0, (int) (beatMs * 0.001 * sr), (int) (beatMs * 0.001 * sr));
        int on = -1, off = -1;
        for (int i = (int) (beatMs * 0.001 * sr); i < (int) (2 * beatMs * 0.001 * sr); ++i)
            if (std::abs (b.getSample (0, i)) > pk * 0.01f) { if (on < 0) on = i; off = i; }
        const double onMs = on * 1000.0 / sr - beatMs, offMs = off * 1000.0 / sr - beatMs;
        check (std::abs (onMs - delay) < 3.0 && offMs < gate + 1.0 && offMs > gate - 30.0, ("PATTERN timing at " + String (bpm, 0) + " BPM: starts on DELAY, gone before the kick").toRawUTF8(),
               "(start " + String (onMs, 1) + " / " + String (delay, 1) + " ms, end " + String (offMs, 1) + " / " + String (gate, 1) + " ms)");
    }
    // 3) TAIL backwards: on the note in the swell, rising at the end (the tail's pitch drop played backwards)
    {
        p.setCurrentProgram (byName (p, "PUMP - QL Tail")); host.bpm = 150.0;
        set (p, subHz, 20.f); set (p, highCut, 3000.f); set (p, drive, 0.f); set (p, uniVoices, 1.f);
        auto b = play (p, host, everyBeat (2, 150.0), 0.8);
        const double hz = domHz (b, 190.0, 80.0), hzLate = domHz (b, 360.0, 30.0);
        check (hz > 45.0 && hz < 65.0 && hzLate > hz * 1.3, "TAIL: on the note, then rises before the kick", String (hz, 1) + " -> " + String (hzLate, 1) + " Hz");
    }
    // 4) SUB: the clean sine adds low end, the low end is not cut by default
    {
        p.setCurrentProgram (0); host.bpm = 150.0;
        auto low = [&] (const AudioBuffer<float>& b) { return domHz (b, 200.0, 120.0); };
        auto b = play (p, host, everyBeat (2, 150.0), 0.8);
        auto* lc = p.apvts.getParameter (lowCut);
        check (lc != nullptr && lc->getValue() < 0.001f && std::abs (low (b) - 49.0) < 4.0, "the low end stays: LOW CUT off by default, the note is the strongest",
               String (low (b), 1) + " Hz");
    }
    // 5) GATE mode: a 300 ms note sounds, silence after note-off + FADE
    {
        for (int i = 0; i < p.getNumPrograms(); ++i) if (p.getProgramName (i).startsWith ("GATE")) p.setCurrentProgram (i);
        auto b = play (p, host, { { 100.0, 300.0, 31 } }, 0.8);
        const double during = rms (b, 200.0, 380.0), after = rms (b, 460.0, 780.0);
        check (during > 0.05 && after < during * 0.001, "GATE: plays while the note is held, silent after FADE", "(" + String (during, 3) + " / " + String (after, 6) + ")");
    }
    // 6) MASK: offbeats only on 1/16 (steps 2 + 4)
    {
        for (int i = 0; i < p.getNumPrograms(); ++i) if (p.getProgramName (i).startsWith ("PULSE")) p.setCurrentProgram (i);
        host.bpm = 150.0;
        auto b = play (p, host, everyBeat (2, 150.0), 0.8);
        const double s1 = rms (b, 400.0, 500.0), s2 = rms (b, 500.0, 600.0), s3 = rms (b, 600.0, 700.0), s4 = rms (b, 700.0, 800.0);
        check (s2 > 0.02 && s4 > 0.02 && s1 < s2 * 0.01 && s3 < s4 * 0.01, "MASK: 1/16 steps 2 + 4 play, 1 + 3 are silent",
               String (s1, 4) + " / " + String (s2, 3) + " / " + String (s3, 4) + " / " + String (s4, 3));
    }
    // 7) the same bass at 44.1 / 48 / 96 kHz
    {
        double lv[3]; int k = 0;
        for (double rate : { 44100.0, 48000.0, 96000.0 })
        {
            sr = rate; p.setCurrentProgram (byName (p, "PUMP - QL Tail")); host.bpm = 150.0;
            lv[k++] = 20.0 * std::log10 (rms (play (p, host, everyBeat (2, 150.0), 0.8), 400.0, 800.0) + 1e-9);
        }
        sr = 48000.0;
        check (std::abs (lv[0] - lv[2]) < 1.0 && std::abs (lv[1] - lv[2]) < 1.0, "44.1 / 48 / 96 kHz: the same level", String (lv[0], 1) + " / " + String (lv[1], 1) + " / " + String (lv[2], 1) + " dB");
    }
    // 8) bit-identical renders (phase reset per step, the tail rendered the same way)
    {
        p.setCurrentProgram (byName (p, "SWELL - Synth Bend"));
        auto a = play (p, host, everyBeat (2, 150.0), 0.8), b = play (p, host, everyBeat (2, 150.0), 0.8);
        double d = 0; for (int i = 0; i < a.getNumSamples(); ++i) d += std::abs (a.getSample (0, i) - b.getSample (0, i));
        check (d == 0.0, "two renders are bit-identical", String (d, 8));
    }
    // 9) PHASE WARP: every mode clean and different from OFF; every source / pitch mode clean
    {
        int bad = 0; String why;
        auto render = [&] () { return play (p, host, everyBeat (2, 150.0), 0.8); };
        p.setCurrentProgram (byName (p, "SWELL - Synth Bend")); set (p, warp, 0.f);
        const auto off = render();
        for (int w = 1; w <= 4; ++w)
        {
            set (p, warp, (float) w); set (p, warpAmt, 0.8f);
            auto b = render();
            double d = 0; for (int i = 0; i < b.getNumSamples(); ++i) d += std::abs (b.getSample (0, i) - off.getSample (0, i));
            if (! finite (b) || b.getMagnitude (0, 0, b.getNumSamples()) > 1.f || d < 1.0) { ++bad; why << "warp " << w << " "; }
        }
        for (int pm = 0; pm < 4; ++pm) for (int src = 0; src < 2; ++src)
        {
            p.setCurrentProgram (0); set (p, source, (float) src); set (p, pitchMode, (float) pm); set (p, pitchAmt, 12.f);
            auto b = render();
            if (! finite (b) || b.getMagnitude (0, 0, b.getNumSamples()) > 1.f || rms (b, 0, 800) < 0.01) { ++bad; why << "pitch " << pm << "/" << src << " "; }
        }
        for (int t = 0; t < 5; ++t) { p.setCurrentProgram (byName (p, "SWELL - Synth Bend")); p.applyTemplate (t); auto b = render(); if (! finite (b) || rms (b, 0, 800) < 0.01) { ++bad; why << "template " << t << " "; } }
        check (bad == 0, "warp modes, pitch modes, sources, templates: clean", why);
    }
    // 10) CPU
    {
        p.setCurrentProgram (byName (p, "PUMP - QL Tail")); host.bpm = 150.0;
        auto t0 = std::chrono::high_resolution_clock::now();
        play (p, host, everyBeat (25, 150.0), 10.0);
        const double secs = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        check (secs / 10.0 < 0.15, "CPU (10 s at 150 BPM, TAIL source)", String (secs / 10.0 * 100.0, 1) + " %");
    }
    // 12) the result never depends on the host's buffer size (internal 32-sample grid): 1, 7, 64, 333, 4096 and random sizes
    {
        double worst = 0; String why;
        for (const char* pre : { "PUMP - QL Tail", "SWELL - Synth Bend", "GATE", "PULSE" })
        {
            p.setCurrentProgram (byName (p, pre)); host.bpm = 150.0;
            const bool gate = String (pre) == "GATE";
            auto notes = gate ? std::vector<Note> { { 30.0, 300.0, 31 }, { 500.0, 200.0, 34 } } : everyBeat (2, 150.0);
            const auto ref = play (p, host, notes, 0.8, 512);
            for (int bs : { 1, 7, 64, 333, 4096, -1 })
            {
                const double d = diff (ref, play (p, host, notes, 0.8, bs));
                if (d > worst) { worst = d; why = String (pre) + " @ " + String (bs); }
            }
        }
        // (block 1: the oversampler's float rounding differs by ~1e-8 = -160 dB; everything else is bit-identical)
        check (worst < 1.0e-6, "buffer size 1 / 7 / 64 / 333 / 4096 / random: the same samples", "max diff " + String (worst, 9) + (worst > 0 ? " (" + why + ")" : String()));
    }
    // 13) parameter IDs are frozen since v0.3: the layout must match Tests/param_ids.txt exactly (add new IDs at the end, never rename)
    {
        StringArray now; for (auto* prm : p.getParameters()) if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm)) now.add (rp->paramID);
        const File list = File (__FILE__).getSiblingFile ("param_ids.txt");
        if (SystemStats::getEnvironmentVariable ("RE4GE_WRITE_IDS", {}).isNotEmpty()) list.replaceWithText (now.joinIntoString ("\n") + "\n");
        StringArray frozen; frozen.addLines (list.loadFileAsString()); frozen.removeEmptyStrings();
        String missing; for (auto& id : frozen) if (! now.contains (id)) missing << id << " ";
        String extra; for (auto& id : now) if (! frozen.contains (id)) extra << id << " ";
        check (frozen.size() > 0 && missing.isEmpty() && extra.isEmpty(), "parameter IDs = Tests/param_ids.txt (frozen)",
               String (now.size()) + " IDs" + (missing.isNotEmpty() ? " MISSING: " + missing : String()) + (extra.isNotEmpty() ? " NOT IN LIST: " + extra : String()));
        int legacyOk = 0; for (auto* old : { "drive", "high_cut", "trigger", "width", "tail_highcut", "stereo_width" }) legacyOk += now.contains (ParamIDs::legacy (old)) ? 1 : 0;
        check (legacyOk == 6, "old v0.2 IDs map to v0.3 IDs (old user presets load)");
    }
    // 14b) KICK TAIL: knobs work live (PATTERN, one note for 4 beats: a TAIL DRIVE change after beat 1 is heard from beat 2 on),
    //      and no CPU spike on the note (the tail renders over the silent DELAY, not in one block)
    {
        p.setCurrentProgram (byName (p, "PUMP - QL Tail")); host.bpm = 150.0; set (p, beats, 2.f);       // BEATS 4
        p.setPlayHead (&host); p.prepareToPlay (sr, block);
        const int beatS = (int) (0.4 * sr);
        AudioBuffer<float> out (2, beatS * 4); out.clear();
        double worstMs = 0.0;
        for (int pos = 0; pos < out.getNumSamples(); pos += block)
        {
            const int n = jmin (block, out.getNumSamples() - pos);
            if (pos >= beatS && pos - block < beatS) set (p, tailDrive, 6.f);
            AudioBuffer<float> b (2, n); b.clear(); MidiBuffer m; if (pos == 0) m.addEvent (MidiMessage::noteOn (1, 31, 1.f), 0);
            const auto t0 = std::chrono::high_resolution_clock::now();
            p.processBlock (b, m);
            worstMs = jmax (worstMs, std::chrono::duration<double, std::milli> (std::chrono::high_resolution_clock::now() - t0).count());
            for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, b, ch, 0, n);
        }
        // beat 1 vs beat 3: different tails (drive 29.5 -> 6 dB)
        double d = 0; for (int i = 0; i < beatS; ++i) d += std::abs (out.getSample (0, i) - out.getSample (0, i + 2 * beatS));
        const double blockMs = 1000.0 * block / sr;
        check (d > 1.0, "KICK TAIL knobs work live (a change mid-note is heard on the next step)", "diff " + String (d, 2));
        check (worstMs < blockMs * 0.35, "KICK TAIL: no CPU spike on the note (worst block)", String (worstMs, 2) + " ms of " + String (blockMs, 1) + " ms");
    }
    // 14a) SHAPE DESIGNER: a drawn AMP curve really shapes the step (silent where the drawing is 0), saved in the state, undoable
    {
        p.setCurrentProgram (byName (p, "INIT")); host.bpm = 150.0;
        set (p, source, 0.f); set (p, laneId (0, "on"), 1.f); set (p, laneId (0, "depth"), 1.f);
        p.undo.beginNewTransaction();
        p.setShape (0, doomsday::mod::Shape::make ({ { 0.f, 0.f }, { 499.f, 0.f }, { 500.f, 1.f }, { 1000.f, 1.f } }));     // silent first half, full second
        auto b = play (p, host, everyBeat (2, 150.0), 0.8);
        const double first = rms (b, 430.0, 580.0), second = rms (b, 620.0, 780.0);
        MemoryBlock st; p.getStateInformation (st);
        p.undo.beginNewTransaction(); p.undo.undo();
        const bool undone = ! (p.getShape (0) == doomsday::mod::Shape::make ({ { 0.f, 0.f }, { 499.f, 0.f }, { 500.f, 1.f }, { 1000.f, 1.f } }));
        p.setStateInformation (st.getData(), (int) st.getSize());
        const bool restored = p.getShape (0) == doomsday::mod::Shape::make ({ { 0.f, 0.f }, { 499.f, 0.f }, { 500.f, 1.f }, { 1000.f, 1.f } });
        check (first < second * 0.02 && second > 0.05 && undone && restored, "SHAPE DESIGNER: drawn AMP plays, undo works, the curve is saved in the project",
               String (first, 4) + " / " + String (second, 3) + (undone ? "" : " NO UNDO") + (restored ? "" : " NOT RESTORED"));
    }
    // 14c) KICK-TO-REVERSE: a synthetic kick (click + pitch drop 400 -> 55 Hz + a long 55 Hz tail) is split in the right order, its note found;
    //      the sample lives inside the project (it plays after the file is gone); ORIGINAL skips the distortion
    {
        const File kf = outDir.getChildFile ("test_kick.wav");
        {
            AudioBuffer<float> k (2, (int) (sr * 0.7)); double ph = 0; Random rr (5);
            for (int i = 0; i < k.getNumSamples(); ++i)
            {
                const double t = i / sr, hz = 55.0 + 345.0 * std::exp (-t / 0.018);
                ph += hz / sr;
                float v = (float) (std::sin (6.2831853 * ph) * std::exp (-t * 4.0) * 0.8);
                if (t < 0.004) v += (rr.nextFloat() * 2.f - 1.f) * 0.6f * (float) (1.0 - t / 0.004);
                k.setSample (0, i, v); k.setSample (1, i, v);
            }
            writeWav (kf, k);
        }
        p.setCurrentProgram (byName (p, "INIT"));
        const bool loaded = p.loadSample (kf);
        const auto k = p.kickSections();
        const bool order = k.ok && k.transientEnd > 0.5 && k.transientEnd < 20.0 && k.punchEnd >= k.transientEnd && k.bodyEnd >= k.punchEnd && k.bodyEnd < 200.0 && k.tailEnd > 300.0;
        const bool note = k.rootNote == 33;           // 55 Hz = A1
        p.detectKick (0);
        const bool region = std::abs (p.apvts.getRawParameterValue (smpStart)->load() - (float) k.bodyEnd) < 1.f && p.apvts.getRawParameterValue (smpRoot)->load() == 33.f;
        check (loaded && order && note && region, "KICK-TO-REVERSE: sections in order, note found, DETECT sets the region + ROOT",
               String (k.transientEnd, 1) + " / " + String (k.punchEnd, 1) + " / " + String (k.bodyEnd, 1) + " / " + String (k.tailEnd, 1) + " ms, " + String (k.rootHz, 1) + " Hz = " + String (k.rootNote));
        // OWN SOUND: save the project, delete the file, load the project -> the same sound from the copy inside
        set (p, source, 2.f);
        const auto before = play (p, host, everyBeat (2, 150.0), 0.8);
        MemoryBlock st; p.getStateInformation (st);
        kf.deleteFile();
        ReProcessor p2; p2.setStateInformation (st.getData(), (int) st.getSize());
        const auto after = play (p2, host, everyBeat (2, 150.0), 0.8);
        check (p2.sampleIsEmbeddedOnly() && diff (before, after) < 2.0e-3 && rms (after, 400, 800) > 0.01, "the kick is saved INSIDE the project (plays without the file)",
               "max diff " + String (diff (before, after), 5));
        p.clearSample(); p.setPlayHead (&host);
    }
    // 14d) KEEP LENGTH (WSOLA grains): an octave up keeps the sample's length (VARISPEED halves it), the pitch really goes up
    {
        const File sf = outDir.getChildFile ("test_sine.wav");
        { AudioBuffer<float> k (2, (int) (sr * 0.4)); for (int i = 0; i < k.getNumSamples(); ++i) { const float v = 0.6f * (float) std::sin (6.2831853 * 55.0 * i / sr); k.setSample (0, i, v); k.setSample (1, i, v); } writeWav (sf, k); }
        auto run = [&] (bool keep)
        {
            p.setCurrentProgram (byName (p, "GATE")); host.bpm = 150.0;
            p.loadSample (sf);
            set (p, source, 2.f); set (p, smpReverse, 0.f); set (p, smpRoot, 33.f); set (p, smpStart, 0.f); set (p, smpPitch, keep ? 1.f : 0.f);
            set (p, subHz, 20.f); set (p, uniVoices, 1.f); set (p, drive, 0.f); set (p, highCut, 16000.f); set (p, swell, 2.f); set (p, smpMode, 0.f);
            return play (p, host, { { 0.0, 700.0, 45 } }, 0.8);
        };
        const auto keep = run (true), vari = run (false);
        const double keepLate = rms (keep, 270.0, 360.0), variLate = rms (vari, 270.0, 360.0), hz = domHz (keep, 60.0, 120.0);
        check (keepLate > 0.05 && variLate < keepLate * 0.1 && std::abs (hz - 110.0) < 8.0, "KEEP LENGTH: +12 st keeps the length (varispeed ends at half), the pitch doubles",
               "late rms " + String (keepLate, 3) + " vs " + String (variLate, 4) + ", " + String (hz, 1) + " Hz");
        p.clearSample();
    }
    // 14f) RANDOMIZER: 1000 random sounds (all modes / styles) - never NaN / silent / clipping, the level is matched, the low end and
    //      the late start stay; AMOUNT 0 changes nothing; LOCKS hold; one click = one UNDO step
    {
        using RZ = re4ge::Randomizer;
        p.setCurrentProgram (byName (p, "CLASSIC - Dense Rise")); host.bpm = 150.0;
        auto snapshot = [&p] { std::vector<float> v; for (auto* a : p.getParameters()) v.push_back (a->getValue()); return v; };
        auto params = [&p] { StringArray ids; for (auto* a : p.getParameters()) if (auto* w = dynamic_cast<AudioProcessorParameterWithID*> (a)) ids.add (w->paramID); return ids; };
        const auto ids = params();
        RZ::Settings st; st.amount = 0.f;
        auto before = snapshot(); p.randomizeWith (st);
        const bool amount0 = snapshot() == before;
        // locks: everything but DIST locked -> only dist* moves
        st = {}; st.mode = RZ::Full; for (auto& l : st.lock) l = true; st.lock[RZ::GDist] = false;
        before = snapshot(); p.undo.beginNewTransaction(); p.randomizeWith (st);
        auto after = snapshot(); String leaked; bool distMoved = false;
        for (int i = 0; i < ids.size(); ++i) if (after[(size_t) i] != before[(size_t) i])
        {
            if (ids[i].startsWith ("dist")) distMoved = true;
            else if (ids[i] != level) leaked << ids[i] << " ";
        }
        // undo: one step back = exactly the sound before
        p.undo.beginNewTransaction(); p.undo.undo();
        const bool undoOk = snapshot() == before;
        // 1000 random sounds
        Random rng (2026); int bad = 0, n = 0; String why; double secs = 0; float lo = 99.f, hi = -99.f;
        for (int i = 0; i < 1000; ++i)
        {
            st = {}; st.mode = rng.nextInt (RZ::NumModes); st.style = rng.nextInt (RZ::NumStyles); st.amount = rng.nextFloat() < 0.7f ? 1.f : rng.nextFloat();
            if (i % 9 == 0) p.setCurrentProgram (rng.nextInt (p.getNumPrograms()));
            const double t0 = Time::getMillisecondCounterHiRes();
            const float db = p.randomizeWith (st);
            secs += (Time::getMillisecondCounterHiRes() - t0) * 0.001;
            ++n; lo = jmin (lo, db); hi = jmax (hi, db);
            const bool rules = raw (p, lowCut) < 20.5f && raw (p, delayPct) >= 19.9f && raw (p, subHz) >= 59.f;
            if (! std::isfinite (db) || db < -24.f || db > -8.f || ! rules) { ++bad; if (why.length() < 300) why << "#" << i << " m" << st.mode << " s" << st.style << " " << String (db, 1) << "dB" << (rules ? "" : " RULES") << "; ";
                if (std::getenv ("RND_DUMP") != nullptr) { for (auto* a : p.getParameters()) if (auto* w = dynamic_cast<RangedAudioParameter*> (a)) if (std::abs (w->getValue() - w->getDefaultValue()) > 1e-4f) printf ("  %s %s\n", w->paramID.toRawUTF8(), w->getCurrentValueAsText().toRawUTF8()); printf ("dump of #%d\n", i); break; } }
            if (i % 50 == 0)
            {
                auto b = play (p, host, everyBeat (2, 150.0), 0.8);
                const double r = rms (b, 400, 800); float pk = 0.f; for (int ch = 0; ch < 2; ++ch) pk = jmax (pk, b.getMagnitude (ch, 0, b.getNumSamples()));
                if (! finite (b) || r < 0.01 || pk > 1.01f) { ++bad; if (why.length() < 300) why << "#" << i << " play rms " << String (r, 4) << " pk " << String (pk, 2) << "; "; }
            }
        }
        check (amount0 && distMoved && leaked.isEmpty() && undoOk, "RANDOMIZER: AMOUNT 0 = no change, LOCKS hold, one click = one undo",
               String (amount0 ? "" : "AMOUNT0 ") + (distMoved ? "" : "DIST STILL ") + (leaked.isEmpty() ? "" : "LEAK " + leaked) + (undoOk ? "" : "NO UNDO"));
        check (bad == 0, "RANDOMIZER: 1000 sounds clean, level matched, rules kept",
               String (lo, 1) + " .. " + String (hi, 1) + " dB, " + String (secs / n * 1000.0, 0) + " ms per click " + why);
        p.setCurrentProgram (byName (p, "CLASSIC - Dense Rise"));
        p.undo.clearUndoHistory();
    }
    // 14g) SEQUENCER + GATE: an OFF step is silent, +12 st doubles the note, LEVEL scales, SWING delays every 2nd step,
    //      the steps follow the FL grid (a note on beat 2 starts at step 5); the rhythmic GATE cuts between its hits (HIGHS keeps the lows)
    {
        auto setup = [&] (std::vector<std::pair<const char*, float>> extra)
        {
            p.setCurrentProgram (byName (p, "INIT")); host.bpm = 150.0;
            set (p, source, 0.f); set (p, step, 1.f);                         // 1/8: 2 steps per beat (200 ms each at 150 BPM)
            set (p, delayPct, 0.f); set (p, swell, 2.f); set (p, gatePct, 95.f); set (p, seqOn, 1.f);
            for (auto& [id, v] : extra) set (p, id, v);
        };
        // steps 1 / 2 / 3 / 4 = normal, OFF, +12 st, 50 %
        setup ({ { seqId (1, "on"), 0.f }, { seqId (2, "pitch"), 12.f }, { seqId (3, "vel"), 0.5f }, { seqLen, 4.f } });
        auto b = play (p, host, everyBeat (2, 150.0), 0.8);
        const double s1 = rms (b, 40, 180), s2 = rms (b, 240, 380), s4 = rms (b, 640, 780);
        const double hz1 = domHz (b, 60, 120), hz3 = domHz (b, 460, 120);
        const bool seqOk = s2 < s1 * 0.01 && std::abs (hz3 / hz1 - 2.0) < 0.15 && s4 < s1 * 0.75 && s4 > s1 * 0.3;
        // SWING: the 2nd step starts later
        setup ({ { seqSwing, 60.f } });
        auto sw = play (p, host, everyBeat (1, 150.0), 0.4);
        setup ({});
        auto st = play (p, host, everyBeat (1, 150.0), 0.4);
        const bool swingOk = rms (sw, 205, 240) < rms (st, 205, 240) * 0.3 && rms (sw, 330, 380) > 0.02;
        // FL grid: a note on beat 2 (ppq 1) starts at song step 2 (1/8) -> SEQ step 3 (+12 st) plays first
        setup ({ { seqId (2, "pitch"), 12.f }, { seqLen, 4.f } });
        host.withPpq = true;
        auto g1 = play (p, host, { { 400.0, 100.0, 31 } }, 0.8);
        host.withPpq = false;
        const double gridHz = domHz (g1, 460, 120);
        const bool gridOk = std::abs (gridHz / hz1 - 2.0) < 0.15;
        // GATE: 1/8 hard, depth 100 %, length 50 % -> silent in the second half of each 1/8 (200 ms)
        setup ({ { seqOn, 0.f }, { gateOn, 1.f }, { gateDiv, 0.f }, { gateSmooth, 1.f } });
        auto gt = play (p, host, everyBeat (1, 150.0), 0.4);
        const double open = rms (gt, 30, 90), shut = rms (gt, 120, 190);
        setup ({ { seqOn, 0.f }, { gateOn, 1.f }, { gateDiv, 0.f }, { gateSmooth, 1.f }, { gateBand, 1.f } });
        auto gh = play (p, host, everyBeat (1, 150.0), 0.4);
        const bool gateOk = shut < open * 0.02 && rms (gh, 120, 190) > open * 0.2;
        check (seqOk && swingOk && gridOk && gateOk, "SEQUENCER: off / +12 st / level / swing / FL grid; GATE cuts (HIGHS keeps the lows)",
               String (s2 / s1, 3) + " " + String (hz3 / hz1, 2) + " " + String (s4 / s1, 2) + (swingOk ? "" : " SWING") + (gridOk ? "" : " GRID " + String (gridHz, 1))
               + " gate " + String (shut / open, 3) + " / " + String (rms (gh, 120, 190) / open, 2));
    }
    // 14h) POST EQ + COMP: a LOW CUT at 300 Hz never cuts the clean SUB (the note stays), a +12 dB bell at 1 kHz is heard,
    //      the compressor makes the bass more even (lower crest factor)
    {
        auto power = [] (const AudioBuffer<float>& b, double hz, double fromMs, double toMs)
        {
            const int a0 = (int) (fromMs * 0.001 * sr), e = (int) (toMs * 0.001 * sr);
            double re = 0, im = 0;
            for (int i = a0; i < e; ++i) { const double w = 0.5 - 0.5 * std::cos (6.283185307 * (i - a0) / (e - a0)); re += b.getSample (0, i) * w * std::cos (6.283185307 * hz * i / sr); im += b.getSample (0, i) * w * std::sin (6.283185307 * hz * i / sr); }
            return 10.0 * std::log10 (re * re + im * im + 1e-20);
        };
        auto band = [&] (const AudioBuffer<float>& b, double lo, double hi) { double acc = 0; for (double f = lo; f <= hi; f += 25.0) acc += std::pow (10.0, power (b, f, 250, 390) / 10.0); return 10.0 * std::log10 (acc + 1e-20); };
        p.setCurrentProgram (byName (p, "INIT")); host.bpm = 150.0; set (p, source, 0.f);
        auto base = play (p, host, everyBeat (1, 150.0), 0.4);
        set (p, eqId (0, "on"), 1.f); set (p, eqId (0, "freq"), 300.f);
        auto cut = play (p, host, everyBeat (1, 150.0), 0.4);
        const double subBase = power (base, 49.0, 250, 390), subCut = power (cut, 49.0, 250, 390);
        const double midBase = band (base, 150, 250), midCut = band (cut, 150, 250);
        set (p, eqId (0, "on"), 0.f); set (p, eqId (3, "on"), 1.f); set (p, eqId (3, "freq"), 1000.f); set (p, eqId (3, "gain"), 12.f);
        auto bell = play (p, host, everyBeat (1, 150.0), 0.4);
        const double bellUp = band (bell, 850, 1150) - band (base, 850, 1150);
        set (p, eqId (3, "on"), 0.f);
        // the swell's range: loud end vs the quiet start (a compressor makes it smaller)
        auto crest = [] (const AudioBuffer<float>& b) { return 20.0 * std::log10 (rms (b, 210, 280) / jmax (1e-9, rms (b, 150, 175))); };
        set (p, compOn, 1.f); set (p, compThresh, -30.f); set (p, compRatio, 8.f); set (p, compAttack, 0.5f);
        auto cmp = play (p, host, everyBeat (1, 150.0), 0.4);
        const bool ok3 = std::abs (subCut - subBase) < 1.5 && midCut < midBase - 6.0 && bellUp > 6.0 && crest (cmp) < crest (base) - 2.0 && finite (cmp);
        check (ok3, "POST EQ: LOW CUT keeps the SUB, the bell is heard; COMP evens the swell",
               "sub " + String (subCut - subBase, 1) + " dB, 150-250 Hz " + String (midCut - midBase, 1) + " dB, bell +" + String (bellUp, 1) + " dB, swell range " + String (crest (base), 1) + " -> " + String (crest (cmp), 1));
    }
    // 14i) EXPORT: the current bass -> a 24-bit WAV (2 beats at the FL tempo, the note asked), it plays, it is the right length;
    //      every factory preset has a category
    {
        p.setCurrentProgram (byName (p, "CLASSIC - Dense Rise")); host.bpm = 150.0;
        play (p, host, everyBeat (1, 150.0), 0.1);                              // the engine knows the tempo
        auto job = p.makeExportJob (31, 2);
        job.name = "RenderTest export";
        const File f = re4ge::Exporter::run (job);
        bool ok4 = f.existsAsFile();
        double len = 0, lvl = 0;
        if (ok4)
        {
            WavAudioFormat wav;
            std::unique_ptr<AudioFormatReader> r (wav.createReaderFor (new FileInputStream (f), true));
            ok4 = r != nullptr && r->bitsPerSample == 24;
            if (ok4)
            {
                AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples); r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
                len = r->lengthInSamples / r->sampleRate; lvl = b.getRMSLevel (0, 0, b.getNumSamples());
            }
            f.deleteFile();
        }
        int noCat = 0; for (int i = 0; i < p.getNumPrograms(); ++i) if (ReProcessor::presetCategory (p.getProgramName (i)) == "OTHER") ++noCat;
        check (ok4 && std::abs (len - 0.8) < 0.01 && lvl > 0.02 && noCat == 0, "EXPORT: 2 beats -> a 24-bit WAV that plays; presets have categories",
               String (len, 3) + " s, rms " + String (lvl, 3) + ", " + String (noCat) + " presets without a category");
    }
    // 14j) MORPH A / B: A = the note, B = +12 st; MORPH 0 / 50 / 100 % = 1x / ~1.41x / 2x the pitch; the slots are saved in the project;
    //      a click on A loads A back (undoable)
    {
        p.setCurrentProgram (byName (p, "INIT")); host.bpm = 150.0; set (p, source, 0.f);
        p.morphReset(); p.morphSelect (0); p.morphSelect (1);         // A = the current sound, B = a copy that is edited now
        set (p, tune, 12.f); p.morphSync();
        set (p, morphOn, 1.f);
        // the morphed sound = the sound with the knob set to the middle / end by hand
        double hz[3];
        for (int k = 0; k < 3; ++k)
        {
            set (p, morph, 0.5f * k); auto m = play (p, host, everyBeat (1, 150.0), 0.4);
            set (p, morphOn, 0.f); const float keep = raw (p, tune); set (p, tune, 6.f * k); auto h = play (p, host, everyBeat (1, 150.0), 0.4);
            set (p, tune, keep); set (p, morphOn, 1.f);
            hz[k] = diff (m, h);
        }
        MemoryBlock st; p.getStateInformation (st);
        ReProcessor p2; p2.setStateInformation (st.getData(), (int) st.getSize());
        const bool saved = p2.morphHas (0) && p2.morphHas (1) && p2.morphActive() == 1;
        set (p, morphOn, 0.f);
        p.undo.beginNewTransaction(); p.morphSelect (0);
        const bool backToA = std::abs (raw (p, tune)) < 0.01f;
        p.undo.undo();
        const bool undoOk = std::abs (raw (p, tune) - 12.f) < 0.01f;
        // a CHOICE never passes the options in between: PITCH STABLE (A) -> BOUNCE (B) at 25 % plays STABLE, at 75 % BOUNCE
        set (p, morphOn, 0.f); p.morphSelect (1); set (p, pitchMode, 3.f); p.morphSync(); p.morphSelect (0); set (p, pitchMode, 0.f); p.morphSync(); set (p, morphOn, 1.f);
        set (p, morph, 0.25f); const auto c1 = p.gather().pitchMode; set (p, morph, 0.75f); const auto c2 = p.gather().pitchMode;
        set (p, morph, 0.f); set (p, morphOn, 0.f); p.morphSelect (1); set (p, pitchMode, 0.f); p.morphSync(); set (p, morphOn, 1.f);
        if (c1 != 0 || c2 != 3) hz[1] = 1.0 + c1 * 10 + c2;
        const bool okM = hz[0] < 1.0e-4 && hz[1] < 1.0e-4 && hz[2] < 1.0e-4 && saved && backToA && undoOk;
        check (okM, "MORPH A / B: 0 / 50 / 100 % = TUNE 0 / +6 / +12 st, saved in the project, A / B undoable",
               "diff " + String (hz[0], 6) + " / " + String (hz[1], 6) + " / " + String (hz[2], 6) + (saved ? "" : " NOT SAVED") + (backToA ? "" : " NO LOAD") + (undoOk ? "" : " NO UNDO"));
        p.morphReset();
    }
    // 14k) REFERENCE ANALYZER: the sound vs itself = "close"; vs a darker copy = it names PRESENCE / AIR; a later start is seen
    {
        p.setCurrentProgram (byName (p, "CLASSIC - Dense Rise")); host.bpm = 150.0;
        play (p, host, everyBeat (1, 150.0), 0.1);
        auto prof = [&p] { auto j = p.makeExportJob (31, 1); const auto b = re4ge::Exporter::render (j); std::vector<float> m ((size_t) b.getNumSamples());
                           for (int i = 0; i < b.getNumSamples(); ++i) m[(size_t) i] = 0.5f * (b.getSample (0, i) + b.getSample (1, i));
                           return re4ge::RefAnalysis::analyse (m.data(), (int) m.size(), j.rate, (int) m.size()); };
        const auto a = prof();
        const auto same = re4ge::RefAnalysis::hints (a, prof());
        set (p, highCut, 900.f); const auto dark = prof(); set (p, highCut, 7000.f);
        const auto darkHints = re4ge::RefAnalysis::hints (a, dark);
        set (p, delayPct, 55.f); const auto late = prof();
        const auto lateHints = re4ge::RefAnalysis::hints (a, late);
        const bool okR = a.valid && same.size() == 1 && same[0].startsWith ("Close") && darkHints.joinIntoString (" ").contains ("less")
                         && (darkHints.joinIntoString (" ").contains ("PRESENCE") || darkHints.joinIntoString (" ").contains ("AIR"))
                         && lateHints.joinIntoString (" ").contains ("LATER") && a.peakPct > a.startPct;
        check (okR, "REFERENCE: itself = close, darker = less PRESENCE / AIR, later DELAY = starts LATER",
               "peak " + String (a.peakPct, 0) + " %, start " + String (a.startPct, 0) + " % | " + darkHints.joinIntoString (" | ").substring (0, 120) + " | " + lateHints.joinIntoString (" | ").substring (0, 80));
    }
    // 14e) RE4GE FX: kicks in the INPUT are found (one hit each, on time), the dry kick passes untouched, the bass sounds between them;
    //      LIVE: the previous kick reversed (no bass after the first kick, bass after the second)
    {
        ReProcessor fx (true);
        fx.setPlayHead (&host); host.bpm = 150.0;
        fx.setCurrentProgram (byName (fx, "CLASSIC - Dense Rise"));
        const int beat = (int) (0.4 * sr), N = beat * 4;
        AudioBuffer<float> in (2, N); in.clear();
        for (int b = 0; b < 4; ++b) { double ph = 0; for (int i = 0; i < beat; ++i) { const double t = i / sr; ph += (52.0 + 300.0 * std::exp (-t / 0.02)) / sr; const float v = (float) (std::tanh (3.0 * std::sin (6.2831853 * ph)) * std::exp (-t * 9.0) * 0.8); in.setSample (0, b * beat + i, v); in.setSample (1, b * beat + i, v); } }
        auto runFx = [&] (bool live, bool dryOn)
        {
            if (auto* a = fx.apvts.getParameter (ParamIDs::fxLive)) a->setValueNotifyingHost (live ? 1.f : 0.f);
            if (auto* a = fx.apvts.getParameter (ParamIDs::fxDry)) a->setValueNotifyingHost (dryOn ? 1.f : 0.f);
            set (fx, source, live ? 2.f : 0.f);
            fx.prepareToPlay (sr, block);
            const int h0 = fx.fxHits.load();
            AudioBuffer<float> out (2, N);
            for (int pos = 0; pos < N; pos += block)
            {
                const int n = jmin (block, N - pos);
                AudioBuffer<float> b (4, n); b.clear(); MidiBuffer m;
                for (int ch = 0; ch < 2; ++ch) b.copyFrom (ch, 0, in, ch, pos, n);
                fx.processBlock (b, m);
                for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, b, ch, 0, n);
            }
            return std::make_pair (out, fx.fxHits.load() - h0);
        };
        auto [wet, hits] = runFx (false, false);
        auto [withDry, hits2] = runFx (false, true);
        AudioBuffer<float> diffB (2, N); for (int ch = 0; ch < 2; ++ch) { diffB.copyFrom (ch, 0, withDry, ch, 0, N); diffB.addFrom (ch, 0, wet, ch, 0, N, -1.f); }
        const double dryErr = diff (diffB, in);
        const double between = rms (wet, 400 + 200, 400 + 360), onKick = rms (wet, 400, 400 + 40);
        check (hits == 4 && hits2 == 4 && dryErr < 1.0e-5 && between > 0.03 && onKick < between * 0.3,
               "RE4GE FX: 4 kicks found, the dry kick passes untouched, the bass sounds between the kicks",
               String (hits) + " hits, dry err " + String (dryErr, 7) + ", bass " + String (between, 3) + " / on the kick " + String (onKick, 4));
        fx.setCurrentProgram (byName (fx, "INIT"));            // (no OSC 2-4: only the SAMPLE source sounds)
        auto [liveOut, hits3] = runFx (true, false);
        const double firstBeat = rms (liveOut, 100, 380), secondBeat = rms (liveOut, 500, 780);
        check (hits3 == 4 && firstBeat < 1.0e-4 && secondBeat > 0.02, "RE4GE FX LIVE: no bass after the first kick, the previous kick reversed after the next",
               String (firstBeat, 5) + " / " + String (secondBeat, 4));
        {   // the FX page
            std::unique_ptr<AudioProcessorEditor> ed (fx.createEditor()); ed->setSize (Canvas::W, Canvas::H);
            dynamic_cast<ReEditor&> (*ed).getCanvas().showPageForTest (18); dynamic_cast<ReEditor&> (*ed).getCanvas().tickForTest();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.f);
            PNGImageFormat png; FileOutputStream fo (outDir.getChildFile ("gui_fx.png")); fo.setPosition (0); fo.truncate(); png.writeImageToStream (img, fo);
        }
        fx.setPlayHead (nullptr);
    }
    // 14) every knob changes the sound: min vs max, in at least one of the contexts where it matters
    {
        // a short reversed-kick-like WAV for the SAMPLE source
        const File wav = outDir.getChildFile ("test_sample.wav");
        {
            AudioBuffer<float> k (2, (int) (sr * 0.6)); double ph = 0;
            for (int i = 0; i < k.getNumSamples(); ++i) { const double t = i / sr; ph += (50.0 + 250.0 * std::exp (-t * 25.0)) / sr; const float v = (float) (std::tanh (3.0 * std::sin (6.2831853 * ph)) * std::exp (-t * 3.0)) * 0.7f; k.setSample (0, i, v); k.setSample (1, i, v); }
            writeWav (wav, k);
        }
        struct Ctx { const char* preset; std::vector<std::pair<const char*, float>> extra; bool sample = false; bool gate = false; bool offline = false; double bpm = 150.0; bool morphAB = false; };
        const std::vector<Ctx> ctxs {
            { "PUMP - QL Tail", {} },
            { "SWELL - Synth Bend", {} },
            { "OWNER - 1", {} },
            { "INIT", { { source, 0.f }, { warp, 0.f } } },
            { "INIT", { { source, 2.f } }, true },
            { "INIT", { { timing, 1.f } } },
            { "INIT", { { clickLevel, -6.f }, { smear, 0.6f } } },
            { "INIT", { { source, 0.f }, { labId (0, "lab"), 6.f }, { labId (0, "tone"), 0.6f }, { dist2Drive, 12.f }, { labId (1, "lab"), 6.f }, { labId (1, "tone"), 0.6f }, { dist2Bias, 0.3f } } },
            { "INIT", { { source, 0.f }, { labId (0, "lab"), 1.f }, { labId (1, "lab"), 1.f }, { dist2Drive, 12.f } } },
            { "INIT", { { source, 0.f } }, false, false, true },
            { "INIT", { { source, 0.f }, { warp, 1.f }, { warpAmt, 0.6f }, { preType, 1.f }, { preHz, 1500.f }, { oscId (2, "level"), -6.f }, { oscId (2, "pm"), 0.6f },
                        { laneId (0, "on"), 1.f }, { laneId (1, "on"), 1.f }, { laneId (2, "on"), 1.f }, { laneId (3, "on"), 1.f },
                        { laneId (0, "depth"), 0.7f }, { laneId (2, "depth"), 0.7f }, { laneId (3, "depth"), 0.7f },
                        { laneId (0, "len"), 50.f }, { laneId (1, "len"), 50.f }, { laneId (2, "len"), 50.f }, { laneId (3, "len"), 50.f } } },
            { "INIT", { { clickLevel, -6.f }, { clickAt, 1.f }, { swell, 120.f } } },
            { "INIT", { { timing, 1.f }, { sustain, 0.f }, { decayMs, 150.f }, { holdMs, 50.f } } },
            { "INIT", { { keyTrack, 0.f }, { pitchMode, 3.f } } },
            { "PULSE", {} },
            { "INIT", { { source, 0.f }, { preType, 1.f }, { preHz, 1200.f }, { oscId (2, "level"), -6.f }, { dist2Drive, 6.f }, { clickLevel, -12.f }, { smear, 0.3f },
                        { lfoId (0, "sync"), 0.f }, { lfoId (0, "rate"), 7.f }, { lfoId (1, "wave"), 4.f }, { lfoId (2, "wave"), 5.f }, { lfoId (3, "wave"), 1.f }, { lfoId (3, "retrig"), 0.f },
                        { lfoId (1, "phase"), 0.3f }, { lfoId (2, "div"), 3.f }, { lfoId (3, "phase"), 0.25f },
                        { mtxId (0, "on"), 1.f }, { mtxId (0, "src"), 1.f }, { mtxId (0, "dst"), 7.f }, { mtxId (0, "amt"), 0.5f },
                        { mtxId (1, "on"), 1.f }, { mtxId (1, "src"), 2.f }, { mtxId (1, "dst"), 9.f }, { mtxId (1, "amt"), 0.5f },
                        { mtxId (2, "on"), 1.f }, { mtxId (2, "src"), 3.f }, { mtxId (2, "dst"), 2.f }, { mtxId (2, "amt"), 0.5f },
                        { mtxId (3, "on"), 1.f }, { mtxId (3, "src"), 4.f }, { mtxId (3, "dst"), 5.f }, { mtxId (3, "amt"), 0.3f } } },
            // the same LFOs with the other SYNC setting (RATE vs DIVISION) and smooth waves (PHASE)
            { "INIT", { { source, 0.f }, { preType, 1.f }, { preHz, 1200.f }, { oscId (2, "level"), -6.f },
                        { lfoId (0, "sync"), 1.f }, { lfoId (0, "div"), 6.f }, { lfoId (1, "sync"), 0.f }, { lfoId (1, "rate"), 5.f }, { lfoId (2, "sync"), 0.f }, { lfoId (2, "rate"), 6.f },
                        { lfoId (3, "sync"), 0.f }, { lfoId (3, "rate"), 9.f },
                        { mtxId (0, "on"), 1.f }, { mtxId (0, "src"), 1.f }, { mtxId (0, "dst"), 7.f }, { mtxId (0, "amt"), 0.5f },
                        { mtxId (1, "on"), 1.f }, { mtxId (1, "src"), 2.f }, { mtxId (1, "dst"), 9.f }, { mtxId (1, "amt"), 0.5f },
                        { mtxId (2, "on"), 1.f }, { mtxId (2, "src"), 3.f }, { mtxId (2, "dst"), 2.f }, { mtxId (2, "amt"), 0.5f },
                        { mtxId (3, "on"), 1.f }, { mtxId (3, "src"), 4.f }, { mtxId (3, "dst"), 5.f }, { mtxId (3, "amt"), 0.3f } } },
            { "INIT", { { source, 0.f }, { beats, 1.f }, { preType, 1.f }, { mtxId (0, "on"), 1.f }, { mtxId (0, "src"), 22.f }, { mtxId (0, "dst"), 7.f }, { mtxId (0, "amt"), 0.6f }, { mtxId (0, "pol"), 1.f } } },
            { "INIT", { { source, 0.f }, { beats, 1.f }, { preType, 1.f }, { mtxId (1, "on"), 1.f }, { mtxId (1, "src"), 22.f }, { mtxId (1, "dst"), 7.f }, { mtxId (1, "amt"), 0.6f }, { mtxId (1, "pol"), 1.f } } },
            { "INIT", { { source, 0.f }, { beats, 1.f }, { preType, 1.f }, { mtxId (2, "on"), 1.f }, { mtxId (2, "src"), 22.f }, { mtxId (2, "dst"), 7.f }, { mtxId (2, "amt"), 0.6f }, { mtxId (2, "pol"), 1.f } } },
            { "INIT", { { source, 0.f }, { beats, 1.f }, { preType, 1.f }, { mtxId (3, "on"), 1.f }, { mtxId (3, "src"), 22.f }, { mtxId (3, "dst"), 7.f }, { mtxId (3, "amt"), 0.6f }, { mtxId (3, "pol"), 1.f } } },
            { "INIT", { { source, 0.f }, { beats, 1.f }, { preType, 1.f }, { mtxId (4, "on"), 1.f }, { mtxId (4, "src"), 22.f }, { mtxId (4, "dst"), 7.f }, { mtxId (4, "amt"), 0.6f }, { mtxId (4, "pol"), 1.f } } },
            { "INIT", { { source, 0.f }, { beats, 1.f }, { preType, 1.f }, { mtxId (5, "on"), 1.f }, { mtxId (5, "src"), 22.f }, { mtxId (5, "dst"), 7.f }, { mtxId (5, "amt"), 0.6f }, { mtxId (5, "pol"), 1.f } } },
            { "INIT", { { source, 0.f }, { beats, 1.f }, { preType, 1.f }, { mtxId (6, "on"), 1.f }, { mtxId (6, "src"), 22.f }, { mtxId (6, "dst"), 7.f }, { mtxId (6, "amt"), 0.6f }, { mtxId (6, "pol"), 1.f } } },
            { "INIT", { { source, 0.f }, { beats, 1.f }, { preType, 1.f }, { mtxId (7, "on"), 1.f }, { mtxId (7, "src"), 22.f }, { mtxId (7, "dst"), 7.f }, { mtxId (7, "amt"), 0.6f }, { mtxId (7, "pol"), 1.f } } },
            { "INIT", { { source, 0.f }, { beats, 1.f }, { preType, 1.f }, { mtxId (8, "on"), 1.f }, { mtxId (8, "src"), 22.f }, { mtxId (8, "dst"), 7.f }, { mtxId (8, "amt"), 0.6f }, { mtxId (8, "pol"), 1.f } } },
            { "INIT", { { source, 0.f }, { beats, 1.f }, { preType, 1.f }, { mtxId (9, "on"), 1.f }, { mtxId (9, "src"), 22.f }, { mtxId (9, "dst"), 7.f }, { mtxId (9, "amt"), 0.6f }, { mtxId (9, "pol"), 1.f } } },
            { "INIT", { { source, 0.f }, { beats, 1.f }, { preType, 1.f }, { mtxId (10, "on"), 1.f }, { mtxId (10, "src"), 22.f }, { mtxId (10, "dst"), 7.f }, { mtxId (10, "amt"), 0.6f }, { mtxId (10, "pol"), 1.f } } },
            { "INIT", { { source, 0.f }, { beats, 1.f }, { preType, 1.f }, { mtxId (11, "on"), 1.f }, { mtxId (11, "src"), 22.f }, { mtxId (11, "dst"), 7.f }, { mtxId (11, "amt"), 0.6f }, { mtxId (11, "pol"), 1.f } } },
            { "INIT", { { source, 0.f }, { beats, 1.f }, { preType, 1.f }, { mtxId (12, "on"), 1.f }, { mtxId (12, "src"), 22.f }, { mtxId (12, "dst"), 7.f }, { mtxId (12, "amt"), 0.6f }, { mtxId (12, "pol"), 1.f } } },
            { "INIT", { { source, 0.f }, { beats, 1.f }, { preType, 1.f }, { mtxId (13, "on"), 1.f }, { mtxId (13, "src"), 22.f }, { mtxId (13, "dst"), 7.f }, { mtxId (13, "amt"), 0.6f }, { mtxId (13, "pol"), 1.f } } },
            { "INIT", { { source, 0.f }, { beats, 1.f }, { preType, 1.f }, { mtxId (14, "on"), 1.f }, { mtxId (14, "src"), 22.f }, { mtxId (14, "dst"), 7.f }, { mtxId (14, "amt"), 0.6f }, { mtxId (14, "pol"), 1.f } } },
            { "INIT", { { source, 0.f }, { beats, 1.f }, { preType, 1.f }, { mtxId (15, "on"), 1.f }, { mtxId (15, "src"), 22.f }, { mtxId (15, "dst"), 7.f }, { mtxId (15, "amt"), 0.6f }, { mtxId (15, "pol"), 1.f } } },

            { "INIT", { { source, 0.f }, { dist2Drive, 14.f }, { dist2Type, 2.f },
                        { oscId (2, "level"), 0.f }, { oscId (2, "wave"), 4.f }, { oscId (2, "semi"), 7.f },
                        { oscId (3, "level"), 0.f }, { oscId (3, "wave"), 4.f }, { oscId (3, "semi"), 5.f },
                        { oscId (4, "level"), 0.f }, { oscId (4, "wave"), 4.f }, { oscId (4, "semi"), 3.f } } },
            { "GATE", {}, false, true },
            { "GATE", { { source, 1.f } }, false, true },
            // SEQUENCER: 16 steps of 1/16 in 0.8 s at 300 BPM; every step different (level / pitch) so LENGTH and each step matter
            { "INIT", { { source, 0.f }, { step, 2.f }, { beats, 3.f }, { seqOn, 1.f }, { seqSwing, 30.f }, { seqId (0, "pitch"), 5.f }, { seqId (1, "vel"), 0.5f } }, false, false, false, 300.0 },
            // GATE (rhythm): on, every knob in play; HIGHS band separately
            { "INIT", { { source, 0.f }, { gateOn, 1.f }, { gateDepth, 0.8f }, { gateShape, 1.f } } },
            { "INIT", { { source, 0.f }, { gateOn, 1.f }, { gateBand, 1.f }, { gateDiv, 2.f } } },
            // POST EQ: every band on with its default type (cuts / shelves / bells), then every band a bell with gain
            { "INIT", { { source, 0.f }, { eqId (0, "on"), 1.f }, { eqId (1, "on"), 1.f }, { eqId (2, "on"), 1.f }, { eqId (3, "on"), 1.f },
                        { eqId (4, "on"), 1.f }, { eqId (5, "on"), 1.f }, { eqId (6, "on"), 1.f }, { eqId (7, "on"), 1.f },
                        { eqId (1, "gain"), 6.f }, { eqId (2, "gain"), 6.f }, { eqId (3, "gain"), 6.f }, { eqId (4, "gain"), 6.f }, { eqId (5, "gain"), 6.f }, { eqId (6, "gain"), 6.f },
                        { eqId (0, "freq"), 200.f }, { eqId (7, "freq"), 3000.f } } },
            { "INIT", { { source, 0.f }, { eqId (0, "on"), 1.f }, { eqId (0, "type"), 2.f }, { eqId (0, "gain"), 9.f }, { eqId (0, "freq"), 400.f },
                        { eqId (7, "on"), 1.f }, { eqId (7, "type"), 2.f }, { eqId (7, "gain"), 9.f }, { eqId (7, "freq"), 3000.f } } },
            // COMP: well over the threshold so every knob is heard
            { "INIT", { { source, 0.f }, { compOn, 1.f }, { compThresh, -30.f }, { compRatio, 6.f }, { compMakeup, 6.f }, { compMix, 0.7f } } },
            // MORPH: A = INIT synth, B = the same with +12 st and more drive
            { "INIT", { { source, 0.f } }, false, false, false, 150.0, true } };
        String dead; int tested = 0;
        for (auto* prm : p.getParameters())
        {
            auto* rp = dynamic_cast<RangedAudioParameter*> (prm); if (rp == nullptr) continue;
            ++tested;
            bool moves = false;
            for (auto& cx : ctxs)
            {
                p.setCurrentProgram (byName (p, cx.preset)); host.bpm = cx.bpm;
                for (auto& [id, v] : cx.extra) set (p, id, v);
                if (cx.morphAB)
                {
                    const float keep = rp->getValue();
                    p.morphReset(); p.morphSelect (0); p.morphSelect (1); set (p, tune, 12.f); set (p, drive, 24.f); p.morphSync(); set (p, morphOn, 1.f); set (p, morph, 0.5f);
                    rp->setValueNotifyingHost (keep);
                }
                if (cx.sample) p.loadSample (wav); else p.clearSample();
                p.setNonRealtime (cx.offline);
                auto notes = cx.gate ? std::vector<Note> { { 30.0, 500.0, 31, 0.6f }, { 300.0, 400.0, 38, 0.6f } } : std::vector<Note> { { 0.0, 100.0, 31, 0.6f } };
                rp->setValueNotifyingHost (0.f); const auto a = play (p, host, notes, 0.8);
                rp->setValueNotifyingHost (1.f); const auto b = play (p, host, notes, 0.8);
                if (diff (a, b) > 1.0e-4) { moves = true; break; }
                rp->setValueNotifyingHost (0.5f);                                   // circular knobs (PHASE 0 = 360): min vs the middle
                if (diff (a, play (p, host, notes, 0.8)) > 1.0e-4) { moves = true; break; }
            }
            if (! moves) dead << rp->paramID << " ";
        }
        p.clearSample(); p.setNonRealtime (false); host.bpm = 150.0; p.morphReset();
        check (dead.isEmpty(), "every parameter changes the sound (min vs max)", String (tested) + " params" + (dead.isNotEmpty() ? " DEAD: " + dead : String()));
    }
    // 15) GUI: editor stress + snapshots of the three pages
    {
        p.setCurrentProgram (byName (p, "PUMP - QL Tail"));
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (Canvas::W, Canvas::H);
        Random r (3);
        for (int it = 0; it < 150; ++it)
        {
            auto* prm = p.getParameters()[r.nextInt (p.getParameters().size())];
            prm->setValueNotifyingHost (r.nextFloat());
            AudioBuffer<float> b (2, 512); b.clear(); MidiBuffer m; if (it % 9 == 0) m.addEvent (MidiMessage::noteOn (1, 31, 1.f), 0);
            p.processBlock (b, m);
            dynamic_cast<ReEditor&> (*ed).getCanvas().tickForTest();
            if (it % 10 == 0) ed->createComponentSnapshot (ed->getLocalBounds(), true, 0.5f);
        }
        p.setCurrentProgram (byName (p, "CLASSIC - Dense Rise"));
        play (p, host, everyBeat (1, 150.0), 0.3);
        {   // a kick for the KICK-TO-REVERSE page
            const File kf = outDir.getChildFile ("gui_kick.wav");
            AudioBuffer<float> k (2, (int) (sr * 0.6)); double ph = 0;
            for (int i = 0; i < k.getNumSamples(); ++i) { const double t = i / sr; ph += (52.0 + 300.0 * std::exp (-t / 0.02)) / sr; const float v = (float) (std::tanh (4.0 * std::sin (6.2831853 * ph)) * std::exp (-t * 5.0) * 0.8); k.setSample (0, i, v); k.setSample (1, i, v); }
            writeWav (kf, k); p.loadSample (kf); p.detectKick (0);
        }
        const std::pair<int, const char*> pages[] { { 0, "gui_main.png" }, { 1, "gui_osc.png" }, { 3, "gui_envelope.png" }, { 4, "gui_shape.png" },
                                                    { 5, "gui_filter.png" }, { 6, "gui_dist.png" }, { 8, "gui_tail.png" }, { 10, "gui_sub.png" }, { 11, "gui_stereo.png" }, { 13, "gui_designer.png" }, { 14, "gui_waveshaper.png" }, { 15, "gui_settings.png" }, { 16, "gui_lfo.png" }, { 17, "gui_matrix.png" }, { 9, "gui_kick.png" }, { 19, "gui_random.png" }, { 20, "gui_seq.png" }, { 21, "gui_gate.png" }, { 7, "gui_eq.png" }, { 22, "gui_dyn.png" }, { 23, "gui_export.png" } };
        for (auto [pg, name] : pages)
        {
            dynamic_cast<ReEditor&> (*ed).getCanvas().showPageForTest (pg);
            dynamic_cast<ReEditor&> (*ed).getCanvas().tickForTest();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.f);
            PNGImageFormat png; FileOutputStream fo (outDir.getChildFile (name)); fo.setPosition (0); fo.truncate(); png.writeImageToStream (img, fo);
        }
        {   // the REFERENCE page with a reference (the Dense Rise rendered darker) and the plugin's own sound
            auto j = p.makeExportJob (31, 1); j.name = "gui_reference"; j.params.highCutHz = 1500.f; j.params.delayPct = 40.f;
            const File rf = re4ge::Exporter::run (j);
            auto& cv = dynamic_cast<ReEditor&> (*ed).getCanvas();
            cv.showPageForTest (24); cv.refPanelForTest().loadForTest (rf); cv.refPanelForTest().analyseMineForTest(); cv.tickForTest();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.f);
            PNGImageFormat png; FileOutputStream fo (outDir.getChildFile ("gui_reference.png")); fo.setPosition (0); fo.truncate(); png.writeImageToStream (img, fo);
            rf.deleteFile();
        }
        p.setPlayHead (nullptr);
    }
    printf ("%s\n", ok ? "ALL OK" : "SOME FAILED");
    return ok ? 0 : 1;
}
