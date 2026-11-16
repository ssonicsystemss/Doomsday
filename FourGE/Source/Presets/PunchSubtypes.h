#pragma once
// =====================================================================
//  4GE - PUNCH SUBTYPES (owner 2026-11-16): not separate generators but sound PROFILES of the one engine of a PUNCH FAMILY
//  that change several parameters at once.
//     PUNCH FAMILY   EUPHORIC  CLASSIC / 2010-2012 / FESTIVAL     (punch_mode EUPHORIC, psh_*)
//                    PVC       CLASSIC / KLOENK / MODERN          (punch_mode PVC, the KLOENK model, pvc_*)
//  A profile = conceptual 0..1 values (the owner's PUNCH_PROFILE: attack shape, body, click, sub punch, drive, curve,
//  resonance freq / Q / amount, harmonics, decay, stereo) mapped onto the family's parameters + the shared FRONT
//  CLICK / SUB PUNCH. It NEVER changes the TOK (TOK TYPE etc. stay: KLOENK + WOOD TOK, MODERN + PUP TOK ...), the tail,
//  the lengths or the style. One pick = one undo step. The values are a first guess - to be tuned by ear.
//  Files / presets layer only (no DSP, no GUI). State property "punchSub" = the last picked profile (display only).
// =====================================================================
#include <vector>
#include <utility>
#include <cmath>
#include "../PluginProcessor.h"

namespace fge::punchsub
{
    enum Id { EuphClassic, Euph2010, EuphFestival, PvcClassic, PvcKloenk, PvcModern, count };
    inline const char* name (int i)
    {
        static const char* n[count] { "CLASSIC", "2010-2012", "FESTIVAL", "CLASSIC", "KLOENK", "MODERN" };
        return i >= 0 && i < count ? n[i] : "";
    }
    inline int family (int i) { return i < PvcClassic ? 2 : 3; }            // the punch_mode of the family

    struct Profile
    {
        float attack, body, click, clickTone, subPunch, drive, curve, resFreq, resQ, resonance, harmonics, decay, stereo, lowClean;
    };
    inline const Profile& profile (int i)
    {
        //                  attack body  click ctone  sub   drive curve rfreq rQ    reso  harm  decay stereo lowclean
        static const Profile p[count] {
            { 0.45f, 0.75f, 0.35f, 0.45f, 0.70f, 0.25f, 0.30f, 0.35f, 0.30f, 0.20f, 0.40f, 0.50f, 0.00f, 0.3f },   // EUPHORIC CLASSIC: soft, round, tonal
            { 0.75f, 0.70f, 0.70f, 0.65f, 0.50f, 0.50f, 0.55f, 0.50f, 0.40f, 0.35f, 0.65f, 0.40f, 0.00f, 0.4f },   // 2010-2012: raw, mechanical, a clear TOK -> CLICK -> PUNCH
            { 0.85f, 0.90f, 0.60f, 0.90f, 0.75f, 0.50f, 0.40f, 0.60f, 0.35f, 0.40f, 0.85f, 0.55f, 0.60f, 0.7f },   // FESTIVAL: big, wide (above ~250 Hz), controlled low
            { 0.95f, 0.20f, 0.60f, 0.60f, 0.50f, 0.45f, 0.40f, 0.45f, 0.40f, 0.40f, 0.60f, 0.15f, 0.00f, 0.4f },   // PVC CLASSIC: short, dry, plastic "TOK -> K"
            { 0.80f, 0.50f, 0.75f, 0.60f, 0.45f, 0.55f, 0.50f, 0.55f, 0.90f, 0.90f, 0.65f, 0.50f, 0.00f, 0.4f },   // KLOENK: one narrow resonant peak in the upper low-mid
            { 1.00f, 0.75f, 0.65f, 0.50f, 0.70f, 0.75f, 0.35f, 0.60f, 0.50f, 0.45f, 0.90f, 0.40f, 0.25f, 0.9f } }; // MODERN: clean attack, dense harmonics, clean low
        return p[i < 0 ? 0 : i >= count ? count - 1 : i];
    }

    inline std::vector<std::pair<const char*, float>> values (int i)
    {
        const auto& q = profile (i);
        std::vector<std::pair<const char*, float>> v;
        auto db = [] (float x) { return -24.f + 22.f * x; };                 // 0.35 -> -16 dB, 0.75 -> -7.5 dB
        v.push_back ({ "punch_mode", (float) family (i) });
        // the shared FRONT: CLICK + SUB PUNCH (the TOK is never touched)
        v.push_back ({ "clk_level", db (q.click) });
        v.push_back ({ "clk_type", q.attack > 0.9f ? 1.f : q.click > 0.65f ? 1.f : 0.f });     // HARD / SOFT
        v.push_back ({ "clk_tone", q.clickTone });
        v.push_back ({ "clk_length", 14.f - 9.f * q.attack });
        v.push_back ({ "clk_decay", 0.3f + 0.4f * (1.f - q.decay) });
        v.push_back ({ "spk_level", db (q.subPunch) });
        v.push_back ({ "spk_length", 35.f + 35.f * q.body });
        v.push_back ({ "spk_drive", 0.1f + 0.4f * q.drive * (1.f - q.lowClean) });
        v.push_back ({ "spk_tone", 0.5f - 0.3f * q.lowClean });
        if (family (i) == 2)                                                 // EUPHORIC: psh_*
        {
            v.push_back ({ "psh_spike", 10.f - 8.f * q.attack });
            v.push_back ({ "psh_body", 0.35f + 0.55f * q.body });
            v.push_back ({ "psh_body_ms", 50.f + 70.f * q.body });
            v.push_back ({ "psh_click", 0.4f * q.click });                   // (the model's own click stays under the FRONT CLICK)
            v.push_back ({ "psh_click_tone", 4000.f + 10000.f * q.clickTone });
            v.push_back ({ "psh_sub", 0.4f + 0.8f * q.subPunch });
            v.push_back ({ "psh_drive", 8.f + 30.f * q.drive });
            v.push_back ({ "psh_asym", q.curve });
            v.push_back ({ "psh_mid_freq", 600.f * std::exp2 (2.5f * q.resFreq) });
            v.push_back ({ "psh_mid_gain", 4.f + 12.f * q.resonance + 6.f * q.harmonics });
            v.push_back ({ "psh_end", 90.f + 160.f * q.decay });
            v.push_back ({ "psh_low_cut", 150.f + 250.f * q.lowClean });
        }
        else                                                                 // PVC: the KLOENK model, pvc_*
        {
            v.push_back ({ "pvc_model", 0.f });
            v.push_back ({ "pvc_attack", 1.5f * (1.f - q.attack) });
            v.push_back ({ "pvc_length", 35.f + 90.f * q.body });
            v.push_back ({ "pvc_decay", 15.f + 70.f * q.decay });
            v.push_back ({ "pvc_drive", 8.f + 30.f * q.drive });
            v.push_back ({ "pvc_asym", q.curve });
            v.push_back ({ "pvc_sat", q.harmonics });
            v.push_back ({ "pvc_res_freq", 300.f * std::exp2 (3.f * q.resFreq) });
            v.push_back ({ "pvc_res_q", 2.f + 26.f * q.resQ });
            v.push_back ({ "pvc_res_amount", q.resonance });
            v.push_back ({ "pvc_high_mid", -2.f + 8.f * q.harmonics });
            v.push_back ({ "pvc_low_mid", -6.f * q.lowClean });
            v.push_back ({ "pvc_comp", 0.4f + 0.4f * q.lowClean });
        }
        // STEREO: only above ~250 Hz (the lows stay mono)
        v.push_back ({ "spc_punch_on", q.stereo > 0.05f ? 1.f : 0.f });
        if (q.stereo > 0.05f)
        {
            v.push_back ({ "spc_punch_mode", 1.f });                         // DECORRELATION (the punch is mono)
            v.push_back ({ "spc_punch_width", 1.f + q.stereo });
            v.push_back ({ "spc_punch_mix", q.stereo });
            v.push_back ({ "spc_punch_low_mono", 1.f });
            v.push_back ({ "spc_punch_crossover", 250.f });
        }
        return v;
    }

    inline void apply (GeProcessor& p, int i)
    {
        p.beginUndoStep (juce::String ("Punch ") + (family (i) == 2 ? "EUPHORIC " : "PVC ") + name (i));
        for (auto& [id, x] : values (i)) if (p.apvts.getParameter (id) != nullptr) p.setParamInUndoStep (id, x);
        p.apvts.state.setProperty ("punchSub", i, nullptr);
    }
}
