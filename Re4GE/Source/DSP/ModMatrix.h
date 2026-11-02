#pragma once
// RE4GE - MODULATION (step 4, signature feature 4): LFO 1-4 + a 16-row MATRIX.
//   A row: ON, SOURCE, DESTINATION, AMOUNT (-1..+1 of the destination's range), POLARITY (AS IS / UNI 0..1 / BI -1..+1).
//   Several rows on one destination add up; the result is clamped to the destination's limits.
//   Evaluated once per 32-sample grid point (~1.4 kHz control rate - smooth for LFOs up to ~30 Hz) on a copy of the settings,
//   so the knobs themselves never move (automation and presets stay clean). The MACROS' fixed targets still work on top.
#include "ReParams.h"

namespace re
{
    enum ModSrc { SrcOff = 0, SrcLfo1, SrcLfo2, SrcLfo3, SrcLfo4, SrcAmpEnv, SrcShapePitch, SrcShapeHarm, SrcShapePhase,
                  SrcVelocity, SrcModWheel, SrcAftertouch, SrcPitchBend, SrcKey,
                  SrcMacro1, SrcMacro2, SrcMacro3, SrcMacro4, SrcMacro5, SrcMacro6, SrcMacro7, SrcMacro8,
                  SrcRandom, SrcStep, NumModSrc };
    enum ModDst { DstNone = 0, DstOsc1Level, DstOsc2Level, DstOsc3Level, DstOsc4Level, DstPitch, DstFine, DstCutoff, DstReso,
                  DstDrive, DstDrive2, DstAsym, DstWarp, DstPm, DstBlend, DstPulseWidth, DstBell, DstHighCut, DstSubLevel,
                  DstWidth, DstSmear, DstDetune, DstOutput, DstHard1, DstHard2, DstTone1, DstTone2, DstClick, DstSwell, NumModDst };

    inline const juce::StringArray& modSourceNames()
    {
        static const juce::StringArray n { "OFF", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "AMP ENV", "SHAPE PITCH", "SHAPE HARM", "SHAPE PHASE",
                                           "VELOCITY", "MOD WHEEL", "AFTERTOUCH", "PITCH BEND", "KEY",
                                           "MACRO BODY", "MACRO WEIGHT", "MACRO REVERSE", "MACRO PITCH", "MACRO GRIT", "MACRO BITE", "MACRO MOVEMENT", "MACRO SPACE",
                                           "RANDOM (step)", "STEP" };
        return n;
    }
    inline const juce::StringArray& modDestNames()
    {
        static const juce::StringArray n { "NONE", "OSC 1 level", "OSC 2 level", "OSC 3 level", "OSC 4 level", "Pitch (+-24 st)", "Fine (+-100 ct)",
                                           "Filter cutoff (+-5 oct)", "Filter reso", "Drive (+-24 dB)", "Stage 2 drive", "Asym", "Phase warp", "OSC 2-4 PM",
                                           "Saw / pulse", "Pulse width", "Bell gain", "High cut (+-5 oct)", "Sub level", "Stereo width", "Smear",
                                           "Unison detune", "Output (+-24 dB)", "Lab 1 hardness", "Lab 2 hardness", "Lab 1 tone", "Lab 2 tone",
                                           "Click level", "Swell (+-300 ms)" };
        return n;
    }

    // the sources at this grid point (the engine fills them)
    struct ModSources { std::array<float, NumModSrc> v {}; };

    // adds the matrix to a copy of the settings
    inline void applyMatrix (Params& p, const ModSources& s)
    {
        std::array<float, NumModDst> d {};
        bool any = false;
        for (const auto& r : p.mtx)
        {
            if (! r.on || r.src <= SrcOff || r.dst <= DstNone || r.src >= NumModSrc || r.dst >= NumModDst || r.amount == 0.f) continue;
            float x = s.v[(size_t) r.src];
            const bool bipolarSrc = r.src <= SrcLfo4 || r.src == SrcPitchBend || r.src == SrcKey || r.src == SrcRandom || r.src == SrcShapePitch;
            if (r.polarity == 1 && bipolarSrc) x = 0.5f * (x + 1.f);           // UNI: 0..1
            else if (r.polarity == 2 && ! bipolarSrc) x = 2.f * x - 1.f;        // BI: -1..1
            d[(size_t) r.dst] += x * r.amount;
            any = true;
        }
        if (! any) return;
        auto db = [] (float& v, float m, float lo, float hi) { if (m != 0.f) v = juce::jlimit (lo, hi, (v <= -59.f ? -36.f : v) + m); };
        auto lin = [] (float& v, float m, float lo, float hi) { if (m != 0.f) v = juce::jlimit (lo, hi, v + m); };
        auto oct = [] (float& v, float m, float lo, float hi) { if (m != 0.f) v = juce::jlimit (lo, hi, v * std::exp2 (m)); };
        db (p.osc1Db, 24.f * d[DstOsc1Level], -60.f, 6.f);
        for (int i = 0; i < 3; ++i) db (p.osc[(size_t) i].levelDb, 24.f * d[(size_t) (DstOsc2Level + i)], -60.f, 6.f);
        p.modSt += 24.f * d[DstPitch] + d[DstFine];
        oct (p.preHz, 5.f * d[DstCutoff], 30.f, 16000.f);
        lin (p.preReso, d[DstReso], 0.f, 1.f);
        lin (p.driveDb, 24.f * d[DstDrive], 0.f, 48.f);
        lin (p.dist2Db, 24.f * d[DstDrive2], 0.f, 40.f);
        lin (p.bias, d[DstAsym], -1.f, 1.f);
        lin (p.warpAmt, d[DstWarp], 0.f, 1.f);
        if (d[DstPm] != 0.f) for (auto& o : p.osc) o.pm = juce::jlimit (0.f, 1.f, o.pm + d[DstPm]);
        lin (p.blend, d[DstBlend], 0.f, 1.f);
        lin (p.width, 0.45f * d[DstPulseWidth], 0.05f, 0.95f);
        lin (p.bellDb, 18.f * d[DstBell], -18.f, 18.f);
        oct (p.highCutHz, 5.f * d[DstHighCut], 200.f, 20000.f);
        db (p.subDb, 24.f * d[DstSubLevel], -60.f, 6.f);
        lin (p.stereoWidth, d[DstWidth], 0.f, 1.f);
        lin (p.smear, d[DstSmear], 0.f, 1.f);
        lin (p.uniDetuneCt, 60.f * d[DstDetune], 0.f, 80.f);
        lin (p.levelDb, 24.f * d[DstOutput], -60.f, 6.f);
        lin (p.lab[0].hard, d[DstHard1], 0.f, 1.f);
        lin (p.lab[1].hard, d[DstHard2], 0.f, 1.f);
        lin (p.lab[0].tone, 2.f * d[DstTone1], -1.f, 1.f);
        lin (p.lab[1].tone, 2.f * d[DstTone2], -1.f, 1.f);
        db (p.clickDb, 36.f * d[DstClick], -60.f, 0.f);
        lin (p.swellMs, 300.f * d[DstSwell], 2.f, 400.f);
    }
}
