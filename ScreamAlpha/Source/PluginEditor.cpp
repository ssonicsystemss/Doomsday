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

// ======================================================================= canvas
Component* AlphaCanvas::knob (const char* id, const char* label, Colour c, bool bipolar)
{
    auto k = std::make_unique<Knob> (proc.apvts, id, label, c, bipolar);
    auto* raw = k.get(); addAndMakeVisible (*raw); owned.push_back (std::move (k));
    return raw;
}

AlphaCanvas::AlphaCanvas (AlphaProcessor& p) : proc (p), scope (p)
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
    addAndMakeVisible (scope);

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.onChange = [this] { if (presetBox.getSelectedId() > 0) proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    prev.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    undoB.onClick = [this] { proc.undo.undo(); };
    redoB.onClick = [this] { proc.undo.redo(); };
    for (auto* c : { (Component*) &presetBox, (Component*) &prev, (Component*) &next, (Component*) &undoB, (Component*) &redoB }) addAndMakeVisible (c);
    setSize (W, H);
    timerCallback();
    startTimerHz (30);
}

AlphaCanvas::~AlphaCanvas() { setLookAndFeel (nullptr); }

void AlphaCanvas::step (int d)
{
    const int n = proc.getNumPrograms();
    proc.setCurrentProgram ((proc.getCurrentProgram() + d + n) % n);
}

void AlphaCanvas::timerCallback()
{
    if (proc.presetName() != lastName)
    {
        lastName = proc.presetName();
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, dontSendNotification);
    }
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
    scope.setBounds (440, 12, 260, 34);
    cards[2].area.setRight (W - m);
    x = m;
    for (int i = 3; i < 6; ++i) { auto& c = cards[(size_t) i]; c.area = { x, 234, widthOf (c), 2 * kh + 60 }; x += c.area.getWidth() + gap; }
    cards[5].area.setRight (W - m);
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
    // the Core card has its octave switch under the knobs instead of the title row (narrow card)
    {
        auto& c = cards[1];
        c.header->setBounds (c.area.getX() + 16 + 2 * kw + 6, c.area.getY() + 70, 92, 26);
    }
    const int bx = W - m;
    redoB.setBounds (bx - 70, 14, 70, 30);
    undoB.setBounds (bx - 146, 14, 70, 30);
    next.setBounds (bx - 190, 14, 34, 30);
    presetBox.setBounds (bx - 196 - 250, 14, 250, 30);
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
    g.drawText ("v0.1" + dot + "test build", Rectangle<float> (tx + 170.f, 0.f, 200.f, 56.f), Justification::centredLeft);
    for (auto& c : cards) card (g, lnf, c.area, c.title, c.dot);
}

// ======================================================================= editor
AlphaEditor::AlphaEditor (AlphaProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (744, 286, 2232, 857);
    getConstrainer()->setFixedAspectRatio ((double) AlphaCanvas::W / (double) AlphaCanvas::H);
    setSize (AlphaCanvas::W, AlphaCanvas::H);
}

void AlphaEditor::resized()
{
    canvas.setBounds (0, 0, AlphaCanvas::W, AlphaCanvas::H);
    canvas.setTransform (AffineTransform::scale ((float) getWidth() / (float) AlphaCanvas::W));
}
