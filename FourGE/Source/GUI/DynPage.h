#pragma once
// =====================================================================
//  4GE module 08 - the DYNAMICS page (GUI only; presets / files / tools in Presets/M08Tools.h, displays in DynScope on a thread):
//   COMPRESSOR (4) - 8 types, detection, sidechain from another part, parallel dry / wet, lookahead (MASTER OUTPUT only);
//   MULTIBAND      - the module 07 multiband dynamics + BAND LINK;   TRANSIENT (5) - attack / sustain per part;
//   CLIPPER (2)    - 7 curves incl. a drawn CUSTOM CURVE, oversampling;   LIMITER (2) - 5 modes, true peak, lookahead;
//   CREST & LEVEL  - crest factor control, AUTO GAIN, LEVEL MATCH, the processing order.
//   Every processor: the static curve, the gain-reduction history of a real render, live IN / OUT / GR meters, warnings.
//   PRESETS / SAVE / LOAD / A-B / COPY / PASTE / RESET - one UNDO step each.
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayFlat.h"
#include "../PluginProcessor.h"
#include "DynScope.h"
#include "DistPage.h"
#include "../Presets/M08Tools.h"

namespace fge::gui
{
    class DynPage : public juce::Component, private juce::Timer
    {
    public:
        enum Tab { TComp = 0, TMultiband, TTransient, TClipper, TLimiter, TCrest, NumTabs };
        DynPage (GeProcessor&, doomsday::flat::FlatLookAndFeel&);
        ~DynPage() override;
        void paint (juce::Graphics&) override;
        void resized() override;
        void showTab (int);
        void select (int);
        int tab() const { return tabIdx; }
        int selected() const { return sel; }
        int kind() const;                                         // m08t::Kind of the tab (-1 = MULTIBAND)
        juce::Component* findControl (const juce::String& id);
        juce::StringArray visibleIds() const;
        bool isControlEnabled (const juce::String& id) { auto* c = findControl (id); return c != nullptr && c->isEnabled(); }
        void requestScope();
        void adoptScope();
        DynScope::Result scopeResult() const { return scope.result(); }
        juce::StringArray warnings() const;

    private:
        void timerCallback() override;
        void build();
        void forget();
        void updateEnabled();
        void showPresetMenu();
        void saveFile();
        void loadFile();
        juce::Component* knob (const juce::String& id, const juce::String& label, juce::Colour, bool bip = false, int d = 26);
        juce::Component* toggle (const juce::String& id, const juce::String& text, juce::Colour);
        juce::Component* combo (const juce::String& id, const juce::String& caption);
        void place (juce::Component* c, int x, int y, int w, int h) { if (c != nullptr) layoutMap[c] = { x, y, w, h }; }
        float raw (const juce::String& id) const;
        juce::String id (const juce::String& k) const { return m08t::pid (kind(), sel, k); }
        int meterIndex() const;

        GeProcessor& proc;
        doomsday::flat::FlatLookAndFeel& lnf;
        int tabIdx = TComp, sel = 0;
        std::array<int, NumTabs> selOf {};
        std::array<juce::TextButton, NumTabs> tabBtn;
        std::array<juce::TextButton, 5> selBtn;
        juce::TextButton presetBtn { "PRESETS" }, saveBtn { "SAVE" }, loadBtn { "LOAD" }, abBtn { "[A] / B" },
                         copyBtn { "COPY" }, pasteBtn { "PASTE" }, resetBtn { "RESET" }, maxBtn { "RESET MAX" };
        std::vector<std::unique_ptr<juce::Component>> ctl;
        std::map<juce::Component*, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
        std::multimap<juce::String, juce::Component*> byId;
        std::map<juce::Component*, juce::String> captions;
        std::map<juce::Component*, juce::Rectangle<int>> layoutMap;
        std::vector<std::pair<juce::Rectangle<int>, juce::String>> titles;
        CurveEditor clipEd;
        DynScope scope;
        DynScope::Result shown;
        m08t::Compare compare;
        std::unique_ptr<juce::XmlElement> clip;
        std::unique_ptr<juce::FileChooser> chooser;
        ParamFrame lastAsked; uint32_t lastShapes = 0, lastDist = 0xffffffffu; int lastKind = -2, lastSel = -2;
        juce::Rectangle<int> curveArea, histArea, meterArea;
    };
}
