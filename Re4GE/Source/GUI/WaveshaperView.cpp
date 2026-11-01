#include "WaveshaperView.h"

using namespace juce;
using namespace doomsday::flat;
namespace mod = doomsday::mod;

Rectangle<float> WaveshaperView::plot() const
{
    auto r = getLocalBounds().toFloat().reduced (16.f, 10.f).withTrimmedTop (34.f);
    const float side = jmin (r.getWidth(), r.getHeight());
    return r.withSizeKeepingCentre (side, side).withX (r.getX());
}
Point<float> WaveshaperView::toScreen (const mod::Shape::Pt& pt) const
{
    const auto r = plot();
    return { r.getX() + r.getWidth() * pt.ms / 1000.f, r.getBottom() - r.getHeight() * pt.v };
}
mod::Shape::Pt WaveshaperView::fromScreen (Point<float> s) const
{
    const auto r = plot();
    return { jlimit (0.f, 1000.f, (s.x - r.getX()) / r.getWidth() * 1000.f), jlimit (0.f, 1.f, (r.getBottom() - s.y) / r.getHeight()), 0.f };
}
int WaveshaperView::pointAt (Point<float> s) const
{
    const auto& sh = cur[stage];
    for (int i = 0; i < sh.n; ++i) if (toScreen (sh.pts[(size_t) i]).getDistanceFrom (s) < 9.f) return i;
    return -1;
}
void WaveshaperView::mouseDown (const MouseEvent& e)
{
    for (int i = 0; i < 2; ++i) if (tab (i).contains (e.position)) { stage = i; drag = -1; if (onStageChanged) onStageChanged(); repaint(); return; }
    if (! plot().expanded (8.f).contains (e.position)) return;
    proc.undo.beginNewTransaction();
    const int hit = pointAt (e.position);
    bending = hit >= 0 && (e.mods.isAltDown() || e.mods.isRightButtonDown());
    if (bending) { drag = hit; bendStartY = e.position.y; bendStart = cur[stage].pts[(size_t) hit].bend; return; }
    if (hit >= 0) { drag = hit; return; }
    auto s = cur[stage];
    const auto pt = fromScreen (e.position);
    if (! s.insert (pt.ms, pt.v)) return;
    for (int i = 0; i < s.n; ++i) if (s.pts[(size_t) i].ms == pt.ms && s.pts[(size_t) i].v == pt.v) drag = i;
    cur[stage] = s; proc.setShape (4 + stage, s); repaint();
}
void WaveshaperView::mouseDrag (const MouseEvent& e)
{
    if (drag < 0) return;
    auto s = cur[stage];
    auto& pt = s.pts[(size_t) drag];
    if (bending) pt.bend = jlimit (-1.f, 1.f, bendStart + (bendStartY - e.position.y) / 120.f);
    else
    {
        const auto np = fromScreen (e.position);
        const float lo = drag > 0 ? s.pts[(size_t) drag - 1].ms : 0.f, hi = drag + 1 < s.n ? s.pts[(size_t) drag + 1].ms : 1000.f;
        pt.ms = drag == 0 ? 0.f : jlimit (lo, hi, np.ms);
        pt.v = np.v;
    }
    cur[stage] = s; proc.setShape (4 + stage, s); repaint();
}
void WaveshaperView::mouseDoubleClick (const MouseEvent& e)
{
    const int hit = pointAt (e.position);
    if (hit <= 0) return;
    proc.undo.beginNewTransaction();
    auto s = cur[stage]; s.remove (hit); cur[stage] = s;
    proc.setShape (4 + stage, s); drag = -1; repaint();
}
void WaveshaperView::tick()
{
    if (proc.getShapeSerial() != lastSerial) { lastSerial = proc.getShapeSerial(); for (int i = 0; i < 2; ++i) cur[i] = proc.getShape (4 + i); repaint(); }
    for (int i = 0; i < 2; ++i) { const float pk = proc.getEngine().drivePeak[(size_t) i].exchange (0.f); dot[i] = jmax (pk, dot[i] * 0.8f); }
    repaint();
}
void WaveshaperView::paint (Graphics& g)
{
    auto& lf = lnfOf (*this);
    const auto& tk = tokens();
    const auto hot = Colour (0xffff3b3b);
    g.setColour (tk.surface); g.fillRoundedRectangle (getLocalBounds().toFloat(), 12.f);
    for (int i = 0; i < 2; ++i)
    {
        const auto t = tab (i); const bool act = i == stage;
        g.setColour (act ? tk.control : tk.surface); g.fillRoundedRectangle (t, 7.f);
        g.setColour (act ? hot : tk.border); g.drawRoundedRectangle (t.reduced (0.5f), 7.f, 1.f);
        g.setFont (lf.font (11.5f, act ? Bold : Medium, 0.08f)); g.setColour (act ? tk.text : tk.textDim);
        g.drawText ("STAGE " + String (i + 1), t, Justification::centred);
    }
    const auto pl = plot();
    for (int k = 0; k <= 8; ++k)
    {
        g.setColour (k == 4 ? tk.border : tk.divider);
        g.drawVerticalLine ((int) (pl.getX() + pl.getWidth() * k / 8.f), pl.getY(), pl.getBottom());
        g.drawHorizontalLine ((int) (pl.getY() + pl.getHeight() * k / 8.f), pl.getX(), pl.getRight());
    }
    g.setColour (tk.textMute.withAlpha (0.35f)); g.drawLine (pl.getX(), pl.getBottom(), pl.getRight(), pl.getY(), 1.f);     // straight = clean
    // the curve as the engine uses it (symmetric = the right half mirrored)
    const bool sym = proc.apvts.getRawParameterValue (ParamIDs::labId (stage, "sym"))->load() > 0.5f;
    doomsday::lab::CustomTable t; t.build (cur[stage], sym);
    Path path;
    for (int c = 0; c <= (int) pl.getWidth(); ++c)
    {
        const double x = -1.0 + 2.0 * c / pl.getWidth();
        const float y = pl.getCentreY() - pl.getHeight() * 0.5f * (float) t.eval (x);
        if (c == 0) path.startNewSubPath (pl.getX() + (float) c, y); else path.lineTo (pl.getX() + (float) c, y);
    }
    g.setColour (hot); g.strokePath (path, PathStrokeType (2.f));
    const auto& sh = cur[stage];
    for (int i = 0; i < sh.n; ++i)
    {
        const auto s = toScreen (sh.pts[(size_t) i]);
        if (sym && sh.pts[(size_t) i].ms < 500.f) continue;          // symmetric: only the right half is edited
        g.setColour (tk.ground); g.fillEllipse (s.x - 5.f, s.y - 5.f, 10.f, 10.f);
        g.setColour (Colours::white); g.drawEllipse (s.x - 5.f, s.y - 5.f, 10.f, 10.f, 1.8f);
    }
    // the live dot: how far the signal goes into the curve right now
    const float lv = jlimit (0.f, 1.f, dot[stage]);
    if (lv > 0.001f)
    {
        const float px = pl.getCentreX() + pl.getWidth() * 0.5f * lv, py = pl.getCentreY() - pl.getHeight() * 0.5f * (float) t.eval (lv);
        g.setColour (Colours::white); g.fillEllipse (px - 5.f, py - 5.f, 10.f, 10.f);
        g.setColour (Colours::white.withAlpha (0.25f)); g.drawVerticalLine ((int) px, pl.getY(), pl.getBottom());
    }
    // help text right of the plot
    auto info = getLocalBounds().toFloat().reduced (16.f, 10.f).withTrimmedTop (34.f).withTrimmedLeft (pl.getWidth() + 24.f);
    g.setFont (lf.font (12.f, Medium)); g.setColour (tk.textDim);
    g.drawFittedText ("Input left -> right, output bottom -> top. The faint diagonal = clean.\n\n"
                      "This curve plays when the stage's LAB is CUSTOM (DISTORTION page).\n\n"
                      "The white dot = how far the bass goes into the curve right now (play a note).\n\n"
                      "click = point   drag = move\nAlt / right-drag = bend   double-click = delete\n\n"
                      "SYMMETRIC: draw the right half, the left half mirrors it.",
                      info.toNearestInt(), Justification::topLeft, 14);
}
