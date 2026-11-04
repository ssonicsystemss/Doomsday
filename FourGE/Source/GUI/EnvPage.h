#pragma once
// =====================================================================
//  4GE module 02 - the ENVELOPES page (GUI only).
//  PART (PUNCH / BODY / TAIL / SUB) x VIEW (PITCH / AMPLITUDE / FILTER / DISTORTION / RESONANCE) picks one envelope;
//  the big EnvEditor edits its drawn shape, the row below holds that envelope's real parameters (APVTS attachments).
//  Drawing on an envelope that is in ADSR / an analytic pitch mode switches it to DRAWN / CUSTOM in the same undo step,
//  so a visible curve always plays (no dummy curves - MODULE02 section 15).
//  COPY / PASTE (clipboard), RESET, FIT, SNAP, SAVE / LOAD a shape file (Documents / 4GE / Envelopes).
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayFlat.h"
#include "../PluginProcessor.h"
#include "EnvEditor.h"

namespace fge::gui
{
    class EnvPage : public juce::Component, private juce::Timer
    {
    public:
        enum View { Pitch = 0, Amp, Filter, Drive, Reso, NumViews };
        EnvPage (GeProcessor&, doomsday::flat::FlatLookAndFeel&);
        ~EnvPage() override;
        void paint (juce::Graphics&) override;
        void resized() override;
        void select (int part, int view);
        int slot() const;                                      // env::Slot of the current PART x VIEW (-1 = none)
        static int slotOf (int part, int view);
        EnvEditor& editor() { return ed; }
        juce::Component* findControl (const juce::String& id);  // tests
        void commitShape (const env::Shape&);                   // what the editor does on mouse-up (tests use it too)
        void copyShape();
        bool pasteShape();
        void resetShape();

    private:
        void timerCallback() override;
        void rebuildControls();
        void refreshEditor();
        bool slotActive (juce::String& why) const;
        juce::String paramPrefix() const;
        float rawParam (const juce::String& id) const;
        void setParam (const juce::String& id, float v);
        void saveShapeFile();
        void loadShapeFile();

        GeProcessor& proc;
        doomsday::flat::FlatLookAndFeel& lnf;
        EnvEditor ed;
        fge::Engine cooker;                                     // computes the time / pitch axes from the parameters (no audio)
        int part = 2, view = Pitch;
        uint32_t lastVersion = 0;
        std::array<float, P::count> lastFrame {};
        std::array<juce::TextButton, 4> partBtn;
        std::array<juce::TextButton, NumViews> viewBtn;
        juce::TextButton copyBtn { "COPY" }, pasteBtn { "PASTE" }, resetBtn { "RESET" }, fitBtn { "FIT" }, snapBtn { "SNAP" },
                         saveBtn { "SAVE" }, loadBtn { "LOAD" };
        std::vector<std::unique_ptr<juce::Component>> slotCtl, globalCtl;
        std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
        std::multimap<juce::String, juce::Component*> byId;
        std::unique_ptr<juce::FileChooser> chooser;
        juce::Rectangle<int> left, top, bottom, edArea;
    };
}
