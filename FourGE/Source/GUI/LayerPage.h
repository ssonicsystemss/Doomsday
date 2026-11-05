#pragma once
// =====================================================================
//  4GE module 04 - the LAYERS page (GUI only; tools / files in Presets/LayerTools.h, renders in KickPreview):
//   NOISE     - noise generator A / B: source, level, color, filter, resonance, drive, envelope, width, resonator, digital,
//               seed / random, routing, modulation envelope, the real noise waveform, the drawn profile / envelopes;
//   TRANSIENT - click (source, level, length, pitch, tone, drive, mix, width, position + sync, output), attack level / time /
//               decay / low / high, the transient shaper, the real transient waveform, the drawn CUSTOM transient;
//   LAYERS    - the scrollable list of layers (name, group, source, level, pitch, length, pan, phase, filter, drive, mute,
//               solo, lock, routing, mini waveform) + LAYER / GROUPS / TOOLS (randomizer, phase align, copy / move / rename).
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayFlat.h"
#include "../PluginProcessor.h"
#include "EnvEditor.h"
#include "KickPreview.h"
#include "../Presets/LayerTools.h"

namespace fge::gui
{
    class LayerPage : public juce::Component, private juce::Timer
    {
    public:
        enum View { ViewNoise = 0, ViewTransient, ViewLayers };
        enum Detail { DetLayer = 0, DetGroups, DetTools };
        LayerPage (GeProcessor&, doomsday::flat::FlatLookAndFeel&);
        ~LayerPage() override;
        void paint (juce::Graphics&) override;
        void resized() override;
        void showView (int);
        void selectNoise (int);
        void selectLayer (int);
        void showDetail (int);
        int view() const { return viewIdx; }
        juce::Component* findControl (const juce::String& id);
        KickPreview& preview() { return renders; }
        // tests
        dsp::AlignResult lastAlign;
        bool alignDone = false;
        void runAlign();
        void applyAlign();

    private:
        struct Row : public juce::Component
        {
            int layer = 0;
            std::function<void (int)> onSelect;
            void mouseDown (const juce::MouseEvent&) override { if (onSelect) onSelect (layer); }
            void paint (juce::Graphics&) override;
            bool selected = false; KickPreview::Wave wave; juce::Colour col;
            juce::Rectangle<int> waveArea;
        };
        void timerCallback() override;
        void buildNoise();
        void buildTransient();
        void buildLayers();
        void rebuildRows();
        void layoutRows();
        void rebuildDetail();
        void refreshEditor();
        void setJobs();
        void showPresetMenu();
        void saveFile();
        void loadFile();
        juce::String nid (const juce::String& s) const { return juce::String (noiseSel == 0 ? "noiseA_" : "noiseB_") + s; }
        juce::String lid (const juce::String& s) const { return layers::lid (layerSel, s); }

        // control factories (each registers in byId and in the given owner list)
        juce::Component* knob (std::vector<std::unique_ptr<juce::Component>>& own, const juce::String& id, const juce::String& label, juce::Colour, int d = 30, bool bip = false);
        juce::Component* toggle (std::vector<std::unique_ptr<juce::Component>>& own, const juce::String& id, const juce::String& text, juce::Colour, bool chip = false);
        juce::Component* combo (std::vector<std::unique_ptr<juce::Component>>& own, const juce::String& id, const juce::String& caption);
        juce::Component* bar (std::vector<std::unique_ptr<juce::Component>>& own, const juce::String& id);
        void forget (std::vector<std::unique_ptr<juce::Component>>& own);

        GeProcessor& proc;
        doomsday::flat::FlatLookAndFeel& lnf;
        int viewIdx = ViewLayers, noiseSel = 0, layerSel = 0, detailIdx = DetLayer, noiseEdit = 0;
        std::array<juce::TextButton, 3> viewBtn;
        std::array<juce::TextButton, 2> noiseBtn;
        std::array<juce::TextButton, 3> noiseEditBtn, detailBtn;
        juce::TextButton presetBtn { "PRESETS" }, saveBtn { "SAVE" }, loadBtn { "LOAD" };
        std::vector<std::unique_ptr<juce::Component>> noiseCtl, trCtl, rowCtl, detCtl, fixedCtl;
        std::map<juce::Component*, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
        std::map<juce::Component*, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtt;
        std::map<juce::Component*, juce::Rectangle<int>> layoutMap;      // where each control sits (detail controls: relative to the detail area)
        void place (juce::Component* c, int x, int y, int w, int h) { layoutMap[c] = { x, y, w, h }; }
        std::multimap<juce::String, juce::Component*> byId;
        std::map<juce::Component*, juce::String> captions;
        juce::Viewport listView;
        juce::Component listHolder;
        std::array<Row, 8> rows;
        EnvEditor editor;
        KickPreview renders;
        std::unique_ptr<juce::FileChooser> chooser;
        // tools
        juce::ComboBox randMode, alignRef, alignTarget;
        juce::Slider randIntensity, randSeed, alignSearch;
        juce::ToggleButton keepGroups { "PRESERVE GROUPS" }, keepRouting { "PRESERVE ROUTING" };
        juce::TextButton randBtn { "RANDOMIZE" }, analyseBtn { "ANALYSE" }, applyBtn { "APPLY PHASE" },
                         copyBtn { "COPY" }, pasteBtn { "PASTE" }, dupBtn { "DUPLICATE" }, delBtn { "DELETE" }, upBtn { "MOVE UP" }, downBtn { "MOVE DOWN" },
                         renameBtn { "RENAME" }, lockBtn { "LOCK" };
        std::unique_ptr<juce::XmlElement> clip;
        juce::Rectangle<int> top, content, waveArea, editArea, listArea, detailArea, alignArea;
        uint32_t lastShapes = 0;
    };
}
