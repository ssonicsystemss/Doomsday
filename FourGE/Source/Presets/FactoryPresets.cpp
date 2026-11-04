#include "FactoryPresets.h"

namespace fge
{
    const std::vector<FactoryPreset>& factoryPresets()
    {
        static const std::vector<FactoryPreset> list {
            { "INIT", {} },
            // matched to the owner's reference kicks (sound pass: unison, stage 2, mid bell, cuts, master clip)
            { "MODERN - Festival Drive", { { "core_auto_length", 0 }, { "core_length", 760.f }, { "tail_character", 5 }, { "tail_source", 0 }, { "tail_st2_type", 2 }, { "pe_start", 491.f }, { "pe_time", 143.f }, { "pe_curve", 0.8f }, { "tail_drive", 0.51f }, { "tail_tone", 3647.f }, { "tail_resonance", 0.6f }, { "tail_decay", 2376.f }, { "tail_sustain", 0.88f }, { "tail_length", 687.f }, { "body_level", -24.33f }, { "body_pitch", 103.f }, { "sub_level", -3.0f }, { "punch_level", -7.01f }, { "punch_pitch", 684.f }, { "punch_drive", 0.69f }, { "tail_level", -1.03f }, { "tail_uni_detune", 0.48f }, { "tail_uni_drift", 17.66f }, { "tail_uni_mix", 0.65f }, { "tail_st2_drive", 19.8f }, { "tail_mid_hz", 4000.f }, { "tail_mid_gain", 12.0f }, { "tail_low_cut", 61.81f }, { "tail_high_cut", 5328.f }, { "master_clip_drive", 10.52f }, { "tail_attack", 49.5f } } },
            { "RAW - Buzz Saw", { { "core_auto_length", 0 }, { "core_length", 1150.f }, { "tail_character", 3 }, { "tail_source", 2 }, { "tail_st2_type", 2 }, { "pe_start", 359.f }, { "pe_time", 400.f }, { "pe_curve", 0.06f }, { "tail_drive", 0.2f }, { "tail_tone", 9801.f }, { "tail_resonance", 0.0f }, { "tail_decay", 182.f }, { "tail_sustain", 0.35f }, { "tail_length", 1100.f }, { "body_level", -3.0f }, { "body_pitch", 276.f }, { "sub_level", -23.14f }, { "punch_level", -10.79f }, { "punch_pitch", 120.f }, { "punch_drive", 0.44f }, { "tail_level", -3.14f }, { "tail_uni_detune", 32.26f }, { "tail_uni_drift", 20.0f }, { "tail_uni_mix", 0.2f }, { "tail_st2_drive", 13.2f }, { "tail_mid_hz", 597.f }, { "tail_mid_gain", -11.06f }, { "tail_low_cut", 21.6f }, { "tail_high_cut", 15214.f }, { "master_clip_drive", 7.48f }, { "tail_attack", 22.13f } } },
            { "EARLY - Short Tonal", { { "core_auto_length", 0 }, { "core_length", 420.f }, { "tail_character", 2 }, { "tail_source", 2 }, { "tail_st2_type", 0 }, { "pe_start", 600.f }, { "pe_time", 71.55f }, { "pe_curve", 0.67f }, { "tail_drive", 0.95f }, { "tail_tone", 7691.f }, { "tail_resonance", 0.45f }, { "tail_decay", 924.f }, { "tail_sustain", 0.3f }, { "tail_length", 150.f }, { "body_level", -5.33f }, { "body_pitch", 109.f }, { "sub_level", -9.16f }, { "punch_level", 0.0f }, { "punch_pitch", 283.f }, { "punch_drive", 0.54f }, { "tail_level", -8.0f }, { "tail_uni_detune", 30.07f }, { "tail_uni_drift", 9.64f }, { "tail_uni_mix", 0.86f }, { "tail_st2_drive", 0.87f }, { "tail_mid_hz", 732.f }, { "tail_mid_gain", -7.01f }, { "tail_low_cut", 53.14f }, { "tail_high_cut", 5466.f }, { "master_clip_drive", 2.3f }, { "tail_attack", 45.9f } } },
            { "CORE - Tonal Classic", { { "tail_character", 1 }, { "tail_drive", 0.75f }, { "tail_tone", 6000.f }, { "tail_resonance", 0.3f },
                                        { "pe_start", 260.f }, { "pe_time", 110.f }, { "pe_curve", 0.35f }, { "body_level", -12.f } } },
            { "CORE - Dirty Raw",     { { "tail_character", 2 }, { "tail_drive", 0.9f }, { "tail_source", 2 }, { "tail_tone", 7000.f },
                                        { "pe_mode", 1 }, { "pe_start", 300.f }, { "pe_time", 120.f }, { "punch_drive", 0.6f } } },
            { "CORE - Scream Tail",   { { "tail_character", 3 }, { "tail_drive", 0.7f }, { "tail_tone", 8000.f }, { "pe_start", 240.f },
                                        { "core_length", 600.f } } },
            { "CORE - Metallic",      { { "tail_character", 4 }, { "tail_drive", 0.6f }, { "tail_tone", 9000.f }, { "pe_mode", 3 }, { "pe_time", 160.f } } },
            { "CORE - Buzz",          { { "tail_character", 5 }, { "tail_drive", 0.5f }, { "tail_tone", 5000.f }, { "tail_resonance", 0.4f } } },
            { "CORE - Crushed",       { { "tail_character", 6 }, { "tail_drive", 0.55f }, { "tail_tone", 10000.f }, { "pe_mode", 2 } } },
            { "CORE - Short Punchy",  { { "core_length", 220.f }, { "punch_level", 0.f }, { "punch_drive", 0.5f }, { "tail_drive", 0.7f } } },
            { "CORE - Long Rise",     { { "core_length", 1200.f }, { "pe_mode", 4 }, { "pe_start", 98.f }, { "pe_time", 600.f }, { "tail_character", 1 } } },
        };
        return list;
    }
}
