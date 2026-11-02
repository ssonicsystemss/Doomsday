#pragma once
// RE4GE - POST EQ graph: 8 points on a log frequency scale (20 Hz - 20 kHz, +-18 dB), the summed curve (the same maths as the sound).
// Drag a point = FREQ / GAIN (it switches the band on), wheel = Q, double-click = band on / off, right-click = type menu.
// GUI only: the parameters (gestures for the host).
#include "../PluginProcessor.h"
#include "../DSP/Eq8.h"
#include "DoomsdayFlat.h"

class EqView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit EqView (ReProcessor& p) : proc (p) { setTooltip ("Drag a point = frequency / gain (the band turns on), wheel = Q, double-click = on / off, right-click = type"); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void tick();
private:
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (44.f, 16.f).withTrimmedBottom (12.f); }
    float xOf (double hz) const;
    double hzOf (float x) const;
    float yOf (float db) const;
    float dbOf (float y) const;
    juce::Point<float> node (int b) const;
    int bandAt (juce::Point<float>) const;
    float val (int b, const char* what) const;
    void set (int b, const char* what, float v);
    bool hasGain (int b) const;
    ReProcessor& proc;
    int hover = -1, dragBand = -1;
    std::array<float, 40> last {};
};
