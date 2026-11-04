#pragma once
// =====================================================================
//  4GE MODULE 02 - the multipoint envelope (2..64 points) and the lock-free bank of all drawn envelopes.
//  A shape lives in normalized space: t 0..1 (the owner's time axis is scaled by the envelope's TIME / the part's LENGTH),
//  v -1..+1 (pitch: x depth in semitones, so START / END never rewrite the drawn curve) or 0..1 (amp / filter / drive / reso).
//  Each segment (point i -> i+1) has its own interpolation, evaluated in the DSP (never GUI-only):
//    LINEAR, EXPONENTIAL, LOGARITHMIC, EASE IN, EASE OUT, EASE IN/OUT, BEZIER (two handles), STEP.
//  TENSION (-1..1) bends EXP / LOG / EASE. Pure data + math: no allocation, no locks.
// =====================================================================
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cmath>

namespace fge::env
{
    enum Curve : uint8_t { Linear = 0, Exponential, Logarithmic, EaseIn, EaseOut, EaseInOut, Bezier, Step, NumCurves };
    inline const char* curveName (int c)
    {
        static const char* n[NumCurves] { "LINEAR", "EXP", "LOG", "EASE IN", "EASE OUT", "EASE IN/OUT", "BEZIER", "STEP" };
        return n[juce::jlimit (0, (int) NumCurves - 1, c)];
    }

    struct Point
    {
        float t = 0.f, v = 0.f;
        uint8_t curve = Exponential;      // shape of the segment that STARTS at this point
        float tension = 0.f;              // -1..1
        float h1x = 0.33f, h1y = 0.f, h2x = 0.67f, h2y = 1.f;     // BEZIER handles inside the segment box (0..1)
    };

    constexpr int maxPoints = 64;

    // fraction 0..1 of the way from a to b inside one segment
    inline float segmentShape (const Point& a, float x)
    {
        x = juce::jlimit (0.f, 1.f, x);
        const float k = juce::jlimit (-1.f, 1.f, a.tension);
        switch (a.curve)
        {
            case Linear: return x;
            case Exponential:                         // fast first (classic kick drop); tension = steepness
            {
                const float s = 4.f + 6.f * k;          // -2 .. 10
                if (std::abs (s) < 1.0e-3f) return x;
                return (1.f - std::exp (-s * x)) / (1.f - std::exp (-s));
            }
            case Logarithmic:                         // slow first, fast at the end
            {
                const float s = 4.f + 6.f * k;
                if (std::abs (s) < 1.0e-3f) return x;
                return (std::exp (s * x) - 1.f) / (std::exp (s) - 1.f);
            }
            case EaseIn:    return std::pow (x, 2.f + 2.f * k);
            case EaseOut:   return 1.f - std::pow (1.f - x, 2.f + 2.f * k);
            case EaseInOut: { const float p = 2.f + 2.f * k; return x < 0.5f ? 0.5f * std::pow (2.f * x, p) : 1.f - 0.5f * std::pow (2.f - 2.f * x, p); }
            case Bezier:
            {
                // cubic bezier (0,0) (h1x,h1y) (h2x,h2y) (1,1): solve X(u) = x (Newton), return Y(u)
                const float ax = juce::jlimit (0.f, 1.f, a.h1x), bx = juce::jlimit (0.f, 1.f, a.h2x);
                auto X = [ax, bx] (float u) { const float m = 1.f - u; return 3.f * m * m * u * ax + 3.f * m * u * u * bx + u * u * u; };
                auto dX = [ax, bx] (float u) { const float m = 1.f - u; return 3.f * m * m * ax + 6.f * m * u * (bx - ax) + 3.f * u * u * (1.f - bx); };
                float u = x;
                for (int i = 0; i < 6; ++i) { const float d = dX (u); if (std::abs (d) < 1.0e-5f) break; u = juce::jlimit (0.f, 1.f, u - (X (u) - x) / d); }
                const float m = 1.f - u;
                return 3.f * m * m * u * a.h1y + 3.f * m * u * u * a.h2y + u * u * u;
            }
            case Step: return x < 1.f ? 0.f : 1.f;
            default:   return x;
        }
    }

    struct Shape
    {
        std::array<Point, maxPoints> p {};
        int n = 2;

        // value at normalized time t; 'seg' is a cursor (kept per voice) so a per-sample walk is O(1)
        inline float eval (float t, int& seg) const
        {
            if (n <= 0) return 0.f;
            if (t <= p[0].t) { seg = 0; return p[0].v; }
            if (t >= p[(size_t) n - 1].t) { seg = n - 1; return p[(size_t) n - 1].v; }
            if (seg < 0 || seg >= n - 1 || p[(size_t) seg].t > t) seg = 0;
            while (seg < n - 2 && p[(size_t) seg + 1].t <= t) ++seg;
            const auto& a = p[(size_t) seg];
            const auto& b = p[(size_t) seg + 1];
            const float w = b.t - a.t;
            const float x = w > 1.0e-7f ? (t - a.t) / w : 1.f;
            const float f = segmentShape (a, x);
            const float v = a.v + (b.v - a.v) * f;
            return std::isfinite (v) ? v : 0.f;
        }
        float eval (float t) const { int s = 0; return eval (t, s); }

        // keeps the rules: 2..64 points, first at t = 0, times rising, values in range
        void sanitize (float vMin, float vMax)
        {
            n = juce::jlimit (2, maxPoints, n);
            p[0].t = 0.f;
            for (int i = 0; i < n; ++i)
            {
                auto& q = p[(size_t) i];
                q.t = juce::jlimit (i == 0 ? 0.f : p[(size_t) i - 1].t, 1.f, std::isfinite (q.t) ? q.t : 1.f);
                q.v = juce::jlimit (vMin, vMax, std::isfinite (q.v) ? q.v : 0.f);
                q.curve = (uint8_t) juce::jlimit (0, (int) NumCurves - 1, (int) q.curve);
                q.tension = juce::jlimit (-1.f, 1.f, std::isfinite (q.tension) ? q.tension : 0.f);
            }
        }

        // text form (preset / state): "n;t,v,curve,tension,h1x,h1y,h2x,h2y;..."
        juce::String toString() const
        {
            juce::String s (n);
            for (int i = 0; i < n; ++i)
            {
                const auto& q = p[(size_t) i];
                s << ";" << juce::String (q.t, 5) << "," << juce::String (q.v, 5) << "," << (int) q.curve << "," << juce::String (q.tension, 3)
                  << "," << juce::String (q.h1x, 3) << "," << juce::String (q.h1y, 3) << "," << juce::String (q.h2x, 3) << "," << juce::String (q.h2y, 3);
            }
            return s;
        }
        static bool fromString (const juce::String& s, Shape& out, float vMin, float vMax)
        {
            auto parts = juce::StringArray::fromTokens (s, ";", "");
            if (parts.size() < 3) return false;
            Shape r; r.n = juce::jlimit (2, maxPoints, parts[0].getIntValue());
            if (parts.size() < r.n + 1) return false;
            for (int i = 0; i < r.n; ++i)
            {
                auto f = juce::StringArray::fromTokens (parts[i + 1], ",", "");
                if (f.size() < 2) return false;
                auto& q = r.p[(size_t) i];
                q.t = f[0].getFloatValue(); q.v = f[1].getFloatValue();
                if (f.size() >= 4) { q.curve = (uint8_t) f[2].getIntValue(); q.tension = f[3].getFloatValue(); }
                if (f.size() >= 8) { q.h1x = f[4].getFloatValue(); q.h1y = f[5].getFloatValue(); q.h2x = f[6].getFloatValue(); q.h2y = f[7].getFloatValue(); }
            }
            r.sanitize (vMin, vMax);
            out = r;
            return true;
        }
        static Shape make (std::initializer_list<std::pair<float, float>> pts, uint8_t curve = Exponential)
        {
            Shape s; s.n = 0;
            for (auto& [t, v] : pts) if (s.n < maxPoints) { auto& q = s.p[(size_t) s.n++]; q.t = t; q.v = v; q.curve = curve; }
            return s;
        }
        bool operator== (const Shape& o) const
        {
            if (n != o.n) return false;
            for (int i = 0; i < n; ++i)
            {
                const auto& a = p[(size_t) i]; const auto& b = o.p[(size_t) i];
                if (a.t != b.t || a.v != b.v || a.curve != b.curve || a.tension != b.tension || a.h1x != b.h1x || a.h1y != b.h1y || a.h2x != b.h2x || a.h2y != b.h2y) return false;
            }
            return true;
        }
    };

    // ------------------------------------------------------------------ the drawn shapes of the whole kick
    //  pitch CUSTOM curve per part (-1..1) + the DRAWN envelopes AMP / FILTER / DRIVE / RESO per part (0..1)
    enum Slot : int
    {
        PitchPunch = 0, PitchBody, PitchTail, PitchSub,
        AmpPunch, AmpBody, AmpTail, AmpSub,
        FilterPunch, FilterBody, FilterTail,
        DrivePunch, DriveBody, DriveTail, DriveSub,
        ResoBody, ResoTail,
        NumSlots
    };
    inline bool isPitch (int slot) { return slot <= PitchSub; }
    inline const char* slotKey (int s)
    {
        static const char* k[NumSlots] { "env_pitch_punch", "env_pitch_body", "env_pitch_tail", "env_pitch_sub",
                                         "env_amp_punch", "env_amp_body", "env_amp_tail", "env_amp_sub",
                                         "env_filter_punch", "env_filter_body", "env_filter_tail",
                                         "env_drive_punch", "env_drive_body", "env_drive_tail", "env_drive_sub",
                                         "env_reso_body", "env_reso_tail" };
        return k[juce::jlimit (0, (int) NumSlots - 1, s)];
    }
    // the default drawn shape of every slot
    inline Shape defaultShape (int slot)
    {
        if (isPitch (slot)) return Shape::make ({ { 0.f, 1.f }, { 0.35f, 0.25f }, { 1.f, 0.f } });           // a drop, editable
        if (slot <= AmpSub) return Shape::make ({ { 0.f, 1.f }, { 0.75f, 0.85f }, { 1.f, 0.f } });           // body then release
        return Shape::make ({ { 0.f, 1.f }, { 0.3f, 0.35f }, { 1.f, 0.f } });                                // falls over the part
    }

    struct Bank { std::array<Shape, NumSlots> s; Bank() { for (int i = 0; i < NumSlots; ++i) s[(size_t) i] = defaultShape (i); } };

    // lock-free triple buffer: the message thread writes whole banks, the audio thread takes the newest at a block start
    class BankExchange
    {
    public:
        // message thread
        Bank& back() { return buf[(size_t) backIdx]; }
        void publish() { backIdx = mid.exchange (backIdx | dirty) & ~dirty; }
        // audio thread (start of a block)
        const Bank& front()
        {
            if (mid.load() & dirty) frontIdx = mid.exchange (frontIdx) & ~dirty;
            return buf[(size_t) frontIdx];
        }
    private:
        static constexpr int dirty = 4;
        std::array<Bank, 3> buf;
        std::atomic<int> mid { 1 };
        int backIdx = 0, frontIdx = 2;
    };
}
