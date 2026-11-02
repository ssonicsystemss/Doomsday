#pragma once
#include "../PluginProcessor.h"
#include "DoomsdayUI.h"
#include "DoomsdayFlat.h"
#include "ShapeDesigner.h"
#include "WaveshaperView.h"
#include "MatrixView.h"
#include "KickView.h"
#include "RandomPanel.h"
#include "SeqView.h"
#include "EqView.h"
#include <map>

// =====================================================================
//  RE4GE - flat UI (Shared/DESIGN.md), red accent, the owner's layout (Re4GE/Design mockup, PLAN.md "Nawigacja"):
//  top: presets + BPM + undo | left: NAV - 01 SOUND / 02 PROCESS / 03 MODULATION / 04 SOURCE + SYSTEM at the bottom
//  (a click on a block opens its list, a module fills the work area) | bottom, always: 8 MACROS + MASTER.
//  Only modules that already work are listed (every visible control changes the sound).
// =====================================================================
namespace ReTheme
{
    const juce::Colour accent { 0xffff3b3b }, bass { 0xff4ea1ff }, bassHot { 0xffa9d0ff }, bassDeep { 0xff10243d }, kick { 0xffff3b3b };
}

class StepView : public juce::Component
{
public:
    explicit StepView (ReProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void tick();
private:
    ReProcessor& proc;
    float lastSig = -1.f;
    int lastHit = -1, lastLen = -1;
};

// the left navigation: blocks with their modules; SYSTEM (buttons) is drawn by the canvas below it
class NavBar : public juce::Component
{
public:
    struct Item { juce::String name; int page; std::function<bool()> lit; };
    struct Block { juce::String num, name; std::vector<Item> items; juce::String later; };
    std::vector<Block> blocks;
    std::function<void (int)> onSelect;
    int page = 0, open = 0;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent& e) override { const int h = hit (e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
private:
    // hit codes: block b -> 1000 + b, item -> page
    int hit (juce::Point<float>) const;
    std::vector<std::pair<juce::Rectangle<float>, int>> zones;
    int hover = -1;
};

class Canvas : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    static constexpr int W = 1280, H = 800;
    explicit Canvas (ReProcessor&);
    ~Canvas() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragHover = false; repaint(); }
    void tickForTest() { timerCallback(); }
    void showPageForTest (int p) { showPage (p); }
    void openPresetBrowserForTest() { refreshList(); }
    RandomPanel& randomPanelForTest() { return randomPanel; }
private:
    struct Card
    {
        juce::String title; juce::Colour dot; int page = -1;
        std::vector<juce::Component*> knobs;
        std::vector<juce::Component*> head;
        std::function<bool()> shown;              // optional: the card is only there in some modes
        juce::Rectangle<int> area;
    };
    void timerCallback() override;
    Card& card (const juce::String& title, juce::Colour dot, int page);
    juce::Component* knob (Card&, const char* id, const char* label, const char* tip, bool bipolar = false);
    void showPage (int);
    void refreshList();
    void step (int);
    void showSave();
    void layoutCard (Card&, int kw);
    bool cardOn (const Card& c) const { return (c.page < 0 || c.page == page) && (! c.shown || c.shown()); }

    ReProcessor& proc;
    doomsday::flat::FlatLookAndFeel lnf;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<Card> cards;
    std::map<juce::Component*, int> segW;
    StepView view;
    NavBar nav;
    ShapeDesigner designer;
    WaveshaperView shaperView;
    MatrixView matrix;
    KickView kickView;
    RandomPanel randomPanel;
    SeqView seqView;
    EqView eqView;
    int page = 0;
    enum Page { PMain = 0, POsc, PPitch, PEnv, PShape, PFilter, PDist, PEq, PTail, PSample, PSub, PStereo, PTransient, PDesigner, PWaveshaper, PSettings, PLfo, PMatrix, PFx, PRandom, PSeq, PGate, PDyn, NumPages };
    int cUni = 0, cMaster = 0, cMacros = 0, cSource = 0, cWarp = 0, cPhase = 0, cOsc[3] {}, cTail = 0, cSample = 0, cTrig = 0, cSteps = 0, cPitch = 0, cEnv = 0,
        cTimeSync = 0, cTimeMs = 0, cPre = 0, cDrive = 0, cDrive2 = 0, cPost = 0, cSub = 0, cClick = 0, cLane[4] {}, cLab[2] {}, cCurve[2] {}, cSettings = 0, cLfo[4] {}, cFxDet = -1, cFxOut = -1, cMSource = 0, cMEnv = 0, cMTone = 0, cMLow = 0, cSeq = 0, cGate = 0, cComp = 0;
    juce::Rectangle<int> navArea, sysArea, workArea, fxLedArea, grArea;
    float grShown = 0.f;
    int lastFxHits = 0; float fxBlink = 0.f;
    juce::ComboBox presetBox;
    juce::TextButton prev { "<" }, next { ">" }, randomB { "Random" }, saveB { "Save" }, folderB { "Folder" },
                     undoB { "Undo" }, redoB { "Redo" }, loadBtn { "Load" }, clearBtn { "x" }, settingsB { "Settings" }, rndPageB { "Randomizer" };
    std::vector<std::unique_ptr<juce::TextButton>> templ;
    struct PlayButton : public juce::TextButton
    {
        std::function<void (bool)> onPress;
        PlayButton() : juce::TextButton ("Play") {}
        void mouseDown (const juce::MouseEvent& e) override { juce::TextButton::mouseDown (e); if (onPress) onPress (true); }
        void mouseUp (const juce::MouseEvent& e) override   { juce::TextButton::mouseUp (e);   if (onPress) onPress (false); }
    } playB;
    juce::Array<juce::File> userFiles;
    std::unique_ptr<juce::AlertWindow> dialog;
    std::unique_ptr<juce::FileChooser> chooser;
    static constexpr int userBase = 1000;
    juce::String lastName;
    juce::Rectangle<int> sampleNameArea, meterArea;
    float meterLevel = 0.f;
    int lastSampleSerial = -1, lastMode = -1;
    bool dragHover = false;
    juce::TooltipWindow tips { this, 600 };
};

class ReEditor : public juce::AudioProcessorEditor
{
public:
    explicit ReEditor (ReProcessor&);
    void resized() override;
    Canvas& getCanvas() { return canvas; }
private:
    Canvas canvas;
};
