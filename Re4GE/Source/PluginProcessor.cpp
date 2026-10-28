#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace juce;
using namespace doomsday;
namespace fm = doomsday::fmt;

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

AudioProcessorValueTreeState::ParameterLayout ReProcessor::createLayout()
{
    using namespace ParamIDs;
    using make::f; using make::b; using make::c; using make::skew;
    AudioProcessorValueTreeState::ParameterLayout l;
    // trigger / timing
    l.add (c (trigger, "Trigger", Choices::triggers, 0));
    l.add (c (timing, "Timing", Choices::timings, 0));
    l.add (c (step, "Step", Choices::steps, 0));
    l.add (b (step1, "Step 1", true));
    l.add (b (step2, "Step 2", true));
    l.add (b (step3, "Step 3", true));
    l.add (b (step4, "Step 4", true));
    l.add (f (offset, "Step Offset", { 0.f, 50.f }, 0.f, stepPct, fm::num));
    l.add (c (beats, "Beats Per Note", Choices::beats, 0));
    // envelope
    l.add (f (delayPct, "Delay (step)", { 0.f, 95.f }, 36.f, stepPct, fm::num));
    l.add (f (holdPct, "Hold (step)", { 0.f, 100.f }, 100.f, stepPct, fm::num));
    l.add (f (decayPct, "Decay (step)", { 0.f, 100.f }, 0.f, [] (float v, int) { return v < 0.5f ? String ("OFF") : stepPct (v, 0); }, fm::num));
    l.add (f (gatePct, "Gate (step)", { 5.f, 100.f }, 98.f, stepPct, fm::num));
    l.add (f (delayMs, "Delay (ms)", skew (0.f, 1000.f, 120.f), 120.f, fm::ms, fm::msIn));
    l.add (f (holdMs, "Hold (ms)", skew (0.f, 4000.f, 300.f), 400.f, fm::ms, fm::msIn));
    l.add (f (decayMs, "Decay (ms)", skew (0.f, 4000.f, 300.f), 0.f, [] (float v, int) { return v < 0.5f ? String ("OFF") : fm::ms (v, 0); }, fm::msIn));
    l.add (f (gateMs, "Gate (ms)", skew (10.f, 4000.f, 400.f), 380.f, fm::ms, fm::msIn));
    l.add (f (duck, "Duck Level", { 0.f, 1.f }, 0.f, fm::pct, fm::pctIn));
    l.add (f (initial, "Initial Level", { 0.f, 1.f }, 0.f, fm::pct, fm::pctIn));
    l.add (f (swell, "Swell", skew (2.f, 400.f, 60.f), 60.f, fm::ms, fm::msIn));
    l.add (f (curve, "Swell Curve", { -1.f, 1.f }, 0.6f, fm::bip, fm::pctIn));
    l.add (f (sustain, "Sustain", { 0.f, 1.f }, 1.f, fm::pct, fm::pctIn));
    l.add (f (decayCurve, "Decay Curve", skew (0.3f, 6.f, 1.5f), 1.5f, fmtNum, fm::num));
    l.add (f (fade, "Fade", skew (1.f, 150.f, 25.f), 25.f, fm::ms, fm::msIn));
    // source
    l.add (c (source, "Source", Choices::sources, 1));
    l.add (f (blend, "Saw / Pulse", { 0.f, 1.f }, 0.3f, fm::pct, fm::pctIn));
    l.add (f (width, "Pulse Width", { 0.05f, 0.95f }, 0.5f, fm::pct, fm::pctIn));
    l.add (f (sine, "Sine Level", { -60.f, 6.f }, -6.f, offDb, fm::num));
    l.add (c (warp, "Phase Warp", Choices::warps, 0));
    l.add (f (warpAmt, "Warp Amount", { 0.f, 1.f }, 0.5f, fm::pct, fm::pctIn));
    l.add (f (warpEnv, "Warp Envelope", { 0.f, 1.f }, 0.5f, fm::pct, fm::pctIn));
    l.add (f (tailStart, "Tail Start Pitch", skew (1.f, 12.f, 3.f), 4.f, fmtX, fm::num));
    l.add (f (tailDrop, "Tail Pitch Drop", skew (2.f, 400.f, 60.f), 60.f, fm::ms, fm::msIn));
    l.add (f (tailCurve, "Tail Drop Curve", { 0.5f, 10.f }, 3.f, fmtNum, fm::num));
    l.add (f (tailWave, "Tail Wave", { 0.f, 1.f }, 0.f, fm::pct, fm::pctIn));
    l.add (c (tailDist, "Tail Distortion", Choices::distTypes, 4));
    l.add (f (tailDrive, "Tail Drive", { 0.f, 48.f }, 28.f, fm::db, fm::num));
    l.add (f (tailTone, "Tail Tone", { -1.f, 1.f }, 0.2f, fm::bip, fm::pctIn));
    l.add (f (tailHighCut, "Tail High Cut", skew (500.f, 20000.f, 5000.f), 9000.f, fm::hz, fm::hzIn));
    l.add (f (tailScream, "Tail Scream", { 0.f, 1.f }, 0.3f, fm::pct, fm::pctIn));
    l.add (f (tailScreamHz, "Tail Scream Freq", skew (150.f, 5000.f, 900.f), 1200.f, fm::hz, fm::hzIn));
    l.add (f (tailLen, "Tail Length (Gate)", skew (40.f, 2000.f, 300.f), 300.f, fm::ms, fm::msIn));
    l.add (f (tailRoom, "Tail Room", { 0.f, 1.f }, 0.f, fm::pct, fm::pctIn));
    l.add (f (tailRoomSize, "Tail Room Size", { 0.f, 1.f }, 0.5f, fm::pct, fm::pctIn));
    l.add (b (smpReverse, "Sample Reverse", true));
    l.add (f (smpRoot, "Sample Root", { 0.f, 127.f, 1.f }, 31.f, noteName, fm::num));
    l.add (f (smpStart, "Sample Start", { 0.f, 500.f }, 0.f, fm::ms, fm::msIn));
    // pitch
    l.add (c (pitchMode, "Pitch Mode", Choices::pitchModes, 0));
    l.add (f (pitchAmt, "Pitch Amount", { -24.f, 24.f }, 12.f, fm::semi, fm::num));
    l.add (f (pitchTime, "Pitch Time", skew (2.f, 400.f, 60.f), 60.f, fm::ms, fm::msIn));
    l.add (f (tune, "Tune", { -24.f, 24.f }, 0.f, fm::semi, fm::num));
    l.add (f (fine, "Fine", { -100.f, 100.f }, 0.f, fm::cents, fm::num));
    l.add (b (keyTrack, "Key Track", true));
    l.add (f (fixedNote, "Fixed Note", { 12.f, 72.f, 1.f }, 29.f, noteName, fm::num));
    // tone
    l.add (c (preType, "Pre Filter Type", Choices::filterTypes, 0));
    l.add (f (preHz, "Pre Filter Cutoff", skew (30.f, 16000.f, 900.f), 800.f, fm::hz, fm::hzIn));
    l.add (f (preReso, "Pre Filter Resonance", { 0.f, 1.f }, 0.2f, fm::pct, fm::pctIn));
    l.add (c (distType, "Distortion", Choices::distTypes, 0));
    l.add (f (drive, "Drive", { 0.f, 40.f }, 10.f, fm::db, fm::num));
    l.add (f (driveEnv, "Drive Envelope", { 0.f, 1.f }, 0.f, fm::pct, fm::pctIn));
    l.add (f (bias, "Asymmetry", { -1.f, 1.f }, 0.f, fm::bip, fm::pctIn));
    l.add (f (mix, "Harmonic Mix", { 0.f, 1.f }, 1.f, fm::pct, fm::pctIn));
    l.add (f (lowCut, "Low Cut", skew (20.f, 400.f, 60.f), 20.f, lowCutFmt, fm::hzIn));
    l.add (f (highCut, "High Cut", skew (200.f, 16000.f, 1500.f), 900.f, fm::hz, fm::hzIn));
    l.add (f (bellHz, "Bell Freq", skew (100.f, 6000.f, 700.f), 700.f, fm::hz, fm::hzIn));
    l.add (f (bellGain, "Bell Gain", { -18.f, 18.f }, 0.f, fm::db, fm::num));
    l.add (f (bellQ, "Bell Q", skew (0.3f, 8.f, 1.2f), 1.f, fmtNum, fm::num));
    l.add (f (subHz, "Sub Crossover", skew (20.f, 250.f, 90.f), 90.f, subFmt, fm::hzIn));
    l.add (f (subLevel, "Sub Level", { -60.f, 6.f }, 0.f, offDb, fm::num));
    l.add (f (clip, "Clip", { 0.f, 1.f }, 0.2f, fm::pct, fm::pctIn));
    l.add (f (level, "Level", { -36.f, 6.f }, -3.f, fm::db, fm::num));
    l.add (f (velocity, "Velocity Sensitivity", { 0.f, 1.f }, 0.f, fm::pct, fm::pctIn));
    l.add (f (uniVoices, "Unison Voices", { 1.f, 7.f, 1.f }, 5.f, [] (float v, int) { return v < 1.5f ? String ("OFF") : String (roundToInt (v)); }, fm::num));
    l.add (f (uniDetune, "Unison Detune", { 0.f, 60.f }, 18.f, fm::cents, fm::num));
    l.add (f (uniDrift, "Unison Drift", { 0.f, 30.f }, 6.f, fm::cents, fm::num));
    l.add (f (uniMix, "Unison Mix", { 0.f, 1.f }, 0.6f, fm::pct, fm::pctIn));
    l.add (f (stereoWidth, "Stereo Width", { 0.f, 1.f }, 0.4f, fm::pct, fm::pctIn));
    l.add (f (widthSplit, "Width From", skew (80.f, 1500.f, 350.f), 350.f, fm::hz, fm::hzIn));
    return l;
}

// ------------------------------------------------------------ factory presets + templates
namespace
{
    struct BassPreset { String name; PresetValues v; };
    std::vector<BassPreset> buildPresets();
    const std::vector<BassPreset>& presets() { static const std::vector<BassPreset> p = buildPresets(); return p; }
    // envelope templates (the envelope / step part of a preset)
    const std::vector<PresetValues>& templates()
    {
        static const std::vector<PresetValues> t {
            // PUMP: like the FHSP "reversed bass kicks" - ducked, back up fast (sidechain release), a level shelf, cut before the kick
            { { "timing", 0.f }, { "delay_pct", 36.f }, { "duck", 0.f }, { "initial", 0.f }, { "swell", 60.f }, { "curve", 0.6f }, { "hold_pct", 100.f },
              { "decay_pct", 0.f }, { "sustain", 1.f }, { "gate_pct", 98.f }, { "fade", 25.f } },
            // SWELL: the classic breathing one - slow rise, an early peak, a long fading tail
            { { "timing", 0.f }, { "delay_pct", 25.f }, { "duck", 0.f }, { "initial", 0.1f }, { "swell", 140.f }, { "curve", -0.4f }, { "hold_pct", 0.f },
              { "decay_pct", 60.f }, { "sustain", 0.f }, { "decay_curve", 2.2f }, { "gate_pct", 97.f }, { "fade", 15.f } },
            // TIGHT: short and compact
            { { "timing", 0.f }, { "delay_pct", 30.f }, { "duck", 0.f }, { "initial", 0.f }, { "swell", 15.f }, { "curve", 0.5f }, { "hold_pct", 5.f },
              { "decay_pct", 40.f }, { "sustain", 0.f }, { "decay_curve", 1.2f }, { "gate_pct", 90.f }, { "fade", 10.f } },
            // ROLL: long, spreading
            { { "timing", 0.f }, { "delay_pct", 20.f }, { "duck", 0.f }, { "initial", 0.2f }, { "swell", 60.f }, { "curve", 0.f }, { "hold_pct", 10.f },
              { "decay_pct", 90.f }, { "sustain", 0.3f }, { "decay_curve", 0.8f }, { "gate_pct", 99.f }, { "fade", 20.f } },
            // PULSE: fast and aggressive on 1/16
            { { "timing", 0.f }, { "step", 2.f }, { "delay_pct", 15.f }, { "duck", 0.f }, { "initial", 0.f }, { "swell", 10.f }, { "curve", 0.8f }, { "hold_pct", 30.f },
              { "decay_pct", 0.f }, { "sustain", 1.f }, { "gate_pct", 85.f }, { "fade", 6.f } } };
        return t;
    }
    PresetValues withTemplate (int t, PresetValues v) { auto out = templates()[(size_t) t]; for (auto& x : v) out.push_back (x); return out; }

    // the owner's Forge tails (QL, Volcanic, Undertow) as the TAIL source, with the bass settings of his BASS presets
    PresetValues qlTail()       { return { { "source", 1.f }, { "tail_start", 4.99f }, { "tail_drop", 32.f }, { "tail_curve", 4.42f }, { "tail_wave", 0.f }, { "tail_dist", 4.f },
                                           { "tail_drive", 29.5f }, { "tail_tone", 0.214f }, { "tail_highcut", 8557.f }, { "tail_scream", 0.406f }, { "tail_scream_hz", 1240.f }, { "tune", 1.35f } }; }
    PresetValues volcanicTail() { return { { "source", 1.f }, { "tail_start", 5.27f }, { "tail_drop", 186.f }, { "tail_curve", 2.17f }, { "tail_wave", 0.f }, { "tail_dist", 2.f },
                                           { "tail_drive", 46.f }, { "tail_tone", -0.079f }, { "tail_highcut", 10198.f }, { "tail_scream", 0.769f }, { "tail_scream_hz", 643.f } }; }
    PresetValues undertowTail() { return { { "source", 1.f }, { "tail_start", 3.81f }, { "tail_drop", 87.8f }, { "tail_curve", 2.2f }, { "tail_wave", 0.17f }, { "tail_dist", 1.f },
                                           { "tail_drive", 46.4f }, { "tail_tone", 0.952f }, { "tail_highcut", 6336.f }, { "tail_scream", 0.624f }, { "tail_scream_hz", 1500.f } }; }
    PresetValues plus (PresetValues a, const PresetValues& b) { for (auto& x : b) a.push_back (x); return a; }
    const PresetValues forgeBass { { "step", 0.f }, { "drive", 10.f }, { "high_cut", 9000.f }, { "tail_room", 0.35f }, { "tail_room_size", 0.5f }, { "level", -4.f }, { "sub_hz", 90.f }, { "sub_level", 0.f }, { "fade", 25.f }, { "swell", 60.f } };

    std::vector<BassPreset> buildPresets()
    {
        std::vector<BassPreset> L;
        L.push_back ({ "INIT - Pump Tail", withTemplate (0, { { "level", -5.5f } }) });
        // the owner's own basses (2026-10-28: "w miarę spoko") - exactly as he saved them
        L.push_back ({ "OWNER - 1", { { "beats", 0.f }, { "bell_gain", 8.3146f }, { "bell_hz", 1959.1f }, { "bell_q", 1.2366f }, { "bias", 0.31818f }, { "blend", 0.069694f }, { "clip", 0.64545f }, { "curve", -0.65455f }, { "decay_curve", 6.f }, { "decay_ms", 0.f }, { "decay_pct", 46.364f }, { "delay_ms", 120.f }, { "delay_pct", 25.864f }, { "dist_type", 1.f }, { "drive", 14.858f }, { "drive_env", 0.18953f }, { "duck", 0.f }, { "fade", 30.948f }, { "fine", 0.f }, { "fixed_note", 34.f }, { "gate_ms", 380.f }, { "gate_pct", 100.f }, { "high_cut", 16000.f }, { "hold_ms", 400.f }, { "hold_pct", 0.f }, { "initial", 0.f }, { "key_track", 1.f }, { "level", -0.65454f }, { "low_cut", 21.622f }, { "mix", 1.f }, { "offset", 0.f }, { "pitch_amt", -24.f }, { "pitch_mode", 3.f }, { "pitch_time", 8.4857f }, { "pre_hz", 13931.f }, { "pre_reso", 0.65455f }, { "pre_type", 1.f }, { "sine", -6.f }, { "smp_reverse", 1.f }, { "smp_root", 31.f }, { "smp_start", 0.f }, { "source", 0.f }, { "step", 0.f }, { "step1", 1.f }, { "step2", 1.f }, { "step3", 1.f }, { "step4", 1.f }, { "sub_hz", 85.537f }, { "sub_level", 0.9f }, { "sustain", 0.4f }, { "swell", 140.f }, { "tail_curve", 2.6405f }, { "tail_dist", 3.f }, { "tail_drive", 29.349f }, { "tail_drop", 56.132f }, { "tail_highcut", 8557.f }, { "tail_len", 300.f }, { "tail_scream", 0.f }, { "tail_scream_hz", 684.79f }, { "tail_start", 6.4963f }, { "tail_tone", 0.30015f }, { "tail_wave", 0.f }, { "timing", 0.f }, { "trigger", 0.f }, { "tune", 12.f }, { "velocity", 0.69091f }, { "warp", 1.f }, { "warp_amt", 0.28198f }, { "warp_env", 0.1422f }, { "width", 0.84203f } } });
        L.push_back ({ "OWNER - 2", { { "beats", 0.f }, { "bell_gain", 8.3146f }, { "bell_hz", 1959.1f }, { "bell_q", 1.2366f }, { "bias", 0.31818f }, { "blend", 0.f }, { "clip", 0.64545f }, { "curve", -1.f }, { "decay_curve", 6.f }, { "decay_ms", 0.f }, { "decay_pct", 25.909f }, { "delay_ms", 120.f }, { "delay_pct", 24.136f }, { "dist_type", 1.f }, { "drive", 14.858f }, { "drive_env", 0.18953f }, { "duck", 0.f }, { "fade", 95.397f }, { "fine", 0.f }, { "fixed_note", 34.f }, { "gate_ms", 380.f }, { "gate_pct", 100.f }, { "high_cut", 16000.f }, { "hold_ms", 400.f }, { "hold_pct", 11.818f }, { "initial", 0.f }, { "key_track", 1.f }, { "level", -0.65454f }, { "low_cut", 21.622f }, { "mix", 1.f }, { "offset", 0.f }, { "pitch_amt", -24.f }, { "pitch_mode", 3.f }, { "pitch_time", 79.335f }, { "pre_hz", 13931.f }, { "pre_reso", 0.65455f }, { "pre_type", 1.f }, { "sine", -6.f }, { "smp_reverse", 1.f }, { "smp_root", 31.f }, { "smp_start", 0.f }, { "source", 0.f }, { "step", 0.f }, { "step1", 1.f }, { "step2", 1.f }, { "step3", 1.f }, { "step4", 1.f }, { "sub_hz", 85.537f }, { "sub_level", 0.9f }, { "sustain", 0.4f }, { "swell", 145.16f }, { "tail_curve", 5.2746f }, { "tail_dist", 2.f }, { "tail_drive", 35.24f }, { "tail_drop", 194.21f }, { "tail_highcut", 8557.f }, { "tail_len", 300.f }, { "tail_scream", 0.090909f }, { "tail_scream_hz", 684.79f }, { "tail_start", 6.0979f }, { "tail_tone", 0.51834f }, { "tail_wave", 0.77727f }, { "timing", 0.f }, { "trigger", 0.f }, { "tune", 12.f }, { "velocity", 0.69091f }, { "warp", 3.f }, { "warp_amt", 0.17273f }, { "warp_env", 0.21947f }, { "width", 0.84203f } } });
        L.push_back ({ "PUMP - QL Tail", withTemplate (0, plus (qlTail(), forgeBass)) });
        L.push_back ({ "PUMP - Volcanic Tail [F]", withTemplate (0, plus (volcanicTail(), plus (forgeBass, { { "level", -9.f } }))) });
        L.push_back ({ "PUMP - Undertow Tail [G]", withTemplate (0, plus (undertowTail(), plus (forgeBass, { { "level", -8.f } }))) });
        L.push_back ({ "SWELL - QL Tail", withTemplate (1, plus (qlTail(), { { "drive", 12.f }, { "high_cut", 1100.f }, { "drive_env", 0.6f }, { "level", -6.f } })) });
        L.push_back ({ "SWELL - Synth Bend", withTemplate (1, { { "source", 0.f }, { "warp", 1.f }, { "warp_amt", 0.55f }, { "warp_env", 0.8f }, { "blend", 0.2f },
                                                                 { "drive", 14.f }, { "dist_type", 1.f }, { "drive_env", 0.5f }, { "high_cut", 1400.f }, { "bell_gain", 2.5f } }) });
        L.push_back ({ "TIGHT - Synth Squeeze 1/8", withTemplate (2, { { "source", 0.f }, { "step", 1.f }, { "warp", 2.f }, { "warp_amt", 0.6f }, { "warp_env", 0.6f },
                                                                        { "drive", 16.f }, { "dist_type", 0.f }, { "high_cut", 1600.f }, { "bell_gain", 2.f } }) });
        L.push_back ({ "ROLL - Synth Mirror", withTemplate (3, { { "source", 0.f }, { "warp", 4.f }, { "warp_amt", 0.7f }, { "warp_env", 0.3f }, { "blend", 0.5f },
                                                                  { "drive", 12.f }, { "dist_type", 4.f }, { "bias", 0.3f }, { "high_cut", 1200.f } }) });
        L.push_back ({ "PULSE - Synth Sync Offbeats", withTemplate (4, { { "source", 0.f }, { "step1", 0.f }, { "step3", 0.f }, { "warp", 3.f }, { "warp_amt", 0.5f },
                                                                          { "warp_env", 1.f }, { "drive", 18.f }, { "dist_type", 2.f }, { "high_cut", 2200.f }, { "bell_gain", 3.f }, { "level", -9.f } }) });
        L.push_back ({ "RISE - QL Tail Pitch Up", withTemplate (0, plus (qlTail(), plus (forgeBass, { { "source", 1.f }, { "pitch_mode", 2.f }, { "pitch_amt", 5.f }, { "pitch_time", 90.f } }))) });
        L.push_back ({ "GATE - Bass Line (notes)", { { "trigger", 1.f }, { "timing", 1.f }, { "delay_ms", 0.f }, { "swell", 25.f }, { "curve", 0.5f }, { "hold_ms", 4000.f },
                                                     { "decay_ms", 0.f }, { "fade", 20.f }, { "source", 0.f }, { "blend", 0.3f }, { "warp", 1.f }, { "warp_amt", 0.35f }, { "warp_env", 0.f },
                                                     { "drive", 12.f }, { "high_cut", 1300.f } } });
        return L;
    }
}

const StringArray& ReProcessor::templateNames() { static const StringArray n { "Pump", "Swell", "Tight", "Roll", "Pulse" }; return n; }
void ReProcessor::applyTemplate (int t)
{
    if (! isPositiveAndBelow (t, (int) templates().size())) return;
    for (auto& [id, v] : templates()[(size_t) t]) set (id.toRawUTF8(), v);
    if (t != 4 && get (ParamIDs::step) > 1.5f) set (ParamIDs::step, 0.f);        // PULSE set 1/16; the others go back to 1/4
}
void ReProcessor::set (const char* id, float v)
{
    if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); }
}

int ReProcessor::getNumPrograms() { return (int) presets().size(); }
const String ReProcessor::getProgramName (int i) { return isPositiveAndBelow (i, getNumPrograms()) ? presets()[(size_t) i].name : String(); }
void ReProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    presetName = presets()[(size_t) i].name;
    PresetIO::apply (*this, apvts, presets()[(size_t) i].v);
}

bool ReProcessor::loadUserPresetFile (const File& f)
{
    auto xml = XmlDocument::parse (f);
    if (xml == nullptr || ! PresetIO::load (*this, apvts, f)) return false;
    loadSample (File (xml->getStringAttribute ("samplePath")));
    presetName = f.getFileNameWithoutExtension();
    return true;
}
bool ReProcessor::saveUserPresetAs (const String& name)
{
    String clean;
    if (! io.save (apvts, name, clean)) return false;
    presetName = clean;
    return true;
}
void ReProcessor::getStateInformation (MemoryBlock& d) { PresetIO::getState (apvts, d, currentProgram, presetName); }
void ReProcessor::setStateInformation (const void* data, int size)
{
    if (PresetIO::setState (*this, apvts, data, size, currentProgram, presetName)) loadSample (File (apvts.state.getProperty ("samplePath").toString()));
}

void ReProcessor::randomizeSound()
{
    using namespace ParamIDs;
    auto between = [this] (float a, float b) { return a + rng.nextFloat() * (b - a); };
    auto logB = [this] (float a, float b) { return a * std::pow (b / a, rng.nextFloat()); };
    applyTemplate (rng.nextInt (5));
    const bool tail = rng.nextFloat() < 0.5f;
    set (source, tail ? 1.f : 0.f);
    if (tail)
    {
        set (tailStart, logB (2.5f, 6.f)); set (tailDrop, logB (25.f, 180.f)); set (tailCurve, between (2.f, 5.f)); set (tailDist, (float) rng.nextInt (5));
        set (tailDrive, between (20.f, 44.f)); set (tailTone, between (-0.2f, 0.9f)); set (tailScream, rng.nextFloat() < 0.6f ? between (0.2f, 0.8f) : 0.f); set (tailScreamHz, logB (500.f, 1800.f));
    }
    else
    {
        set (warp, (float) rng.nextInt (5)); set (warpAmt, between (0.2f, 0.8f)); set (warpEnv, between (0.f, 1.f)); set (blend, between (0.f, 0.8f)); set (width, between (0.2f, 0.6f));
    }
    set (distType, (float) rng.nextInt (6)); set (ParamIDs::drive, between (6.f, 20.f)); set (driveEnv, between (0.f, 0.8f)); set (bias, rng.nextFloat() < 0.3f ? between (-0.5f, 0.5f) : 0.f);
    set (highCut, logB (1500.f, 16000.f)); set (bellHz, logB (1200.f, 2500.f)); set (bellGain, between (0.f, 8.f));   // the owner likes presence ~2 kHz (OWNER - 1 / 2)
    set (subHz, between (70.f, 120.f)); set (subLevel, 0.f); set (lowCut, 20.f);       // the low end stays (owner's rule)
    presetName = "Random #" + String (1000 + rng.nextInt (9000));
}

ReProcessor::ReProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "RE4GE", createLayout())
{
    setCurrentProgram (0);
}

void ReProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const SpinLock::ScopedLockType l (prepareLock);
    engine.setParams (gather());
    engine.prepare (sampleRate, samplesPerBlock);
}

re::Params ReProcessor::gather() const
{
    using namespace ParamIDs;
    auto ch = [this] (const char* id, int hi) { return jlimit (0, hi, (int) std::round (get (id))); };
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
    return p;
}

void ReProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    buffer.clear();
    const SpinLock::ScopedTryLockType prepared (prepareLock);
    if (! prepared.isLocked()) return;
    engine.setParams (gather());
    double bpmNow = 150.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bp = pos->getBpm()) bpmNow = *bp;
    engine.setTempo (bpmNow);
    if (pendingAudition.exchange (false)) engine.audition (auditionNote);
    engine.process (buffer, midi);
}

// message thread: a WAV / AIFF / FLAC into a mono buffer (max 4 s) for the SAMPLE source
bool ReProcessor::loadSample (const File& f)
{
    if (! f.existsAsFile()) { if (f.getFullPathName().isEmpty()) clearSample(); return false; }
    AudioFormatManager afm; afm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> r (afm.createReaderFor (f));
    if (r == nullptr) return false;
    const int len = (int) std::min<long long> ((long long) r->lengthInSamples, (long long) (r->sampleRate * 4.0));
    const int nch = r->numChannels > 0 ? (int) r->numChannels : 1;
    AudioBuffer<float> b (nch, len);
    r->read (&b, 0, len, 0, true, true);
    auto s = std::make_unique<re::Sample>();
    s->rate = r->sampleRate; s->name = f.getFileNameWithoutExtension();
    s->data.resize ((size_t) len);
    for (int i = 0; i < len; ++i)
    {
        float v = 0.f;
        for (int c = 0; c < b.getNumChannels(); ++c) v += b.getSample (c, i);
        s->data[(size_t) i] = v / (float) b.getNumChannels();
    }
    engine.setSample (std::move (s));
    apvts.state.setProperty ("samplePath", f.getFullPathName(), nullptr);
    sampleSerial.fetch_add (1);
    return true;
}
void ReProcessor::clearSample()
{
    engine.setSample (nullptr);
    apvts.state.setProperty ("samplePath", String(), nullptr);
    sampleSerial.fetch_add (1);
}
String ReProcessor::sampleName() const { auto* s = engine.currentSample(); return s != nullptr ? s->name : String(); }

AudioProcessorEditor* ReProcessor::createEditor() { return new ReEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReProcessor(); }
