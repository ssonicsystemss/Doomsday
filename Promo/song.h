#pragma once
// shared timeline for the promo: 150 BPM, 48 kHz, 30 fps (1600 samples per frame)
constexpr double SR = 48000.0;
constexpr int FPS = 30;
constexpr int SPF = 1600;                       // samples per frame
constexpr double BPM = 150.0;
constexpr int BEAT = 19200;                     // samples per beat
constexpr int BAR = BEAT * 4;
constexpr int BARS = 12;
constexpr int TAIL = 48000;                     // 1 s ring-out after the last bar
constexpr int TOTAL = BAR * BARS + TAIL;

// kick on every beat from bar 2 up to bar 11 beat 2 (then the last screech note rings alone)
inline bool kickAtBeat (int beat) { return beat >= 8 && beat < 4 * 11 + 2; }
