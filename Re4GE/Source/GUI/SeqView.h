#pragma once
// RE4GE - SEQUENCER grid (16 steps): click = step ON / OFF, drag the bar = LEVEL, drag the lower cell = PITCH (+-12 st).
// Double-click a bar = 100 %, a pitch cell = 0. Steps past LENGTH are dimmed; the playing step is lit. GUI only (the parameters).
#include "../PluginProcessor.h"
#include "DoomsdayFlat.h"

class SeqView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SeqView (ReProcessor& p) : proc (p) { setTooltip ("Click the top = step on / off, drag the bar = level, drag the lower cell = pitch (double-click = reset)"); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void tick();
private:
    enum Zone { ZNone, ZOn, ZVel, ZPitch };
    struct Hit { int step = -1; Zone z = ZNone; };
    Hit hitAt (juce::Point<float>) const;
    juce::Rectangle<float> column (int i) const;
    juce::Rectangle<float> onArea (int i) const   { return column (i).removeFromTop (26.f); }
    juce::Rectangle<float> velArea (int i) const  { return column (i).withTrimmedTop (32.f).withTrimmedBottom (38.f); }
    juce::Rectangle<float> pitchArea (int i) const { return column (i).removeFromBottom (32.f); }
    void setNorm (const char* id, float v01);
    float value (const char* id) const;
    ReProcessor& proc;
    Hit drag;
    float dragStartPitch = 0.f, dragStartY = 0.f;
    juce::RangedAudioParameter* gesture = nullptr;
    int lastNow = -2;
};
