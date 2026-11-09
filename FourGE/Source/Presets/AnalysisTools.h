#pragma once
// =====================================================================
//  4GE module 16 - ANALYSIS TOOLS (message / background thread: files + suggestions, no DSP code, no GUI)
//   loadReference: a WAV / AIFF / FLAC / MP3 (the first 10 s, stereo kept).
//   suggest: the reference vs the 4GE kick -> a list of "what to change" (pitch start / end / drop / tok, length, punch,
//   sub / body / mid / air, density, width, level) with the exact new values. NOTHING changes until APPLY (one UNDO step).
// =====================================================================
#include <juce_audio_formats/juce_audio_formats.h>
#include "../PluginProcessor.h"
#include "../DSP/Analysis/KickAnalysis.h"
#include "../DSP/Export/Render.h"

namespace fge::analysis
{
    namespace ka = fge::dsp::analysis;
    inline bool loadReference (const juce::File& f, juce::AudioBuffer<float>& out, double& rate, juce::String& err)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
        if (r == nullptr) { err = "not an audio file: " + f.getFileName(); return false; }
        const int n = (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (10.0 * r->sampleRate));
        if (n < 64) { err = "the file is empty"; return false; }
        out.setSize (2, n); out.clear();
        juce::AudioBuffer<float> tmp ((int) juce::jmax (1u, r->numChannels), n);
        r->read (&tmp, 0, n, 0, true, true);
        out.copyFrom (0, 0, tmp, 0, 0, n);
        out.copyFrom (1, 0, tmp, tmp.getNumChannels() > 1 ? 1 : 0, 0, n);
        // start at the hit (skip leading silence up to 1 s)
        int a = 0; while (a < juce::jmin (n - 64, (int) r->sampleRate) && juce::jmax (std::abs (out.getSample (0, a)), std::abs (out.getSample (1, a))) < 0.003f) ++a;
        if (a > 0) { juce::AudioBuffer<float> t (2, n - a); for (int c = 0; c < 2; ++c) t.copyFrom (c, 0, out, c, a, n - a); out = std::move (t); }
        // and end after the hit (trailing silence away, 50 ms kept: the waveform / pitch views show the kick, not the silence)
        int e = out.getNumSamples(); while (e > 64 && juce::jmax (std::abs (out.getSample (0, e - 1)), std::abs (out.getSample (1, e - 1))) < 0.0005f) --e;
        e = juce::jmin (out.getNumSamples(), e + (int) (0.05 * r->sampleRate));
        if (e < out.getNumSamples()) out.setSize (2, e, true);
        rate = r->sampleRate;
        return true;
    }

    // the 4GE kick as the export renders it (background thread: a SNAPSHOT, full kick, AUTO length, the master output included)
    inline juce::AudioBuffer<float> renderMine (const fge::dsp::xport::Snapshot& snap, int note, double bpm, const std::atomic<bool>* cancel = nullptr)
    {
        fge::dsp::xport::Spec s;
        s.note = juce::jlimit (0, 127, note); s.bpm = bpm > 0.0 ? bpm : 150.0; s.rate = 44100.0; s.seed = snap.presetSeed;
        s.lengthMode = fge::dsp::xport::LenAuto; s.maxMs = 4000.f;
        return std::move (fge::dsp::xport::render (snap, s, cancel).audio);
    }

    struct Change { juce::String id; float value; };
    struct Suggestion { juce::String text; std::vector<Change> changes; };   // no changes = information only

    inline std::vector<Suggestion> suggest (const ka::Report& ref, const ka::Report& me, const ParamFrame& f)
    {
        std::vector<Suggestion> out;
        if (! ref.valid || ! me.valid) return out;
        auto lim = [] (int idx, float v) { return juce::jlimit (def (idx).min, def (idx).max, v); };
        auto st = [] (float a, float b) { return a > 0.f && b > 0.f ? 12.f * std::log2 (a / b) : 0.f; };
        // pitch: the tail note
        if (ref.endHz > 0.f && me.endHz > 0.f && std::abs (st (ref.endHz, me.endHz)) >= 0.5f)
        {
            if (f.on (P::coreKeyTrack))
                out.push_back ({ "The reference sits on " + ka::noteName (ref.endNote) + " (" + juce::String (ref.endHz, 1) + " Hz), your kick on " + ka::noteName (me.endNote)
                                 + " - play the kick on " + ka::noteName (ref.endNote) + " in the piano roll (PITCH TRACKING is on)", {} });
            else
                out.push_back ({ "Tail note " + ka::noteName (me.endNote) + " -> " + ka::noteName (ref.endNote) + ": END " + juce::String (ref.endHz, 1) + " Hz",
                                 { { "pe_end", lim (P::peEnd, ref.endHz) } } });
        }
        if (ref.startHz > 0.f && me.startHz > 0.f && std::abs (st (ref.startHz, me.startHz)) > 2.f)
        {
            const float v = lim (P::peStart, f[P::peStart] * juce::jlimit (0.5f, 2.f, ref.startHz / me.startHz));   // (one octave per click at most: measure again)
            out.push_back ({ "Pitch drop starts " + juce::String (std::abs (st (ref.startHz, me.startHz)), 1) + " st " + (ref.startHz > me.startHz ? "higher" : "lower")
                             + " in the reference: START " + juce::String (juce::roundToInt (f[P::peStart])) + " -> " + juce::String (juce::roundToInt (v)) + " Hz", { { "pe_start", v } } });
        }
        // the drop itself (after the held tok)
        // (the LENGTH suggestion below turns AUTO LENGTH on: the drop then scales with the length, so compare with the scaled drop)
        const bool lenChange = ref.lengthMs > 0.f && me.lengthMs > 0.f && std::abs (ref.lengthMs / me.lengthMs - 1.f) > 0.15f;
        const float lenK = lenChange ? juce::jlimit (0.25f, 4.f, ref.lengthMs / me.lengthMs) : 1.f;
        const float rf = ref.dropMs - ref.tokMs, mf = (me.dropMs - me.tokMs) * lenK;
        if (ref.dropMs > 0.f && me.dropMs > 0.f && rf > 0.f && mf > 0.f && std::abs (rf - mf) > 15.f)
        {
            const float v = lim (P::peTime, f[P::peTime] * juce::jlimit (0.33f, 3.f, rf / mf));
            out.push_back ({ "The pitch falls in " + juce::String (juce::roundToInt (rf)) + " ms in the reference vs " + juce::String (juce::roundToInt (mf))
                             + " ms" + (lenChange ? " (at the new length)" : "") + ": PITCH TIME " + juce::String (juce::roundToInt (f[P::peTime])) + " -> " + juce::String (juce::roundToInt (v)) + " ms", { { "pe_time", v } } });
        }
        if (std::abs (ref.tokMs - me.tokMs) > 20.f && (ref.tokMs >= 30.f || f[P::tailPOffset] > 1.f))
        {
            const float v = lim (P::tailPOffset, juce::jmax (0.f, f[P::tailPOffset] + ref.tokMs - me.tokMs));
            out.push_back ({ ref.tokMs >= 30.f ? "The reference holds a TONAL TOK ~" + juce::String (juce::roundToInt (ref.tokMs)) + " ms before the drop (yours ~" + juce::String (juce::roundToInt (me.tokMs))
                                                 + " ms): TAIL PITCH OFFSET " + juce::String (juce::roundToInt (v)) + " ms"
                                               : juce::String ("The reference has no held tok: TAIL PITCH OFFSET ") + juce::String (juce::roundToInt (v)) + " ms",
                             { { "tail_penv_on", 1.f }, { "tail_penv_offset", v } } });
        }
        // length
        if (lenChange)
        {
            const float v = lim (P::coreLength, f[P::coreLength] * ref.lengthMs / me.lengthMs);
            out.push_back ({ "Length " + juce::String (juce::roundToInt (ref.lengthMs)) + " ms in the reference vs " + juce::String (juce::roundToInt (me.lengthMs))
                             + " ms: LENGTH " + juce::String (juce::roundToInt (v)) + " ms (AUTO LENGTH on, SYNC off)", { { "core_length", v }, { "core_auto_length", 1.f }, { "len_sync", 0.f } } });
        }
        // transient
        if (std::abs (ref.transientDb - me.transientDb) > 3.f)
        {
            const float d = juce::jlimit (-9.f, 9.f, ref.transientDb - me.transientDb);
            out.push_back ({ juce::String (d > 0 ? "More" : "Less") + " punch in the reference (" + juce::String (d, 1) + " dB): PUNCH LEVEL " + juce::String (f[P::punchLevel] + d, 1) + " dB",
                             { { "punch_level", lim (P::punchLevel, f[P::punchLevel] + d) } } });
        }
        // bands
        const float dSub = (ref.bandDb[0] + ref.bandDb[1] - me.bandDb[0] - me.bandDb[1]) * 0.5f;
        if (std::abs (dSub) > 2.f)
            out.push_back ({ juce::String (dSub > 0 ? "More" : "Less") + " sub / low in the reference (" + juce::String (dSub, 1) + " dB): SUB LEVEL " + juce::String (f[P::subLevel] + dSub * 0.8f, 1) + " dB",
                             { { "sub_level", lim (P::subLevel, f[P::subLevel] + dSub * 0.8f) } } });
        const float dBody = ref.bandDb[2] - me.bandDb[2];
        if (std::abs (dBody) > 2.f)
            out.push_back ({ juce::String (dBody > 0 ? "More" : "Less") + " body (150-400 Hz) in the reference (" + juce::String (dBody, 1) + " dB): BODY LEVEL " + juce::String (f[P::bodyLevel] + dBody * 0.8f, 1) + " dB",
                             { { "body_level", lim (P::bodyLevel, f[P::bodyLevel] + dBody * 0.8f) } } });
        const float dMid = (ref.bandDb[3] + ref.bandDb[4] - me.bandDb[3] - me.bandDb[4]) * 0.5f;
        if (std::abs (dMid) > 2.f)
        {
            const float drv = lim (P::tailDrive, f[P::tailDrive] + (dMid > 0 ? 0.1f : -0.1f)), tone = lim (P::tailTone, f[P::tailTone] * (dMid > 0 ? 1.5f : 1.f / 1.5f));
            out.push_back ({ juce::String (dMid > 0 ? "More" : "Less") + " mid / presence (the distorted tail) in the reference (" + juce::String (dMid, 1) + " dB): TAIL DRIVE "
                             + juce::String (juce::roundToInt (drv * 100.f)) + " %, TAIL TONE " + juce::String (juce::roundToInt (tone)) + " Hz", { { "tail_drive", drv }, { "tail_tone", tone } } });
        }
        const float dAir = ref.bandDb[5] - me.bandDb[5];
        if (std::abs (dAir) > 3.f)
        {
            const float v = lim (P::tailHighCut, f[P::tailHighCut] * (dAir > 0 ? 2.f : 0.5f));
            out.push_back ({ juce::String (dAir > 0 ? "More" : "Less") + " air (5-16 kHz) in the reference (" + juce::String (dAir, 1) + " dB): TAIL HIGH CUT " + juce::String (juce::roundToInt (v)) + " Hz",
                             { { "tail_high_cut", v } } });
        }
        // density (crest)
        if (std::abs (ref.crestDb - me.crestDb) > 2.f)
        {
            const float v = lim (P::clipDrive, f[P::clipDrive] + (me.crestDb - ref.crestDb));
            out.push_back ({ "The reference is " + juce::String (ref.crestDb < me.crestDb ? "denser" : "more dynamic") + " (crest " + juce::String (ref.crestDb, 1) + " vs " + juce::String (me.crestDb, 1)
                             + " dB): MASTER CLIP " + juce::String (v, 1) + " dB", { { "master_clip_drive", v } } });
        }
        // width
        if (ref.stereo && ! me.stereo)
            out.push_back ({ "The reference is stereo (side " + juce::String (ref.sideDb, 1) + " dB), your kick is mono: TAIL stereo unit ON, DECORRELATION 150 %, LOW MONO",
                             { { "spc_tail_on", 1.f }, { "spc_tail_mode", 1.f }, { "spc_tail_width", 1.5f }, { "spc_tail_low_mono", 1.f } } });
        else if (! ref.stereo && me.stereo)
            out.push_back ({ "The reference is mono, your kick is stereo: MASTER stereo unit ON, WIDTH 0 %", { { "spc_master_on", 1.f }, { "spc_master_width", 0.f }, { "spc_master_mode", 0.f } } });
        // level (only to compare by ear)
        if (std::abs (ref.m.rmsDb - me.m.rmsDb) > 1.5f)
        {
            const float v = lim (P::coreMaster, f[P::coreMaster] + (ref.m.rmsDb - me.m.rmsDb));
            out.push_back ({ "Loudness: the reference is " + juce::String (std::abs (ref.m.rmsDb - me.m.rmsDb), 1) + " dB " + (ref.m.rmsDb > me.m.rmsDb ? "louder" : "quieter")
                             + " (RMS) - MASTER " + juce::String (v, 1) + " dB to compare fairly", { { "core_master", v } } });
        }
        if (out.empty()) out.push_back ({ "Close to the reference: no difference above the thresholds", {} });
        return out;
    }
    // APPLY: the chosen suggestions as ONE undo step
    inline int apply (GeProcessor& p, const std::vector<Suggestion>& s)
    {
        int n = 0;
        p.beginUndoStep ("Analysis suggestions");
        for (auto& x : s) for (auto& c : x.changes) { p.setParamInUndoStep (c.id, c.value); ++n; }
        return n;
    }
}
