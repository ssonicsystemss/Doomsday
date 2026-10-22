#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "../../Shared/Source/DoomsdayFlat.h"

// 5CR34M 4LPH4 - flat UI: every engine control as a knob, grouped as the signal flows; MOD 1 / MOD 2 panel (AlphaMod.h)
namespace alphaui
{
    // the two MOD colours (different from the knob accents hot / cool)
    inline juce::Colour modColour (int m) { return m == 0 ? juce::Colour (0xff3bd6a0) : juce::Colour (0xffb07cff); }
}

class AlphaScope : public juce::Component
{
public:
    explicit AlphaScope (AlphaProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    AlphaProcessor& proc;
};

// one cycle of a MOD: ready shapes are shown, CUSTOM is drawn with points (touching a ready shape turns it into CUSTOM)
//   click = add a point, drag = move, drag the small ring in the middle of a line = bend it,
//   double-click / right-click a point = delete, double-click a ring = straight again, SHIFT = no grid snap
class ModGraph : public juce::Component
{
public:
    ModGraph (AlphaProcessor& p, int mod) : proc (p), m (mod) {}
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = hoverMid = -1; repaint(); }
    void tick();
private:
    int shapeType() const;
    juce::Point<float> toScreen (float x1000, float v) const;
    juce::Point<float> midOf (int seg) const;
    int pointAt (juce::Point<float>) const;
    int midAt (juce::Point<float>) const;
    float snapX (float x1000, bool free) const;
    void makeCustom();          // a ready shape -> points (CUSTOM)
    void commit();
    AlphaProcessor& proc;
    const int m;
    juce::Rectangle<float> plot;
    doomsday::mod::Shape shape;
    int lastSerial = -1, lastType = -1; float lastPh = -1.f; double lastBeats = -1.0;
    int hover = -1, hoverMid = -1, drag = -1, bendSeg = -1;
    float bendStart = 0.f, bendY = 0.f;
};

// the range of a MOD: one slider = the other end of the movement (absolute knob position), from the knob's position
class RangeBar : public juce::Component, public juce::SettableTooltipClient
{
public:
    RangeBar (AlphaProcessor&, int mod);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick();
private:
    float base01() const;
    bool plus() const;
    float valueAtX (float x) const;
    juce::Rectangle<float> track() const;
    AlphaProcessor& proc;
    const int m;
    juce::RangedAudioParameter* range;
    float lastBase = -1.f, lastRange = -1.f; int lastTarget = -1, lastPol = -1;
};

class AlphaCanvas;

// MOD 1 / MOD 2 tabs + the selected MOD's controls
class ModPanel : public juce::Component
{
public:
    ModPanel (AlphaCanvas&, AlphaProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void select (int m);
    int selected() const { return sel; }
    void tick();
private:
    juce::Rectangle<int> tabBounds (int m) const;
    juce::String targetName (int m) const;
    struct Mod
    {
        std::unique_ptr<doomsday::flat::Segmented> shape, rate, pol;
        std::unique_ptr<ModGraph> graph;
        std::unique_ptr<RangeBar> range;
        juce::TextButton assign, clear;
        juce::String lastName;
    };
    AlphaCanvas& canvas;
    AlphaProcessor& proc;
    std::array<Mod, AlphaProcessor::numMods> mods;
    int sel = 0;
};

// shown after ASSIGN: click a knob to give it to the MOD (Esc / anywhere else = cancel)
class AssignOverlay : public juce::Component
{
public:
    explicit AssignOverlay (AlphaCanvas& c) : canvas (c) { setWantsKeyboardFocus (true); }
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    int mod = 0;
private:
    AlphaCanvas& canvas;
    int hover = -1;
};

class AlphaCanvas : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1240, H = 1000;
    explicit AlphaCanvas (AlphaProcessor&);
    ~AlphaCanvas() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    // ASSIGN mode (the overlay)
    void beginAssign (int mod);
    void endAssign();
    void assign (int mod, int targetIndex);
    struct ModKnob { doomsday::flat::Knob* knob; int target; juce::Colour accent; int shownMod = -2; };
    const std::vector<ModKnob>& modKnobs() const { return assignable; }
    int knobAt (juce::Point<int>) const;          // index into modKnobs(), -1 none
    ModPanel& modPanel() { return mods; }
    void refreshNow() { timerCallback(); }     // tests: one UI update without the message loop
private:
    struct Card
    {
        juce::String title; juce::Colour dot; int columns;
        std::vector<juce::Component*> knobs;
        juce::Component* header = nullptr;      // optional segmented control in the title row
        juce::Rectangle<int> area;
    };
    void timerCallback() override;
    juce::Component* knob (const char* id, const char* label, juce::Colour c, bool bipolar = false);
    void step (int);
    void refreshList();
    void showSave();
    void updateModKnobs();

    AlphaProcessor& proc;
    doomsday::flat::FlatLookAndFeel lnf;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<Card> cards;
    std::vector<ModKnob> assignable;
    juce::Rectangle<int> modArea;
    AlphaScope scope;
    juce::Component* fortuneKnob = nullptr;
    ModPanel mods;
    AssignOverlay overlay;
    juce::ComboBox presetBox;
    juce::TextButton prev { "<" }, next { ">" }, undoB { "Undo" }, redoB { "Redo" }, randomB { "Random" }, saveB { "Save" };
    juce::Array<juce::File> userFiles;
    std::unique_ptr<juce::AlertWindow> dialog;
    static constexpr int userBase = 1000;
    juce::String lastName;
    int lastScope = -1;
    juce::TooltipWindow tips { this, 600 };
};

class AlphaEditor : public juce::AudioProcessorEditor
{
public:
    explicit AlphaEditor (AlphaProcessor&);
    void resized() override;
    AlphaCanvas canvas;
};
