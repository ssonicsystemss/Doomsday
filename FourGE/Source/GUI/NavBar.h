#pragma once
// =====================================================================
//  4GE module 17 - the MAIN NAVIGATION (left side): 5 blocks, every page, the module numbers (all 17 modules have a place),
//  a dot when that part of the instrument changes the sound. GUI only (what is "active" comes from the canvas).
//   01 KICK        MAIN (01 13 14) / ENVELOPES (02)
//   02 SYNTHESIS   OSCILLATORS (03) / LAYERS (04: noise, transient, layers) / SAMPLES (10)
//   03 PROCESSING  DISTORTION (05) / SCREECH (06: filters, resonators, screech) / EQ (07: EQ, dynamic EQ, multiband, spectral) / DYNAMICS (08) / STEREO (11)
//   04 MODULATION  MOD MATRIX (09: matrix, LFO, SEQ, RND, follower, MIDI CC, the macro mapping)
//   05 SYSTEM      PRESETS (15) / ANALYZE (16) / EXPORT (12) / SETTINGS (17)
// =====================================================================
#include <juce_gui_basics/juce_gui_basics.h>
#include "Palette.h"

namespace fge::gui
{
    class NavBar : public juce::Component, public juce::TooltipClient
    {
    public:
        struct Item { int page; const char* name; const char* modules; const char* tip; };
        struct Block { const char* name; std::vector<Item> items; };
        static const std::vector<Block>& blocks()
        {
            static const std::vector<Block> b {
                { "KICK", { { 0, "MAIN", "01 13 14", "The kick: pitch engine, the central waveform, the four parts (PUNCH / BODY / TAIL / SUB), sources and the core mixer." },
                            { 1, "ENVELOPES", "02", "Draw the pitch / amplitude / filter / distortion / resonance envelope of every part." },
                            { 15, "VARIATION", "09", "KICK VARIATION: the kick's DNA, MUTATE into new versions, MORPH between them, HUMANIZE." } } },
                { "SYNTHESIS", { { 2, "OSCILLATORS", "03", "Four extra oscillators (waveforms, wavetables, harmonics, unison, FM, ring, sync) routed into the parts." },
                                 { 3, "LAYERS", "04", "Noise generators, the transient engine and 8 layers with groups and routing." },
                                 { 9, "SAMPLES", "10", "8 sample slots (WAV / AIFF / FLAC): the waveform editor, loop / fades / reverse, the browser - each slot is one more layer." } } },
                { "PROCESSING", { { 4, "DISTORTION", "05", "16 distortion stages: serial / parallel / multiband chains on the whole kick, a part, the texture or a layer." },
                                  { 5, "SCREECH", "06", "Filters, resonators and the SCREECH engine (synthesised screeches with their own envelopes and timing)." },
                                  { 6, "EQ", "07", "EQ (16 bands), dynamic EQ, multiband, spectral shaper - on the kick, a part, the texture, a layer or the screech." },
                                  { 7, "DYNAMICS", "08", "Compressors, transient shapers, clippers, limiters, crest control - where the punch and the loudness are made." },
                                  { 10, "STEREO", "11", "PAN / WIDTH / M-S / low mono / polarity per group and on the master, SUB MONO LOCK, vector scope, mono audition." } } },
                { "MODULATION", { { 8, "MOD MATRIX", "09", "64 modulation slots, LFO 1-8, step sequencer, random, follower, MIDI CC and what the 8 MASTER MACROS move." } } },
                { "SYSTEM", { { 12, "PRESETS", "15", "The preset browser: styles, lengths, tags, favourites, recent, A / B, preview." },
                              { 13, "ANALYZE", "16", "Compare 4GE with a reference kick: waveform, pitch, spectrum, bands, loudness, stereo - and apply the suggestions." },
                              { 11, "EXPORT", "12", "Master output, meters and EXPORT to WAV / AIFF / FLAC (groups, layers, lengths, batch, history)." },
                              { 14, "SETTINGS", "17", "Interface: size, tooltips, the info line, the start page, shortcuts." } } } };
            return b;
        }
        static juce::String pageName (int page)
        {
            for (auto& b : blocks()) for (auto& i : b.items) if (i.page == page) return i.name;
            return {};
        }
        static juce::String blockOf (int page)
        {
            int n = 1;
            for (auto& b : blocks()) { for (auto& i : b.items) if (i.page == page) return juce::String (n).paddedLeft ('0', 2) + "  " + b.name; ++n; }
            return {};
        }
        static juce::String pageTip (int page)
        {
            for (auto& b : blocks()) for (auto& i : b.items) if (i.page == page) return i.tip;
            return {};
        }
        // the page order of the navigation (Ctrl + left / right walks it)
        static std::vector<int> order() { std::vector<int> o; for (auto& b : blocks()) for (auto& i : b.items) o.push_back (i.page); return o; }

        std::function<void (int page)> onSelect;
        std::function<bool (int page)> isActive;            // a dot: this page changes the sound now (optional)

        void setCurrent (int p) { if (p != current) { current = p; lastIn[(size_t) blockOfPage (p)] = p; repaint(); } }
        int getCurrent() const { return current; }
        void refresh()                                       // (timer) repaint when an active dot changed
        {
            if (! isActive) return;
            std::vector<bool> now; for (auto p : order()) now.push_back (isActive (p));
            if (now != lastOn) { lastOn = now; repaint(); }
        }
        // GUI v2 (owner 2026-11-19, Design/GUI_V2.md): the navigation runs along the top - row 1 = the 5 blocks,
        // row 2 = the pages of the current block (a click on a block opens the page you had there last)
        std::function<juce::Font (float height, bool bold)> font;
        std::function<juce::Font (float height)> headFont;               // row 1 (the blocks): the wide heading font
        juce::Font hf (float h) const { return headFont ? headFont (h) : f (h, true); }
        static constexpr int rowH = 52, row2H = 36;
        static juce::String display (const juce::String& n)
        {
            if (n == "EQ") return n;
            return n.substring (0, 1) + n.substring (1).toLowerCase();
        }
        juce::Font f (float h, bool bold) const { return font ? font (h, bold) : juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
        int blockOfPage (int page) const
        {
            int n = 0;
            for (auto& b : blocks()) { for (auto& i : b.items) if (i.page == page) return n; ++n; }
            return 0;
        }
        juce::Rectangle<int> blockBounds (int block) const
        {
            int x = 0, n = 0;
            for (auto& b : blocks())
            {
                const int w = juce::GlyphArrangement::getStringWidthInt (hf (12.f), display (b.name)) + 30;
                if (n == block) return { x, 0, w, rowH };
                x += w; ++n;
            }
            return {};
        }
        juce::Rectangle<int> itemBounds (int page) const                // (row 2 of the page's own block)
        {
            const auto& b = blocks()[(size_t) blockOfPage (page)];
            int x = 0;
            for (auto& i : b.items)
            {
                const int w = juce::GlyphArrangement::getStringWidthInt (f (13.5f, false), display (i.name)) + 34;
                if (i.page == page) return { x, rowH, w, row2H };
                x += w;
            }
            return {};
        }
        int pageAt (juce::Point<int> p) const
        {
            if (p.y < rowH)
            {
                for (int n = 0; n < (int) blocks().size(); ++n)
                    if (blockBounds (n).contains (p))
                    {
                        const auto& items = blocks()[(size_t) n].items;
                        for (auto& i : items) if (i.page == lastIn[(size_t) n]) return i.page;
                        return items.front().page;
                    }
                return -1;
            }
            for (auto& i : blocks()[(size_t) blockOfPage (current)].items) if (itemBounds (i.page).contains (p)) return i.page;
            return -1;
        }
        void paint (juce::Graphics& g) override
        {
            const auto& c = palette();
            const int cur = blockOfPage (current);
            for (int n = 0; n < (int) blocks().size(); ++n)
            {
                const auto r = blockBounds (n);
                const bool here = n == cur, hov = hover >= 0 && hover < 100 && pageAt (getMouseXYRelative()) >= 0 && r.contains (getMouseXYRelative());
                g.setColour (here ? c.silver : (hov ? c.silver.withAlpha (0.85f) : c.textDim));
                g.setFont (hf (12.f));
                g.drawText (display (blocks()[(size_t) n].name), r, juce::Justification::centred);
                if (here) { g.setColour (c.red); g.fillRect (r.getX() + 12, r.getBottom() - 2, r.getWidth() - 24, 2); }
            }
            g.setColour (c.line); g.drawHorizontalLine (rowH, 0.f, (float) getWidth());
            for (auto& i : blocks()[(size_t) cur].items)
            {
                const auto r = itemBounds (i.page);
                const bool sel = i.page == current, hov = i.page == hover;
                g.setColour (sel ? c.silver : (hov ? c.silver.withAlpha (0.85f) : c.textDim));
                g.setFont (f (13.5f, sel));
                g.drawText (display (i.name), r, juce::Justification::centred);
                if (sel) { g.setColour (c.silver.withAlpha (0.6f)); g.fillRect (r.getX() + 14, r.getBottom() - 6, r.getWidth() - 28, 1); }
                if (isActive && isActive (i.page))
                { g.setColour (c.hot); g.fillEllipse ((float) r.getRight() - 12.f, (float) r.getY() + 10.f, 4.f, 4.f); }
            }
        }
        void mouseMove (const juce::MouseEvent& e) override { const int h = pageAt (e.getPosition()); if (h != hover) { hover = h; repaint(); } setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor); }
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override { const int p = pageAt (e.getPosition()); if (p >= 0 && onSelect) onSelect (p); }
        juce::String getTooltip() override
        {
            const auto p = getMouseXYRelative();
            const int pg = p.y >= rowH ? pageAt (p) : -1;
            return pg >= 0 ? pageTip (pg) : juce::String();
        }
        int contentWidth() const { return blockBounds ((int) blocks().size() - 1).getRight(); }
    private:
        int current = 0, hover = -1;
        std::array<int, 8> lastIn { -1, -1, -1, -1, -1, -1, -1, -1 };
        std::vector<bool> lastOn;
    };

    // ---- a drop-down choice (STYLE / LENGTH in the top bar; owner 2026-11-12: "a drop-down list instead of 10 buttons"):
    //      shows the current item, a click opens the list (the current one ticked), a right-click opens onMenu.
    //      Items have their own ids (the list order may differ from the ids: STYLE shows EARLY, EUPHORIC, MODERN ...).
    class ChoiceDrop : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        ChoiceDrop (juce::StringArray l, std::vector<int> itemIds = {}) : labels (std::move (l)), ids (std::move (itemIds))
        { if (ids.empty()) for (int i = 0; i < labels.size(); ++i) ids.push_back (i); }
        std::function<void (int id)> onPick;
        std::function<void (int id)> onMenu;                   // right-click (the id shown now)
        std::function<juce::String (int id)> tipFor;
        juce::String emptyText { "-" };                        // shown when nothing is lit (e.g. LENGTH SYNC)
        void setLit (int id) { if (id != lit) { lit = id; if (tipFor) setTooltip (lit >= 0 ? tipFor (lit) : juce::String()); repaint(); } }
        int getLit() const { return lit; }
        int count() const { return labels.size(); }
        const std::vector<int>& itemIds() const { return ids; }
        juce::String labelOf (int id) const { for (size_t i = 0; i < ids.size(); ++i) if (ids[i] == id) return labels[(int) i]; return emptyText; }
        void paint (juce::Graphics& g) override
        {
            const auto& c = palette();
            auto r = getLocalBounds().toFloat();
            g.setColour (hover ? c.control.brighter (0.08f) : c.control); g.fillRoundedRectangle (r, 4.f);
            g.setColour (c.red); g.fillRect (r.getX() + 1.f, r.getBottom() - 3.f, r.getWidth() - 2.f, 2.f);
            g.setColour (c.silver); g.setFont (juce::FontOptions (12.f, juce::Font::bold));
            g.drawText (labelOf (lit), getLocalBounds().withTrimmedLeft (12).withTrimmedRight (24), juce::Justification::centredLeft, true);
            juce::Path a; const float x = r.getRight() - 16.f, y = r.getCentreY() - 2.f;
            a.addTriangle (x - 5.f, y, x + 5.f, y, x, y + 5.f);
            g.setColour (c.silver.withAlpha (0.7f)); g.fillPath (a);
        }
        void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu()) { if (onMenu) onMenu (lit); return; }
            juce::PopupMenu m;
            for (size_t i = 0; i < ids.size(); ++i)
            {
                juce::PopupMenu::Item it (labels[(int) i]); it.itemID = (int) i + 1; it.isTicked = ids[i] == lit;
                m.addItem (it);
            }
            juce::Component::SafePointer<ChoiceDrop> self (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()), [self] (int r)
            {
                if (self != nullptr && r > 0 && self->onPick) self->onPick (self->ids[(size_t) (r - 1)]);
            });
        }
    private:
        juce::StringArray labels;
        std::vector<int> ids;
        int lit = -1;
        bool hover = false;
    };

    // ---- a row of choice buttons (STYLE / LENGTH in the top bar): one is lit, a click picks, a right-click opens a menu
    class ChoiceStrip : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit ChoiceStrip (juce::StringArray l) : labels (std::move (l)) {}
        std::function<void (int)> onPick;
        std::function<void (int)> onMenu;                     // right-click on item i
        std::function<juce::String (int)> tipFor;
        void setLit (int i) { if (i != lit) { lit = i; repaint(); } }
        int getLit() const { return lit; }
        int count() const { return labels.size(); }
        juce::Rectangle<int> itemBounds (int i) const { const int w = getWidth() / juce::jmax (1, labels.size()); return { i * w, 0, i == labels.size() - 1 ? getWidth() - i * w : w, getHeight() }; }
        void paint (juce::Graphics& g) override
        {
            const auto& c = palette();
            g.setColour (c.control); g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.f);
            for (int i = 0; i < labels.size(); ++i)
            {
                auto r = itemBounds (i);
                if (i == lit) { g.setColour (c.red.withAlpha (0.22f)); g.fillRect (r.reduced (1)); g.setColour (c.red); g.fillRect (r.getX() + 1, r.getBottom() - 3, r.getWidth() - 2, 2); }
                else if (i == hover) { g.setColour (c.panel.brighter (0.1f)); g.fillRect (r.reduced (1)); }
                if (i > 0) { g.setColour (c.ground); g.drawVerticalLine (r.getX(), 3.f, (float) getHeight() - 3.f); }
                g.setColour (i == lit ? c.silver : c.silver.withAlpha (0.6f)); g.setFont (juce::FontOptions (11.f, juce::Font::bold));
                g.drawText (labels[i], r, juce::Justification::centred, true);
            }
        }
        int itemAt (juce::Point<int> p) const { for (int i = 0; i < labels.size(); ++i) if (itemBounds (i).contains (p)) return i; return -1; }
        void mouseMove (const juce::MouseEvent& e) override
        {
            const int h = itemAt (e.getPosition()); if (h != hover) { hover = h; repaint(); }
            if (tipFor && h >= 0) setTooltip (tipFor (h));
        }
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            const int i = itemAt (e.getPosition()); if (i < 0) return;
            if (e.mods.isPopupMenu()) { if (onMenu) onMenu (i); }
            else if (onPick) onPick (i);
        }
        int hoverIndex() const { return hover; }
    private:
        juce::StringArray labels;
        int lit = -1, hover = -1;
    };
}
