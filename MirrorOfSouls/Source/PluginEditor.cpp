#include "PluginEditor.h"
#include "DoomsdayBrand.h"

using namespace juce;
using namespace doomsday::ui;

Theme MirrorTheme::make()
{
    Theme t;
    t.bg = Colour (0xff020406);
    t.panel = Colour (0xff05080b);
    t.label = Colour (0xff9fb1bb);
    t.knobLight = Colour (0xff2c353b);
    t.knobDark = Colour (0xff04070a);
    t.accents = { { ice, iceDeep, iceHot } };
    t.crown = { iceDeep.brighter (0.4f), ice, iceHot, Colours::white };
    return t;
}

namespace
{
    const double fMin = 20.0, fMax = 20000.0;
    String noteName (double hz)
    {
        const int midi = (int) std::round (69.0 + 12.0 * std::log2 (hz / 440.0));
        return MidiMessage::getMidiNoteName (jlimit (0, 127, midi), true, true, 5);   // FL Studio: middle C = C5
    }
    String hzText (double hz) { return hz >= 1000.0 ? String (hz / 1000.0, hz >= 10000.0 ? 1 : 2) + " kHz" : String ((int) std::round (hz)) + " Hz"; }
}

// ======================================================================= EqDisplay
EqDisplay::EqDisplay (MirrorProcessor& p, std::function<void (int)> cb) : proc (p), onSelect (std::move (cb))
{
    scratch.resize ((size_t) doomsday::dsp::Spectrum::size);
    setMouseCursor (MouseCursor::CrosshairCursor);
}

void EqDisplay::resized() { plot = getLocalBounds().toFloat().reduced (46.f, 18.f).withTrimmedBottom (14.f); }

float EqDisplay::xOf (double hz) const { return plot.getX() + plot.getWidth() * (float) (std::log (hz / fMin) / std::log (fMax / fMin)); }
double EqDisplay::hzOf (float x) const { return fMin * std::pow (fMax / fMin, (double) jlimit (0.f, 1.f, (x - plot.getX()) / plot.getWidth())); }
float EqDisplay::eqY (float db) const { return plot.getCentreY() - db / 18.f * plot.getHeight() * 0.5f; }
float EqDisplay::dbOfY (float y) const { return jlimit (-24.f, 24.f, (plot.getCentreY() - y) / (plot.getHeight() * 0.5f) * 18.f); }
float EqDisplay::anY (float dbfs) const { return plot.getBottom() - jlimit (0.f, 1.f, (dbfs + 90.f) / 90.f) * plot.getHeight(); }

RangedAudioParameter* EqDisplay::prm (int b, const char* key) const { return proc.apvts.getParameter (ParamIDs::band (b, key)); }
float EqDisplay::val (int b, const char* key) const { return proc.apvts.getRawParameterValue (ParamIDs::band (b, key))->load(); }
void EqDisplay::setVal (int b, const char* key, float v) { if (auto* r = prm (b, key)) r->setValueNotifyingHost (r->convertTo0to1 (v)); }

Point<float> EqDisplay::nodePos (int b) const
{
    const int t = (int) std::round (val (b, ParamIDs::type));
    const float g = MirrorEngine::gainType (t) ? val (b, ParamIDs::gain) : 0.f;
    return { xOf (val (b, ParamIDs::freq)), eqY (g) };
}

int EqDisplay::bandAt (Point<float> pt) const
{
    int best = -1; float bestD = 14.f;
    for (int b = 0; b < ParamIDs::numBands; ++b)
    {
        const float d = nodePos (b).getDistanceFrom (pt);
        if (d < bestD) { bestD = d; best = b; }
    }
    return best;
}

void EqDisplay::tick()
{
    const double sr = proc.getEngine().sampleRate();
    pre.setSampleRate (sr); post.setSampleRate (sr);
    const int mode = (int) std::round (proc.apvts.getRawParameterValue (ParamIDs::analyzer)->load());
    if (mode >= 1) { proc.getEngine().postTap.latest (scratch.data(), doomsday::dsp::Spectrum::size); post.feed (scratch.data()); }
    if (mode >= 2) { proc.getEngine().preTap.latest (scratch.data(), doomsday::dsp::Spectrum::size);  pre.feed (scratch.data()); }
    repaint();
}

void EqDisplay::paint (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    const auto ice = MirrorTheme::ice;
    auto r = getLocalBounds().toFloat();
    // screen glass
    ColourGradient bg (Colour (0xff061019), r.getCentreX(), r.getY(), Colour (0xff010305), r.getCentreX(), r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, 10.f);
    g.setColour (ice.withAlpha (0.25f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.f, 1.f);

    // grid: frequencies + dB
    g.setFont (lnf.labelFont (10.5f, 0.1f));
    for (double f : { 20.0, 30.0, 40.0, 50.0, 60.0, 80.0, 100.0, 200.0, 300.0, 400.0, 500.0, 600.0, 800.0, 1000.0, 2000.0, 3000.0, 4000.0, 5000.0, 6000.0, 8000.0, 10000.0, 20000.0 })
    {
        const bool major = f == 20.0 || f == 50.0 || f == 100.0 || f == 200.0 || f == 500.0 || f == 1000.0 || f == 2000.0 || f == 5000.0 || f == 10000.0 || f == 20000.0;
        const float x = xOf (f);
        g.setColour (Colours::white.withAlpha (major ? 0.07f : 0.03f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        if (major)
        {
            g.setColour (lnf.theme.label.withAlpha (0.7f));
            g.drawText (f >= 1000.0 ? String ((int) (f / 1000.0)) + "k" : String ((int) f), Rectangle<float> (x - 20.f, plot.getBottom() + 2.f, 40.f, 14.f), Justification::centred);
        }
    }
    for (int db = -18; db <= 18; db += 6)
    {
        const float y = eqY ((float) db);
        g.setColour (Colours::white.withAlpha (db == 0 ? 0.12f : 0.04f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (ice.withAlpha (0.55f));
        g.drawText ((db > 0 ? "+" : "") + String (db), Rectangle<float> (plot.getRight() + 4.f, y - 7.f, 36.f, 14.f), Justification::centredLeft);
    }
    for (int db = -80; db <= 0; db += 20)
    {
        g.setColour (lnf.theme.dim);
        g.drawText (String (db), Rectangle<float> (plot.getX() - 42.f, anY ((float) db) - 7.f, 36.f, 14.f), Justification::centredRight);
    }

    const int mode = (int) std::round (proc.apvts.getRawParameterValue (ParamIDs::analyzer)->load());
    const int N = 360;
    auto spectrumPath = [&] (doomsday::dsp::Spectrum& s, bool closed)
    {
        Path pth;
        for (int i = 0; i <= N; ++i)
        {
            const double f = fMin * std::pow (fMax / fMin, (double) i / N);
            // pink-ish tilt (+4.5 dB/oct) so a balanced mix looks level
            const float db = s.dbAt (f) + 4.5f * (float) std::log2 (f / 1000.0);
            const float x = xOf (f), y = anY (db);
            if (i == 0) pth.startNewSubPath (x, y); else pth.lineTo (x, y);
        }
        if (closed) { pth.lineTo (plot.getRight(), plot.getBottom()); pth.lineTo (plot.getX(), plot.getBottom()); pth.closeSubPath(); }
        return pth;
    };
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (plot.toNearestInt());
        if (mode >= 2)
        {
            g.setColour (Colours::white.withAlpha (0.22f));
            g.strokePath (spectrumPath (pre, false), PathStrokeType (1.f));
        }
        if (mode >= 1)
        {
            auto fill = spectrumPath (post, true);
            ColourGradient fg (ice.withAlpha (0.38f), 0.f, plot.getY(), ice.withAlpha (0.02f), 0.f, plot.getBottom(), false);
            g.setGradientFill (fg);
            g.fillPath (fill);
            g.setColour (ice.withAlpha (0.55f));
            g.strokePath (spectrumPath (post, false), PathStrokeType (1.f));
        }

        // EQ curve: composite + the selected band's own curve
        const EqParams ep = proc.gather();
        const double fs = proc.getEngine().sampleRate() > 0 ? proc.getEngine().sampleRate() : 48000.0;
        std::array<BandDesign, ParamIDs::numBands> d;
        for (int b = 0; b < ParamIDs::numBands; ++b) d[(size_t) b] = MirrorEngine::design (ep.band[(size_t) b], fs, ep.band[(size_t) b].gainDb, ep.amount);
        Path curve, sel;
        for (int i = 0; i <= N; ++i)
        {
            const double f = fMin * std::pow (fMax / fMin, (double) i / N);
            double m = 1.0;
            for (int b = 0; b < ParamIDs::numBands; ++b) if (ep.band[(size_t) b].on) m *= d[(size_t) b].magnitude (f, fs);
            const float y = eqY ((float) Decibels::gainToDecibels (m, -60.0));
            const float ys = ep.band[(size_t) selected].on ? eqY ((float) Decibels::gainToDecibels (d[(size_t) selected].magnitude (f, fs), -60.0)) : eqY (0.f);
            if (i == 0) { curve.startNewSubPath (xOf (f), y); sel.startNewSubPath (xOf (f), eqY (0.f)); sel.lineTo (xOf (f), ys); }
            else        { curve.lineTo (xOf (f), y); sel.lineTo (xOf (f), ys); }
        }
        sel.lineTo (plot.getRight(), eqY (0.f)); sel.closeSubPath();
        g.setColour (ice.withAlpha (0.10f)); g.fillPath (sel);
        g.setColour (ice.withAlpha (0.18f)); g.strokePath (curve, PathStrokeType (7.f));
        g.setColour (MirrorTheme::iceHot);   g.strokePath (curve, PathStrokeType (2.f));
    }

    // band points
    for (int b = 0; b < ParamIDs::numBands; ++b)
    {
        const bool on = val (b, ParamIDs::on) > 0.5f;
        const bool isSel = b == selected, hot = b == hoverBand || b == dragBand;
        const auto c = nodePos (b);
        const float rad = isSel ? 11.f : 9.f;
        if (on && val (b, ParamIDs::dyn) > 0.5f && MirrorEngine::gainType ((int) std::round (val (b, ParamIDs::type))))
        {
            // dynamic band: where the gain is right now
            const auto live = Point<float> (c.x, eqY (proc.getEngine().liveGainDb[(size_t) b].load()));
            g.setColour (ice.withAlpha (0.5f));
            g.drawLine ({ c, live }, 1.f);
            g.drawEllipse (Rectangle<float> (12.f, 12.f).withCentre (live), 1.5f);
        }
        if (on || isSel)
        {
            ColourGradient halo (ice.withAlpha (hot || isSel ? 0.45f : 0.25f), c.x, c.y, ice.withAlpha (0.f), c.x + rad * 2.4f, c.y, true);
            g.setGradientFill (halo);
            g.fillEllipse (Rectangle<float> (rad * 4.8f, rad * 4.8f).withCentre (c));
        }
        g.setColour (on ? Colour (0xff04121b) : Colour (0xff070a0d));
        g.fillEllipse (Rectangle<float> (rad * 2.f, rad * 2.f).withCentre (c));
        g.setColour (on ? (isSel ? Colours::white : ice) : lnf.theme.dim);
        g.drawEllipse (Rectangle<float> (rad * 2.f, rad * 2.f).withCentre (c), isSel ? 2.f : 1.4f);
        g.setFont (lnf.titleFont (9.5f));
        g.drawText (String (b + 1), Rectangle<float> (rad * 2.f, rad * 2.f).withCentre (c).translated (0.5f, 0.f), Justification::centred);
    }

    // hover readout
    if (plot.contains (hover))
    {
        const double f = hzOf (hover.x);
        String t = hzText (f) + "   " + noteName (f);
        if (hoverBand >= 0)
        {
            const int ty = (int) std::round (val (hoverBand, ParamIDs::type));
            t = "BAND " + String (hoverBand + 1) + "  " + Choices::types[ty] + "   " + hzText (val (hoverBand, ParamIDs::freq)) + "   "
              + noteName (val (hoverBand, ParamIDs::freq))
              + (MirrorEngine::gainType (ty) ? "   " + String (val (hoverBand, ParamIDs::gain), 1) + " dB" : String())
              + "   Q " + String (val (hoverBand, ParamIDs::q), 2);
        }
        else t << "   " << String (dbOfY (hover.y), 1) << " dB";
        g.setFont (lnf.labelFont (13.f, 0.06f));
        g.setColour (MirrorTheme::iceHot);
        g.drawText (t, plot.reduced (10.f, 8.f).toNearestInt(), Justification::topLeft);
        g.setColour (ice.withAlpha (0.2f));
        g.drawVerticalLine ((int) hover.x, plot.getY(), plot.getBottom());
    }
}

void EqDisplay::mouseMove (const MouseEvent& e)
{
    hover = e.position;
    hoverBand = bandAt (e.position);
    setMouseCursor (hoverBand >= 0 ? MouseCursor::DraggingHandCursor : MouseCursor::CrosshairCursor);
    repaint();
}

void EqDisplay::mouseDown (const MouseEvent& e)
{
    const int b = bandAt (e.position);
    if (b < 0) return;
    if (e.mods.isPopupMenu())
    {
        if (auto* p = prm (b, ParamIDs::on)) p->setValueNotifyingHost (val (b, ParamIDs::on) > 0.5f ? 0.f : 1.f);
        return;
    }
    selected = b;
    if (onSelect) onSelect (b);
    dragBand = b;
    for (auto* k : { ParamIDs::freq, ParamIDs::gain, ParamIDs::on }) if (auto* p = prm (b, k)) p->beginChangeGesture();
    if (val (b, ParamIDs::on) < 0.5f) setVal (b, ParamIDs::on, 1.f);   // grabbing a band switches it on
}

void EqDisplay::mouseDrag (const MouseEvent& e)
{
    if (dragBand < 0) return;
    hover = e.position;
    const double f = hzOf (jlimit (plot.getX(), plot.getRight(), e.position.x));
    setVal (dragBand, ParamIDs::freq, (float) f);
    if (MirrorEngine::gainType ((int) std::round (val (dragBand, ParamIDs::type))))
        setVal (dragBand, ParamIDs::gain, e.mods.isShiftDown() ? 0.f : dbOfY (e.position.y));
    repaint();
}

void EqDisplay::mouseUp (const MouseEvent&)
{
    if (dragBand >= 0) for (auto* k : { ParamIDs::freq, ParamIDs::gain, ParamIDs::on }) if (auto* p = prm (dragBand, k)) p->endChangeGesture();
    dragBand = -1;
}

void EqDisplay::mouseDoubleClick (const MouseEvent& e)
{
    const int b = bandAt (e.position);
    if (b >= 0)
    {
        // double-click a point: back to 0 dB
        if (auto* p = prm (b, ParamIDs::gain)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (0.f)); p->endChangeGesture(); }
        return;
    }
    // double-click empty space: the first unused band becomes a bell right there
    for (int i = 0; i < ParamIDs::numBands; ++i)
        if (val (i, ParamIDs::on) < 0.5f)
        {
            setVal (i, ParamIDs::type, (float) Choices::Bell);
            setVal (i, ParamIDs::freq, (float) hzOf (e.position.x));
            setVal (i, ParamIDs::gain, dbOfY (e.position.y));
            setVal (i, ParamIDs::on, 1.f);
            selected = i;
            if (onSelect) onSelect (i);
            return;
        }
}

void EqDisplay::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const int b = bandAt (e.position) >= 0 ? bandAt (e.position) : selected;
    if (auto* p = prm (b, ParamIDs::q))
    {
        const float q = val (b, ParamIDs::q) * std::pow (1.25f, w.deltaY * 4.f);
        p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (jlimit (0.1f, 18.f, q))); p->endChangeGesture();
    }
}

// ======================================================================= BandChips
BandChips::BandChips (MirrorProcessor& p, std::function<void (int)> cb) : proc (p), onSelect (std::move (cb)) { setMouseCursor (MouseCursor::PointingHandCursor); }
int BandChips::chipAt (Point<int> pt) const { return jlimit (0, ParamIDs::numBands - 1, pt.x * ParamIDs::numBands / jmax (1, getWidth())); }
void BandChips::mouseDown (const MouseEvent& e) { selected = chipAt (e.getPosition()); if (onSelect) onSelect (selected); repaint(); }
void BandChips::mouseDoubleClick (const MouseEvent& e)
{
    const int b = chipAt (e.getPosition());
    if (auto* p = proc.apvts.getParameter (ParamIDs::band (b, ParamIDs::on))) p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.f : 1.f);
}
void BandChips::paint (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    const auto ice = MirrorTheme::ice;
    const float cw = (float) getWidth() / (float) ParamIDs::numBands;
    for (int b = 0; b < ParamIDs::numBands; ++b)
    {
        auto c = Rectangle<float> (cw * (float) b, 0.f, cw, (float) getHeight()).reduced (3.f, 1.f);
        const bool on = proc.apvts.getRawParameterValue (ParamIDs::band (b, ParamIDs::on))->load() > 0.5f;
        const int t = (int) std::round (proc.apvts.getRawParameterValue (ParamIDs::band (b, ParamIDs::type))->load());
        const bool sel = b == selected;
        g.setColour (sel ? ice.withAlpha (0.18f) : Colour (0xcc06090c));
        g.fillRoundedRectangle (c, 7.f);
        g.setColour (sel ? ice : Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (c, 7.f, sel ? 1.4f : 1.f);
        auto led = Rectangle<float> (6.f, 6.f).withCentre ({ c.getX() + 13.f, c.getY() + 13.f });
        if (on) { g.setColour (ice.withAlpha (0.45f)); g.fillEllipse (led.expanded (3.f)); }
        g.setColour (on ? MirrorTheme::iceHot : Colour (0xff2b3034)); g.fillEllipse (led);
        g.setFont (lnf.titleFont (12.f));
        g.setColour (sel ? lnf.theme.text : lnf.theme.label);
        g.drawText (String (b + 1), c.withTrimmedBottom (c.getHeight() * 0.42f), Justification::centred);
        g.setFont (lnf.labelFont (10.5f, 0.12f));
        g.setColour (on ? ice.withAlpha (0.85f) : lnf.theme.dim);
        g.drawText (Choices::typesShort[t], c.withTrimmedTop (c.getHeight() * 0.5f), Justification::centred);
    }
}

// ======================================================================= BandControls
BandControls::BandControls (MirrorProcessor& p, int b) : proc (p), band (b)
{
    using namespace ParamIDs;
    auto id = [b] (const char* k) { return ParamIDs::band (b, k); };
    power = std::make_unique<Switch> (p.apvts, id (on), "", Switch::Power);
    dyn = std::make_unique<Switch> (p.apvts, id (ParamIDs::dyn), "DYNAMIC", Switch::Chip);
    dyn->setTooltip ("DYNAMIC: the band only works when the sound in its range goes over THRESH");
    type = std::make_unique<Choice> (p.apvts, id (ParamIDs::type), Choices::types);
    slope = std::make_unique<Choice> (p.apvts, id (ParamIDs::slope), Choices::slopes);
    mode = std::make_unique<Choice> (p.apvts, id (ParamIDs::mode), Choices::modes);
    freq = std::make_unique<Knob> (p.apvts, id (ParamIDs::freq), "FREQ");
    gain = std::make_unique<Knob> (p.apvts, id (ParamIDs::gain), "GAIN", 0, true);
    q = std::make_unique<Knob> (p.apvts, id (ParamIDs::q), "Q");
    thresh = std::make_unique<Knob> (p.apvts, id (ParamIDs::thresh), "THRESH");
    for (Component* c : { (Component*) power.get(), (Component*) dyn.get(), (Component*) type.get(), (Component*) slope.get(), (Component*) mode.get(),
                          (Component*) freq.get(), (Component*) gain.get(), (Component*) q.get(), (Component*) thresh.get() })
        addAndMakeVisible (c);
}

void BandControls::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (30);
    power->setBounds (top.removeFromRight (56));
    top.removeFromRight (12);
    mode->setBounds (top.removeFromRight (210));
    top.removeFromRight (12);
    slope->setBounds (top.removeFromRight (170));
    top.removeFromRight (12);
    type->setBounds (top);
    r.removeFromTop (8);
    const int cw = 110;
    freq->setBounds (r.getX(), r.getY(), cw, r.getHeight());
    gain->setBounds (r.getX() + cw, r.getY(), cw, r.getHeight());
    q->setBounds (r.getX() + 2 * cw, r.getY(), cw, r.getHeight());
    auto right = r.withTrimmedLeft (3 * cw + 40);
    dyn->setBounds (right.getX(), right.getCentreY() - 14, 120, 28);
    thresh->setBounds (right.getX() + 130, r.getY(), cw, r.getHeight());
}

void BandControls::refresh()
{
    const int t = (int) std::round (proc.apvts.getRawParameterValue (ParamIDs::band (band, ParamIDs::type))->load());
    const int sl = (int) std::round (proc.apvts.getRawParameterValue (ParamIDs::band (band, ParamIDs::slope))->load());
    const bool on = power->isOn();
    const bool cut = t == Choices::LowCut || t == Choices::HighCut;
    auto set = [] (Component* c, bool active) { const float a = active ? 1.f : 0.3f; if (std::abs (c->getAlpha() - a) > 0.001f) c->setAlpha (a); };
    set (gain.get(), on && MirrorEngine::gainType (t));
    set (slope.get(), on && cut);
    set (q.get(), on && ! (cut && sl > 0));
    set (dyn.get(), on && MirrorEngine::gainType (t));
    set (thresh.get(), on && MirrorEngine::gainType (t) && dyn->isOn());
    for (auto* c : { (Component*) type.get(), (Component*) mode.get(), (Component*) freq.get() }) set (c, on);
}

// ======================================================================= Canvas
Canvas::Canvas (MirrorProcessor& p)
    : proc (p), lnf (MirrorTheme::make()),
      display (p, [this] (int b) { select (b); }),
      chips (p, [this] (int b) { select (b); }),
      presets (p, false, false)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (display);
    addAndMakeVisible (chips);
    addAndMakeVisible (presets);
    for (int b = 0; b < ParamIDs::numBands; ++b) { bands[(size_t) b] = std::make_unique<BandControls> (p, b); addChildComponent (*bands[(size_t) b]); }
    amount = std::make_unique<Knob> (p.apvts, ParamIDs::amount, "", 0, false, true);
    amount->slider.setTooltip ("AMOUNT - how strongly the whole curve works: 0 % = flat, 100 % = as set, 150 % = more  (double-click = 100 %)");
    output = std::make_unique<Knob> (p.apvts, ParamIDs::output, "OUTPUT", 0, true);
    monoBass = std::make_unique<Knob> (p.apvts, ParamIDs::monoBass, "MONO BASS");
    analyzer = std::make_unique<Choice> (p.apvts, ParamIDs::analyzer, Choices::analyzers, 0, true);
    for (Component* c : { (Component*) amount.get(), (Component*) output.get(), (Component*) monoBass.get(), (Component*) analyzer.get() }) addAndMakeVisible (c);
    select (2);
    setSize (W, H);
    timerCallback();
    startTimerHz (30);
}

Canvas::~Canvas() { setLookAndFeel (nullptr); }

void Canvas::select (int b)
{
    display.selected = b; chips.selected = b;
    if (b != shown)
    {
        for (int i = 0; i < ParamIDs::numBands; ++i) bands[(size_t) i]->setVisible (i == b);
        shown = b;
    }
    display.repaint(); chips.repaint();
}

void Canvas::resized()
{
    panels.clear();
    const int m = 20;
    presets.setBounds (W - m - 300, 34, 300, 90);
    display.setBounds (m, 150, W - 2 * m, 450);
    const Rectangle<int> bandA (m, 614, 980, H - m - 614), globA (bandA.getRight() + 14, 614, W - m - bandA.getRight() - 14, H - m - 614);
    panels.push_back ({ "BAND", bandA });
    panels.push_back ({ "SOUL", globA });
    auto b = bandA.reduced (16, 10);
    b.removeFromTop (30);
    chips.setBounds (b.removeFromTop (46));
    b.removeFromTop (12);
    for (auto& bc : bands) bc->setBounds (b);
    auto gA = globA.reduced (14, 10);
    gA.removeFromTop (26);
    auto left = gA.removeFromLeft (130);
    amount->setBounds (left.withTrimmedTop (22));
    auto right = gA.removeFromRight (104);
    analyzer->setBounds (right.withSizeKeepingCentre (right.getWidth(), 96));
    output->setBounds (gA.removeFromTop (gA.getHeight() / 2));
    monoBass->setBounds (gA);
    renderBackground();
}

void Canvas::renderBackground()
{
    const float scale = 2.f;
    background = Image (Image::RGB, (int) (W * scale), (int) (H * scale), true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (scale));
    const auto ice = MirrorTheme::ice;
    g.fillAll (lnf.theme.bg);
    for (int i = 0; i < 4; ++i)
    {
        const float a = -0.5f + (float) i * 0.2f;
        paint::beam (g, { 160.f, -20.f }, -a - 0.2f, 0.05f, 1200.f, ice, 0.04f);
        paint::beam (g, { W - 160.f, -20.f }, a + 0.2f, 0.05f, 1200.f, ice, 0.04f);
    }
    {
        ColourGradient haze (ice.withAlpha (0.12f), W * 0.5f, 70.f, ice.withAlpha (0.f), W * 0.5f + 420.f, 70.f, true);
        g.setGradientFill (haze); g.fillRect (0, 0, W, 160);
        ColourGradient vig (Colours::transparentBlack, W * 0.5f, H * 0.45f, Colours::black.withAlpha (0.7f), 0.f, -80.f, true);
        g.setGradientFill (vig); g.fillRect (0, 0, W, H);
        ColourGradient line (ice.withAlpha (0.f), 0.f, 0.f, ice.withAlpha (0.f), (float) W, 0.f, false);
        line.addColour (0.5, ice.withAlpha (0.8f));
        g.setGradientFill (line); g.fillRect (0.f, 136.f, (float) W, 1.f);
    }
    // title + its reflection in the "mirror" line
    const String title ("MIRROR OF SOULS");
    const auto area = Rectangle<float> (0.f, 30.f, (float) W, 54.f);
    g.setFont (lnf.titleFont (40.f, 0.22f));
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (ice.withAlpha (0.10f / (float) i));
        for (auto o : { Point<float> (-1.6f * (float) i, 0.f), Point<float> (1.6f * (float) i, 0.f), Point<float> (0.f, 1.6f * (float) i), Point<float> (0.f, -1.6f * (float) i) })
            g.drawText (title, area.translated (o.x, o.y), Justification::centred);
    }
    ColourGradient tg (Colours::white, 0.f, area.getY() + 8.f, MirrorTheme::ice, 0.f, area.getBottom() - 6.f, false);
    g.setGradientFill (tg);
    g.drawText (title, area, Justification::centred);
    {
        Graphics::ScopedSaveState ss (g);
        const float axis = area.getBottom() + 2.f;
        g.addTransform (AffineTransform::verticalFlip (axis * 2.f));
        g.reduceClipRegion (Rectangle<int> (0, (int) area.getY() + 10, W, (int) area.getHeight()));
        ColourGradient rg (ice.withAlpha (0.0f), 0.f, area.getY() + 10.f, ice.withAlpha (0.28f), 0.f, area.getBottom(), false);
        g.setGradientFill (rg);
        g.drawText (title, area, Justification::centred);
    }
    g.setFont (lnf.labelFont (12.5f, 0.6f));
    g.setColour (ice.withAlpha (0.8f));
    g.drawText ("HARDSTYLE  EQUALIZER", 0, 108, W, 18, Justification::centred);
    g.setFont (lnf.titleFont (10.f, 0.25f));
    g.setColour (lnf.theme.text.withAlpha (0.75f));
    g.drawText ("V1.1", 20, 40, 300, 14, Justification::left);
    g.setFont (lnf.labelFont (12.f, 0.28f));
    g.setColour (lnf.theme.label);
    g.drawText ("8 BANDS  /  DYNAMIC  /  MID-SIDE", 20, 58, 340, 16, Justification::left);
    g.setColour (ice);
    g.drawText ("SEE YOUR SOUND", 20, 76, 320, 16, Justification::left);
    doomsday::brand::drawBadge (g, { 290.f, 36.f, 110.f, 56.f }, 0.5f);
    for (auto& [t, a] : panels) paint::glassPanel (g, lnf, a, t, 0);
    {
        auto a = panels[1].second;
        g.setFont (lnf.labelFont (11.f, 0.2f));
        g.setColour (lnf.theme.label);
        g.drawText ("AMOUNT", a.getX() + 14, a.getY() + 34, 130, 16, Justification::centred);
        g.drawText ("ANALYZER", a.getRight() - 118, a.getY() + 34, 104, 16, Justification::centred);
    }
}

void Canvas::timerCallback()
{
    display.tick();
    chips.repaint();
    presets.refresh();
    for (auto& b : bands) if (b->isVisible()) b->refresh();
}

void Canvas::paint (Graphics& g)
{
    if (background.isValid()) g.drawImage (background, getLocalBounds().toFloat(), RectanglePlacement::stretchToFit);
}

MirrorEditor::MirrorEditor (MirrorProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (840, 516, 2100, 1290);
    getConstrainer()->setFixedAspectRatio ((double) Canvas::W / (double) Canvas::H);
    setSize (1120, 688);
}
