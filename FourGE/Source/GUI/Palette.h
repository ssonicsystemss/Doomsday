#pragma once
// 4GE colours: BLACK INDUSTRIAL / BLOOD RED (owner, Design/PALETTE.md) - 80 % black / graphite / steel, 15 % greys, 5 % strong colours
#include <juce_graphics/juce_graphics.h>
#include <array>

namespace fge::gui
{
    struct Palette
    {
        juce::Colour ground  { 0xff101113 };   // Obsidian Black - main background
        juce::Colour panel   { 0xff1b1d21 };   // Industrial Graphite - panels
        juce::Colour control { 0xff292c31 };   // Steel Grey - controls
        juce::Colour red     { 0xffe33135 };   // Blood Red - main accent
        juce::Colour hot     { 0xfff05a50 };   // Hot Red - active elements
        juce::Colour silver  { 0xffd5d7da };   // Cold Silver - text, markings
        juce::Colour textDim { 0xff8d9299 };
        juce::Colour grid    { 0xff23262b };
        // the part colour code (waveform, envelopes, time markers, layer editors)
        bool glass = false;                    // SMOKE (liquid glass, owner 2026-11-14): blurred glows behind translucent panels
        std::array<juce::Colour, 6> part { juce::Colour (0xffe33135),     // PUNCH
                                           juce::Colour (0xffe6a23c),     // BODY
                                           juce::Colour (0xff9b70d5),     // TAIL
                                           juce::Colour (0xff36a9d6),     // SUB
                                           juce::Colour (0xffe65b9a),     // SCREECH
                                           juce::Colour (0xffa0a5ad) };   // TEXTURE
    };
    inline Palette& paletteMut() { static Palette p; return p; }
    inline const Palette& palette() { return paletteMut(); }
    // THEME (module 17 settings): 0 FLAT (the original black industrial), 1 SMOKE (liquid glass, owner 2026-11-14 "dawaj tego smoke")
    inline void setTheme (int t)
    {
        auto& p = paletteMut();
        p = Palette();
        if (t == 1)
        {
            p.glass = true;
            p.ground  = juce::Colour (0x8c050506);    // translucent: the canvas glows show through the pages
            p.panel   = juce::Colour (0xa618181c);
            p.control = juce::Colour (0xd024252a);
            p.grid    = juce::Colour (0x1affffff);
            p.red     = juce::Colour (0xffff3b3f);
            p.hot     = juce::Colour (0xffff5a5a);
            p.silver  = juce::Colour (0xffe8e9ec);
            p.textDim = juce::Colour (0xff9aa0aa);
        }
    }
    // a glass panel: translucent fill, a hairline edge and a highlight along the top (SMOKE); a plain rounded fill (FLAT)
    inline void glassPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius = 6.f)
    {
        const auto& c = palette();
        if (! c.glass) { g.setColour (c.panel); g.fillRoundedRectangle (r, radius); return; }
        const float rr = juce::jmax (radius, 12.f);
        g.setColour (c.panel); g.fillRoundedRectangle (r, rr);
        g.setColour (juce::Colours::white.withAlpha (0.09f)); g.drawRoundedRectangle (r.reduced (0.5f), rr, 1.f);
        g.setColour (juce::Colours::white.withAlpha (0.16f)); g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + rr, r.getRight() - rr);
    }
    // the SMOKE background: near-black with soft red / grey glows (painted once by the canvas)
    inline void glassBackground (juce::Graphics& g, juce::Rectangle<float> a)
    {
        g.setColour (juce::Colour (0xff050506)); g.fillRect (a);
        auto glow = [&] (float cx, float cy, float rad, juce::Colour col)
        {
            juce::ColourGradient gr (col, cx, cy, col.withAlpha (0.f), cx + rad, cy, true);
            g.setGradientFill (gr); g.fillEllipse (cx - rad, cy - rad, 2.f * rad, 2.f * rad);
        };
        glow (a.getX() + a.getWidth() * 0.12f, a.getY() + a.getHeight() * 0.05f, a.getWidth() * 0.32f, juce::Colour (0x38ff2832));
        glow (a.getRight() - a.getWidth() * 0.08f, a.getY() + a.getHeight() * 0.62f, a.getWidth() * 0.34f, juce::Colour (0x2c9696a0));
        glow (a.getX() + a.getWidth() * 0.45f, a.getBottom(), a.getWidth() * 0.30f, juce::Colour (0x205a5a64));
    }
}
