#pragma once
// RE4GE - KICK-TO-REVERSE view: the loaded WAV's waveform with the detected TRANSIENT / PUNCH / BODY / TAIL sections,
// the region that plays (START .. END, drag the white handles) and the detected ROOT. Draws only; analysis = DSP/KickAnalysis.h.
#include "../PluginProcessor.h"
#include "DoomsdayFlat.h"

class KickView : public juce::Component
{
public:
    explicit KickView (ReProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { drag = -1; }
    void tick();
private:
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (16.f, 10.f).withTrimmedTop (30.f).withTrimmedBottom (34.f); }
    double viewMs() const;
    float xOf (double ms) const { const auto r = plot(); return r.getX() + r.getWidth() * (float) (ms / viewMs()); }
    double msOf (float x) const { const auto r = plot(); return (x - r.getX()) / r.getWidth() * viewMs(); }
    float param (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void setParam (const char* id, float v);
    ReProcessor& proc;
    std::vector<float> lo, hi;
    int lastSerial = -1, drag = -1;
    float lastSig = 0.f;
};
