#pragma once
// RE4GE - WAVESHAPER: the drawn CUSTOM distortion curves of stage 1 / 2 (x = input, y = output) with a live dot = the level
// going into the curve right now. Click = point, drag = move, Alt / right-drag = bend, double-click = delete (undoable).
// Edits go through the processor; no DSP here.
#include "../PluginProcessor.h"
#include "DoomsdayFlat.h"

class WaveshaperView : public juce::Component
{
public:
    explicit WaveshaperView (ReProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { drag = -1; }
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void tick();
    int stage = 0;                    // 0 / 1 -> shapes 4 / 5
    std::function<void()> onStageChanged;
private:
    juce::Rectangle<float> plot() const;
    juce::Rectangle<float> tab (int i) const { return { 16.f + (float) i * 104.f, 8.f, 98.f, 26.f }; }
    juce::Point<float> toScreen (const doomsday::mod::Shape::Pt&) const;
    doomsday::mod::Shape::Pt fromScreen (juce::Point<float>) const;
    int pointAt (juce::Point<float>) const;
    ReProcessor& proc;
    doomsday::mod::Shape cur[2];
    int lastSerial = -1, drag = -1;
    bool bending = false;
    float bendStartY = 0.f, bendStart = 0.f, dot[2] { 0.f, 0.f };
};
