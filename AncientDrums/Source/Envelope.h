#pragma once
// Volume envelope of one layer, edited like an automation clip.
// Points are (time in ms from the layer start, gain 0..1), linear between points.
#include <juce_core/juce_core.h>
#include <vector>
#include <memory>
#include <algorithm>

struct Envelope
{
    struct Point { float ms, gain; };
    std::vector<Point> points;

    static Envelope make (float lengthMs, float fadeMs = -1.f)
    {
        if (fadeMs < 0.f) fadeMs = std::min (30.f, lengthMs * 0.3f);
        Envelope e;
        e.points = { { 0.f, 1.f }, { std::max (1.f, lengthMs - fadeMs), 1.f }, { lengthMs, 0.f } };
        return e;
    }

    float endMs() const { return points.empty() ? 0.f : points.back().ms; }
    bool endsSilent() const { return ! points.empty() && points.back().gain <= 0.0001f; }

    // seg is a per-voice cursor so evaluation is O(1) while time moves forward
    float eval (float ms, size_t& seg) const
    {
        if (points.empty()) return 1.f;
        if (ms <= points.front().ms) return points.front().gain;
        while (seg + 1 < points.size() && ms > points[seg + 1].ms) ++seg;
        if (seg + 1 >= points.size()) return points.back().gain;
        const auto& a = points[seg];
        const auto& b = points[seg + 1];
        const float span = b.ms - a.ms;
        return span <= 0.f ? b.gain : a.gain + (b.gain - a.gain) * (ms - a.ms) / span;
    }

    float evalAt (float ms) const { size_t s = 0; return eval (ms, s); }

    void sortAndClamp()
    {
        for (auto& p : points) { p.ms = std::max (0.f, p.ms); p.gain = juce::jlimit (0.f, 1.f, p.gain); }
        std::stable_sort (points.begin(), points.end(), [] (const Point& a, const Point& b) { return a.ms < b.ms; });
        if (! points.empty()) points.front().ms = 0.f;      // the first point is pinned to the layer start
    }

    juce::String toString() const
    {
        juce::StringArray s;
        for (auto& p : points) s.add (juce::String (p.ms, 2) + ":" + juce::String (p.gain, 4));
        return s.joinIntoString (";");
    }

    static bool fromString (const juce::String& text, Envelope& out)
    {
        Envelope e;
        for (auto& tok : juce::StringArray::fromTokens (text, ";", ""))
        {
            if (! tok.containsChar (':')) continue;
            e.points.push_back ({ tok.upToFirstOccurrenceOf (":", false, false).getFloatValue(),
                                  tok.fromFirstOccurrenceOf (":", false, false).getFloatValue() });
        }
        if (e.points.size() < 2) return false;
        e.sortAndClamp();
        out = std::move (e);
        return true;
    }
};
using EnvPtr = std::shared_ptr<const Envelope>;
