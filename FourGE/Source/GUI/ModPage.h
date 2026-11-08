#pragma once
// =====================================================================
//  4GE module 09 - the MOD page (GUI only; the matrix lives in the processor state MODM, the DSP in DSP/Mod/Mod.h):
//   MATRIX     - 64 slots, 16 per view (ON, SOURCE, DESTINATION, DEPTH, OFFSET, POLARITY, CURVE, MODE, SMOOTH, LAG, MUTE, SOLO, LABEL)
//                + a live bar per slot (BASE / FINAL), a filter by source (= the MACRO MAPPING of one macro), clear / reset;
//   LFO 1-8    - wave, rate / sync / division, phase, retrigger, mode, polarity, fades, smoothing, pulse width + the live value;
//   SEQUENCER  - 64 steps drawn with the mouse (value, probability, ratchet, curve, tie, accent, mute) + its settings;
//   RANDOM / MACROS - the two random sources, the 8 MASTER MACROS + 4 USER MACROS, MOVEMENT;
//   SOURCE MONITOR - every source's live value (the engine publishes them once per block).
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayFlat.h"
#include "../PluginProcessor.h"

namespace fge::gui
{
    class ModPage : public juce::Component, private juce::Timer
    {
    public:
        enum Tab { TLfo = 0, TSeq, TRandom, TMidi, TVariant, NumTabs };
        ModPage (GeProcessor&, doomsday::flat::FlatLookAndFeel&);
        ~ModPage() override;
        void paint (juce::Graphics&) override;
        void resized() override;
        void showTab (int);
        void showLfo (int);
        void setView (int firstSlot);                    // 0, 16, 32, 48
        void setFilter (int source);                     // -1 = all slots, else only this source (MACRO MAPPING view)
        int tab() const { return tabIdx; }
        juce::Component* findControl (const juce::String& id);
        juce::StringArray visibleIds() const;
        // the matrix rows as the GUI sees them (tests)
        int rowSlot (int row) const;
        void editSlot (int slot, const std::function<void (fge::modstate::SlotDesc&)>& change);
        void pickDestination (int slot, const juce::String& id) { editSlot (slot, [&] (auto& s) { s.dst = id; if (s.src == 0) s.src = dsp::mod::SLfo1; s.on = true; }); }
        // the sequencer grid (tests): set a step value by the same code the mouse uses
        void drawStep (int step, float value);
        void selectSlot (int slot) { selSlot = slot; repaint(); }
        int selectedSlot() const { return selSlot; }
        juce::String destinationMonitor() const;          // BASE / MODULATION / FINAL of the selected slot's target (in its units)
        // the CUSTOM LFO editor (tests use the same code as the mouse)
        void addLfoPoint (float x, float y);
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override { dragPt = -1; }

        class Row;
        class StepGrid;
    private:
        void timerCallback() override;
        void build();
        void forget();
        void refreshRows();
        void showDestinationMenu (int slot, juce::Component* target);
        juce::Component* knob (const juce::String& id, const juce::String& label, juce::Colour, bool bip = false, int d = 26);
        juce::Component* toggle (const juce::String& id, const juce::String& text, juce::Colour);
        juce::Component* combo (const juce::String& id, const juce::String& caption);
        void place (juce::Component* c, int x, int y, int w, int h) { if (c != nullptr) layoutMap[c] = { x, y, w, h }; }

        GeProcessor& proc;
        doomsday::flat::FlatLookAndFeel& lnf;
        int tabIdx = TLfo, lfoSel = 0, firstSlot = 0, filterSrc = -1, selSlot = -1, dragPt = -1, learnMacro = 0;
        bool newPtStep = true;
        juce::Point<float> ptAt (const dsp::mod::LfoShape&, int) const;
        juce::Point<float> fromView (juce::Point<float>) const;
        uint32_t shownVersion = 0;
        std::array<juce::TextButton, NumTabs> tabBtn;
        std::array<juce::TextButton, 8> lfoBtn;
        std::array<juce::TextButton, 4> viewBtn;
        juce::ComboBox filterBox;
        juce::TextButton clearBtn { "CLEAR ALL" }, resetBtn { "DEFAULT MACROS" }, addBtn { "+ ADD" };
        std::vector<std::unique_ptr<Row>> rows;
        std::unique_ptr<StepGrid> grid;
        juce::TextButton storeA { "STORE A" }, storeB { "STORE B" }, rndGo { "RANDOMIZE" }, rndRegen { "REGENERATE" }, macroLearn { "LEARN" };
        std::array<juce::TextButton, 4> ccLearn;
        std::array<juce::TextButton, 7> lockBtn;
        juce::ComboBox styleBox, scopeBox, macroBox;
        juce::Slider amountSl, seedSl;
        juce::ToggleButton routingTg { "ROUTING (a few LFO / SEQ / RANDOM connections)" };
        void loadVariant();
        void saveVariant();
        juce::TextButton seqCopy { "COPY" }, seqPaste { "PASTE" }, seqRev { "REVERSE" }, seqShift { "SHIFT >" }, seqRand { "RANDOM" }, seqClear { "CLEAR" };
        std::array<dsp::mod::Step, dsp::mod::maxSteps> seqClip {}; bool hasSeqClip = false;
        std::vector<std::unique_ptr<juce::Component>> ctl;
        std::map<juce::Component*, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
        std::multimap<juce::String, juce::Component*> byId;
        std::map<juce::Component*, juce::String> captions;
        std::map<juce::Component*, juce::Rectangle<int>> layoutMap;
        std::vector<std::pair<juce::Rectangle<int>, juce::String>> titles;
        std::vector<int> visibleSlots;
        juce::Rectangle<int> matrixArea, editArea, monitorArea, macroArea, lfoView, builtFor;
        friend class Row;
    };
}
