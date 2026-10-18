#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

// ======================================================================= LookAndFeel
GoldLookAndFeel::GoldLookAndFeel()
{
    tfTitle = Typeface::createSystemTypefaceFor (BinaryData::CinzelVF_ttf, BinaryData::CinzelVF_ttfSize);
    tfHead  = Typeface::createSystemTypefaceFor (BinaryData::MichromaRegular_ttf, BinaryData::MichromaRegular_ttfSize);
    tfLabel = Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedSemiBold_ttf, BinaryData::BarlowSemiCondensedSemiBold_ttfSize);
    tfValue = Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedMedium_ttf, BinaryData::BarlowSemiCondensedMedium_ttfSize);
    tfMono  = Typeface::createSystemTypefaceFor (BinaryData::JetBrainsMonoVF_ttf, BinaryData::JetBrainsMonoVF_ttfSize);

    setColour (PopupMenu::backgroundColourId, Colour (0xfff8f4ec));
    setColour (PopupMenu::textColourId, Palette::ink);
    setColour (PopupMenu::headerTextColourId, Palette::goldDeep);
    setColour (PopupMenu::highlightedBackgroundColourId, Palette::gold);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (ComboBox::textColourId, Palette::ink);
    setColour (ComboBox::arrowColourId, Palette::gold);
    setColour (Label::textColourId, Palette::ink);
    setColour (TextEditor::backgroundColourId, Colours::white);
    setColour (TextEditor::textColourId, Palette::ink);
    setColour (TextEditor::highlightColourId, Palette::goldLight);
    setColour (TextEditor::outlineColourId, Palette::gold.withAlpha (0.5f));
    setColour (TextEditor::focusedOutlineColourId, Palette::gold);
    setColour (CaretComponent::caretColourId, Palette::goldDeep);
    setColour (AlertWindow::backgroundColourId, Colour (0xfff8f4ec));
    setColour (AlertWindow::textColourId, Palette::ink);
    setColour (AlertWindow::outlineColourId, Palette::gold);
    setColour (TextButton::textColourOffId, Palette::ink);
    setColour (TextButton::textColourOnId, Colours::white);
    setColour (TooltipWindow::backgroundColourId, Colour (0xfffbf8f2));
    setColour (TooltipWindow::textColourId, Palette::ink);
    setColour (TooltipWindow::outlineColourId, Palette::gold.withAlpha (0.6f));
}

static Font makeFont (const Typeface::Ptr& tf, float h, float k)
{
    auto o = FontOptions (tf).withHeight (h);
    if (k != 0.f) o = o.withKerningFactor (k);
    return Font (o);
}
Font GoldLookAndFeel::titleFont (float h, float k) const { return makeFont (tfTitle, h, k); }
Font GoldLookAndFeel::headFont  (float h, float k) const { return makeFont (tfHead, h, k); }
Font GoldLookAndFeel::labelFont (float h, float k) const { return makeFont (tfLabel, h, k); }
Font GoldLookAndFeel::valueFont (float h, float k) const { return makeFont (tfValue, h, k); }
Font GoldLookAndFeel::monoFont  (float h, float k) const { return makeFont (tfMono, h, k); }

GoldLookAndFeel& goldLnf (Component& c) { return dynamic_cast<GoldLookAndFeel&> (c.getLookAndFeel()); }

void GoldLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, Slider& s)
{
    const bool bipolar = (bool) s.getProperties()["bipolar"];
    const bool hover = s.isMouseOverOrDragging() && s.isEnabled();
    auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.f);
    const float size = jmin (bounds.getWidth(), bounds.getHeight());
    bounds = bounds.withSizeKeepingCentre (size, size);
    const auto c = bounds.getCentre();
    const float r = size * 0.5f;
    const float ringW = 3.f;
    const float ringR = r - 5.f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    for (int i = 0; i < 11; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 10.f;
        g.setColour (a <= angle + 0.001f ? Palette::gold : Colour (0xffd9cfbd));
        g.fillEllipse (Rectangle<float> (2.f, 2.f).withCentre (c.getPointOnCircumference (r - 0.5f, a)));
    }

    Path track;
    track.addCentredArc (c.x, c.y, ringR, ringR, 0.f, startAngle, endAngle, true);
    g.setColour (Colour (0xffe4dac7));
    g.strokePath (track, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));

    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.002f)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, ringR, ringR, 0.f, jmin (from, angle), jmax (from, angle), true);
        g.setColour (Palette::gold.withAlpha (hover ? 0.28f : 0.16f));
        g.strokePath (arc, PathStrokeType (ringW * 3.2f, PathStrokeType::curved, PathStrokeType::rounded));
        ColourGradient grad (Palette::goldDeep, c.x - ringR, c.y + ringR, Palette::goldLight, c.x + ringR, c.y - ringR, false);
        grad.addColour (0.5, Palette::gold);
        g.setGradientFill (grad);
        g.strokePath (arc, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // pearl body
    const float bodyR = ringR - ringW - 4.5f;
    const auto body = Rectangle<float> (bodyR * 2.f, bodyR * 2.f).withCentre (c);
    g.setColour (Colour (0x40403420));
    g.fillEllipse (body.translated (0.f, 2.5f).expanded (1.5f));
    ColourGradient bg (Colours::white, c.x - bodyR * 0.4f, c.y - bodyR * 0.6f, Colour (0xffded5c4), c.x + bodyR * 0.5f, c.y + bodyR, true);
    g.setGradientFill (bg);
    g.fillEllipse (body);
    ColourGradient rim (Colours::white, c.x, body.getY(), Colour (0xffb3a68f), c.x, body.getBottom(), false);
    g.setGradientFill (rim);
    g.drawEllipse (body.reduced (0.5f), 1.f);
    g.setColour (Colour (0x30a08a60));
    g.drawEllipse (body.reduced (bodyR * 0.3f), 0.8f);

    const auto p1 = c.getPointOnCircumference (bodyR * 0.36f, angle);
    const auto p2 = c.getPointOnCircumference (bodyR * 0.86f, angle);
    if (hover) { g.setColour (Palette::gold.withAlpha (0.45f)); g.drawLine ({ p1, p2 }, 5.f); }
    g.setColour (s.isEnabled() ? Palette::ink : Palette::dim);
    g.drawLine ({ p1, p2 }, 2.f);
}

Label* GoldLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (valueFont (13.f, 0.02f));
    l->setColour (Label::textColourId, Palette::ink);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setColour (Label::textWhenEditingColourId, Palette::ink);
    l->setColour (Label::outlineWhenEditingColourId, Palette::gold);
    l->setColour (Label::backgroundWhenEditingColourId, Colours::white);
    l->setJustificationType (Justification::centred);
    return l;
}

void GoldLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (0.5f);
    g.setColour (Colours::white.withAlpha (0.85f));
    g.fillRoundedRectangle (r, 6.f);
    g.setColour (box.isMouseOver (true) ? Palette::gold : Palette::gold.withAlpha (0.45f));
    g.drawRoundedRectangle (r, 6.f, 1.f);
    Path chev;
    const float ax = (float) w - 15.f, ay = (float) h * 0.5f;
    chev.startNewSubPath (ax - 4.f, ay - 2.f); chev.lineTo (ax, ay + 2.f); chev.lineTo (ax + 4.f, ay - 2.f);
    g.setColour (Palette::gold);
    g.strokePath (chev, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
}

Font GoldLookAndFeel::getComboBoxFont (ComboBox&) { return labelFont (15.f, 0.03f); }

void GoldLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (Justification::centredLeft);
}

Font GoldLookAndFeel::getPopupMenuFont() { return valueFont (16.f); }

void GoldLookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (findColour (PopupMenu::backgroundColourId));
    g.setColour (Palette::gold.withAlpha (0.7f));
    g.drawRect (0, 0, w, h, 1);
}

void GoldLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool hover, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getHeight() * 0.5f;
    if (down)
    {
        ColourGradient gr (Palette::goldLight, 0.f, r.getY(), Palette::gold, 0.f, r.getBottom(), false);
        g.setGradientFill (gr);
    }
    else g.setColour (Colours::white.withAlpha (0.85f));
    g.fillRoundedRectangle (r, rad);
    if (hover) { g.setColour (Palette::gold.withAlpha (0.2f)); g.drawRoundedRectangle (r.expanded (1.5f), rad + 1.5f, 3.f); }
    g.setColour (hover ? Palette::gold : Palette::gold.withAlpha (0.5f));
    g.drawRoundedRectangle (r, rad, 1.f);
}

void GoldLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool hover, bool)
{
    g.setFont (labelFont (b.getButtonText().length() <= 1 ? 15.f : 12.f, 0.2f));
    g.setColour (hover ? Palette::goldDeep : Palette::ink);
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
}

Font GoldLookAndFeel::getAlertWindowTitleFont()   { return titleFont (20.f, 0.1f); }
Font GoldLookAndFeel::getAlertWindowMessageFont() { return valueFont (15.f); }
Font GoldLookAndFeel::getTextButtonFont (TextButton&, int) { return labelFont (13.f, 0.2f); }

// ======================================================================= Knob / Switch / Segmented
Knob::Knob (AudioProcessorValueTreeState& apvts, const String& paramId, const String& text, bool bipolar) : label (text)
{
    slider.getProperties().set ("bipolar", bipolar);
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 90, 18);
    slider.setMouseDragSensitivity (240);
    slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxTextColourId, Palette::ink);
    slider.setColour (Slider::textBoxHighlightColourId, Palette::goldLight);
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
    auto& lnf = goldLnf (*this);
    g.setFont (lnf.labelFont (11.5f, 0.16f));
    g.setColour (slider.isMouseOverOrDragging() ? Palette::goldDeep : Palette::label);
    g.drawText (label, labelArea, Justification::centred);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    const int need = 18 + 58 + 18;
    if (r.getHeight() > need) r = r.withSizeKeepingCentre (r.getWidth(), need);
    labelArea = r.removeFromTop (18);
    slider.setBounds (r);
}

Switch::Switch (AudioProcessorValueTreeState& apvts, const String& paramId, const String& t, Style s) : text (t), style (s)
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
    auto& lnf = goldLnf (*this);
    const bool hover = isMouseOver();
    auto r = getLocalBounds().toFloat();

    if (style == Power)
    {
        auto pill = Rectangle<float> (34.f, 18.f).withCentre ({ r.getRight() - 18.f, r.getCentreY() });
        if (text.isNotEmpty())
        {
            g.setFont (lnf.labelFont (11.f, 0.2f));
            g.setColour (state ? Palette::goldDeep : Palette::dim);
            g.drawText (text, r.withRight (pill.getX() - 8.f), Justification::centredRight);
        }
        if (state)
        {
            g.setColour (Palette::gold.withAlpha (0.25f));
            g.fillRoundedRectangle (pill.expanded (2.5f), 11.f);
            ColourGradient gr (Palette::goldLight, pill.getX(), pill.getY(), Palette::goldDeep, pill.getX(), pill.getBottom(), false);
            gr.addColour (0.5, Palette::gold);
            g.setGradientFill (gr);
        }
        else g.setColour (Colour (0xffddd5c7));
        g.fillRoundedRectangle (pill, 9.f);
        g.setColour (state ? Palette::goldDeep.withAlpha (0.8f) : Colour (hover ? 0xffb6a990 : 0xffc9bfae));
        g.drawRoundedRectangle (pill.reduced (0.5f), 9.f, 1.f);
        const float kx = state ? pill.getRight() - 9.f : pill.getX() + 9.f;
        auto knobR = Rectangle<float> (12.f, 12.f).withCentre ({ kx, pill.getCentreY() });
        g.setColour (Colour (0x40000000)); g.fillEllipse (knobR.translated (0.f, 1.f));
        g.setColour (Colours::white); g.fillEllipse (knobR);
    }
    else
    {
        auto chip = r.reduced (1.f);
        const float rad = chip.getHeight() * 0.5f;
        if (state)
        {
            ColourGradient gr (Palette::goldLight, chip.getX(), chip.getY(), Palette::gold, chip.getX(), chip.getBottom(), false);
            g.setGradientFill (gr);
        }
        else g.setColour (Colours::white.withAlpha (0.75f));
        g.fillRoundedRectangle (chip, rad);
        g.setColour (state ? Palette::goldDeep.withAlpha (0.7f) : (hover ? Palette::gold : Palette::gold.withAlpha (0.4f)));
        g.drawRoundedRectangle (chip, rad, 1.f);
        g.setFont (lnf.labelFont (11.5f, 0.18f));
        g.setColour (state ? Colours::white : Palette::label);
        g.drawText (text, chip, Justification::centred);
    }
}

Segmented::Segmented (AudioProcessorValueTreeState& apvts, const String& paramId, StringArray l) : labels (std::move (l))
{
    auto* p = apvts.getParameter (paramId);
    jassert (p != nullptr);
    attachment = std::make_unique<ParameterAttachment> (*p, [this] (float v) { index = (int) std::round (v); repaint(); }, nullptr);
    attachment->sendInitialUpdate();
    setTooltip (p->getName (64));
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void Segmented::mouseDown (const MouseEvent& e)
{
    const float ih = (float) getHeight() / (float) labels.size();
    attachment->setValueAsCompleteGesture ((float) jlimit (0, labels.size() - 1, (int) ((float) e.y / ih)));
}

void Segmented::paint (Graphics& g)
{
    auto& lnf = goldLnf (*this);
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::white.withAlpha (0.6f));
    g.fillRoundedRectangle (r, 6.f);
    g.setColour (Palette::gold.withAlpha (0.35f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.f, 1.f);
    const float ih = r.getHeight() / (float) labels.size();
    for (int i = 0; i < labels.size(); ++i)
    {
        auto item = Rectangle<float> (r.getX(), r.getY() + ih * (float) i, r.getWidth(), ih).reduced (3.f, 2.f);
        if (i == index)
        {
            ColourGradient gr (Palette::goldLight, item.getX(), item.getY(), Palette::gold, item.getX(), item.getBottom(), false);
            g.setGradientFill (gr);
            g.fillRoundedRectangle (item, 4.f);
        }
        g.setFont (lnf.labelFont (11.5f, 0.16f));
        g.setColour (i == index ? Colours::white : Palette::label);
        g.drawText (labels[i], item, Justification::centred);
    }
}

// ======================================================================= LayerControls
static const char* layerNames[] = { "PUNCH", "CRUNCH", "KICK" };

LayerControls::LayerControls (AncientProcessor& p, int l)
    : proc (p), layer (l),
      onSwitch (p.apvts, ParamIDs::id (l, ParamIDs::on), "", Switch::Power),
      revChip (p.apvts, ParamIDs::id (l, ParamIDs::rev), "REVERSE", Switch::Chip),
      forgeChip (p.apvts, ParamIDs::id (l, ParamIDs::forge), "FORGE", Switch::Chip)
{
    using namespace ParamIDs;
    for (auto* c : { (Component*) &onSwitch, (Component*) &revChip, (Component*) &forgeChip }) addAndMakeVisible (c);
    forgeChip.setTooltip ("Send this layer through the FORGE effects");
    if (layer == Punch)
    {
        keyChip = std::make_unique<Switch> (p.apvts, punchKey, "KEY", Switch::Chip);
        keyChip->setTooltip ("Punch follows the played note");
        addAndMakeVisible (*keyChip);
    }

    source.addItemList (Library::choicesFor (layer), 1);
    addAndMakeVisible (source);
    sourceAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, id (layer, src), source);
    source.setTooltip ("Sample from the built-in library or your own (USER SAMPLE)");

    loadButton.setTooltip ("Load your own sample (wav / aiff / flac / ogg / mp3)");
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> (String ("Load ") + layerNames[layer] + " sample", File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [this] (const FileChooser& fc) { if (fc.getResult().existsAsFile()) proc.loadUserSample (layer, fc.getResult()); });
    };
    resetEnvButton.setTooltip ("Reset the volume envelope of this layer");
    resetEnvButton.onClick = [this]
    {
        auto s = proc.getLayerSample (layer);
        if (s != nullptr && s->isUser)
        {
            const float ms = (float) ((double) s->data.size() / s->sampleRate * 1000.0);
            proc.setEnvelope (layer, Envelope::make (ms, jmin (30.f, ms * 0.1f)));
        }
        else proc.setEnvelope (layer, AncientProcessor::defaultEnvelope (layer));
    };
    addAndMakeVisible (loadButton);
    addAndMakeVisible (resetEnvButton);

    const std::pair<const char*, const char*> k[] = { { level, "LEVEL" }, { pitch, "PITCH" }, { offset, "OFFSET" },
                                                       { hp, "LOW CUT" }, { lp, "HIGH CUT" } };
    for (auto& [suffix, name] : k)
    {
        const String sfx (suffix);
        knobs.push_back (std::make_unique<Knob> (p.apvts, id (layer, suffix), name, sfx == pitch || sfx == offset));
        addAndMakeVisible (*knobs.back());
    }
}

void LayerControls::refresh()
{
    String now;
    if (auto s = proc.getLayerSample (layer))
        now = (s->isUser ? "USER  /  " : "LIBRARY  /  ") + s->name + "  /  " + String (roundToInt ((double) s->data.size() / s->sampleRate * 1000.0)) + " ms";
    else if (proc.getUserSamplePath (layer).isNotEmpty())
        now = "MISSING FILE: " + File (proc.getUserSamplePath (layer)).getFileName();
    else
        now = "NO SAMPLE";
    if (now != info) { info = now; repaint(); }
}

bool LayerControls::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files) if (AncientProcessor::isSupportedAudioFile (f)) return true;
    return false;
}

void LayerControls::filesDropped (const StringArray& files, int, int)
{
    for (auto& f : files) if (AncientProcessor::isSupportedAudioFile (f)) { proc.loadUserSample (layer, File (f)); break; }
}

void LayerControls::paint (Graphics& g)
{
    auto& lnf = goldLnf (*this);
    auto r = getLocalBounds().reduced (16, 12);
    auto head = r.removeFromTop (28);
    auto dot = Rectangle<float> (10.f, 10.f).withCentre ({ (float) head.getX() + 6.f, (float) head.getCentreY() });
    g.setColour (Palette::layerColour (layer).withAlpha (0.35f)); g.fillEllipse (dot.expanded (3.f));
    g.setColour (Palette::layerColour (layer).darker (0.4f)); g.fillEllipse (dot);
    g.setFont (lnf.headFont (15.f, 0.25f));
    g.setColour (Palette::ink);
    g.drawText (layerNames[layer], head.withTrimmedLeft (20), Justification::centredLeft);

    g.setFont (lnf.labelFont (11.f, 0.06f));
    g.setColour (info.startsWith ("MISSING") ? Colour (0xffb0402a) : Palette::label);
    g.drawText (info, getLocalBounds().reduced (16, 0).removeFromBottom (30), Justification::centred);
}

void LayerControls::resized()
{
    auto r = getLocalBounds().reduced (16, 12);
    auto head = r.removeFromTop (28);
    onSwitch.setBounds (head.removeFromRight (60));
    r.removeFromTop (12);
    auto srcRow = r.removeFromTop (30);
    loadButton.setBounds (srcRow.removeFromRight (64));
    srcRow.removeFromRight (8);
    source.setBounds (srcRow);
    r.removeFromTop (12);
    auto chips = r.removeFromTop (26);
    const int cw = (chips.getWidth() - 16) / 3;
    revChip.setBounds (chips.removeFromLeft (cw)); chips.removeFromLeft (8);
    forgeChip.setBounds (chips.removeFromLeft (cw)); chips.removeFromLeft (8);
    if (keyChip) keyChip->setBounds (chips);
    r.removeFromTop (8);
    r.removeFromBottom (30);

    const int rowH = r.getHeight() / 2;
    const int colW = r.getWidth() / 3;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int row = (int) i / 3, col = (int) i % 3;
        knobs[i]->setBounds (r.getX() + col * colW, r.getY() + row * rowH, colW, rowH);
    }
    auto cell = Rectangle<int> (r.getX() + 2 * colW, r.getY() + rowH, colW, rowH);
    resetEnvButton.setBounds (cell.withSizeKeepingCentre (colW - 4, 26));
    resetEnvButton.setButtonText ("RESET ENV");
}

// ======================================================================= KickScreen
constexpr float KickScreen::zooms[3];

KickScreen::KickScreen (AncientProcessor& p) : proc (p)
{
    setTooltip ("Drag the waveform: move the layer in time (Shift = fine)\n"
                "Drag a point: shape the volume\n"
                "Double-click: add a point / remove a point\n"
                "Right-click a point: delete it\n"
                "Drop an audio file here to load it into the selected layer");
}

void KickScreen::resized()
{
    auto inner = getLocalBounds().toFloat().reduced (9.f);
    plot = inner.withTrimmedTop (42.f).withTrimmedBottom (26.f).reduced (14.f, 0.f);
}

Rectangle<int> KickScreen::chipBounds (int i) const { return { 22 + i * 98, 18, 92, 24 }; }
Rectangle<int> KickScreen::zoomBounds (int i) const { return { getWidth() - 22 - (3 - i) * 58, 18, 54, 24 }; }

RangedAudioParameter* KickScreen::offsetParam (int layer) const
{
    return proc.apvts.getParameter (ParamIDs::id (layer, ParamIDs::offset));
}

KickScreen::LayerView KickScreen::viewOf (int l) const
{
    LayerView v;
    v.sample = proc.getLayerSample (l);
    v.env = proc.getEnvelope (l);
    auto raw = [this, l] (const char* s) { return proc.apvts.getRawParameterValue (ParamIDs::id (l, s))->load(); };
    v.offsetMs = raw (ParamIDs::offset);
    v.on = raw (ParamIDs::on) > 0.5f;
    v.reverse = raw (ParamIDs::rev) > 0.5f;
    v.rate = std::pow (2.f, raw (ParamIDs::pitch) / 12.f);
    return v;
}

float KickScreen::msToX (float ms) const { return plot.getX() + ms / proc.getZoomMs() * plot.getWidth(); }
float KickScreen::xToMs (float x) const { return (x - plot.getX()) / plot.getWidth() * proc.getZoomMs(); }
float KickScreen::gainToY (float g) const { return plot.getBottom() - 8.f - g * (plot.getHeight() - 16.f); }
float KickScreen::yToGain (float y) const { return jlimit (0.f, 1.f, (plot.getBottom() - 8.f - y) / (plot.getHeight() - 16.f)); }

int KickScreen::nodeAt (Point<float> pt) const
{
    const int sel = proc.getSelectedLayer();
    auto env = proc.getEnvelope (sel);
    if (env == nullptr) return -1;
    const float off = viewOf (sel).offsetMs;
    int best = -1; float bestD = 10.f;
    for (size_t i = 0; i < env->points.size(); ++i)
    {
        const Point<float> np (msToX (off + env->points[i].ms), gainToY (env->points[i].gain));
        const float d = np.getDistanceFrom (pt);
        if (d < bestD) { bestD = d; best = (int) i; }
    }
    return best;
}

void KickScreen::refresh()
{
    String sig;
    sig << proc.getSelectedLayer() << "|" << proc.getZoomMs();
    for (int l = 0; l < NumSampleLayers; ++l)
    {
        auto v = viewOf (l);
        sig << "|" << String::toHexString ((pointer_sized_int) v.sample.get()) << String::toHexString ((pointer_sized_int) v.env.get())
            << v.offsetMs << v.rate << (int) v.on << (int) v.reverse;
    }
    const int s = proc.getHitSerial(), len = proc.getHitLength();
    const bool playing = len != lastLen || s != lastSerial;
    lastSerial = s; lastLen = len;
    if (sig != lastSignature || playing) { lastSignature = sig; repaint(); }
}

void KickScreen::drawLayer (Graphics& g, int l, bool selected)
{
    auto v = viewOf (l);
    if (v.sample == nullptr || v.sample->data.size() < 4) return;
    const auto& d = v.sample->data;
    const double len = (double) d.size() - 1.0;
    const int x0 = (int) plot.getX(), x1 = (int) plot.getRight();
    const float mid = plot.getCentreY(), amp = plot.getHeight() * 0.40f;
    const double perMs = v.sample->sampleRate / 1000.0 * v.rate;

    Path fill, edgeTop, edgeBot;
    std::vector<Point<float>> top, bot;
    size_t seg = 0;
    for (int x = x0; x <= x1; ++x)
    {
        const float msA = xToMs ((float) x) - v.offsetMs, msB = xToMs ((float) x + 1.f) - v.offsetMs;
        if (msB <= 0.f) continue;
        const double pa = jmax (0.0, (double) msA * perMs), pb = jmax (pa + 1.0, (double) msB * perMs);
        if (pa >= len) break;
        float g0 = 1.f;
        if (v.env != nullptr)
        {
            if (v.env->endsSilent() && msA >= v.env->endMs()) break;
            g0 = v.env->eval (jmax (0.f, msA), seg);
        }
        float lo = 0.f, hi = 0.f;
        const int ia = (int) pa, ib = (int) jmin (len, pb);
        const int step = jmax (1, (ib - ia) / 64);
        for (int i = ia; i <= ib; i += step)
        {
            const float s = d[(size_t) (v.reverse ? (int) len - i : i)];
            lo = jmin (lo, s); hi = jmax (hi, s);
        }
        top.push_back ({ (float) x, mid - hi * g0 * amp });
        bot.push_back ({ (float) x, mid - lo * g0 * amp });
    }
    if (top.size() < 2) return;

    fill.startNewSubPath (top.front());
    for (auto& pt : top) fill.lineTo (pt);
    for (auto it = bot.rbegin(); it != bot.rend(); ++it) fill.lineTo (*it);
    fill.closeSubPath();
    for (size_t i = 0; i < top.size(); ++i)
    {
        if (i == 0) { edgeTop.startNewSubPath (top[i]); edgeBot.startNewSubPath (bot[i]); }
        else { edgeTop.lineTo (top[i]); edgeBot.lineTo (bot[i]); }
    }

    const auto col = Palette::layerColour (l);
    const float onA = v.on ? 1.f : 0.35f;
    if (selected)
    {
        ColourGradient fg (col.withAlpha (0.55f * onA), 0.f, mid - amp, col.withAlpha (0.18f * onA), 0.f, mid, false);
        g.setGradientFill (fg);
        g.fillPath (fill);
        g.setColour (col.withAlpha (0.25f * onA));
        g.strokePath (edgeTop, PathStrokeType (4.f));
        g.strokePath (edgeBot, PathStrokeType (4.f));
        g.setColour (Palette::ice.withAlpha (0.95f * onA));
        g.strokePath (edgeTop, PathStrokeType (1.3f));
        g.strokePath (edgeBot, PathStrokeType (1.3f));
    }
    else
    {
        g.setColour (col.withAlpha (0.20f * onA));
        g.fillPath (fill);
        g.setColour (col.withAlpha (0.5f * onA));
        g.strokePath (edgeTop, PathStrokeType (1.f));
        g.strokePath (edgeBot, PathStrokeType (1.f));
    }

    // start marker
    const float sx = msToX (jmax (0.f, v.offsetMs));
    g.setColour (col.withAlpha ((selected ? 0.8f : 0.3f) * onA));
    g.drawVerticalLine ((int) sx, plot.getY() + 4.f, plot.getBottom() - 4.f);
}

void KickScreen::drawEnvelope (Graphics& g, int l)
{
    auto v = viewOf (l);
    if (v.env == nullptr) return;
    const auto& pts = v.env->points;
    Path line;
    const float startX = msToX (jmax (0.f, v.offsetMs));
    line.startNewSubPath (jmax (startX, msToX (v.offsetMs)), gainToY (pts.front().gain));
    for (auto& p : pts) line.lineTo (msToX (v.offsetMs + p.ms), gainToY (p.gain));
    if (! v.env->endsSilent() && v.sample != nullptr)
    {
        const float endMs = v.offsetMs + (float) ((double) v.sample->data.size() / v.sample->sampleRate * 1000.0 / v.rate);
        line.lineTo (msToX (endMs), gainToY (pts.back().gain));
    }

    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (plot.toNearestInt());
        g.setColour (Colours::white.withAlpha (0.18f));
        g.strokePath (line, PathStrokeType (5.f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colours::white.withAlpha (0.9f));
        g.strokePath (line, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (plot.expanded (9.f).toNearestInt());

    for (size_t i = 0; i < pts.size(); ++i)
    {
        const Point<float> c (msToX (v.offsetMs + pts[i].ms), gainToY (pts[i].gain));
        const bool hot = (int) i == hoverNode || (int) i == dragNode;
        const float r = hot ? 7.f : 5.f;
        g.setColour (Palette::cyan.withAlpha (hot ? 0.45f : 0.25f));
        g.fillEllipse (Rectangle<float> (r * 2.f + 6.f, r * 2.f + 6.f).withCentre (c));
        g.setColour (Palette::lcdDeep);
        g.fillEllipse (Rectangle<float> (r * 2.f, r * 2.f).withCentre (c));
        g.setColour (hot ? Colours::white : Palette::ice);
        g.drawEllipse (Rectangle<float> (r * 2.f, r * 2.f).withCentre (c), 1.6f);
    }
}

void KickScreen::paint (Graphics& g)
{
    auto& lnf = goldLnf (*this);
    auto outer = getLocalBounds().toFloat();

    // gold bezel
    {
        Path bez; bez.addRoundedRectangle (outer.reduced (1.f), 16.f);
        ColourGradient bg (Palette::goldLight, 0.f, outer.getY(), Palette::goldDeep, 0.f, outer.getBottom(), false);
        bg.addColour (0.5, Palette::gold);
        g.setGradientFill (bg);
        g.fillPath (bez);
        g.setColour (Colours::white.withAlpha (0.6f));
        g.drawRoundedRectangle (outer.reduced (1.5f), 16.f, 1.f);
    }
    auto inner = outer.reduced (9.f);
    {
        ColourGradient lg (Palette::lcd, inner.getCentreX(), inner.getY(), Palette::lcdDeep, inner.getCentreX(), inner.getBottom(), false);
        g.setGradientFill (lg);
        g.fillRoundedRectangle (inner, 10.f);
        ColourGradient glow (Palette::blue.withAlpha (0.20f), inner.getCentreX(), inner.getCentreY(), Colours::transparentBlack,
                             inner.getCentreX(), inner.getY() - 60.f, true);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (inner, 10.f);
    }

    const int sel = proc.getSelectedLayer();
    const float zoom = proc.getZoomMs();

    // chips
    for (int i = 0; i < 3; ++i)
    {
        auto c = chipBounds (i).toFloat();
        const bool on = proc.apvts.getRawParameterValue (ParamIDs::id (i, ParamIDs::on))->load() > 0.5f;
        const auto col = Palette::layerColour (i);
        if (i == sel)
        {
            g.setColour (col.withAlpha (0.22f)); g.fillRoundedRectangle (c, 5.f);
            g.setColour (col); g.drawRoundedRectangle (c, 5.f, 1.2f);
        }
        else { g.setColour (Palette::grid.withAlpha (0.8f)); g.drawRoundedRectangle (c, 5.f, 1.f); }
        auto led = Rectangle<float> (6.f, 6.f).withCentre ({ c.getX() + 12.f, c.getCentreY() });
        if (on) { g.setColour (col.withAlpha (0.5f)); g.fillEllipse (led.expanded (3.f)); }
        g.setColour (on ? col : Palette::grid.brighter (0.4f)); g.fillEllipse (led);
        g.setFont (lnf.monoFont (12.f, 0.08f));
        g.setColour (i == sel ? Palette::ice : col.withAlpha (0.55f));
        g.drawText (layerNames[i], c.withTrimmedLeft (18.f), Justification::centred);
    }
    for (int i = 0; i < 3; ++i)
    {
        auto c = zoomBounds (i).toFloat();
        const bool cur = std::abs (zooms[i] - zoom) < 1.f;
        g.setColour (cur ? Palette::cyan.withAlpha (0.2f) : Colours::transparentBlack); g.fillRoundedRectangle (c, 5.f);
        g.setColour (cur ? Palette::cyan : Palette::grid.brighter (0.3f)); g.drawRoundedRectangle (c, 5.f, 1.f);
        g.setFont (lnf.monoFont (11.5f));
        g.setColour (cur ? Palette::ice : Palette::cyan.withAlpha (0.5f));
        g.drawText (zooms[i] < 1000.f ? "0.5s" : String ((int) (zooms[i] / 1000.f)) + "s", c, Justification::centred);
    }

    // grid + ruler
    {
        Graphics::ScopedSaveState ss (g);
        const float step = zoom / 20.f;
        g.setFont (lnf.monoFont (10.f));
        for (int i = 0; i <= 20; ++i)
        {
            const float x = msToX (step * (float) i);
            const bool major = i % 4 == 0;
            g.setColour (major ? Palette::grid.brighter (0.25f) : Palette::grid);
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            if (major)
            {
                g.setColour (Palette::cyan.withAlpha (0.55f));
                g.drawText (String (roundToInt (step * (float) i)) + "ms", Rectangle<float> (x - 30.f, plot.getBottom() + 5.f, 60.f, 14.f), Justification::centred);
            }
        }
        for (int i = 0; i <= 4; ++i)
        {
            g.setColour (i == 2 ? Palette::grid.brighter (0.35f) : Palette::grid);
            g.drawHorizontalLine ((int) (plot.getY() + plot.getHeight() * (float) i / 4.f), plot.getX(), plot.getRight());
        }
    }

    // layers: others first, selected on top
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (plot.toNearestInt());
        for (int l = 0; l < NumSampleLayers; ++l) if (l != sel) drawLayer (g, l, false);
        drawLayer (g, sel, true);
    }
    drawEnvelope (g, sel);

    // playhead of the last hit
    {
        const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
        const float ms = (float) (proc.getHitLength() / sr * 1000.0);
        if (proc.getHitLength() > 0 && ms < zoom - 1.f && ms < 1990.f)
        {
            const float x = msToX (ms);
            g.setColour (Palette::cyan.withAlpha (0.25f)); g.fillRect (x - 3.f, plot.getY(), 6.f, plot.getHeight());
            g.setColour (Palette::ice); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        }
    }

    // readout + hint
    {
        auto v = viewOf (sel);
        String info = String (layerNames[sel]) + "   OFFSET " + (v.offsetMs >= 0.f ? "+" : "") + String (v.offsetMs, 1) + " ms";
        if (v.env != nullptr) info << "   END " << roundToInt (v.offsetMs + v.env->endMs()) << " ms";
        if (v.reverse) info << "   REVERSE";
        g.setFont (lnf.monoFont (11.f));
        g.setColour (Palette::ice.withAlpha (0.85f));
        g.drawText (info, plot.reduced (8.f, 6.f).toNearestInt(), Justification::topRight);

    }

    // scanlines + glass
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (inner.toNearestInt());
        g.setColour (Colours::black.withAlpha (0.12f));
        for (float y = inner.getY(); y < inner.getBottom(); y += 3.f) g.fillRect (inner.getX(), y, inner.getWidth(), 1.f);
        ColourGradient gl (Colours::white.withAlpha (0.07f), inner.getX(), inner.getY(), Colours::white.withAlpha (0.f),
                           inner.getX() + inner.getWidth() * 0.45f, inner.getY() + inner.getHeight() * 0.6f, false);
        g.setGradientFill (gl);
        Path sheen;
        sheen.addTriangle (inner.getX(), inner.getY(), inner.getX() + inner.getWidth() * 0.7f, inner.getY(), inner.getX(), inner.getY() + inner.getHeight() * 0.8f);
        g.fillPath (sheen);
    }
    if (dragHover)
    {
        g.setColour (Palette::cyan.withAlpha (0.15f)); g.fillRoundedRectangle (inner, 10.f);
        g.setColour (Palette::ice); g.setFont (lnf.monoFont (14.f));
        g.drawText (String ("DROP SAMPLE ON ") + layerNames[sel], inner.toNearestInt(), Justification::centred);
    }
}

void KickScreen::mouseMove (const MouseEvent& e)
{
    const int n = plot.contains (e.position) ? nodeAt (e.position) : -1;
    if (n != hoverNode) { hoverNode = n; repaint(); }
    if (n >= 0) setMouseCursor (MouseCursor::DraggingHandCursor);
    else if (plot.contains (e.position)) setMouseCursor (MouseCursor::LeftRightResizeCursor);
    else
    {
        bool overChip = false;
        for (int i = 0; i < 3; ++i) overChip |= chipBounds (i).contains (e.getPosition()) || zoomBounds (i).contains (e.getPosition());
        setMouseCursor (overChip ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
    }
}

void KickScreen::mouseDown (const MouseEvent& e)
{
    for (int i = 0; i < 3; ++i)
    {
        if (chipBounds (i).contains (e.getPosition())) { proc.setSelectedLayer (i); repaint(); return; }
        if (zoomBounds (i).contains (e.getPosition())) { proc.setZoomMs (zooms[i]); repaint(); return; }
    }
    if (! plot.contains (e.position)) return;

    const int sel = proc.getSelectedLayer();
    const int n = nodeAt (e.position);
    if (n >= 0)
    {
        auto env = proc.getEnvelope (sel);
        editing = *env;
        if (e.mods.isPopupMenu())
        {
            if (editing.points.size() > 2) { editing.points.erase (editing.points.begin() + n); proc.setEnvelope (sel, editing); }
            hoverNode = -1; repaint();
            return;
        }
        dragNode = n;
        return;
    }
    if (e.mods.isPopupMenu()) return;
    if (auto* p = offsetParam (sel))
    {
        draggingOffset = true;
        dragStartMs = xToMs (e.position.x);
        dragStartOffset = p->convertFrom0to1 (p->getValue());
        p->beginChangeGesture();
    }
}

void KickScreen::mouseDrag (const MouseEvent& e)
{
    const int sel = proc.getSelectedLayer();
    if (dragNode >= 0 && dragNode < (int) editing.points.size())
    {
        const float off = viewOf (sel).offsetMs;
        const float lo = dragNode == 0 ? 0.f : editing.points[(size_t) dragNode - 1].ms;
        const float hi = dragNode + 1 < (int) editing.points.size() ? editing.points[(size_t) dragNode + 1].ms : 10000.f;
        auto& pt = editing.points[(size_t) dragNode];
        pt.ms = jlimit (lo, hi, xToMs (e.position.x) - off);
        pt.gain = yToGain (e.position.y);
        proc.setEnvelope (sel, editing);
        repaint();
    }
    else if (draggingOffset)
    {
        if (auto* p = offsetParam (sel))
        {
            const float fine = e.mods.isShiftDown() ? 0.2f : 1.f;
            const float v = dragStartOffset + (xToMs (e.position.x) - dragStartMs) * fine;
            p->setValueNotifyingHost (p->convertTo0to1 (jlimit (-150.f, 300.f, v)));
        }
    }
}

void KickScreen::mouseUp (const MouseEvent&)
{
    if (draggingOffset) if (auto* p = offsetParam (proc.getSelectedLayer())) p->endChangeGesture();
    draggingOffset = false;
    dragNode = -1;
    repaint();
}

void KickScreen::mouseDoubleClick (const MouseEvent& e)
{
    if (! plot.contains (e.position)) return;
    const int sel = proc.getSelectedLayer();
    auto env = proc.getEnvelope (sel);
    if (env == nullptr) return;
    Envelope ed = *env;
    const int n = nodeAt (e.position);
    if (n >= 0)
    {
        if (ed.points.size() > 2) ed.points.erase (ed.points.begin() + n);
    }
    else
    {
        const float ms = xToMs (e.position.x) - viewOf (sel).offsetMs;
        if (ms < 0.f) return;
        ed.points.push_back ({ ms, yToGain (e.position.y) });
    }
    proc.setEnvelope (sel, ed);
    hoverNode = -1;
    repaint();
}

bool KickScreen::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files) if (AncientProcessor::isSupportedAudioFile (f)) return true;
    return false;
}

void KickScreen::filesDropped (const StringArray& files, int, int)
{
    dragHover = false;
    for (auto& f : files)
        if (AncientProcessor::isSupportedAudioFile (f)) { proc.loadUserSample (proc.getSelectedLayer(), File (f)); break; }
    repaint();
}

// ======================================================================= Canvas
Knob* Canvas::knob (const String& id, const String& label, bool bipolar)
{
    auto k = std::make_unique<Knob> (proc.apvts, id, label, bipolar);
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

Canvas::Canvas (AncientProcessor& p) : proc (p), screen (p)
{
    setLookAndFeel (&lnf);
    using namespace ParamIDs;

    for (int l = 0; l < NumSampleLayers; ++l)
    {
        layerControls[(size_t) l] = std::make_unique<LayerControls> (p, l);
        addChildComponent (*layerControls[(size_t) l]);
    }
    addAndMakeVisible (screen);

    forgePower = toggle (fOn, "", Switch::Power);
    forgeCells = { knob (fDrive, "DRIVE"), knob (fFocus, "FOCUS"), knob (fGrit, "GRIT"),
                   knob (fCrunch, "CRUNCH"), knob (fTone, "TONE", true), knob (fOtt, "OTT") };

    subPower = toggle (subOn, "", Switch::Power);
    auto wave = std::make_unique<Segmented> (p.apvts, subWave, StringArray { "SINE", "SAW", "SQR" });
    subCells = { wave.get(), knob (subOct, "OCTAVE", true), knob (subLevel, "LEVEL"), knob (subLength, "LENGTH"),
                 knob (subTone, "TONE"), knob (subDrive, "DRIVE") };
    addAndMakeVisible (*wave);
    owned.push_back (std::move (wave));

    rumPower = toggle (rumOn, "", Switch::Power);
    rumCells = { knob (rumLevel, "LEVEL"), knob (rumDelay, "DELAY"), knob (rumDecay, "DECAY"),
                 knob (rumDrive, "DRIVE"), knob (rumLp, "LOW PASS"), knob (rumDuck, "DUCK") };

    masterCells = { knob (root, "ROOT"), knob (preGain, "PRE-GAIN"), toggle (clipOn, "CLIP", Switch::Chip),
                    knob (clipKnee, "SOFT"), toggle (limOn, "LIMIT", Switch::Chip), knob (volume, "VOLUME") };

    refreshPresetList();
    presetBox.onChange = [this]
    {
        const int pid = presetBox.getSelectedId();
        if (pid <= 0) return;
        if (pid < userIdBase) proc.setCurrentProgram (pid - 1);
        else if (isPositiveAndBelow (pid - userIdBase, userPresetFiles.size())) proc.loadUserPreset (userPresetFiles[pid - userIdBase]);
        lastPresetName = {};
    };
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { showSaveDialog(); };
    randomButton.onClick = [this] { proc.randomize(); };
    playButton.onClick = [this] { proc.triggerTest(); };
    saveButton.setTooltip ("Save preset (with your samples' paths and envelopes)");
    randomButton.setTooltip ("Random kick within musical ranges");
    playButton.setTooltip ("Play the kick at the ROOT note");
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
    saveDialog = std::make_unique<AlertWindow> ("Save preset", "Preset name (Documents / Ancient Drums / Presets):", MessageBoxIconType::NoIcon, this);
    saveDialog->setLookAndFeel (&lnf);
    saveDialog->addTextEditor ("name", proc.getPresetName().startsWith ("Random") ? String ("My Kick") : proc.getPresetName());
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
            else AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Save preset", "Could not save the preset.");
        }
    }), false);
}

void Canvas::timerCallback()
{
    const int sel = proc.getSelectedLayer();
    if (sel != shownLayer)
    {
        for (int l = 0; l < NumSampleLayers; ++l) layerControls[(size_t) l]->setVisible (l == sel);
        shownLayer = sel;
    }
    layerControls[(size_t) sel]->refresh();
    screen.refresh();

    auto dim = [] (std::vector<Component*>& cells, bool on)
    {
        const float a = on ? 1.f : 0.4f;
        for (auto* c : cells) if (std::abs (c->getAlpha() - a) > 0.001f) c->setAlpha (a);
    };
    dim (forgeCells, forgePower->isOn());
    dim (subCells, subPower->isOn());
    dim (rumCells, rumPower->isOn());

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
}

void Canvas::resized()
{
    panels.clear();
    const Rectangle<int> left (20, 124, 316, 440), scr (350, 124, 660, 440), right (1024, 124, 316, 440);
    panels.push_back ({ "", left });
    panels.push_back ({ "FORGE", right });
    for (auto& lc : layerControls) lc->setBounds (left);
    screen.setBounds (scr);

    // FORGE: 2 x 3 knobs
    forgePower->setBounds (right.getRight() - 16 - 60, right.getY() + 12, 60, 24);
    {
        auto a = right.reduced (16, 12);
        a.removeFromTop (60);
        a.removeFromBottom (10);
        const int cw = a.getWidth() / 2, rh = a.getHeight() / 3;
        for (int i = 0; i < 6; ++i)
            forgeCells[(size_t) i]->setBounds (a.getX() + (i % 2) * cw, a.getY() + (i / 2) * rh, cw, rh);
    }

    // bottom row
    const int by = 578, bh = 182;
    const Rectangle<int> subA (20, by, 436, bh), rumA (470, by, 420, bh), masA (904, by, 436, bh);
    panels.push_back ({ "SUB BASS", subA });
    panels.push_back ({ "RUMBLE", rumA });
    panels.push_back ({ "MASTER", masA });
    subPower->setBounds (subA.getRight() - 16 - 60, subA.getY() + 10, 60, 24);
    rumPower->setBounds (rumA.getRight() - 16 - 60, rumA.getY() + 10, 60, 24);

    auto placeRow = [] (Rectangle<int> area, std::vector<Component*>& cells)
    {
        area = area.reduced (14, 10);
        area.removeFromTop (32);
        const float cw = (float) area.getWidth() / (float) cells.size();
        for (size_t i = 0; i < cells.size(); ++i)
        {
            auto cell = Rectangle<int> (area.getX() + (int) (cw * (float) i), area.getY(), (int) cw, area.getHeight());
            if (dynamic_cast<Switch*> (cells[i]) != nullptr)         cells[i]->setBounds (cell.withSizeKeepingCentre (cell.getWidth() - 8, 28));
            else if (dynamic_cast<Segmented*> (cells[i]) != nullptr) cells[i]->setBounds (cell.withSizeKeepingCentre (cell.getWidth() - 10, 84));
            else                                                     cells[i]->setBounds (cell);
        }
    };
    placeRow (subA, subCells);
    placeRow (rumA, rumCells);
    placeRow (masA, masterCells);

    // header
    const int px = W - 20 - 290;
    prevButton.setBounds (px, 30, 32, 32);
    presetBox.setBounds (px + 38, 30, 214, 32);
    nextButton.setBounds (px + 258, 30, 32, 32);
    randomButton.setBounds (px, 70, 128, 28);
    saveButton.setBounds (px + 134, 70, 74, 28);
    playButton.setBounds (px + 214, 70, 76, 28);

    renderBackground();
}

void Canvas::drawPanel (Graphics& g, const Panel& pnl)
{
    auto r = pnl.area.toFloat();
    Path shape; shape.addRoundedRectangle (r, 12.f);
    DropShadow (Colour (0x30503c1a), 24, { 0, 8 }).drawForPath (g, shape);
    g.setColour (Colours::white.withAlpha (0.66f));
    g.fillPath (shape);
    ColourGradient sheen (Colours::white.withAlpha (0.55f), 0.f, r.getY(), Colours::white.withAlpha (0.f), 0.f, r.getY() + 120.f, false);
    g.setGradientFill (sheen);
    g.fillPath (shape);
    ColourGradient border (Palette::gold.withAlpha (0.85f), 0.f, r.getY(), Palette::gold.withAlpha (0.25f), 0.f, r.getBottom(), false);
    g.setGradientFill (border);
    g.strokePath (shape, PathStrokeType (1.f));
    ColourGradient hl (Colours::white.withAlpha (0.f), r.getX() + 12.f, 0.f, Colours::white.withAlpha (0.f), r.getRight() - 12.f, 0.f, false);
    hl.addColour (0.5, Colours::white);
    g.setGradientFill (hl);
    g.fillRect (r.getX() + 12.f, r.getY() + 1.f, r.getWidth() - 24.f, 1.f);

    if (pnl.title.isNotEmpty())
    {
        g.setColour (Palette::gold);
        g.fillRoundedRectangle (r.getX() + 16.f, r.getY() + 15.f, 3.f, 12.f, 1.5f);
        g.setFont (lnf.headFont (12.f, 0.3f));
        g.setColour (Palette::ink);
        g.drawText (pnl.title, (int) r.getX() + 26, (int) r.getY() + 12, 240, 18, Justification::centredLeft);
    }
}

void Canvas::renderBackground()
{
    const float scale = 2.f;
    background = Image (Image::RGB, (int) (W * scale), (int) (H * scale), true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (scale));

    ColourGradient base (Colour (0xfffaf7f0), 0.f, 0.f, Palette::ivoryDeep, 0.f, (float) H, false);
    g.setGradientFill (base);
    g.fillAll();

    // marble veins
    Random rnd (42);
    for (int i = 0; i < 70; ++i)
    {
        Path v;
        float x = rnd.nextFloat() * W, y = rnd.nextFloat() * H;
        v.startNewSubPath (x, y);
        for (int k = 0; k < 5; ++k)
        {
            const float nx = x + (rnd.nextFloat() - 0.3f) * 220.f, ny = y + (rnd.nextFloat() - 0.5f) * 120.f;
            v.quadraticTo ((x + nx) * 0.5f + (rnd.nextFloat() - 0.5f) * 60.f, (y + ny) * 0.5f + (rnd.nextFloat() - 0.5f) * 60.f, nx, ny);
            x = nx; y = ny;
        }
        const bool goldVein = i % 9 == 0;
        g.setColour (goldVein ? Palette::gold.withAlpha (0.10f) : Colour (0xff8c8172).withAlpha (0.045f + 0.04f * rnd.nextFloat()));
        g.strokePath (v, PathStrokeType (goldVein ? 1.2f : 0.5f + rnd.nextFloat() * 1.2f));
    }

    // warm light from above + soft golden beams
    ColourGradient light (Colours::white.withAlpha (0.9f), W * 0.5f, -40.f, Colours::transparentWhite, W * 0.5f, 520.f, true);
    g.setGradientFill (light);
    g.fillRect (0, 0, W, H);
    for (int i = -5; i <= 5; ++i)
    {
        const Point<float> src (W * 0.5f, -80.f);
        const float a = (float) i * 0.17f;
        const auto end = src + Point<float> (std::sin (a), std::cos (a)) * 1100.f;
        const auto perp = Point<float> (std::cos (a), -std::sin (a)) * 46.f;
        Path beam; beam.startNewSubPath (src); beam.lineTo (end + perp); beam.lineTo (end - perp); beam.closeSubPath();
        ColourGradient bg (Palette::goldLight.withAlpha (0.10f), src.x, src.y, Palette::goldLight.withAlpha (0.f), end.x, end.y, false);
        g.setGradientFill (bg);
        g.fillPath (beam);
    }

    // header ornament: horizon line + meander band
    {
        ColourGradient line (Palette::gold.withAlpha (0.f), 0.f, 0.f, Palette::gold.withAlpha (0.f), (float) W, 0.f, false);
        line.addColour (0.5, Palette::gold);
        g.setGradientFill (line);
        g.fillRect (0.f, 110.f, (float) W, 1.2f);
        Path meander;
        const float y0 = 113.f, s = 5.f;
        for (float x = 360.f; x < W - 360.f; x += s * 4.f)
        {
            meander.startNewSubPath (x, y0 + s); meander.lineTo (x, y0); meander.lineTo (x + s * 2.f, y0);
            meander.lineTo (x + s * 2.f, y0 + s * 0.6f); meander.lineTo (x + s, y0 + s * 0.6f);
            meander.startNewSubPath (x, y0 + s); meander.lineTo (x + s * 4.f, y0 + s);
        }
        g.setColour (Palette::gold.withAlpha (0.45f));
        g.strokePath (meander, PathStrokeType (0.8f));
        Path diamond;
        diamond.addQuadrilateral (W * 0.5f, 104.f, W * 0.5f + 6.f, 110.6f, W * 0.5f, 117.f, W * 0.5f - 6.f, 110.6f);
        g.setColour (Palette::gold); g.fillPath (diamond);
    }

    // header side text
    g.setFont (lnf.headFont (10.f, 0.25f));
    g.setColour (Palette::ink.withAlpha (0.8f));
    g.drawText ("V2.0", 20, 28, 300, 14, Justification::left);
    g.setFont (lnf.labelFont (12.f, 0.28f));
    g.setColour (Palette::label);
    g.drawText ("PUNCH  /  CRUNCH  /  KICK", 20, 46, 320, 16, Justification::left);
    g.setColour (Palette::dim);
    g.drawText ("FORGE  /  SUB  /  RUMBLE", 20, 62, 320, 16, Justification::left);
    g.setColour (Palette::goldDeep);
    g.drawText ("FORGED IN THE OLD WAYS", 20, 78, 320, 16, Justification::left);

    // title
    const auto titleArea = Rectangle<float> (0.f, 14.f, (float) W, 64.f);
    g.setFont (lnf.titleFont (54.f, 0.12f));
    g.setColour (Colour (0x2a6b4a12));
    g.drawText ("ANCIENT DRUMS", titleArea.translated (0.f, 2.5f), Justification::centred);
    ColourGradient tg (Palette::goldLight, 0.f, titleArea.getY() + 10.f, Palette::goldDeep, 0.f, titleArea.getBottom() - 8.f, false);
    tg.addColour (0.5, Palette::gold);
    g.setGradientFill (tg);
    for (float dx : { -0.6f, 0.f, 0.6f }) g.drawText ("ANCIENT DRUMS", titleArea.translated (dx, 0.f), Justification::centred);   // a touch bolder
    g.setFont (lnf.labelFont (12.5f, 0.62f));
    g.setColour (Palette::goldDeep);
    g.drawText ("HARDSTYLE  KICK  GENERATOR", 0, 80, W, 18, Justification::centred);

    g.setFont (lnf.labelFont (11.f, 0.35f));
    g.setColour (Palette::dim);
    g.drawText ("PRESET", W - 20 - 290 + 38, 14, 214, 14, Justification::centred);

    for (auto& p : panels) drawPanel (g, p);
    {
        auto r = panels[1].area.toFloat();
        g.setFont (lnf.labelFont (11.f, 0.2f));
        g.setColour (Palette::dim);
        g.drawText ("HARDSTYLE TAIL CHAIN", (int) r.getX() + 26, (int) r.getY() + 32, 260, 16, Justification::centredLeft);
    }
}

void Canvas::paint (Graphics& g)
{
    if (background.isValid())
        g.drawImage (background, getLocalBounds().toFloat(), RectanglePlacement::stretchToFit);
}

// ======================================================================= Editor
AncientEditor::AncientEditor (AncientProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (816, 468, 2040, 1170);
    getConstrainer()->setFixedAspectRatio ((double) Canvas::W / (double) Canvas::H);
    setSize (1156, 663);
}

void AncientEditor::resized()
{
    canvas.setBounds (0, 0, Canvas::W, Canvas::H);
    canvas.setTransform (AffineTransform::scale ((float) getWidth() / (float) Canvas::W));
}
