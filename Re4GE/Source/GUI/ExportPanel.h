#pragma once
// RE4GE - SYSTEM -> EXPORT page: LENGTH (beats) + NOTE, EXPORT WAV (rendered on a background thread), then the file can be
// DRAGGED straight into FL Studio (playlist / a sampler) or its folder opened. GUI only: the render + file = re4ge::Exporter.
#include "../PluginProcessor.h"
#include "DoomsdayFlat.h"

class ExportPanel : public juce::Component, public juce::SettableTooltipClient, public juce::DragAndDropContainer
{
public:
    explicit ExportPanel (ReProcessor& p) : proc (p) {}
    ~ExportPanel() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    // tests: export now and wait for the file
    juce::File exportNowForTest();
private:
    // zone codes: 100 + length chip, 200 + note chip (12), 300 + octave chip (0..2), 500 = EXPORT, 600 = drag the file, 700 = folder
    std::vector<std::pair<juce::Rectangle<float>, int>> layout() const;
    int hit (juce::Point<float>) const;
    int beats() const;
    int note() const;
    void start();
    ReProcessor& proc;
    int hover = -1;
    bool busy = false, dragging = false;
    juce::File last;
    juce::String status;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
};
