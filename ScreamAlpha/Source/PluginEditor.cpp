#include "PluginEditor.h"
#include "../../Shared/Source/DoomsdayBrand.h"

using namespace juce;
using namespace doomsday::flat;

namespace { const String dot (CharPointer_UTF8 (" \xc2\xb7 ")); }

// ======================================================================= scope
void AlphaScope::paint (Graphics& g)
{
    const auto& t = tokens();
    auto r = getLocalBounds().toFloat();
    g.setColour (t.ground.withAlpha (0.7f)); g.fillRoundedRectangle (r, 8.f);
    constexpr int N = AlphaProcessor::scopeSize, show = 400;
    const int w = proc.scopeWrite.load (std::memory_order_acquire);
    std::array<float, N> s;
    for (int i = 0; i < N; ++i) s[(size_t) i] = proc.scope[(size_t) ((w + i) & (N - 1))];
    int start = N - show - 1;
    for (int i = N - show - 1; i > N - show - 1500 && i > 1; --i) if (s[(size_t) i - 1] < 0.f && s[(size_t) i] >= 0.f) { start = i; break; }
    Path p;
    for (int i = 0; i < show; ++i)
    {
        const float x = r.getX() + 6.f + (r.getWidth() - 12.f) * (float) i / (float) (show - 1);
        const float y = r.getCentreY() - jlimit (-1.f, 1.f, s[(size_t) (start + i)] * 1.6f) * r.getHeight() * 0.42f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (t.hot); g.strokePath (p, PathStrokeType (1.5f));
}

// ======================================================================= automation preview
void AutoPreview::tick()
{
    const int shape = roundToInt (proc.apvts.getRawParameterValue ("autoShape")->load());
    const float depth = proc.apvts.getRawParameterValue ("autoOct")->load();
    const double ph = proc.autoPhaseNow();
    if (shape != lastShape || std::abs (depth - lastDepth) > 0.01f || std::abs (ph - lastPh) > 0.002)
    { lastShape = shape; lastDepth = depth; lastPh = ph; repaint(); }
}

void AutoPreview::paint (Graphics& g)
{
    const auto& t = tokens();
    auto r = getLocalBounds().toFloat();
    g.setColour (t.ground.withAlpha (0.7f)); g.fillRoundedRectangle (r, 8.f);
    auto plot = r.reduced (10.f, 10.f);
    g.setColour (t.divider);
    for (int i = 1; i < 4; ++i) g.fillRect (plot.getX() + plot.getWidth() * (float) i / 4.f, plot.getY(), 1.f, plot.getHeight());
    const int shape = roundToInt (proc.apvts.getRawParameterValue ("autoShape")->load());
    const float depth = proc.apvts.getRawParameterValue ("autoOct")->load();
    auto& lnf = lnfOf (*this);
    if (shape == 0 || std::abs (depth) < 0.01f)
    {
        g.setFont (lnf.font (12.f, Medium)); g.setColour (t.textMute);
        g.drawText ("Pick a shape and set Depth: the cutoff moves by itself, locked to the FL tempo", r.reduced (12.f, 0.f), Justification::centred, true);
        return;
    }
    auto val = [shape] (float ph)
    {
        switch (shape)
        {
            case 1: return ph;
            case 2: return 1.f - ph;
            case 3: return 1.f - std::abs (2.f * ph - 1.f);
            case 4: return 0.5f + 0.5f * std::sin (ph * MathConstants<float>::twoPi - 1.5707963f);
            default: { const int st = (int) (ph * 16.f); uint32_t h = (uint32_t) (st + 7) * 2654435761u; h ^= h >> 13; return (float) (h & 0xffff) / 65535.f; }
        }
    };
    auto yOf = [&] (float v) { const float u = depth >= 0.f ? v : 1.f - v; return plot.getBottom() - u * plot.getHeight(); };
    Path p;
    for (int i = 0; i <= 200; ++i)
    {
        const float ph = (float) i / 200.f;
        const float x = plot.getX() + ph * plot.getWidth(), y = yOf (val (ph));
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    Path fill (p); fill.lineTo (plot.getRight(), plot.getBottom()); fill.lineTo (plot.getX(), plot.getBottom()); fill.closeSubPath();
    g.setColour (t.cool.withAlpha (0.12f)); g.fillPath (fill);
    g.setColour (t.cool); g.strokePath (p, PathStrokeType (2.f));
    const float ph = (float) proc.autoPhaseNow();
    const float px = plot.getX() + ph * plot.getWidth();
    g.setColour (Colours::white.withAlpha (0.5f)); g.fillRect (px, plot.getY(), 1.5f, plot.getHeight());
    g.setColour (Colours::white); g.fillEllipse (Rectangle<float> (8.f, 8.f).withCentre ({ px, yOf (val (ph)) }));
    g.setFont (lnf.font (11.f, SemiBold)); g.setColour (t.textMute);
    g.drawText ("cutoff " + String (depth > 0 ? "+" : "") + String (depth, 1) + " oct", plot.reduced (4.f, 0.f), Justification::topLeft);
}

// ======================================================================= canvas
Component* AlphaCanvas::knob (const char* id, const char* label, Colour c, bool bipolar)
{
    auto k = std::make_unique<Knob> (proc.apvts, id, label, c, bipolar);
    auto* raw = k.get(); addAndMakeVisible (*raw); owned.push_back (std::move (k));
    return raw;
}

AlphaCanvas::AlphaCanvas (AlphaProcessor& p) : proc (p), scope (p), autoView (p)
{
    setLookAndFeel (&lnf);
    const auto& t = tokens();
    const Colour hot = t.hot, cool = t.cool, n { 0xffb4bac3 };
    auto seg = [this] (const char* id, StringArray labels, Colour c)
    {
        auto s = std::make_unique<Segmented> (proc.apvts, id, labels, c);
        auto* raw = s.get(); addAndMakeVisible (*raw); owned.push_back (std::move (s)); return (Component*) raw;
    };

    cards.push_back ({ "Note throw", hot, 5, { knob ("scoopSt", "Scoop", hot, true), knob ("scoopMs", "Time", hot), knob ("scoopCurve", "Curve", hot),
                                               knob ("glideMs", "Glide", hot), knob ("tuneSt", "Tune", hot, true) } });
    cards.push_back ({ "Core", n, 2, { knob ("coreLevel", "Level", n), knob ("coreShape", "Shape", n) },
                       seg ("coreOct", { "-1", "0", "+1" }, n) });
    cards.push_back ({ "Amp", n, 4, { knob ("attackMs", "Attack", n), knob ("decayMs", "Decay", n), knob ("sustain", "Sustain", n), knob ("releaseMs", "Release", n) } });
    cards.push_back ({ "Hash", cool, 6, { knob ("hashLevel", "Level", cool), knob ("hashDecayMs", "Decay", cool), knob ("hashSustain", "Sustain", cool),
                                          knob ("voices", "Voices", cool), knob ("detuneCents", "Detune", cool), knob ("carrierSaw", "Saw", cool),
                                          knob ("ratio", "FM ratio", cool), knob ("index", "FM", cool), knob ("chaos", "Chaos", cool),
                                          knob ("chaosHz", "Chaos speed", cool), knob ("grit", "Grit", cool), knob ("hashHp", "Low cut", cool) } });
    cards.push_back ({ "Scream", hot, 4, { knob ("drive1Db", "Drive", hot), knob ("combMix", "Comb", hot), knob ("combFb", "Comb fb", hot, true),
                                           knob ("combRatio", "Comb tune", hot), knob ("formHz", "Formant", hot), knob ("formGainDb", "Gain", hot, true),
                                           knob ("formQ", "Q", hot) },
                       seg ("drive1Curve", { "Soft", "Hard", "Tube", "Fold", "Cub" }, hot) });
    cards.push_back ({ "Output", n, 3, { knob ("drive2Db", "Drive 2", n), knob ("toneHz", "Tone", n), knob ("lowCut", "Low cut", n),
                                         knob ("chorus", "Chorus", n), knob ("outDb", "Volume", n, true) },
                       seg ("drive2Curve", { "Soft", "Hard", "Tube", "Fold", "Cub" }, n) });
    cards.push_back ({ "Space", cool, 6, { knob ("ott", "OTT", cool), knob ("dlyMix", "Delay", cool), knob ("dlyFb", "Feedback", cool),
                                           knob ("revMix", "Reverb", cool), knob ("revSize", "Size", cool), knob ("revDamp", "Damp", cool) },
                       seg ("dlyDiv", { "1/16", "1/8", "1/8D", "1/4", "1/4D", "1/2" }, cool) });
    // FORTUNE: the big macro knob (from Fortune of Seekers)
    {
        auto k = std::make_unique<Knob> (proc.apvts, "fortune", "FORTUNE", hot, false, 112);
        k->setCrown (true);
        fortuneKnob = k.get(); addAndMakeVisible (*k); owned.push_back (std::move (k));
        cards.push_back ({ "Fortune", hot, 1, {} });
    }
    // FILTER + built-in cutoff automation
    cards.push_back ({ "Filter + cutoff automation", cool, 3, { knob ("cutoffHz", "Cutoff", cool), knob ("reso", "Reso", cool), knob ("autoOct", "Depth", cool, true) },
                       seg ("filtMode", { "LP 24", "LP 12", "Band", "High" }, cool) });
    shapeSeg = seg ("autoShape", { "Off", "Rise", "Fall", "Up/Dn", "Wobble", "Steps" }, cool);
    lenSeg = seg ("autoLen", { "1 beat", "2 beats", "1 bar", "2 bars", "4 bars", "8 bars" }, cool);
    addAndMakeVisible (autoView);
    addAndMakeVisible (scope);

    refreshList();
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        if (id < userBase) proc.setCurrentProgram (id - 1);
        else if (isPositiveAndBelow (id - userBase, userFiles.size())) proc.loadUserPreset (userFiles[id - userBase]);
        lastName = {};
    };
    randomB.onClick = [this] { proc.randomize(); lastName = {}; };
    saveB.onClick = [this] { showSave(); };
    randomB.setTooltip ("A new sound in sensible ranges (level matched)");
    saveB.setTooltip ("Save the current sound to Documents / 5CR34M 4LPH4 / Presets");
    FlatLookAndFeel::makePrimary (saveB);
    prev.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    undoB.onClick = [this] { proc.undo.undo(); };
    redoB.onClick = [this] { proc.undo.redo(); };
    for (auto* c : { (Component*) &presetBox, (Component*) &prev, (Component*) &next, (Component*) &undoB, (Component*) &redoB,
                     (Component*) &randomB, (Component*) &saveB }) addAndMakeVisible (c);
    setSize (W, H);
    timerCallback();
    startTimerHz (30);
}

AlphaCanvas::~AlphaCanvas()
{
    if (dialog != nullptr) dialog->exitModalState (0);
    dialog.reset();
    setLookAndFeel (nullptr);
}

void AlphaCanvas::refreshList()
{
    presetBox.clear (dontSendNotification);
    presetBox.addSectionHeading ("FACTORY");
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    userFiles = proc.userPresets();
    if (! userFiles.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("USER");
        for (int i = 0; i < userFiles.size(); ++i) presetBox.addItem (userFiles[i].getFileNameWithoutExtension(), userBase + i);
    }
    lastName = {};
}

void AlphaCanvas::showSave()
{
    if (dialog != nullptr) return;
    dialog = std::make_unique<AlertWindow> ("Save preset", "Name (Documents / 5CR34M 4LPH4 / Presets):", MessageBoxIconType::NoIcon, this);
    dialog->setLookAndFeel (&lnf);
    dialog->addTextEditor ("name", proc.presetName().startsWith ("Random") ? String ("My Screech") : proc.presetName());
    dialog->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    SafePointer<AlphaCanvas> safe (this);
    dialog->enterModalState (true, ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr || safe->dialog == nullptr) return;
        const auto name = safe->dialog->getTextEditorContents ("name");
        MessageManager::callAsync ([safe] { if (safe != nullptr) safe->dialog.reset(); });
        if (result == 1)
        {
            if (safe->proc.saveUserPreset (name)) safe->refreshList();
            else AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Save preset", "Could not save the preset.");
        }
    }), false);
}

void AlphaCanvas::step (int d)
{
    Array<int> ids;
    for (int i = 0; i < presetBox.getNumItems(); ++i) ids.add (presetBox.getItemId (i));
    if (ids.isEmpty()) return;
    int at = ids.indexOf (presetBox.getSelectedId());
    at = at < 0 ? 0 : (at + d + ids.size()) % ids.size();
    presetBox.setSelectedId (ids[at], sendNotificationSync);
}

void AlphaCanvas::timerCallback()
{
    if (proc.presetName() != lastName)
    {
        lastName = proc.presetName();
        int found = 0;
        for (int i = 0; i < presetBox.getNumItems(); ++i) if (presetBox.getItemText (i) == lastName) { found = presetBox.getItemId (i); break; }
        if (proc.programIndex() >= 0) found = proc.programIndex() + 1;
        if (found != 0) presetBox.setSelectedId (found, dontSendNotification);
        else presetBox.setText (lastName, dontSendNotification);
    }
    autoView.tick();
    const int w = proc.scopeWrite.load();
    if (w != lastScope) { lastScope = w; scope.repaint(); }
}

void AlphaCanvas::resized()
{
    const int m = 20, gap = 12, kw = 84, kh = Knob::heightFor (44);
    auto widthOf = [&] (const Card& c) { return c.columns * kw + 32; };
    // row 1: Note throw / Core / Amp      row 2: Hash / Scream / Output
    int x = m;
    for (int i = 0; i < 3; ++i) { auto& c = cards[(size_t) i]; c.area = { x, 72, widthOf (c) + (c.header ? 100 : 0), 150 }; x += c.area.getWidth() + gap; }
    scope.setBounds (390, 12, 180, 34);
    cards[2].area.setRight (W - m);
    x = m;
    for (int i = 3; i < 6; ++i) { auto& c = cards[(size_t) i]; c.area = { x, 234, widthOf (c), 2 * kh + 60 }; x += c.area.getWidth() + gap; }
    cards[5].area.setRight (W - m);
    cards[6].area = { m, 466, 6 * kw + 32 + 240, 150 };
    cards[7].area = { cards[6].area.getRight() + gap, 466, W - m - cards[6].area.getRight() - gap, 312 };
    cards[8].area = { m, 628, cards[6].area.getWidth(), 150 };
    for (auto& c : cards)
    {
        auto a = c.area.reduced (16, 10);
        auto head = a.removeFromTop (30);
        if (c.header) { const int hw = jmin (220, head.getWidth() - 90); c.header->setBounds (head.removeFromRight (hw).withSizeKeepingCentre (hw, 24)); }
        const int rows = ((int) c.knobs.size() + c.columns - 1) / c.columns;
        for (size_t k = 0; k < c.knobs.size(); ++k)
        {
            const int row = (int) k / c.columns, col = (int) k % c.columns;
            c.knobs[k]->setBounds (a.getX() + col * kw, a.getY() + 4 + row * (kh + 6), kw, kh);
        }
        ignoreUnused (rows);
    }
    // FORTUNE big knob, FILTER extras
    fortuneKnob->setBounds (cards[7].area.withSizeKeepingCentre (220, Knob::heightFor (112)).translated (0, 6));
    {
        auto a = cards[8].area.reduced (16, 10);
        a.removeFromTop (34);
        a.removeFromLeft (3 * kw + 16);
        auto segs = a.removeFromLeft (300);
        shapeSeg->setBounds (segs.removeFromTop (28).reduced (0, 1));
        segs.removeFromTop (12);
        lenSeg->setBounds (segs.removeFromTop (28).reduced (0, 1));
        a.removeFromLeft (16);
        autoView.setBounds (a.withTrimmedBottom (4));
    }
    // the Core card has its octave switch under the knobs instead of the title row (narrow card)
    {
        auto& c = cards[1];
        c.header->setBounds (c.area.getX() + 16 + 2 * kw + 6, c.area.getY() + 70, 92, 26);
    }
    const int bx = W - m;
    redoB.setBounds (bx - 62, 14, 62, 30);
    undoB.setBounds (bx - 130, 14, 62, 30);
    saveB.setBounds (bx - 210, 14, 72, 30);
    randomB.setBounds (bx - 302, 14, 86, 30);
    next.setBounds (bx - 346, 14, 34, 30);
    presetBox.setBounds (bx - 352 - 240, 14, 240, 30);
    prev.setBounds (presetBox.getX() - 40, 14, 34, 30);
}

void AlphaCanvas::paint (Graphics& g)
{
    const auto& t = tokens();
    g.fillAll (t.ground);
    g.setColour (t.divider); g.fillRect (0, 56, W, 1);
    doomsday::brand::drawBadge (g, doomsday::brand::headerBadgeArea (1.f, 28.f), 0.6f, RectanglePlacement::xLeft | RectanglePlacement::yMid);
    const float tx = doomsday::brand::headerTextX (1.f);
    g.setFont (lnf.font (19.f, Bold, 0.06f)); g.setColour (t.text);
    g.drawText ("5CR34M 4LPH4", Rectangle<float> (tx, 0.f, 200.f, 56.f), Justification::centredLeft);
    g.setFont (lnf.font (12.f, Medium)); g.setColour (t.textMute);
    g.drawText ("v0.3" + dot + "test build", Rectangle<float> (tx + 170.f, 0.f, 200.f, 56.f), Justification::centredLeft);
    for (auto& c : cards) card (g, lnf, c.area, c.title, c.dot);
    {   // FORTUNE caption
        auto a = cards[7].area;
        g.setFont (lnf.font (11.5f, Medium)); g.setColour (t.textMute);
        g.drawFittedText ("One knob: more hash, drive, chaos, detune, formant, cutoff, throw and space (level kept steady)",
                          a.reduced (20, 0).withTop (a.getBottom() - 56).withHeight (40), Justification::centred, 2);
    }
}

// ======================================================================= editor
AlphaEditor::AlphaEditor (AlphaProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (744, 480, 2232, 1440);
    getConstrainer()->setFixedAspectRatio ((double) AlphaCanvas::W / (double) AlphaCanvas::H);
    setSize (AlphaCanvas::W, AlphaCanvas::H);
}

void AlphaEditor::resized()
{
    canvas.setBounds (0, 0, AlphaCanvas::W, AlphaCanvas::H);
    canvas.setTransform (AffineTransform::scale ((float) getWidth() / (float) AlphaCanvas::W));
}
