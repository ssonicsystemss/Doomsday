#pragma once
// =====================================================================
//  4GE module 15 - the PRESET BROWSER page (GUI only; metadata / file actions = Presets/PresetLibrary.h)
//   opened by a click on the preset name. Filters: SEARCH (name, tags, notes), STYLE, LENGTH, SOURCE (ALL / FACTORY / USER /
//   FAVORITES / RECENT), TAG (character / kick type). The list (style, length, tags, favorite). The selected preset: what it is
//   made of (modules), MISSING SAMPLES, TAGS / NOTES. Actions: LOAD (double click), PREVIEW (render + listen without loading), SAVE,
//   SAVE AS, DUPLICATE, RENAME, DELETE, IMPORT, EXPORT. A / B comparison of two complete sounds.
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayFlat.h"
#include "DoomsdayUI.h"
#include "../PluginProcessor.h"
#include "../Presets/PresetLibrary.h"

namespace fge::gui
{
    class PresetPage : public juce::Component, private juce::ListBoxModel, private juce::Timer
    {
    public:
        PresetPage (GeProcessor&, doomsday::ui::PresetBar&);
        ~PresetPage() override;
        void paint (juce::Graphics&) override;
        void resized() override;
        void visibilityChanged() override { if (isVisible()) refresh(); }
        void refresh();                                               // rescan + filter
        std::function<void()> onClose, onRelink;
        // tests / actions
        fge::lib::Library& library() { return lib; }
        int rows() const { return (int) shown.size(); }
        const fge::lib::Entry* entry (int row) const { return juce::isPositiveAndBelow (row, (int) shown.size()) ? &lib.all()[(size_t) shown[(size_t) row]] : nullptr; }
        void setFilter (const juce::String& search, int style, int length, int source, const juce::String& tag);
        void selectRow (int row) { list.selectRow (row); }
        int selectedRow() const { return list.getSelectedRow(); }
        bool loadRow (int row);
        void previewRow (int row);
        bool previewBusy() const { return previewing.load(); }
        juce::String status() const { return msg; }
    private:
        void timerCallback() override;
        int getNumRows() override { return (int) shown.size(); }
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { loadRow (row); }
        void listBoxItemClicked (int row, const juce::MouseEvent& e) override;
        void selectedRowsChanged (int) override { showSelected(); }
        void showSelected();
        void apply();
        void ask (const juce::String& title, const juce::String& text, std::function<void (const juce::String&)> done);
        GeProcessor& proc;
        doomsday::ui::PresetBar& bar;
        fge::lib::Library lib;
        std::vector<int> shown;
        juce::TextEditor search, tagsEd, notesEd;
        juce::ComboBox styleBox, lengthBox, sourceBox, tagBox;
        juce::ListBox list { "presets", this };
        juce::TextButton loadBtn { "LOAD" }, previewBtn { "PREVIEW" }, stopBtn { "STOP" }, saveBtn { "SAVE" }, saveAsBtn { "SAVE AS" }, dupBtn { "DUPLICATE" },
                         renBtn { "RENAME" }, delBtn { "DELETE" }, impBtn { "IMPORT" }, expBtn { "EXPORT" }, tagsBtn { "SAVE TAGS" }, closeBtn { "CLOSE" },
                         aBtn { "A" }, bBtn { "B" }, copyBtn { "COPY A > B" }, relinkBtn { "RELINK..." };
        juce::String msg;
        std::unique_ptr<juce::FileChooser> chooser;
        std::atomic<bool> previewing { false };
        std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
        juce::Rectangle<int> infoArea, filterArea;
    };
}
