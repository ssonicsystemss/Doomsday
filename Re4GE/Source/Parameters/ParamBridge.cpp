// RE4GE - parameters -> engine settings (the only place that knows both the APVTS and re::Params)
#include "ParamBridge.h"
#include "../DSP/ModMatrix.h"

using namespace juce;

namespace re4ge
{
re::Params gatherParams (const AudioProcessorValueTreeState& apvts, ParamCache& cache)
{
    // the first call (the processor's constructor, message thread) records the parameters in call order; after that every call
    // only walks the recorded pointers - no string building / lookups / allocation on the audio thread
    const bool record = cache.ptrs.empty();
    size_t idx = 0;
    auto get = [&] (const char* id) -> float
    {
        if (record) { cache.ptrs.push_back (apvts.getRawParameterValue (id)); return cache.ptrs.back()->load(); }
        return cache.ptrs[idx++]->load();
    };
    using namespace ParamIDs;
    auto ch = [&get] (const char* id, int hi) { return jlimit (0, hi, (int) std::round (get (id))); };
    re::Params p;
    p.gate = get (trigger) > 0.5f; p.sync = get (timing) < 0.5f;
    const int st = ch (step, 2); p.stepBeats = st == 2 ? 0.25 : st == 1 ? 0.5 : 1.0;
    p.mask = (get (step1) > 0.5f ? 1 : 0) | (get (step2) > 0.5f ? 2 : 0) | (get (step3) > 0.5f ? 4 : 0) | (get (step4) > 0.5f ? 8 : 0);
    if (st == 0) p.mask = 1;                          // 1/4: one step per beat
    else if (st == 1) p.mask &= 3;                    // 1/8: steps 1 and 2
    p.offsetPct = get (offset);
    const int bt = ch (beats, 3); p.beats = bt == 3 ? 8 : bt == 2 ? 4 : bt == 1 ? 2 : 1;
    p.delayPct = get (delayPct); p.holdPct = get (holdPct); p.decayPct = get (decayPct); p.gatePct = get (gatePct);
    p.delayMs = get (delayMs); p.holdMs = get (holdMs); p.decayMs = get (decayMs); p.gateMs = get (gateMs);
    p.duck = get (duck); p.initial = get (initial); p.swellMs = get (swell); p.curve = get (curve); p.sustain = get (sustain);
    p.decayCurve = get (decayCurve); p.fadeMs = get (fade);
    p.source = ch (source, 2); p.blend = get (blend); p.width = get (width); p.sineDb = get (sine);
    p.warp = ch (warp, 4); p.warpAmt = get (warpAmt); p.warpEnv = get (warpEnv);
    auto& t = p.tail;
    t.startRatio = get (tailStart); t.dropMs = get (tailDrop); t.curve = get (tailCurve); t.wave = get (tailWave);
    t.distType = ch (tailDist, 5); t.driveDb = get (tailDrive); t.tone = get (tailTone); t.highCutHz = get (tailHighCut);
    t.scream = get (tailScream); t.screamHz = get (tailScreamHz); t.lowCutHz = 20.f; t.dist2Type = 0; t.autoGain = false;
    p.tailLenMs = get (tailLen); p.tailRoom = get (tailRoom); p.tailRoomSize = get (tailRoomSize);
    p.sampleReverse = get (smpReverse) > 0.5f; p.root = roundToInt (get (smpRoot)); p.sampleStartMs = get (smpStart);
    p.pitchMode = ch (pitchMode, 3); p.pitchSt = get (pitchAmt); p.pitchMs = get (pitchTime); p.tuneSt = get (tune); p.fineCt = get (fine);
    p.keyTrack = get (keyTrack) > 0.5f; p.fixedNote = roundToInt (get (fixedNote));
    p.preType = ch (preType, 4); p.preHz = get (preHz); p.preReso = get (preReso);
    p.distType = ch (distType, 5); p.driveDb = get (drive); p.driveEnv = get (driveEnv); p.bias = get (bias); p.mix = get (mix);
    p.lowCutHz = get (lowCut); p.highCutHz = get (highCut); p.bellHz = get (bellHz); p.bellDb = get (bellGain); p.bellQ = get (bellQ);
    p.subHz = get (subHz); p.subDb = get (subLevel); p.clip = get (clip); p.levelDb = get (level); p.velocitySens = get (velocity);
    p.uniVoices = roundToInt (get (uniVoices)); p.uniDetuneCt = get (uniDetune); p.driftCt = get (uniDrift); p.uniMix = get (uniMix);
    p.stereoWidth = get (stereoWidth); p.splitHz = get (widthSplit);
    p.osc1Db = get (osc1Level);
    for (int o = 2; o <= 4; ++o)
    {
        auto& x = p.osc[(size_t) (o - 2)];
        x.wave = ch (oscId (o, "wave"), 5); x.levelDb = get (oscId (o, "level")); x.oct = get (oscId (o, "oct")); x.semi = get (oscId (o, "semi"));
        x.fineCt = get (oscId (o, "fine")); x.phaseDeg = get (oscId (o, "phase")); x.width = get (oscId (o, "width"));
        x.sync = get (oscId (o, "sync")) > 0.5f; x.pm = get (oscId (o, "pm"));
    }
    p.phaseFree = get (phaseMode) > 0.5f;
    p.preEnvOct = get (preEnv); p.preKey = get (preKey);
    p.dist2Type = ch (dist2Type, 5); p.dist2Db = get (dist2Drive); p.dist2Mix = get (dist2Mix);
    for (int k = 0; k < 8; ++k) p.macro[(size_t) k] = get (macros[k]);
    p.subOctDown = get (subOct) > 0.5f; p.subHarm = get (subHarm); p.subSat = get (subSat);
    p.clickDb = get (clickLevel); p.clickHz = get (clickTone); p.clickMs = get (clickLen); p.clickAt = ch (clickAt, 1);
    p.glideMs = get (glide); p.smear = get (smear); p.smearSize = get (smearSize);
    for (int i = 0; i < 4; ++i)
    {
        auto& L = p.lane[(size_t) i];
        L.on = get (laneId (i, "on")) > 0.5f; L.lenPct = get (laneId (i, "len")); L.anchor = ch (laneId (i, "anchor"), 1);
        L.loop = get (laneId (i, "mode")) > 0.5f; L.depth = get (laneId (i, "depth"));      // (the curves come from the processor)
    }
    for (int s = 0; s < 2; ++s)
    {
        auto& L = p.lab[(size_t) s];
        L.algo = ch (labId (s, "lab"), 6) - 1; L.hard = get (labId (s, "hard")); L.tone = get (labId (s, "tone"));
        L.tonePost = get (labId (s, "tone_post")) > 0.5f; L.outDb = get (labId (s, "out")); L.symmetric = get (labId (s, "sym")) > 0.5f;
    }
    p.dist2Bias = get (dist2Bias); p.autoGain = get (autoGain) > 0.5f;
    p.qualityRt = ch (qualityRt, 3); p.qualityExport = ch (qualityExport, 3);
    for (int i = 0; i < 4; ++i)
    {
        auto& L = p.lfo[(size_t) i];
        L.wave = ch (lfoId (i, "wave"), 7); L.sync = get (lfoId (i, "sync")) > 0.5f; L.hz = get (lfoId (i, "rate"));
        L.div = ch (lfoId (i, "div"), 18); L.phase = get (lfoId (i, "phase")); L.retrig = get (lfoId (i, "retrig")) > 0.5f;
    }
    for (int r = 0; r < 16; ++r)
    {
        auto& m = p.mtx[(size_t) r];
        m.on = get (mtxId (r, "on")) > 0.5f; m.src = ch (mtxId (r, "src"), re::NumModSrc - 1);
        m.dst = ch (mtxId (r, "dst"), re::NumModDst - 1); m.amount = get (mtxId (r, "amt")); m.polarity = ch (mtxId (r, "pol"), 2);
    }
    p.sampleEndMs = get (smpEnd); p.sampleOriginal = get (smpMode) < 0.5f; p.sampleKeepLength = get (smpPitch) > 0.5f;
    return p;
}
}
