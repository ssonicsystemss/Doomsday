#pragma once
// =====================================================================
//  4GE module 06 - the SCREECH page (GUI only; tools / presets / files in Presets/M06Tools.h, displays in M06Scope):
//   FILTERS    - 8 filter instances: type, place (location + before / after the distortion), path / split, the controls
//                the type uses, the own envelope; FILTER RESPONSE (start / later), ENVELOPE, ROUTING;
//   RESONANCE  - 2 resonators: mode, excitation, frequency / Q / decay / damping / feedback / tracking, pitch envelope,
//                the drawn CUSTOM spectrum; RESONANCE display + its pitch over time;
//   SCREECH    - the screech: mode, oscillators (one at a time), the mode's own controls, 2 filters, AMP / MOD / PITCH envelopes,
//                timing (start mode ...); its waveform + spectrum.
//   PRESETS / SAVE / LOAD / A-B / COPY / PASTE / RANDOMIZE (+ LOCK per engine) - every action one UNDO step.
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayFlat.h"
#include "../PluginProcessor.h"
#include "EnvEditor.h"
#include "M06Scope.h"
#include "../Presets/M06Tools.h"

namespace fge::gui
{
    class ScreechPage : public juce::Component, private juce::Timer
    {
    public:
        enum Tab { TFilters = 0, TResonance, TScreech };
        enum ScrView { SvSource = 0, SvShape, SvEnv, SvTiming, SvPaths };
        ScreechPage (GeProcessor&, doomsday::flat::FlatLookAndFeel&);
        ~ScreechPage() override;
        void paint (juce::Graphics&) override;
        void resized() override;
        void showTab (int);
        void selectFilter (int);
        void selectRes (int);
        void selectOsc (int);
        void showScreechView (int);
        int tab() const { return tabIdx; }
        juce::Component* findControl (const juce::String& id);
        juce::StringArray visibleIds() const;
        void requestScope();
        void adoptScope() { shown = scope.result(); repaint(); }
        M06Scope::Result scopeResult() const { return scope.result(); }
        std::array<bool, 3> locks() const { return { lockBtn[0].getToggleState(), lockBtn[1].getToggleState(), lockBtn[2].getToggleState() }; }

    private:
        void timerCallback() override;
        void build();
        void forget();
        void updateEnabled();
        void showPresetMenu();
        void saveFile();
        void loadFile();
        juce::String signature() const;
        juce::Component* knob (const juce::String& id, const juce::String& label, juce::Colour, bool bip = false, int d = 26);
        juce::Component* toggle (const juce::String& id, const juce::String& text, juce::Colour);
        juce::Component* combo (const juce::String& id, const juce::String& caption);
        void place (juce::Component* c, int x, int y, int w, int h) { if (c != nullptr) layoutMap[c] = { x, y, w, h }; }
        float raw (const juce::String& id) const;

        GeProcessor& proc;
        doomsday::flat::FlatLookAndFeel& lnf;
        int tabIdx = TScreech, fltSel = 0, resSel = 0, oscSel = 0, scrView = SvSource;
        std::array<juce::TextButton, 3> tabBtn;
        std::array<juce::TextButton, 8> fltBtn;
        std::array<juce::TextButton, 2> resBtn;
        std::array<juce::TextButton, 4> oscBtn; std::array<juce::TextButton, 5> viewBtn;
        std::array<juce::ToggleButton, 3> lockBtn;
        juce::TextButton presetBtn { "PRESETS" }, saveBtn { "SAVE" }, loadBtn { "LOAD" }, abBtn { "A / B" }, randBtn { "RANDOMIZE" }, copyBtn { "COPY" }, pasteBtn { "PASTE" };
        std::vector<std::unique_ptr<juce::Component>> ctl;
        std::map<juce::Component*, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
        std::multimap<juce::String, juce::Component*> byId;
        std::map<juce::Component*, juce::String> captions;
        std::map<juce::Component*, juce::Rectangle<int>> layoutMap;
        std::vector<std::pair<juce::Rectangle<int>, juce::String>> titles;
        EnvEditor customEd;
        M06Scope scope;
        M06Scope::Result shown;
        m06t::Compare compare;
        std::unique_ptr<juce::XmlElement> clipFilter, clipRes;
        std::unique_ptr<juce::FileChooser> chooser;
        juce::String lastSig;
        ParamFrame lastAsked; uint32_t lastShapes = 0; int lastF = -1, lastR = -1;
        juce::Rectangle<int> graphA, graphB, graphC;
    };
}
