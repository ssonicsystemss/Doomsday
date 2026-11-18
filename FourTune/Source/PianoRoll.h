#pragma once
// 4TUNE - PIANO ROLL (spec 23): GUI only. Every edit goes to TuneProcessor::setMelodyNotes (state child MELODY, undoable).
//   double-click = add, drag = move (time + pitch), right edge = length, right-click = delete, drag on empty = select,
//   velocity lane at the bottom, keys: Delete, Ctrl+A / C / V / D, Up / Down (+Shift = octave), Q = quantize, Ctrl+Z / Y.
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include <set>

class PianoRoll : public juce::Component, private juce::Timer
{
public:
    explicit PianoRoll (TuneProcessor&);
    ~PianoRoll() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    // toolbar actions (on the selection, or on everything when nothing is selected)
    void quantizeSel();
    void humanizeSel();
    void transposeSel (int semis);
    void duplicateSel();
    void deleteSel();
    void clearAll();
    void copySel();
    void paste();
    void selectAll();
    void setGrid (double beats) { grid = beats; repaint(); }
    double gridBeats() const { return grid; }
    void reload();                                     // take the notes from the processor
    int numSelected() const { return (int) sel.size(); }

    std::function<juce::Colour()> leadColour, bounceColour, accent;

private:
    void timerCallback() override;
    juce::Rectangle<float> rollArea() const;
    juce::Rectangle<float> velArea() const;
    juce::Rectangle<float> noteRect (const tune::melody::Note&) const;
    float beatToX (double b) const;
    double xToBeat (float x) const;
    int yToPitch (float y) const;
    float pitchToY (int p) const;
    double snap (double b) const { return std::round (b / grid) * grid; }
    int hit (juce::Point<float>, bool& edge) const;
    void commit (bool newTransaction, const juce::String& name = "Edit melody");
    std::vector<int> targets() const;                  // the selection, or all notes

    TuneProcessor& proc;
    std::vector<tune::melody::Note> notes, dragStart;
    std::set<int> sel;
    static inline std::vector<tune::melody::Note> clipboard;
    int version = -1, lowPitch = 52, rows = 34;
    double grid = 0.25, lastLen = 0.5, lengthBeats = 16;
    enum Drag { None, Move, Resize, Marquee, Velocity } drag = None;
    juce::Point<float> downPos; juce::Rectangle<float> marquee;
    int anchor = -1;
};
