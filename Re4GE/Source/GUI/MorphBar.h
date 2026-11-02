#pragma once
// RE4GE - top bar: A / B sounds + COPY + MORPH (on + amount). Click A or B = edit that sound (an empty slot takes the current one),
// COPY = the edited sound into the other slot, MORPH ON = play between A and B with the bar (automatable in FL).
// GUI only: the slots live in the processor's state.
#include "../PluginProcessor.h"
#include "DoomsdayFlat.h"

class MorphBar : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MorphBar (ReProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void tick();                         // the knobs -> the edited slot (when MORPH is used), repaint on changes
private:
    // 0 = A, 1 = B, 2 = COPY, 3 = ON, 4 = bar
    std::array<juce::Rectangle<float>, 5> zones() const;
    int hit (juce::Point<float>) const;
    ReProcessor& proc;
    int hover = -1;
    bool dragBar = false;
    float lastAmt = -1.f, lastOn = -1.f; int lastActive = -1, syncCount = 0;
};
