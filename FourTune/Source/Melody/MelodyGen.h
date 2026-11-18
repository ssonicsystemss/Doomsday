#pragma once
// =====================================================================
//  4TUNE - MELODY ENGINE (spec 21-22). Pure logic: settings + seed -> notes. No DSP, no GUI, no files.
//  A hardstyle melody = a chord per bar, chord tones on the strong steps, scale steps between them, a motif that repeats
//  and answers itself, a resolution at the end of the phrase. The 9 MODES change these rules, not just the notes.
//  Reference: the owner's MIDI (Wildstylez - No Time To Waste): a shuffled bounce, a lead note 2/3 beat on the beat +
//  a low chord root on the triplet off-beat ("ROOT BOUNCE"), one chord per bar (i - VI - VII - V style).
// =====================================================================
#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace tune::melody
{
    struct Note { double start = 0, len = 0.5; int pitch = 60; int vel = 100; };

    enum Scale { Major, Minor, HarmonicMinor, MelodicMinor, Dorian, Phrygian, Chromatic, Custom, numScales };
    enum Mode  { Classic, Euphoric, Rawphoric, MelodicRaw, Festival, Emotional, Anthem, Dark, Minimal, numModes };
    enum Grid  { Quarter, Eighth, Shuffle, Sixteenth };     // rhythm grid (RHYTHM DENSITY + mode pick it)

    inline const char* scaleName (int s)
    {
        static const char* n[] { "MAJOR", "MINOR", "HARMONIC MINOR", "MELODIC MINOR", "DORIAN", "PHRYGIAN", "CHROMATIC", "CUSTOM" };
        return n[std::clamp (s, 0, numScales - 1)];
    }
    inline const char* modeName (int m)
    {
        static const char* n[] { "CLASSIC", "EUPHORIC", "RAWPHORIC", "MELODIC RAW", "FESTIVAL", "EMOTIONAL", "ANTHEM", "DARK", "MINIMAL" };
        return n[std::clamp (m, 0, numModes - 1)];
    }
    inline const char* keyName (int k)
    {
        static const char* n[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return n[((k % 12) + 12) % 12];
    }

    struct Settings
    {
        int key = 11;                  // B (No Time To Waste is in B minor)
        int scale = Minor;
        uint16_t customMask = 0xAD5;    // C D E F G A B (used only by CUSTOM)
        int bars = 4;                  // phrase length: 1, 2, 4, 8
        float density = 0.6f;          // how many of the rhythm slots get a note
        float complexity = 0.4f;       // syncopation, passing notes, rhythm variety
        float repetition = 0.6f;       // how much the motif comes back
        float movement = 0.5f;         // interval size / contour range
        float rhythm = 0.5f;           // rhythm density: 1/4 -> 1/8 -> shuffle 1/8T -> 1/16
        int octLow = 4, octHigh = 5;   // melody range (C4 = 60)
        int mode = Euphoric;
        bool rootBounce = true;        // the low chord root on the off-beat (the reference's bounce)
        uint32_t seed = 1;
    };

    // ------------------------------------------------------------------ scales
    inline std::vector<int> scaleSteps (const Settings& s)
    {
        static const int maj[] { 0, 2, 4, 5, 7, 9, 11 }, mnr[] { 0, 2, 3, 5, 7, 8, 10 }, hm[] { 0, 2, 3, 5, 7, 8, 11 },
                         mm[] { 0, 2, 3, 5, 7, 9, 11 }, dor[] { 0, 2, 3, 5, 7, 9, 10 }, phr[] { 0, 1, 3, 5, 7, 8, 10 };
        const int* t = nullptr;
        switch (s.scale)
        {
            case Major: t = maj; break; case Minor: t = mnr; break; case HarmonicMinor: t = hm; break;
            case MelodicMinor: t = mm; break; case Dorian: t = dor; break; case Phrygian: t = phr; break;
            case Chromatic: { std::vector<int> v; for (int i = 0; i < 12; ++i) v.push_back (i); return v; }
            default:
            {
                std::vector<int> v;
                for (int i = 0; i < 12; ++i) if (s.customMask & (1u << i)) v.push_back (i);
                if (v.empty()) v = { 0, 2, 3, 5, 7, 8, 10 };
                return v;
            }
        }
        return std::vector<int> (t, t + 7);
    }
    inline bool inScale (const Settings& s, int pitch)
    {
        const int pc = (((pitch - s.key) % 12) + 12) % 12;
        for (int x : scaleSteps (s)) if (x == pc) return true;
        return false;
    }

    // ------------------------------------------------------------------ the generator
    class Generator
    {
    public:
        std::vector<Note> generate (const Settings& in)
        {
            s = in;
            s.bars = s.bars <= 1 ? 1 : s.bars <= 2 ? 2 : s.bars <= 4 ? 4 : 8;
            s.octHigh = std::max (s.octLow, s.octHigh);
            rng = s.seed * 2654435761u + 0x9E3779B9u + (uint32_t) s.mode * 7919u;
            steps = scaleSteps (s);
            lo = 12 * (s.octLow + 1); hi = 12 * (s.octHigh + 2) - 1;       // OCT 4-5 = C4..B5 (the reference lead: F#4..E5)
            if (hi - lo < 12) hi = lo + 12;

            progression();
            grid = pickGrid();
            std::vector<Note> out;

            // ---- 1. the motif rhythm (one bar) and an answer variant
            auto motif = rhythmBar (true);
            auto answer = chance (s.complexity) ? rhythmBar (false) : motif;

            // ---- 2. phrase form: bars reuse the motif depending on REPETITION and the mode
            int prevPitch = startPitch();
            std::vector<std::vector<int>> barPitches;      // pitch contour of each bar (for repetition)
            for (int bar = 0; bar < s.bars; ++bar)
            {
                const bool last = bar == s.bars - 1;
                const bool question = s.bars >= 2 && bar == s.bars / 2 - 1;     // end of the first half = "question"
                const auto& rh = (bar % 2 == 1 && motifHalfAnswer()) ? answer : motif;
                const int chord = chordOf (bar);

                // repetition: bars of the second half copy the first half (transposed to the new chord), but the end is new
                const int src = bar - std::max (1, s.bars / 2);
                const bool copy = s.bars >= 2 && src >= 0 && ! last && chance (s.repetition * repeatBias());
                std::vector<int> pitches;
                for (size_t i = 0; i < rh.size(); ++i)
                {
                    const auto& r = rh[i];
                    int p;
                    if (copy && src < (int) barPitches.size() && i < barPitches[(size_t) src].size())
                        p = fitToChord (barPitches[(size_t) src][i] + (chordRoot (chord) - chordRoot (chordOf (src))), chord, r.strong);
                    else p = nextPitch (prevPitch, chord, r.strong, bar, (int) i, (int) rh.size());
                    // the phrase ends: resolve (root / fifth of the question)
                    if (i + 1 == rh.size() && (last || question)) p = resolve (p, chord, last);
                    while (p > hi) p -= 12;                 // out of range: an octave back, never clamp (a clamped note leaves the key)
                    while (p < lo) p += 12;
                    if (s.scale != Chromatic) p = snapToScale (p);
                    if (p > hi) p = stepScale (p, -1);
                    pitches.push_back (p);
                    prevPitch = p;
                    Note n; n.start = bar * 4.0 + r.start; n.len = r.len; n.pitch = p; n.vel = r.strong ? 104 : 96;
                    out.push_back (n);
                }
                barPitches.push_back (pitches);

                // ---- 3. ROOT BOUNCE: the low chord root on the shuffle off-beats where the lead rests
                if (s.rootBounce) bounce (out, bar, chord, rh);
            }
            std::sort (out.begin(), out.end(), [] (const Note& a, const Note& b) { return a.start < b.start || (a.start == b.start && a.pitch < b.pitch); });
            return out;
        }

        // chord roots of the last generated phrase (the piano roll shades them)
        std::vector<int> chordRootsPc() const
        {
            std::vector<int> v;
            for (int b = 0; b < s.bars; ++b) v.push_back (((chordRoot (chordOf (b)) % 12) + 12) % 12);
            return v;
        }

    private:
        struct Slot { double start, len; bool strong; };
        Settings s; uint32_t rng = 1; std::vector<int> steps; int lo = 60, hi = 84; Grid grid = Eighth;
        std::vector<int> prog;                 // scale degree of the chord per bar (0 = i)

        float rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float) (rng & 0xFFFFFF) / 16777216.f; }
        bool chance (float p) { return rnd() < p; }
        int pick (int n) { return std::min (n - 1, (int) (rnd() * (float) n)); }

        // ---------------------------------------------------- harmony
        void progression()
        {
            // one chord per bar, as scale degrees (minor-key thinking; works on major too: degrees are scale relative)
            static const std::vector<std::vector<int>> forms[numModes] {
                { { 0, 5, 2, 6 }, { 0, 5, 6, 4 }, { 0, 3, 5, 6 } },             // CLASSIC   i-VI-III-VII, i-VI-VII-v, i-iv-VI-VII
                { { 0, 5, 2, 6 }, { 5, 6, 0, 0 }, { 0, 5, 6, 4 } },             // EUPHORIC  i-VI-III-VII, VI-VII-i, i-VI-VII-V
                { { 0, 5, 6, 0 }, { 0, 3, 5, 4 }, { 0, 0, 5, 6 } },             // RAWPHORIC i-VI-VII-i, i-iv-VI-V
                { { 0, 6, 5, 6 }, { 0, 5, 3, 4 } },                             // MELODIC RAW
                { { 5, 6, 0, 0 }, { 5, 6, 2, 4 } },                             // FESTIVAL  VI-VII-i (the drop)
                { { 0, 5, 3, 6 }, { 5, 3, 0, 4 } },                             // EMOTIONAL i-VI-iv-VII
                { { 0, 0, 5, 6 }, { 0, 5, 0, 6 } },                             // ANTHEM    chant over few chords
                { { 0, 1, 0, 6 }, { 0, 5, 1, 0 } },                             // DARK      phrygian bII
                { { 0, 0, 5, 5 }, { 0, 0, 0, 6 } } };                           // MINIMAL
            const auto& f = forms[std::clamp (s.mode, 0, numModes - 1)];
            const auto& base = f[(size_t) pick ((int) f.size())];
            // 1 bar = the first chord, 2 bars = the 1st and 3rd, 4 bars = all, 8 bars = each chord for two bars
            prog.clear();
            for (int b = 0; b < s.bars; ++b)
                prog.push_back (base[(size_t) (s.bars == 1 ? 0 : s.bars == 2 ? b * 2 : s.bars == 4 ? b : b / 2)]);
        }
        int chordOf (int bar) const { return prog.empty() ? 0 : prog[(size_t) std::clamp (bar, 0, (int) prog.size() - 1)]; }
        int degreePitch (int degree) const      // scale degree (any octave) -> semitones above the key
        {
            const int n = (int) steps.size();
            const int oct = (int) std::floor ((double) degree / n);
            return steps[(size_t) (((degree % n) + n) % n)] + 12 * oct;
        }
        int chordRoot (int degree) const { return s.key + degreePitch (degree); }
        bool isChordTone (int pitch, int chord) const
        {
            for (int k : { 0, 2, 4 })
            {
                const int pc = ((s.key + degreePitch (chord + k)) % 12 + 12) % 12;
                if (((pitch % 12) + 12) % 12 == pc) return true;
            }
            return false;
        }
        // the chord tone nearest to p (strong steps) or p unchanged
        int fitToChord (int p, int chord, bool strong) const
        {
            if (! strong || isChordTone (p, chord)) return p;
            for (int d = 1; d < 7; ++d) { if (isChordTone (p + d, chord)) return p + d; if (isChordTone (p - d, chord)) return p - d; }
            return p;
        }
        int snapToScale (int p) const
        {
            if (inScale (s, p)) return p;
            // the mode may keep a chromatic colour (raw: b2 / leading tone) - inScale handles the scale; else nearest scale note
            for (int d = 1; d < 3; ++d) { if (inScale (s, p - d)) return p - d; if (inScale (s, p + d)) return p + d; }
            return p;
        }

        // ---------------------------------------------------- rhythm
        Grid pickGrid()
        {
            float r = s.rhythm;
            switch (s.mode)
            {
                case Classic: case Euphoric: if (r > 0.3f && r < 0.8f) return Shuffle; break;   // the reference bounce
                case Minimal: r *= 0.5f; break;
                case Anthem:  r = std::max (r, 0.35f); break;
                case Rawphoric: case MelodicRaw: r = std::min (1.f, r + 0.15f); break;
                default: break;
            }
            return r < 0.25f ? Quarter : r < 0.6f ? Eighth : r < 0.8f ? Shuffle : Sixteenth;
        }
        // one bar of slots (start, length in beats, strong?)
        std::vector<Slot> rhythmBar (bool main)
        {
            std::vector<Slot> v;
            float dens = s.density;
            if (s.mode == Minimal) dens *= 0.55f;
            if (s.mode == Festival || s.mode == Emotional) dens *= 0.8f;
            if (s.mode == Anthem) dens = std::max (dens, 0.75f);
            const float sync = s.complexity * (s.mode == Classic ? 0.5f : s.mode == Anthem ? 0.3f : 1.f);
            if (grid == Shuffle)
            {
                // every beat: the lead note 2/3 on the beat; the off-beat third = lead (complexity) or left to ROOT BOUNCE
                for (int b = 0; b < 4; ++b)
                {
                    const bool on = b == 0 || chance (0.55f + 0.45f * dens);
                    const bool off = chance (sync * 0.6f) || (! s.rootBounce && chance (dens * 0.5f));
                    if (on) v.push_back ({ (double) b, off ? 2.0 / 3.0 : (s.rootBounce ? 2.0 / 3.0 : 1.0), true });
                    if (off) v.push_back ({ b + 2.0 / 3.0, 1.0 / 3.0, false });
                }
                return v;
            }
            const double step = grid == Quarter ? 1.0 : grid == Eighth ? 0.5 : 0.25;
            const int n = (int) std::round (4.0 / step);
            for (int i = 0; i < n; ++i)
            {
                const double t = i * step;
                const bool beat = std::fmod (t, 1.0) < 1e-6;
                float p = beat ? 0.55f + 0.45f * dens : dens * (0.35f + 0.65f * sync);
                if (i == 0) p = 1.f;
                if (s.mode == Anthem && ! main) p = beat ? 1.f : 0.6f;
                if (! chance (p)) continue;
                v.push_back ({ t, step, beat });
            }
            // lengths: hold until the next note (legato lead), the festival / emotional modes keep long notes
            for (size_t i = 0; i < v.size(); ++i)
            {
                const double next = i + 1 < v.size() ? v[i + 1].start : 4.0;
                const double gap = next - v[i].start;
                const bool longNote = s.mode == Festival || s.mode == Emotional || s.mode == Minimal;
                v[i].len = longNote ? gap : std::min (gap, std::max (step, gap * (chance (0.5f) ? 1.0 : 0.5)));
            }
            return v;
        }
        bool motifHalfAnswer() const { return s.mode != Anthem && s.mode != Minimal; }
        float repeatBias() const
        {
            switch (s.mode) { case Classic: case Anthem: case Minimal: return 1.3f; case Euphoric: case Festival: return 1.0f; default: return 0.8f; }
        }

        // ---------------------------------------------------- pitch
        int startPitch()
        {
            // a chord tone near the middle of the range (the melody then has room both ways)
            const int mid = (lo + hi) / 2;
            int p = chordRoot (chordOf (0)) + degreePitch (chordOf (0) + 2 * pick (3)) - degreePitch (chordOf (0));
            while (p < mid - 6) p += 12;
            while (p > mid + 6) p -= 12;
            return fitToChord (p, chordOf (0), true);
        }
        int nextPitch (int prev, int chord, bool strong, int bar, int idx, int count)
        {
            // interval size: MOVEMENT + the mode (euphoric / festival leap, classic / anthem step, raw jumps, emotional steps)
            float leap = s.movement;
            switch (s.mode)
            {
                case Euphoric: leap = 0.25f + 0.75f * leap; break;
                case Festival: leap = 0.35f + 0.65f * leap; break;
                case Classic: case Anthem: leap *= 0.6f; break;
                case Emotional: leap *= 0.45f; break;
                case Minimal: leap *= 0.5f; break;
                default: break;
            }
            // contour: rise to the middle of the phrase, fall to the end (an arch), steered by MOVEMENT
            const double pos = (bar * count + idx) / (double) std::max (1, s.bars * count);
            const int mid = (lo + hi) / 2;
            int dir = pos < 0.45 ? 1 : -1;
            if (prev > mid + 7) dir = -1;            // near the top / bottom of the range: turn back (no clamping to one note)
            if (prev < mid - 7) dir = 1;
            int deg = 1;
            if (chance (leap * 0.7f)) deg = 2 + pick (chance (leap) ? 4 : 2);          // a third .. a sixth
            if ((s.mode == Euphoric || s.mode == Festival) && chance (leap * 0.12f)) deg = 7;   // the octave jump
            const int sign = chance (0.65f) ? dir : -dir;
            int p = stepScale (prev, sign * deg);
            // raw / dark colours: a chromatic neighbour now and then
            if ((s.mode == Rawphoric || s.mode == Dark) && ! strong && chance (0.18f * s.complexity + 0.06f)) p = prev + (chance (0.5f) ? 1 : -1);
            if (s.mode == Dark) p -= (p > (lo + hi) / 2 ? 12 : 0);
            if (strong) p = fitToChord (p, chord, true);
            while (p > hi) p -= 12;
            while (p < lo) p += 12;
            return p;
        }
        int stepScale (int p, int degrees) const
        {
            // move p by 'degrees' scale steps
            int q = p; const int dir = degrees > 0 ? 1 : -1;
            for (int k = 0; k < std::abs (degrees); ++k) { q += dir; while (! inScale (s, q)) q += dir; }
            return q;
        }
        int resolve (int p, int chord, bool end) const
        {
            // the end of the phrase lands on the root (euphoric: strongly), the question on the fifth / third of its chord
            const int target = end ? s.key : chordRoot (chord) + degreePitch (chord + (s.mode == Euphoric ? 4 : 2)) - degreePitch (chord);
            int best = p, bestD = 99;
            for (int o = -2; o <= 2; ++o)
            {
                const int c = (((target % 12) + 12) % 12) + 12 * ((p / 12) + o);
                if (c < lo || c > hi) continue;
                if (std::abs (c - p) < bestD) { bestD = std::abs (c - p); best = c; }
            }
            return best;
        }
        void bounce (std::vector<Note>& out, int bar, int chord, const std::vector<Slot>& rh)
        {
            int root = chordRoot (chord);
            while (root >= lo - 12) root -= 12;        // an octave+ under the melody (the reference: B1 / G2 / A2 / F#2)
            while (root < lo - 24) root += 12;
            for (int b = 0; b < 4; ++b)
            {
                const double at = grid == Shuffle ? b + 2.0 / 3.0 : b + 0.5;
                const double len = grid == Shuffle ? 1.0 / 3.0 : 0.5;
                bool busy = false;
                for (auto& r : rh) if (std::abs (r.start - at) < 1e-6) busy = true;
                if (busy) continue;
                Note n; n.start = bar * 4.0 + at; n.len = len; n.pitch = root; n.vel = 92;
                out.push_back (n);
            }
        }
    };

    // ------------------------------------------------------------------ editing helpers (piano roll / tests)
    inline void quantize (std::vector<Note>& v, double grid, double strength = 1.0)
    {
        for (auto& n : v)
        {
            const double q = std::round (n.start / grid) * grid;
            n.start += (q - n.start) * strength;
            const double ql = std::max (grid, std::round (n.len / grid) * grid);
            n.len += (ql - n.len) * strength;
        }
    }
    inline void humanize (std::vector<Note>& v, uint32_t seed, double timeBeats = 0.02, int velRange = 12)
    {
        uint32_t r = seed * 747796405u + 2891336453u;
        auto rnd = [&r] { r ^= r << 13; r ^= r >> 17; r ^= r << 5; return (double) (r & 0xFFFF) / 65535.0 * 2.0 - 1.0; };
        for (auto& n : v) { n.start = std::max (0.0, n.start + rnd() * timeBeats); n.vel = std::clamp (n.vel + (int) std::round (rnd() * velRange), 1, 127); }
    }
    inline void transpose (std::vector<Note>& v, int semis) { for (auto& n : v) n.pitch = std::clamp (n.pitch + semis, 0, 127); }
}
