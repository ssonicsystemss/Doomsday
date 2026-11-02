#pragma once
// RE4GE - SYSTEM -> RANDOMIZER page: MODE / STYLE / AMOUNT / LOCKS (saved in the plugin state) + the big RANDOMIZE button.
// GUI only: the settings are written to the state, the click asks the processor (Randomizer) for a new sound.
#include "../PluginProcessor.h"
#include "DoomsdayFlat.h"

class RandomPanel : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit RandomPanel (ReProcessor& p) : proc (p) {}
    std::function<void()> onRandomized;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void clickRandomForTest() { fire(); }
private:
    // zone codes: 100 + mode, 200 + style, 300 + lock, 400 = amount bar, 500 = RANDOMIZE
    std::vector<std::pair<juce::Rectangle<float>, int>> layout() const;
    int hit (juce::Point<float>) const;
    void setAmountAt (float x);
    void fire();
    ReProcessor& proc;
    juce::Rectangle<float> amountBar;
    int hover = -1;
    bool hasLevel = false;
};
