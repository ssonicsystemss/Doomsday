#include "DoomsdayFlat.h"
#include "BinaryData.h"

using namespace juce;

namespace doomsday::flat
{
const Tokens& tokens()
{
    static const Tokens t;
    return t;
}

// ======================================================================= LookAndFeel
FlatLookAndFeel::FlatLookAndFeel()
{
    tf[Medium]   = Typeface::createSystemTypefaceFor (BinaryData::InterMedium_ttf, BinaryData::InterMedium_ttfSize);
    tf[SemiBold] = Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);
    tf[Bold]     = Typeface::createSystemTypefaceFor (BinaryData::InterBold_ttf, BinaryData::InterBold_ttfSize);
    setDefaultSansSerifTypeface (tf[Medium]);

    const auto& t = tokens();
    setColour (ResizableWindow::backgroundColourId, t.ground);
    setColour (PopupMenu::backgroundColourId, t.surface);
    setColour (PopupMenu::textColourId, t.text);
    setColour (PopupMenu::headerTextColourId, t.textMute);
    setColour (PopupMenu::highlightedBackgroundColourId, t.border);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (ComboBox::textColourId, t.text);
    setColour (ComboBox::arrowColourId, t.textDim);
    setColour (Label::textColourId, t.text);
    setColour (TextEditor::backgroundColourId, t.control);
    setColour (TextEditor::textColourId, t.text);
    setColour (TextEditor::highlightColourId, t.hot.withAlpha (0.35f));
    setColour (TextEditor::outlineColourId, t.border);
    setColour (TextEditor::focusedOutlineColourId, t.hot);
    setColour (CaretComponent::caretColourId, t.hot);
    setColour (AlertWindow::backgroundColourId, t.surface);
    setColour (AlertWindow::textColourId, t.text);
    setColour (AlertWindow::outlineColourId, t.border);
    setColour (TextButton::textColourOffId, t.text);
    setColour (TextButton::textColourOnId, t.ground);
    setColour (TooltipWindow::backgroundColourId, t.border);
    setColour (TooltipWindow::textColourId, t.text);
    setColour (TooltipWindow::outlineColourId, Colours::transparentBlack);
}

Font FlatLookAndFeel::font (float h, Weight w, float k) const
{
    auto o = FontOptions (tf[w]).withHeight (h);
    if (k != 0.f) o = o.withKerningFactor (k);
    return Font (o);
}

FlatLookAndFeel& lnfOf (Component& c) { return dynamic_cast<FlatLookAndFeel&> (c.getLookAndFeel()); }

void FlatLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s)
{
    const auto& t = tokens();
    const bool bipolar = (bool) s.getProperties()["bipolar"];
    const bool hover = s.isMouseOverOrDragging() && s.isEnabled();
    const Colour accent = s.getProperties().contains ("accent") ? Colour ((uint32) (int64) s.getProperties()["accent"]) : t.hot;

    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size = jmin (b.getWidth(), b.getHeight());
    const float stroke = jlimit (3.f, 7.f, size * 0.095f);
    const bool crown = (bool) s.getProperties()["crown"];
    const float r = size * 0.5f - stroke * 0.5f - (crown ? 16.f : 6.f);      // room for the modulation ring / crown
    const auto c = b.getCentre();
    const float angle = a0 + pos * (a1 - a0);

    // CROWN (the FORTUNE knob, after Fortune of Seekers): tick ring that lights up hot -> cool along the travel
    if (crown)
    {
        const int ticks = 60;
        const float ro = size * 0.5f - 2.f;
        for (int i = 0; i <= ticks; ++i)
        {
            const float a = a0 + (a1 - a0) * (float) i / (float) ticks;
            const float tt = (float) i / (float) ticks;
            const bool lit = a <= angle + 0.001f;
            const float len = i % 5 == 0 ? 8.f : 4.f;
            g.setColour (lit ? t.hot.interpolatedWith (t.cool, tt).withAlpha (0.65f + 0.35f * tt) : t.border);
            g.drawLine ({ c.getPointOnCircumference (ro, a), c.getPointOnCircumference (ro - len, a) }, i % 5 == 0 ? 1.8f : 1.2f);
        }
    }

    Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.f, a0, a1, true);
    g.setColour (hover ? t.border.brighter (0.15f) : t.border);
    g.strokePath (track, PathStrokeType (stroke, PathStrokeType::curved, PathStrokeType::rounded));

    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    if (std::abs (angle - from) > 0.01f)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, r, r, 0.f, jmin (from, angle), jmax (from, angle), true);
        g.setColour (s.isEnabled() ? accent : t.textMute);
        g.strokePath (arc, PathStrokeType (stroke, PathStrokeType::curved, PathStrokeType::rounded));
    }
    else if (bipolar)
    {
        g.setColour (t.textMute);
        g.fillEllipse (Rectangle<float> (stroke, stroke).withCentre (c.getPointOnCircumference (r, from)));
    }

    // modulation ring (Knob::setModulation)
    const float depth = (float) s.getProperties().getWithDefault ("modDepth", 0.f);
    if (std::abs (depth) > 0.001f)
    {
        const float rr = r + stroke * 0.5f + 3.5f;
        const float e = jlimit (a0, a1, angle + depth * (a1 - a0));
        Path ring;
        ring.addCentredArc (c.x, c.y, rr, rr, 0.f, jmin (angle, e), jmax (angle, e), true);
        g.setColour (accent.withAlpha (0.55f));
        g.strokePath (ring, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
        const float now = (float) s.getProperties().getWithDefault ("modNow", -1.f);
        if (now >= 0.f)
        {
            g.setColour (Colours::white);
            g.fillEllipse (Rectangle<float> (4.f, 4.f).withCentre (c.getPointOnCircumference (rr, a0 + jlimit (0.f, 1.f, now) * (a1 - a0))));
        }
    }

    // a small dot at the value end shows "grab here" on hover
    if (hover)
    {
        g.setColour (Colours::white);
        g.fillEllipse (Rectangle<float> (stroke * 0.9f, stroke * 0.9f).withCentre (c.getPointOnCircumference (r, angle)));
    }
}

Label* FlatLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    const auto& t = tokens();
    const float h = (float) s.getProperties().getWithDefault ("valueSize", 13.f);
    l->setFont (font (h, SemiBold));
    l->setColour (Label::textColourId, t.text);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setColour (Label::textWhenEditingColourId, t.text);
    l->setColour (Label::outlineWhenEditingColourId, t.hot);
    l->setColour (Label::backgroundWhenEditingColourId, t.control);
    l->setJustificationType (Justification::centred);
    return l;
}

Slider::SliderLayout FlatLookAndFeel::getSliderLayout (Slider& s) { return LookAndFeel_V4::getSliderLayout (s); }

void FlatLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    const auto& t = tokens();
    auto r = Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (0.5f);
    g.setColour (t.control);
    g.fillRoundedRectangle (r, 8.f);
    g.setColour (box.isMouseOver (true) ? t.textMute : t.border);
    g.drawRoundedRectangle (r, 8.f, 1.f);
    Path chev;
    const float ax = (float) w - 16.f, ay = (float) h * 0.5f;
    chev.startNewSubPath (ax - 4.f, ay - 2.f); chev.lineTo (ax, ay + 2.f); chev.lineTo (ax + 4.f, ay - 2.f);
    g.setColour (t.textDim);
    g.strokePath (chev, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
}

Font FlatLookAndFeel::getComboBoxFont (ComboBox&) { return font (13.f, SemiBold); }

void FlatLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (Justification::centredLeft);
}

Font FlatLookAndFeel::getPopupMenuFont() { return font (14.f, Medium); }

void FlatLookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (tokens().surface);
    g.setColour (tokens().border);
    g.drawRect (0, 0, w, h, 1);
}

void FlatLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool hover, bool down)
{
    const auto& t = tokens();
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool primary = (bool) b.getProperties()["primary"];
    if (primary)
    {
        g.setColour (down ? t.hot.darker (0.2f) : (hover ? t.hot.brighter (0.12f) : t.hot));
        g.fillRoundedRectangle (r, 8.f);
        return;
    }
    g.setColour (down ? t.border : (hover ? t.control.brighter (0.08f) : t.control));
    g.fillRoundedRectangle (r, 8.f);
    g.setColour (hover ? t.textMute : t.border);
    g.drawRoundedRectangle (r, 8.f, 1.f);
}

void FlatLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool hover, bool)
{
    const auto& t = tokens();
    const bool primary = (bool) b.getProperties()["primary"];
    g.setFont (font (13.f, primary ? Bold : SemiBold));
    g.setColour (primary ? t.ground : (hover || b.isEnabled() ? t.text : t.textMute));
    if (! b.isEnabled()) g.setColour (t.textMute);
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
}

Font FlatLookAndFeel::getTextButtonFont (TextButton&, int) { return font (13.f, SemiBold); }
Font FlatLookAndFeel::getAlertWindowTitleFont()   { return font (16.f, Bold); }
Font FlatLookAndFeel::getAlertWindowMessageFont() { return font (14.f, Medium); }

Rectangle<int> FlatLookAndFeel::getTooltipBounds (const String& text, Point<int> pos, Rectangle<int> parent)
{
    const int w = jmin (360, (int) std::ceil (GlyphArrangement::getStringWidth (font (13.f, Medium), text)) + 20);
    const int h = 26;
    return Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                           pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h).constrainedWithin (parent);
}

void FlatLookAndFeel::drawTooltip (Graphics& g, const String& text, int w, int h)
{
    g.setColour (tokens().border);
    g.fillRoundedRectangle (0.f, 0.f, (float) w, (float) h, 6.f);
    g.setColour (tokens().text);
    g.setFont (font (13.f, Medium));
    g.drawText (text, Rectangle<int> (0, 0, w, h).reduced (10, 0), Justification::centredLeft, true);
}

// ======================================================================= Knob
Knob::Knob (AudioProcessorValueTreeState& apvts, const String& paramId, const String& text, Colour accent, bool bipolar, int d)
    : label (text), diameter (d)
{
    slider.getProperties().set ("bipolar", bipolar);
    slider.getProperties().set ("accent", (int64) accent.getARGB());
    slider.getProperties().set ("valueSize", d >= 80 ? 18.f : 13.f);
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, jmax (d + 30, 72), d >= 80 ? 24 : 18);
    slider.setMouseDragSensitivity (d >= 80 ? 320 : 220);
    slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxTextColourId, tokens().text);
    slider.setColour (Slider::textBoxHighlightColourId, accent.withAlpha (0.35f));
    addAndMakeVisible (slider);

    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
    if (auto* p = apvts.getParameter (paramId))
    {
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.setTooltip (p->getName (64) + "  (double-click = default)");
    }
}

void Knob::setAccent (Colour c)
{
    slider.getProperties().set ("accent", (int64) c.getARGB());
    slider.repaint();
}

void Knob::setModulation (float depth, float now)
{
    auto& p = slider.getProperties();
    const float od = (float) p.getWithDefault ("modDepth", 0.f), on = (float) p.getWithDefault ("modNow", -1.f);
    if (std::abs (od - depth) < 0.002f && std::abs (on - now) < 0.004f) return;
    p.set ("modDepth", depth);
    p.set ("modNow", now);
    slider.repaint();
}

void Knob::paint (Graphics& g)
{
    if (label.isEmpty()) return;
    g.setFont (lnfOf (*this).font (diameter >= 80 ? 12.f : 10.5f, SemiBold, 0.02f));
    g.setColour (slider.isMouseOverOrDragging() ? tokens().text : tokens().textMute);
    g.drawText (label, labelArea, Justification::centred);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    const int textH = diameter >= 80 ? 24 : 18;
    const int labelH = label.isNotEmpty() ? 14 : 0;
    const int need = diameter + textH + labelH + 2;
    if (r.getHeight() > need) r = r.withSizeKeepingCentre (r.getWidth(), need);
    labelArea = r.removeFromBottom (labelH);
    r.removeFromBottom (2);
    slider.setBounds (r);
}

// ======================================================================= Toggle
Toggle::Toggle (AudioProcessorValueTreeState& apvts, const String& paramId, const String& t, Style s, Colour a)
    : text (t), style (s), accent (a)
{
    auto* p = apvts.getParameter (paramId);
    jassert (p != nullptr);
    attachment = std::make_unique<ParameterAttachment> (*p, [this] (float v) { state = v > 0.5f; repaint(); }, nullptr);
    attachment->sendInitialUpdate();
    setTooltip (p->getName (64));
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void Toggle::mouseDown (const MouseEvent&) { attachment->setValueAsCompleteGesture (state ? 0.f : 1.f); }

void Toggle::paint (Graphics& g)
{
    const auto& t = tokens();
    auto& lnf = lnfOf (*this);
    const bool hover = isMouseOver();
    auto r = getLocalBounds().toFloat();

    if (style == Pill)
    {
        auto track = Rectangle<float> (30.f, 16.f).withCentre ({ r.getRight() - 16.f, r.getCentreY() });
        if (text.isNotEmpty())
        {
            g.setFont (lnf.font (11.f, SemiBold, 0.04f));
            g.setColour (state ? t.text : t.textMute);
            g.drawText (text, r.withRight (track.getX() - 8.f), Justification::centredRight);
        }
        g.setColour (state ? accent : (hover ? t.border.brighter (0.2f) : t.border));
        g.fillRoundedRectangle (track, 8.f);
        const float kx = state ? track.getRight() - 8.f : track.getX() + 8.f;
        g.setColour (state ? Colours::white : t.textDim);
        g.fillEllipse (Rectangle<float> (12.f, 12.f).withCentre ({ kx, track.getCentreY() }));
    }
    else
    {
        auto chip = r.reduced (0.5f);
        g.setColour (state ? accent.withAlpha (0.16f) : t.control);
        g.fillRoundedRectangle (chip, 8.f);
        g.setColour (state ? accent : (hover ? t.textMute : t.border));
        g.drawRoundedRectangle (chip, 8.f, 1.f);
        g.setFont (lnf.font (12.f, SemiBold, 0.03f));
        g.setColour (state ? t.text : t.textDim);
        g.drawText (text, chip, Justification::centred);
    }
}

// ======================================================================= Segmented
Segmented::Segmented (AudioProcessorValueTreeState& apvts, const String& paramId, StringArray l, Colour a)
    : labels (std::move (l)), accent (a)
{
    auto* p = apvts.getParameter (paramId);
    jassert (p != nullptr);
    attachment = std::make_unique<ParameterAttachment> (*p, [this] (float v) { index = (int) std::round (v); repaint(); }, nullptr);
    attachment->sendInitialUpdate();
    setTooltip (p->getName (64));
    setMouseCursor (MouseCursor::PointingHandCursor);
}

Rectangle<float> Segmented::itemBounds (int i) const
{
    const float w = (float) getWidth() / (float) labels.size();
    return { w * (float) i, 0.f, w, (float) getHeight() };
}

int Segmented::itemAt (Point<int> pt) const
{
    return jlimit (0, labels.size() - 1, (int) ((float) pt.x / ((float) getWidth() / (float) labels.size())));
}

void Segmented::mouseDown (const MouseEvent& e) { attachment->setValueAsCompleteGesture ((float) itemAt (e.getPosition())); }
void Segmented::mouseMove (const MouseEvent& e) { const int h = itemAt (e.getPosition()); if (h != hover) { hover = h; repaint(); } }

void Segmented::paint (Graphics& g)
{
    const auto& t = tokens();
    auto& lnf = lnfOf (*this);
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (t.control);
    g.fillRoundedRectangle (r, 8.f);
    g.setColour (t.border);
    g.drawRoundedRectangle (r, 8.f, 1.f);
    for (int i = 0; i < labels.size(); ++i)
    {
        auto it = itemBounds (i).reduced (3.f);
        const bool sel = i == index;
        if (sel)      { g.setColour (t.border); g.fillRoundedRectangle (it, 6.f); g.setColour (accent); g.fillRoundedRectangle (it.getX() + it.getWidth() * 0.3f, it.getBottom() - 2.5f, it.getWidth() * 0.4f, 2.f, 1.f); }
        else if (i == hover) { g.setColour (t.border.withAlpha (0.5f)); g.fillRoundedRectangle (it, 6.f); }
        g.setFont (lnf.font (11.5f, SemiBold, 0.02f));
        g.setColour (sel ? t.text : t.textDim);
        g.drawText (labels[i], it, Justification::centred);
    }
}

// ======================================================================= TabStrip
TabStrip::TabStrip (StringArray n, Colour a) : names (std::move (n)), accent (a), lastOn ((size_t) names.size(), false) {}

void TabStrip::setSelected (int i)
{
    if (! isPositiveAndBelow (i, names.size()) || i == selected) return;
    selected = i;
    repaint();
}

void TabStrip::mouseDown (const MouseEvent& e)
{
    const int i = jlimit (0, names.size() - 1, (int) ((float) e.x / ((float) getWidth() / (float) names.size())));
    setSelected (i);
    if (onSelect) onSelect (i);
}

void TabStrip::mouseMove (const MouseEvent& e)
{
    const int h = jlimit (0, names.size() - 1, (int) ((float) e.x / ((float) getWidth() / (float) names.size())));
    setMouseCursor (MouseCursor::PointingHandCursor);
    if (h != hover) { hover = h; repaint(); }
}

void TabStrip::refresh()
{
    if (! isOn) return;
    bool changed = false;
    for (int i = 0; i < names.size(); ++i)
    {
        const bool on = isOn (i);
        if (on != lastOn[(size_t) i]) { lastOn[(size_t) i] = on; changed = true; }
    }
    if (changed) repaint();
}

void TabStrip::paint (Graphics& g)
{
    const auto& t = tokens();
    auto& lnf = lnfOf (*this);
    const float w = (float) getWidth() / (float) names.size();
    const float h = (float) getHeight();
    g.setColour (t.divider);
    g.fillRect (0.f, h - 1.f, (float) getWidth(), 1.f);
    for (int i = 0; i < names.size(); ++i)
    {
        auto r = Rectangle<float> (w * (float) i, 0.f, w, h - 1.f);
        const bool sel = i == selected;
        if (i == hover && ! sel) { g.setColour (t.border.withAlpha (0.35f)); g.fillRoundedRectangle (r.reduced (2.f, 4.f), 6.f); }
        g.setFont (lnf.font (12.f, sel ? Bold : SemiBold, 0.04f));
        const auto textW = GlyphArrangement::getStringWidth (g.getCurrentFont(), names[i]);
        const bool dot = isOn != nullptr;
        const float total = textW + (dot ? 12.f : 0.f);
        float x = r.getCentreX() - total * 0.5f;
        if (dot)
        {
            const bool on = lastOn[(size_t) i];
            g.setColour (on ? accent : t.border.brighter (0.1f));
            g.fillEllipse (x, r.getCentreY() - 3.f, 6.f, 6.f);
            x += 12.f;
        }
        g.setColour (sel ? t.text : (i == hover ? t.text.withAlpha (0.85f) : t.textDim));
        g.drawText (names[i], Rectangle<float> (x, r.getY(), textW + 4.f, r.getHeight()), Justification::centredLeft);
        if (sel)
        {
            g.setColour (accent);
            g.fillRoundedRectangle (r.getCentreX() - total * 0.5f, h - 2.5f, total, 2.5f, 1.2f);
        }
    }
}

// ======================================================================= card
void card (Graphics& g, FlatLookAndFeel& lnf, Rectangle<int> area, const String& title, Colour dot)
{
    const auto& t = tokens();
    g.setColour (t.surface);
    g.fillRoundedRectangle (area.toFloat(), 12.f);
    if (title.isEmpty()) return;
    float x = (float) area.getX() + 16.f;
    if (! dot.isTransparent())
    {
        g.setColour (dot);
        g.fillEllipse (x, (float) area.getY() + 17.f, 6.f, 6.f);
        x += 12.f;
    }
    g.setColour (t.textDim);
    g.setFont (lnf.font (12.f, Bold, 0.12f));
    g.drawText (title.toUpperCase(), Rectangle<float> (x, (float) area.getY() + 10.f, (float) area.getWidth() - 40.f, 20.f), Justification::centredLeft);
}
}
