#pragma once
// 4GE HELP (owner 2026-11-20: "mały przycisk z ? i tam jakieś pomocowe rzeczy; napisz po angielsku, że to nie jest finalna wersja").
// An overlay over the whole window: the TEST BUILD notice + short help per area. GUI only. Esc / Close / a click outside closes it.
#include <juce_gui_basics/juce_gui_basics.h>
#include "GeLook.h"
#include "DoomsdayBrand.h"

namespace fge::gui
{
    class HelpPanel : public juce::Component
    {
    public:
        HelpPanel (GeLook& l, juce::String version) : lnf (l), ver (std::move (version))
        {
            close.setButtonText ("Close");
            close.onClick = [this] { setVisible (false); };
            addAndMakeVisible (close);
            setWantsKeyboardFocus (true);
        }
        void show() { setVisible (true); toFront (true); grabKeyboardFocus(); }

        static const std::vector<std::pair<const char*, const char*>>& sections()
        {
            static const std::vector<std::pair<const char*, const char*>> s {
                { "NAVIGATION", "Top row: Kick, Synthesis, Processing, Modulation, System. Row two: the pages of that block. "
                                "Ctrl + Left / Right: previous / next page." },
                { "MAIN", "Console: one strip per part of the kick (TOK, CLICK, PUNCH, BODY, TAIL, SUB, SCREECH). "
                          "Click a strip name to open every parameter of that part. Lanes: each part as a track with its envelope - click a lane to edit it." },
                { "KNOBS", "Drag to change, double-click to reset. Right-click: reset, type a value, copy / paste, modulation. "
                           "The bar at the bottom of the window tells what is under the mouse." },
                { "MODULATION", "Drag the small handle next to a macro onto any knob to modulate it. "
                                "Every connection: Modulation > Mod matrix." },
                { "STYLE AND LENGTH", "Style and Length are independent. Randomize stays in the chosen style; RND sets how far it goes. "
                                      "Kick end: LEN (the set length), NOTE (the piano-roll note cuts the kick), +FX." },
                { "FILES", "Drag the waveform into FL Studio to get the kick as a WAV. Drop a .4gp preset or a WAV onto the window. "
                           "Click the preset name to browse the presets." },
                { "SHORTCUTS", "Ctrl + Z: undo.   Ctrl + Y: redo.   Ctrl + Left / Right: page." } };
            return s;
        }

        void paint (juce::Graphics& g) override
        {
            using namespace juce;
            const auto& c = palette();
            g.fillAll (Colours::black.withAlpha (0.62f));
            const auto card = cardArea().toFloat();
            g.setColour (c.panel); g.fillRoundedRectangle (card, 6.f);
            paintWatermark (g, card);
            g.setColour (c.line.brighter (0.3f)); g.drawRoundedRectangle (card.reduced (0.5f), 6.f, 1.f);
            auto r = card.reduced (32.f, 26.f);
            g.setColour (c.silver); g.setFont (lnf.head (22.f));
            g.drawText ("4GE HELP", r.removeFromTop (34.f), Justification::centredLeft);
            // the TEST BUILD notice
            auto note = r.removeFromTop (64.f).withTrimmedTop (8.f);
            g.setColour (c.red.withAlpha (0.12f)); g.fillRoundedRectangle (note, 4.f);
            g.setColour (c.red); g.fillRect (note.getX(), note.getY(), 3.f, note.getHeight());
            g.setColour (c.silver); g.setFont (lnf.font (13.f, doomsday::flat::SemiBold));
            g.drawFittedText ("TEST BUILD - this is not the final version of 4GE. The sound, the presets and the layout can still change before the release.",
                              note.reduced (16.f, 6.f).toNearestInt(), Justification::centredLeft, 2, 1.f);
            r.removeFromTop (18.f);
            // two columns of short sections
            const auto& s = sections();
            const float colW = (r.getWidth() - 36.f) * 0.5f;
            float y[2] { r.getY(), r.getY() };
            for (size_t i = 0; i < s.size(); ++i)
            {
                const int col = y[0] <= y[1] ? 0 : 1;
                const float x = r.getX() + (float) col * (colW + 36.f);
                g.setColour (c.silver); g.setFont (lnf.head (11.5f));
                g.drawText (s[i].first, Rectangle<float> (x, y[col], colW, 18.f), Justification::centredLeft);
                AttributedString a; a.setJustification (Justification::topLeft); a.setLineSpacing (3.f);
                a.append (s[i].second, lnf.font (12.f, doomsday::flat::Medium), Colour (0xffb4b8be));
                TextLayout t; t.createLayout (a, colW);
                t.draw (g, Rectangle<float> (x, y[col] + 22.f, colW, t.getHeight()));
                y[col] += 22.f + t.getHeight() + 20.f;
            }
            g.setColour (c.textDim); g.setFont (lnf.font (11.f, doomsday::flat::Medium));
            g.drawText ("4GE " + ver + "   sonicsystems", card.reduced (32.f, 18.f).removeFromBottom (24.f), Justification::centredLeft);
        }
        void resized() override { const auto card = cardArea(); close.setBounds (card.getRight() - 32 - 96, card.getBottom() - 18 - 28, 96, 28); }
        void mouseDown (const juce::MouseEvent& e) override { if (! cardArea().contains (e.getPosition())) setVisible (false); }
        bool keyPressed (const juce::KeyPress& k) override { if (k == juce::KeyPress::escapeKey) { setVisible (false); return true; } return false; }

    private:
        // owner 2026-11-20: a watermark pattern of the sonicsystems logo, turned 45 degrees to the left, WITHOUT the DOOMSDAY line
        static juce::Image markOnly()
        {
            static juce::Image m = [] {
                auto full = doomsday::brand::logo();
                return full.isValid() ? full.getClippedImage ({ 0, 0, full.getWidth(), juce::roundToInt (full.getHeight() * 0.8f) }) : juce::Image();
            }();
            return m;
        }
        void paintWatermark (juce::Graphics& g, juce::Rectangle<float> card) const
        {
            using namespace juce;
            const auto img = markOnly();
            if (! img.isValid()) return;
            Graphics::ScopedSaveState save (g);
            Path clip; clip.addRoundedRectangle (card, 6.f); g.reduceClipRegion (clip);
            g.addTransform (AffineTransform::rotation (-MathConstants<float>::pi * 0.25f, card.getCentreX(), card.getCentreY()));
            g.setOpacity (0.045f);
            const float w = 132.f, h = w * (float) img.getHeight() / (float) img.getWidth();
            const float stepX = w + 70.f, stepY = h + 56.f;
            const float reach = std::hypot (card.getWidth(), card.getHeight()) * 0.5f + w;
            int row = 0;
            for (float y = card.getCentreY() - reach; y < card.getCentreY() + reach; y += stepY, ++row)
                for (float x = card.getCentreX() - reach + (row % 2 ? stepX * 0.5f : 0.f); x < card.getCentreX() + reach; x += stepX)
                    g.drawImage (img, Rectangle<float> (x, y, w, h), RectanglePlacement::stretchToFit);
        }
        juce::Rectangle<int> cardArea() const { return getLocalBounds().withSizeKeepingCentre (980, 640); }
        GeLook& lnf;
        juce::String ver;
        juce::TextButton close;
    };
}
