// RE4GE - factory presets + envelope templates (data only: no DSP, no GUI, no files)
#include "FactoryPresets.h"
#include "../Parameters/ParamIDs.h"

using namespace juce;
using doomsday::PresetValues;

namespace
{
    using re4ge::FactoryPreset;
    std::vector<FactoryPreset> buildPresets();
}

namespace re4ge
{
    const std::vector<FactoryPreset>& factoryPresets() { static const std::vector<FactoryPreset> p = buildPresets(); return p; }
    const StringArray& templateNames() { static const StringArray n { "Pump", "Swell", "Tight", "Roll", "Pulse" }; return n; }
    const std::vector<PresetValues>& templates()
    {
        static const std::vector<PresetValues> t {
            // PUMP: like the FHSP "reversed bass kicks" - ducked, back up fast (sidechain release), a level shelf, cut before the kick
            { { "trg_timing", 0.f }, { "env_delay_pct", 36.f }, { "env_duck", 0.f }, { "env_initial", 0.f }, { "env_swell", 60.f }, { "env_curve", 0.6f }, { "env_hold_pct", 100.f },
              { "env_decay_pct", 0.f }, { "env_sustain", 1.f }, { "env_gate_pct", 98.f }, { "env_fade", 25.f } },
            // SWELL: the classic breathing one - slow rise, an early peak, a long fading tail
            { { "trg_timing", 0.f }, { "env_delay_pct", 25.f }, { "env_duck", 0.f }, { "env_initial", 0.1f }, { "env_swell", 140.f }, { "env_curve", -0.4f }, { "env_hold_pct", 0.f },
              { "env_decay_pct", 60.f }, { "env_sustain", 0.f }, { "env_decay_curve", 2.2f }, { "env_gate_pct", 97.f }, { "env_fade", 15.f } },
            // TIGHT: short and compact
            { { "trg_timing", 0.f }, { "env_delay_pct", 30.f }, { "env_duck", 0.f }, { "env_initial", 0.f }, { "env_swell", 15.f }, { "env_curve", 0.5f }, { "env_hold_pct", 5.f },
              { "env_decay_pct", 40.f }, { "env_sustain", 0.f }, { "env_decay_curve", 1.2f }, { "env_gate_pct", 90.f }, { "env_fade", 10.f } },
            // ROLL: long, spreading
            { { "trg_timing", 0.f }, { "env_delay_pct", 20.f }, { "env_duck", 0.f }, { "env_initial", 0.2f }, { "env_swell", 60.f }, { "env_curve", 0.f }, { "env_hold_pct", 10.f },
              { "env_decay_pct", 90.f }, { "env_sustain", 0.3f }, { "env_decay_curve", 0.8f }, { "env_gate_pct", 99.f }, { "env_fade", 20.f } },
            // PULSE: fast and aggressive on 1/16
            { { "trg_timing", 0.f }, { "trg_step", 2.f }, { "env_delay_pct", 15.f }, { "env_duck", 0.f }, { "env_initial", 0.f }, { "env_swell", 10.f }, { "env_curve", 0.8f }, { "env_hold_pct", 30.f },
              { "env_decay_pct", 0.f }, { "env_sustain", 1.f }, { "env_gate_pct", 85.f }, { "env_fade", 6.f } } };
        return t;
    }
}

namespace
{
    using namespace re4ge;
    PresetValues withTemplate (int t, PresetValues v) { auto out = templates()[(size_t) t]; for (auto& x : v) out.push_back (x); return out; }

    // the owner's Forge tails (QL, Volcanic, Undertow) as the TAIL source, with the bass settings of his BASS presets
    PresetValues qlTail()       { return { { "src_type", 1.f }, { "tail_start", 4.99f }, { "tail_drop", 32.f }, { "tail_curve", 4.42f }, { "tail_wave", 0.f }, { "tail_dist", 4.f },
                                           { "tail_drive", 29.5f }, { "tail_tone", 0.214f }, { "tail_high_cut", 8557.f }, { "tail_scream", 0.406f }, { "tail_scream_hz", 1240.f }, { "pitch_tune", 1.35f } }; }
    PresetValues volcanicTail() { return { { "src_type", 1.f }, { "tail_start", 5.27f }, { "tail_drop", 186.f }, { "tail_curve", 2.17f }, { "tail_wave", 0.f }, { "tail_dist", 2.f },
                                           { "tail_drive", 46.f }, { "tail_tone", -0.079f }, { "tail_high_cut", 10198.f }, { "tail_scream", 0.769f }, { "tail_scream_hz", 643.f } }; }
    PresetValues undertowTail() { return { { "src_type", 1.f }, { "tail_start", 3.81f }, { "tail_drop", 87.8f }, { "tail_curve", 2.2f }, { "tail_wave", 0.17f }, { "tail_dist", 1.f },
                                           { "tail_drive", 46.4f }, { "tail_tone", 0.952f }, { "tail_high_cut", 6336.f }, { "tail_scream", 0.624f }, { "tail_scream_hz", 1500.f } }; }
    PresetValues plus (PresetValues a, const PresetValues& b) { for (auto& x : b) a.push_back (x); return a; }
    const PresetValues forgeBass { { "trg_step", 0.f }, { "dist_drive", 10.f }, { "eq_high_cut", 9000.f }, { "tail_room", 0.35f }, { "tail_room_size", 0.5f }, { "out_level", -4.f }, { "sub_hz", 90.f }, { "sub_level", 0.f }, { "env_fade", 25.f }, { "env_swell", 60.f } };

    std::vector<FactoryPreset> buildPresets()
    {
        std::vector<FactoryPreset> L;
        L.push_back ({ "INIT - Pump Tail", withTemplate (0, { { "out_level", -5.5f } }) });
        // the owner's own basses (2026-10-28: "w miarę spoko") - exactly as he saved them
        L.push_back ({ "OWNER - 1", { { "trg_beats", 0.f }, { "eq_bell_gain", 8.3146f }, { "eq_bell_hz", 1959.1f }, { "eq_bell_q", 1.2366f }, { "dist_bias", 0.31818f }, { "osc_blend", 0.069694f }, { "out_clip", 0.64545f }, { "env_curve", -0.65455f }, { "env_decay_curve", 6.f }, { "env_decay_ms", 0.f }, { "env_decay_pct", 46.364f }, { "env_delay_ms", 120.f }, { "env_delay_pct", 25.864f }, { "dist_type", 1.f }, { "dist_drive", 14.858f }, { "dist_env", 0.18953f }, { "env_duck", 0.f }, { "env_fade", 30.948f }, { "pitch_fine", 0.f }, { "pitch_fixed_note", 34.f }, { "env_gate_ms", 380.f }, { "env_gate_pct", 100.f }, { "eq_high_cut", 16000.f }, { "env_hold_ms", 400.f }, { "env_hold_pct", 0.f }, { "env_initial", 0.f }, { "pitch_key_track", 1.f }, { "out_level", -0.65454f }, { "eq_low_cut", 21.622f }, { "dist_mix", 1.f }, { "trg_offset", 0.f }, { "pitch_amt", -24.f }, { "pitch_mode", 3.f }, { "pitch_time", 8.4857f }, { "flt_pre_hz", 13931.f }, { "flt_pre_reso", 0.65455f }, { "flt_pre_type", 1.f }, { "osc_sine", -6.f }, { "smp_reverse", 1.f }, { "smp_root", 31.f }, { "smp_start", 0.f }, { "src_type", 0.f }, { "trg_step", 0.f }, { "trg_step1", 1.f }, { "trg_step2", 1.f }, { "trg_step3", 1.f }, { "trg_step4", 1.f }, { "sub_hz", 85.537f }, { "sub_level", 0.9f }, { "env_sustain", 0.4f }, { "env_swell", 140.f }, { "tail_curve", 2.6405f }, { "tail_dist", 3.f }, { "tail_drive", 29.349f }, { "tail_drop", 56.132f }, { "tail_high_cut", 8557.f }, { "tail_len", 300.f }, { "tail_scream", 0.f }, { "tail_scream_hz", 684.79f }, { "tail_start", 6.4963f }, { "tail_tone", 0.30015f }, { "tail_wave", 0.f }, { "trg_timing", 0.f }, { "trg_mode", 0.f }, { "pitch_tune", 12.f }, { "midi_velocity", 0.69091f }, { "osc_warp", 1.f }, { "osc_warp_amt", 0.28198f }, { "osc_warp_env", 0.1422f }, { "osc_pw", 0.84203f } } });
        L.push_back ({ "OWNER - 2", { { "trg_beats", 0.f }, { "eq_bell_gain", 8.3146f }, { "eq_bell_hz", 1959.1f }, { "eq_bell_q", 1.2366f }, { "dist_bias", 0.31818f }, { "osc_blend", 0.f }, { "out_clip", 0.64545f }, { "env_curve", -1.f }, { "env_decay_curve", 6.f }, { "env_decay_ms", 0.f }, { "env_decay_pct", 25.909f }, { "env_delay_ms", 120.f }, { "env_delay_pct", 24.136f }, { "dist_type", 1.f }, { "dist_drive", 14.858f }, { "dist_env", 0.18953f }, { "env_duck", 0.f }, { "env_fade", 95.397f }, { "pitch_fine", 0.f }, { "pitch_fixed_note", 34.f }, { "env_gate_ms", 380.f }, { "env_gate_pct", 100.f }, { "eq_high_cut", 16000.f }, { "env_hold_ms", 400.f }, { "env_hold_pct", 11.818f }, { "env_initial", 0.f }, { "pitch_key_track", 1.f }, { "out_level", -0.65454f }, { "eq_low_cut", 21.622f }, { "dist_mix", 1.f }, { "trg_offset", 0.f }, { "pitch_amt", -24.f }, { "pitch_mode", 3.f }, { "pitch_time", 79.335f }, { "flt_pre_hz", 13931.f }, { "flt_pre_reso", 0.65455f }, { "flt_pre_type", 1.f }, { "osc_sine", -6.f }, { "smp_reverse", 1.f }, { "smp_root", 31.f }, { "smp_start", 0.f }, { "src_type", 0.f }, { "trg_step", 0.f }, { "trg_step1", 1.f }, { "trg_step2", 1.f }, { "trg_step3", 1.f }, { "trg_step4", 1.f }, { "sub_hz", 85.537f }, { "sub_level", 0.9f }, { "env_sustain", 0.4f }, { "env_swell", 145.16f }, { "tail_curve", 5.2746f }, { "tail_dist", 2.f }, { "tail_drive", 35.24f }, { "tail_drop", 194.21f }, { "tail_high_cut", 8557.f }, { "tail_len", 300.f }, { "tail_scream", 0.090909f }, { "tail_scream_hz", 684.79f }, { "tail_start", 6.0979f }, { "tail_tone", 0.51834f }, { "tail_wave", 0.77727f }, { "trg_timing", 0.f }, { "trg_mode", 0.f }, { "pitch_tune", 12.f }, { "midi_velocity", 0.69091f }, { "osc_warp", 3.f }, { "osc_warp_amt", 0.17273f }, { "osc_warp_env", 0.21947f }, { "osc_pw", 0.84203f } } });
        L.push_back ({ "PUMP - QL Tail", withTemplate (0, plus (qlTail(), forgeBass)) });
        L.push_back ({ "PUMP - Volcanic Tail [F]", withTemplate (0, plus (volcanicTail(), plus (forgeBass, { { "out_level", -9.f } }))) });
        L.push_back ({ "PUMP - Undertow Tail [G]", withTemplate (0, plus (undertowTail(), plus (forgeBass, { { "out_level", -8.f } }))) });
        L.push_back ({ "SWELL - QL Tail", withTemplate (1, plus (qlTail(), { { "dist_drive", 12.f }, { "eq_high_cut", 1100.f }, { "dist_env", 0.6f }, { "out_level", -6.f } })) });
        L.push_back ({ "SWELL - Synth Bend", withTemplate (1, { { "src_type", 0.f }, { "osc_warp", 1.f }, { "osc_warp_amt", 0.55f }, { "osc_warp_env", 0.8f }, { "osc_blend", 0.2f },
                                                                 { "dist_drive", 14.f }, { "dist_type", 1.f }, { "dist_env", 0.5f }, { "eq_high_cut", 1400.f }, { "eq_bell_gain", 2.5f } }) });
        L.push_back ({ "TIGHT - Synth Squeeze 1/8", withTemplate (2, { { "src_type", 0.f }, { "trg_step", 1.f }, { "osc_warp", 2.f }, { "osc_warp_amt", 0.6f }, { "osc_warp_env", 0.6f },
                                                                        { "dist_drive", 16.f }, { "dist_type", 0.f }, { "eq_high_cut", 1600.f }, { "eq_bell_gain", 2.f } }) });
        L.push_back ({ "ROLL - Synth Mirror", withTemplate (3, { { "src_type", 0.f }, { "osc_warp", 4.f }, { "osc_warp_amt", 0.7f }, { "osc_warp_env", 0.3f }, { "osc_blend", 0.5f },
                                                                  { "dist_drive", 12.f }, { "dist_type", 4.f }, { "dist_bias", 0.3f }, { "eq_high_cut", 1200.f } }) });
        L.push_back ({ "PULSE - Synth Sync Offbeats", withTemplate (4, { { "src_type", 0.f }, { "trg_step1", 0.f }, { "trg_step3", 0.f }, { "osc_warp", 3.f }, { "osc_warp_amt", 0.5f },
                                                                          { "osc_warp_env", 1.f }, { "dist_drive", 18.f }, { "dist_type", 2.f }, { "eq_high_cut", 2200.f }, { "eq_bell_gain", 3.f }, { "out_level", -9.f } }) });
        L.push_back ({ "RISE - QL Tail Pitch Up", withTemplate (0, plus (qlTail(), plus (forgeBass, { { "src_type", 1.f }, { "pitch_mode", 2.f }, { "pitch_amt", 5.f }, { "pitch_time", 90.f } }))) });
        L.push_back ({ "GATE - Bass Line (notes)", { { "trg_mode", 1.f }, { "trg_timing", 1.f }, { "env_delay_ms", 0.f }, { "env_swell", 25.f }, { "env_curve", 0.5f }, { "env_hold_ms", 4000.f },
                                                     { "env_decay_ms", 0.f }, { "env_fade", 20.f }, { "src_type", 0.f }, { "osc_blend", 0.3f }, { "osc_warp", 1.f }, { "osc_warp_amt", 0.35f }, { "osc_warp_env", 0.f },
                                                     { "dist_drive", 12.f }, { "eq_high_cut", 1300.f } } });
        return L;
    }
}

