#pragma once
// =====================================================================
//  4GE module 09 (6) - the VARIANT RANDOMIZER (message thread only: tools / presets, never the audio thread, never GUI code)
//   STYLE   EARLY (clean / tonal tail, little drive, classic drop) / MODERN (dirty, controlled) / RAW (scream / metal / buzz / crush,
//           a lot of drive) / EXPERIMENTAL (everything) - every style has its own MUSICAL ranges per area (owner: RULES.md 5).
//   SCOPE   FULL KICK or one area (PUNCH / BODY / TAIL / SUB / PITCH / DRIVE / SCREECH);  LOCK = areas that never change.
//   AMOUNT  100 % = a new value inside the style's range, less = a step from the current value toward it (a variation).
//   SEED    the same seed + settings + starting sound = the same result;  REGENERATE = back to the sound before the last
//           randomize and the same seed again (try another STYLE / AMOUNT on the same starting point).
//   ROUTING (off by default): also a few LFO / sequencer / random connections in the free matrix slots.
//   The level is matched after every randomize (an offline render: louder is not better) - one click = one UNDO step.
// =====================================================================
#include "../PluginProcessor.h"

namespace fge::variant
{
    enum Style { Early = 0, Modern, Raw, Experimental, NumStyles };
    enum Area { APunch = 0, ABody, ATail, ASub, APitch, ADrive, AScreech, NumAreas };
    inline const juce::StringArray& styleNames() { static const juce::StringArray a { "EARLY", "MODERN", "RAW", "EXPERIMENTAL" }; return a; }
    inline const juce::StringArray& areaNames()  { static const juce::StringArray a { "PUNCH", "BODY", "TAIL", "SUB", "PITCH", "DRIVE", "SCREECH" }; return a; }
    inline const juce::StringArray& scopeNames() { static const juce::StringArray a { "FULL KICK", "PUNCH", "BODY", "TAIL", "SUB", "PITCH", "DRIVE", "SCREECH" }; return a; }

    struct Settings
    {
        int style = Modern, scope = 0;        // scope 0 = FULL KICK, 1.. = one area
        float amount = 1.f;
        int seed = 1;
        uint32_t locks = 0;                   // bit = Area
        bool routing = false;
    };
    // a range: a float [lo, hi], or (choices non-empty) one of the listed choice indices
    struct R { const char* id; float lo, hi; std::vector<int> choices {}; };
    using Table = std::array<std::vector<R>, NumAreas>;

    inline const Table& table (int style)
    {
        static const std::array<Table, NumStyles> t = []
        {
            std::array<Table, NumStyles> x;
            // ---- EARLY: classic, melodic, tonal body, simpler distortion, clear pitch drop
            x[Early][APunch]   = { { "punch_source", 0, 0, { 0, 1 } }, { "punch_pitch", 220, 420 }, { "punch_level", -12, -6 }, { "punch_drive", 0.f, 0.3f }, { "punch_length", 20, 40 } };
            x[Early][ABody]    = { { "body_link", 0, 0, { 1 } }, { "body_link_source", 0, 0, { 2 } }, { "body_follow_depth", 0, 0, { 0 } }, { "body_follow_offset", 0, 0, { 0, 0, 12 } },
                                   { "body_source", 0, 0, { 0, 1, 2 } }, { "body_level", -14, -6 }, { "body_drive", 0.1f, 0.45f }, { "body_decay", 80, 200 },
                                   { "body_tone", 1500, 4500 }, { "body_resonance", 0.f, 0.3f } };
            x[Early][ATail]    = { { "tail_character", 0, 0, { 0, 1, 2 } }, { "tail_source", 0, 0, { 0, 1 } }, { "tail_drive", 0.2f, 0.5f }, { "tail_tone", 2000, 6000 },
                                   { "tail_resonance", 0.f, 0.25f }, { "tail_mid_gain", 0, 3 }, { "tail_uni_voices", 1, 3 }, { "tail_decay", 400, 900 } };
            x[Early][ASub]     = { { "sub_level", -14, -8 }, { "sub_drive", 0.f, 0.2f }, { "sub_length", 300, 500 } };
            x[Early][APitch]   = { { "pe_mode", 0, 0, { 0, 2 } }, { "pe_start", 160, 300 }, { "pe_time", 60, 160 }, { "pe_curve", 0.2f, 0.5f } };
            x[Early][ADrive]   = { { "tail_st2_type", 0, 0, { 0, 1, 3 } }, { "tail_st2_drive", 0, 6 }, { "master_clip_drive", 0, 3 } };
            x[Early][AScreech] = { { "scr_on", 0, 0, { 0 } } };
            // ---- MODERN: strong, clean and controlled, precise transient, layered body
            x[Modern][APunch]   = { { "punch_source", 0, 0, { 0, 1, 2, 5 } }, { "punch_pitch", 250, 500 }, { "punch_level", -10, -4 }, { "punch_drive", 0.1f, 0.5f }, { "punch_length", 20, 45 } };
            x[Modern][ABody]    = { { "body_link", 0, 0, { 1 } }, { "body_link_source", 0, 0, { 2 } }, { "body_follow_depth", 0, 0, { 0 } }, { "body_follow_offset", 0, 0, { 0, 7, 12, 12, 19 } },
                                    { "body_source", 0, 0, { 1, 2, 3 } }, { "body_level", -13, -5 }, { "body_drive", 0.2f, 0.7f }, { "body_decay", 80, 220 },
                                    { "body_tone", 2000, 8000 }, { "body_resonance", 0.f, 0.4f } };
            x[Modern][ATail]    = { { "tail_character", 0, 0, { 2, 3 } }, { "tail_source", 0, 0, { 0, 1, 2 } }, { "tail_drive", 0.45f, 0.75f }, { "tail_tone", 4000, 10000 },
                                    { "tail_resonance", 0.05f, 0.35f }, { "tail_mid_gain", 0, 5 }, { "tail_uni_voices", 3, 5 }, { "tail_decay", 500, 1000 } };
            x[Modern][ASub]     = { { "sub_level", -12, -6 }, { "sub_drive", 0.f, 0.3f }, { "sub_length", 300, 550 } };
            x[Modern][APitch]   = { { "pe_mode", 0, 0, { 0, 1 } }, { "pe_start", 200, 380 }, { "pe_time", 40, 110 }, { "pe_curve", 0.15f, 0.45f } };
            x[Modern][ADrive]   = { { "tail_st2_type", 0, 0, { 0, 1, 2, 3 } }, { "tail_st2_drive", 3, 12 }, { "master_clip_drive", 0, 6 } };
            x[Modern][AScreech] = { { "scr_on", 0, 0, { 0, 0, 0, 1 } }, { "scr_level", -18, -9 } };
            // ---- RAW: aggressive, strong distortion, sharp screech, asymmetry, raw character
            x[Raw][APunch]   = { { "punch_source", 0, 0, { 0, 2, 3, 5 } }, { "punch_pitch", 300, 600 }, { "punch_level", -9, -3 }, { "punch_drive", 0.3f, 0.7f }, { "punch_length", 15, 40 } };
            x[Raw][ABody]    = { { "body_link", 0, 0, { 1 } }, { "body_link_source", 0, 0, { 2 } }, { "body_follow_depth", 0, 0, { 0 } }, { "body_follow_offset", 0, 0, { 0, 7, 12, 19, 24 } },
                                 { "body_source", 0, 0, { 2, 3, 4 } }, { "body_level", -12, -2 }, { "body_drive", 0.4f, 1.f }, { "body_decay", 60, 260 },
                                 { "body_tone", 2500, 12000 }, { "body_resonance", 0.f, 0.6f } };
            x[Raw][ATail]    = { { "tail_character", 0, 0, { 3, 4, 5, 6 } }, { "tail_source", 0, 0, { 0, 2, 3 } }, { "tail_drive", 0.65f, 0.95f }, { "tail_tone", 7000, 15000 },
                                 { "tail_resonance", 0.2f, 0.6f }, { "tail_mid_gain", 2, 7 }, { "tail_uni_voices", 3, 7 }, { "tail_decay", 500, 1100 } };
            x[Raw][ASub]     = { { "sub_level", -12, -6 }, { "sub_drive", 0.f, 0.4f }, { "sub_length", 250, 500 } };
            x[Raw][APitch]   = { { "pe_mode", 0, 0, { 0, 1, 3 } }, { "pe_start", 250, 500 }, { "pe_time", 30, 90 }, { "pe_curve", 0.1f, 0.4f } };
            x[Raw][ADrive]   = { { "tail_st2_type", 0, 0, { 2, 3, 4, 5 } }, { "tail_st2_drive", 6, 18 }, { "tail_st2_bias", 0.f, 0.4f }, { "master_clip_drive", 3, 9 } };
            x[Raw][AScreech] = { { "scr_on", 0, 0, { 0, 1 } }, { "scr_level", -15, -6 } };
            // ---- EXPERIMENTAL: hybrid, unusual - the whole range
            x[Experimental][APunch]   = { { "punch_source", 0, 0, { 0, 1, 2, 3, 4, 5 } }, { "punch_pitch", 150, 900 }, { "punch_level", -14, -2 }, { "punch_drive", 0.f, 0.9f }, { "punch_length", 8, 80 } };
            x[Experimental][ABody]    = { { "body_link", 0, 0, { 1 } }, { "body_link_source", 0, 0, { 2 } }, { "body_follow_depth", 0, 0, { 0 } }, { "body_follow_offset", 0, 0, { -12, 0, 5, 7, 12, 19, 24 } },
                                          { "body_source", 0, 0, { 0, 1, 2, 3, 4 } }, { "body_level", -18, -2 }, { "body_drive", 0.f, 1.f }, { "body_decay", 30, 400 },
                                          { "body_tone", 500, 14000 }, { "body_resonance", 0.f, 0.8f } };
            x[Experimental][ATail]    = { { "tail_character", 0, 0, { 0, 1, 2, 3, 4, 5, 6 } }, { "tail_source", 0, 0, { 0, 1, 2, 3, 4, 5 } }, { "tail_drive", 0.2f, 1.f },
                                          { "tail_tone", 1000, 18000 }, { "tail_resonance", 0.f, 0.9f }, { "tail_mid_gain", -6, 9 }, { "tail_uni_voices", 1, 7 }, { "tail_decay", 200, 1500 } };
            x[Experimental][ASub]     = { { "sub_level", -18, -4 }, { "sub_drive", 0.f, 0.6f }, { "sub_length", 150, 700 } };
            x[Experimental][APitch]   = { { "pe_mode", 0, 0, { 0, 1, 2, 3, 4, 5 } }, { "pe_start", 150, 700 }, { "pe_time", 20, 250 }, { "pe_curve", 0.f, 0.8f } };
            x[Experimental][ADrive]   = { { "tail_st2_type", 0, 0, { 0, 1, 2, 3, 4, 5, 6 } }, { "tail_st2_drive", 0, 24 }, { "tail_st2_bias", -0.5f, 0.5f }, { "master_clip_drive", 0, 12 } };
            x[Experimental][AScreech] = { { "scr_on", 0, 0, { 0, 1 } }, { "scr_level", -18, -4 } };
            return x;
        }();
        return t[(size_t) juce::jlimit (0, NumStyles - 1, style)];
    }

    // ---- settings in the state (root properties: saved with the project, not automatable)
    inline Settings load (GeProcessor& p)
    {
        const auto& t = p.apvts.state;
        Settings s;
        s.style = juce::jlimit (0, NumStyles - 1, (int) t.getProperty ("vrStyle", (int) Modern)); s.scope = juce::jlimit (0, (int) NumAreas, (int) t.getProperty ("vrScope", 0));
        s.amount = juce::jlimit (0.f, 1.f, (float) (double) t.getProperty ("vrAmount", 1.0)); s.seed = (int) t.getProperty ("vrSeed", 1);
        s.locks = (uint32_t) (int) t.getProperty ("vrLocks", 0); s.routing = (bool) t.getProperty ("vrRouting", false);
        return s;
    }
    inline void save (GeProcessor& p, const Settings& s)
    {
        auto t = p.apvts.state;
        t.setProperty ("vrStyle", s.style, nullptr); t.setProperty ("vrScope", s.scope, nullptr); t.setProperty ("vrAmount", s.amount, nullptr);
        t.setProperty ("vrSeed", s.seed, nullptr); t.setProperty ("vrLocks", (int) s.locks, nullptr); t.setProperty ("vrRouting", s.routing, nullptr);
    }

    inline double kickRms (GeProcessor& p)
    {
        const auto x = renderKick (p.frame(), p.auditionNote, 44100.0, 0.5, -1, 1.f, &p.shapes(), 0.0, -1.0, nullptr, &p.oscData(), &p.distData(), nullptr, &p.modAudioCopy());
        double e = 0.0; for (int c = 0; c < 2; ++c) for (int i = 0; i < x.getNumSamples(); ++i) e += (double) x.getSample (c, i) * x.getSample (c, i);
        return std::sqrt (e / (2.0 * x.getNumSamples()) + 1.0e-20);
    }

    // ---- one randomize (one undo step); 'fromSnapshot' = REGENERATE
    inline void apply (GeProcessor& p, const Settings& s, bool regenerate = false)
    {
        p.undo.beginNewTransaction (regenerate ? "Regenerate" : "Randomize");
        // REGENERATE: back to the sound before the last randomize (kept in the state, property vrStart)
        if (regenerate)
            if (auto old = juce::parseXML (p.apvts.state.getProperty ("vrStart").toString()))
                for (auto* e : old->getChildWithTagNameIterator ("V")) p.setParamInUndoStep (e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("v"));
        const double before = kickRms (p);
        // remember the starting values of everything the randomizer may touch
        auto snap = std::make_unique<juce::XmlElement> ("START");
        const auto& tb = table (s.style);
        juce::StringArray touched;
        for (auto& area : tb) for (auto& r : area) touched.addIfNotAlreadyThere (r.id);
        touched.add ("core_master");
        const auto f0 = p.frame();
        for (auto& id : touched) { const int k = fge::modstate::indexOf (id); if (k >= 0) { auto* e = snap->createNewChildElement ("V"); e->setAttribute ("id", id); e->setAttribute ("v", f0[k]); } }

        p.apvts.state.setProperty ("vrStart", snap->toString (juce::XmlElement::TextFormat().singleLine()), nullptr);
        juce::Random rng ((juce::int64) s.seed * 7919 + 13);
        for (int a = 0; a < NumAreas; ++a)
        {
            const bool inScope = s.scope == 0 || s.scope - 1 == a;
            const bool locked = (s.locks >> a) & 1u;
            for (auto& r : tb[(size_t) a])
            {
                const float u = rng.nextFloat(), w = rng.nextFloat();          // (drawn for every range: a lock / scope never shifts the others)
                if (! inScope || locked) continue;
                const int k = fge::modstate::indexOf (r.id);
                if (k < 0) continue;
                const float cur = f0[k];
                float v;
                if (! r.choices.empty())
                {
                    if (w > s.amount && std::find (r.choices.begin(), r.choices.end(), (int) std::lround (cur)) != r.choices.end()) continue;   // (small AMOUNT: keep a legal choice)
                    v = (float) r.choices[(size_t) juce::jlimit (0, (int) r.choices.size() - 1, (int) (u * (float) r.choices.size()))];
                }
                else
                {
                    const float target = r.lo + (r.hi - r.lo) * u;
                    v = cur + (target - cur) * s.amount;
                    if (def (k).kind == Kind::Float && def (k).unit == Unit::None) v = std::round (v);      // (counts: unison voices)
                }
                const float hi = def (k).kind == Kind::Choice ? (float) (def (k).choices().size() - 1) : def (k).max;
                p.setParamInUndoStep (r.id, juce::jlimit (def (k).kind == Kind::Choice ? 0.f : def (k).min, hi, v));
            }
        }
        // ROUTING: a few movements in free matrix slots (LFO / sequencer / random on musical targets)
        if (s.routing)
        {
            static const char* targets[] { "tail_tone", "tail_drive", "tail_resonance", "pe_start", "body_tone", "tail_mid_gain" };
            int added = 0;
            for (int slot = 0; slot < fge::dsp::mod::numSlots && added < 3; ++slot)
            {
                if (! p.modDesc().slot[(size_t) slot].empty()) continue;
                fge::modstate::SlotDesc d; d.on = true;
                const int kind = rng.nextInt (3);
                d.src = kind == 0 ? fge::dsp::mod::SLfo1 + rng.nextInt (2) : kind == 1 ? fge::dsp::mod::SSeq : fge::dsp::mod::SRnd1;
                d.dst = targets[rng.nextInt (6)]; d.depth = (rng.nextFloat() * 0.3f + 0.05f) * (rng.nextBool() ? 1.f : -1.f) * s.amount; d.pol = fge::dsp::mod::PBipolar;
                d.label = "RANDOM ROUTING";
                p.setModSlot (slot, d, false);
                ++added;
            }
        }
        // the same level as before (a kick is judged by its shape, not by being louder)
        const double after = kickRms (p);
        if (before > 1.0e-6 && after > 1.0e-6)
        {
            const float m = juce::jlimit (-24.f, 3.f, p.frame()[P::coreMaster] + (float) (20.0 * std::log10 (before / after)));
            p.setParamInUndoStep ("core_master", m);
        }
    }
    inline void randomize (GeProcessor& p) { auto s = load (p); s.seed = s.seed % 9999 + 1; save (p, s); apply (p, s); }
}
