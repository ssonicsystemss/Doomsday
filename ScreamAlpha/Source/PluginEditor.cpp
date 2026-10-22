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

// ======================================================================= MOD graph
namespace
{
    const StringArray shapeNames { "Off", "Rise", "Fall", "Up/Dn", "Wobble", "Steps", "Custom" };
    const StringArray rateNames { "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 bar" };
    float raw (AlphaProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
    void setParam (AlphaProcessor& p, const char* id, float v)
    {
        if (auto* q = p.apvts.getParameter (id)) { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (v)); q->endChangeGesture(); }
    }
}

int ModGraph::shapeType() const { return roundToInt (raw (proc, alpha::mod::shapeId (m))); }

void ModGraph::resized() { plot = getLocalBounds().toFloat().reduced (14.f, 14.f).withTrimmedTop (8.f); }

Point<float> ModGraph::toScreen (float x, float v) const
{
    return { plot.getX() + plot.getWidth() * x / 1000.f, plot.getBottom() - plot.getHeight() * v };
}

Point<float> ModGraph::midOf (int seg) const
{
    const auto& a = shape.pts[(size_t) seg]; const auto& b = shape.pts[(size_t) seg + 1];
    return toScreen (0.5f * (a.ms + b.ms), a.v + (b.v - a.v) * doomsday::mod::Shape::bent (0.5f, a.bend));
}

int ModGraph::pointAt (Point<float> s) const
{
    if (shapeType() != alpha::mod::Custom) return -1;
    int best = -1; float bd = 10.f;
    for (int i = 0; i < shape.n; ++i)
    {
        const float d = toScreen (shape.pts[(size_t) i].ms, shape.pts[(size_t) i].v).getDistanceFrom (s);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

int ModGraph::midAt (Point<float> s) const
{
    if (shapeType() != alpha::mod::Custom) return -1;
    for (int i = 0; i + 1 < shape.n; ++i)
    {
        const auto& a = shape.pts[(size_t) i]; const auto& b = shape.pts[(size_t) i + 1];
        if ((b.ms - a.ms) * plot.getWidth() / 1000.f < 22.f || std::abs (b.v - a.v) < 0.02f) continue;   // too short / flat to bend
        if (midOf (i).getDistanceFrom (s) < 8.f) return i;
    }
    return -1;
}

float ModGraph::snapX (float x, bool free) const
{
    x = jlimit (0.f, 1000.f, x);
    return free ? x : std::round (x / 62.5f) * 62.5f;      // grid = 1/16 of the cycle
}

void ModGraph::tick()
{
    const int ser = proc.customSerial(), type = shapeType();
    const double beats = proc.modBeats (m);
    const float ph = proc.modPhase (m);
    if (ser != lastSerial) { lastSerial = ser; if (drag < 0 && bendSeg < 0) shape = proc.getCustom (m); repaint(); }
    if (type != lastType || beats != lastBeats || (type != 0 && std::abs (ph - lastPh) > 0.002f))
    { lastType = type; lastBeats = beats; lastPh = ph; repaint(); }
}

void ModGraph::paint (Graphics& g)
{
    const auto& t = tokens();
    auto& lnf = lnfOf (*this);
    const Colour col = alphaui::modColour (m);
    auto r = getLocalBounds().toFloat();
    g.setColour (t.ground.withAlpha (0.7f)); g.fillRoundedRectangle (r, 8.f);

    // grid: 16ths of the cycle (bar lines stronger), 25 / 50 / 75 %
    const double beats = proc.modBeats (m);
    const int n16 = (int) std::round (beats * 4.0);
    const bool even = std::abs (beats * 4.0 - n16) < 1e-6 && n16 >= 2;
    const int lines = even ? n16 : 4;
    for (int i = 1; i < lines; ++i)
    {
        const bool strong = even && n16 > 4 && i % 4 == 0;
        g.setColour (strong ? t.border : t.divider);
        g.fillRect (plot.getX() + plot.getWidth() * (float) i / (float) lines, plot.getY(), 1.f, plot.getHeight());
    }
    g.setColour (t.divider.withAlpha (0.6f));
    for (int i = 1; i < 4; ++i) g.fillRect (plot.getX(), plot.getBottom() - plot.getHeight() * (float) i / 4.f, plot.getWidth(), 1.f);

    const int type = shapeType();
    const int target = proc.modTarget (m);
    g.setFont (lnf.font (11.5f, SemiBold));
    {
        String tag = target > 0 ? String (CharPointer_UTF8 ("\xe2\x86\x92 ")) + proc.targetParam (target)->getName (40).toUpperCase() : String ("not assigned - press ASSIGN");
        g.setColour (target > 0 ? col : t.textMute);
        g.drawText (tag, r.reduced (12.f, 4.f).withHeight (18.f), Justification::topRight);
    }
    if (type == alpha::mod::Off)
    {
        g.setFont (lnf.font (12.5f, Medium)); g.setColour (t.textMute);
        g.drawText ("Off" + String (CharPointer_UTF8 (" \xc2\xb7 ")) + "pick a shape above, or click here to draw your own", plot, Justification::centred, true);
        return;
    }

    Path p;
    if (type == alpha::mod::Custom)
    {
        for (int i = 0; i < shape.n; ++i)
        {
            const auto& a = shape.pts[(size_t) i];
            const auto pa = toScreen (a.ms, a.v);
            if (i == 0) { p.startNewSubPath (plot.getX(), pa.y); p.lineTo (pa); }
            if (i + 1 < shape.n)
            {
                const auto& b = shape.pts[(size_t) i + 1];
                for (int k = 1; k <= 24; ++k)
                {
                    const float tt = (float) k / 24.f;
                    p.lineTo (toScreen (a.ms + (b.ms - a.ms) * tt, a.v + (b.v - a.v) * doomsday::mod::Shape::bent (tt, a.bend)));
                }
            }
            else p.lineTo (plot.getRight(), pa.y);
        }
    }
    else
    {
        for (int i = 0; i <= 400; ++i)
        {
            const float ph = (float) i / 400.f;
            const auto pt = toScreen (ph * 1000.f, alpha::mod::builtin (type, jmin (0.99999, (double) ph) * beats, beats, m));
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
    }
    Path fill (p); fill.lineTo (plot.getRight(), plot.getBottom()); fill.lineTo (plot.getX(), plot.getBottom()); fill.closeSubPath();
    g.setColour (col.withAlpha (0.12f)); g.fillPath (fill);
    g.setColour (col); g.strokePath (p, PathStrokeType (2.f));

    if (type == alpha::mod::Custom)
    {
        for (int i = 0; i + 1 < shape.n; ++i)
        {
            const auto& a = shape.pts[(size_t) i]; const auto& b = shape.pts[(size_t) i + 1];
            if ((b.ms - a.ms) * plot.getWidth() / 1000.f < 22.f || std::abs (b.v - a.v) < 0.02f) continue;
            const auto c = midOf (i);
            g.setColour (i == hoverMid || i == bendSeg ? Colours::white : col.withAlpha (0.7f));
            g.drawEllipse (Rectangle<float> (7.f, 7.f).withCentre (c), 1.4f);
        }
        for (int i = 0; i < shape.n; ++i)
        {
            const auto c = toScreen (shape.pts[(size_t) i].ms, shape.pts[(size_t) i].v);
            const bool hot = i == hover || i == drag;
            g.setColour (hot ? col : t.ground); g.fillEllipse (Rectangle<float> (11.f, 11.f).withCentre (c));
            g.setColour (hot ? Colours::white : col); g.drawEllipse (Rectangle<float> (11.f, 11.f).withCentre (c), 1.6f);
        }
        g.setFont (lnf.font (11.f, Medium)); g.setColour (t.textMute);
        g.drawText ("click = point" + String (CharPointer_UTF8 ("  \xc2\xb7  ")) + "drag a ring = bend" + String (CharPointer_UTF8 ("  \xc2\xb7  "))
                    + "double-click = delete" + String (CharPointer_UTF8 ("  \xc2\xb7  ")) + "Shift = no snap",
                    r.reduced (12.f, 4.f).withHeight (18.f), Justification::topLeft);
    }
    else
    {
        g.setFont (lnf.font (11.f, Medium)); g.setColour (t.textMute);
        g.drawText ("click to edit this shape", r.reduced (12.f, 4.f).withHeight (18.f), Justification::topLeft);
    }

    // where it is now
    const float ph = proc.modPhase (m);
    const float px = plot.getX() + ph * plot.getWidth();
    g.setColour (Colours::white.withAlpha (0.45f)); g.fillRect (px, plot.getY(), 1.5f, plot.getHeight());
    g.setColour (Colours::white); g.fillEllipse (Rectangle<float> (8.f, 8.f).withCentre (toScreen (ph * 1000.f, proc.modValue (m))));
}

void ModGraph::makeCustom()
{
    const int type = shapeType();
    shape = alpha::mod::toPoints (type == alpha::mod::Off ? (int) alpha::mod::Rise : type, proc.modBeats (m), m);
    if (type == alpha::mod::Custom) shape = proc.getCustom (m);
    proc.setCustom (m, shape);
    if (type != alpha::mod::Custom) setParam (proc, alpha::mod::shapeId (m), (float) alpha::mod::Custom);
}

void ModGraph::commit() { proc.setCustom (m, shape); lastSerial = proc.customSerial(); repaint(); }

void ModGraph::mouseMove (const MouseEvent& e)
{
    const int h = pointAt (e.position), hm = h < 0 ? midAt (e.position) : -1;
    if (h != hover || hm != hoverMid) { hover = h; hoverMid = hm; repaint(); }
    setMouseCursor (h >= 0 || hm >= 0 ? MouseCursor::DraggingHandCursor : MouseCursor::CrosshairCursor);
}

void ModGraph::mouseDown (const MouseEvent& e)
{
    if (shapeType() != alpha::mod::Custom) { makeCustom(); repaint(); return; }      // first touch: the ready shape becomes editable points
    shape = proc.getCustom (m);
    const int hit = pointAt (e.position);
    if (e.mods.isPopupMenu())
    {
        if (hit > 0 && hit < shape.n - 1) { shape.remove (hit); hover = -1; commit(); }
        return;
    }
    if (hit >= 0) { drag = hit; return; }
    const int seg = midAt (e.position);
    if (seg >= 0) { bendSeg = seg; bendStart = shape.pts[(size_t) seg].bend; bendY = e.position.y; return; }
    if (! plot.expanded (6.f).contains (e.position)) return;
    const float x = snapX ((e.position.x - plot.getX()) / plot.getWidth() * 1000.f, e.mods.isShiftDown());
    const float v = jlimit (0.f, 1.f, (plot.getBottom() - e.position.y) / plot.getHeight());
    if (x <= 0.f || x >= 1000.f || ! shape.insert (x, v)) return;
    for (int i = 0; i < shape.n; ++i) if (shape.pts[(size_t) i].ms == x && shape.pts[(size_t) i].v == v) drag = i;
    commit();
}

void ModGraph::mouseDrag (const MouseEvent& e)
{
    if (bendSeg >= 0 && bendSeg + 1 < shape.n)
    {
        const auto& a = shape.pts[(size_t) bendSeg]; const auto& b = shape.pts[(size_t) bendSeg + 1];
        const float dir = b.v >= a.v ? 1.f : -1.f;
        shape.pts[(size_t) bendSeg].bend = jlimit (-1.f, 1.f, bendStart + (e.position.y - bendY) / 120.f * dir);
        commit();
        return;
    }
    if (drag < 0 || drag >= shape.n) return;
    float x = snapX ((e.position.x - plot.getX()) / plot.getWidth() * 1000.f, e.mods.isShiftDown());
    if (drag == 0) x = 0.f;
    else if (drag == shape.n - 1) x = 1000.f;
    else x = jlimit (shape.pts[(size_t) drag - 1].ms, shape.pts[(size_t) drag + 1].ms, x);
    shape.pts[(size_t) drag].ms = x;
    shape.pts[(size_t) drag].v = jlimit (0.f, 1.f, (plot.getBottom() - e.position.y) / plot.getHeight());
    commit();
}

void ModGraph::mouseUp (const MouseEvent&) { drag = bendSeg = -1; repaint(); }

void ModGraph::mouseDoubleClick (const MouseEvent& e)
{
    if (shapeType() != alpha::mod::Custom) return;
    shape = proc.getCustom (m);
    const int hit = pointAt (e.position);
    if (hit > 0 && hit < shape.n - 1) { shape.remove (hit); hover = drag = -1; commit(); return; }
    const int seg = midAt (e.position);
    if (seg >= 0) { shape.pts[(size_t) seg].bend = 0.f; commit(); }
}

// ======================================================================= range slider
RangeBar::RangeBar (AlphaProcessor& p, int mod) : proc (p), m (mod), range (p.apvts.getParameter (alpha::mod::rangeId (mod)))
{
    setTooltip ("Range: drag to where the knob should go. + = from the knob up to here, - = from the knob down to here");
}

float RangeBar::base01() const { if (auto* prm = proc.targetParam (proc.modTarget (m))) return prm->getValue(); return 0.f; }
bool RangeBar::plus() const { return roundToInt (raw (proc, alpha::mod::polId (m))) == 0; }
Rectangle<float> RangeBar::track() const { return getLocalBounds().toFloat().reduced (8.f, 0.f); }
float RangeBar::valueAtX (float x) const
{
    const auto tr = track();
    const float v = jlimit (0.f, 1.f, (x - tr.getX()) / tr.getWidth());
    const float b = base01();
    return plus() ? jmax (v, b) : jmin (v, b);
}

void RangeBar::tick()
{
    const float b = base01(), rg = range->getValue();
    const int tg = proc.modTarget (m), pl = plus() ? 0 : 1;
    if (std::abs (b - lastBase) > 1e-4f || std::abs (rg - lastRange) > 1e-4f || tg != lastTarget || pl != lastPol)
    { lastBase = b; lastRange = rg; lastTarget = tg; lastPol = pl; repaint(); }
}

void RangeBar::paint (Graphics& g)
{
    const auto& t = tokens();
    auto& lnf = lnfOf (*this);
    const Colour col = alphaui::modColour (m);
    auto r = getLocalBounds().toFloat();
    g.setColour (t.control); g.fillRoundedRectangle (r, 7.f);
    g.setColour (t.border); g.drawRoundedRectangle (r.reduced (0.5f), 7.f, 1.f);
    auto* prm = proc.targetParam (proc.modTarget (m));
    g.setFont (lnf.font (11.5f, SemiBold));
    if (prm == nullptr)
    {
        g.setColour (t.textMute); g.drawText ("Range" + String (CharPointer_UTF8 (" \xc2\xb7 ")) + "assign a knob first", r, Justification::centred);
        return;
    }
    const auto tr = track();
    const float b = prm->getValue();
    const float e = plus() ? jmax (range->getValue(), b) : jmin (range->getValue(), b);
    const float xb = tr.getX() + b * tr.getWidth(), xe = tr.getX() + e * tr.getWidth();
    g.setColour (col.withAlpha (0.3f)); g.fillRect (Rectangle<float> (jmin (xb, xe), r.getY() + 3.f, std::abs (xe - xb), r.getHeight() - 6.f));
    g.setColour (Colours::white.withAlpha (0.8f)); g.fillRect (xb - 1.f, r.getY() + 4.f, 2.f, r.getHeight() - 8.f);      // the knob
    g.setColour (col); g.fillRoundedRectangle (Rectangle<float> (6.f, r.getHeight() - 6.f).withCentre ({ xe, r.getCentreY() }), 3.f);   // the slider
    const String arrow (CharPointer_UTF8 ("  \xe2\x86\x92  "));
    // the text sits in the bigger empty part of the bar (never under the markers)
    const float lo = jmin (xb, xe), hi = jmax (xb, xe);
    auto textArea = r.reduced (12.f, 0.f);
    if (lo - r.getX() > 150.f || r.getRight() - hi > 150.f)
        textArea = lo - r.getX() >= r.getRight() - hi ? r.withRight (lo - 6.f).reduced (8.f, 0.f) : r.withLeft (hi + 6.f).reduced (8.f, 0.f);
    g.setColour (t.text);
    g.drawFittedText (prm->getText (b, 24) + arrow + prm->getText (e, 24), textArea.toNearestInt(), Justification::centred, 1, 0.8f);
}

void RangeBar::mouseDown (const MouseEvent& e)
{
    if (proc.targetParam (proc.modTarget (m)) == nullptr) return;
    range->beginChangeGesture();
    range->setValueNotifyingHost (valueAtX (e.position.x));
}
void RangeBar::mouseDrag (const MouseEvent& e)
{
    if (proc.targetParam (proc.modTarget (m)) == nullptr) return;
    range->setValueNotifyingHost (valueAtX (e.position.x));
}
void RangeBar::mouseUp (const MouseEvent&) { if (proc.targetParam (proc.modTarget (m)) != nullptr) range->endChangeGesture(); }

// ======================================================================= MOD panel
ModPanel::ModPanel (AlphaCanvas& c, AlphaProcessor& p) : canvas (c), proc (p)
{
    for (int m = 0; m < AlphaProcessor::numMods; ++m)
    {
        auto& d = mods[(size_t) m];
        const Colour col = alphaui::modColour (m);
        d.shape = std::make_unique<Segmented> (p.apvts, alpha::mod::shapeId (m), shapeNames, col);
        d.rate = std::make_unique<Segmented> (p.apvts, alpha::mod::divId (m), rateNames, col);
        d.pol = std::make_unique<Segmented> (p.apvts, alpha::mod::polId (m), StringArray { "+", String (CharPointer_UTF8 ("\xe2\x88\x92")) }, col);
        d.graph = std::make_unique<ModGraph> (p, m);
        d.range = std::make_unique<RangeBar> (p, m);
        d.shape->setTooltip ("Shape of one cycle (touch the graph to draw your own)");
        d.rate->setTooltip ("Length of one cycle, locked to the FL tempo");
        d.pol->setTooltip ("+ = the knob moves up from where it is, - = down");
        d.assign.setButtonText ("ASSIGN");
        d.assign.setTooltip ("Click, then click any knob: MOD " + String (m + 1) + " moves it");
        d.assign.onClick = [this, m] { canvas.beginAssign (m); };
        d.clear.setButtonText (String (CharPointer_UTF8 ("\xc3\x97")));
        d.clear.setTooltip ("Remove the assignment");
        d.clear.onClick = [this, m] { canvas.assign (m, 0); };
        for (auto* comp : { (Component*) d.shape.get(), (Component*) d.rate.get(), (Component*) d.pol.get(), (Component*) d.graph.get(),
                            (Component*) d.range.get(), (Component*) &d.assign, (Component*) &d.clear })
            addChildComponent (comp);
    }
    select (0);
}

Rectangle<int> ModPanel::tabBounds (int m) const { return { m * 200, 0, 190, 30 }; }

String ModPanel::targetName (int m) const
{
    if (auto* prm = proc.targetParam (proc.modTarget (m))) return prm->getName (40).toUpperCase();
    return {};
}

void ModPanel::select (int m)
{
    sel = jlimit (0, AlphaProcessor::numMods - 1, m);
    for (int i = 0; i < AlphaProcessor::numMods; ++i)
    {
        auto& d = mods[(size_t) i];
        for (auto* comp : { (Component*) d.shape.get(), (Component*) d.rate.get(), (Component*) d.pol.get(), (Component*) d.graph.get(),
                            (Component*) d.range.get(), (Component*) &d.assign, (Component*) &d.clear })
            comp->setVisible (i == sel);
    }
    repaint();
}

void ModPanel::resized()
{
    auto a = getLocalBounds();
    a.removeFromTop (38);
    for (auto& d : mods)
    {
        auto row = a;
        auto top = row.removeFromTop (28);
        d.shape->setBounds (top.removeFromLeft (440).reduced (0, 1));
        top.removeFromLeft (14);
        d.rate->setBounds (top.reduced (0, 1));
        row.removeFromTop (10);
        auto bottom = row.removeFromBottom (30);
        row.removeFromBottom (10);
        d.graph->setBounds (row);
        d.assign.setBounds (bottom.removeFromLeft (170));
        bottom.removeFromLeft (6);
        d.clear.setBounds (bottom.removeFromLeft (30));
        bottom.removeFromLeft (14);
        d.pol->setBounds (bottom.removeFromLeft (76).reduced (0, 1));
        bottom.removeFromLeft (10);
        d.range->setBounds (bottom);
    }
}

void ModPanel::paint (Graphics& g)
{
    const auto& t = tokens();
    auto& lnf = lnfOf (*this);
    for (int m = 0; m < AlphaProcessor::numMods; ++m)
    {
        auto tb = tabBounds (m).toFloat();
        const Colour col = alphaui::modColour (m);
        const bool on = m == sel;
        g.setColour (col); g.fillEllipse (Rectangle<float> (8.f, 8.f).withCentre ({ tb.getX() + 6.f, tb.getCentreY() - 2.f }));
        g.setFont (lnf.font (13.5f, Bold, 0.05f)); g.setColour (on ? t.text : t.textDim);
        g.drawText ("MOD " + String (m + 1), tb.withTrimmedLeft (16.f).withTrimmedBottom (4.f), Justification::centredLeft);
        const auto name = targetName (m);
        if (name.isNotEmpty())
        {
            g.setFont (lnf.font (11.f, SemiBold)); g.setColour (on ? col : t.textMute);
            g.drawText (String (CharPointer_UTF8 ("\xe2\x86\x92 ")) + name, tb.withTrimmedLeft (78.f).withTrimmedBottom (4.f), Justification::centredLeft, true);
        }
        if (on) { g.setColour (col); g.fillRect (tb.getX(), tb.getBottom() - 2.f, tb.getWidth() - 20.f, 2.f); }
    }
}

void ModPanel::mouseDown (const MouseEvent& e)
{
    for (int m = 0; m < AlphaProcessor::numMods; ++m) if (tabBounds (m).contains (e.getPosition())) { select (m); return; }
}

void ModPanel::tick()
{
    for (int m = 0; m < AlphaProcessor::numMods; ++m)
    {
        auto& d = mods[(size_t) m];
        const auto name = targetName (m);
        if (name != d.lastName)
        {
            d.lastName = name;
            d.assign.setButtonText (name.isEmpty() ? String ("ASSIGN") : name);
            d.clear.setEnabled (name.isNotEmpty());
            repaint();
        }
    }
    mods[(size_t) sel].graph->tick();
    mods[(size_t) sel].range->tick();
}

// ======================================================================= ASSIGN overlay
void AssignOverlay::paint (Graphics& g)
{
    const auto& t = tokens();
    auto& lnf = lnfOf (*this);
    const Colour col = alphaui::modColour (mod);
    g.fillAll (t.ground.withAlpha (0.55f));
    const auto& ks = canvas.modKnobs();
    for (int i = 0; i < (int) ks.size(); ++i)
    {
        auto b = ks[(size_t) i].knob->getBounds().toFloat().reduced (3.f, 1.f);
        if (i == hover) { g.setColour (col.withAlpha (0.2f)); g.fillRoundedRectangle (b, 8.f); }
        g.setColour (col.withAlpha (i == hover ? 1.f : 0.6f));
        g.drawRoundedRectangle (b, 8.f, i == hover ? 2.f : 1.2f);
    }
    auto box = Rectangle<float> (0.f, 0.f, 520.f, 40.f).withCentre ({ getWidth() * 0.5f, 28.f });
    g.setColour (t.surface); g.fillRoundedRectangle (box, 10.f);
    g.setColour (col); g.drawRoundedRectangle (box, 10.f, 1.5f);
    g.setFont (lnf.font (14.f, SemiBold)); g.setColour (t.text);
    g.drawText ("Click a knob for MOD " + String (mod + 1) + String (CharPointer_UTF8 ("   \xc2\xb7   ")) + "Esc / anywhere else = cancel", box, Justification::centred);
}

void AssignOverlay::mouseMove (const MouseEvent& e)
{
    const int h = canvas.knobAt (e.getPosition());
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
}

void AssignOverlay::mouseDown (const MouseEvent& e)
{
    const int h = canvas.knobAt (e.getPosition());
    if (h >= 0) canvas.assign (mod, canvas.modKnobs()[(size_t) h].target);
    hover = -1;
    canvas.endAssign();
}

bool AssignOverlay::keyPressed (const KeyPress& k)
{
    if (k == KeyPress::escapeKey) { canvas.endAssign(); return true; }
    return false;
}

// ======================================================================= canvas
Component* AlphaCanvas::knob (const char* id, const char* label, Colour c, bool bipolar)
{
    auto k = std::make_unique<Knob> (proc.apvts, id, label, c, bipolar);
    auto* kp = k.get(); addAndMakeVisible (*kp); owned.push_back (std::move (k));
    if (const int ti = alpha::mod::targetIndex (id); ti > 0) assignable.push_back ({ kp, ti, c });
    return kp;
}

AlphaCanvas::AlphaCanvas (AlphaProcessor& p) : proc (p), scope (p), mods (*this, p), overlay (*this)
{
    setLookAndFeel (&lnf);
    const auto& t = tokens();
    const Colour hot = t.hot, cool = t.cool, n { 0xffb4bac3 };
    auto seg = [this] (const char* id, StringArray labels, Colour c)
    {
        auto s = std::make_unique<Segmented> (proc.apvts, id, labels, c);
        auto* sp = s.get(); addAndMakeVisible (*sp); owned.push_back (std::move (s)); return (Component*) sp;
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
        fortuneKnob = k.get(); addAndMakeVisible (*k);
        assignable.push_back ({ k.get(), alpha::mod::targetIndex ("fortune"), hot });
        owned.push_back (std::move (k));
        cards.push_back ({ "Fortune", hot, 1, {} });
    }
    // FILTER (its cutoff moved by MOD 1 / MOD 2 when assigned)
    cards.push_back ({ "Filter", cool, 2, { knob ("cutoffHz", "Cutoff", cool), knob ("reso", "Reso", cool) },
                       seg ("filtMode", { "LP 24", "LP 12", "Band", "High" }, cool) });
    addAndMakeVisible (mods);
    addAndMakeVisible (scope);
    addChildComponent (overlay);

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
    mods.tick();
    updateModKnobs();
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
    // row 3: Space | Filter      row 4: MOD 1 / MOD 2 | Fortune
    cards[6].area = { m, 466, 6 * kw + 32 + 240, 150 };
    cards[8].area = { cards[6].area.getRight() + gap, 466, W - m - cards[6].area.getRight() - gap, 150 };
    modArea = { m, 628, cards[6].area.getWidth(), H - 20 - 628 };
    cards[7].area = { cards[8].area.getX(), 628, cards[8].area.getWidth(), modArea.getHeight() };
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
    mods.setBounds (modArea.reduced (16, 12));
    overlay.setBounds (getLocalBounds());
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
    g.drawText ("v0.4" + dot + "test build", Rectangle<float> (tx + 170.f, 0.f, 200.f, 56.f), Justification::centredLeft);
    for (auto& c : cards) card (g, lnf, c.area, c.title, c.dot);
    card (g, lnf, modArea);
    {   // FORTUNE caption
        auto a = cards[7].area;
        g.setFont (lnf.font (11.5f, Medium)); g.setColour (t.textMute);
        g.drawFittedText ("One knob: more hash, drive, chaos, detune, formant, cutoff, throw and space (level kept steady)",
                          a.reduced (20, 0).withTop (a.getBottom() - 56).withHeight (40), Justification::centred, 2);
    }
}

// ======================================================================= ASSIGN
void AlphaCanvas::beginAssign (int mod)
{
    overlay.mod = mod;
    overlay.setBounds (getLocalBounds());
    overlay.setVisible (true);
    overlay.toFront (true);
    overlay.repaint();
}

void AlphaCanvas::endAssign() { overlay.setVisible (false); }

void AlphaCanvas::assign (int mod, int targetIndex)
{
    if (auto* q = proc.apvts.getParameter (alpha::mod::targetPid (mod)))
    { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 ((float) targetIndex)); q->endChangeGesture(); }
    if (auto* prm = proc.targetParam (targetIndex))
    {
        // a new assignment starts with an audible range and a shape
        const float b = prm->getValue();
        const bool up = roundToInt (proc.apvts.getRawParameterValue (alpha::mod::polId (mod))->load()) == 0;
        if (auto* r = proc.apvts.getParameter (alpha::mod::rangeId (mod)))
        { r->beginChangeGesture(); r->setValueNotifyingHost (up ? jmin (1.f, b + 0.35f) : jmax (0.f, b - 0.35f)); r->endChangeGesture(); }
        if (roundToInt (proc.apvts.getRawParameterValue (alpha::mod::shapeId (mod))->load()) == alpha::mod::Off)
            if (auto* sh = proc.apvts.getParameter (alpha::mod::shapeId (mod)))
            { sh->beginChangeGesture(); sh->setValueNotifyingHost (sh->convertTo0to1 ((float) alpha::mod::Wobble)); sh->endChangeGesture(); }
    }
    mods.select (mod);
}

int AlphaCanvas::knobAt (Point<int> p) const
{
    for (int i = 0; i < (int) assignable.size(); ++i) if (assignable[(size_t) i].knob->getBounds().contains (p)) return i;
    return -1;
}

// knobs taken by a MOD wear its colour and a ring showing the range + where the MOD moves them now
void AlphaCanvas::updateModKnobs()
{
    namespace am = alpha::mod;
    int tgt[AlphaProcessor::numMods];
    for (int m = 0; m < AlphaProcessor::numMods; ++m)
    {
        tgt[m] = proc.modTarget (m);
        // the slider never sits on the wrong side of the knob (turning the knob past it pushes it along)
        if (auto* prm = proc.targetParam (tgt[m]))
        {
            auto* r = proc.apvts.getParameter (am::rangeId (m));
            const float b = prm->getValue(), rv = r->getValue();
            const bool up = roundToInt (proc.apvts.getRawParameterValue (am::polId (m))->load()) == 0;
            if ((up && rv < b - 1e-4f) || (! up && rv > b + 1e-4f)) r->setValueNotifyingHost (b);
        }
    }
    for (auto& k : assignable)
    {
        int which = -1; float depth = 0.f, now = -1.f;
        for (int m = 0; m < AlphaProcessor::numMods; ++m)
        {
            if (tgt[m] != k.target) continue;
            if (which < 0) which = m;
            auto* prm = proc.targetParam (tgt[m]);
            const float b = prm->getValue(), rv = proc.apvts.getParameter (am::rangeId (m))->getValue();
            const bool up = roundToInt (proc.apvts.getRawParameterValue (am::polId (m))->load()) == 0;
            depth += (up ? jmax (rv, b) : jmin (rv, b)) - b;
            if (proc.modKnob01 (m) >= 0.f) now = proc.modKnob01 (m);
        }
        if (which != k.shownMod)
        {
            k.shownMod = which;
            k.knob->setAccent (which < 0 ? k.accent : alphaui::modColour (which));
        }
        k.knob->setModulation (which < 0 ? 0.f : depth, which < 0 ? -1.f : now);
    }
}

// ======================================================================= editor
AlphaEditor::AlphaEditor (AlphaProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (744, 600, 2232, 1800);
    getConstrainer()->setFixedAspectRatio ((double) AlphaCanvas::W / (double) AlphaCanvas::H);
    setSize (AlphaCanvas::W, AlphaCanvas::H);
}

void AlphaEditor::resized()
{
    canvas.setBounds (0, 0, AlphaCanvas::W, AlphaCanvas::H);
    canvas.setTransform (AffineTransform::scale ((float) getWidth() / (float) AlphaCanvas::W));
}
