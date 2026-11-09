#pragma once
// =====================================================================
//  4GE module 16 - the ANALYSIS page (GUI only; the analysis = DSP/Analysis/KickAnalysis.h, reference files / 4GE render /
//  suggestions = Presets/AnalysisTools.h, both on a background thread):
//   LOAD REFERENCE (or drop a WAV), ANALYZE 4GE (the current sound rendered like the export), NOTE.
//   WAVEFORM + PITCH CURVE + SPECTRUM overlays (reference silver, 4GE red), BAND bars, the METRICS table REF | 4GE | DIFF,
//   the FEATURES of both, SUGGESTIONS with check boxes -> APPLY SELECTED / APPLY ALL = ONE UNDO step. Nothing changes without APPLY.
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayFlat.h"
#include "../PluginProcessor.h"
#include "../Presets/AnalysisTools.h"

namespace fge::gui
{
    class AnalysisPage : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer, private juce::ListBoxModel
    {
    public:
        AnalysisPage (GeProcessor&, doomsday::flat::FlatLookAndFeel&);
        ~AnalysisPage() override;
        void paint (juce::Graphics&) override;
        void resized() override;
        bool isInterestedInFileDrag (const juce::StringArray&) override;
        void filesDropped (const juce::StringArray&, int, int) override;
        // actions (buttons / tests)
        void loadReference (const juce::File&);
        void analyseMine();
        void clearReference();
        bool busy() const { return jobs.load() > 0; }
        const fge::dsp::analysis::Report& refReport() const { return ref; }
        const fge::dsp::analysis::Report& mineReport() const { return mine; }
        const std::vector<fge::analysis::Suggestion>& suggestions() const { return sugg; }
        void setChosen (int i, bool on) { if (i >= 0 && i < (int) chosen.size()) chosen[(size_t) i] = on; list.repaint(); }
        int applySelected();
        int applyAll();
        void setNote (int midi);
        int note() const { return noteMidi; }
        juce::String statusText() const { return status; }
    private:
        void timerCallback() override;
        int getNumRows() override { return (int) sugg.size(); }
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
        void rebuildSuggestions();
        int applyRows (const std::vector<int>&);
        void paintWave (juce::Graphics&, juce::Rectangle<int>, float tMax);
        void paintPitch (juce::Graphics&, juce::Rectangle<int>, float tMax);
        void paintSpectrum (juce::Graphics&, juce::Rectangle<int>);
        void paintBands (juce::Graphics&, juce::Rectangle<int>);
        void paintTable (juce::Graphics&, juce::Rectangle<int>);
        void paintFeatures (juce::Graphics&, juce::Rectangle<int>);
        GeProcessor& proc;
        juce::TextButton loadBtn { "LOAD REFERENCE..." }, mineBtn { "ANALYZE 4GE" }, clearBtn { "CLEAR" }, applyBtn { "APPLY SELECTED" }, allBtn { "APPLY ALL" };
        juce::ComboBox noteBox;
        juce::ListBox list { "suggestions", this };
        std::unique_ptr<juce::FileChooser> chooser;
        fge::dsp::analysis::Report ref, mine;
        ParamFrame mineFrame;
        std::vector<fge::analysis::Suggestion> sugg;
        std::vector<bool> chosen;
        juce::String refName, status { "Load a reference kick (or drop a WAV here) and press ANALYZE 4GE. The analysis changes nothing until you press APPLY." };
        int noteMidi = 31;
        std::atomic<int> jobs { 0 };
        int reanalyseIn = 0;
        std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
        juce::Rectangle<int> barArea, waveArea, pitchArea, specArea, bandArea, tableArea, featArea, suggArea;
    };
}
