#include "EqView.h"

using namespace juce;
using namespace doomsday::flat;

namespace { constexpr double fMin = 20.0, fMax = 20000.0; constexpr float dbRange = 18.f; }

float EqView::xOf (double hz) const { auto r = plot(); return r.getX() + r.getWidth() * (float) (std::log (hz / fMin) / std::log (fMax / fMin)); }
double EqView::hzOf (float x) const { auto r = plot(); return fMin * std::pow (fMax / fMin, (double) jlimit (0.f, 1.f, (x - r.getX()) / r.getWidth())); }
float EqView::yOf (float db) const { auto r = plot(); return r.getCentreY() - jlimit (-1.2f, 1.2f, db / dbRange) * r.getHeight() * 0.5f; }
float EqView::dbOf (float y) const { auto r = plot(); return jlimit (-dbRange, dbRange, (r.getCentreY() - y) / (r.getHeight() * 0.5f) * dbRange); }
float EqView::val (int b, const char* what) const { return proc.apvts.getRawParameterValue (ParamIDs::eqId (b, what))->load(); }
void EqView::set (int b, const char* what, float v)
{
    if (auto* a = proc.apvts.getParameter (ParamIDs::eqId (b, what)))
    {
        const bool inGesture = dragBand >= 0;
        if (! inGesture) a->beginChangeGesture();
        a->setValueNotifyingHost (a->convertTo0to1 (v));
        if (! inGesture) a->endChangeGesture();
    }
    repaint();
}
bool EqView::hasGain (int b) const { const int t = roundToInt (val (b, "type")); return t == re::EqBell || t == re::EqLowShelf || t == re::EqHighShelf; }
Point<float> EqView::node (int b) const { return { xOf (val (b, "freq")), hasGain (b) ? yOf (val (b, "gain")) : yOf (0.f) }; }
int EqView::bandAt (Point<float> p) const
{
    int best = -1; float bd = 14.f;
    for (int b = 0; b < 8; ++b) { const float d = node (b).getDistanceFrom (p); if (d < bd) { bd = d; best = b; } }
    return best;
}

void EqView::paint (Graphics& g)
{
    const auto& t = tokens();
    auto& lnf = lnfOf (*this);
    const auto r = plot();
    // grid: frequencies + dB
    g.setFont (lnf.font (10.f, Medium));
    for (double hz : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = xOf (hz);
        g.setColour (t.divider); g.drawVerticalLine (roundToInt (x), r.getY(), r.getBottom());
        g.setColour (t.textMute); g.drawText (hz >= 1000.0 ? String (hz / 1000.0, 0) + "k" : String ((int) hz), Rectangle<float> (x - 20.f, r.getBottom() + 2.f, 40.f, 12.f), Justification::centred);
    }
    for (float db : { -12.f, -6.f, 0.f, 6.f, 12.f })
    {
        g.setColour (db == 0.f ? t.border : t.divider); g.drawHorizontalLine (roundToInt (yOf (db)), r.getX(), r.getRight());
        g.setColour (t.textMute); g.drawText ((db > 0 ? "+" : "") + String ((int) db), Rectangle<float> (r.getX() - 40.f, yOf (db) - 6.f, 34.f, 12.f), Justification::centredRight);
    }
    // the curve (the engine's own maths at 48 kHz)
    const auto prm = proc.gather();
    Path curve; bool first = true;
    for (float x = r.getX(); x <= r.getRight(); x += 2.f)
    {
        const float y = yOf (re::EqCoefs::responseDb (prm, hzOf (x), 48000.0));
        if (first) { curve.startNewSubPath (x, y); first = false; } else curve.lineTo (x, y);
    }
    Path fill (curve); fill.lineTo (r.getRight(), yOf (0.f)); fill.lineTo (r.getX(), yOf (0.f)); fill.closeSubPath();
    g.setColour (t.cool.withAlpha (0.10f)); g.fillPath (fill);
    g.setColour (t.cool); g.strokePath (curve, PathStrokeType (2.f));
    // points
    for (int b = 0; b < 8; ++b)
    {
        const auto c = node (b);
        const bool on = val (b, "on") > 0.5f;
        g.setColour (on ? t.cool : t.control); g.fillEllipse (Rectangle<float> (16.f, 16.f).withCentre (c));
        g.setColour (b == hover ? t.text : on ? t.cool.brighter (0.4f) : t.border); g.drawEllipse (Rectangle<float> (16.f, 16.f).withCentre (c), 1.5f);
        g.setFont (lnf.font (10.f, Bold)); g.setColour (on ? Colours::white : t.textMute);
        g.drawText (String (b + 1), Rectangle<float> (16.f, 16.f).withCentre (c), Justification::centred);
    }
    if (hover >= 0)
    {
        const int b = hover;
        String txt = String (b + 1) + "  " + Choices::eqTypes[roundToInt (val (b, "type"))] + "  " + String (roundToInt (val (b, "freq"))) + " Hz";
        if (hasGain (b)) txt << "  " << (val (b, "gain") > 0 ? "+" : "") << String (val (b, "gain"), 1) << " dB";
        txt << "  Q " << String (val (b, "q"), 2) << (val (b, "on") > 0.5f ? "" : "  (OFF)");
        g.setFont (lnf.font (12.f, SemiBold)); g.setColour (t.text);
        g.drawText (txt, r.withHeight (18.f).translated (8.f, 4.f), Justification::centredLeft);
    }
}

void EqView::mouseDown (const MouseEvent& e)
{
    const int b = bandAt (e.position);
    if (b < 0) return;
    if (e.mods.isPopupMenu())
    {
        PopupMenu m;
        for (int k = 0; k < Choices::eqTypes.size(); ++k) m.addItem (k + 1, Choices::eqTypes[k], true, roundToInt (val (b, "type")) == k);
        m.addSeparator(); m.addItem (100, val (b, "on") > 0.5f ? "Turn off" : "Turn on");
        SafePointer<EqView> safe (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [safe, b] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            if (r == 100) safe->set (b, "on", safe->val (b, "on") > 0.5f ? 0.f : 1.f);
            else { safe->set (b, "type", (float) (r - 1)); safe->set (b, "on", 1.f); }
        });
        return;
    }
    dragBand = b;
    for (const char* w : { "freq", "gain", "on" }) if (auto* a = proc.apvts.getParameter (ParamIDs::eqId (b, w))) a->beginChangeGesture();
    if (val (b, "on") < 0.5f) set (b, "on", 1.f);
}
void EqView::mouseDrag (const MouseEvent& e)
{
    if (dragBand < 0) return;
    set (dragBand, "freq", (float) jlimit (20.0, 20000.0, hzOf (e.position.x)));
    if (hasGain (dragBand)) set (dragBand, "gain", dbOf (e.position.y));
}
void EqView::mouseUp (const MouseEvent&)
{
    if (dragBand >= 0) for (const char* w : { "freq", "gain", "on" }) if (auto* a = proc.apvts.getParameter (ParamIDs::eqId (dragBand, w))) a->endChangeGesture();
    dragBand = -1;
}
void EqView::mouseDoubleClick (const MouseEvent& e)
{
    const int b = bandAt (e.position);
    if (b >= 0) set (b, "on", val (b, "on") > 0.5f ? 0.f : 1.f);
}
void EqView::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const int b = bandAt (e.position);
    if (b < 0) return;
    set (b, "q", jlimit (0.2f, 10.f, val (b, "q") * std::pow (1.25f, w.deltaY > 0 ? 1.f : -1.f)));
}
void EqView::mouseMove (const MouseEvent& e) { const int b = bandAt (e.position); if (b != hover) { hover = b; repaint(); } }
void EqView::tick()
{
    // repaint when the host / automation / undo moved a band
    std::array<float, 40> now {};
    for (int b = 0; b < 8; ++b) { int k = 0; for (const char* w : { "on", "type", "freq", "gain", "q" }) now[(size_t) (b * 5 + k++)] = val (b, w); }
    if (now != last) { last = now; repaint(); }
}
