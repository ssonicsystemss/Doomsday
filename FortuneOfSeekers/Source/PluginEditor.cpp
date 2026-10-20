#include "PluginEditor.h"
#include "../../Shared/Source/DoomsdayBrand.h"
#include "BinaryData.h"

using namespace juce;

// ======================================================================= LookAndFeel
SeekersLookAndFeel::SeekersLookAndFeel()
{
    tfTitle = Typeface::createSystemTypefaceFor (BinaryData::MichromaRegular_ttf, BinaryData::MichromaRegular_ttfSize);
    tfLabel = Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedSemiBold_ttf, BinaryData::BarlowSemiCondensedSemiBold_ttfSize);
    tfValue = Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedMedium_ttf, BinaryData::BarlowSemiCondensedMedium_ttfSize);
    tfBold  = Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedBold_ttf, BinaryData::BarlowSemiCondensedBold_ttfSize);

    const auto& g = Palette::gold;
    setColour (PopupMenu::backgroundColourId, Colour (0xf20a0a0b));
    setColour (PopupMenu::textColourId, Palette::value);
    setColour (PopupMenu::headerTextColourId, g.main);
    setColour (PopupMenu::highlightedBackgroundColourId, g.deep.withAlpha (0.9f));
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (ComboBox::textColourId, Palette::text);
    setColour (ComboBox::arrowColourId, g.main);
    setColour (Label::textColourId, Palette::text);
    setColour (TextEditor::backgroundColourId, Colour (0xff0e0f10));
    setColour (TextEditor::textColourId, Palette::text);
    setColour (TextEditor::highlightColourId, g.deep);
    setColour (TextEditor::outlineColourId, Colours::transparentBlack);
    setColour (TextEditor::focusedOutlineColourId, g.main);
    setColour (CaretComponent::caretColourId, g.main);
    setColour (AlertWindow::backgroundColourId, Colour (0xff0b0b0c));
    setColour (AlertWindow::textColourId, Palette::text);
    setColour (AlertWindow::outlineColourId, g.main.withAlpha (0.6f));
    setColour (TextButton::textColourOffId, Palette::text);
    setColour (TextButton::textColourOnId, Colours::white);
    setColour (TooltipWindow::backgroundColourId, Colour (0xff0e0f10));
    setColour (TooltipWindow::textColourId, Palette::value);
    setColour (TooltipWindow::outlineColourId, Colours::white.withAlpha (0.12f));
}

static Font makeFont (const Typeface::Ptr& tf, float h, float k)
{
    auto o = FontOptions (tf).withHeight (h);
    if (k != 0.f) o = o.withKerningFactor (k);
    return Font (o);
}
Font SeekersLookAndFeel::titleFont (float h, float k) const { return makeFont (tfTitle, h, k); }
Font SeekersLookAndFeel::labelFont (float h, float k) const { return makeFont (tfLabel, h, k); }
Font SeekersLookAndFeel::valueFont (float h, float k) const { return makeFont (tfValue, h, k); }
Font SeekersLookAndFeel::boldFont  (float h, float k) const { return makeFont (tfBold, h, k); }
SeekersLookAndFeel& seekLnf (Component& c) { return dynamic_cast<SeekersLookAndFeel&> (c.getLookAndFeel()); }

void SeekersLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, Slider& s)
{
    const bool big     = (bool) s.getProperties()["big"];
    const bool bipolar = (bool) s.getProperties()["bipolar"];
    const auto& acc    = Palette::accent ((int) s.getProperties()["accent"]);
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
            const bool lit = a <= angle + 0.001f;
            const float t = (float) i / (float) ticks;
            // the FORTUNE crown runs green -> gold -> purple
            const Colour tc = t < 0.5f ? Palette::green.main.interpolatedWith (Palette::gold.main, t * 2.f)
                                       : Palette::gold.main.interpolatedWith (Palette::purple.main, (t - 0.5f) * 2.f);
            const float len = i % 5 == 0 ? 8.f : 3.5f;
            g.setColour (lit ? tc.withAlpha (0.6f + 0.4f * t) : Colours::white.withAlpha (0.08f));
            g.drawLine ({ c.getPointOnCircumference (r - 1.f, a), c.getPointOnCircumference (r - 1.f - len, a) }, i % 5 == 0 ? 1.6f : 1.1f);
        }
    }

    Path track;
    track.addCentredArc (c.x, c.y, ringR, ringR, 0.f, startAngle, endAngle, true);
    g.setColour (Colours::white.withAlpha (0.075f));
    g.strokePath (track, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));

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
    ColourGradient bg (Colour (0xff34383a), c.x - bodyR * 0.35f, c.y - bodyR * 0.65f, Colour (0xff070808), c.x + bodyR * 0.4f, c.y + bodyR * 1.05f, true);
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
    g.setColour (s.isEnabled() ? Colours::white : Palette::dim);
    g.drawLine ({ p1, p2 }, big ? 3.f : 1.8f);
}

Label* SeekersLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    const bool big = (bool) s.getProperties()["big"];
    l->setFont (big ? titleFont (15.f, 0.08f) : valueFont (13.f, 0.02f));
    l->setColour (Label::textColourId, big ? Palette::text : Palette::value);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setColour (Label::textWhenEditingColourId, Palette::text);
    l->setColour (Label::outlineWhenEditingColourId, Palette::gold.main.withAlpha (0.6f));
    l->setColour (Label::backgroundWhenEditingColourId, Colour (0xff0e0f10));
    l->setJustificationType (Justification::centred);
    return l;
}

void SeekersLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (0.5f);
    g.setColour (Colour (0xcc080909));
    g.fillRoundedRectangle (r, 6.f);
    ColourGradient sheen (Colours::white.withAlpha (0.07f), 0.f, r.getY(), Colours::white.withAlpha (0.01f), 0.f, r.getBottom(), false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (r, 6.f);
    g.setColour (box.isMouseOver (true) ? Palette::gold.main.withAlpha (0.8f) : Colours::white.withAlpha (0.13f));
    g.drawRoundedRectangle (r, 6.f, 1.f);
    Path chev;
    const float ax = (float) w - 16.f, ay = (float) h * 0.5f;
    chev.startNewSubPath (ax - 4.f, ay - 2.f); chev.lineTo (ax, ay + 2.f); chev.lineTo (ax + 4.f, ay - 2.f);
    g.setColour (Palette::gold.main);
    g.strokePath (chev, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
}

Font SeekersLookAndFeel::getComboBoxFont (ComboBox&) { return labelFont (16.f, 0.03f); }
void SeekersLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (Justification::centred);
}
Font SeekersLookAndFeel::getPopupMenuFont() { return valueFont (16.f); }
void SeekersLookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (findColour (PopupMenu::backgroundColourId));
    g.setColour (Palette::gold.main.withAlpha (0.35f));
    g.fillRect (0, 0, w, 1);
    g.setColour (Colours::white.withAlpha (0.1f));
    g.drawRect (0, 0, w, h, 1);
}

void SeekersLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool hover, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getHeight() * 0.5f;
    g.setColour (down ? Palette::gold.deep.withAlpha (0.8f) : Colour (0xcc080909));
    g.fillRoundedRectangle (r, rad);
    ColourGradient sheen (Colours::white.withAlpha (hover ? 0.10f : 0.06f), 0.f, r.getY(), Colours::white.withAlpha (0.f), 0.f, r.getBottom(), false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (r, rad);
    if (hover) { g.setColour (Palette::gold.main.withAlpha (0.18f)); g.drawRoundedRectangle (r.expanded (1.5f), rad + 1.5f, 3.f); }
    g.setColour (hover ? Palette::gold.main.withAlpha (0.9f) : Colours::white.withAlpha (0.14f));
    g.drawRoundedRectangle (r, rad, 1.f);
}

void SeekersLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool hover, bool)
{
    g.setFont (labelFont (b.getButtonText().length() <= 1 ? 15.f : 12.5f, 0.22f));
    g.setColour (hover ? Colours::white : Palette::value);
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
}
Font SeekersLookAndFeel::getAlertWindowTitleFont()   { return titleFont (15.f, 0.15f); }
Font SeekersLookAndFeel::getAlertWindowMessageFont() { return valueFont (15.f); }
Font SeekersLookAndFeel::getTextButtonFont (TextButton&, int) { return labelFont (13.f, 0.2f); }

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
    slider.setColour (Slider::textBoxTextColourId, Palette::value);
    slider.setColour (Slider::textBoxHighlightColourId, Palette::accent (accent).deep);
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
    g.setFont (seekLnf (*this).labelFont (11.f, 0.14f));
    g.setColour (slider.isMouseOverOrDragging() ? Palette::text : Palette::label);
    g.drawText (label, labelArea, Justification::centred);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    const int textH = big ? 26 : 18;
    const int labelH = label.isNotEmpty() ? 17 : 0;
    const int maxD = big ? 1000 : 58;
    const int need = labelH + maxD + textH;
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
    auto& lnf = seekLnf (*this);
    const auto& acc = Palette::accent (accent);
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
        g.setColour (state ? Palette::text : Palette::label);
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
    const float n = (float) labels.size();
    const float t = vertical ? (float) pt.y / (float) getHeight() : (float) pt.x / (float) getWidth();
    return jlimit (0, labels.size() - 1, (int) (t * n));
}

void Choice::mouseDown (const MouseEvent& e) { attachment->setValueAsCompleteGesture ((float) itemAt (e.getPosition())); }
void Choice::mouseMove (const MouseEvent& e) { const int h = itemAt (e.getPosition()); if (h != hover) { hover = h; repaint(); } }

void Choice::paint (Graphics& g)
{
    auto& lnf = seekLnf (*this);
    const auto& acc = Palette::accent (accent);
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
        g.setColour (sel ? Colours::white : (i == hover ? Palette::text : Palette::label));
        g.drawText (labels[i], item, Justification::centred);
    }
}

// ======================================================================= OscPanel
OscPanel::OscPanel (FortuneProcessor& p, int pa, int o)
    : part (pa), osc (o),
      power (p.apvts, ParamIDs::osc (pa, o, ParamIDs::on), "", Switch::Power, pa),
      waves (p.apvts, ParamIDs::osc (pa, o, ParamIDs::wave), Choices::waveShort, pa, false)
{
    using namespace ParamIDs;
    addAndMakeVisible (power);
    addAndMakeVisible (waves);
    addAndMakeVisible (content);
    content.setInterceptsMouseClicks (false, true);

    const std::pair<const char*, const char*> defs[] = {
        { voices, "VOICES" }, { oct, "OCTAVE" }, { semi, "SEMI" }, { vol, "VOLUME" }, { phase, "PHASE" },
        { detune, "DETUNE" }, { stereo, "STEREO" }, { pan, "PAN" }, { scream, "SCREAM" } };
    for (auto& [b, name] : defs)
    {
        const String base (b);
        const bool bip = base == oct || base == semi || base == pan;
        knobs.push_back (std::make_unique<Knob> (p.apvts, ParamIDs::osc (pa, o, b), name, pa, bip));
        content.addAndMakeVisible (*knobs.back());
    }
    detuneStereo[0] = knobs[5].get();
    detuneStereo[1] = knobs[6].get();
    detuneStereo[2] = knobs[4].get();
    voicesParam = p.apvts.getRawParameterValue (ParamIDs::osc (pa, o, voices));
    waveParam = p.apvts.getRawParameterValue (ParamIDs::osc (pa, o, wave));
}

void OscPanel::resized()
{
    auto r = getLocalBounds().reduced (14, 10);
    auto head = r.removeFromTop (26);
    power.setBounds (head.removeFromRight (56));
    head.removeFromRight (14);
    waves.setBounds (head.removeFromRight (300));
    r.removeFromTop (6);
    content.setBounds (r);
    const float cw = (float) r.getWidth() / (float) knobs.size();
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds ((int) (cw * (float) i), 0, (int) cw, r.getHeight());
}

void OscPanel::refresh()
{
    const float a = power.isOn() ? 1.f : 0.32f;
    if (std::abs (content.getAlpha() - a) > 0.001f) content.setAlpha (a);
    if (std::abs (waves.getAlpha() - a) > 0.001f) waves.setAlpha (a);
    // DETUNE / STEREO only mean something with 2+ voices; PHASE does nothing on NOISE
    const bool noise = (int) std::round (waveParam->load()) == 5;
    const bool uni = voicesParam->load() > 1.5f && ! noise;
    for (int i = 0; i < 2; ++i) { const float da = uni ? 1.f : 0.35f; if (std::abs (detuneStereo[i]->getAlpha() - da) > 0.001f) detuneStereo[i]->setAlpha (da); }
    const float pa = noise ? 0.35f : 1.f;
    if (std::abs (detuneStereo[2]->getAlpha() - pa) > 0.001f) detuneStereo[2]->setAlpha (pa);
}

// ======================================================================= PartPanel
PartPanel::PartPanel (FortuneProcessor& p, int pa)
    : part (pa),
      power (p.apvts, ParamIDs::part (pa, ParamIDs::partOn), "", Switch::Power, pa),
      ftype (p.apvts, ParamIDs::part (pa, ParamIDs::ftype), Choices::filters, pa, false)
{
    using namespace ParamIDs;
    addAndMakeVisible (power);
    addAndMakeVisible (ftype);
    addAndMakeVisible (content);
    content.setInterceptsMouseClicks (false, true);
    const std::tuple<const char*, const char*, bool> defs[] = {
        { cutoff, "CUTOFF", false }, { reso, "RESO", false }, { fdrive, "DRIVE", false }, { fenv, "F.ENV", true }, { fdecay, "F.DECAY", false },
        { attack, "ATTACK", false }, { decay, "DECAY", false }, { sustain, "SUSTAIN", false }, { release, "RELEASE", false } };
    for (auto& [b, name, bip] : defs)
    {
        knobs.push_back (std::make_unique<Knob> (p.apvts, ParamIDs::part (pa, b), name, pa, bip));
        content.addAndMakeVisible (*knobs.back());
    }
}

void PartPanel::resized()
{
    auto r = getLocalBounds().reduced (14, 10);
    auto head = r.removeFromTop (26);
    power.setBounds (head.removeFromRight (56));
    head.removeFromRight (14);
    ftype.setBounds (head.removeFromRight (240));
    r.removeFromTop (6);
    content.setBounds (r);
    const float cw = (float) (r.getWidth() - 12) / 9.f;
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds ((int) (cw * (float) i) + (i >= 5 ? 12 : 0), 0, (int) cw, r.getHeight());
    dividerX = r.getX() + (int) (cw * 5.f) + 6;
}

void PartPanel::paint (Graphics& g)
{
    const auto& acc = Palette::accent (part);
    ColourGradient d (acc.main.withAlpha (0.f), 0.f, 52.f, acc.main.withAlpha (0.f), 0.f, (float) getHeight() - 12.f, false);
    d.addColour (0.5, acc.main.withAlpha (0.35f));
    g.setGradientFill (d);
    g.fillRect ((float) dividerX, 52.f, 1.f, (float) getHeight() - 64.f);
}

void PartPanel::refresh()
{
    const float a = power.isOn() ? 1.f : 0.32f;
    if (std::abs (content.getAlpha() - a) > 0.001f) content.setAlpha (a);
    if (std::abs (ftype.getAlpha() - a) > 0.001f) ftype.setAlpha (a);
}

// ======================================================================= FxTabs
static const char* fxNames[] = { "DIST", "CHORUS", "DELAY", "REVERB", "PUMP" };

void FxTabs::Page::resized()
{
    auto r = getLocalBounds();
    if (seg != nullptr)
    {
        auto left = r.removeFromLeft (96);
        seg->setBounds (left.withSizeKeepingCentre (left.getWidth(), jmin (left.getHeight(), 24 * 4 + 8)));
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

FxTabs::FxTabs (FortuneProcessor& p, int pa) : part (pa)
{
    using namespace ParamIDs;
    auto id = [pa] (const char* b) { return ParamIDs::part (pa, b); };
    auto knob = [&p, &id, pa] (const char* b, const char* name) { return std::make_unique<Knob> (p.apvts, id (b), name, pa); };

    pages[0].seg = std::make_unique<Choice> (p.apvts, id (distType), Choices::dists, pa, true);
    pages[0].cells.push_back (knob (distDrive, "DRIVE"));
    pages[0].cells.push_back (knob (distMix, "MIX"));

    pages[1].cells.push_back (knob (choRate, "RATE"));
    pages[1].cells.push_back (knob (choDepth, "DEPTH"));
    pages[1].cells.push_back (knob (choMix, "MIX"));

    pages[2].cells.push_back (knob (dlyDiv, "TIME"));
    pages[2].cells.push_back (knob (dlyFb, "FEEDBACK"));
    pages[2].cells.push_back (knob (dlyMix, "MIX"));
    pages[2].cells.push_back (std::make_unique<Switch> (p.apvts, id (dlyPP), "P.PONG", Switch::Chip, pa));

    pages[3].cells.push_back (knob (revSize, "SIZE"));
    pages[3].cells.push_back (knob (revDamp, "DAMP"));
    pages[3].cells.push_back (knob (revMix, "MIX"));

    pages[4].cells.push_back (knob (pumpDiv, "RATE"));
    pages[4].cells.push_back (knob (pumpDepth, "DEPTH"));
    pages[4].cells.push_back (knob (pumpShape, "RELEASE"));

    const char* onIds[] = { distOn, choOn, dlyOn, revOn, pumpOn };
    for (int i = 0; i < numTabs; ++i)
    {
        auto& pg = pages[(size_t) i];
        if (pg.seg) pg.addAndMakeVisible (*pg.seg);
        for (auto& c : pg.cells) pg.addAndMakeVisible (*c);
        addChildComponent (pg);
        powers[(size_t) i] = std::make_unique<Switch> (p.apvts, id (onIds[i]), "ACTIVE", Switch::Power, pa);
        addChildComponent (*powers[(size_t) i]);
        onFlags[(size_t) i] = p.apvts.getRawParameterValue (id (onIds[i]));
    }
    pages[0].setVisible (true);
    powers[0]->setVisible (true);
}

Rectangle<int> FxTabs::tabBounds (int i) const { return { 12 + i * 82, 8, 78, 32 }; }

void FxTabs::resized()
{
    for (auto& s : powers) s->setBounds (getWidth() - 14 - 104, 12, 104, 24);
    auto body = getLocalBounds().reduced (16, 0).withTrimmedTop (54).withTrimmedBottom (10);
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
    for (int i = 0; i < numTabs; ++i) if (tabBounds (i).contains (e.getPosition())) selectTab (i);
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
}

void FxTabs::paint (Graphics& g)
{
    auto& lnf = seekLnf (*this);
    const auto& acc = Palette::accent (part);
    for (int i = 0; i < numTabs; ++i)
    {
        auto t = tabBounds (i).toFloat();
        const bool sel = i == current, on = lastOn[(size_t) i];
        if (sel)
        {
            g.setColour (Colours::white.withAlpha (0.045f));
            g.fillRoundedRectangle (t, 6.f);
            auto bar = Rectangle<float> (t.getX() + 10.f, t.getBottom() + 5.f, t.getWidth() - 20.f, 2.f);
            g.setColour (acc.main.withAlpha (0.25f)); g.fillRoundedRectangle (bar.expanded (2.f, 2.f), 3.f);
            g.setColour (acc.main); g.fillRoundedRectangle (bar, 1.f);
        }
        else if (i == hoverTab)
        {
            g.setColour (Colours::white.withAlpha (0.03f));
            g.fillRoundedRectangle (t, 6.f);
        }
        auto led = Rectangle<float> (5.f, 5.f).withCentre ({ t.getX() + 12.f, t.getCentreY() });
        if (on) { g.setColour (acc.main.withAlpha (0.45f)); g.fillEllipse (led.expanded (3.f)); }
        g.setColour (on ? acc.hot : Colour (0xff2e3233));
        g.fillEllipse (led);
        g.setFont (lnf.titleFont (9.5f, 0.12f));
        g.setColour (sel ? Palette::text : (i == hoverTab ? Palette::value : Palette::label.withAlpha (0.8f)));
        g.drawText (fxNames[i], t.withTrimmedLeft (19.f), Justification::centredLeft);
    }
    ColourGradient sep (Colours::white.withAlpha (0.f), 16.f, 0.f, Colours::white.withAlpha (0.f), (float) getWidth() - 16.f, 0.f, false);
    sep.addColour (0.5, Colours::white.withAlpha (0.09f));
    g.setGradientFill (sep);
    g.fillRect (16.f, 47.f, (float) getWidth() - 32.f, 1.f);
}

// ======================================================================= PathView
void PathView::tick()
{
    bool moved = false;
    for (int p = 0; p < 2; ++p)
    {
        const int w = proc.getPath (p).write.load (std::memory_order_acquire);
        if (w != lastWrite[(size_t) p]) { lastWrite[(size_t) p] = w; moved = true; }
    }
    if (moved) repaint();
}

void PathView::paint (Graphics& g)
{
    auto& lnf = seekLnf (*this);
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xcc040505));
    g.fillRoundedRectangle (r, 8.f);
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.f, 1.f);

    constexpr int N = FortuneEngine::pathSize;
    const float laneGap = 10.f;
    auto inner = r.reduced (10.f, 8.f);
    auto head = inner.removeFromTop (18.f);
    const float laneW = (inner.getWidth() - laneGap) / 2.f;

    for (int p = 0; p < 2; ++p)
    {
        const auto& acc = Palette::accent (p);
        auto lane = Rectangle<float> (inner.getX() + (laneW + laneGap) * (float) p, inner.getY(), laneW, inner.getHeight());
        auto labelR = Rectangle<float> (lane.getX(), head.getY(), lane.getWidth(), head.getHeight());

        // lane frame + time grid (every 250 ms, rows are ~4 ms)
        g.setColour (acc.main.withAlpha (0.04f));
        g.fillRoundedRectangle (lane, 5.f);
        g.setColour (Colours::white.withAlpha (0.05f));
        for (float y = lane.getY() + 62.5f; y < lane.getBottom(); y += 62.5f) g.drawHorizontalLine ((int) y, lane.getX() + 4.f, lane.getRight() - 4.f);
        g.setColour (acc.main.withAlpha (0.12f));
        g.drawVerticalLine ((int) lane.getCentreX(), lane.getY(), lane.getBottom());

        // copy the feed: row 0 (top) = newest
        const auto& feed = proc.getPath (p);
        const int w = feed.write.load (std::memory_order_acquire);
        const int rows = (int) lane.getHeight();
        std::vector<float> lo ((size_t) rows), hi ((size_t) rows);
        float e = 0.f;
        for (int k = 0; k < rows; ++k)
        {
            const int idx = (w - 1 - k + 4 * N) & (N - 1);
            lo[(size_t) k] = jlimit (-1.f, 1.f, feed.lo[(size_t) idx]);
            hi[(size_t) k] = jlimit (-1.f, 1.f, feed.hi[(size_t) idx]);
            if (k < 24) e = jmax (e, hi[(size_t) k] - lo[(size_t) k]);
        }
        energy[(size_t) p] = e > energy[(size_t) p] ? e : energy[(size_t) p] * 0.85f + e * 0.15f;

        Path band, edgeL, edgeR;
        const float cx = lane.getCentreX(), amp = lane.getWidth() * 0.46f;
        for (int k = 0; k < rows; ++k)
        {
            const float y = lane.getY() + (float) k;
            const float xl = cx + jmin (lo[(size_t) k], -0.004f) * amp, xr = cx + jmax (hi[(size_t) k], 0.004f) * amp;
            if (k == 0) { band.startNewSubPath (xl, y); edgeL.startNewSubPath (xl, y); edgeR.startNewSubPath (xr, y); }
            else        { band.lineTo (xl, y); edgeL.lineTo (xl, y); edgeR.lineTo (xr, y); }
        }
        for (int k = rows - 1; k >= 0; --k)
            band.lineTo (cx + jmax (hi[(size_t) k], 0.004f) * amp, lane.getY() + (float) k);
        band.closeSubPath();

        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (lane.toNearestInt());
        ColourGradient fill (acc.main.withAlpha (0.55f), 0.f, lane.getY(), acc.main.withAlpha (0.03f), 0.f, lane.getBottom(), false);
        fill.addColour (0.15, acc.main.withAlpha (0.35f));
        g.setGradientFill (fill);
        g.fillPath (band);
        ColourGradient edge (acc.hot.withAlpha (0.95f), 0.f, lane.getY(), acc.main.withAlpha (0.1f), 0.f, lane.getBottom(), false);
        g.setGradientFill (edge);
        g.strokePath (edgeL, PathStrokeType (1.1f));
        g.strokePath (edgeR, PathStrokeType (1.1f));

        // glowing "head" where the sound enters the path
        const float en = jlimit (0.f, 1.f, energy[(size_t) p]);
        ColourGradient headGlow (acc.hot.withAlpha (0.08f + 0.3f * en), cx, lane.getY(), acc.main.withAlpha (0.f), cx, lane.getY() + 22.f, false);
        g.setGradientFill (headGlow);
        g.fillRect (lane.withHeight (22.f));
        g.setColour (acc.hot.withAlpha (0.4f + 0.6f * en));
        g.fillRect (lane.getX() + 4.f, lane.getY(), lane.getWidth() - 8.f, 1.5f);

        g.setFont (lnf.titleFont (9.5f, 0.3f));
        g.setColour (acc.main.withAlpha (0.9f));
        g.drawText (p == 0 ? "PART A" : "PART B", labelR, Justification::centred);
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
        const Colour c = t > 0.95f ? Colours::white : t > 0.8f ? Palette::gold.hot : t > 0.5f ? Palette::gold.main : Palette::green.main.interpolatedWith (Palette::purple.main, t * 2.f);
        g.setColour (t <= level ? c : Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (seg, 1.f);
        if (std::abs (t - hold) < 0.5f / (float) segs && hold > 0.02f) { g.setColour (Colours::white.withAlpha (0.8f)); g.fillRoundedRectangle (seg, 1.f); }
    }
}

// ======================================================================= Canvas
Knob* Canvas::knob (const String& id, const String& label, int accent, bool bipolar, bool big)
{
    auto k = std::make_unique<Knob> (proc.apvts, id, label, accent, bipolar, big);
    auto* raw = k.get();
    addAndMakeVisible (*raw);
    owned.push_back (std::move (k));
    return raw;
}

Switch* Canvas::toggle (const String& id, const String& label, Switch::Style st, int accent)
{
    auto t = std::make_unique<Switch> (proc.apvts, id, label, st, accent);
    auto* raw = t.get();
    addAndMakeVisible (*raw);
    owned.push_back (std::move (t));
    return raw;
}

Canvas::Canvas (FortuneProcessor& p) : proc (p), pathView (p)
{
    setLookAndFeel (&lnf);
    using namespace ParamIDs;
    for (int pa = 0; pa < 2; ++pa)
    {
        for (int o = 0; o < 2; ++o)
        {
            oscPanels[(size_t) pa][(size_t) o] = std::make_unique<OscPanel> (p, pa, o);
            addAndMakeVisible (*oscPanels[(size_t) pa][(size_t) o]);
        }
        partPanels[(size_t) pa] = std::make_unique<PartPanel> (p, pa);
        fxTabs[(size_t) pa] = std::make_unique<FxTabs> (p, pa);
        addAndMakeVisible (*partPanels[(size_t) pa]);
        addAndMakeVisible (*fxTabs[(size_t) pa]);
    }
    fortuneKnob = knob (fortune, "", 2, false, true);
    fortuneKnob->slider.setTooltip ("FORTUNE - opens both filters, widens the detune, adds scream and space  (double-click = 0)");
    globalCells = { knob (part (0, level), "A LEVEL", 0), knob (part (1, level), "B LEVEL", 1), knob (glide, "GLIDE", 2), knob (volume, "VOLUME", 2) };
    monoChip = toggle (mono, "MONO", Switch::Chip, 2);
    monoChip->setTooltip ("MONO: one voice, legato + glide.  Off: 12-voice polyphony for chords");
    addAndMakeVisible (pathView);
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
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { showSaveDialog(); };
    randomButton.onClick = [this] { proc.randomize(); };
    playButton.onPress = [this] (bool down) { proc.auditionChord (down); };
    saveButton.setTooltip ("Save the current sound as a preset");
    randomButton.setTooltip ("Random melody sound within musical ranges");
    playButton.setTooltip ("Hold to play a C major chord");
    for (auto* c : { (Component*) &presetBox, (Component*) &prevButton, (Component*) &nextButton,
                     (Component*) &saveButton, (Component*) &randomButton, (Component*) &playButton })
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
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    userPresetFiles = proc.getUserPresets();
    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("USER");
        for (int i = 0; i < userPresetFiles.size(); ++i) presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(), userIdBase + i);
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
    saveDialog = std::make_unique<AlertWindow> ("SAVE PRESET", "Preset name (Documents / Fortune of Seekers / Presets):", MessageBoxIconType::NoIcon, this);
    saveDialog->setLookAndFeel (&lnf);
    saveDialog->addTextEditor ("name", proc.getPresetName().startsWith ("Random") ? String ("My Melody") : proc.getPresetName());
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
            else AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "SAVE PRESET", "Could not save the preset.");
        }
    }), false);
}

void Canvas::timerCallback()
{
    for (auto& part : oscPanels) for (auto& o : part) o->refresh();
    for (auto& pp : partPanels) pp->refresh();
    for (auto& f : fxTabs) f->refresh();
    pathView.tick();

    const auto name = proc.getPresetName();
    if (name != lastPresetName)
    {
        lastPresetName = name;
        int found = 0;
        for (int i = 0; i < presetBox.getNumItems(); ++i)
            if (presetBox.getItemText (i) == name) { found = presetBox.getItemId (i); break; }
        if (found != 0) presetBox.setSelectedId (found, dontSendNotification);
        else presetBox.setText (name, dontSendNotification);
    }

    const float peak = proc.takePeak();
    meter.setLevel (peak);
    const float target = jlimit (0.f, 1.f, peak * 1.4f);
    glow = target > glow ? target : glow * 0.9f;
    if (std::abs (glow - lastGlow) > 0.01f) { lastGlow = glow; repaint (300, 0, W - 600, 110); }
}

void Canvas::resized()
{
    const int m = 20, gap = 14, colW = 556, midW = 300;
    const int xA = m, xMid = xA + colW + gap, xB = xMid + midW + gap;
    const int oscH = 150, partH = 150;
    const int y1 = 132, y2 = y1 + oscH + gap, y3 = y2 + oscH + gap, y4 = y3 + partH + gap, yEnd = H - m;

    panels.clear();
    for (int pa = 0; pa < 2; ++pa)
    {
        const int x = pa == 0 ? xA : xB;
        const String P (pa == 0 ? "A" : "B");
        panels.push_back ({ "OSC " + P + "1", { x, y1, colW, oscH }, pa });
        panels.push_back ({ "OSC " + P + "2", { x, y2, colW, oscH }, pa });
        panels.push_back ({ "PART " + P, { x, y3, colW, partH }, pa });
        panels.push_back ({ "", { x, y4, colW, yEnd - y4 }, pa });
        oscPanels[(size_t) pa][0]->setBounds (panels[panels.size() - 4].area);
        oscPanels[(size_t) pa][1]->setBounds (panels[panels.size() - 3].area);
        partPanels[(size_t) pa]->setBounds (panels[panels.size() - 2].area);
        fxTabs[(size_t) pa]->setBounds (panels[panels.size() - 1].area);
    }

    // centre column: FORTUNE, THE PATH, GLOBAL
    const Rectangle<int> fortA (xMid, y1, midW, 196), pathA (xMid, fortA.getBottom() + gap, midW, 300);
    const Rectangle<int> globA (xMid, pathA.getBottom() + gap, midW, yEnd - pathA.getBottom() - gap);
    panels.push_back ({ "", fortA, 2, true });
    panels.push_back ({ "THE PATH", pathA, 2 });
    panels.push_back ({ "GLOBAL", globA, 2 });
    {
        auto a = fortA.reduced (40, 10);
        a.removeFromTop (22);
        fortuneKnob->setBounds (a);
    }
    {
        auto a = pathA.reduced (10, 8);
        a.removeFromTop (30);
        pathView.setBounds (a);
    }
    {
        monoChip->setBounds (globA.getRight() - 14 - 84, globA.getY() + 10, 84, 26);
        auto a = globA.reduced (10, 8);
        a.removeFromTop (34);
        meter.setBounds (a.removeFromRight (14).reduced (0, 4));
        a.removeFromRight (6);
        const float cw = (float) a.getWidth() / 4.f;
        for (int i = 0; i < 4; ++i) globalCells[(size_t) i]->setBounds (a.getX() + (int) (cw * (float) i), a.getY(), (int) cw, a.getHeight());
    }

    // header
    const int px = W - m - 300;
    prevButton.setBounds (px, 36, 32, 32);
    presetBox.setBounds (px + 38, 36, 224, 32);
    nextButton.setBounds (px + 268, 36, 32, 32);
    randomButton.setBounds (px, 76, 132, 28);
    saveButton.setBounds (px + 138, 76, 76, 28);
    playButton.setBounds (px + 220, 76, 80, 28);

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

    // LED wall behind the header: green on the left, purple on the right
    {
        Random r (11);
        for (int y = 4; y < 180; y += 7)
            for (int x = 4; x < W; x += 7)
            {
                const float t = (float) x / (float) W;
                const float dx = (t - 0.5f) * 2.f;
                const float fall = 1.f - (float) y / 180.f;
                const float a = 0.10f * fall * (0.35f + 0.65f * dx * dx) * (0.6f + 0.4f * r.nextFloat());
                g.setColour ((t < 0.5f ? Palette::green.main : Palette::purple.main).withAlpha (jmax (0.f, a)));
                g.fillRect ((float) x, (float) y, 2.f, 2.f);
            }
    }

    // beams: green rig left, purple rig right, a narrow gold spot on FORTUNE
    const float angles[] = { -0.55f, -0.32f, -0.12f, 0.12f };
    for (int i = 0; i < 4; ++i)
    {
        drawBeam (g, { 120.f, -20.f }, -angles[i] - 0.22f, 0.05f, 1150.f, Palette::green.main, 0.06f);
        drawBeam (g, { W - 120.f, -20.f }, angles[i] + 0.22f, 0.05f, 1150.f, Palette::purple.main, 0.07f);
    }
    drawBeam (g, { W * 0.5f, -60.f }, 0.f, 0.09f, 480.f, Palette::gold.hot, 0.06f);

    // floor haze in both colours
    {
        ColourGradient fl (Palette::green.main.withAlpha (0.10f), W * 0.2f, H + 100.f, Colours::transparentBlack, W * 0.2f, H - 300.f, true);
        g.setGradientFill (fl); g.fillRect (0, H - 380, W, 380);
        ColourGradient fr (Palette::purple.main.withAlpha (0.13f), W * 0.8f, H + 100.f, Colours::transparentBlack, W * 0.8f, H - 300.f, true);
        g.setGradientFill (fr); g.fillRect (0, H - 380, W, 380);
    }
    {
        ColourGradient vig (Colours::transparentBlack, W * 0.5f, H * 0.45f, Colours::black.withAlpha (0.75f), 0.f, -60.f, true);
        g.setGradientFill (vig);
        g.fillRect (0, 0, W, H);
    }

    // horizon line green -> gold -> purple
    {
        ColourGradient line (Palette::green.main.withAlpha (0.f), 0.f, 0.f, Palette::purple.main.withAlpha (0.f), (float) W, 0.f, false);
        line.addColour (0.2, Palette::green.main.withAlpha (0.6f));
        line.addColour (0.5, Palette::gold.main.withAlpha (0.9f));
        line.addColour (0.8, Palette::purple.main.withAlpha (0.6f));
        g.setGradientFill (line);
        g.fillRect (0.f, 118.f, (float) W, 1.f);
    }

    // header side text
    g.setFont (lnf.titleFont (10.f, 0.25f));
    g.setColour (Palette::text.withAlpha (0.75f));
    g.drawText ("V1.1", (int) doomsday::brand::headerTextX (1184.f / (float) W), 30, 300, 14, Justification::left);
    g.setFont (lnf.labelFont (12.f, 0.28f));
    g.setColour (Palette::green.main.withAlpha (0.85f));
    g.drawText ("PART A  /  2 OSCILLATORS", (int) doomsday::brand::headerTextX (1184.f / (float) W), 48, 320, 16, Justification::left);
    g.setColour (Palette::purple.main.withAlpha (0.95f));
    g.drawText ("PART B  /  2 OSCILLATORS", (int) doomsday::brand::headerTextX (1184.f / (float) W), 64, 320, 16, Justification::left);
    g.setColour (Palette::gold.main);
    g.drawText ("SEEK THE MELODY", (int) doomsday::brand::headerTextX (1184.f / (float) W), 80, 320, 16, Justification::left);
    doomsday::brand::drawHeaderBadge (g, (1184.f / (float) W), 0.55f);

    g.setFont (lnf.labelFont (11.f, 0.35f));
    g.setColour (Palette::dim);
    g.drawText ("PRESET", W - 20 - 300 + 38, 18, 224, 14, Justification::centred);

    g.setFont (lnf.labelFont (12.5f, 0.62f));
    g.setColour (Palette::gold.main);
    g.drawText ("HARDSTYLE  MELODY  SYNTHESIZER", 0, 84, W, 18, Justification::centred);

    for (auto& p : panels) drawGlassPanel (g, p);

    // FORTUNE panel: compass rose behind the knob + title
    {
        auto a = panels[8].area.toFloat();
        const auto c = Point<float> (a.getCentreX(), a.getY() + 22.f + (a.getHeight() - 32.f) * 0.5f);
        const float R = 92.f;
        for (int i = 0; i < 16; ++i)
        {
            const float ang = (float) i * MathConstants<float>::twoPi / 16.f;
            const bool major = i % 4 == 0, mid = i % 2 == 0;
            const float len = major ? R : mid ? R * 0.78f : R * 0.62f;
            Path ray;
            ray.startNewSubPath (c.getPointOnCircumference (len, ang));
            ray.lineTo (c.getPointOnCircumference (12.f, ang - 0.12f));
            ray.lineTo (c.getPointOnCircumference (12.f, ang + 0.12f));
            ray.closeSubPath();
            g.setColour (Palette::gold.main.withAlpha (major ? 0.16f : 0.08f));
            g.fillPath (ray);
        }
        g.setColour (Palette::gold.main.withAlpha (0.14f));
        g.drawEllipse (Rectangle<float> (R * 2.f, R * 2.f).withCentre (c), 1.f);
        g.drawEllipse (Rectangle<float> (R * 1.7f, R * 1.7f).withCentre (c), 0.7f);
        g.setFont (lnf.titleFont (11.f, 0.4f));
        g.setColour (Palette::text.withAlpha (0.92f));
        g.drawText ("FORTUNE", (int) a.getX(), (int) a.getY() + 12, (int) a.getWidth(), 16, Justification::centred);
    }

}

void Canvas::drawGlassPanel (Graphics& g, const Panel& pnl)
{
    const auto& acc = Palette::accent (pnl.accent);
    auto r = pnl.area.toFloat();
    const float rad = 12.f;
    Path shape;
    shape.addRoundedRectangle (r, rad);
    DropShadow (Colours::black.withAlpha (0.6f), 26, { 0, 10 }).drawForPath (g, shape);
    g.setColour (Colour (0xff070808).withAlpha (0.78f));
    g.fillPath (shape);
    ColourGradient sheen (Colours::white.withAlpha (0.055f), 0.f, r.getY(), Colours::white.withAlpha (0.01f), 0.f, r.getBottom(), false);
    g.setGradientFill (sheen);
    g.fillPath (shape);
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (shape);
        ColourGradient spill (acc.main.withAlpha (pnl.spot ? 0.22f : 0.09f), r.getCentreX(), r.getY() - 10.f,
                              acc.main.withAlpha (0.f), r.getCentreX(), r.getY() + (pnl.spot ? 200.f : 90.f), true);
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

    if (pnl.title.isNotEmpty())
    {
        g.setColour (acc.main);
        g.fillRoundedRectangle (r.getX() + 16.f, r.getY() + 16.f, 3.f, 11.f, 1.5f);
        g.setFont (lnf.titleFont (11.f, 0.3f));
        g.setColour (Palette::text.withAlpha (0.92f));
        g.drawText (pnl.title, (int) r.getX() + 26, (int) r.getY() + 13, 200, 17, Justification::centredLeft);
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

    // title: green -> white -> purple, glow breathes with the output
    const auto titleArea = Rectangle<float> (0.f, 20.f, (float) W, 56.f);
    const String title ("FORTUNE OF SEEKERS");
    g.setFont (lnf.titleFont (36.f, 0.2f));
    const float a = 0.10f + 0.22f * glow;
    for (int i = 4; i >= 1; --i)
    {
        const float o = (float) i * 1.6f;
        g.setColour (Palette::green.main.withAlpha (a / (float) i));
        g.drawText (title, titleArea.translated (-o, 0.f), Justification::centred);
        g.drawText (title, titleArea.translated (0.f, -o), Justification::centred);
        g.setColour (Palette::purple.main.withAlpha (a / (float) i));
        g.drawText (title, titleArea.translated (o, 0.f), Justification::centred);
        g.drawText (title, titleArea.translated (0.f, o), Justification::centred);
    }
    ColourGradient tg (Palette::green.hot, W * 0.25f, 0.f, Palette::purple.hot, W * 0.75f, 0.f, false);
    tg.addColour (0.5, Colours::white);
    g.setGradientFill (tg);
    g.drawText (title, titleArea, Justification::centred);
}

// ======================================================================= Editor
FortuneEditor::FortuneEditor (FortuneProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (888, 498, 2220, 1245);
    getConstrainer()->setFixedAspectRatio ((double) Canvas::W / (double) Canvas::H);
    setSize (1184, 664);
}

void FortuneEditor::resized()
{
    canvas.setBounds (0, 0, Canvas::W, Canvas::H);
    canvas.setTransform (AffineTransform::scale ((float) getWidth() / (float) Canvas::W));
}
