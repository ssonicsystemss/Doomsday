#pragma once
// =====================================================================
//  4GE module 03 - the OSCILLATORS page (GUI only; files go through Presets/OscFiles.h).
//  Top: the 4 oscillators (ON, MUTE, SOLO, RESET, PRESET, WAVEFORM, PITCH FROM, LEVEL, OCT, SEMI, FINE, PHASE, UNISON, routing
//  -> PUNCH / BODY / TAIL / SUB, PAN). Click a strip to select it. Bottom: the 8 tabs of the selected oscillator
//  (WAVEFORM / WAVETABLE / HARMONICS / UNISON / FM MATRIX / RING MOD / PHASE / HARD SYNC, each with SAVE / LOAD)
//  and the scope (real waveform + real spectrum, one oscillator or the sum, before / after modulation).
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayFlat.h"
#include "../PluginProcessor.h"
#include "EnvEditor.h"
#include "OscScope.h"

namespace fge::gui
{
    // the 64 harmonic amplitudes of the HARMONIC profile: drag = draw, right-click = harmonic on / off
    class HarmonicEditor : public juce::Component
    {
    public:
        void setData (const dsp::OscData::One& d) { if (! dragging) { data = d; repaint(); } }
        std::function<void (const dsp::OscData::One&)> onCommit;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
    private:
        int barAt (float x) const;
        void setFrom (juce::Point<float>);
        dsp::OscData::One data;
        bool dragging = false;
    };

    class OscPage : public juce::Component, private juce::Timer
    {
    public:
        OscPage (GeProcessor&, doomsday::flat::FlatLookAndFeel&);
        ~OscPage() override;
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void selectOsc (int);
        void selectTab (int);
        int selectedOsc() const { return sel; }
        int selectedTab() const { return tab; }
        juce::Component* findControl (const juce::String& id);
        OscScope& scope() { return scopeView; }

    private:
        void timerCallback() override;
        void rebuildTab();
        void showPresetMenu (int osc);
        void saveFile (int what);                     // 0 tab, 1 oscillator, 2 set
        void loadFile (int what);
        juce::String sid (const juce::String& suffix) const { return "osc" + juce::String (sel + 1) + "_" + suffix; }

        GeProcessor& proc;
        doomsday::flat::FlatLookAndFeel& lnf;
        int sel = 0, tab = 0;
        std::array<std::vector<juce::Component*>, 4> strip;
        std::array<juce::Rectangle<int>, 4> stripArea;
        std::array<juce::TextButton, 4> resetBtn, presetBtn;
        std::array<juce::TextButton, 8> tabBtn;
        juce::TextButton tabSave { "SAVE" }, tabLoad { "LOAD" }, oscSave { "SAVE OSC" }, oscLoad { "LOAD OSC" }, setSave { "SAVE ALL 4" }, setLoad { "LOAD ALL 4" };
        juce::TextButton viewSum { "SUM" }, viewPost { "AFTER MOD" };
        std::vector<std::unique_ptr<juce::Component>> owned, tabCtl;
        std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt, tabComboAtt;
        std::multimap<juce::String, juce::Component*> byId;
        EnvEditor cycleEditor;                         // CUSTOM: the drawn cycle
        HarmonicEditor harmEditor;
        OscScope scopeView;
        juce::ComboBox cycleLen;                       // WAVETABLE import: samples per frame
        std::unique_ptr<juce::FileChooser> chooser;
        juce::Rectangle<int> tabArea, tabContent, scopeArea, tabEditorArea;
        uint32_t lastData = 0;
        int lastWave = -1;
    };
}
