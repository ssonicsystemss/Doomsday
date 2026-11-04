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
        std::array<juce::Colour, 6> part { juce::Colour (0xffe33135),     // PUNCH
                                           juce::Colour (0xffe6a23c),     // BODY
                                           juce::Colour (0xff9b70d5),     // TAIL
                                           juce::Colour (0xff36a9d6),     // SUB
                                           juce::Colour (0xffe65b9a),     // SCREECH
                                           juce::Colour (0xffa0a5ad) };   // TEXTURE
    };
    inline const Palette& palette() { static const Palette p; return p; }
}
