#pragma once
#include "PluginProcessor.h"
#include "DoomsdayUI.h"
#include "DoomsdayFlat.h"
#include <map>

// =====================================================================
//  RE4GE - flat UI (the 5CR34M 4LPH4 / 4GE look) with a deep violet accent
//  top: presets | THE STEP (one beat: the envelope of every step, pitch, the ms scale, the last note's output) + MASTER
//  pages SOURCE / SHAPE / TONE
// =====================================================================
namespace ReTheme
{
    const juce::Colour violet { 0xff9b6bff }, violetHot { 0xffd2b8ff }, violetDeep { 0xff2a1452 }, kick { 0xffff5a1f };
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

class Canvas : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    static constexpr int W = 1400, H = 612;
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
    std::unique_ptr<doomsday::flat::TabStrip> tabs;
    int page = 0;
    int cMaster = 0, cSource = 0, cWarp = 0, cTail = 0, cSample = 0, cTrig = 0, cPitch = 0, cEnv = 0, cTimeSync = 0, cTimeMs = 0,
        cPre = 0, cDrive = 0, cPost = 0;
    juce::ComboBox presetBox;
    juce::TextButton prev { "<" }, next { ">" }, randomB { "Random" }, saveB { "Save" }, folderB { "Folder" },
                     undoB { "Undo" }, redoB { "Redo" }, loadBtn { "Load" }, clearBtn { "x" };
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
