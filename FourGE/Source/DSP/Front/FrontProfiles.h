#pragma once
// =====================================================================
//  4GE - the PUNCH TYPE profiles for the shared CLICK / SUB PUNCH modules (owner 2026-11-16): the modules are common, every PUNCH
//  TYPE changes HOW they are used - internal offsets on top of the user's knobs (never overwriting them; LEVEL 0 stays off).
//  Index = punch_mode: PUNCH, TOK (the classic generator), EUPHORIC, PVC, ZAAG.
// =====================================================================
namespace fge::dsp::front
{
    struct Profile
    {
        float clickDb, clickLenMul, clickTone, clickDrive;     // CLICK: level offset, length x, tone +, drive +
        float subDb, subLenMul, subDrive, subTone;             // SUB PUNCH: level offset, length x, drive +, tone +
    };
    inline const Profile& profileFor (int punchMode)
    {
        static const Profile p[5] {
            {  0.f, 1.0f,  0.0f, 0.0f,    0.f, 1.0f, 0.0f,  0.0f },   // PUNCH (classic): neutral
            {  0.f, 1.0f,  0.0f, 0.0f,    0.f, 1.0f, 0.0f,  0.0f },   // TOK (classic): neutral
            { -3.f, 0.8f,  0.1f, 0.0f,    1.f, 1.3f, 0.0f, -0.1f },   // EUPHORIC: a softer, shorter click over the crack; the sub punch longer, cleaner
            { -1.f, 0.6f, -0.1f, 0.0f,    1.f, 0.9f, 0.1f,  0.0f },   // PVC: a dry short click; the sub punch earlier / firmer under the tube
            {  2.f, 1.0f,  0.15f, 0.2f,   1.f, 1.0f, 0.3f,  0.1f } }; // ZAAG: a strong, bright click; a heavy driven sub punch
        return p[punchMode < 0 ? 0 : punchMode > 4 ? 4 : punchMode];
    }
}
