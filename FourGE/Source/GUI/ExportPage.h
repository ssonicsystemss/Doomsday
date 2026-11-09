#pragma once
// =====================================================================
//  4GE module 12 - the EXPORT page (GUI only; render = DSP/Export, files / history / batch = Presets/Export.h):
//   SOURCE, LENGTH, FORMAT, QUALITY, NORMALIZATION, SEED, DESTINATION (folder, name template), BATCH, RENDER (PREVIEW / EXPORT /
//   BATCH EXPORT / CANCEL, progress, the result - drag it into FL), EXPORT HISTORY (RE-EXPORT);
//   MASTER OUTPUT (level, mute, bypass, mono / stereo, SAFETY CLIP / LIMITER with a clear warning) and the MASTER METERS
//   (peak L / R + hold + clip, max peak, TRUE PEAK, RMS, LUFS M / S / I, DC offset, RESET).
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayFlat.h"
#include "../PluginProcessor.h"

namespace fge::gui
{
    class ExportPage : public juce::Component, private juce::Timer, private juce::ListBoxModel
    {
    public:
        ExportPage (GeProcessor&, doomsday::flat::FlatLookAndFeel&);
        ~ExportPage() override;
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDrag (const juce::MouseEvent&) override;
        // tests / GUI actions
        void exportNow (bool batch);
        void previewNow();
        void cancel() { proc.exporter.cancel(); }
        bool busy() const { return proc.exporter.busy() || previewing.load(); }
        juce::String resultText() const { return result; }
        juce::File lastFile() const { return lastOut; }
        juce::Component* findControl (const juce::String& id);
        void loadSettings();                                         // settings -> controls
        void storeSettings();                                        // controls -> settings (state child EXPORT)
        int historyRows() const { return (int) hist.size(); }
        void reExport (int row);
    private:
        void timerCallback() override;
        int getNumRows() override { return (int) hist.size(); }
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void showResults (const std::vector<fge::xp::JobResult>&);
        juce::ComboBox* addCombo (const juce::String& name, const juce::StringArray& items, const juce::String& tip);
        juce::Slider* addSlider (const juce::String& name, double lo, double hi, double step, const juce::String& suffix, const juce::String& tip);
        juce::ToggleButton* addToggle (const juce::String& name, const juce::String& tip);
        GeProcessor& proc;
        doomsday::flat::FlatLookAndFeel& lnf;
        std::map<juce::String, juce::Component*> named;
        std::vector<std::unique_ptr<juce::Component>> own;
        std::map<juce::Component*, juce::String> captions;
        juce::TextEditor nameTmpl;
        juce::TextButton folderBtn { "FOLDER..." }, previewBtn { "PREVIEW" }, exportBtn { "EXPORT" }, batchBtn { "BATCH EXPORT" }, cancelBtn { "CANCEL" },
                         diceBtn { "NEW PRESET SEED" }, resetBtn { "RESET METERS" }, reBtn { "RE-EXPORT" }, openBtn { "SHOW FILE" };
        std::array<juce::TextButton, 7> batchSrc;
        juce::ListBox histList { "history", this };
        std::vector<fge::xp::HistoryEntry> hist;
        std::vector<std::unique_ptr<juce::Component>> masterCtl;
        juce::String result, info;
        juce::File lastOut;
        std::atomic<bool> previewing { false };
        std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
        bool loading = false;
        juce::Rectangle<int> leftArea, masterArea, meterArea, histArea, resultArea;
    };
}
