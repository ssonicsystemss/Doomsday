#pragma once
// =====================================================================
//  4GE module 10 - the SAMPLES page (GUI only; decoding / tools / files in Presets/SamplePool.h + SampleBrowser.h,
//  playback in DSP/Sample/Sample.h):
//   8 SLOTS (name, status, ON) + LOAD / RELINK / CLEAR / NORMALIZE / DC / TRIM / COLLECT SAMPLES;
//   the WAVEFORM EDITOR - START / END / LOOP markers (drag; SHIFT = snap to a transient), fades, detected transients,
//   zoom (wheel / + / -), scroll (drag the background), FIT (double click), the live playback position, REGION PREVIEW;
//   the slot's knobs (level, pitch, rate, region, loop, root / tracking, filter, envelope, group / routing, link);
//   the BROWSER - folder, search, FAVORITES, RECENT, PREVIEW (without loading), LOAD TO SLOT, drag & drop of files.
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayFlat.h"
#include "../PluginProcessor.h"
#include "../Presets/SampleBrowser.h"

namespace fge::gui
{
    class SampleWave : public juce::Component, public juce::FileDragAndDropTarget, public juce::SettableTooltipClient
    {
    public:
        explicit SampleWave (GeProcessor& p) : proc (p) {}
        void setSlot (int k) { slot = k; fit(); }
        void fit() { v0 = 0.0; v1 = 1.0; repaint(); }
        void zoom (double factor, double around01);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override { dragging = -1; }
        void mouseDoubleClick (const juce::MouseEvent&) override { fit(); }
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        bool isInterestedInFileDrag (const juce::StringArray& f) override { return f.size() == 1 && smp::Browser::isAudio (juce::File (f[0])); }
        void filesDropped (const juce::StringArray& f, int, int) override { if (onFile) onFile (juce::File (f[0])); }
        std::function<void (const juce::File&)> onFile;
        double viewStart() const { return v0; }
        double viewEnd() const { return v1; }
        // marker i (0 START, 1 END, 2 LOOP START, 3 LOOP END) as a position 0..1 of the sample
        double markerPos (int i) const;
        void setMarker (int i, double pos01, bool snap);           // (tests: the same code as a drag)
        float lastPlayPos = -1.f;
    private:
        juce::String id (const char* k) const { return "smp" + juce::String (slot + 1) + "_" + k; }
        float raw (const char* k) const { auto* v = proc.apvts.getRawParameterValue (id (k)); return v != nullptr ? v->load() : 0.f; }
        double toPos (float x) const { return v0 + (v1 - v0) * (double) x / juce::jmax (1, getWidth()); }
        float toX (double p) const { return (float) ((p - v0) / juce::jmax (1.0e-9, v1 - v0) * getWidth()); }
        GeProcessor& proc;
        int slot = 0, dragging = -1;
        double v0 = 0.0, v1 = 1.0, dragFrom = 0.0, dragV0 = 0.0;
    };

    class SamplePage : public juce::Component, private juce::Timer, private juce::ListBoxModel
    {
    public:
        SamplePage (GeProcessor&, doomsday::flat::FlatLookAndFeel&);
        ~SamplePage() override;
        void paint (juce::Graphics&) override;
        void resized() override;
        void selectSlot (int);
        int slot() const { return sel; }
        juce::Component* findControl (const juce::String& id);
        SampleWave& wave() { return waveEd; }
        smp::Browser& browser() { return files; }
        void setBrowserMode (int m) { mode = m; refreshList(); }
        void setSearch (const juce::String& s) { search.setText (s, juce::dontSendNotification); refreshList(); }
        int listedFiles() const { return listed.size(); }
        void loadListed (int row);                                  // LOAD TO SLOT (tests)
        void previewListed (int row);

    private:
        void timerCallback() override;
        void build();
        void forget();
        void refreshList();
        void updateSlotButtons();
        // ListBoxModel
        int getNumRows() override { return listed.size(); }
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { loadListed (row); }
        juce::var getDragSourceDescription (const juce::SparseSet<int>&) override { return {}; }

        juce::Component* knob (const char* k, const juce::String& label, bool bip = false);
        juce::Component* toggle (const char* k, const juce::String& text);
        juce::Component* combo (const char* k);
        juce::String id (const char* k) const { return "smp" + juce::String (sel + 1) + "_" + k; }

        GeProcessor& proc;
        doomsday::flat::FlatLookAndFeel& lnf;
        int sel = 0, mode = smp::Browser::All;
        std::array<juce::TextButton, 8> slotBtn;
        juce::TextButton loadBtn { "LOAD" }, relinkBtn { "RELINK" }, clearBtn { "CLEAR" }, normBtn { "NORMALIZE" }, dcBtn { "DC" }, trimBtn { "TRIM" },
                         collectBtn { "COLLECT" }, fitBtn { "FIT" }, zinBtn { "+" }, zoutBtn { "-" }, regionBtn { "PREVIEW REGION" }, stopBtn { "STOP" },
                         folderBtn { "FOLDER..." }, allBtn { "ALL" }, favBtn { "FAVORITES" }, recentBtn { "RECENT" }, prevBtn { "PREVIEW" },
                         starBtn { "FAVORITE" }, toSlotBtn { "LOAD TO SLOT" };
        juce::TextEditor search;
        juce::ListBox list { "files", this };
        juce::Array<juce::File> listed;
        smp::Browser files;
        SampleWave waveEd;
        std::vector<std::unique_ptr<juce::Component>> ctl;
        std::map<juce::Component*, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
        std::multimap<juce::String, juce::Component*> byId;
        std::map<juce::Component*, juce::Rectangle<int>> layoutMap;
        std::map<juce::Component*, juce::String> captions;
        std::unique_ptr<juce::FileChooser> chooser;
        juce::String status;
        uint32_t lastVersion = 0xffffffffu;
        juce::Rectangle<int> waveArea, ctlArea, browserArea;
    };
}
