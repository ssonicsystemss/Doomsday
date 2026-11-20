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
            // owner 2026-11-19: "B (Michroma), ale musi być ta czcionka wszędzie" - one wide technical family for everything;
            // it is much wider than Barlow, so every font() is drawn a little smaller (sizeScale) to keep the layouts
            auto mi = Typeface::createSystemTypefaceFor (BinaryData::MichromaRegular_ttf, BinaryData::MichromaRegular_ttfSize);
            setTypefaces (mi, mi, mi);
            sizeScale = 1.06f;
            headTf = mi;
            pctTf = Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedSemiBold_ttf, BinaryData::BarlowSemiCondensedSemiBold_ttfSize);
        }
        juce::Typeface::Ptr pctTf;      // Michroma's "%" reads like "0/0" - every % is drawn in Barlow

        // a table cell (a "quiet" combo): the choice in producer shorthand, fitted - the open list keeps the full names
        static juce::String abbrevChoice (const juce::String& t)
        {
            static const std::pair<const char*, const char*> map[] {
                { "UNIPOLAR +", "UNI +" }, { "UNIPOLAR -", "UNI -" }, { "UNIPOLAR", "UNI" }, { "BIPOLAR", "BI" },
                { "EXPONENTIAL", "EXP" }, { "LOGARITHMIC", "LOG" }, { "LINEAR", "LIN" }, { "S-CURVE", "S-CRV" }, { "INVERTED", "INV" },
                { "MULTIPLY", "MULT" }, { "REPLACE", "REPL" }, { "TEXTURE", "TXT" }, { "SINGLE (GROUP)", "SINGLE" }, { "MULTI (SENDS)", "MULTI" },
                { "VELOCITY", "VEL" }, { "ENVELOPE", "ENV" }, { "RANDOM", "RND" }, { "FOLLOWER", "FOLLOW" }, { "KEYTRACK", "KEY" } };
            const auto up = t.toUpperCase();
            for (auto& [a, b] : map) if (up == a) return b;
            juce::StringArray w; w.addTokens (t, " ", "");
            for (auto& x : w) for (auto& [a, b] : map) if (x.toUpperCase() == a) { x = b; break; }
            return abbrev (w.joinIntoString (" "));
        }

        void drawLabel (juce::Graphics& g, juce::Label& l) override
        {
            using namespace juce;
            if (auto* cb = dynamic_cast<ComboBox*> (l.getParentComponent()); cb != nullptr && (bool) cb->getProperties()["quiet"] && ! l.isBeingEdited())
            {
                g.setFont (getLabelFont (l));
                g.setColour (l.findColour (Label::textColourId).withMultipliedAlpha (l.isEnabled() ? 1.f : 0.5f));
                g.drawFittedText (abbrevChoice (l.getText()), getLabelBorderSize (l).subtractedFrom (l.getLocalBounds()), l.getJustificationType(), 1, 0.8f);
                return;
            }
            const auto text = l.getText();
            if (l.isBeingEdited() || ! text.containsChar ('%')) { FlatLookAndFeel::drawLabel (g, l); return; }
            g.fillAll (l.findColour (Label::backgroundColourId));
            const Font f = getLabelFont (l);
            const Font pf (FontOptions (pctTf).withHeight (f.getHeight() * 1.08f));
            StringArray parts; parts.addTokens (text, "%", "");
            float w = 0.f;
            const float pw = GlyphArrangement::getStringWidth (pf, "%");
            for (int i = 0; i < parts.size(); ++i) { w += GlyphArrangement::getStringWidth (f, parts[i]); if (i < parts.size() - 1) w += pw; }
            auto area = getLabelBorderSize (l).subtractedFrom (l.getLocalBounds()).toFloat();
            const auto j = l.getJustificationType();
            float x = j.testFlags (Justification::left) ? area.getX() : j.testFlags (Justification::right) ? area.getRight() - w : area.getCentreX() - w * 0.5f;
            g.setColour (l.findColour (Label::textColourId).withMultipliedAlpha (l.isEnabled() ? 1.f : 0.5f));
            for (int i = 0; i < parts.size(); ++i)
            {
                g.setFont (f); const float pwid = GlyphArrangement::getStringWidth (f, parts[i]);
                g.drawText (parts[i], Rectangle<float> (x, area.getY(), pwid + 1.f, area.getHeight()), Justification::centredLeft, false); x += pwid;
                if (i < parts.size() - 1) { g.setFont (pf); g.drawText ("%", Rectangle<float> (x, area.getY(), pw + 1.f, area.getHeight()), Justification::centredLeft, false); x += pw; }
            }
        }
        juce::Typeface::Ptr headTf;
        juce::Font head (float h, float kerning = 0.02f) const { return juce::Font (juce::FontOptions (headTf).withHeight (h)).withExtraKerningFactor (kerning); }

        // owner 2026-11-19: "ważne, żeby były widoczne - możesz używać skrótów, ale takich, które są znane producentom"
        juce::String shortLabel (const juce::String& s) const override { return abbrev (s).toUpperCase(); }   // (Michroma reads best in capitals)
        juce::Colour knobLabelColour (bool hot) const override { return hot ? palette().silver : juce::Colour (0xffb4b8be); }
        static juce::String abbrev (const juce::String& s)
        {
            static const std::pair<const char*, const char*> map[] {
                { "LENGTH", "LEN" }, { "ATTACK", "ATK" }, { "DECAY", "DEC" }, { "SUSTAIN", "SUS" }, { "RELEASE", "REL" },
                { "RESONANCE", "RES" }, { "FREQUENCY", "FREQ" }, { "FEEDBACK", "FB" }, { "AMOUNT", "AMT" }, { "THRESHOLD", "THRESH" },
                { "DETUNE", "DET" }, { "VELOCITY", "VEL" }, { "OUTPUT", "OUT" }, { "INPUT", "IN" }, { "LEVEL", "LVL" },
                { "TRANSIENT", "TRANS" }, { "SMOOTH", "SMTH" }, { "DISTORTION", "DIST" }, { "SATURATION", "SAT" },
                { "MODULATION", "MOD" }, { "ENVELOPE", "ENV" }, { "FILTER", "FLT" }, { "SPREAD", "SPRD" }, { "PRE-DELAY", "PRE-DLY" },
                { "PREDELAY", "PRE-DLY" }, { "DELAY", "DLY" }, { "REVERB", "RVB" }, { "HARMONICS", "HARM" }, { "HARMONIC", "HARM" },
                { "CROSSOVER", "X-OVER" }, { "FUNDAMENTAL", "FUND" }, { "OSCILLATOR", "OSC" }, { "FREQ", "FREQ" }, { "SEMITONES", "ST" },
                { "TRACKING", "TRK" }, { "CHARACTER", "CHAR" }, { "INTENSITY", "INT" }, { "POSITION", "POS" }, 
                { "TENSION", "TENS" }, { "FREQUENCY", "FREQ" }, { "BALANCE", "BAL" }, { "SIDECHAIN", "SC" }, { "LOOKAHEAD", "LOOK" },
                { "PROBABILITY", "PROB" }, { "QUALITY", "QUAL" }, { "VOLUME", "VOL" } };
            juce::StringArray w; w.addTokens (s, " ", "");
            for (auto& x : w)
            {
                const auto up = x.toUpperCase();
                for (auto& [a, b] : map)
                    if (up == a) { x = x == up ? juce::String (b) : juce::String (b).substring (0, 1) + juce::String (b).substring (1).toLowerCase(); break; }
            }
            return w.joinIntoString (" ");
        }

        // buttons (GUI v2): a quiet outline, filled only when it is ON / selected, sentence-case text (short acronyms stay)
        static juce::String caseOf (const juce::String& t)
        {
            static const juce::StringArray keep { "LUFS", "MIDI", "TOK", "WAV", "FLAC", "AIFF", "A/B", "EQ", "FX", "+FX", "LFO", "DC", "MS", "M/S", "OK", "RND", "FFT" };
            juce::StringArray w; w.addTokens (t, " ", "");
            for (auto& x : w)
            {
                if (keep.contains (x) || x.length() <= 3 || x.containsAnyOf ("0123456789") || x != x.toUpperCase()) continue;
                x = x.substring (0, 1) + x.substring (1).toLowerCase();
            }
            return w.joinIntoString (" ");
        }
        // a page marks its selected tab by giving it a colour (buttonColourId): that tab = filled + a line in that colour
        static bool marked (const juce::Button& b, juce::Colour bg)
        {
            const auto& c = palette();
            return b.isColourSpecified (juce::TextButton::buttonColourId) && bg.isOpaque() && bg != c.control && bg != c.panel && bg != c.ground;
        }
        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool hover, bool down) override
        {
            const auto& c = palette();
            auto r = b.getLocalBounds().toFloat().reduced (0.5f);
            if ((bool) b.getProperties()["primary"]) { FlatLookAndFeel::drawButtonBackground (g, b, {}, hover, down); return; }
            if (marked (b, bg))
            {
                g.setColour (c.control.brighter (0.22f)); g.fillRoundedRectangle (r, 4.f);
                g.setColour (bg == c.silver ? c.red : bg); g.fillRect (r.getX() + 6.f, r.getBottom() - 2.5f, r.getWidth() - 12.f, 2.f);
                return;
            }
            const bool on = b.getToggleState();
            if ((bool) b.getProperties()["quiet"] && ! on && ! hover && ! down) return;      // (a table cell: no box until the mouse is on it)
            if (on || hover || down) { g.setColour (on ? c.control.brighter (0.22f) : c.control.brighter (down ? 0.15f : 0.06f)); g.fillRoundedRectangle (r, 4.f); }
            g.setColour (on ? c.line.brighter (0.6f) : c.line.brighter (0.15f)); g.drawRoundedRectangle (r, 4.f, 1.f);
        }
        void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool hover, bool) override
        {
            const auto& c = palette();
            if ((bool) b.getProperties()["primary"]) { FlatLookAndFeel::drawButtonText (g, b, hover, false); return; }
            g.setFont (font (13.5f, doomsday::flat::SemiBold));
            const bool sel = b.getToggleState() || marked (b, b.findColour (juce::TextButton::buttonColourId));
            g.setColour (! b.isEnabled() ? c.textDim.withAlpha (0.5f) : (sel || hover ? c.silver : c.silver.withAlpha (0.72f)));
            g.drawFittedText (caseOf (b.getButtonText()), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1, 0.85f);
        }
        void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
        {
            const auto& c = palette();
            auto r = juce::Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (0.5f);
            const bool quiet = (bool) box.getProperties()["quiet"], hov = box.isMouseOver (true);   // (a table cell: no box until the mouse is on it)
            if (! quiet || hov)
            {
                g.setColour (c.ground.darker (0.1f)); g.fillRoundedRectangle (r, 4.f);
                g.setColour (hov ? c.line.brighter (0.5f) : c.line.brighter (0.15f)); g.drawRoundedRectangle (r, 4.f, 1.f);
            }
            juce::Path chev; const float ax = (float) w - 14.f, ay = (float) h * 0.5f;
            chev.startNewSubPath (ax - 4.f, ay - 2.f); chev.lineTo (ax, ay + 2.f); chev.lineTo (ax + 4.f, ay - 2.f);
            g.setColour (c.textDim); g.strokePath (chev, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.5f, doomsday::flat::SemiBold); }

        // every plain font of the plugin (pages that ask for FontOptions (size, bold)) = Barlow, bold = Barlow Bold
        juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
        {
            if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
                return headTf;
            return FlatLookAndFeel::getTypefaceForFont (f);
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
