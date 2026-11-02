#include "SeqView.h"

using namespace juce;
using namespace doomsday::flat;

Rectangle<float> SeqView::column (int i) const
{
    auto a = getLocalBounds().toFloat().reduced (16.f, 12.f);
    const float w = a.getWidth() / 16.f;
    return { a.getX() + i * w + 3.f, a.getY(), w - 6.f, a.getHeight() };
}
SeqView::Hit SeqView::hitAt (Point<float> p) const
{
    for (int i = 0; i < 16; ++i)
    {
        if (onArea (i).contains (p)) return { i, ZOn };
        if (velArea (i).expanded (0.f, 3.f).contains (p)) return { i, ZVel };
        if (pitchArea (i).contains (p)) return { i, ZPitch };
    }
    return {};
}
float SeqView::value (const char* id) const { auto* a = proc.apvts.getParameter (id); return a != nullptr ? a->convertFrom0to1 (a->getValue()) : 0.f; }
void SeqView::setNorm (const char* id, float v01)
{
    if (auto* a = proc.apvts.getParameter (id))
    {
        if (gesture == nullptr) a->beginChangeGesture();
        a->setValueNotifyingHost (jlimit (0.f, 1.f, v01));
        if (gesture == nullptr) a->endChangeGesture();
    }
    repaint();
}

void SeqView::paint (Graphics& g)
{
    const auto& t = tokens();
    auto& lnf = lnfOf (*this);
    const int len = roundToInt (value (ParamIDs::seqLen));
    const bool on = value (ParamIDs::seqOn) > 0.5f;
    const int now = on ? proc.getEngine().seqStepNow() : -1;
    const Colour acc = t.cool;
    for (int i = 0; i < 16; ++i)
    {
        const bool inLen = i < len, stepOn = value (ParamIDs::seqId (i, "on")) > 0.5f;
        const float alpha = (inLen ? 1.f : 0.3f) * (on ? 1.f : 0.45f);   // SEQ OFF: the grid is dimmed (the trigger STEPS play)
        // ON cell (beats: every 4th step a little brighter)
        auto o = onArea (i);
        g.setColour ((stepOn ? acc.withAlpha (0.85f) : (i % 4 == 0 ? t.control.brighter (0.12f) : t.control)).withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (o, 5.f);
        if (i == now) { g.setColour (t.hot); g.drawRoundedRectangle (o.reduced (0.5f), 5.f, 2.f); }
        g.setFont (lnf.font (10.5f, SemiBold)); g.setColour ((stepOn ? Colours::white : t.textMute).withMultipliedAlpha (alpha));
        g.drawText (String (i + 1), o, Justification::centred);
        // LEVEL bar
        auto v = velArea (i);
        g.setColour (t.control.withMultipliedAlpha (alpha)); g.fillRoundedRectangle (v, 4.f);
        const float lvl = value (ParamIDs::seqId (i, "vel"));
        if (stepOn)
        {
            g.setColour ((i == now ? t.hot : acc).withAlpha (0.75f * alpha));
            g.fillRoundedRectangle (v.withTrimmedTop (v.getHeight() * (1.f - lvl)), 4.f);
        }
        // PITCH cell
        auto pc = pitchArea (i);
        const int st = roundToInt (value (ParamIDs::seqId (i, "pitch")));
        g.setColour ((st != 0 ? t.hot.withAlpha (0.18f) : t.control).withMultipliedAlpha (alpha)); g.fillRoundedRectangle (pc, 5.f);
        g.setFont (lnf.font (11.f, SemiBold)); g.setColour ((st != 0 ? t.text : t.textMute).withMultipliedAlpha (alpha));
        g.drawText (st == 0 ? String ("0") : (st > 0 ? "+" : "") + String (st), pc, Justification::centred);
    }
}

void SeqView::mouseDown (const MouseEvent& e)
{
    drag = hitAt (e.position);
    if (drag.step < 0) return;
    const int i = drag.step;
    if (drag.z == ZOn) { setNorm (ParamIDs::seqId (i, "on"), value (ParamIDs::seqId (i, "on")) > 0.5f ? 0.f : 1.f); drag = {}; return; }
    gesture = proc.apvts.getParameter (drag.z == ZVel ? ParamIDs::seqId (i, "vel") : ParamIDs::seqId (i, "pitch"));
    if (gesture != nullptr) gesture->beginChangeGesture();
    dragStartPitch = value (ParamIDs::seqId (i, "pitch")); dragStartY = e.position.y;
    mouseDrag (e);
}
void SeqView::mouseDrag (const MouseEvent& e)
{
    if (drag.step < 0 || gesture == nullptr) return;
    const int i = drag.step;
    if (drag.z == ZVel)
    {
        auto v = velArea (i);
        setNorm (ParamIDs::seqId (i, "vel"), 1.f - (e.position.y - v.getY()) / v.getHeight());
    }
    else if (drag.z == ZPitch)
    {
        const float st = jlimit (-12.f, 12.f, (float) std::round (dragStartPitch + (dragStartY - e.position.y) / 8.f));
        setNorm (ParamIDs::seqId (i, "pitch"), gesture->convertTo0to1 (st));
    }
}
void SeqView::mouseUp (const MouseEvent&) { if (gesture != nullptr) gesture->endChangeGesture(); gesture = nullptr; drag = {}; }
void SeqView::mouseDoubleClick (const MouseEvent& e)
{
    const auto h = hitAt (e.position);
    if (h.step < 0) return;
    if (h.z == ZVel) setNorm (ParamIDs::seqId (h.step, "vel"), 1.f);
    else if (h.z == ZPitch) if (auto* a = proc.apvts.getParameter (ParamIDs::seqId (h.step, "pitch"))) setNorm (ParamIDs::seqId (h.step, "pitch"), a->convertTo0to1 (0.f));
}
void SeqView::tick()
{
    const int now = proc.getEngine().seqStepNow();
    if (now != lastNow) { lastNow = now; repaint(); }
}
