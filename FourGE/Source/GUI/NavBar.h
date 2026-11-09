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
                            { 1, "ENVELOPES", "02", "Draw the pitch / amplitude / filter / distortion / resonance envelope of every part." } } },
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

        void setCurrent (int p) { if (p != current) { current = p; repaint(); } }
        int getCurrent() const { return current; }
        void refresh()                                       // (timer) repaint when an active dot changed
        {
            if (! isActive) return;
            std::vector<bool> now; for (auto p : order()) now.push_back (isActive (p));
            if (now != lastOn) { lastOn = now; repaint(); }
        }
        juce::Rectangle<int> itemBounds (int page) const
        {
            int y = 0;
            for (auto& b : blocks())
            {
                y += headH;
                for (auto& i : b.items) { if (i.page == page) return { 0, y, getWidth(), itemH }; y += itemH; }
                y += gap;
            }
            return {};
        }
        int pageAt (juce::Point<int> p) const
        {
            for (auto pg : order()) if (itemBounds (pg).contains (p)) return pg;
            return -1;
        }
        void paint (juce::Graphics& g) override
        {
            const auto& c = palette();
            int y = 0, n = 1;
            for (auto& b : blocks())
            {
                bool here = false; for (auto& i : b.items) here |= i.page == current;
                auto head = juce::Rectangle<int> (0, y, getWidth(), headH - 4);
                g.setColour (here ? c.panel.brighter (0.06f) : c.panel); g.fillRoundedRectangle (head.toFloat(), 4.f);
                if (here) { g.setColour (c.red); g.fillRect (head.getX(), head.getY(), 3, head.getHeight()); }
                g.setColour (here ? c.red : c.textDim); g.setFont (juce::FontOptions (10.f, juce::Font::bold));
                g.drawText (juce::String (n).paddedLeft ('0', 2), head.withTrimmedLeft (10).withWidth (22), juce::Justification::centredLeft);
                g.setColour (here ? c.silver : c.silver.withAlpha (0.7f)); g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
                g.drawText (b.name, head.withTrimmedLeft (36), juce::Justification::centredLeft);
                y += headH;
                for (auto& i : b.items)
                {
                    auto r = juce::Rectangle<int> (0, y, getWidth(), itemH);
                    const bool sel = i.page == current, hov = i.page == hover;
                    if (sel) { g.setColour (c.red.withAlpha (0.16f)); g.fillRect (r.withTrimmedLeft (14)); g.setColour (c.red); g.fillRect (r.getX() + 14, r.getY() + 3, 2, r.getHeight() - 6); }
                    else if (hov) { g.setColour (c.control.withAlpha (0.6f)); g.fillRect (r.withTrimmedLeft (14)); }
                    g.setColour (sel ? c.silver : (hov ? c.silver.withAlpha (0.9f) : c.silver.withAlpha (0.62f)));
                    g.setFont (juce::FontOptions (12.f, sel ? juce::Font::bold : juce::Font::plain));
                    g.drawText (i.name, r.withTrimmedLeft (26).withTrimmedRight (52), juce::Justification::centredLeft, true);
                    g.setColour (c.textDim.withAlpha (sel ? 0.9f : 0.55f)); g.setFont (juce::FontOptions (9.f));
                    g.drawText (i.modules, r.withTrimmedRight (18).withTrimmedLeft (r.getWidth() - 70), juce::Justification::centredRight);
                    if (isActive && isActive (i.page))
                    { g.setColour (c.hot); g.fillEllipse ((float) r.getRight() - 12.f, (float) r.getCentreY() - 2.5f, 5.f, 5.f); }
                    y += itemH;
                }
                y += gap; ++n;
            }
        }
        void mouseMove (const juce::MouseEvent& e) override { const int h = pageAt (e.getPosition()); if (h != hover) { hover = h; repaint(); } setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor); }
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override { const int p = pageAt (e.getPosition()); if (p >= 0 && onSelect) onSelect (p); }
        juce::String getTooltip() override
        {
            const auto p = getMouseXYRelative();
            const int pg = pageAt (p);
            return pg >= 0 ? pageTip (pg) : juce::String();
        }
        static constexpr int headH = 32, itemH = 27, gap = 10;
        int contentHeight() const { int h = 0; for (auto& b : blocks()) h += headH + (int) b.items.size() * itemH + gap; return h; }
    private:
        int current = 0, hover = -1;
        std::vector<bool> lastOn;
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
