#pragma once
// RE4GE - REVERSE SHAPE DESIGNER view: the 4 drawn lanes (AMP / PITCH / HARMONIC / PHASE) on one step's time line.
// Click = a new point, drag = move, Alt / right-drag on a point = bend the segment after it, double-click = delete.
// Every edit goes through the processor (undoable); this view only draws and edits curves - no DSP.
#include "../PluginProcessor.h"
#include "DoomsdayFlat.h"

class ShapeDesigner : public juce::Component
{
public:
    explicit ShapeDesigner (ReProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { drag = -1; }
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent& e) override { const int h = pointAt (e.position); if (h != hover) { hover = h; repaint(); } }
    void tick();
    int lane = 0;
    std::function<void()> onLaneChanged;
    static juce::Colour laneColour (int);
private:
    struct Window { double start = 0.0, len = 1.0, ref = 1.0; };
    Window window (int lane) const;
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (16.f, 10.f).withTrimmedTop (34.f).withTrimmedBottom (18.f); }
    juce::Rectangle<float> tab (int i) const { return { 16.f + (float) i * 104.f, 8.f, 98.f, 26.f }; }
    juce::Point<float> toScreen (const doomsday::mod::Shape::Pt&, const Window&) const;
    doomsday::mod::Shape::Pt fromScreen (juce::Point<float>, const Window&) const;
    int pointAt (juce::Point<float>) const;
    ReProcessor& proc;
    doomsday::mod::Shape cur[4];
    int lastSerial = -1, drag = -1, hover = -1;
    bool bending = false;
    float bendStartY = 0.f, bendStart = 0.f;
    float lastSig = 0.f;
};
