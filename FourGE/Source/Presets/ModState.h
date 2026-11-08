#pragma once
// =====================================================================
//  4GE module 09 - the MODULATION MATRIX as saved data (state child MODM; never GUI, never the audio thread):
//   64 slots (SOURCE, DESTINATION = the parameter's stable ID, DEPTH, OFFSET, POLARITY, CURVE, MODE, SMOOTHING, LAG, MUTE, SOLO, LABEL)
//   + the 64 steps of the step sequencer. No MODM child = the DEFAULT MACRO MAPPING (old projects / presets get it too).
//   resolve() turns the IDs into parameter indices for the audio thread; an unknown or illegal target stays OFF and is counted.
// =====================================================================
#include <juce_data_structures/juce_data_structures.h>
#include "../DSP/Mod/Mod.h"

namespace fge::modstate
{
    using namespace fge::dsp::mod;
    struct SlotDesc
    {
        bool on = false, mute = false, solo = false;
        int src = SNone, pol = PUniPos, curve = CLinear, mode = MAdd;
        juce::String dst, label;
        float depth = 0.f, offset = 0.f, smoothMs = 0.f, lagMs = 0.f;
        bool empty() const { return src == SNone && dst.isEmpty(); }
        bool operator== (const SlotDesc& o) const { return toText() == o.toText(); }
        juce::String toText() const
        {
            juce::StringArray a { on ? "1" : "0", juce::String (src), dst, juce::String (depth, 4), juce::String (offset, 4), juce::String (pol), juce::String (curve),
                                  juce::String (mode), juce::String (smoothMs, 2), juce::String (lagMs, 2), mute ? "1" : "0", solo ? "1" : "0",
                                  label.replaceCharacter ('|', '/') };
            return a.joinIntoString ("|");
        }
        static SlotDesc fromText (const juce::String& t)
        {
            SlotDesc s;
            const auto a = juce::StringArray::fromTokens (t, "|", "");
            if (a.size() < 12) return s;
            s.on = a[0] == "1"; s.src = juce::jlimit (0, NumSrc - 1, a[1].getIntValue()); s.dst = a[2];
            auto fl = [] (const juce::String& x, float lo, float hi) { const float v = x.getFloatValue(); return std::isfinite (v) ? juce::jlimit (lo, hi, v) : 0.f; };
            s.depth = fl (a[3], -1.f, 1.f); s.offset = fl (a[4], -1.f, 1.f);
            s.pol = juce::jlimit (0, 2, a[5].getIntValue()); s.curve = juce::jlimit (0, 4, a[6].getIntValue()); s.mode = juce::jlimit (0, 4, a[7].getIntValue());
            s.smoothMs = fl (a[8], 0.f, 2000.f); s.lagMs = fl (a[9], 0.f, 5000.f); s.mute = a[10] == "1"; s.solo = a[11] == "1";
            if (a.size() > 12) s.label = a[12];
            return s;
        }
    };
    inline juce::String stepsToText (const std::array<Step, maxSteps>& st)
    {
        juce::StringArray a;
        for (auto& s : st) a.add (juce::String (s.value, 3) + "," + juce::String (s.prob, 3) + "," + (s.tie ? "1" : "0") + "," + (s.accent ? "1" : "0") + ","
                                   + (s.mute ? "1" : "0") + "," + juce::String (s.ratchet) + "," + juce::String (s.curve));
        return a.joinIntoString (";");
    }
    inline std::array<Step, maxSteps> stepsFromText (const juce::String& t)
    {
        std::array<Step, maxSteps> st {};
        const auto a = juce::StringArray::fromTokens (t, ";", "");
        for (int i = 0; i < juce::jmin (maxSteps, a.size()); ++i)
        {
            const auto b = juce::StringArray::fromTokens (a[i], ",", "");
            if (b.size() < 7) continue;
            auto& s = st[(size_t) i];
            s.value = juce::jlimit (-1.f, 1.f, b[0].getFloatValue()); s.prob = juce::jlimit (0.f, 1.f, b[1].getFloatValue());
            s.tie = b[2] == "1"; s.accent = b[3] == "1"; s.mute = b[4] == "1"; s.ratchet = juce::jlimit (1, 8, b[5].getIntValue()); s.curve = juce::jlimit (0, 4, b[6].getIntValue());
        }
        return st;
    }
    // CUSTOM LFO shape text "x,y;x,y;..." (x 0..1 ascending, y -1..1); empty = the default shape
    inline LfoShape defaultShape()
    {
        LfoShape s; const float xs[5] { 0.f, 0.2f, 0.45f, 0.6f, 0.8f }, ys[5] { 1.f, -0.3f, 0.6f, -1.f, 0.2f };
        s.n = 5; for (int i = 0; i < 5; ++i) { s.x[(size_t) i] = xs[i]; s.y[(size_t) i] = ys[i]; }
        return s;
    }
    inline juce::String shapeToText (const LfoShape& sh)
    {
        juce::StringArray a; for (int i = 0; i < sh.n; ++i) a.add (juce::String (sh.x[(size_t) i], 4) + "," + juce::String (sh.y[(size_t) i], 4));
        return a.joinIntoString (";");
    }
    inline LfoShape shapeFromText (const juce::String& t)
    {
        LfoShape sh;
        const auto a = juce::StringArray::fromTokens (t, ";", "");
        std::vector<std::pair<float, float>> p;
        for (auto& e : a) { const auto b = juce::StringArray::fromTokens (e, ",", ""); if (b.size() == 2) p.push_back ({ juce::jlimit (0.f, 0.9999f, b[0].getFloatValue()), juce::jlimit (-1.f, 1.f, b[1].getFloatValue()) }); }
        std::sort (p.begin(), p.end());
        for (auto& [x, y] : p) { if (sh.n >= maxPts) break; sh.x[(size_t) sh.n] = x; sh.y[(size_t) sh.n] = y; ++sh.n; }
        return sh.n > 0 ? sh : defaultShape();
    }
    inline juce::String floatsToCsv (const float* v, int n) { juce::StringArray a; for (int i = 0; i < n; ++i) a.add (juce::String (v[i], 4)); return a.joinIntoString (","); }
    inline bool csvToFloats (const juce::String& t, float* v, int n, float lo, float hi)
    {
        const auto a = juce::StringArray::fromTokens (t, ",", "");
        if (a.size() != n) return false;
        for (int i = 0; i < n; ++i) v[i] = juce::jlimit (lo, hi, a[i].getFloatValue());
        return true;
    }
    struct Desc
    {
        std::array<SlotDesc, numSlots> slot {};
        std::array<Step, maxSteps> step {};
        std::array<LfoShape, 8> lfo { defaultShape(), defaultShape(), defaultShape(), defaultShape(), defaultShape(), defaultShape(), defaultShape(), defaultShape() };
        std::array<float, 12> morphA {}, morphB {};
        bool hasA = false, hasB = false;
        std::array<float, 12> ccMacro { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };   // MIDI LEARN: the CC number of each macro (-1 = none)
    };

    // ---- the DEFAULT MACRO MAPPING (MASTER MACROS, PLAN "Odpowiedzi na pytania końcowe"): every macro at 0 = the sound is unchanged
    inline Desc defaults()
    {
        Desc d;
        struct M { int macro; const char* dst; float depth; };
        static const M map[] {
            { 0, "punch_level", 0.10f }, { 0, "punch_drive", 0.45f }, { 0, "tr_attack_level", 0.30f },          // IMPACT
            { 1, "body_level", 0.09f },  { 1, "body_decay", 0.20f },  { 1, "body_drive", 0.30f },               // BODY
            { 2, "tail_length", 0.25f }, { 2, "tail_decay", 0.30f },  { 2, "tail_release", 0.25f },             // TAIL
            { 3, "pe_start", 0.22f },    { 3, "pe_time", 0.20f },     { 3, "pe_curve", 0.30f },                 // PITCH
            { 4, "tail_drive", 0.25f },  { 4, "tail_st2_drive", 0.25f }, { 4, "body_drive", 0.25f }, { 4, "dist1_drive", 0.40f },   // GRIT
            { 5, "tail_resonance", 0.45f }, { 5, "scr_level", 0.40f }, { 5, "tail_mid_gain", 0.15f },           // SCREAM
            { 6, "sub_level", 0.10f },   { 6, "sub_drive", 0.40f },   { 6, "sub_length", 0.20f },               // WEIGHT
            { 7, "tail_uni_drift", 0.50f }, { 7, "tail_uni_detune", 0.35f }, { 7, "tail_pulse_width", 0.25f }, // CHAOS
        };
        int k = 0;
        for (auto& m : map)
        {
            auto& s = d.slot[(size_t) k++];
            s.on = true; s.src = SMacro1 + m.macro; s.dst = m.dst; s.depth = m.depth; s.pol = PUniPos; s.smoothMs = 20.f;
            s.label = sourceNames()[s.src] + " > " + juce::String (m.dst).replace ("_", " ").toUpperCase();
        }
        return d;
    }

    static constexpr const char* tag = "MODM";
    inline Desc fromTree (const juce::ValueTree& t)
    {
        if (! t.isValid()) return defaults();
        Desc d;
        for (int i = 0; i < numSlots; ++i) if (t.hasProperty ("s" + juce::String (i + 1))) d.slot[(size_t) i] = SlotDesc::fromText (t.getProperty ("s" + juce::String (i + 1)).toString());
        if (t.hasProperty ("steps")) d.step = stepsFromText (t.getProperty ("steps").toString());
        for (int i = 0; i < 8; ++i) if (t.hasProperty ("lfo" + juce::String (i + 1))) d.lfo[(size_t) i] = shapeFromText (t.getProperty ("lfo" + juce::String (i + 1)).toString());
        d.hasA = csvToFloats (t.getProperty ("morphA").toString(), d.morphA.data(), 12, 0.f, 1.f);
        d.hasB = csvToFloats (t.getProperty ("morphB").toString(), d.morphB.data(), 12, 0.f, 1.f);
        csvToFloats (t.getProperty ("ccMacro").toString(), d.ccMacro.data(), 12, -1.f, 127.f);
        return d;
    }
    // the parameter index of every ID (built once)
    inline int indexOf (const juce::String& id)
    {
        static const std::map<juce::String, int> m = [] { std::map<juce::String, int> x; for (int i = 0; i < P::count; ++i) x[def (i).id] = i; return x; }();
        const auto it = m.find (id);
        return it == m.end() ? -1 : it->second;
    }
    // -> the audio-thread data; 'unknown' = slots whose target is missing / not modulatable (they stay OFF)
    inline void resolve (const Desc& d, Data& out, int& unknown, uint32_t version)
    {
        unknown = 0;
        out = {};
        int last = 0;
        for (int i = 0; i < numSlots; ++i)
        {
            const auto& s = d.slot[(size_t) i];
            auto& o = out.slot[(size_t) i];
            o.on = s.on; o.mute = s.mute; o.solo = s.solo; o.src = s.src; o.pol = s.pol; o.curve = s.curve; o.mode = s.mode;
            o.depth = s.depth; o.offset = s.offset; o.smoothMs = s.smoothMs; o.lagMs = s.lagMs;
            o.dst = s.dst.isEmpty() ? -1 : indexOf (s.dst);
            if (s.dst.isNotEmpty() && ! modulatable (o.dst)) { o.dst = -1; ++unknown; }
            if (o.on && o.dst >= 0 && o.src > SNone) last = i + 1;
        }
        out.step = d.step;
        out.lfoShape = d.lfo;
        out.morphA = d.morphA; out.morphB = d.morphB; out.morphValid = d.hasA && d.hasB;
        out.active = last;
        out.version = version;
    }
}
