#pragma once
// =====================================================================
//  4GE module 09 (6) - the VARIANT RANDOMIZER (message thread only: tools / presets, never the audio thread, never GUI code)
//   STYLE   EARLY 2004-08 (909 punch + room + drive) / EUPHORIC 2009-12 (short tok with a driven pitch drop, melodic driven tail) / MODERN (dirty, controlled) / RAW (scream / metal / buzz / crush,
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
    enum Style { Early = 0, Modern, Raw, Experimental, Euphoric, NumStyles };
    enum Area { APunch = 0, ABody, ATail, ASub, APitch, ADrive, AScreech, NumAreas };
    inline const juce::StringArray& styleNames() { static const juce::StringArray a { "EARLY", "MODERN", "RAW", "EXPERIMENTAL", "EUPHORIC" }; return a; }
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

    // ---- EUPHORIC 2009-12 (its own style since 2026-11-12, was an EARLY pattern) (Wildstylez / Headhunterz / Brennan Heart / Noisecontrollers; owner 2026-11-12).
    //      Measured on the owner's refs (Wildstylez "Unleash The Beast", Noisecontrollers WS BD, Zatox "Wakanda"): a SHORT tonal tok
    //      ~520-700 Hz for ~20 ms, a fast drop to the note in 60-100 ms, the bass late (~100-130 ms, weak under the tok), a dense,
    //      well-driven tail that is the loudest part and holds (flat, "gated"), a room in the drive (the reverb kicks of that time).
    inline const Table& euphoricTable()
    {
        // ranges around the EUPHORIC template = the owner's own kick (2026-11-13, from INIT): low punch, high long BODY (the crunch), the tail
        // falling from ~760 Hz (the crack), soft -> wavefold, the 6th harmonic; never RAW: no screech, moderate drive
        static const Table x = []
        {
            Table t;
            t[APunch]   = { { "punch_mode", 0, 0, { 2 } }, { "punch_level", -7.5f, -4.5f },                 // SHAPED PUNCH (owner 2026-11-14)
                            { "psh_mid_freq", 800, 1800 }, { "psh_mid_gain", 9, 15 }, { "psh_drive", 18, 30 }, { "psh_asym", 0.3f, 0.7f },
                            { "psh_tape", 0.3f, 0.7f }, { "psh_click", 0.3f, 0.7f }, { "psh_sub", 0.6f, 1.1f }, { "psh_sub_delay", 15, 35 } };
            t[ABody]    = { { "body_link", 0, 0, { 0 } }, { "body_pitch", 1050, 1400 }, { "body_level", -5.5f, -3 }, { "body_drive", 0.38f, 0.62f } };
            t[ATail]    = { { "tail_drive", 0.5f, 0.75f }, { "tail_level", -5.5f, -3.5f }, { "tail_room", 0.1f, 0.35f }, { "tail_room_input", 0, 0, { 0 } },
                            { "tail_room_decay", 350, 550 }, { "tail_room_tone", 5000, 8000 },
                            { "tail_color_gain", 4, 8 }, { "tail_color_track", 0, 0, { 1 } }, { "tail_color_harm", 0, 0, { 6, 7, 7, 7, 8 } }, { "tail_color_q", 1.f, 1.6f } };
            t[ASub]     = { { "sub_level", 0, 0, { -12 } } };                     // INIT sub (the owner's kick)
            t[APitch]   = { { "pe_start", 650, 900 }, { "pe_time", 130, 200 }, { "pe_curve", 0.18f, 0.32f }, { "pe_amount", -15, -12 } };
            t[ADrive]   = { { "master_clip_drive", 0.5f, 2.5f } };
            t[AScreech] = { { "scr_on", 0, 0, { 0 } } };
            return t;
        }();
        return x;
    }
    inline const Table& table (int style)
    {
        static const std::array<Table, NumStyles> t = []
        {
            std::array<Table, NumStyles> x;
            // ---- EARLY 2004-08: a 909-like PUNCH (no tonal tok), a room before the drive, melodic body, clear pitch drop
            x[Early][APunch]   = { { "punch_mode", 0, 0, { 0 } }, { "punch_source", 0, 0, { 0, 1 } }, { "punch_pitch", 600, 1600 }, { "punch_glide", 2, 8 }, { "punch_level", -6, -1 }, { "punch_drive", 0.f, 0.5f }, { "punch_length", 15, 50 } };
            x[Early][ABody]    = { { "body_link", 0, 0, { 1 } }, { "body_link_source", 0, 0, { 2 } }, { "body_follow_depth", 0, 0, { 0 } }, { "body_follow_offset", 0, 0, { 0, 0, 12 } },
                                   { "body_source", 0, 0, { 0, 1, 2 } }, { "body_level", -14, -6 }, { "body_drive", 0.1f, 0.45f }, { "body_decay", 80, 200 },
                                   { "body_tone", 1500, 4500 }, { "body_resonance", 0.f, 0.3f } };
            x[Early][ATail]    = { { "tail_character", 0, 0, { 0, 1, 2 } }, { "tail_source", 0, 0, { 0, 1 } }, { "tail_drive", 0.2f, 0.5f }, { "tail_tone", 2000, 6000 },
                                   { "tail_resonance", 0.f, 0.25f }, { "tail_mid_gain", 0, 3 }, { "tail_uni_voices", 1, 3 }, { "tail_decay", 400, 900 },
                                    { "tail_room", 0.2f, 0.6f }, { "tail_room_decay", 300, 700 }, { "tail_room_tone", 3000, 7000 } };
            x[Early][ASub]     = { { "sub_level", -14, -8 }, { "sub_drive", 0.f, 0.2f }, { "sub_length", 300, 500 }, { "sub_amp_env_delay", 0, 25 } };
            x[Early][APitch]   = { { "pe_mode", 0, 0, { 0, 2 } }, { "pe_start", 160, 300 }, { "pe_time", 60, 160 }, { "pe_curve", 0.2f, 0.5f } };
            x[Early][ADrive]   = { { "tail_st2_type", 0, 0, { 0, 1, 3 } }, { "tail_st2_drive", 0, 6 }, { "master_clip_drive", 0, 3 } };
            x[Early][AScreech] = { { "scr_on", 0, 0, { 0 } } };
            // ---- MODERN: strong, CLEAN and controlled (owner 2026-11-10: "too wild, like RAW") - CLEAN / TONAL tail, noise body, soft / tube only, no screech
            x[Modern][APunch]   = { { "punch_source", 0, 0, { 0, 1, 2 } }, { "punch_pitch", 392, 660 }, { "punch_level", -12, -8 }, { "punch_drive", 0.1f, 0.35f }, { "punch_length", 80, 140 }, { "punch_tok_decay", -6, -2 } };
            x[Modern][ABody]    = { { "body_link", 0, 0, { 1 } }, { "body_link_source", 0, 0, { 2 } }, { "body_follow_depth", 0, 0, { 0 } }, { "body_follow_offset", 0, 0, { 0, 7, 12, 12, 19 } },
                                    { "body_source", 0, 0, { 4, 4, 1 } }, { "body_level", -18, -10 }, { "body_drive", 0.1f, 0.35f }, { "body_decay", 80, 220 },
                                    { "body_tone", 3000, 7500 }, { "body_resonance", 0.f, 0.2f } };
            x[Modern][ATail]    = { { "tail_character", 0, 0, { 0, 1, 1 } }, { "tail_source", 0, 0, { 2, 2, 1 } }, { "tail_drive", 0.35f, 0.6f }, { "tail_tone", 3500, 8000 },
                                    { "tail_resonance", 0.f, 0.25f }, { "tail_mid_gain", 0, 3 }, { "tail_uni_voices", 3, 5 }, { "tail_decay", 500, 1000 } };
            x[Modern][ASub]     = { { "sub_level", -12, -6 }, { "sub_drive", 0.f, 0.3f }, { "sub_length", 300, 550 }, { "sub_amp_env_delay", 15, 40 }, { "sub_attack", 30, 60 } };   // the bass under the tok (refs)
            x[Modern][APitch]   = { { "pe_mode", 0, 0, { 0, 1 } }, { "pe_start", 200, 380 }, { "pe_time", 40, 110 }, { "pe_curve", 0.15f, 0.45f } };
            x[Modern][ADrive]   = { { "tail_st2_type", 0, 0, { 0, 1, 3 } }, { "tail_st2_drive", 2, 7 }, { "master_clip_drive", 0, 3 } };   // MODERN = clean (owner 2026-11-10)
            x[Modern][AScreech] = { { "scr_on", 0, 0, { 0 } } };   // no screech: that is RAW
            // ---- RAW: aggressive, strong distortion, sharp screech, asymmetry, raw character
            x[Raw][APunch]   = { { "punch_source", 0, 0, { 0, 2, 3, 5 } }, { "punch_pitch", 520, 990 }, { "punch_level", -12, -7 }, { "punch_drive", 0.3f, 0.7f }, { "punch_length", 80, 150 }, { "punch_tok_decay", -7, -2 } };
            x[Raw][ABody]    = { { "body_link", 0, 0, { 1 } }, { "body_link_source", 0, 0, { 2 } }, { "body_follow_depth", 0, 0, { 0 } }, { "body_follow_offset", 0, 0, { 0, 7, 12, 19, 24 } },
                                 { "body_source", 0, 0, { 2, 3, 4 } }, { "body_level", -12, -2 }, { "body_drive", 0.4f, 1.f }, { "body_decay", 60, 260 },
                                 { "body_tone", 2500, 12000 }, { "body_resonance", 0.f, 0.6f } };
            x[Raw][ATail]    = { { "tail_character", 0, 0, { 3, 4, 5, 6 } }, { "tail_source", 0, 0, { 0, 2, 3 } }, { "tail_drive", 0.65f, 0.95f }, { "tail_tone", 7000, 15000 },
                                 { "tail_resonance", 0.2f, 0.6f }, { "tail_mid_gain", 2, 7 }, { "tail_uni_voices", 3, 7 }, { "tail_decay", 500, 1100 } };
            x[Raw][ASub]     = { { "sub_level", -12, -6 }, { "sub_drive", 0.f, 0.4f }, { "sub_length", 250, 500 }, { "sub_amp_env_delay", 40, 80 } };
            x[Raw][APitch]   = { { "pe_mode", 0, 0, { 0, 1, 3 } }, { "pe_start", 250, 500 }, { "pe_time", 30, 90 }, { "pe_curve", 0.1f, 0.4f } };
            x[Raw][ADrive]   = { { "tail_st2_type", 0, 0, { 2, 3, 4, 5 } }, { "tail_st2_drive", 6, 18 }, { "tail_st2_bias", 0.f, 0.4f }, { "master_clip_drive", 3, 9 } };
            x[Raw][AScreech] = { { "scr_on", 0, 0, { 0, 1 } }, { "scr_level", -15, -6 } };
            // ---- EXPERIMENTAL: hybrid, unusual - the whole range
            x[Experimental][APunch]   = { { "punch_source", 0, 0, { 0, 1, 2, 3, 4, 5 } }, { "punch_pitch", 150, 1200 }, { "punch_level", -8, 0 }, { "punch_drive", 0.f, 0.9f }, { "punch_length", 5, 150 }, { "punch_tok_decay", -12, 0 }, { "punch_tok_bend", -12, 12 } };
            x[Experimental][ABody]    = { { "body_link", 0, 0, { 1 } }, { "body_link_source", 0, 0, { 2 } }, { "body_follow_depth", 0, 0, { 0 } }, { "body_follow_offset", 0, 0, { -12, 0, 5, 7, 12, 19, 24 } },
                                          { "body_source", 0, 0, { 0, 1, 2, 3, 4 } }, { "body_level", -18, -2 }, { "body_drive", 0.f, 1.f }, { "body_decay", 30, 400 },
                                          { "body_tone", 500, 14000 }, { "body_resonance", 0.f, 0.8f } };
            x[Experimental][ATail]    = { { "tail_character", 0, 0, { 0, 1, 2, 3, 4, 5, 6 } }, { "tail_source", 0, 0, { 0, 1, 2, 3, 4, 5 } }, { "tail_drive", 0.2f, 1.f },
                                          { "tail_tone", 1000, 18000 }, { "tail_resonance", 0.f, 0.9f }, { "tail_mid_gain", -6, 9 }, { "tail_uni_voices", 1, 7 }, { "tail_decay", 200, 1500 } };
            x[Experimental][ASub]     = { { "sub_level", -18, -4 }, { "sub_drive", 0.f, 0.6f }, { "sub_length", 150, 700 }, { "sub_amp_env_delay", 0, 120 } };
            x[Experimental][APitch]   = { { "pe_mode", 0, 0, { 0, 1, 2, 3, 4, 5 } }, { "pe_start", 150, 700 }, { "pe_time", 20, 250 }, { "pe_curve", 0.f, 0.8f } };
            x[Experimental][ADrive]   = { { "tail_st2_type", 0, 0, { 0, 1, 2, 3, 4, 5, 6 } }, { "tail_st2_drive", 0, 24 }, { "tail_st2_bias", -0.5f, 0.5f }, { "master_clip_drive", 0, 12 } };
            x[Experimental][AScreech] = { { "scr_on", 0, 0, { 0, 1 } }, { "scr_level", -18, -4 } };
            x[Euphoric] = euphoricTable();
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
        juce::Random rng ((juce::int64) s.seed * 7919 + 13);
        const auto& tb = table (s.style);
        juce::StringArray touched;
        for (auto& area : tb) for (auto& r : area) touched.addIfNotAlreadyThere (r.id);
        touched.add ("core_master");
        const auto f0 = p.frame();
        for (auto& id : touched) { const int k = fge::modstate::indexOf (id); if (k >= 0) { auto* e = snap->createNewChildElement ("V"); e->setAttribute ("id", id); e->setAttribute ("v", f0[k]); } }

        p.apvts.state.setProperty ("vrStart", snap->toString (juce::XmlElement::TextFormat().singleLine()), nullptr);
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
