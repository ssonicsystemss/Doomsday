#include "MatrixView.h"

using namespace juce;
using namespace doomsday::flat;

MatrixView::MatrixView (ReProcessor& p) : proc (p)
{
    for (int r = 0; r < 16; ++r)
    {
        auto& w = rows[(size_t) r];
        w.src.addItemList (re::modSourceNames(), 1);
        w.dst.addItemList (re::modDestNames(), 1);
        w.pol.addItemList (Choices::polarities, 1);
        w.on.setTooltip ("This row is on");
        w.on.onStateChange = [this] { repaint(); };
        w.src.setTooltip ("SOURCE: what moves (LFO, envelope, the drawn lanes, MIDI, macros, a random value per step, the step number)");
        w.dst.setTooltip ("DESTINATION: what is moved. Several rows on one destination add up");
        w.amt.setTooltip ("AMOUNT: how far (100% = the whole range shown in the destination's name), - = the other way");
        w.pol.setTooltip ("AS IS: the source as it is. UNI: 0..1 (only up). BI: -1..+1 (up and down around the knob)");
        w.amt.setTextBoxStyle (Slider::TextBoxRight, false, 52, 20);
        for (auto* c : { (Component*) &w.on, (Component*) &w.src, (Component*) &w.dst, (Component*) &w.amt, (Component*) &w.pol }) addAndMakeVisible (c);
        w.aOn = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamIDs::mtxId (r, "on"), w.on);
        w.aSrc = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ParamIDs::mtxId (r, "src"), w.src);
        w.aDst = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ParamIDs::mtxId (r, "dst"), w.dst);
        w.aPol = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ParamIDs::mtxId (r, "pol"), w.pol);
        w.aAmt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::mtxId (r, "amt"), w.amt);
    }
}

void MatrixView::resized()
{
    auto a = getLocalBounds().reduced (16, 10).withTrimmedTop (30);
    const int rh = jmin (30, a.getHeight() / 16);
    for (int r = 0; r < 16; ++r)
    {
        auto row = a.removeFromTop (rh).reduced (0, 2);
        auto& w = rows[(size_t) r];
        row.removeFromLeft (28);                                   // the number
        w.on.setBounds (row.removeFromLeft (28));
        w.src.setBounds (row.removeFromLeft (200)); row.removeFromLeft (8);
        w.dst.setBounds (row.removeFromLeft (230)); row.removeFromLeft (8);
        w.pol.setBounds (row.removeFromRight (90)); row.removeFromRight (8);
        w.amt.setBounds (row);
    }
}

void MatrixView::paint (Graphics& g)
{
    auto& lf = lnfOf (*this);
    const auto& tk = tokens();
    g.setColour (tk.surface); g.fillRoundedRectangle (getLocalBounds().toFloat(), 12.f);
    g.setFont (lf.font (11.f, Bold, 0.12f)); g.setColour (tk.textDim);
    auto h = getLocalBounds().reduced (16, 10).removeFromTop (24);
    h.removeFromLeft (56);
    g.drawText ("SOURCE", h.removeFromLeft (208), Justification::centredLeft);
    g.drawText ("DESTINATION", h.removeFromLeft (238), Justification::centredLeft);
    g.drawText ("POLARITY", h.removeFromRight (98), Justification::centredLeft);
    g.drawText ("AMOUNT", h, Justification::centredLeft);
    auto a = getLocalBounds().reduced (16, 10).withTrimmedTop (30);
    const int rh = jmin (30, a.getHeight() / 16);
    g.setFont (lf.font (11.f, Medium));
    for (int r = 0; r < 16; ++r)
    {
        const bool on = rows[(size_t) r].on.getToggleState();
        g.setColour (on ? Colour (0xffff3b3b) : tk.textMute);
        g.drawText (String (r + 1), a.getX(), a.getY() + r * rh, 24, rh, Justification::centredLeft);
    }
}
