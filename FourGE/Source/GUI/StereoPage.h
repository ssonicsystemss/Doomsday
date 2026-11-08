#pragma once
// =====================================================================
//  4GE module 11 - the STEREO page (GUI only; DSP in DSP/Spatial/, the analyzer is filled by the processor):
//   7 stereo units (PUNCH / BODY / TAIL / SUB / TEXTURE / CUSTOM groups + MASTER): PAN, BALANCE, WIDTH + algorithm, MIX, OUTPUT,
//   CHANNEL SWAP, POLARITY, CHANNEL DELAY, HAAS, LOW FREQUENCY MONO + crossover, the M/S engine; SUB MONO LOCK; MONO AUDITION
//   (a listening switch, never saved); the visualizer: VECTOR SCOPE, CORRELATION, MID / SIDE and L / R meters, STEREO FIELD.
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayFlat.h"
#include "../PluginProcessor.h"

namespace fge::gui
{
    class StereoScope : public juce::Component
    {
    public:
        explicit StereoScope (GeProcessor& p) : proc (p) {}
        void paint (juce::Graphics&) override;
    private:
        GeProcessor& proc;
        float peakHold = 0.1f;
    };

    class StereoPage : public juce::Component, private juce::Timer
    {
    public:
        StereoPage (GeProcessor&, doomsday::flat::FlatLookAndFeel&);
        ~StereoPage() override;
        void paint (juce::Graphics&) override;
        void resized() override;
        void select (int target);
        int selected() const { return sel; }
        juce::Component* findControl (const juce::String& id);
        juce::StringArray visibleIds() const;
        void setMonoAudition (bool on);
        juce::String warning() const;
        void mouseDown (const juce::MouseEvent&) override;
        juce::Rectangle<int> overviewArea() const { return { 0, ctlArea.getBottom() + 50, ctlArea.getRight() + 10, 7 * 22 + 50 }; }
    private:
        void timerCallback() override;
        void build();
        void forget();
        juce::String id (const char* k) const;
        juce::Component* knob (const char* k, const juce::String& label, bool bip = false);
        juce::Component* toggle (const char* k, const juce::String& text);
        juce::Component* combo (const char* k, const juce::String& caption);
        GeProcessor& proc;
        doomsday::flat::FlatLookAndFeel& lnf;
        int sel = 2;
        std::array<juce::TextButton, 7> tabBtn;
        juce::TextButton monoBtn { "MONO AUDITION" };
        std::unique_ptr<doomsday::flat::Toggle> lockTgl;
        std::vector<std::unique_ptr<juce::Component>> ctl;
        std::map<juce::Component*, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
        std::multimap<juce::String, juce::Component*> byId;
        std::map<juce::Component*, juce::Rectangle<int>> layoutMap;
        std::map<juce::Component*, juce::String> captions;
        std::vector<std::pair<juce::Rectangle<int>, juce::String>> titles;
        StereoScope scope;
        juce::Rectangle<int> ctlArea, scopeArea;
    };
}
