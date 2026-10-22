#include "DoomsdayUI.h"
#include "BinaryData.h"

using namespace juce;

namespace doomsday::ui
{
// ======================================================================= LookAndFeel
StageLookAndFeel::StageLookAndFeel (Theme t) : theme (std::move (t))
{
    tfTitle = Typeface::createSystemTypefaceFor (BinaryData::MichromaRegular_ttf, BinaryData::MichromaRegular_ttfSize);
    tfLabel = Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedSemiBold_ttf, BinaryData::BarlowSemiCondensedSemiBold_ttfSize);
    tfValue = Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedMedium_ttf, BinaryData::BarlowSemiCondensedMedium_ttfSize);
    tfBold  = Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedBold_ttf, BinaryData::BarlowSemiCondensedBold_ttfSize);

    const auto& a = theme.accent (theme.uiAccent);
    setColour (PopupMenu::backgroundColourId, Colour (0xf20a0a0b));
    setColour (PopupMenu::textColourId, theme.value);
    setColour (PopupMenu::headerTextColourId, a.main);
    setColour (PopupMenu::highlightedBackgroundColourId, a.deep.withAlpha (0.9f));
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (ComboBox::textColourId, theme.text);
    setColour (ComboBox::arrowColourId, a.main);
    setColour (Label::textColourId, theme.text);
    setColour (TextEditor::backgroundColourId, Colour (0xff0e0f10));
    setColour (TextEditor::textColourId, theme.text);
    setColour (TextEditor::highlightColourId, a.deep);
    setColour (TextEditor::outlineColourId, Colours::transparentBlack);
    setColour (TextEditor::focusedOutlineColourId, a.main);
    setColour (CaretComponent::caretColourId, a.main);
    setColour (AlertWindow::backgroundColourId, Colour (0xff0b0b0c));
    setColour (AlertWindow::textColourId, theme.text);
    setColour (AlertWindow::outlineColourId, a.main.withAlpha (0.6f));
    setColour (TextButton::textColourOffId, theme.text);
    setColour (TextButton::textColourOnId, Colours::white);
    setColour (TooltipWindow::backgroundColourId, Colour (0xff0e0f10));
    setColour (TooltipWindow::textColourId, theme.value);
    setColour (TooltipWindow::outlineColourId, Colours::white.withAlpha (0.12f));
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
StageLookAndFeel& lnfOf (Component& c) { return dynamic_cast<StageLookAndFeel&> (c.getLookAndFeel()); }

void StageLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, Slider& s)
{
    const bool big     = (bool) s.getProperties()["big"];
    const bool bipolar = (bool) s.getProperties()["bipolar"];
    const auto& acc    = theme.accent ((int) s.getProperties()["accent"]);
    const bool hover   = s.isMouseOverOrDragging() && s.isEnabled();

    auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (big ? 6.f : 3.f);
    const float size = jmin (bounds.getWidth(), bounds.getHeight());
    bounds = bounds.withSizeKeepingCentre (size, size);
    const auto c = bounds.getCentre();
    const float r = size * 0.5f;
    const float ringW = big ? 5.f : 2.6f;
    const float ringR = r - ringW * (big ? 2.2f : 1.6f);
    const float angle = startAngle + pos * (endAngle - startAngle);

    if (big)
    {
        const int ticks = 60;
        for (int i = 0; i <= ticks; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * (float) i / (float) ticks;
            const float t = (float) i / (float) ticks;
            Colour tc = acc.main;
            if (theme.crown.size() >= 2)
            {
                const float f = t * (float) (theme.crown.size() - 1);
                const int k = jmin ((int) f, (int) theme.crown.size() - 2);
                tc = theme.crown[(size_t) k].interpolatedWith (theme.crown[(size_t) k + 1], f - (float) k);
            }
            const float len = i % 5 == 0 ? 8.f : 3.5f;
            g.setColour (a <= angle + 0.001f ? tc.withAlpha (0.6f + 0.4f * t) : Colours::white.withAlpha (0.08f));
            g.drawLine ({ c.getPointOnCircumference (r - 1.f, a), c.getPointOnCircumference (r - 1.f - len, a) }, i % 5 == 0 ? 1.6f : 1.1f);
        }
    }

    Path track;
    track.addCentredArc (c.x, c.y, ringR, ringR, 0.f, startAngle, endAngle, true);
    g.setColour (Colours::white.withAlpha (0.075f));
    g.strokePath (track, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));

    // modulation ring (Serum / Vital style): range of the modulation outside the track + where it is right now
    {
        const float md = (float) (double) s.getProperties().getWithDefault ("modDepth", 0.0);
        if (std::abs (md) > 0.001f)
        {
            const float mr = ringR + ringW * (big ? 1.6f : 1.5f);
            const float a2 = jlimit (startAngle, endAngle, angle + md * (endAngle - startAngle));
            Path mp; mp.addCentredArc (c.x, c.y, mr, mr, 0.f, jmin (angle, a2), jmax (angle, a2), true);
            g.setColour (acc.hot.withAlpha (0.25f)); g.strokePath (mp, PathStrokeType (ringW * 1.8f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (acc.hot.withAlpha (0.85f)); g.strokePath (mp, PathStrokeType (jmax (1.f, ringW * 0.55f), PathStrokeType::curved, PathStrokeType::rounded));
            const float now = (float) (double) s.getProperties().getWithDefault ("modNow", -1.0);
            if (now >= 0.f)
            {
                const auto p = c.getPointOnCircumference (mr, startAngle + jlimit (0.f, 1.f, now) * (endAngle - startAngle));
                g.setColour (Colours::white); g.fillEllipse (Rectangle<float> (ringW * 1.3f, ringW * 1.3f).withCentre (p));
            }
        }
    }

    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.002f)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, ringR, ringR, 0.f, jmin (from, angle), jmax (from, angle), true);
        g.setColour (acc.main.withAlpha (hover ? 0.20f : 0.12f));
        g.strokePath (arc, PathStrokeType (ringW * 4.5f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (acc.main.withAlpha (hover ? 0.40f : 0.28f));
        g.strokePath (arc, PathStrokeType (ringW * 2.2f, PathStrokeType::curved, PathStrokeType::rounded));
        ColourGradient grad (acc.deep, c.x - ringR, c.y + ringR, acc.hot, c.x + ringR, c.y - ringR, false);
        grad.addColour (0.45, acc.main);
        g.setGradientFill (grad);
        g.strokePath (arc, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));
        const auto tip = c.getPointOnCircumference (ringR, angle);
        g.setColour (acc.hot.withAlpha (0.35f));
        g.fillEllipse (Rectangle<float> (ringW * 3.f, ringW * 3.f).withCentre (tip));
        g.setColour (Colours::white);
        g.fillEllipse (Rectangle<float> (ringW * 0.9f, ringW * 0.9f).withCentre (tip));
    }

    const float bodyR = ringR - ringW - (big ? 14.f : 5.5f);
    const auto body = Rectangle<float> (bodyR * 2.f, bodyR * 2.f).withCentre (c);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.fillEllipse (body.translated (0.f, big ? 5.f : 2.5f).expanded (big ? 4.f : 1.5f));
    ColourGradient bg (theme.knobLight, c.x - bodyR * 0.35f, c.y - bodyR * 0.65f, theme.knobDark, c.x + bodyR * 0.4f, c.y + bodyR * 1.05f, true);
    g.setGradientFill (bg);
    g.fillEllipse (body);
    ColourGradient rim (Colours::white.withAlpha (0.22f), c.x, body.getY(), Colours::black.withAlpha (0.6f), c.x, body.getBottom(), false);
    g.setGradientFill (rim);
    g.drawEllipse (body.reduced (0.5f), 1.f);
    if (big)
    {
        for (int i = 1; i <= 3; ++i)
        {
            g.setColour (Colours::black.withAlpha (0.35f));
            g.drawEllipse (body.reduced (bodyR * 0.16f * (float) i), 1.f);
            g.setColour (Colours::white.withAlpha (0.035f));
            g.drawEllipse (body.reduced (bodyR * 0.16f * (float) i).translated (0.f, 1.f), 1.f);
        }
        g.setColour (acc.main.withAlpha (0.10f + 0.3f * pos));
        g.fillEllipse (body.reduced (bodyR * 0.62f));
    }
    else
    {
        g.setColour (Colours::white.withAlpha (0.05f));
        g.fillEllipse (Rectangle<float> (bodyR * 0.9f, bodyR * 0.5f).withCentre ({ c.x - bodyR * 0.15f, c.y - bodyR * 0.45f }));
    }
    const auto p1 = c.getPointOnCircumference (bodyR * (big ? 0.55f : 0.42f), angle);
    const auto p2 = c.getPointOnCircumference (bodyR * 0.86f, angle);
    if (hover) { g.setColour (acc.main.withAlpha (0.55f)); g.drawLine ({ p1, p2 }, big ? 7.f : 4.5f); }
    g.setColour (s.isEnabled() ? Colours::white : theme.dim);
    g.drawLine ({ p1, p2 }, big ? 3.f : 1.8f);
}

Label* StageLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    const bool big = (bool) s.getProperties()["big"];
    l->setFont (big ? titleFont (15.f, 0.08f) : valueFont (13.f, 0.02f));
    l->setColour (Label::textColourId, big ? theme.text : theme.value);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setColour (Label::textWhenEditingColourId, theme.text);
    l->setColour (Label::outlineWhenEditingColourId, theme.accent (theme.uiAccent).main.withAlpha (0.6f));
    l->setColour (Label::backgroundWhenEditingColourId, Colour (0xff0e0f10));
    l->setJustificationType (Justification::centred);
    return l;
}

void StageLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    const auto& a = theme.accent (theme.uiAccent);
    auto r = Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (0.5f);
    g.setColour (Colour (0xcc080909));
    g.fillRoundedRectangle (r, 6.f);
    ColourGradient sheen (Colours::white.withAlpha (0.07f), 0.f, r.getY(), Colours::white.withAlpha (0.01f), 0.f, r.getBottom(), false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (r, 6.f);
    g.setColour (box.isMouseOver (true) ? a.main.withAlpha (0.8f) : Colours::white.withAlpha (0.13f));
    g.drawRoundedRectangle (r, 6.f, 1.f);
    Path chev;
    const float ax = (float) w - 16.f, ay = (float) h * 0.5f;
    chev.startNewSubPath (ax - 4.f, ay - 2.f); chev.lineTo (ax, ay + 2.f); chev.lineTo (ax + 4.f, ay - 2.f);
    g.setColour (a.main);
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
    g.setColour (theme.accent (theme.uiAccent).main.withAlpha (0.35f));
    g.fillRect (0, 0, w, 1);
    g.setColour (Colours::white.withAlpha (0.1f));
    g.drawRect (0, 0, w, h, 1);
}
void StageLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool hover, bool down)
{
    const auto& a = theme.accent (theme.uiAccent);
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getHeight() * 0.5f;
    g.setColour (down ? a.deep.withAlpha (0.8f) : Colour (0xcc080909));
    g.fillRoundedRectangle (r, rad);
    ColourGradient sheen (Colours::white.withAlpha (hover ? 0.10f : 0.06f), 0.f, r.getY(), Colours::white.withAlpha (0.f), 0.f, r.getBottom(), false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (r, rad);
    if (hover) { g.setColour (a.main.withAlpha (0.18f)); g.drawRoundedRectangle (r.expanded (1.5f), rad + 1.5f, 3.f); }
    g.setColour (hover ? a.main.withAlpha (0.9f) : Colours::white.withAlpha (0.14f));
    g.drawRoundedRectangle (r, rad, 1.f);
}
void StageLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool hover, bool)
{
    g.setFont (labelFont (b.getButtonText().length() <= 1 ? 15.f : 12.5f, 0.22f));
    g.setColour (hover ? Colours::white : theme.value);
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
}
Font StageLookAndFeel::getAlertWindowTitleFont()   { return titleFont (15.f, 0.15f); }
Font StageLookAndFeel::getAlertWindowMessageFont() { return valueFont (15.f); }
Font StageLookAndFeel::getTextButtonFont (TextButton&, int) { return labelFont (13.f, 0.2f); }

// ======================================================================= Knob
Knob::Knob (AudioProcessorValueTreeState& apvts, const String& paramId, const String& text, int accent, bool bipolar, bool isBig)
    : label (text), big (isBig)
{
    slider.getProperties().set ("bipolar", bipolar);
    slider.getProperties().set ("big", isBig);
    slider.getProperties().set ("accent", accent);
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, isBig ? 140 : 80, isBig ? 26 : 18);
    slider.setMouseDragSensitivity (isBig ? 360 : 240);
    slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
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
    auto& lnf = lnfOf (*this);
    g.setFont (lnf.labelFont (11.f, 0.14f));
    g.setColour (slider.isMouseOverOrDragging() ? lnf.theme.text : lnf.theme.label);
    g.drawText (label, labelArea, Justification::centred);
}
void Knob::resized()
{
    auto r = getLocalBounds();
    const int textH = big ? 26 : 18;
    const int labelH = label.isNotEmpty() ? 17 : 0;
    const int need = labelH + (big ? 1000 : maxDiameter) + textH;
    if (r.getHeight() > need) r = r.withSizeKeepingCentre (r.getWidth(), need);
    labelArea = r.removeFromTop (labelH);
    slider.setBounds (r);
}

// ======================================================================= Switch
Switch::Switch (AudioProcessorValueTreeState& apvts, const String& paramId, const String& t, Style s, int acc)
    : text (t), style (s), accent (acc)
{
    auto* p = apvts.getParameter (paramId);
    jassert (p != nullptr);
    attachment = std::make_unique<ParameterAttachment> (*p, [this] (float v) { state = v > 0.5f; repaint(); }, nullptr);
    attachment->sendInitialUpdate();
    setTooltip (p->getName (64));
    setMouseCursor (MouseCursor::PointingHandCursor);
}
void Switch::mouseDown (const MouseEvent&) { attachment->setValueAsCompleteGesture (state ? 0.f : 1.f); }
void Switch::paint (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    const auto& acc = lnf.theme.accent (accent);
    const bool hover = isMouseOver();
    auto r = getLocalBounds().toFloat();
    if (style == Power)
    {
        auto pill = Rectangle<float> (34.f, 18.f).withCentre ({ r.getRight() - 18.f, r.getCentreY() });
        if (text.isNotEmpty())
        {
            g.setFont (lnf.labelFont (11.f, 0.2f));
            g.setColour (state ? lnf.theme.text : lnf.theme.dim);
            g.drawText (text, r.withRight (pill.getX() - 8.f), Justification::centredRight);
        }
        if (state)
        {
            g.setColour (acc.main.withAlpha (0.25f));
            g.fillRoundedRectangle (pill.expanded (3.f), 12.f);
            ColourGradient gr (acc.hot, pill.getX(), pill.getY(), acc.deep, pill.getX(), pill.getBottom(), false);
            gr.addColour (0.5, acc.main);
            g.setGradientFill (gr);
        }
        else g.setColour (Colour (0xff17191a));
        g.fillRoundedRectangle (pill, 9.f);
        g.setColour (state ? acc.hot.withAlpha (0.8f) : Colours::white.withAlpha (hover ? 0.3f : 0.14f));
        g.drawRoundedRectangle (pill.reduced (0.5f), 9.f, 1.f);
        const float kx = state ? pill.getRight() - 9.f : pill.getX() + 9.f;
        auto knobR = Rectangle<float> (12.f, 12.f).withCentre ({ kx, pill.getCentreY() });
        g.setColour (Colours::black.withAlpha (0.4f)); g.fillEllipse (knobR.translated (0.f, 1.f));
        g.setColour (state ? Colours::white : Colour (0xff666b6a)); g.fillEllipse (knobR);
    }
    else
    {
        auto chip = r.reduced (1.f);
        const float rad = chip.getHeight() * 0.5f;
        g.setColour (state ? acc.deep.withAlpha (0.5f) : Colour (0xcc080909));
        g.fillRoundedRectangle (chip, rad);
        if (state) { g.setColour (acc.main.withAlpha (0.2f)); g.drawRoundedRectangle (chip.expanded (1.5f), rad + 1.5f, 3.f); }
        g.setColour (state ? acc.main.withAlpha (0.9f) : Colours::white.withAlpha (hover ? 0.3f : 0.14f));
        g.drawRoundedRectangle (chip, rad, 1.f);
        auto led = Rectangle<float> (6.f, 6.f).withCentre ({ chip.getX() + 13.f, chip.getCentreY() });
        if (state) { g.setColour (acc.main.withAlpha (0.45f)); g.fillEllipse (led.expanded (3.5f)); }
        g.setColour (state ? acc.hot : Colour (0xff2e3233));
        g.fillEllipse (led);
        g.setFont (lnf.labelFont (11.5f, 0.18f));
        g.setColour (state ? lnf.theme.text : lnf.theme.label);
        g.drawText (text, chip.withTrimmedLeft (20.f).withTrimmedRight (6.f), Justification::centred);
    }
}

// ======================================================================= Choice
Choice::Choice (AudioProcessorValueTreeState& apvts, const String& paramId, StringArray l, int acc, bool vert)
    : labels (std::move (l)), accent (acc), vertical (vert)
{
    auto* p = apvts.getParameter (paramId);
    jassert (p != nullptr);
    attachment = std::make_unique<ParameterAttachment> (*p, [this] (float v) { index = (int) std::round (v); repaint(); }, nullptr);
    attachment->sendInitialUpdate();
    setTooltip (p->getName (64));
    setMouseCursor (MouseCursor::PointingHandCursor);
}
Rectangle<float> Choice::itemBounds (int i) const
{
    auto r = getLocalBounds().toFloat();
    const float n = (float) labels.size();
    return vertical ? Rectangle<float> (r.getX(), r.getY() + r.getHeight() * (float) i / n, r.getWidth(), r.getHeight() / n).reduced (3.f, 2.f)
                    : Rectangle<float> (r.getX() + r.getWidth() * (float) i / n, r.getY(), r.getWidth() / n, r.getHeight()).reduced (2.f, 3.f);
}
int Choice::itemAt (Point<int> pt) const
{
    const float t = vertical ? (float) pt.y / (float) getHeight() : (float) pt.x / (float) getWidth();
    return jlimit (0, labels.size() - 1, (int) (t * (float) labels.size()));
}
void Choice::mouseDown (const MouseEvent& e) { attachment->setValueAsCompleteGesture ((float) itemAt (e.getPosition())); }
void Choice::mouseMove (const MouseEvent& e) { const int h = itemAt (e.getPosition()); if (h != hover) { hover = h; repaint(); } }
void Choice::paint (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    const auto& acc = lnf.theme.accent (accent);
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xaa060707));
    g.fillRoundedRectangle (r, 7.f);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r.reduced (0.5f), 7.f, 1.f);
    for (int i = 0; i < labels.size(); ++i)
    {
        auto item = itemBounds (i);
        const bool sel = i == index;
        if (sel)
        {
            g.setColour (acc.main.withAlpha (0.16f));
            g.fillRoundedRectangle (item.expanded (1.f), 5.f);
            ColourGradient gr (acc.deep.withAlpha (0.95f), item.getX(), item.getY(), acc.main.withAlpha (0.55f), item.getRight(), item.getBottom(), false);
            g.setGradientFill (gr);
            g.fillRoundedRectangle (item, 5.f);
            g.setColour (acc.hot.withAlpha (0.7f));
            g.drawRoundedRectangle (item, 5.f, 1.f);
        }
        else if (i == hover)
        {
            g.setColour (Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (item, 5.f);
        }
        g.setFont (lnf.labelFont (11.5f, 0.14f));
        g.setColour (sel ? Colours::white : (i == hover ? lnf.theme.text : lnf.theme.label));
        g.drawText (labels[i], item, Justification::centred);
    }
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
    auto& lnf = lnfOf (*this);
    const auto& a = lnf.theme.accent (0);
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xbb050606)); g.fillRoundedRectangle (r, 3.f);
    g.setColour (Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (r.reduced (0.5f), 3.f, 1.f);
    auto inner = r.reduced (3.f);
    const int segs = 22;
    const float sh = inner.getHeight() / (float) segs;
    for (int i = 0; i < segs; ++i)
    {
        const float t = (float) (i + 1) / (float) segs;
        auto seg = Rectangle<float> (inner.getX(), inner.getBottom() - sh * (float) (i + 1) + 1.f, inner.getWidth(), sh - 2.f);
        const Colour c = t > 0.95f ? Colours::white : t > 0.8f ? a.hot : t > 0.4f ? a.main : a.deep.brighter (0.4f);
        g.setColour (t <= level ? c : Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (seg, 1.f);
        if (std::abs (t - hold) < 0.5f / (float) segs && hold > 0.02f) { g.setColour (Colours::white.withAlpha (0.8f)); g.fillRoundedRectangle (seg, 1.f); }
    }
}

// ======================================================================= PresetBar
namespace
{
    String categoryOf (const String& n)
    {
        const int d = n.indexOf (" - ");
        return d > 0 && d <= 14 ? n.substring (0, d).trim().toUpperCase() : String ("OTHER");
    }

    // preset browser shown in a CallOutBox: search, categories, favourites, list; click = load (stays open to audition)
    class PresetBrowser : public Component, private ListBoxModel, private TextEditor::Listener
    {
    public:
        explicit PresetBrowser (PresetBar& b) : bar (b)
        {
            search.setTextToShowWhenEmpty ("SEARCH...", Colours::grey);
            search.addListener (this);
            addAndMakeVisible (search);
            folder = bar.presetHost().userPresetFolder();
            if (folder != File())
            {
                openFolder.setTooltip ("Open the folder with your own presets (" + folder.getFullPathName() + ")");
                openFolder.onClick = [this] { folder.createDirectory(); folder.startAsProcess(); };
                addAndMakeVisible (openFolder);
            }
            cats.add ("ALL"); cats.add ("FAVOURITES");
            for (auto& e : bar.entries()) if (! cats.contains (e.category)) cats.add (e.category);
            catModel.owner = this;              // BEFORE setModel: the ListBox asks the model for rows right away (FL crash, 2026-10-22)
            catList.setModel (&catModel); presetList.setModel (this);
            catList.setRowHeight (24); presetList.setRowHeight (24);
            for (auto* l : { &catList, &presetList }) { l->setColour (ListBox::backgroundColourId, Colour (0xff0a0a0c)); addAndMakeVisible (*l); }
            filter();
            setSize (460, 420);
            const int cur = bar.currentIndex();
            const int row = shown.indexOf (cur);
            if (row >= 0) { presetList.selectRow (row, false, true); presetList.scrollToEnsureRowIsOnscreen (row); }
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (8);
            auto top = r.removeFromTop (30);
            if (openFolder.isVisible()) { openFolder.setBounds (top.removeFromRight (150)); top.removeFromRight (6); }
            search.setBounds (top);
            r.removeFromTop (6);
            catList.setBounds (r.removeFromLeft (130));
            r.removeFromLeft (6);
            presetList.setBounds (r);
        }
        void paint (Graphics& g) override { g.fillAll (Colour (0xff050506)); }
    private:
        struct CatModel : public ListBoxModel
        {
            PresetBrowser* owner = nullptr;
            int getNumRows() override { return owner != nullptr ? owner->cats.size() : 0; }
            void paintListBoxItem (int row, Graphics& g, int w, int h, bool sel) override
            {
                auto& lnf = lnfOf (owner->bar);
                if (sel) { g.setColour (lnf.theme.accent (lnf.theme.uiAccent).main.withAlpha (0.25f)); g.fillRect (0, 0, w, h); }
                g.setColour (sel ? lnf.theme.text : lnf.theme.label); g.setFont (lnf.labelFont (12.f, 0.15f));
                g.drawText (owner->cats[row], 8, 0, w - 8, h, Justification::centredLeft);
            }
            void selectedRowsChanged (int row) override { if (row >= 0) { owner->cat = owner->cats[row]; owner->filter(); } }
        } catModel;
        int getNumRows() override { return shown.size(); }
        void paintListBoxItem (int row, Graphics& g, int w, int h, bool sel) override
        {
            if (! isPositiveAndBelow (row, shown.size())) return;
            auto& lnf = lnfOf (bar);
            const auto& e = bar.entries()[(size_t) shown[row]];
            const bool cur = shown[row] == bar.currentIndex();
            if (sel || cur) { g.setColour (lnf.theme.accent (lnf.theme.uiAccent).main.withAlpha (sel ? 0.3f : 0.15f)); g.fillRect (0, 0, w, h); }
            g.setColour (lnf.theme.text); g.setFont (lnf.valueFont (13.f));
            g.drawText ((e.factory < 0 ? "USER  " : "") + e.name, 8, 0, w - 34, h, Justification::centredLeft, true);
            g.setColour (bar.isFavourite (e.name) ? Colour (0xffffd34d) : Colours::white.withAlpha (0.15f));
            g.setFont (lnf.boldFont (15.f));
            g.drawText ("*", w - 26, 0, 20, h, Justification::centred);
        }
        void listBoxItemClicked (int row, const MouseEvent& e) override
        {
            if (! isPositiveAndBelow (row, shown.size())) return;
            if (e.x > presetList.getWidth() - 34) { bar.toggleFavourite (bar.entries()[(size_t) shown[row]].name); presetList.repaint(); return; }
            bar.load (shown[row]);
            presetList.repaint();
        }
        void returnKeyPressed (int row) override { if (isPositiveAndBelow (row, shown.size())) bar.load (shown[row]); }
        void textEditorTextChanged (TextEditor&) override { filter(); }
        void filter()
        {
            shown.clear();
            const auto q = search.getText().trim().toLowerCase();
            for (int i = 0; i < (int) bar.entries().size(); ++i)
            {
                const auto& e = bar.entries()[(size_t) i];
                if (cat == "FAVOURITES" && ! bar.isFavourite (e.name)) continue;
                if (cat != "ALL" && cat != "FAVOURITES" && e.category != cat) continue;
                if (q.isNotEmpty() && ! e.name.toLowerCase().contains (q)) continue;
                shown.add (i);
            }
            presetList.updateContent(); presetList.repaint();
        }
        PresetBar& bar;
        TextEditor search;
        TextButton openFolder { "MY PRESETS FOLDER" };
        File folder;
        ListBox catList, presetList;
        StringArray cats;
        String cat { "ALL" };
        Array<int> shown;
    };
}

PresetBar::PresetBar (PresetHost& h, bool play_, bool random_) : host (h), withPlay (play_), withRandom (random_)
{
    prev.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    save.onClick = [this] { showSave(); };
    random.onClick = [this] { host.randomizeSound(); };
    play.onPress = [this] (bool d) { host.audition (d); };
    undoBtn.onClick = [this] { if (auto* um = host.undoManager()) um->undo(); };
    redoBtn.onClick = [this] { if (auto* um = host.undoManager()) um->redo(); };
    save.setTooltip ("Save the current sound as a preset");
    random.setTooltip ("Random sound within musical ranges");
    play.setTooltip ("Hold to play");
    undoBtn.setTooltip ("Undo (Ctrl+Z)"); redoBtn.setTooltip ("Redo (Ctrl+Shift+Z / Ctrl+Y)");
    name.setTooltip ("Click: preset browser (search, categories, favourites). Mouse wheel: next / previous. *: favourite");
    for (auto* c : { (Component*) &name, (Component*) &prev, (Component*) &next, (Component*) &save })
        addAndMakeVisible (c);
    if (withRandom) addAndMakeVisible (random);
    if (withPlay) addAndMakeVisible (play);
    if (host.undoManager() != nullptr) { addAndMakeVisible (undoBtn); addAndMakeVisible (redoBtn); }
    favourites.addLines (favouritesFile().loadFileAsString());
    favourites.removeEmptyStrings();
    refreshList();
}
PresetBar::~PresetBar()
{
    if (keyTarget != nullptr) keyTarget->removeKeyListener (this);
    if (dialog != nullptr) dialog->exitModalState (0);
    dialog.reset();
}
void PresetBar::parentHierarchyChanged()
{
    auto* top = getTopLevelComponent();
    if (top == keyTarget.getComponent()) return;
    if (keyTarget != nullptr) keyTarget->removeKeyListener (this);
    keyTarget = top;
    if (top != nullptr && top != this) top->addKeyListener (this);
}
bool PresetBar::keyPressed (const KeyPress& k, Component*)
{
    auto* um = host.undoManager();
    if (um == nullptr || ! k.getModifiers().isCommandDown()) return false;
    const int c = k.getKeyCode();
    if ((c == 'Z' || c == 'z') && k.getModifiers().isShiftDown()) { um->redo(); return true; }
    if (c == 'Z' || c == 'z') { um->undo(); return true; }
    if (c == 'Y' || c == 'y') { um->redo(); return true; }
    return false;
}
void PresetBar::resized()
{
    const int w = getWidth();
    prev.setBounds (0, 18, 32, 32);
    name.setBounds (38, 18, w - 76, 32);
    next.setBounds (w - 32, 18, 32, 32);
    const bool undo = host.undoManager() != nullptr;
    int right = w;
    if (undo)
    {
        redoBtn.setBounds (w - 44, 58, 44, 28);
        undoBtn.setBounds (w - 94, 58, 46, 28);
        right = w - 100;
    }
    const int n = 1 + (withRandom ? 1 : 0) + (withPlay ? 1 : 0);
    const int bw = (right - 6 * (n - 1)) / n;
    int x = 0;
    if (withRandom) { random.setBounds (x, 58, n == 3 ? bw + 16 : bw, 28); x = random.getRight() + 6; }
    save.setBounds (x, 58, n == 3 ? bw - 8 : (n == 1 ? right : bw), 28); x = save.getRight() + 6;
    if (withPlay) play.setBounds (x, 58, right - x, 28);
}
void PresetBar::paint (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    g.setFont (lnf.labelFont (11.f, 0.35f));
    g.setColour (lnf.theme.dim);
    g.drawText ("PRESET", 38, 0, getWidth() - 76, 14, Justification::centred);
}
void PresetBar::Name::paint (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    auto r = getLocalBounds().toFloat();
    const auto& acc = lnf.theme.accent (lnf.theme.uiAccent);
    g.setColour (Colour (0xff0b0b0d)); g.fillRoundedRectangle (r, 6.f);
    g.setColour (acc.main.withAlpha (hover ? 0.7f : 0.35f)); g.drawRoundedRectangle (r.reduced (0.5f), 6.f, 1.f);
    g.setColour (lnf.theme.text); g.setFont (lnf.boldFont (14.f));
    g.drawText (text, r.withTrimmedLeft (10.f).withTrimmedRight (30.f).toNearestInt(), Justification::centred, true);
    g.setColour (bar.isFavourite (text) ? Colour (0xffffd34d) : Colours::white.withAlpha (0.2f));
    g.setFont (lnf.boldFont (17.f));
    g.drawText ("*", starArea().toNearestInt(), Justification::centred);
}
void PresetBar::Name::mouseDown (const MouseEvent& e)
{
    if (starArea().contains (e.position)) { bar.toggleFavourite (text); repaint(); return; }
    bar.openBrowser();
}
File PresetBar::favouritesFile() const
{
    return File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("Doomsday")
             .getChildFile (File::createLegalFileName (host.presetFolderText()) + " favourites.txt");
}
void PresetBar::toggleFavourite (const String& n)
{
    if (n.isEmpty()) return;
    if (favourites.contains (n)) favourites.removeString (n); else favourites.add (n);
    auto f = favouritesFile(); f.getParentDirectory().createDirectory();
    f.replaceWithText (favourites.joinIntoString ("\n"));
    name.repaint();
}
void PresetBar::refreshList()
{
    list.clear();
    for (int i = 0; i < host.numFactoryPresets(); ++i)
    {
        const auto n = host.factoryPresetName (i);
        list.push_back ({ n, categoryOf (n), i, {} });
    }
    for (auto& f : host.userPresetFiles()) list.push_back ({ f.getFileNameWithoutExtension(), "USER", -1, f });
    lastName = {};
}
int PresetBar::currentIndex() const
{
    const auto n = host.currentPresetName();
    for (int i = 0; i < (int) list.size(); ++i) if (list[(size_t) i].name == n) return i;
    return -1;
}
void PresetBar::load (int i)
{
    if (! isPositiveAndBelow (i, (int) list.size())) return;
    const auto& e = list[(size_t) i];
    if (e.factory >= 0) host.loadFactoryPreset (e.factory);
    else host.loadUserPresetFile (e.file);
    if (auto* um = host.undoManager()) um->beginNewTransaction();
    refresh();
}
void PresetBar::refresh()
{
    // one undo step per gesture: start a new transaction whenever no mouse button is held
    if (auto* um = host.undoManager())
        if (! ModifierKeys::currentModifiers.isAnyMouseButtonDown()) um->beginNewTransaction();
    const auto n = host.currentPresetName();
    if (n == lastName) return;
    lastName = n;
    name.text = n; name.repaint();
}
void PresetBar::step (int d)
{
    if (list.empty()) return;
    int at = currentIndex();
    at = at < 0 ? 0 : (at + d + (int) list.size()) % (int) list.size();
    load (at);
}
void PresetBar::openBrowser()
{
    refreshList();
    auto content = std::make_unique<PresetBrowser> (*this);
    auto* top = getTopLevelComponent();
    const auto area = top->getLocalArea (this, name.getBounds());
    CallOutBox::launchAsynchronously (std::move (content), area, top);
}
void PresetBar::showSave()
{
    if (dialog != nullptr) return;
    dialog = std::make_unique<AlertWindow> ("SAVE PRESET", "Preset name (" + host.presetFolderText() + "):", MessageBoxIconType::NoIcon, this);
    dialog->setLookAndFeel (&getLookAndFeel());
    dialog->addTextEditor ("name", host.currentPresetName().startsWith ("Random") ? String ("My Sound") : host.currentPresetName());
    dialog->addButton ("SAVE", 1, KeyPress (KeyPress::returnKey));
    dialog->addButton ("CANCEL", 0, KeyPress (KeyPress::escapeKey));
    SafePointer<PresetBar> safe (this);
    dialog->enterModalState (true, ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr || safe->dialog == nullptr) return;
        const auto name = safe->dialog->getTextEditorContents ("name");
        MessageManager::callAsync ([safe] { if (safe != nullptr) safe->dialog.reset(); });
        if (result == 1)
        {
            if (safe->host.saveUserPresetAs (name)) safe->refreshList();
            else AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "SAVE PRESET", "Could not save the preset.");
        }
    }), false);
}

void Knob::setModulation (float depth, float nowPos01)
{
    auto& pr = slider.getProperties();
    const double d = (double) depth, n = (double) nowPos01;
    if (std::abs ((double) pr.getWithDefault ("modDepth", 0.0) - d) < 1.0e-4 && std::abs ((double) pr.getWithDefault ("modNow", -1.0) - n) < 1.0e-3) return;
    pr.set ("modDepth", d); pr.set ("modNow", n);
    slider.repaint();
}

// ======================================================================= GainScope
void GainScope::push (float peakLin, float grDb)
{
    peak[(size_t) head] = jlimit (0.f, 2.f, peakLin);
    gr[(size_t) head] = jmin (0.f, grDb);
    head = (head + 1) % size;
    if (filled < size) ++filled;
    holdDb = jmin (holdDb, grDb);
    repaint();
}
void GainScope::paint (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff050506)); g.fillRoundedRectangle (b, 6.f);
    auto plot = b.reduced (6.f, 6.f);
    const auto& acc = lnf.theme.accent (accent);
    auto yOfDb = [&] (float db) { return plot.getY() + plot.getHeight() * jlimit (0.f, 1.f, -db / rangeDb); };
    // dB grid
    g.setFont (lnf.valueFont (9.5f));
    for (float d = -rangeDb / 4.f; d >= -rangeDb + 0.01f; d -= rangeDb / 4.f)
    {
        g.setColour (Colours::white.withAlpha (0.05f)); g.drawHorizontalLine ((int) yOfDb (d), plot.getX(), plot.getRight());
        g.setColour (lnf.theme.dim); g.drawText (String ((int) d), (int) plot.getRight() - 30, (int) yOfDb (d) - 7, 28, 12, Justification::centredRight);
    }
    if (filled < 2) return;
    const float dx = plot.getWidth() / (float) (size - 1);
    // signal: mirrored peak envelope (oldest left, newest right)
    Path wave; Path grLine;
    const int start = (head - filled + size) % size;
    const float cy = plot.getCentreY();
    for (int i = 0; i < filled; ++i)
    {
        const int k = (start + i) % size;
        const float x = plot.getRight() - (float) (filled - 1 - i) * dx;
        const float a = jmin (1.f, peak[(size_t) k]) * plot.getHeight() * 0.5f;
        if (i == 0) wave.startNewSubPath (x, cy - a); else wave.lineTo (x, cy - a);
        const float y = yOfDb (gr[(size_t) k]);
        if (i == 0) grLine.startNewSubPath (x, y); else grLine.lineTo (x, y);
    }
    for (int i = filled - 1; i >= 0; --i)
    {
        const int k = (start + i) % size;
        const float x = plot.getRight() - (float) (filled - 1 - i) * dx;
        wave.lineTo (x, cy + jmin (1.f, peak[(size_t) k]) * plot.getHeight() * 0.5f);
    }
    wave.closeSubPath();
    g.setColour (Colours::white.withAlpha (0.14f)); g.fillPath (wave);
    // gain reduction from the top (the louder it pushes, the deeper the line)
    Path grFill (grLine); grFill.lineTo (plot.getRight(), plot.getY()); grFill.lineTo (plot.getRight() - (float) (filled - 1) * dx, plot.getY()); grFill.closeSubPath();
    g.setColour (acc.main.withAlpha (0.18f)); g.fillPath (grFill);
    g.setColour (acc.main.withAlpha (0.25f)); g.strokePath (grLine, PathStrokeType (4.f));
    g.setColour (acc.hot); g.strokePath (grLine, PathStrokeType (1.4f));
    g.setFont (lnf.labelFont (10.f, 0.2f)); g.setColour (acc.hot);
    g.drawText ("MAX " + String (holdDb, 1) + " dB", plot.toNearestInt().removeFromTop (14), Justification::centredLeft);
}

// ======================================================================= FxTabs
void FxTabs::addTab (const String& name, const String& powerParamId, std::vector<std::unique_ptr<Component>> cells, std::unique_ptr<Component> side)
{
    auto pg = std::make_unique<Page>();
    pg->cells = std::move (cells);
    pg->side = std::move (side);
    if (pg->side) pg->addAndMakeVisible (*pg->side);
    for (auto& c : pg->cells) pg->addAndMakeVisible (*c);
    addChildComponent (*pg);
    auto pw = std::make_unique<Switch> (apvts, powerParamId, "ACTIVE", Switch::Power, accent);
    addChildComponent (*pw);
    onFlags.push_back (apvts.getRawParameterValue (powerParamId));
    lastOn.push_back (false);
    names.add (name);
    if (pages.empty()) { pg->setVisible (true); pw->setVisible (true); }
    pages.push_back (std::move (pg));
    powers.push_back (std::move (pw));
    resized();
}

void FxTabs::Page::resized()
{
    auto r = getLocalBounds();
    if (side != nullptr)
    {
        auto left = r.removeFromLeft (96);
        side->setBounds (left.withSizeKeepingCentre (left.getWidth(), jmin (left.getHeight(), 104)));
        r.removeFromLeft (10);
    }
    const int n = (int) cells.size();
    if (n == 0) return;
    const float cw = (float) jmin (r.getWidth(), n * 96) / (float) n;
    for (int i = 0; i < n; ++i)
    {
        auto cell = Rectangle<int> (r.getX() + (int) (cw * (float) i), r.getY(), (int) cw, r.getHeight());
        auto* c = cells[(size_t) i].get();
        if (dynamic_cast<Switch*> (c) != nullptr) c->setBounds (cell.withSizeKeepingCentre (jmin (cell.getWidth() - 6, 90), 28));
        else                                      c->setBounds (cell.withSizeKeepingCentre (jmin (cell.getWidth() - 2, 86), cell.getHeight()));
    }
}

Rectangle<int> FxTabs::tabBounds (int i) const { return { 12 + i * tabWidth, 8, tabWidth - 4, 32 }; }

void FxTabs::resized()
{
    for (auto& s : powers) s->setBounds (getWidth() - 14 - 104, 12, 104, 24);
    auto body = getLocalBounds().reduced (16, 0).withTrimmedTop (54).withTrimmedBottom (10);
    for (auto& pg : pages) pg->setBounds (body);
}

void FxTabs::selectTab (int i)
{
    if (! isPositiveAndBelow (i, (int) pages.size()) || i == current) return;
    pages[(size_t) current]->setVisible (false); powers[(size_t) current]->setVisible (false);
    current = i;
    pages[(size_t) current]->setVisible (true);  powers[(size_t) current]->setVisible (true);
    repaint();
}

void FxTabs::mouseDown (const MouseEvent& e)
{
    for (int i = 0; i < (int) pages.size(); ++i) if (tabBounds (i).contains (e.getPosition())) selectTab (i);
}

void FxTabs::mouseMove (const MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < (int) pages.size(); ++i) if (tabBounds (i).contains (e.getPosition())) h = i;
    setMouseCursor (h >= 0 ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
    if (h != hoverTab) { hoverTab = h; repaint(); }
}

void FxTabs::refresh()
{
    bool changed = false;
    for (size_t i = 0; i < pages.size(); ++i)
    {
        const bool on = onFlags[i]->load() > 0.5f;
        if (on != lastOn[i]) { lastOn[i] = on; changed = true; }
        const float a = on ? 1.f : 0.32f;
        if (std::abs (pages[i]->getAlpha() - a) > 0.001f) pages[i]->setAlpha (a);
    }
    if (changed) repaint (0, 0, getWidth(), 48);
}

void FxTabs::paint (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    const auto& acc = lnf.theme.accent (accent);
    for (int i = 0; i < (int) pages.size(); ++i)
    {
        auto t = tabBounds (i).toFloat();
        const bool sel = i == current, on = lastOn[(size_t) i];
        if (sel)
        {
            g.setColour (Colours::white.withAlpha (0.045f)); g.fillRoundedRectangle (t, 6.f);
            auto bar = Rectangle<float> (t.getX() + 10.f, t.getBottom() + 5.f, t.getWidth() - 20.f, 2.f);
            g.setColour (acc.main.withAlpha (0.25f)); g.fillRoundedRectangle (bar.expanded (2.f, 2.f), 3.f);
            g.setColour (acc.main); g.fillRoundedRectangle (bar, 1.f);
        }
        else if (i == hoverTab) { g.setColour (Colours::white.withAlpha (0.03f)); g.fillRoundedRectangle (t, 6.f); }
        auto led = Rectangle<float> (5.f, 5.f).withCentre ({ t.getX() + 12.f, t.getCentreY() });
        if (on) { g.setColour (acc.main.withAlpha (0.45f)); g.fillEllipse (led.expanded (3.f)); }
        g.setColour (on ? acc.hot : Colour (0xff2e3233)); g.fillEllipse (led);
        g.setFont (lnf.titleFont (9.5f, 0.12f));
        g.setColour (sel ? lnf.theme.text : (i == hoverTab ? lnf.theme.value : lnf.theme.label.withAlpha (0.8f)));
        g.drawText (names[i], t.withTrimmedLeft (19.f), Justification::centredLeft);
    }
    ColourGradient sep (Colours::white.withAlpha (0.f), 16.f, 0.f, Colours::white.withAlpha (0.f), (float) getWidth() - 16.f, 0.f, false);
    sep.addColour (0.5, Colours::white.withAlpha (0.09f));
    g.setGradientFill (sep);
    g.fillRect (16.f, 47.f, (float) getWidth() - 32.f, 1.f);
}

// ======================================================================= FlowLane
void FlowLane::tick()
{
    const int w = feed.write.load (std::memory_order_acquire);
    if (w != lastWrite) { lastWrite = w; repaint(); }
}

void FlowLane::paint (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    const auto& acc = lnf.theme.accent (accent);
    auto all = getLocalBounds().toFloat();
    auto labelR = all.removeFromTop (text.isNotEmpty() ? 18.f : 0.f);
    auto lane = all;
    g.setColour (acc.main.withAlpha (0.04f)); g.fillRoundedRectangle (lane, 5.f);
    g.setColour (Colours::white.withAlpha (0.05f));
    for (float y = lane.getY() + 62.5f; y < lane.getBottom(); y += 62.5f) g.drawHorizontalLine ((int) y, lane.getX() + 4.f, lane.getRight() - 4.f);
    g.setColour (acc.main.withAlpha (0.12f));
    g.drawVerticalLine ((int) lane.getCentreX(), lane.getY(), lane.getBottom());

    constexpr int N = 1024;
    const int w = feed.write.load (std::memory_order_acquire);
    const int rows = jmax (2, (int) lane.getHeight());
    lo.resize ((size_t) rows); hi.resize ((size_t) rows);       // message thread: allocation is fine here
    float e = 0.f;
    for (int k = 0; k < rows; ++k)
    {
        const int idx = (w - 1 - k + 4 * N) & (N - 1);
        lo[(size_t) k] = jlimit (-1.f, 1.f, feed.lo[(size_t) idx]);
        hi[(size_t) k] = jlimit (-1.f, 1.f, feed.hi[(size_t) idx]);
        if (k < 24) e = jmax (e, hi[(size_t) k] - lo[(size_t) k]);
    }
    energy = e > energy ? e : energy * 0.85f + e * 0.15f;

    Path band, edgeL, edgeR;
    const float cx = lane.getCentreX(), amp = lane.getWidth() * 0.46f;
    for (int k = 0; k < rows; ++k)
    {
        const float y = lane.getY() + (float) k;
        const float xl = cx + jmin (lo[(size_t) k], -0.004f) * amp, xr = cx + jmax (hi[(size_t) k], 0.004f) * amp;
        if (k == 0) { band.startNewSubPath (xl, y); edgeL.startNewSubPath (xl, y); edgeR.startNewSubPath (xr, y); }
        else        { band.lineTo (xl, y); edgeL.lineTo (xl, y); edgeR.lineTo (xr, y); }
    }
    for (int k = rows - 1; k >= 0; --k) band.lineTo (cx + jmax (hi[(size_t) k], 0.004f) * amp, lane.getY() + (float) k);
    band.closeSubPath();

    if (text.isNotEmpty())
    {
        g.setFont (lnf.titleFont (9.5f, 0.3f));
        g.setColour (acc.main.withAlpha (0.9f));
        g.drawText (text, labelR, Justification::centred);
    }
    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (lane.toNearestInt());
    ColourGradient fill (acc.main.withAlpha (0.55f), 0.f, lane.getY(), acc.main.withAlpha (0.03f), 0.f, lane.getBottom(), false);
    fill.addColour (0.15, acc.main.withAlpha (0.35f));
    g.setGradientFill (fill); g.fillPath (band);
    ColourGradient edge (acc.hot.withAlpha (0.95f), 0.f, lane.getY(), acc.main.withAlpha (0.1f), 0.f, lane.getBottom(), false);
    g.setGradientFill (edge);
    g.strokePath (edgeL, PathStrokeType (1.1f)); g.strokePath (edgeR, PathStrokeType (1.1f));
    const float en = jlimit (0.f, 1.f, energy);
    ColourGradient head (acc.hot.withAlpha (0.08f + 0.3f * en), cx, lane.getY(), acc.main.withAlpha (0.f), cx, lane.getY() + 22.f, false);
    g.setGradientFill (head); g.fillRect (lane.withHeight (22.f));
    g.setColour (acc.hot.withAlpha (0.4f + 0.6f * en));
    g.fillRect (lane.getX() + 4.f, lane.getY(), lane.getWidth() - 8.f, 1.5f);
}

// ======================================================================= painting helpers
void paint::glassPanel (Graphics& g, StageLookAndFeel& lnf, Rectangle<int> area, const String& title, int accent, bool spot)
{
    const auto& acc = lnf.theme.accent (accent);
    auto r = area.toFloat();
    const float rad = 12.f;
    Path shape;
    shape.addRoundedRectangle (r, rad);
    DropShadow (Colours::black.withAlpha (0.6f), 26, { 0, 10 }).drawForPath (g, shape);
    g.setColour (lnf.theme.panel.withAlpha (0.8f));
    g.fillPath (shape);
    ColourGradient sheen (Colours::white.withAlpha (0.055f), 0.f, r.getY(), Colours::white.withAlpha (0.01f), 0.f, r.getBottom(), false);
    g.setGradientFill (sheen);
    g.fillPath (shape);
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (shape);
        ColourGradient spill (acc.main.withAlpha (spot ? 0.22f : 0.09f), r.getCentreX(), r.getY() - 10.f,
                              acc.main.withAlpha (0.f), r.getCentreX(), r.getY() + (spot ? 200.f : 90.f), true);
        g.setGradientFill (spill);
        g.fillRect (r);
    }
    ColourGradient border (acc.main.withAlpha (0.55f), r.getCentreX(), r.getY(), Colours::white.withAlpha (0.06f), r.getCentreX(), r.getY() + 60.f, false);
    g.setGradientFill (border);
    g.strokePath (shape, PathStrokeType (1.f));
    ColourGradient hl (Colours::white.withAlpha (0.f), r.getX() + rad, 0.f, Colours::white.withAlpha (0.f), r.getRight() - rad, 0.f, false);
    hl.addColour (0.5, Colours::white.withAlpha (0.25f));
    g.setGradientFill (hl);
    g.fillRect (r.getX() + rad, r.getY() + 0.5f, r.getWidth() - rad * 2.f, 1.f);
    if (title.isNotEmpty())
    {
        g.setColour (acc.main);
        g.fillRoundedRectangle (r.getX() + 16.f, r.getY() + 16.f, 3.f, 11.f, 1.5f);
        g.setFont (lnf.titleFont (11.f, 0.3f));
        g.setColour (lnf.theme.text.withAlpha (0.92f));
        g.drawText (title, (int) r.getX() + 26, (int) r.getY() + 13, (int) r.getWidth() - 40, 17, Justification::centredLeft);
    }
}

void paint::beam (Graphics& g, Point<float> src, float angle, float spread, float len, Colour col, float alpha)
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
}
