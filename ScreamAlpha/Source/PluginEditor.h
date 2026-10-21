#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "../../Shared/Source/DoomsdayFlat.h"

// 5CR34M 4LPH4 v0.1 - flat test UI: every engine control as a knob, grouped as the signal flows
class AlphaScope : public juce::Component
{
public:
    explicit AlphaScope (AlphaProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    AlphaProcessor& proc;
};

// one cycle of the built-in cutoff automation + where it is now
class AutoPreview : public juce::Component
{
public:
    explicit AutoPreview (AlphaProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void tick();
private:
    AlphaProcessor& proc;
    double lastPh = -1.0;
    int lastShape = -1; float lastDepth = 99.f;
};

class AlphaCanvas : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1240, H = 800;
    explicit AlphaCanvas (AlphaProcessor&);
    ~AlphaCanvas() override;
    void paint (juce::Graphics&) override;
    void resized() override;
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

    AlphaProcessor& proc;
    doomsday::flat::FlatLookAndFeel lnf;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<Card> cards;
    AlphaScope scope;
    AutoPreview autoView;
    juce::Component* fortuneKnob = nullptr;
    juce::Component* shapeSeg = nullptr;
    juce::Component* lenSeg = nullptr;
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
