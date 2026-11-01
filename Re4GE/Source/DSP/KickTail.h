#pragma once
// RE4GE - KICK TAIL source: 4GE's kick voice (tail + distortion only, no punch / click), rendered forwards and played BACKWARDS
// so its start (the highest pitch, the dirtiest part) lands on the end of the gate.
//
// Why rendered and not generated backwards in time: the tail ROOM (reverb BEFORE reversing, so it swells up) and the filters'
// ringing need the tail's future - backwards that is the pre-swell the owner likes. So it is a render, but:
//   - never a CPU spike: the render is spread over the chunks until the bass is first heard (the silent DELAY before it),
//     finished at once only when it is needed right away (GATE mode, DUCK > 0);
//   - two buffers: the new tail renders into the back one while the front one plays, and they swap when it is complete;
//   - live knobs: in PATTERN mode every step checks whether the tail's knobs (or the note) changed and re-renders if so.
// Bit-identical to the v0.2 render-on-note (the same samples, only computed at other moments).
#include "ReParams.h"
#include <array>
#include <vector>

namespace re
{
    class KickTail
    {
    public:
        KickTail() { for (auto& b : buf) b.assign ((size_t) (192000 * 2.1), 0.f); }
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            voice.prepare (sr);
            cap = juce::jmin ((int) buf[0].size(), (int) (sr * 2.0));
            len = { 0, 0 }; rendering = false; key = {};
        }
        // the key of a tail: the knobs that change it + the note + its length (a re-render only when one of them changed)
        struct Key
        {
            float v[16] {};
            bool operator== (const Key& o) const { for (int i = 0; i < 16; ++i) if (v[i] != o.v[i]) return false; return true; }
        };
        static Key keyOf (const Params& p, double hz, int lenSamples)
        {
            const auto& t = p.tail;
            return { { t.startRatio, t.dropMs, t.curve, t.wave, (float) t.distType, t.driveDb, t.tone, t.highCutHz, t.scream, t.screamHz,
                       p.tailRoom, p.tailRoomSize, (float) hz, (float) lenSamples, t.lowCutHz, (float) t.dist2Type } };
        }
        // start rendering the tail for this note (hz incl. TUNE / FINE) and this length (ms); skipped if the same tail is ready / on its way
        void begin (const Params& p, double hz, double lenMs)
        {
            const int n = juce::jlimit (2, cap, (int) (lenMs * 0.001 * sr));
            const Key k = keyOf (p, hz, n);
            if (k == key && (rendering || len[front] > 0)) return;
            key = k;
            tp = p.tail;
            tp.punchOn = false; tp.clickDb = -60.f; tp.headFixed = false; tp.texDb = -60.f; tp.mute = 0; tp.duck = 0.f; tp.tailAttackMs = 0.f;
            tp.busType = 0; tp.cleanSubHz = 0.f;
            tp.room = p.tailRoom; tp.roomSize = p.tailRoomSize; tp.roomIntoDist = false; tp.roomLowCutHz = 150.f;
            voice.setRoom (p.tailRoomSize);
            tp.amp = ge::Shape::make ({ { 0.f, 1.f }, { 1000.f, 1.f } });
            tp.drive = ge::Shape::make ({ { 0.f, 1.f }, { 1000.f, 1.f } });
            target = n;
            tp.lengthMs = (float) (target * 1000.0 / sr) + 20.f;
            tp.cook (sr);
            voice.trigger ((float) hz, 1.f, tp);
            done = 0; rendering = true;
        }
        // render up to `budget` more samples into the back buffer; swaps when complete
        void advance (int budget)
        {
            if (! rendering) return;
            auto& b = buf[(size_t) (1 - front)];
            const int e = juce::jmin (target, done + juce::jmax (0, budget));
            for (int i = done; i < e; ++i) b[(size_t) i] = voice.render (tp);
            done = e;
            if (done >= target) { rendering = false; front = 1 - front; len[front] = target; }
        }
        void finish() { advance (target); }
        // spread the rest over the chunks left before it is heard (samplesLeft at the base rate, chunk = samples per chunk)
        void pace (double samplesLeft, int chunk)
        {
            if (! rendering) return;
            const int chunksLeft = (int) (samplesLeft / juce::jmax (1, chunk));
            if (chunksLeft <= 1) { finish(); return; }
            const int rest = target - done;
            advance ((rest + chunksLeft - 2) / (chunksLeft - 1));        // done one chunk early
        }
        bool busy() const { return rendering; }
        const float* data() const { return buf[(size_t) front].data(); }
        int length() const { return len[front]; }

    private:
        std::array<std::vector<float>, 2> buf;
        std::array<int, 2> len { 0, 0 };
        int front = 0, cap = 0, target = 0, done = 0;
        bool rendering = false;
        double sr = 44100.0;
        Key key {};
        ge::Params tp;
        ge::Voice voice;
    };
}
