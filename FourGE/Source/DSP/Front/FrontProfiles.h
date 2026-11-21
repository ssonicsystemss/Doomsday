#pragma once
// =====================================================================
//  4GE - the PUNCH TYPE profiles for the shared CLICK / SUB PUNCH modules (owner 2026-11-16): the modules are common, every PUNCH
//  TYPE changes HOW they are used - internal offsets on top of the user's knobs (never overwriting them; LEVEL 0 stays off).
//  INTERNAL click / sub punch = built into a PUNCH model (its sound); GLOBAL = the shared FRONT modules. In AUTO the engine asks the
//  PUNCH TYPE profile whether the global ones are needed (ON always adds them, OFF never).
//  Index = punch_mode: PUNCH, TOK (the classic generator), EUPHORIC, PVC, ZAAG, CLASSIC (EARLY), RAW.
// =====================================================================
namespace fge::dsp::front
{
    struct Profile
    {
        float clickDb, clickLenMul, clickTone, clickDrive;     // CLICK: level offset, length x, tone +, drive +
        float subDb, subLenMul, subDrive, subTone;             // SUB PUNCH: level offset, length x, drive +, tone +
        bool globalInAuto;                                     // AUTO: does this PUNCH TYPE want the GLOBAL click? (no when the model has its own)
        bool subInAuto;                                        // AUTO: ... the GLOBAL sub punch?
    };
    inline const Profile& profileFor (int punchMode)
    {
        static const Profile p[7] {
            {  0.f, 1.0f,  0.0f, 0.0f,    0.f, 1.0f, 0.0f,  0.0f, true, true },    // PUNCH (classic): neutral
            {  0.f, 1.0f,  0.0f, 0.0f,    0.f, 1.0f, 0.0f,  0.0f, true, true },    // TOK (classic): neutral
            { -3.f, 0.8f,  0.1f, 0.0f,    1.f, 1.3f, 0.0f, -0.1f, false, false },   // EUPHORIC (its own INTERNAL click + sub): a softer, shorter click over the crack; the sub punch longer, cleaner
            { -1.f, 0.6f, -0.1f, 0.0f,    1.f, 0.9f, 0.1f,  0.0f, false, false },   // PVC (its own INTERNAL click / tube + sub): a dry short click; the sub punch earlier / firmer under the tube
            {  2.f, 1.0f,  0.15f, 0.2f,   1.f, 1.0f, 0.3f,  0.1f, false, false },  // ZAAG (its own INTERNAL click / pup + thump)
            {  0.f, 1.0f,  0.0f, 0.0f,    0.f, 0.8f, 0.0f,  0.0f, false, true },   // CLASSIC (EARLY REWORK): its own transient (no global click), a short controlled global sub punch
            {  0.f, 1.0f,  0.0f, 0.0f,    0.f, 0.7f, 0.2f,  0.0f, false, true } }; // RAW (owner 2026-11-21): its own SNAP (no global click), a short firm global sub punch under the crack
        return p[punchMode < 0 ? 0 : punchMode > 6 ? 6 : punchMode];
    }
}
