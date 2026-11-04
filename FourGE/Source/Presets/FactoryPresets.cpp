#include "FactoryPresets.h"

namespace fge
{
    const std::vector<FactoryPreset>& factoryPresets()
    {
        static const std::vector<FactoryPreset> list {
            { "INIT", {} },
            // matched to the owner's reference kicks (sound pass + pitch HOLD: the tonal "tok" held, then a sharp drop)
            { "MODERN - Festival Drive", { { "core_auto_length", 0 }, { "core_length", 1000.00f }, { "tail_character", 1 }, { "tail_source", 0 }, { "tail_st2_type", 0 }, { "pe_start", 483.00f }, { "pe_time", 15.49f }, { "tail_penv_offset", 125.85f }, { "pe_curve", 0.80f }, { "tail_drive", 0.86f }, { "tail_tone", 11384.82f }, { "tail_resonance", 0.49f }, { "tail_decay", 1678.50f }, { "tail_sustain", 0.88f }, { "tail_length", 597.80f }, { "body_level", -15.84f }, { "body_pitch", 118.07f }, { "sub_level", -3.00f }, { "punch_level", -9.03f }, { "punch_pitch", 352.72f }, { "punch_drive", 0.50f }, { "tail_level", 0.00f }, { "tail_uni_detune", 19.38f }, { "tail_uni_drift", 5.84f }, { "tail_uni_mix", 0.66f }, { "tail_st2_drive", 12.25f }, { "tail_mid_hz", 1733.47f }, { "tail_mid_gain", -1.86f }, { "tail_low_cut", 65.33f }, { "tail_high_cut", 17047.98f }, { "master_clip_drive", 0.36f }, { "tail_attack", 8.00f }, { "sub_length", 685.99f }, { "sub_decay", 623.51f }, { "tail_release", 21.76f }, { "body_length", 436.80f }, { "body_decay", 131.42f } } },
            { "RAW - Buzz Saw", { { "core_auto_length", 0 }, { "core_length", 1000.00f }, { "tail_character", 1 }, { "tail_source", 0 }, { "tail_st2_type", 0 }, { "pe_start", 820.00f }, { "pe_time", 12.00f }, { "tail_penv_offset", 110.00f }, { "pe_curve", 0.72f }, { "tail_drive", 1.00f }, { "tail_tone", 13227.28f }, { "tail_resonance", 0.55f }, { "tail_decay", 968.41f }, { "tail_sustain", 0.77f }, { "tail_length", 767.59f }, { "body_level", -15.84f }, { "body_pitch", 118.07f }, { "sub_level", -3.00f }, { "punch_level", -16.53f }, { "punch_pitch", 288.78f }, { "punch_drive", 0.50f }, { "tail_level", 0.00f }, { "tail_uni_detune", 15.38f }, { "tail_uni_drift", 5.84f }, { "tail_uni_mix", 0.42f }, { "tail_st2_drive", 12.25f }, { "tail_mid_hz", 2225.81f }, { "tail_mid_gain", -1.86f }, { "tail_low_cut", 65.33f }, { "tail_high_cut", 19000.00f }, { "master_clip_drive", 0.36f }, { "tail_attack", 8.00f }, { "sub_length", 395.78f }, { "sub_decay", 655.48f }, { "tail_release", 18.73f }, { "body_length", 570.74f }, { "body_decay", 113.12f } } },
            { "EARLY - Short Tonal", { { "core_auto_length", 0 }, { "core_length", 500.00f }, { "tail_character", 5 }, { "tail_source", 2 }, { "tail_st2_type", 0 }, { "pe_start", 271.05f }, { "pe_time", 15.91f }, { "tail_penv_offset", 0.00f }, { "pe_curve", 0.46f }, { "tail_drive", 0.20f }, { "tail_tone", 3359.68f }, { "tail_resonance", 0.06f }, { "tail_decay", 1476.70f }, { "tail_sustain", 0.50f }, { "tail_length", 200.00f }, { "body_level", -15.82f }, { "body_pitch", 85.72f }, { "sub_level", -3.00f }, { "punch_level", -17.09f }, { "punch_pitch", 406.53f }, { "punch_drive", 0.90f }, { "tail_level", -5.96f }, { "tail_uni_detune", 40.00f }, { "tail_uni_drift", 9.00f }, { "tail_uni_mix", 0.27f }, { "tail_st2_drive", 2.51f }, { "tail_mid_hz", 1088.99f }, { "tail_mid_gain", -12.00f }, { "tail_low_cut", 21.79f }, { "tail_high_cut", 14797.21f }, { "master_clip_drive", 10.31f }, { "tail_attack", 1.04f }, { "sub_length", 262.76f }, { "sub_decay", 50.00f }, { "tail_release", 14.89f }, { "body_length", 477.62f }, { "body_decay", 90.32f } } },
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
