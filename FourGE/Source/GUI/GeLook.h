#pragma once
// 4GE GUI v2 (owner 2026-11-19 "clean, profesjonalny, czytelny, nie nasrane", Design/GUI_V2.md):
// Barlow Semi Condensed everywhere, one knob drawing (track arc, value arc in the part colour, dark cap, light pointer),
// console faders. GUI only.
#include "DoomsdayFlat.h"
#include "BinaryData.h"
#include "Palette.h"

namespace fge::gui
{
    class GeLook : public doomsday::flat::FlatLookAndFeel
    {
    public:
        GeLook()
        {
            using namespace juce;
            setTypefaces (Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedMedium_ttf, BinaryData::BarlowSemiCondensedMedium_ttfSize),
                          Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedSemiBold_ttf, BinaryData::BarlowSemiCondensedSemiBold_ttfSize),
                          Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedBold_ttf, BinaryData::BarlowSemiCondensedBold_ttfSize));
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
        {
            using namespace juce;
            const auto& c = palette();
            const bool bipolar = (bool) s.getProperties()["bipolar"];
            const bool hover = s.isMouseOverOrDragging() && s.isEnabled();
            const Colour accent = s.getProperties().contains ("accent") ? Colour ((uint32) (int64) s.getProperties()["accent"]) : c.red;
            const auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
            const float size = jmin (b.getWidth(), b.getHeight());
            const float stroke = jlimit (2.5f, 4.f, size * 0.07f);
            const float r = size * 0.5f - stroke * 0.5f - 3.f;                 // room for the modulation ring
            const auto ctr = b.getCentre();
            const float angle = a0 + pos * (a1 - a0);
            const float alpha = s.isEnabled() ? 1.f : 0.35f;

            Path track; track.addCentredArc (ctr.x, ctr.y, r, r, 0.f, a0, a1, true);
            g.setColour (c.line.withMultipliedAlpha (alpha));
            g.strokePath (track, PathStrokeType (stroke, PathStrokeType::curved, PathStrokeType::rounded));
            const float from = bipolar ? (a0 + a1) * 0.5f : a0;
            if (std::abs (angle - from) > 0.01f)
            {
                Path arc; arc.addCentredArc (ctr.x, ctr.y, r, r, 0.f, jmin (from, angle), jmax (from, angle), true);
                g.setColour (accent.withMultipliedAlpha (alpha));
                g.strokePath (arc, PathStrokeType (stroke, PathStrokeType::curved, PathStrokeType::rounded));
            }
            // the cap
            const float rc = r - stroke * 0.5f - 2.f;
            g.setColour (hover ? c.control.brighter (0.12f) : c.control);
            g.fillEllipse (Rectangle<float> (2.f * rc, 2.f * rc).withCentre (ctr));
            g.setColour (c.line.brighter (0.25f).withMultipliedAlpha (alpha));
            g.drawEllipse (Rectangle<float> (2.f * rc, 2.f * rc).withCentre (ctr), 1.f);
            g.setColour (c.silver.withMultipliedAlpha (alpha));
            g.drawLine ({ ctr.getPointOnCircumference (rc * 0.3f, angle), ctr.getPointOnCircumference (rc - 2.f, angle) }, jlimit (1.6f, 2.6f, size * 0.045f));

            // modulation ring (Knob::setModulation)
            const float depth = (float) s.getProperties().getWithDefault ("modDepth", 0.f);
            if (std::abs (depth) > 0.001f)
            {
                const float rr = r + stroke * 0.5f + 3.f;
                const float e = jlimit (a0, a1, angle + depth * (a1 - a0));
                Path ring; ring.addCentredArc (ctr.x, ctr.y, rr, rr, 0.f, jmin (angle, e), jmax (angle, e), true);
                g.setColour (accent.withAlpha (0.6f));
                g.strokePath (ring, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
                const float now = (float) s.getProperties().getWithDefault ("modNow", -1.f);
                if (now >= 0.f)
                {
                    g.setColour (Colours::white);
                    g.fillEllipse (Rectangle<float> (4.f, 4.f).withCentre (ctr.getPointOnCircumference (rr, a0 + jlimit (0.f, 1.f, now) * (a1 - a0))));
                }
            }
        }

        // a console fader (vertical): the track, the level in the part colour, the cap
        void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle st, juce::Slider& s) override
        {
            using namespace juce;
            if (st == Slider::LinearHorizontal)                               // the LANES level: a thin track, the level, a small cap
            {
                const auto& c = palette();
                const Colour accent = s.getProperties().contains ("accent") ? Colour ((uint32) (int64) s.getProperties()["accent"]) : c.red;
                const float cy = (float) y + (float) h * 0.5f;
                g.setColour (c.ground.darker (0.4f)); g.fillRoundedRectangle ((float) x, cy - 1.5f, (float) w, 3.f, 1.5f);
                g.setColour (accent.withAlpha (0.85f)); g.fillRoundedRectangle ((float) x, cy - 1.5f, pos - (float) x, 3.f, 1.5f);
                const auto cap = Rectangle<float> (9.f, 16.f).withCentre ({ pos, cy });
                g.setColour (c.control.brighter (0.12f)); g.fillRoundedRectangle (cap, 2.f);
                g.setColour (c.line.brighter (0.45f)); g.drawRoundedRectangle (cap.reduced (0.5f), 2.f, 1.f);
                return;
            }
            if (st != Slider::LinearVertical) { FlatLookAndFeel::drawLinearSlider (g, x, y, w, h, pos, 0.f, 0.f, st, s); return; }
            const auto& c = palette();
            const Colour accent = s.getProperties().contains ("accent") ? Colour ((uint32) (int64) s.getProperties()["accent"]) : c.red;
            const float cx = (float) x + (float) w * 0.5f;
            const float top = (float) y + 2.f, bottom = (float) (y + h) - 2.f;
            g.setColour (c.ground.darker (0.4f)); g.fillRoundedRectangle (cx - 2.f, top, 4.f, bottom - top, 2.f);
            g.setColour (accent.withAlpha (s.isEnabled() ? 0.85f : 0.3f)); g.fillRoundedRectangle (cx - 2.f, pos, 4.f, bottom - pos, 2.f);
            const auto cap = Rectangle<float> (22.f, 30.f).withCentre ({ cx, pos });
            g.setColour (s.isMouseOverOrDragging() ? c.control.brighter (0.2f) : c.control.brighter (0.08f)); g.fillRoundedRectangle (cap, 3.f);
            g.setColour (c.line.brighter (0.45f)); g.drawRoundedRectangle (cap.reduced (0.5f), 3.f, 1.f);
            g.setColour (c.silver); g.drawLine (cap.getX() + 4.f, pos, cap.getRight() - 4.f, pos, 1.5f);
        }
        int getSliderThumbRadius (juce::Slider& s) override
        {
            return s.getSliderStyle() == juce::Slider::LinearVertical ? 15 : s.getSliderStyle() == juce::Slider::LinearHorizontal ? 5 : FlatLookAndFeel::getSliderThumbRadius (s);
        }
    };
}
