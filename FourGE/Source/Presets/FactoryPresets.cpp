#include "FactoryPresets.h"

namespace fge
{
    const std::vector<FactoryPreset>& factoryPresets()
    {
        static const std::vector<FactoryPreset> list {
            { "INIT", {} },
            // matched to the owner's reference kicks (module 01 core only: one oscillator per part, no unison / noise / stages yet)
            { "MODERN - Festival Drive", { { "core_auto_length", 0 }, { "core_length", 700.f }, { "tail_character", 1 }, { "tail_source", 2 }, { "pe_start", 504.f },
                                           { "pe_time", 173.f }, { "pe_curve", 0.72f }, { "tail_drive", 0.66f }, { "tail_tone", 14869.f }, { "tail_resonance", 0.15f },
                                           { "tail_decay", 135.f }, { "tail_sustain", 1.0f }, { "tail_length", 660.f }, { "body_level", -17.7f }, { "body_pitch", 215.f },
                                           { "sub_level", -11.2f }, { "punch_level", -12.3f }, { "punch_pitch", 529.f }, { "punch_drive", 0.06f }, { "tail_level", -7.7f } } },
            { "RAW - Buzz Saw",          { { "core_auto_length", 0 }, { "core_length", 1150.f }, { "tail_character", 5 }, { "tail_source", 2 }, { "pe_start", 500.f },
                                           { "pe_time", 119.f }, { "pe_curve", 0.8f }, { "tail_drive", 0.87f }, { "tail_tone", 6836.f }, { "tail_resonance", 0.36f },
                                           { "tail_decay", 295.f }, { "tail_sustain", 0.73f }, { "tail_length", 1118.f }, { "body_level", -6.3f }, { "body_pitch", 230.f },
                                           { "sub_level", -14.3f }, { "punch_level", -2.4f }, { "punch_pitch", 154.f }, { "punch_drive", 1.0f }, { "tail_level", -3.f } } },
            { "EARLY - Short Tonal",     { { "core_auto_length", 0 }, { "core_length", 420.f }, { "tail_character", 1 }, { "tail_source", 2 }, { "pe_start", 134.f },
                                           { "pe_time", 87.5f }, { "pe_curve", 0.45f }, { "tail_drive", 0.2f }, { "tail_tone", 5252.f }, { "tail_resonance", 0.0f },
                                           { "tail_decay", 1226.f }, { "tail_sustain", 0.3f }, { "tail_length", 150.f }, { "body_level", -3.0f }, { "body_pitch", 108.f },
                                           { "sub_level", -8.3f }, { "punch_level", -15.8f }, { "punch_pitch", 531.f }, { "punch_drive", 0.58f }, { "tail_level", -6.5f } } },
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
