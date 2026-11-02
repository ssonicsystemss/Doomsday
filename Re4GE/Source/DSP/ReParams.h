#pragma once
// RE4GE - the engine's settings (filled from the parameters by Parameters/ParamBridge; DSP knows no APVTS / GUI / files)
#include "GeKick.h"       // (Shared) ge::Params: the kick tail settings
#include <array>
#include "DoomsdayMod.h"     // (Shared) mod::Shape: the drawn curves
#include "DoomsdayLab.h"     // (Shared) lab:: DISTORTION LAB curves with ADAA

namespace re
{
    enum Source { SrcSynth = 0, SrcTail = 1, SrcSample = 2 };
    enum PitchMode { Stable = 0, Drop = 1, Rise = 2, Bounce = 3 };
    enum Warp { WarpOff = 0, Bend = 1, Squeeze = 2, Sync = 3, Mirror = 4 };

    struct Params
    {
        // trigger / timing
        bool gate = false, sync = true;
        double stepBeats = 1.0;                 // 1 = 1/4, 0.5 = 1/8, 0.25 = 1/16
        int mask = 15, beats = 1;               // MASK bit i = step i of each beat plays
        float offsetPct = 0.f;                  // shifts the steps (0..50 % of a step)
        // envelope
        float delayPct = 36.f, holdPct = 100.f, decayPct = 0.f, gatePct = 98.f;
        float delayMs = 120.f, holdMs = 400.f, decayMs = 0.f, gateMs = 380.f;
        float duck = 0.f, initial = 0.f, swellMs = 60.f, curve = 0.6f, sustain = 1.f, decayCurve = 1.5f, fadeMs = 25.f;
        // source
        int source = SrcTail;
        float blend = 0.3f, width = 0.5f, sineDb = -6.f;
        int warp = WarpOff; float warpAmt = 0.f, warpEnv = 0.f;
        ge::Params tail;                        // the kick tail (only the tail / distortion part is used)
        float tailLenMs = 300.f;                // GATE mode: how much tail is played backwards
        float tailRoom = 0.f, tailRoomSize = 0.5f;   // a room on the tail BEFORE it is reversed: backwards it swells up
        bool sampleReverse = true; int root = 31; float sampleStartMs = 0.f;
        // pitch
        int pitchMode = Stable; float pitchSt = 12.f, pitchMs = 60.f, tuneSt = 0.f, fineCt = 0.f;
        bool keyTrack = true; int fixedNote = 29;
        // tone
        int preType = 0; float preHz = 800.f, preReso = 0.2f;
        int distType = 0; float driveDb = 10.f, driveEnv = 0.f, bias = 0.f, mix = 1.f;
        float lowCutHz = 20.f, highCutHz = 900.f, bellHz = 700.f, bellDb = 0.f, bellQ = 1.f;
        float subHz = 90.f, subDb = 0.f;        // SUB: below subHz the bass is a clean sine (20 = off)
        float clip = 0.2f, levelDb = -3.f, velocitySens = 0.f;
        // UNISON / WIDTH: the centre chain is plain (its lows below SPLIT + the SUB stay mono and stable),
        // the left / right chains add detuned, drifting copies (SYNTH: oscillators, TAIL / SAMPLE: extra read heads) for the mids / highs
        int uniVoices = 1; float uniDetuneCt = 18.f, driftCt = 6.f, uniMix = 0.6f, stereoWidth = 0.4f, splitHz = 350.f;

        // ---- v0.4 (step 1: the core) ----
        float osc1Db = 0.f;                     // OSC 1 = the source above (SYNTH / TAIL / SAMPLE)
        struct Osc { int wave = 2; float levelDb = -60.f, oct = 0.f, semi = 0.f, fineCt = 0.f, phaseDeg = 0.f, width = 0.5f, pm = 0.f; bool sync = false; };
        std::array<Osc, 3> osc {};              // OSC 2 / 3 / 4 (levelDb <= -59 = off)
        bool phaseFree = false;                 // PHASE: RESET (lock, every step the same) / FREE
        float preEnvOct = 0.f, preKey = 0.f;    // PRE FILTER: cutoff follows the envelope (octaves; the knob = cutoff at the peak) + key track
        int dist2Type = 1; float dist2Db = 0.f, dist2Mix = 1.f;   // STAGE 2 after the bell (0 dB = off)
        std::array<float, 8> macro {};          // BODY / WEIGHT / REVERSE / PITCH / GRIT / BITE / MOVEMENT / SPACE (0 = the modules as set)
        // set by the macros (not knobs): extra pitch movement, the BODY / BITE bells
        float extraRiseSt = 0.f, extraDropSt = 0.f, bodyDb = 0.f, biteDb = 0.f;
        // v0.5 (step 1, part 2)
        bool subOctDown = false; float subHarm = 0.f, subSat = 0.f;      // SUB: an octave down, clean 2nd / 3rd harmonics, saturation
        float clickDb = -60.f, clickHz = 2000.f, clickMs = 8.f; int clickAt = 0;   // CLICK: noise tick (own HP), outside the envelope / drive; START / PEAK
        float glideMs = 0.f;                    // GLIDE: the pitch slides from the last note
        float smear = 0.f, smearSize = 0.5f;    // SMEAR: a short diffuser on the mids / highs (fills between the harmonics = density)
        // v0.6 (step 2): REVERSE SHAPE DESIGNER lanes AMP / PITCH / HARMONIC / PHASE (ShapeLanes.h)
        struct LaneSet { bool on = false; float lenPct = 100.f; int anchor = 0; bool loop = false; float depth = 1.f; doomsday::mod::Shape shape; };
        std::array<LaneSet, 4> lane {};
        // v0.7 (step 3): DISTORTION LAB per stage (algo -1 = CLASSIC: the 4GE types above), QUALITY
        struct LabSet { int algo = -1; float hard = 0.5f, tone = 0.f, outDb = 0.f; bool tonePost = false, symmetric = true; };
        std::array<LabSet, 2> lab {};
        float dist2Bias = 0.f;
        bool autoGain = true;
        int qualityRt = 1, qualityExport = 2;      // 0 = 2x, 1 = 4x, 2 = 8x, 3 = 16x oversampling
        std::array<doomsday::mod::Shape, 2> driveCurve {};   // CUSTOM curves (stage 1 / 2)
        int curveSerial = 0;                       // changes when a drawn curve changes (the tables are rebuilt then)
        // v0.8 (step 4): LFO 1-4 + MOD MATRIX (ModMatrix.h)
        struct LfoSet { int wave = 0; bool sync = true, retrig = true; float hz = 2.f; int div = 9; float phase = 0.f; };
        std::array<LfoSet, 4> lfo {};
        struct ModRow { bool on = false; int src = 0, dst = 0, polarity = 0; float amount = 0.f; };
        std::array<ModRow, 16> mtx {};
        float modSt = 0.f;                         // set by the matrix: pitch offset in semitones (not a knob)
        // v0.9 (step 5): KICK-TO-REVERSE - the SAMPLE region (START .. END ms) and ORIGINAL (the kick as it is, no distortion stages)
        float sampleEndMs = 4000.f;
        bool sampleOriginal = false;
        bool stagesOff = false;                    // set by the engine (SAMPLE + ORIGINAL): both distortion stages bypassed
        bool sampleKeepLength = false;             // PITCH: VARISPEED (higher = shorter) / KEEP LENGTH (WSOLA grains, GrainReader.h)
    };
    enum OscWave { WSine = 0, WTri = 1, WSaw = 2, WSquare = 3, WPulse = 4, WNoise = 5 };
}
