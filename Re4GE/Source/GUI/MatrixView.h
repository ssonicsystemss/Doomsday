#pragma once
// RE4GE - MOD MATRIX page: 16 rows (ON / SOURCE / DESTINATION / AMOUNT / POLARITY), bound to the parameters (automatable, saved).
// Pure GUI: the engine reads the same parameters (DSP/ModMatrix.h).
#include "../PluginProcessor.h"
#include "DoomsdayFlat.h"

class MatrixView : public juce::Component
{
public:
    explicit MatrixView (ReProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    struct Row
    {
        juce::ToggleButton on;
        juce::ComboBox src, dst, pol;
        juce::Slider amt { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aOn;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> aSrc, aDst, aPol;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aAmt;
    };
    ReProcessor& proc;
    std::array<Row, 16> rows;
};
