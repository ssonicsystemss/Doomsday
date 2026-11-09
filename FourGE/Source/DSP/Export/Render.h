#pragma once
// =====================================================================
//  4GE module 12 - OFFLINE RENDER for the export (pure DSP: no files, no GUI). Runs on a background thread on a SNAPSHOT of the
//  settings (parameters, drawn shapes, distortion / oscillator / modulation data, the samples kept alive) - changing the plugin
//  while it renders never changes the render. The same Engine class as the live plugin (realtime = export), at the export rate
//  (no resampling: the engine renders natively at 44.1 ... 192 kHz), the MASTER OUTPUT lookahead taken out (latency compensated).
//   SOURCE: FULL / PUNCH / BODY / TAIL / SUB / TEXTURE / CUSTOM group (their buses with every processor on the way, the master
//   processing included) / one LAYER / one SAMPLE slot (soloed, the core parts, noise, transient and screech silent).
//   LENGTH: AUTO (at least the kick LENGTH, then until silence), FIXED, UNTIL SILENCE; PRE-ROLL, POST-ROLL, SILENCE THRESHOLD with
//   +6 dB hysteresis, MINIMUM SILENCE, MAXIMUM LENGTH, TRIM SILENCE. Tails (reverb-like screech, delays) are waited for.
// =====================================================================
#include "../Engine.h"

namespace fge::dsp::xport
{
    enum Source { SrcFull = 0, SrcPunch, SrcBody, SrcTail, SrcSub, SrcTexture, SrcCustom, SrcLayer, SrcSample, NumSources };
    enum LengthMode { LenAuto = 0, LenFixed, LenSilence };
    inline const char* sourceName (int s) { static const char* n[NumSources] { "FULL", "PUNCH", "BODY", "TAIL", "SUB", "TEXTURE", "CUSTOM", "LAYER", "SAMPLE" }; return n[juce::jlimit (0, NumSources - 1, s)]; }

    struct Snapshot
    {
        ParamFrame f;
        std::shared_ptr<env::Bank> shapes = std::make_shared<env::Bank>();
        std::shared_ptr<dist::DistData> dist = std::make_shared<dist::DistData>();
        std::shared_ptr<OscData> osc = std::make_shared<OscData>();
        std::shared_ptr<mod::Data> mod;                                    // null = no modulation
        std::array<std::shared_ptr<const SampleData>, numSampleSlots> samples {};
        juce::String presetName { "INIT" };
        juce::String style;                                                // module 13 (empty = from the preset name)
        uint32_t presetSeed = 0;
    };
    struct Spec
    {
        int source = SrcFull, index = 0, note = 31, lengthMode = LenAuto;
        double rate = 44100.0, bpm = 150.0;
        float fixedMs = 1000.f, preRollMs = 0.f, postRollMs = 20.f, silenceDb = -80.f, minSilenceMs = 60.f, maxMs = 6000.f;
        bool trim = false;
        uint32_t seed = 0;
    };
    struct Result { juce::AudioBuffer<float> audio; bool cancelled = false; double latencyRemoved = 0.0; };

    // the frame for a source (what is soloed / switched off) + which buses the engine keeps
    inline ParamFrame frameFor (ParamFrame f, const Spec& s, uint32_t& keepCore, uint32_t& keepBus)
    {
        keepCore = 0xF; keepBus = 0x7F;
        switch (s.source)
        {
            case SrcPunch: case SrcBody: case SrcTail: case SrcSub: keepCore = 1u << (s.source - SrcPunch); keepBus = 1u << (s.source - SrcPunch); break;
            case SrcTexture: keepCore = 0; keepBus = 1u << BusTexture; break;
            case SrcCustom:  keepCore = 0; keepBus = 1u << BusCustom; break;
            case SrcLayer: case SrcSample:
            {
                keepCore = 0;
                const int ls = P::layer2On - P::layer1On, ss = P::smp2On - P::smp1On;
                for (int i = 0; i < numLayers; ++i) f[P::layer1Solo + i * ls] = s.source == SrcLayer && i == s.index ? 1.f : 0.f;
                for (int i = 0; i < numSampleSlots; ++i) f[P::smp1Solo + i * ss] = s.source == SrcSample && i == s.index ? 1.f : 0.f;
                f[P::layerSoloMode] = 0.f;
                for (int id : { (int) P::noiseNAOn, (int) P::noiseNBOn, (int) P::trOn, (int) P::scrOn }) f[id] = 0.f;
                break;
            }
            default: break;
        }
        return f;
    }

    // render (background thread). 'cancel' may stop it at any block; 'progress' 0..1 (both may be null)
    inline Result render (const Snapshot& snap, const Spec& s, const std::atomic<bool>* cancel = nullptr, std::atomic<float>* progress = nullptr)
    {
        Result res;
        juce::ScopedNoDenormals noDenormals;                                // (the same number handling as the audio thread: realtime = export)
        uint32_t keepCore, keepBus;
        const ParamFrame f = frameFor (snap.f, s, keepCore, keepBus);
        // oscillator tables from the snapshot (the same as renderKick)
        std::array<TablePtr, maxOsc> keep;
        OscTables tables;
        for (int i = 0; i < numOsc; ++i)
            if (f.on (P::osc1On + i * (P::osc2On - P::osc1On))) { keep[(size_t) i] = osctables::build (f, *snap.osc, i); tables.t[(size_t) i] = keep[(size_t) i].get(); }
        Engine::SamplePtrs ptrs;
        for (int k = 0; k < numSampleSlots; ++k) ptrs[(size_t) k].store (snap.samples[(size_t) k].get());
        const int block = 512;
        auto engine = std::make_unique<Engine>();
        engine->prepare (s.rate, block);
        engine->setBank (snap.shapes.get());
        engine->setHostBpm (s.bpm);
        engine->setTables (&tables);
        engine->setDistData (snap.dist.get());
        engine->setOffline (true);
        engine->setModData (snap.mod.get());
        engine->setSamples (&ptrs);
        engine->setSeed (s.seed);
        engine->setExportKeep (keepCore, keepBus);
        engine->cookOnly (f);
        const int look = engine->dynLookahead();
        res.latencyRemoved = look;
        const double r = s.rate;
        const int maxN = (int) (juce::jlimit (50.f, 60000.f, s.maxMs) * 0.001 * r);
        const int fixedN = (int) (juce::jlimit (1.f, 60000.f, s.fixedMs) * 0.001 * r);
        const int minN = s.lengthMode == LenAuto ? (int) (kickLengthMs (f, s.bpm) * 0.001 * r) : 0;
        const int minQuiet = juce::jmax (1, (int) (s.minSilenceMs * 0.001 * r)), post = (int) (juce::jmax (0.f, s.postRollMs) * 0.001 * r);
        const float thr = juce::Decibels::decibelsToGain (s.silenceDb), thrUp = thr * 2.f;     // +6 dB hysteresis
        const int offAt = (int) (kickLengthMs (f, s.bpm) * 0.001 * r);
        juce::AudioBuffer<float> out (2, juce::jmin (maxN, s.lengthMode == LenFixed ? fixedN : maxN)), tmp (2, block);
        out.clear();
        int written = 0, quiet = 0, end = -1, firstSound = -1;
        bool heard = false, inQuiet = false;
        for (int pos = 0; written < out.getNumSamples(); pos += block)
        {
            if (cancel != nullptr && cancel->load()) { res.cancelled = true; return res; }
            tmp.clear();
            juce::MidiBuffer m;
            if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, s.note, 1.f), 0);
            if (offAt >= pos && offAt < pos + block) m.addEvent (juce::MidiMessage::noteOff (1, s.note), offAt - pos);
            engine->process (tmp, m, f);
            for (int i = 0; i < block && written < out.getNumSamples(); ++i)
            {
                if (pos + i < look) continue;                              // the lookahead (latency) is taken out: the hit starts at 0
                const float l = tmp.getSample (0, i), rr = tmp.getSample (1, i);
                out.setSample (0, written, std::isfinite (l) ? l : 0.f); out.setSample (1, written, std::isfinite (rr) ? rr : 0.f);
                const float lv = juce::jmax (std::abs (l), std::abs (rr));
                // silence with hysteresis: it starts under the threshold and only a sound 6 dB above it ends it
                if (lv > thrUp) { heard = true; quiet = 0; inQuiet = false; if (firstSound < 0) firstSound = written; }
                else if (lv < thr) inQuiet = true;
                if (inQuiet) ++quiet;
                ++written;
                if (s.lengthMode != LenFixed && end < 0 && heard && quiet >= minQuiet && written >= minN)
                {
                    end = written - quiet;                                 // the silence started here
                    const int want = juce::jmin (out.getNumSamples(), end + post);
                    if (want <= written) { written = want; break; }
                    out.setSize (2, want, true, true, true);
                }
            }
            if (end >= 0 && written >= out.getNumSamples()) break;
            if (progress != nullptr) progress->store ((float) written / (float) juce::jmax (1, s.lengthMode == LenFixed ? fixedN : juce::jmax (minN * 2, (int) (r * 1.5))));
        }
        out.setSize (2, written, true, true, true);
        // TRIM SILENCE: the leading silence before the hit and anything quiet at the end
        int a = 0, e = written;
        if (s.trim)
        {
            a = juce::jmax (0, firstSound);
            while (e > a + 1 && juce::jmax (std::abs (out.getSample (0, e - 1)), std::abs (out.getSample (1, e - 1))) < thr) --e;
        }
        const int pre = (int) (juce::jmax (0.f, s.preRollMs) * 0.001 * r);
        res.audio.setSize (2, pre + (e - a)); res.audio.clear();
        for (int c = 0; c < 2; ++c) res.audio.copyFrom (c, pre, out, c, a, e - a);
        if (progress != nullptr) progress->store (1.f);
        return res;
    }
}
