#include "PluginEditor.h"
#include "../Shared/Source/DoomsdayBrand.h"
#include "BinaryData.h"

using namespace juce;

// ======================================================================= LookAndFeel
StageLookAndFeel::StageLookAndFeel()
{
    tfTitle = Typeface::createSystemTypefaceFor (BinaryData::MichromaRegular_ttf, BinaryData::MichromaRegular_ttfSize);
    tfLabel = Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedSemiBold_ttf, BinaryData::BarlowSemiCondensedSemiBold_ttfSize);
    tfValue = Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedMedium_ttf, BinaryData::BarlowSemiCondensedMedium_ttfSize);
    tfBold  = Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedBold_ttf, BinaryData::BarlowSemiCondensedBold_ttfSize);

    setColour (PopupMenu::backgroundColourId, Colour (0xf0100a0c));
    setColour (PopupMenu::textColourId, Palette::value);
    setColour (PopupMenu::headerTextColourId, Palette::red);
    setColour (PopupMenu::highlightedBackgroundColourId, Palette::deepRed.withAlpha (0.85f));
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (ComboBox::textColourId, Palette::text);
    setColour (ComboBox::arrowColourId, Palette::red);
    setColour (Label::textColourId, Palette::text);
    setColour (TextEditor::backgroundColourId, Colour (0xff150e10));
    setColour (TextEditor::textColourId, Palette::text);
    setColour (TextEditor::highlightColourId, Palette::deepRed);
    setColour (TextEditor::outlineColourId, Colours::transparentBlack);
    setColour (TextEditor::focusedOutlineColourId, Palette::red);
    setColour (CaretComponent::caretColourId, Palette::red);
    setColour (AlertWindow::backgroundColourId, Colour (0xff100a0c));
    setColour (AlertWindow::textColourId, Palette::text);
    setColour (AlertWindow::outlineColourId, Palette::red.withAlpha (0.6f));
    setColour (TextButton::textColourOffId, Palette::text);
    setColour (TextButton::textColourOnId, Colours::white);
    setColour (TooltipWindow::backgroundColourId, Colour (0xff140d10));
    setColour (TooltipWindow::textColourId, Palette::value);
    setColour (TooltipWindow::outlineColourId, Palette::edge);
}

static Font makeFont (const Typeface::Ptr& tf, float h, float k)
{
    auto o = FontOptions (tf).withHeight (h);
    if (k != 0.f) o = o.withKerningFactor (k);
    return Font (o);
}
Font StageLookAndFeel::titleFont (float h, float k) const { return makeFont (tfTitle, h, k); }
Font StageLookAndFeel::labelFont (float h, float k) const { return makeFont (tfLabel, h, k); }
Font StageLookAndFeel::valueFont (float h, float k) const { return makeFont (tfValue, h, k); }
Font StageLookAndFeel::boldFont  (float h, float k) const { return makeFont (tfBold, h, k); }

StageLookAndFeel& stageLnf (Component& c)
{
    return dynamic_cast<StageLookAndFeel&> (c.getLookAndFeel());
}

void StageLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, Slider& s)
{
    const bool big     = (bool) s.getProperties()["big"];
    const bool bipolar = (bool) s.getProperties()["bipolar"];
    const bool hover   = s.isMouseOverOrDragging() && s.isEnabled();

    auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (big ? 6.f : 3.f);
    const float size = jmin (bounds.getWidth(), bounds.getHeight());
    bounds = bounds.withSizeKeepingCentre (size, size);
    const auto c = bounds.getCentre();
    const float r = size * 0.5f;
    const float ringW = big ? 5.f : 2.6f;
    const float ringR = r - ringW * (big ? 2.2f : 1.6f);
    const float angle = startAngle + pos * (endAngle - startAngle);

    // big knob: outer tick crown
    if (big)
    {
        const int ticks = 48;
        for (int i = 0; i <= ticks; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * (float) i / (float) ticks;
            const bool lit = a <= angle + 0.001f;
            const float len = i % 4 == 0 ? 7.f : 3.5f;
            g.setColour (lit ? Palette::red.withAlpha (0.55f + 0.45f * (float) i / (float) ticks) : Colours::white.withAlpha (0.08f));
            g.drawLine ({ c.getPointOnCircumference (r - 1.f, a), c.getPointOnCircumference (r - 1.f - len, a) }, i % 4 == 0 ? 1.6f : 1.1f);
        }
    }

    // track
    Path track;
    track.addCentredArc (c.x, c.y, ringR, ringR, 0.f, startAngle, endAngle, true);
    g.setColour (Colours::white.withAlpha (0.075f));
    g.strokePath (track, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));

    // value arc with bloom
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.002f)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, ringR, ringR, 0.f, jmin (from, angle), jmax (from, angle), true);
        g.setColour (Palette::red.withAlpha (hover ? 0.20f : 0.12f));
        g.strokePath (arc, PathStrokeType (ringW * 4.5f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Palette::red.withAlpha (hover ? 0.40f : 0.28f));
        g.strokePath (arc, PathStrokeType (ringW * 2.2f, PathStrokeType::curved, PathStrokeType::rounded));
        ColourGradient grad (Palette::deepRed, c.x - ringR, c.y + ringR, Palette::hot, c.x + ringR, c.y - ringR, false);
        grad.addColour (0.45, Palette::red);
        g.setGradientFill (grad);
        g.strokePath (arc, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));

        const auto tip = c.getPointOnCircumference (ringR, angle);
        g.setColour (Palette::hot.withAlpha (0.35f));
        g.fillEllipse (Rectangle<float> (ringW * 3.f, ringW * 3.f).withCentre (tip));
        g.setColour (Colour (0xffffe3e7));
        g.fillEllipse (Rectangle<float> (ringW * 0.9f, ringW * 0.9f).withCentre (tip));
    }

    // body
    const float bodyR = ringR - ringW - (big ? 14.f : 5.5f);
    const auto body = Rectangle<float> (bodyR * 2.f, bodyR * 2.f).withCentre (c);
    g.setColour (Colours::black.withAlpha (0.65f));
    g.fillEllipse (body.translated (0.f, big ? 5.f : 2.5f).expanded (big ? 4.f : 1.5f));

    ColourGradient bg (Colour (0xff3b3135), c.x - bodyR * 0.35f, c.y - bodyR * 0.65f,
                       Colour (0xff0b0809), c.x + bodyR * 0.4f, c.y + bodyR * 1.05f, true);
    g.setGradientFill (bg);
    g.fillEllipse (body);

    // machined rim
    ColourGradient rim (Colours::white.withAlpha (0.22f), c.x, body.getY(), Colours::black.withAlpha (0.6f), c.x, body.getBottom(), false);
    g.setGradientFill (rim);
    g.drawEllipse (body.reduced (0.5f), 1.f);

    if (big)
    {
        // concentric grooves + soft cap
        for (int i = 1; i <= 3; ++i)
        {
            g.setColour (Colours::black.withAlpha (0.35f));
            g.drawEllipse (body.reduced (bodyR * 0.16f * (float) i), 1.f);
            g.setColour (Colours::white.withAlpha (0.035f));
            g.drawEllipse (body.reduced (bodyR * 0.16f * (float) i).translated (0.f, 1.f), 1.f);
        }
        g.setColour (Palette::red.withAlpha (0.10f + 0.25f * pos));
        g.fillEllipse (body.reduced (bodyR * 0.62f));
    }
    else
    {
        g.setColour (Colours::white.withAlpha (0.05f));
        g.fillEllipse (Rectangle<float> (bodyR * 0.9f, bodyR * 0.5f).withCentre ({ c.x - bodyR * 0.15f, c.y - bodyR * 0.45f }));
    }

    // pointer
    const auto p1 = c.getPointOnCircumference (bodyR * (big ? 0.55f : 0.42f), angle);
    const auto p2 = c.getPointOnCircumference (bodyR * 0.86f, angle);
    if (hover)
    {
        g.setColour (Palette::red.withAlpha (0.55f));
        g.drawLine ({ p1, p2 }, big ? 7.f : 4.5f);
    }
    g.setColour (s.isEnabled() ? Colours::white : Palette::dim);
    g.drawLine ({ p1, p2 }, big ? 3.f : 1.8f);
}

Label* StageLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    const bool big = (bool) s.getProperties()["big"];
    l->setFont (big ? titleFont (15.f, 0.08f) : valueFont (13.f, 0.02f));
    l->setColour (Label::textColourId, big ? Palette::text : Palette::value);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setColour (Label::textWhenEditingColourId, Palette::text);
    l->setColour (Label::outlineWhenEditingColourId, Palette::red.withAlpha (0.6f));
    l->setColour (Label::backgroundWhenEditingColourId, Colour (0xff150e10));
    l->setJustificationType (Justification::centred);
    return l;
}

void StageLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (0.5f);
    const bool hover = box.isMouseOver (true);
    g.setColour (Colour (0xcc0c0809));
    g.fillRoundedRectangle (r, 6.f);
    ColourGradient sheen (Colours::white.withAlpha (0.07f), 0.f, r.getY(), Colours::white.withAlpha (0.01f), 0.f, r.getBottom(), false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (r, 6.f);
    g.setColour (hover ? Palette::red.withAlpha (0.8f) : Colours::white.withAlpha (0.13f));
    g.drawRoundedRectangle (r, 6.f, 1.f);

    Path chev;
    const float ax = (float) w - 16.f, ay = (float) h * 0.5f;
    chev.startNewSubPath (ax - 4.f, ay - 2.f);
    chev.lineTo (ax, ay + 2.f);
    chev.lineTo (ax + 4.f, ay - 2.f);
    g.setColour (Palette::red);
    g.strokePath (chev, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
}

Font StageLookAndFeel::getComboBoxFont (ComboBox&) { return labelFont (16.f, 0.03f); }

void StageLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (Justification::centred);
}

Font StageLookAndFeel::getPopupMenuFont() { return valueFont (16.f); }

void StageLookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (findColour (PopupMenu::backgroundColourId));
    g.setColour (Palette::red.withAlpha (0.35f));
    g.fillRect (0, 0, w, 1);
    g.setColour (Palette::edge);
    g.drawRect (0, 0, w, h, 1);
}

void StageLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool hover, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getHeight() * 0.5f;
    g.setColour (down ? Palette::deepRed.withAlpha (0.8f) : Colour (0xcc0c0809));
    g.fillRoundedRectangle (r, rad);
    ColourGradient sheen (Colours::white.withAlpha (hover ? 0.10f : 0.06f), 0.f, r.getY(),
                          Colours::white.withAlpha (0.0f), 0.f, r.getBottom(), false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (r, rad);
    if (hover)
    {
        g.setColour (Palette::red.withAlpha (0.18f));
        g.drawRoundedRectangle (r.expanded (1.5f), rad + 1.5f, 3.f);
    }
    g.setColour (hover ? Palette::red.withAlpha (0.9f) : Colours::white.withAlpha (0.14f));
    g.drawRoundedRectangle (r, rad, 1.f);
}

void StageLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool hover, bool)
{
    g.setFont (labelFont (b.getButtonText().length() <= 1 ? 15.f : 12.5f, 0.22f));
    g.setColour (hover ? Colours::white : Palette::value);
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
}

Font StageLookAndFeel::getAlertWindowTitleFont()   { return titleFont (15.f, 0.15f); }
Font StageLookAndFeel::getAlertWindowMessageFont() { return valueFont (15.f); }
Font StageLookAndFeel::getTextButtonFont (TextButton&, int) { return labelFont (13.f, 0.2f); }
Font StageLookAndFeel::getLabelFont (Label& l) { return l.getFont(); }

// ======================================================================= Knob
Knob::Knob (AudioProcessorValueTreeState& apvts, const String& paramId, const String& text, bool bipolar, bool isBig)
    : label (text), big (isBig)
{
    slider.getProperties().set ("bipolar", bipolar);
    slider.getProperties().set ("big", isBig);
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, isBig ? 140 : 90, isBig ? 26 : 18);
    slider.setMouseDragSensitivity (isBig ? 360 : 240);
    slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxTextColourId, Palette::value);
    slider.setColour (Slider::textBoxHighlightColourId, Palette::deepRed);
    addAndMakeVisible (slider);

    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
    if (auto* p = apvts.getParameter (paramId))
    {
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.setTooltip (p->getName (64) + "  (double-click = default)");
    }
}

void Knob::paint (Graphics& g)
{
    if (label.isEmpty()) return;
    auto& lnf = stageLnf (*this);
    g.setFont (lnf.labelFont (11.5f, 0.16f));
    g.setColour (slider.isMouseOverOrDragging() ? Palette::text : Palette::label);
    g.drawText (label, labelArea, Justification::centred);
}

void Knob::resized()
{
    // every standard knob has the same size, whatever space its panel gives it
    auto r = getLocalBounds();
    const int textH = big ? 26 : 18;
    const int labelH = label.isNotEmpty() ? 18 : 0;
    const int maxD = big ? 1000 : 62;
    const int need = labelH + maxD + textH;
    if (r.getHeight() > need) r = r.withSizeKeepingCentre (r.getWidth(), need);
    labelArea = r.removeFromTop (labelH);
    slider.setBounds (r);
}

// ======================================================================= Switch
Switch::Switch (AudioProcessorValueTreeState& apvts, const String& paramId, const String& t, Style s)
    : text (t), style (s)
{
    auto* p = apvts.getParameter (paramId);
    jassert (p != nullptr);
    attachment = std::make_unique<ParameterAttachment> (*p, [this] (float v) { state = v > 0.5f; repaint(); }, nullptr);
    attachment->sendInitialUpdate();
    setTooltip (p->getName (64));
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void Switch::mouseDown (const MouseEvent&)
{
    attachment->setValueAsCompleteGesture (state ? 0.f : 1.f);
}

void Switch::paint (Graphics& g)
{
    auto& lnf = stageLnf (*this);
    const bool hover = isMouseOver();
    auto r = getLocalBounds().toFloat();

    if (style == Power)
    {
        auto pill = Rectangle<float> (34.f, 18.f).withCentre ({ r.getRight() - 18.f, r.getCentreY() });
        if (text.isNotEmpty())
        {
            g.setFont (lnf.labelFont (11.f, 0.2f));
            g.setColour (state ? Palette::text : Palette::dim);
            g.drawText (text, r.withRight (pill.getX() - 8.f), Justification::centredRight);
        }
        if (state)
        {
            g.setColour (Palette::red.withAlpha (0.25f));
            g.fillRoundedRectangle (pill.expanded (3.f), 12.f);
            ColourGradient gr (Palette::hot, pill.getX(), pill.getY(), Palette::deepRed, pill.getX(), pill.getBottom(), false);
            gr.addColour (0.5, Palette::red);
            g.setGradientFill (gr);
        }
        else
            g.setColour (Colour (0xff1c1517));
        g.fillRoundedRectangle (pill, 9.f);
        g.setColour (state ? Palette::hot.withAlpha (0.8f) : Colours::white.withAlpha (hover ? 0.3f : 0.14f));
        g.drawRoundedRectangle (pill.reduced (0.5f), 9.f, 1.f);

        const float kx = state ? pill.getRight() - 9.f : pill.getX() + 9.f;
        auto knobR = Rectangle<float> (12.f, 12.f).withCentre ({ kx, pill.getCentreY() });
        g.setColour (Colours::black.withAlpha (0.4f));
        g.fillEllipse (knobR.translated (0.f, 1.f));
        g.setColour (state ? Colours::white : Colour (0xff6e6366));
        g.fillEllipse (knobR);
    }
    else
    {
        auto chip = r.reduced (1.f);
        const float rad = chip.getHeight() * 0.5f;
        g.setColour (state ? Palette::deepRed.withAlpha (0.45f) : Colour (0xcc0c0809));
        g.fillRoundedRectangle (chip, rad);
        if (state)
        {
            g.setColour (Palette::red.withAlpha (0.2f));
            g.drawRoundedRectangle (chip.expanded (1.5f), rad + 1.5f, 3.f);
        }
        g.setColour (state ? Palette::red.withAlpha (0.9f) : Colours::white.withAlpha (hover ? 0.3f : 0.14f));
        g.drawRoundedRectangle (chip, rad, 1.f);

        auto led = Rectangle<float> (6.f, 6.f).withCentre ({ chip.getX() + 13.f, chip.getCentreY() });
        if (state)
        {
            g.setColour (Palette::red.withAlpha (0.45f));
            g.fillEllipse (led.expanded (3.5f));
        }
        g.setColour (state ? Palette::hot : Colour (0xff3a3033));
        g.fillEllipse (led);

        g.setFont (lnf.labelFont (11.5f, 0.18f));
        g.setColour (state ? Palette::text : Palette::label);
        g.drawText (text, chip.withTrimmedLeft (20.f).withTrimmedRight (6.f), Justification::centred);
    }
}

// ======================================================================= Segmented
Segmented::Segmented (AudioProcessorValueTreeState& apvts, const String& paramId, StringArray l) : labels (std::move (l))
{
    auto* p = apvts.getParameter (paramId);
    jassert (p != nullptr);
    attachment = std::make_unique<ParameterAttachment> (*p, [this] (float v) { index = (int) std::round (v); repaint(); }, nullptr);
    attachment->sendInitialUpdate();
    setTooltip (p->getName (64));
    setMouseCursor (MouseCursor::PointingHandCursor);
}

int Segmented::itemAt (Point<int> pt) const
{
    const float ih = (float) getHeight() / (float) labels.size();
    return jlimit (0, labels.size() - 1, (int) ((float) pt.y / ih));
}

void Segmented::mouseDown (const MouseEvent& e) { attachment->setValueAsCompleteGesture ((float) itemAt (e.getPosition())); }
void Segmented::mouseMove (const MouseEvent& e) { const int h = itemAt (e.getPosition()); if (h != hover) { hover = h; repaint(); } }

void Segmented::paint (Graphics& g)
{
    auto& lnf = stageLnf (*this);
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xaa0a0708));
    g.fillRoundedRectangle (r, 7.f);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r.reduced (0.5f), 7.f, 1.f);

    const float ih = r.getHeight() / (float) labels.size();
    for (int i = 0; i < labels.size(); ++i)
    {
        auto item = Rectangle<float> (r.getX(), r.getY() + ih * (float) i, r.getWidth(), ih).reduced (3.f, 2.f);
        const bool sel = i == index;
        if (sel)
        {
            g.setColour (Palette::red.withAlpha (0.16f));
            g.fillRoundedRectangle (item.expanded (1.f), 5.f);
            ColourGradient gr (Palette::deepRed.withAlpha (0.95f), item.getX(), item.getY(), Palette::red.withAlpha (0.65f), item.getRight(), item.getBottom(), false);
            g.setGradientFill (gr);
            g.fillRoundedRectangle (item, 5.f);
            g.setColour (Palette::hot.withAlpha (0.7f));
            g.drawRoundedRectangle (item, 5.f, 1.f);
        }
        else if (i == hover)
        {
            g.setColour (Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (item, 5.f);
        }
        g.setFont (lnf.labelFont (12.f, 0.16f));
        g.setColour (sel ? Colours::white : (i == hover ? Palette::text : Palette::label));
        g.drawText (labels[i], item, Justification::centred);
    }
}

// ======================================================================= OscPanel
OscPanel::OscPanel (ProphecyProcessor& p, int o)
    : proc (p), osc (o), power (p.apvts, ParamIDs::osc (o, ParamIDs::on), "", Switch::Power)
{
    using namespace ParamIDs;
    addAndMakeVisible (power);
    addAndMakeVisible (content);
    content.setInterceptsMouseClicks (false, true);

    const std::pair<const char*, const char*> defs[] = {
        { wave, "WAVE" }, { unison, "UNISON" }, { detune, "DETUNE" }, { width, "WIDTH" }, { fm, "FM" }, { fmRatio, "RATIO" },
        { oct, "OCTAVE" }, { semi, "SEMI" }, { fine, "FINE" }, { penv, "P.ENV" }, { pdecay, "P.DECAY" }, { level, "LEVEL" } };
    for (auto& [base, name] : defs)
    {
        const String b (base);
        const bool bip = b == oct || b == semi || b == fine || b == penv;
        knobs.push_back (std::make_unique<Knob> (p.apvts, ParamIDs::osc (o, base), name, bip));
        content.addAndMakeVisible (*knobs.back());
    }
}

void OscPanel::resized()
{
    auto r = getLocalBounds().reduced (16, 10);
    auto head = r.removeFromTop (26);
    power.setBounds (head.removeFromRight (60));
    r.removeFromTop (8);
    content.setBounds (r);

    const int rowH = r.getHeight() / 2;
    const float cw = (float) r.getWidth() / 6.f;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int row = (int) i / 6, col = (int) i % 6;
        auto cell = Rectangle<int> ((int) (cw * (float) col), row * rowH, (int) cw, rowH);
        knobs[i]->setBounds (cell.withSizeKeepingCentre (jmin (cell.getWidth() - 2, 84), cell.getHeight() - 4));
    }
}

void OscPanel::refresh()
{
    const float a = power.isOn() ? 1.f : 0.32f;
    if (std::abs (content.getAlpha() - a) > 0.001f) content.setAlpha (a);
}

// ======================================================================= FxTabs
void FxTabs::Page::resized()
{
    auto r = getLocalBounds();
    if (seg != nullptr)
    {
        auto left = r.removeFromLeft (104);
        seg->setBounds (left.withSizeKeepingCentre (left.getWidth(), jmin (left.getHeight(), 30 * 5)));
        r.removeFromLeft (12);
    }
    const int n = (int) cells.size();
    if (n == 0) return;
    const float cw = (float) r.getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        auto cell = Rectangle<int> (r.getX() + (int) (cw * (float) i), r.getY(), (int) cw, r.getHeight());
        auto* c = cells[(size_t) i].get();
        Rectangle<int> b;
        if (dynamic_cast<Switch*> (c) != nullptr) b = cell.withSizeKeepingCentre (jmin (cell.getWidth() - 6, 86), 28);
        else                                      b = cell.withSizeKeepingCentre (jmin (cell.getWidth() - 2, 86), cell.getHeight());
        c->setBounds (b);
        if (i == overlaySlot && overlay != nullptr) overlay->setBounds (b);
    }
}

FxTabs::FxTabs (ProphecyProcessor& p, int o) : proc (p), osc (o)
{
    using namespace ParamIDs;
    auto id = [o] (const char* base) { return ParamIDs::osc (o, base); };
    auto knob = [&p, &id] (const char* base, const char* name, bool bip = false)
    { return std::make_unique<Knob> (p.apvts, id (base), name, bip); };

    // DIST
    pages[0].seg = std::make_unique<Segmented> (p.apvts, id (dtype), ParamChoices::distTypes);
    pages[0].cells.push_back (knob (drive, "DRIVE"));
    pages[0].cells.push_back (knob (bite, "BITE"));
    pages[0].cells.push_back (knob (noise, "NOISE"));
    pages[0].cells.push_back (knob (dmix, "MIX"));

    // FILTER
    pages[1].seg = std::make_unique<Segmented> (p.apvts, id (ftype), ParamChoices::filterTypes);
    pages[1].cells.push_back (knob (cutoff, "CUTOFF"));
    pages[1].cells.push_back (knob (reso, "RESO"));
    pages[1].cells.push_back (knob (fenv, "ENV", true));
    pages[1].cells.push_back (knob (fdecay, "DECAY"));
    pages[1].cells.push_back (knob (keytrack, "KEY"));

    // LFO
    pages[2].seg = std::make_unique<Segmented> (p.apvts, id (lshape), StringArray { "SINE", "TRI", "SAW", "SQUARE", "S&H" });
    pages[2].cells.push_back (std::make_unique<Switch> (p.apvts, id (lsync), "SYNC", Switch::Chip));
    pages[2].cells.push_back (knob (lfoDiv, "RATE"));
    pages[2].cells.push_back (knob (lcut, "> CUTOFF", true));
    pages[2].cells.push_back (knob (lpitch, "> PITCH", true));
    pages[2].cells.push_back (knob (lamp, "> AMP"));

    // FORMANT (v3): a resonant peak with its own envelope - the "voice" of a screech
    pages[3].cells.push_back (knob (formHz, "FREQ"));
    pages[3].cells.push_back (knob (formGain, "GAIN"));
    pages[3].cells.push_back (knob (formQ, "Q"));
    pages[3].cells.push_back (knob (formEnv, "ENV", true));
    pages[3].cells.push_back (knob (formDec, "DECAY"));
    pages[3].cells.push_back (knob (formKey, "KEY"));
    pages[3].cells.push_back (knob (formLfo, "LFO", true));
    auto rate = knob (lrate, "RATE");
    rateKnob = rate.get();
    divKnob = pages[2].cells[1].get();
    pages[2].overlay = rateKnob;
    pages[2].overlaySlot = 1;
    pages[2].overlayOwner = std::move (rate);
    pages[2].addChildComponent (*pages[2].overlayOwner);

    const char* onIds[] = { distOn, filtOn, lfoOn, formOn };
    for (int i = 0; i < numTabs; ++i)
    {
        auto& pg = pages[(size_t) i];
        if (pg.seg) pg.addAndMakeVisible (*pg.seg);
        for (auto& c : pg.cells) pg.addAndMakeVisible (*c);
        addChildComponent (pg);
        powers[(size_t) i] = std::make_unique<Switch> (p.apvts, id (onIds[i]), "ACTIVE", Switch::Power);
        addChildComponent (*powers[(size_t) i]);
        onFlags[(size_t) i] = p.apvts.getRawParameterValue (id (onIds[i]));
    }
    syncFlag = p.apvts.getRawParameterValue (id (lsync));
    pages[0].setVisible (true);
    powers[0]->setVisible (true);
    setMouseCursor (MouseCursor::NormalCursor);
}

Rectangle<int> FxTabs::tabBounds (int i) const
{
    static const int w[] = { 80, 94, 72, 112 };
    int x = 14;
    for (int k = 0; k < i; ++k) x += w[k] + 4;
    return { x, 8, w[i], 32 };
}

void FxTabs::resized()
{
    for (auto& s : powers) s->setBounds (getWidth() - 16 - 110, 12, 110, 24);
    auto body = getLocalBounds().reduced (16, 0).withTrimmedTop (54).withTrimmedBottom (12);
    for (auto& pg : pages) pg.setBounds (body);
}

void FxTabs::selectTab (int i)
{
    if (! isPositiveAndBelow (i, numTabs) || i == current) return;
    pages[(size_t) current].setVisible (false);
    powers[(size_t) current]->setVisible (false);
    current = i;
    pages[(size_t) current].setVisible (true);
    powers[(size_t) current]->setVisible (true);
    repaint();
}

void FxTabs::mouseDown (const MouseEvent& e)
{
    for (int i = 0; i < numTabs; ++i)
        if (tabBounds (i).contains (e.getPosition())) selectTab (i);
}

void FxTabs::mouseMove (const MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < numTabs; ++i) if (tabBounds (i).contains (e.getPosition())) h = i;
    setMouseCursor (h >= 0 ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
    if (h != hoverTab) { hoverTab = h; repaint(); }
}

void FxTabs::refresh()
{
    bool changed = false;
    for (int i = 0; i < numTabs; ++i)
    {
        const bool on = onFlags[(size_t) i]->load() > 0.5f;
        if (on != lastOn[(size_t) i]) { lastOn[(size_t) i] = on; changed = true; }
        const float a = on ? 1.f : 0.32f;
        if (std::abs (pages[(size_t) i].getAlpha() - a) > 0.001f) pages[(size_t) i].setAlpha (a);
    }
    if (changed) repaint (0, 0, getWidth(), 48);

    const bool sync = syncFlag->load() > 0.5f;
    if (divKnob->isVisible() != sync) divKnob->setVisible (sync);
    if (rateKnob->isVisible() == sync) rateKnob->setVisible (! sync);
}

void FxTabs::paint (Graphics& g)
{
    auto& lnf = stageLnf (*this);
    static const char* names[] = { "DIST", "FILTER", "LFO", "FORMANT" };
    for (int i = 0; i < numTabs; ++i)
    {
        auto t = tabBounds (i).toFloat();
        const bool sel = i == current;
        const bool on = lastOn[(size_t) i];

        if (sel)
        {
            g.setColour (Colours::white.withAlpha (0.045f));
            g.fillRoundedRectangle (t, 6.f);
            auto bar = Rectangle<float> (t.getX() + 12.f, t.getBottom() + 5.f, t.getWidth() - 24.f, 2.f);
            g.setColour (Palette::red.withAlpha (0.25f));
            g.fillRoundedRectangle (bar.expanded (2.f, 2.f), 3.f);
            g.setColour (Palette::red);
            g.fillRoundedRectangle (bar, 1.f);
        }
        else if (i == hoverTab)
        {
            g.setColour (Colours::white.withAlpha (0.03f));
            g.fillRoundedRectangle (t, 6.f);
        }

        auto led = Rectangle<float> (5.f, 5.f).withCentre ({ t.getX() + 14.f, t.getCentreY() });
        if (on)
        {
            g.setColour (Palette::red.withAlpha (0.45f));
            g.fillEllipse (led.expanded (3.f));
        }
        g.setColour (on ? Palette::hot : Colour (0xff3a3033));
        g.fillEllipse (led);

        g.setFont (lnf.titleFont (10.5f, 0.18f));
        g.setColour (sel ? Palette::text : (i == hoverTab ? Palette::value : Palette::label.withAlpha (0.8f)));
        g.drawText (names[i], t.withTrimmedLeft (22.f), Justification::centredLeft);
    }

    ColourGradient sep (Colours::white.withAlpha (0.0f), 16.f, 0.f, Colours::white.withAlpha (0.0f), (float) getWidth() - 16.f, 0.f, false);
    sep.addColour (0.5, Colours::white.withAlpha (0.09f));
    g.setGradientFill (sep);
    g.fillRect (16.f, 47.f, (float) getWidth() - 32.f, 1.f);
}

// ======================================================================= Scope
void Scope::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xbb060405));
    g.fillRoundedRectangle (r, 6.f);

    g.setColour (Colours::white.withAlpha (0.045f));
    for (int i = 1; i < 6; ++i) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / 6.f), r.getY() + 3.f, r.getBottom() - 3.f);
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) i / 4.f), r.getX() + 3.f, r.getRight() - 3.f);

    constexpr int N = ProphecyProcessor::scopeSize;
    constexpr int show = 420;
    std::array<float, N> local;
    const int w = proc.scopeWritePos.load (std::memory_order_acquire);
    for (int i = 0; i < N; ++i) local[(size_t) i] = proc.scopeBuffer[(size_t) ((w + i) & (N - 1))];

    int start = N - show - 1;
    for (int i = N - show - 1; i > N - show - 1 - 1500 && i > 1; --i)
        if (local[(size_t) i - 1] < 0.f && local[(size_t) i] >= 0.f) { start = i; break; }

    Path wave;
    const float midY = r.getCentreY(), amp = r.getHeight() * 0.42f;
    for (int i = 0; i < show; ++i)
    {
        const float xx = r.getX() + 3.f + (r.getWidth() - 6.f) * (float) i / (float) (show - 1);
        const float yy = midY - jlimit (-1.f, 1.f, local[(size_t) (start + i)]) * amp;
        if (i == 0) wave.startNewSubPath (xx, yy); else wave.lineTo (xx, yy);
    }

    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (r.toNearestInt());
    g.setColour (Palette::red.withAlpha (0.14f));
    g.strokePath (wave, PathStrokeType (7.f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Palette::red.withAlpha (0.4f));
    g.strokePath (wave, PathStrokeType (3.f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colour (0xffffd3d9));
    g.strokePath (wave, PathStrokeType (1.1f, PathStrokeType::curved, PathStrokeType::rounded));
}

// ======================================================================= Meter
void Meter::setLevel (float peak)
{
    const float db = Decibels::gainToDecibels (peak, -60.f);
    const float v = jlimit (0.f, 1.f, (db + 60.f) / 60.f);
    const float nl = v > level ? v : level * 0.88f + v * 0.12f;
    if (v >= hold) { hold = v; holdCount = 40; }
    else if (--holdCount < 0) hold = jmax (0.f, hold - 0.01f);
    if (std::abs (nl - level) > 0.002f || holdCount >= 0) { level = nl; repaint(); }
}

void Meter::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xbb060405));
    g.fillRoundedRectangle (r, 3.f);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r.reduced (0.5f), 3.f, 1.f);

    auto inner = r.reduced (3.f);
    const int segs = 24;
    const float sh = inner.getHeight() / (float) segs;
    for (int i = 0; i < segs; ++i)
    {
        const float t = (float) (i + 1) / (float) segs;
        auto seg = Rectangle<float> (inner.getX(), inner.getBottom() - sh * (float) (i + 1) + 1.f, inner.getWidth(), sh - 2.f);
        const bool lit = t <= level;
        const Colour c = t > 0.95f ? Colours::white : (t > 0.8f ? Palette::hot : Palette::red);
        g.setColour (lit ? c : Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (seg, 1.f);
        if (std::abs (t - hold) < 0.5f / (float) segs && hold > 0.02f)
        {
            g.setColour (Colours::white.withAlpha (0.8f));
            g.fillRoundedRectangle (seg, 1.f);
        }
    }
}

// ======================================================================= Canvas
Knob* Canvas::knob (const String& id, const String& label, bool bipolar, bool big)
{
    auto k = std::make_unique<Knob> (proc.apvts, id, label, bipolar, big);
    auto* raw = k.get();
    addAndMakeVisible (*raw);
    owned.push_back (std::move (k));
    return raw;
}

Switch* Canvas::toggle (const String& id, const String& label, Switch::Style st)
{
    auto t = std::make_unique<Switch> (proc.apvts, id, label, st);
    auto* raw = t.get();
    addAndMakeVisible (*raw);
    owned.push_back (std::move (t));
    return raw;
}

Canvas::Canvas (ProphecyProcessor& p) : proc (p), scope (p)
{
    setLookAndFeel (&lnf);
    using namespace ParamIDs;

    osc1 = std::make_unique<OscPanel> (p, 0);
    osc2 = std::make_unique<OscPanel> (p, 1);
    fx1 = std::make_unique<FxTabs> (p, 0);
    fx2 = std::make_unique<FxTabs> (p, 1);
    for (auto* c : { (Component*) osc1.get(), (Component*) osc2.get(), (Component*) fx1.get(), (Component*) fx2.get() })
        addAndMakeVisible (c);

    screamKnob = knob (scream, "", false, true);
    voiceCells = { knob (glide, "GLIDE"), knob (bend, "BEND"), toggle (legato, "LEGATO", Switch::Chip),
                   knob (sweep, "SWEEP", true), knob (sweepTime, "S.TIME") };
    ampCells = { knob (attack, "ATTACK"), knob (decay, "DECAY"), knob (sustain, "SUSTAIN"), knob (release, "RELEASE") };
    delayPower = toggle (dlyOn, "", Switch::Power);
    delayCells = { knob (dtime, "TIME"), knob (dfb, "FEEDBACK"), knob (dfx, "MIX"), toggle (pingpong, "P.PONG", Switch::Chip) };
    reverbPower = toggle (revOn, "", Switch::Power);
    reverbCells = { knob (rsize, "SIZE"), knob (rdamp, "DAMP"), knob (rmix, "MIX") };
    volumeKnob = knob (volume, "VOLUME");
    outCells = { knob (ott, "OTT"), knob (spread, "WIDTH"), volumeKnob };
    addAndMakeVisible (scope);
    addAndMakeVisible (meter);

    refreshPresetList();
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        if (id < userIdBase) proc.setCurrentProgram (id - 1);
        else if (isPositiveAndBelow (id - userIdBase, userPresetFiles.size())) proc.loadUserPreset (userPresetFiles[id - userIdBase]);
        lastPresetName = proc.getPresetName();
    };
    presetBox.setTooltip ("Presety fabryczne i Twoje zapisane presety");
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { showSaveDialog(); };
    randomButton.onClick = [this] { proc.randomize(); };
    saveButton.setTooltip ("Zapisz obecne brzmienie jako preset");
    randomButton.setTooltip ("Losowy screech w rozsadnych zakresach");
    for (auto* c : { (Component*) &presetBox, (Component*) &prevButton, (Component*) &nextButton,
                     (Component*) &saveButton, (Component*) &randomButton })
        addAndMakeVisible (c);

    setSize (W, H);
    timerCallback();
    startTimerHz (30);
}

Canvas::~Canvas()
{
    if (saveDialog != nullptr) saveDialog->exitModalState (0);
    saveDialog.reset();
    setLookAndFeel (nullptr);
}

void Canvas::refreshPresetList()
{
    presetBox.clear (dontSendNotification);
    presetBox.addSectionHeading ("FACTORY");
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);

    userPresetFiles = proc.getUserPresets();
    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("USER");
        for (int i = 0; i < userPresetFiles.size(); ++i)
            presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(), userIdBase + i);
    }
    lastPresetName = {};
}

void Canvas::stepPreset (int delta)
{
    Array<int> ids;
    for (int i = 0; i < presetBox.getNumItems(); ++i) ids.add (presetBox.getItemId (i));
    if (ids.isEmpty()) return;
    int at = ids.indexOf (presetBox.getSelectedId());
    at = at < 0 ? 0 : (at + delta + ids.size()) % ids.size();
    presetBox.setSelectedId (ids[at], sendNotificationSync);
}

void Canvas::showSaveDialog()
{
    if (saveDialog != nullptr) return;
    saveDialog = std::make_unique<AlertWindow> ("SAVE PRESET", "Nazwa presetu (Dokumenty / Prophecy of Scream / Presets):",
                                                MessageBoxIconType::NoIcon, this);
    saveDialog->setLookAndFeel (&lnf);
    saveDialog->addTextEditor ("name", proc.getPresetName().startsWith ("Random") ? String ("My Screech") : proc.getPresetName());
    saveDialog->addButton ("SAVE", 1, KeyPress (KeyPress::returnKey));
    saveDialog->addButton ("CANCEL", 0, KeyPress (KeyPress::escapeKey));

    SafePointer<Canvas> safe (this);
    saveDialog->enterModalState (true, ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr || safe->saveDialog == nullptr) return;
        const auto name = safe->saveDialog->getTextEditorContents ("name");
        MessageManager::callAsync ([safe] { if (safe != nullptr) safe->saveDialog.reset(); });
        if (result == 1)
        {
            if (safe->proc.saveUserPreset (name)) safe->refreshPresetList();
            else AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "SAVE PRESET", "Nie udalo sie zapisac presetu.");
        }
    }), false);
}

void Canvas::timerCallback()
{
    osc1->refresh(); osc2->refresh();
    fx1->refresh();  fx2->refresh();

    // fx sections dim when bypassed
    auto dimAll = [] (std::vector<Component*>& cells, bool on)
    {
        const float a = on ? 1.f : 0.32f;
        for (auto* c : cells) if (std::abs (c->getAlpha() - a) > 0.001f) c->setAlpha (a);
    };
    dimAll (delayCells, delayPower->isOn());
    dimAll (reverbCells, reverbPower->isOn());

    // preset name
    const auto name = proc.getPresetName();
    if (name != lastPresetName)
    {
        lastPresetName = name;
        int found = 0;
        for (int i = 0; i < presetBox.getNumItems(); ++i)
            if (presetBox.getItemText (i) == name) { found = presetBox.getItemId (i); break; }
        if (found != 0) presetBox.setSelectedId (found, dontSendNotification);
        else            presetBox.setText (name, dontSendNotification);
    }

    // output level -> meter + title glow + scope
    const int w = proc.scopeWritePos.load (std::memory_order_acquire);
    if (w != lastScopePos)
    {
        constexpr int N = ProphecyProcessor::scopeSize;
        const int count = lastScopePos < 0 ? 1024 : jlimit (1, N, (w - lastScopePos + N) & (N - 1));
        float peak = 0.f;
        for (int i = 1; i <= count; ++i) peak = jmax (peak, std::abs (proc.scopeBuffer[(size_t) ((w - i + N) & (N - 1))]));
        lastScopePos = w;
        meter.setLevel (peak);
        scope.repaint();
        const float target = jlimit (0.f, 1.f, peak * 1.4f);
        glow = target > glow ? target : glow * 0.9f;
    }
    else
    {
        meter.setLevel (0.f);
        glow *= 0.9f;
    }
    if (std::abs (glow - lastGlow) > 0.01f) { lastGlow = glow; repaint (300, 0, W - 600, 100); }
}

void Canvas::resized()
{
    const int m = 20, gap = 14;
    const int colW = 526, midW = 240;
    const int x1 = m, xMid = m + colW + gap, x2 = xMid + midW + gap;

    panels.clear();
    panels.push_back ({ "OSC 1", { x1, 132, colW, 254 } });
    panels.push_back ({ "", { xMid, 132, midW, 254 }, true });
    panels.push_back ({ "OSC 2", { x2, 132, colW, 254 } });
    panels.push_back ({ "", { x1, 398, colW, 204 } });
    panels.push_back ({ "VOICE", { xMid, 398, midW, 204 } });
    panels.push_back ({ "", { 0, 0, 0, 0 } });             // (v2 scope slot - the scope now sits in the SCREAM panel)
    panels.push_back ({ "", { x2, 398, colW, 204 } });

    osc1->setBounds (panels[0].area);
    osc2->setBounds (panels[2].area);
    fx1->setBounds (panels[3].area);
    fx2->setBounds (panels[6].area);

    // SCREAM (+ the scope along its bottom edge)
    {
        auto a = panels[1].area.reduced (14, 10);
        a.removeFromTop (22);
        scope.setBounds (a.removeFromBottom (34).reduced (4, 2));
        a.removeFromBottom (4);
        screamKnob->setBounds (a);
    }
    // VOICE: GLIDE BEND LEGATO / SWEEP S.TIME
    {
        auto a = panels[4].area.reduced (12, 6);
        a.removeFromTop (28);
        auto row1 = a.removeFromTop (a.getHeight() / 2);
        auto chip = row1.removeFromRight (94);
        voiceCells[2]->setBounds (chip.withSizeKeepingCentre (chip.getWidth() - 4, 28));
        const int cw = row1.getWidth() / 2;
        voiceCells[0]->setBounds (row1.removeFromLeft (cw));
        voiceCells[1]->setBounds (row1);
        const int cw2 = a.getWidth() / 2;
        voiceCells[3]->setBounds (a.removeFromLeft (cw2).reduced (8, 0));
        voiceCells[4]->setBounds (a.reduced (8, 0));
    }

    // bottom row
    const int by = 616, bh = 164;
    const int ampW = 340, dlyW = 380, revW = 300;
    const int outW = W - 2 * m - 3 * gap - ampW - dlyW - revW;
    Rectangle<int> ampA (m, by, ampW, bh), dlyA (ampA.getRight() + gap, by, dlyW, bh),
                   revA (dlyA.getRight() + gap, by, revW, bh), outA (revA.getRight() + gap, by, outW, bh);
    panels.push_back ({ "AMP", ampA });
    panels.push_back ({ "DELAY", dlyA });
    panels.push_back ({ "REVERB", revA });
    panels.push_back ({ "OUTPUT", outA });

    auto placeRow = [] (Rectangle<int> area, std::vector<Component*>& cells)
    {
        area = area.reduced (14, 10);
        area.removeFromTop (30);
        const float cw = (float) area.getWidth() / (float) cells.size();
        for (size_t i = 0; i < cells.size(); ++i)
        {
            auto cell = Rectangle<int> (area.getX() + (int) (cw * (float) i), area.getY(), (int) cw, area.getHeight());
            if (dynamic_cast<Switch*> (cells[i]) != nullptr) cells[i]->setBounds (cell.withSizeKeepingCentre (jmin (cell.getWidth() - 6, 92), 28));
            else cells[i]->setBounds (cell.withSizeKeepingCentre (jmin (cell.getWidth() - 2, 86), cell.getHeight()));
        }
    };
    placeRow (ampA, ampCells);
    placeRow (dlyA, delayCells);
    placeRow (revA, reverbCells);
    delayPower->setBounds (dlyA.getRight() - 16 - 60, dlyA.getY() + 10, 60, 24);
    reverbPower->setBounds (revA.getRight() - 16 - 60, revA.getY() + 10, 60, 24);
    {
        auto a = outA.reduced (14, 10);
        a.removeFromTop (30);
        meter.setBounds (a.removeFromRight (16).reduced (0, 4));
        a.removeFromRight (8);
        const float cw = (float) a.getWidth() / (float) outCells.size();
        for (size_t i = 0; i < outCells.size(); ++i)
            outCells[i]->setBounds (Rectangle<int> (a.getX() + (int) (cw * (float) i), a.getY(), (int) cw, a.getHeight()).reduced (1, 0));
    }

    // header
    const int px = W - m - 290;
    prevButton.setBounds (px, 40, 32, 32);
    presetBox.setBounds (px + 38, 40, 214, 32);
    nextButton.setBounds (px + 258, 40, 32, 32);
    randomButton.setBounds (px, 80, 180, 28);
    saveButton.setBounds (px + 188, 80, 102, 28);

    renderBackground();
}

// ----------------------------------------------------------------------- background (cached)
static void drawBeam (Graphics& g, Point<float> src, float angle, float spread, float len, Colour col, float alpha)
{
    for (int layer = 0; layer < 4; ++layer)
    {
        const float s = spread * (1.f - 0.22f * (float) layer);
        const float a = alpha * (0.35f + 0.25f * (float) layer);
        const auto dir = Point<float> (std::sin (angle), std::cos (angle));
        const auto perp = Point<float> (dir.y, -dir.x);
        const auto end = src + dir * len;
        const float halfW = std::tan (s) * len;
        Path p;
        p.startNewSubPath (src - perp * (1.5f + 0.5f * (float) layer));
        p.lineTo (src + perp * (1.5f + 0.5f * (float) layer));
        p.lineTo (end + perp * halfW);
        p.lineTo (end - perp * halfW);
        p.closeSubPath();
        ColourGradient grad (col.withAlpha (a), src.x, src.y, col.withAlpha (0.f), end.x, end.y, false);
        grad.addColour (0.25, col.withAlpha (a * 0.6f));
        g.setGradientFill (grad);
        g.fillPath (p);
    }
}

void Canvas::drawStage (Graphics& g)
{
    g.fillAll (Palette::bg);

    // LED wall behind the header
    {
        Random r (11);
        for (int y = 4; y < 190; y += 7)
            for (int x = 4; x < W; x += 7)
            {
                const float dx = ((float) x - W * 0.5f) / (W * 0.5f);
                const float fall = 1.f - (float) y / 190.f;
                const float a = 0.11f * fall * (1.f - 0.6f * dx * dx) * (0.6f + 0.4f * r.nextFloat());
                g.setColour (Palette::red.withAlpha (jmax (0.f, a)));
                g.fillRect ((float) x, (float) y, 2.f, 2.f);
            }
    }

    // haze from the top
    {
        ColourGradient haze (Colour (0xff6c0815).withAlpha (0.85f), W * 0.5f, -40.f, Colours::transparentBlack, W * 0.5f, 520.f, true);
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::scale (1.7f, 1.f, W * 0.5f, 0.f));
        g.setGradientFill (haze);
        g.fillRect (-400, 0, W + 800, H);
    }

    // lighting rig: two side trusses + a centre spot on the SCREAM knob
    const Colour beamRed (0xffff2a45), beamWhite (0xffffd9de);
    const Point<float> left (110.f, -20.f), right (W - 110.f, -20.f);
    const float angles[] = { -0.55f, -0.32f, -0.12f, 0.12f };
    for (int i = 0; i < 4; ++i)
    {
        drawBeam (g, left, -angles[i] - 0.22f, 0.055f, 1100.f, i == 2 ? beamWhite : beamRed, i == 2 ? 0.045f : 0.07f);
        drawBeam (g, right, angles[i] + 0.22f, 0.055f, 1100.f, i == 2 ? beamWhite : beamRed, i == 2 ? 0.045f : 0.07f);
    }
    drawBeam (g, { W * 0.5f, -60.f }, 0.f, 0.11f, 520.f, beamWhite, 0.07f);
    drawBeam (g, { W * 0.5f, -60.f }, 0.f, 0.05f, 520.f, beamRed, 0.10f);

    // floor glow + smoke
    {
        ColourGradient floor (Palette::red.withAlpha (0.22f), W * 0.5f, H + 120.f, Colours::transparentBlack, W * 0.5f, H - 260.f, true);
        g.setGradientFill (floor);
        g.fillRect (0, H - 360, W, 360);
        Random r (5);
        for (int i = 0; i < 46; ++i)
        {
            const float x = r.nextFloat() * W, y = H - 40.f - r.nextFloat() * 420.f;
            const float rad = 80.f + r.nextFloat() * 220.f;
            ColourGradient puff (Colour (0xffb01530).withAlpha (0.025f + 0.03f * r.nextFloat()), x, y,
                                 Colours::transparentBlack, x + rad, y, true);
            g.setGradientFill (puff);
            g.fillEllipse (x - rad, y - rad * 0.45f, rad * 2.f, rad * 0.9f);
        }
    }

    // vignette
    {
        ColourGradient vig (Colours::transparentBlack, W * 0.5f, H * 0.45f, Colours::black.withAlpha (0.75f), 0.f, -60.f, true);
        g.setGradientFill (vig);
        g.fillRect (0, 0, W, H);
    }

    // header horizon line
    {
        ColourGradient line (Palette::red.withAlpha (0.f), 0.f, 0.f, Palette::red.withAlpha (0.f), (float) W, 0.f, false);
        line.addColour (0.5, Palette::red.withAlpha (0.85f));
        line.addColour (0.25, Palette::red.withAlpha (0.25f));
        line.addColour (0.75, Palette::red.withAlpha (0.25f));
        g.setGradientFill (line);
        g.fillRect (0.f, 118.f, (float) W, 1.f);
        ColourGradient bloom (Palette::red.withAlpha (0.18f), W * 0.5f, 118.f, Palette::red.withAlpha (0.f), W * 0.5f + 360.f, 118.f, true);
        g.setGradientFill (bloom);
        g.fillRect (0.f, 110.f, (float) W, 16.f);
    }

    // header side text
    g.setFont (lnf.titleFont (10.f, 0.25f));
    g.setColour (Palette::text.withAlpha (0.75f));
    g.drawText ("V3.0", (int) doomsday::brand::headerTextX (1156.f / (float) W), 34, 300, 14, Justification::left);
    g.setFont (lnf.labelFont (12.f, 0.28f));
    g.setColour (Palette::label);
    g.drawText ("TWIN OSCILLATOR ENGINE", (int) doomsday::brand::headerTextX (1156.f / (float) W), 52, 320, 16, Justification::left);
    g.setColour (Palette::dim);
    g.drawText ("4X OVERSAMPLED  /  MONO LEGATO", (int) doomsday::brand::headerTextX (1156.f / (float) W), 68, 320, 16, Justification::left);
    g.setColour (Palette::red);
    g.drawText ("ENTER THE PROPHECY", (int) doomsday::brand::headerTextX (1156.f / (float) W), 84, 320, 16, Justification::left);
    doomsday::brand::drawHeaderBadge (g, (1156.f / (float) W), 0.55f);

    g.setFont (lnf.labelFont (11.f, 0.35f));
    g.setColour (Palette::dim);
    g.drawText ("PRESET", W - 20 - 290 + 38, 22, 214, 14, Justification::centred);

    g.setFont (lnf.labelFont (12.5f, 0.62f));
    g.setColour (Palette::red);
    g.drawText ("HARDSTYLE  SCREECH  SYNTHESIZER", 0, 84, W, 18, Justification::centred);

    for (auto& p : panels) if (! p.area.isEmpty()) drawGlassPanel (g, p);

    // the SCREAM panel title sits centred
    {
        auto a = panels[1].area;
        g.setFont (lnf.titleFont (11.f, 0.4f));
        g.setColour (Palette::text.withAlpha (0.9f));
        g.drawText ("THE SCREAM", a.getX(), a.getY() + 12, a.getWidth(), 16, Justification::centred);
    }
}

void Canvas::drawGlassPanel (Graphics& g, const Panel& pnl)
{
    auto r = pnl.area.toFloat();
    const float rad = 12.f;
    Path shape;
    shape.addRoundedRectangle (r, rad);

    DropShadow (Colours::black.withAlpha (0.6f), 26, { 0, 10 }).drawForPath (g, shape);

    g.setColour (Colour (0xff0a0608).withAlpha (0.74f));
    g.fillPath (shape);

    ColourGradient sheen (Colours::white.withAlpha (0.06f), 0.f, r.getY(), Colours::white.withAlpha (0.012f), 0.f, r.getBottom(), false);
    g.setGradientFill (sheen);
    g.fillPath (shape);

    {   // stage light hitting the top edge
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (shape);
        ColourGradient spill (Palette::red.withAlpha (pnl.spot ? 0.28f : 0.12f), r.getCentreX(), r.getY() - 10.f,
                              Palette::red.withAlpha (0.f), r.getCentreX(), r.getY() + (pnl.spot ? 210.f : 90.f), true);
        g.setGradientFill (spill);
        g.fillRect (r);
    }

    ColourGradient border (Palette::red.withAlpha (0.55f), r.getCentreX(), r.getY(), Colours::white.withAlpha (0.07f), r.getCentreX(), r.getY() + 60.f, false);
    g.setGradientFill (border);
    g.strokePath (shape, PathStrokeType (1.f));

    ColourGradient hl (Colours::white.withAlpha (0.f), r.getX() + rad, 0.f, Colours::white.withAlpha (0.f), r.getRight() - rad, 0.f, false);
    hl.addColour (0.5, Colours::white.withAlpha (0.28f));
    g.setGradientFill (hl);
    g.fillRect (r.getX() + rad, r.getY() + 0.5f, r.getWidth() - rad * 2.f, 1.f);

    if (pnl.title.isNotEmpty())
    {
        g.setColour (Palette::red);
        g.fillRoundedRectangle (r.getX() + 16.f, r.getY() + 16.f, 3.f, 11.f, 1.5f);
        g.setFont (lnf.titleFont (11.f, 0.3f));
        g.setColour (Palette::text.withAlpha (0.92f));
        g.drawText (pnl.title, (int) r.getX() + 26, (int) r.getY() + 13, 260, 17, Justification::centredLeft);
    }
}

void Canvas::renderBackground()
{
    const float scale = 2.f;
    background = Image (Image::RGB, (int) (W * scale), (int) (H * scale), true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (scale));
    drawStage (g);
}

void Canvas::paint (Graphics& g)
{
    if (background.isValid())
        g.drawImage (background, getLocalBounds().toFloat(), RectanglePlacement::stretchToFit);

    // title with a glow that breathes with the output level
    const auto titleArea = Rectangle<float> (0.f, 22.f, (float) W, 56.f);
    const String title ("PROPHECY OF SCREAM");
    g.setFont (lnf.titleFont (38.f, 0.2f));
    const float a = 0.10f + 0.22f * glow;
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (Palette::red.withAlpha (a / (float) i));
        const float o = (float) i * 1.6f;
        g.drawText (title, titleArea.translated (-o, 0.f), Justification::centred);
        g.drawText (title, titleArea.translated (o, 0.f), Justification::centred);
        g.drawText (title, titleArea.translated (0.f, o), Justification::centred);
        g.drawText (title, titleArea.translated (0.f, -o), Justification::centred);
    }
    ColourGradient tg (Colours::white, 0.f, titleArea.getY() + 12.f, Colour (0xffffb9c3), 0.f, titleArea.getBottom() - 10.f, false);
    g.setGradientFill (tg);
    g.drawText (title, titleArea, Justification::centred);
}

// ======================================================================= Editor
ProphecyEditor::ProphecyEditor (ProphecyProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (816, 480, 2040, 1200);
    getConstrainer()->setFixedAspectRatio ((double) Canvas::W / (double) Canvas::H);
    setSize (1156, 680);
}

void ProphecyEditor::resized()
{
    canvas.setBounds (0, 0, Canvas::W, Canvas::H);
    canvas.setTransform (AffineTransform::scale ((float) getWidth() / (float) Canvas::W));
}
