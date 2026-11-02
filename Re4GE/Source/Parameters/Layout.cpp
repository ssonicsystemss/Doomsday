// RE4GE - the parameter layout. IDs are FROZEN since v0.3 (Tests/param_ids.txt); names carry the module prefix (FL's automation list).
#include "Layout.h"
#include "../DSP/ModMatrix.h"
#include "DoomsdayParams.h"

using namespace juce;
using namespace doomsday;
namespace fm = doomsday::fmt;

namespace re4ge
{
namespace
    {
        String fmtX (float v, int) { return "x" + String (v, 2); }
        String fmtNum (float v, int) { return String (v, 2); }
        String offDb (float v, int) { return v <= -59.f ? String ("OFF") : String (v, 1) + " dB"; }
        String stepPct (float v, int) { return String (roundToInt (v)) + "% step"; }
        String noteName (float v, int)
        {
            static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            const int n = roundToInt (v);
            return String (names[(n % 12 + 12) % 12]) + String (n / 12 - 1);
        }
        String subFmt (float v, int) { return v < 20.5f ? String ("OFF") : fm::hz (v, 0); }
        String lowCutFmt (float v, int) { return v < 20.5f ? String ("OFF") : fm::hz (v, 0); }
    }
    
    AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace ParamIDs;
        using make::f; using make::b; using make::c; using make::skew;
        AudioProcessorValueTreeState::ParameterLayout l;
        // trigger / timing
        l.add (c (trigger, "TRG Trigger", Choices::triggers, 0));
        l.add (c (timing, "TRG Timing", Choices::timings, 0));
        l.add (c (step, "TRG Step", Choices::steps, 0));
        l.add (b (step1, "TRG Step 1", true));
        l.add (b (step2, "TRG Step 2", true));
        l.add (b (step3, "TRG Step 3", true));
        l.add (b (step4, "TRG Step 4", true));
        l.add (f (offset, "TRG Step Offset", { 0.f, 50.f }, 0.f, stepPct, fm::num));
        l.add (c (beats, "TRG Beats Per Note", Choices::beats, 0));
        // envelope
        l.add (f (delayPct, "ENV Delay (step)", { 0.f, 95.f }, 36.f, stepPct, fm::num));
        l.add (f (holdPct, "ENV Hold (step)", { 0.f, 100.f }, 100.f, stepPct, fm::num));
        l.add (f (decayPct, "ENV Decay (step)", { 0.f, 100.f }, 0.f, [] (float v, int) { return v < 0.5f ? String ("OFF") : stepPct (v, 0); }, fm::num));
        l.add (f (gatePct, "ENV Gate (step)", { 5.f, 100.f }, 98.f, stepPct, fm::num));
        l.add (f (delayMs, "ENV Delay (ms)", skew (0.f, 1000.f, 120.f), 120.f, fm::ms, fm::msIn));
        l.add (f (holdMs, "ENV Hold (ms)", skew (0.f, 4000.f, 300.f), 400.f, fm::ms, fm::msIn));
        l.add (f (decayMs, "ENV Decay (ms)", skew (0.f, 4000.f, 300.f), 0.f, [] (float v, int) { return v < 0.5f ? String ("OFF") : fm::ms (v, 0); }, fm::msIn));
        l.add (f (gateMs, "ENV Gate (ms)", skew (10.f, 4000.f, 400.f), 380.f, fm::ms, fm::msIn));
        l.add (f (duck, "ENV Duck Level", { 0.f, 1.f }, 0.f, fm::pct, fm::pctIn));
        l.add (f (initial, "ENV Initial Level", { 0.f, 1.f }, 0.f, fm::pct, fm::pctIn));
        l.add (f (swell, "ENV Swell", skew (2.f, 400.f, 60.f), 60.f, fm::ms, fm::msIn));
        l.add (f (curve, "ENV Swell Curve", { -1.f, 1.f }, 0.6f, fm::bip, fm::pctIn));
        l.add (f (sustain, "ENV Sustain", { 0.f, 1.f }, 1.f, fm::pct, fm::pctIn));
        l.add (f (decayCurve, "ENV Decay Curve", skew (0.3f, 6.f, 1.5f), 1.5f, fmtNum, fm::num));
        l.add (f (fade, "ENV Fade", skew (1.f, 150.f, 25.f), 25.f, fm::ms, fm::msIn));
        // source
        l.add (c (source, "SRC Source", Choices::sources, 1));
        l.add (f (blend, "OSC Saw / Pulse", { 0.f, 1.f }, 0.3f, fm::pct, fm::pctIn));
        l.add (f (width, "OSC Pulse Width", { 0.05f, 0.95f }, 0.5f, fm::pct, fm::pctIn));
        l.add (f (sine, "OSC Sine Level", { -60.f, 6.f }, -6.f, offDb, fm::num));
        l.add (c (warp, "OSC Phase Warp", Choices::warps, 0));
        l.add (f (warpAmt, "OSC Warp Amount", { 0.f, 1.f }, 0.5f, fm::pct, fm::pctIn));
        l.add (f (warpEnv, "OSC Warp Envelope", { 0.f, 1.f }, 0.5f, fm::pct, fm::pctIn));
        l.add (f (tailStart, "TAIL Start Pitch", skew (1.f, 12.f, 3.f), 4.f, fmtX, fm::num));
        l.add (f (tailDrop, "TAIL Pitch Drop", skew (2.f, 400.f, 60.f), 60.f, fm::ms, fm::msIn));
        l.add (f (tailCurve, "TAIL Drop Curve", { 0.5f, 10.f }, 3.f, fmtNum, fm::num));
        l.add (f (tailWave, "TAIL Wave", { 0.f, 1.f }, 0.f, fm::pct, fm::pctIn));
        l.add (c (tailDist, "TAIL Distortion", Choices::distTypes, 4));
        l.add (f (tailDrive, "TAIL Drive", { 0.f, 48.f }, 28.f, fm::db, fm::num));
        l.add (f (tailTone, "TAIL Tone", { -1.f, 1.f }, 0.2f, fm::bip, fm::pctIn));
        l.add (f (tailHighCut, "TAIL High Cut", skew (500.f, 20000.f, 5000.f), 9000.f, fm::hz, fm::hzIn));
        l.add (f (tailScream, "TAIL Scream", { 0.f, 1.f }, 0.3f, fm::pct, fm::pctIn));
        l.add (f (tailScreamHz, "TAIL Scream Freq", skew (150.f, 5000.f, 900.f), 1200.f, fm::hz, fm::hzIn));
        l.add (f (tailLen, "TAIL Length (Gate)", skew (40.f, 2000.f, 300.f), 300.f, fm::ms, fm::msIn));
        l.add (f (tailRoom, "TAIL Room", { 0.f, 1.f }, 0.f, fm::pct, fm::pctIn));
        l.add (f (tailRoomSize, "TAIL Room Size", { 0.f, 1.f }, 0.5f, fm::pct, fm::pctIn));
        l.add (b (smpReverse, "SAMPLE Reverse", true));
        l.add (f (smpRoot, "SAMPLE Root", { 0.f, 127.f, 1.f }, 31.f, noteName, fm::num));
        l.add (f (smpStart, "SAMPLE Start", { 0.f, 500.f }, 0.f, fm::ms, fm::msIn));
        // pitch
        l.add (c (pitchMode, "PITCH Mode", Choices::pitchModes, 0));
        l.add (f (pitchAmt, "PITCH Amount", { -24.f, 24.f }, 12.f, fm::semi, fm::num));
        l.add (f (pitchTime, "PITCH Time", skew (2.f, 400.f, 60.f), 60.f, fm::ms, fm::msIn));
        l.add (f (tune, "PITCH Tune", { -24.f, 24.f }, 0.f, fm::semi, fm::num));
        l.add (f (fine, "PITCH Fine", { -100.f, 100.f }, 0.f, fm::cents, fm::num));
        l.add (b (keyTrack, "PITCH Key Track", true));
        l.add (f (fixedNote, "PITCH Fixed Note", { 12.f, 72.f, 1.f }, 29.f, noteName, fm::num));
        // tone
        l.add (c (preType, "FILTER Pre Filter Type", Choices::filterTypes, 0));
        l.add (f (preHz, "FILTER Pre Filter Cutoff", skew (30.f, 16000.f, 900.f), 800.f, fm::hz, fm::hzIn));
        l.add (f (preReso, "FILTER Pre Filter Resonance", { 0.f, 1.f }, 0.2f, fm::pct, fm::pctIn));
        l.add (c (distType, "DIST Distortion", Choices::distTypes, 0));
        l.add (f (drive, "DIST Drive", { 0.f, 40.f }, 10.f, fm::db, fm::num));
        l.add (f (driveEnv, "DIST Drive Envelope", { 0.f, 1.f }, 0.f, fm::pct, fm::pctIn));
        l.add (f (bias, "DIST Asymmetry", { -1.f, 1.f }, 0.f, fm::bip, fm::pctIn));
        l.add (f (mix, "DIST Harmonic Mix", { 0.f, 1.f }, 1.f, fm::pct, fm::pctIn));
        l.add (f (lowCut, "EQ Low Cut", skew (20.f, 400.f, 60.f), 20.f, lowCutFmt, fm::hzIn));
        l.add (f (highCut, "EQ High Cut", skew (200.f, 16000.f, 1500.f), 900.f, fm::hz, fm::hzIn));
        l.add (f (bellHz, "EQ Bell Freq", skew (100.f, 6000.f, 700.f), 700.f, fm::hz, fm::hzIn));
        l.add (f (bellGain, "EQ Bell Gain", { -18.f, 18.f }, 0.f, fm::db, fm::num));
        l.add (f (bellQ, "EQ Bell Q", skew (0.3f, 8.f, 1.2f), 1.f, fmtNum, fm::num));
        l.add (f (subHz, "SUB Crossover", skew (20.f, 250.f, 90.f), 90.f, subFmt, fm::hzIn));
        l.add (f (subLevel, "SUB Level", { -60.f, 6.f }, 0.f, offDb, fm::num));
        l.add (f (clip, "OUT Clip", { 0.f, 1.f }, 0.2f, fm::pct, fm::pctIn));
        l.add (f (level, "OUT Level", { -36.f, 6.f }, -3.f, fm::db, fm::num));
        l.add (f (velocity, "MIDI Velocity Sensitivity", { 0.f, 1.f }, 0.f, fm::pct, fm::pctIn));
        l.add (f (uniVoices, "UNISON Voices", { 1.f, 7.f, 1.f }, 5.f, [] (float v, int) { return v < 1.5f ? String ("OFF") : String (roundToInt (v)); }, fm::num));
        l.add (f (uniDetune, "UNISON Detune", { 0.f, 60.f }, 18.f, fm::cents, fm::num));
        l.add (f (uniDrift, "UNISON Drift", { 0.f, 30.f }, 6.f, fm::cents, fm::num));
        l.add (f (uniMix, "UNISON Mix", { 0.f, 1.f }, 0.6f, fm::pct, fm::pctIn));
        l.add (f (stereoWidth, "STEREO Width", { 0.f, 1.f }, 0.4f, fm::pct, fm::pctIn));
        l.add (f (widthSplit, "STEREO Width From", skew (80.f, 1500.f, 350.f), 350.f, fm::hz, fm::hzIn));
        // ---- v0.4: step 1 (appended: the order of Tests/param_ids.txt)
        l.add (f (osc1Level, "OSC1 Level", { -60.f, 6.f }, 0.f, offDb, fm::num));
        for (int o = 2; o <= 4; ++o)
        {
            const String n = "OSC" + String (o) + " ";
            l.add (c (oscId (o, "wave"), n + "Wave", Choices::oscWaves, 2));
            l.add (f (oscId (o, "level"), n + "Level", { -60.f, 6.f }, -60.f, offDb, fm::num));
            l.add (f (oscId (o, "oct"), n + "Octave", { -2.f, 3.f, 1.f }, 0.f, [] (float v, int) { return (v > 0.f ? "+" : "") + String (roundToInt (v)) + " oct"; }, fm::num));
            l.add (f (oscId (o, "semi"), n + "Semi", { -12.f, 12.f, 1.f }, 0.f, fm::semi, fm::num));
            l.add (f (oscId (o, "fine"), n + "Fine", { -100.f, 100.f }, 0.f, fm::cents, fm::num));
            l.add (f (oscId (o, "phase"), n + "Phase", { 0.f, 360.f }, 0.f, [] (float v, int) { return String (roundToInt (v)) + String (CharPointer_UTF8 ("\xc2\xb0")); }, fm::num));
            l.add (f (oscId (o, "width"), n + "Width", { 0.f, 1.f }, 0.5f, fm::pct, fm::pctIn));
            l.add (b (oscId (o, "sync"), n + "Sync", false));
            l.add (f (oscId (o, "pm"), n + "PM", { 0.f, 1.f }, 0.f, fm::pct, fm::pctIn));
        }
        l.add (c (phaseMode, "PHASE Mode", Choices::phaseModes, 0));
        l.add (f (preEnv, "FILTER Pre Env", { -4.f, 4.f }, 0.f, [] (float v, int) { return (v > 0.f ? "+" : "") + String (v, 1) + " oct"; }, fm::num));
        l.add (f (preKey, "FILTER Pre Key Track", { 0.f, 1.f }, 0.f, fm::pct, fm::pctIn));
        l.add (c (dist2Type, "DIST2 Type", Choices::distTypes, 1));
        l.add (f (dist2Drive, "DIST2 Drive", { 0.f, 40.f }, 0.f, [] (float v, int) { return v < 0.05f ? String ("OFF") : fm::db (v, 0); }, fm::num));
        l.add (f (dist2Mix, "DIST2 Mix", { 0.f, 1.f }, 1.f, fm::pct, fm::pctIn));
        for (int k = 0; k < 8; ++k) l.add (f (macros[k], "MACRO " + macroNames[k], { 0.f, 1.f }, 0.f, fm::pct, fm::pctIn));
        // ---- v0.5
        l.add (c (subOct, "SUB Octave", Choices::subOcts, 0));
        l.add (f (subHarm, "SUB Harmonics", { 0.f, 1.f }, 0.f, fm::pct, fm::pctIn));
        l.add (f (subSat, "SUB Saturation", { 0.f, 1.f }, 0.f, fm::pct, fm::pctIn));
        l.add (f (clickLevel, "CLICK Level", { -60.f, 0.f }, -60.f, offDb, fm::num));
        l.add (f (clickTone, "CLICK Tone", skew (500.f, 10000.f, 2000.f), 2000.f, fm::hz, fm::hzIn));
        l.add (f (clickLen, "CLICK Length", skew (1.f, 40.f, 8.f), 8.f, fm::ms, fm::msIn));
        l.add (c (clickAt, "CLICK At", Choices::clickAts, 0));
        l.add (f (glide, "PITCH Glide", skew (0.f, 300.f, 60.f), 0.f, [] (float v, int) { return v < 0.5f ? String ("OFF") : fm::ms (v, 0); }, fm::msIn));
        l.add (f (smear, "STEREO Smear", { 0.f, 1.f }, 0.f, fm::pct, fm::pctIn));
        l.add (f (smearSize, "STEREO Smear Size", { 0.f, 1.f }, 0.5f, fm::pct, fm::pctIn));
        // ---- v0.6: SHAPE DESIGNER
        for (int i = 0; i < 4; ++i)
        {
            const String n = "SHAPE " + laneNames[i] + " ";
            l.add (b (laneId (i, "on"), n + "On", false));
            l.add (f (laneId (i, "len"), n + "Length", { 10.f, 100.f }, 100.f, [] (float v, int) { return String (roundToInt (v)) + "% step"; }, fm::num));
            l.add (c (laneId (i, "anchor"), n + "Anchor", Choices::anchors, i == 1 ? 1 : 0));
            l.add (c (laneId (i, "mode"), n + "Mode", Choices::laneModes, 0));
            if (i == 1) l.add (f (laneId (i, "depth"), n + "Range", { 0.f, 24.f }, 12.f, fm::semi, fm::num));
            else        l.add (f (laneId (i, "depth"), n + "Depth", { 0.f, 1.f }, 1.f, fm::pct, fm::pctIn));
        }
        // ---- v0.7: DISTORTION LAB + QUALITY
        for (int s = 0; s < 2; ++s)
        {
            const String n = "DIST" + String (s + 1) + " ";
            l.add (c (labId (s, "lab"), n + "Lab", Choices::labAlgos, 0));
            l.add (f (labId (s, "hard"), n + "Hardness", { 0.f, 1.f }, 0.5f, fm::pct, fm::pctIn));
            l.add (f (labId (s, "tone"), n + "Tone", { -1.f, 1.f }, 0.f, fm::bip, fm::pctIn));
            l.add (c (labId (s, "tone_post"), n + "Tone Position", Choices::tonePos, 0));
            l.add (f (labId (s, "out"), n + "Output", { -24.f, 12.f }, 0.f, fm::db, fm::num));
            l.add (b (labId (s, "sym"), n + "Custom Symmetric", true));
        }
        l.add (f (dist2Bias, "DIST2 Asymmetry", { -1.f, 1.f }, 0.f, fm::bip, fm::pctIn));
        l.add (b (autoGain, "DIST Auto Gain", true));
        l.add (c (qualityRt, "QUALITY Realtime", Choices::qualities, 1));
        l.add (c (qualityExport, "QUALITY Export", Choices::qualities, 2));
        // ---- v0.8: LFO 1-4 + MOD MATRIX
        for (int i = 0; i < 4; ++i)
        {
            const String n = "LFO" + String (i + 1) + " ";
            l.add (c (lfoId (i, "wave"), n + "Wave", Choices::lfoWaves, 0));
            l.add (b (lfoId (i, "sync"), n + "Sync", true));
            l.add (f (lfoId (i, "rate"), n + "Rate", skew (0.05f, 40.f, 2.f), 2.f, fm::hz, fm::hzIn));
            l.add (c (lfoId (i, "div"), n + "Division", doomsday::mod::syncNames(), 9));
            l.add (f (lfoId (i, "phase"), n + "Phase", { 0.f, 1.f }, 0.f, fm::pct, fm::pctIn));
            l.add (b (lfoId (i, "retrig"), n + "Retrigger", true));
        }
        for (int r = 0; r < 16; ++r)
        {
            const String n = "MOD" + String (r + 1) + " ";
            l.add (b (mtxId (r, "on"), n + "On", false));
            l.add (c (mtxId (r, "src"), n + "Source", re::modSourceNames(), r < 4 ? 1 + r : 0));
            l.add (c (mtxId (r, "dst"), n + "Destination", re::modDestNames(), 0));
            l.add (f (mtxId (r, "amt"), n + "Amount", { -1.f, 1.f }, 0.f, fm::bip, fm::pctIn));
            l.add (c (mtxId (r, "pol"), n + "Polarity", Choices::polarities, 0));
        }
        // ---- v0.9: KICK-TO-REVERSE
        l.add (f (smpEnd, "SAMPLE End", skew (10.f, 4000.f, 400.f), 4000.f, [] (float v, int) { return v > 3999.f ? String ("END") : fm::ms (v, 0); }, fm::msIn));
        l.add (c (smpMode, "SAMPLE Mode", Choices::smpModes, 1));
        return l;
    }
}
