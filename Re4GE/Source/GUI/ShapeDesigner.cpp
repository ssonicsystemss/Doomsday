#include "ShapeDesigner.h"

using namespace juce;
using namespace doomsday::flat;
namespace mod = doomsday::mod;

Colour ShapeDesigner::laneColour (int i)
{
    static const Colour c[4] { Colour (0xff4ea1ff), Colour (0xffe8eaed), Colour (0xffff3b3b), Colour (0xffffc04d) };
    return c[jlimit (0, 3, i)];
}

// the lane's window on the step's time line (like ShapeLanes::laneX in the engine)
ShapeDesigner::Window ShapeDesigner::window (int i) const
{
    const auto p = proc.gather();
    const double stepMs = p.stepBeats * 60000.0 / jmax (20.f, proc.getEngine().tempoNow.load());
    const auto tm = re::times (p, stepMs);
    Window w;
    w.ref = p.gate ? jmax (1.0, tm.gateEnd) : stepMs;
    const auto& l = p.lane[(size_t) i];
    w.len = jmax (1.0, l.lenPct * 0.01 * w.ref);
    w.start = l.anchor == 1 ? tm.gateEnd - w.len : 0.0;
    return w;
}
Point<float> ShapeDesigner::toScreen (const mod::Shape::Pt& pt, const Window& w) const
{
    const auto r = plot();
    const double ms = w.start + pt.ms * 0.001 * w.len;
    return { r.getX() + r.getWidth() * (float) (ms / w.ref), r.getBottom() - r.getHeight() * pt.v };
}
mod::Shape::Pt ShapeDesigner::fromScreen (Point<float> s, const Window& w) const
{
    const auto r = plot();
    const double ms = (s.x - r.getX()) / r.getWidth() * w.ref;
    return { (float) jlimit (0.0, 1000.0, (ms - w.start) / w.len * 1000.0), jlimit (0.f, 1.f, (r.getBottom() - s.y) / r.getHeight()), 0.f };
}
int ShapeDesigner::pointAt (Point<float> s) const
{
    const auto w = window (lane);
    const auto& sh = cur[lane];
    for (int i = 0; i < sh.n; ++i) if (toScreen (sh.pts[(size_t) i], w).getDistanceFrom (s) < 9.f) return i;
    return -1;
}

void ShapeDesigner::mouseDown (const MouseEvent& e)
{
    for (int i = 0; i < 4; ++i) if (tab (i).contains (e.position)) { lane = i; drag = -1; if (onLaneChanged) onLaneChanged(); repaint(); return; }
    if (! plot().expanded (8.f).contains (e.position)) return;
    proc.undo.beginNewTransaction();
    const int hit = pointAt (e.position);
    bending = hit >= 0 && (e.mods.isAltDown() || e.mods.isRightButtonDown());
    if (bending) { drag = hit; bendStartY = e.position.y; bendStart = cur[lane].pts[(size_t) hit].bend; return; }
    if (hit >= 0) { drag = hit; return; }
    auto s = cur[lane];
    const auto pt = fromScreen (e.position, window (lane));
    if (! s.insert (pt.ms, pt.v)) return;
    for (int i = 0; i < s.n; ++i) if (s.pts[(size_t) i].ms == pt.ms && s.pts[(size_t) i].v == pt.v) drag = i;
    cur[lane] = s;
    proc.setShape (lane, s);
    repaint();
}
void ShapeDesigner::mouseDrag (const MouseEvent& e)
{
    if (drag < 0) return;
    auto s = cur[lane];
    auto& pt = s.pts[(size_t) drag];
    if (bending) pt.bend = jlimit (-1.f, 1.f, bendStart + (bendStartY - e.position.y) / 120.f);
    else
    {
        const auto np = fromScreen (e.position, window (lane));
        const float lo = drag > 0 ? s.pts[(size_t) drag - 1].ms : 0.f, hi = drag + 1 < s.n ? s.pts[(size_t) drag + 1].ms : 1000.f;
        pt.ms = drag == 0 ? 0.f : jlimit (lo, hi, np.ms);           // keeps the order; the first point stays at the start
        pt.v = np.v;
    }
    cur[lane] = s;
    proc.setShape (lane, s);
    repaint();
}
void ShapeDesigner::mouseDoubleClick (const MouseEvent& e)
{
    const int hit = pointAt (e.position);
    if (hit <= 0) return;
    proc.undo.beginNewTransaction();
    auto s = cur[lane]; s.remove (hit); cur[lane] = s;
    proc.setShape (lane, s);
    drag = -1; repaint();
}

void ShapeDesigner::tick()
{
    const auto p = proc.gather();
    float sig = proc.getEngine().tempoNow.load() * 0.001f + (float) p.stepBeats + p.gatePct * 0.01f + p.gateMs * 0.0001f + (p.gate ? 7.f : 0.f);
    for (auto& l : p.lane) sig += l.lenPct * 0.013f + (float) l.anchor * 3.f + (l.loop ? 5.f : 0.f) + (l.on ? 11.f : 0.f) + l.depth * 0.17f;
    if (proc.getShapeSerial() != lastSerial)
    {
        lastSerial = proc.getShapeSerial();
        for (int i = 0; i < 4; ++i) cur[i] = proc.getShape (i);
        repaint();
    }
    else if (std::abs (sig - lastSig) > 1e-5f) repaint();
    lastSig = sig;
}

void ShapeDesigner::paint (Graphics& g)
{
    auto& lf = lnfOf (*this);
    const auto& tk = tokens();
    const auto p = proc.gather();
    auto r = getLocalBounds().toFloat();
    g.setColour (tk.surface); g.fillRoundedRectangle (r, 12.f);
    const auto pl = plot();
    // tabs
    for (int i = 0; i < 4; ++i)
    {
        const auto t = tab (i);
        const bool on = p.lane[(size_t) i].on, act = i == lane;
        g.setColour (act ? tk.control : tk.surface); g.fillRoundedRectangle (t, 7.f);
        g.setColour (act ? laneColour (i) : tk.border); g.drawRoundedRectangle (t.reduced (0.5f), 7.f, 1.f);
        g.setColour (on ? laneColour (i) : tk.border); g.fillEllipse (t.getX() + 10.f, t.getCentreY() - 3.5f, 7.f, 7.f);
        g.setFont (lf.font (11.5f, act ? Bold : Medium, 0.08f)); g.setColour (act ? tk.text : tk.textDim);
        g.drawText (ParamIDs::laneNames[i], t.withTrimmedLeft (22.f), Justification::centredLeft);
    }
    g.setFont (lf.font (10.5f, Medium)); g.setColour (tk.textMute);
    g.drawText ("click = point   drag = move   Alt / right-drag = bend   double-click = delete", r.reduced (16.f, 10.f).withHeight (26.f), Justification::centredRight);
    // grid: one step, the gate end
    const auto w0 = window (lane);
    for (int k = 0; k <= 8; ++k) { g.setColour (tk.divider); g.drawVerticalLine ((int) (pl.getX() + pl.getWidth() * k / 8.f), pl.getY(), pl.getBottom()); }
    for (int k = 0; k <= 4; ++k) { g.setColour (tk.divider); g.drawHorizontalLine ((int) (pl.getY() + pl.getHeight() * k / 4.f), pl.getX(), pl.getRight()); }
    const double stepMs = p.stepBeats * 60000.0 / jmax (20.f, proc.getEngine().tempoNow.load());
    const auto tm = re::times (p, stepMs);
    const float gx = pl.getX() + pl.getWidth() * (float) jlimit (0.0, 1.0, tm.gateEnd / w0.ref);
    g.setColour (Colour (0xffff3b3b).withAlpha (0.8f)); g.fillRect (gx - 1.f, pl.getY(), 2.f, pl.getHeight());
    g.setFont (lf.font (10.f, Medium)); g.setColour (tk.textMute);
    g.drawText (String (roundToInt (w0.ref)) + " ms" + (p.gate ? "  (gate)" : "  (one step)"), pl.withY (pl.getBottom() + 2.f).withHeight (14.f), Justification::centredRight);
    if (lane == 1) { g.setColour (tk.textMute.withAlpha (0.5f)); g.drawHorizontalLine ((int) pl.getCentreY(), pl.getX(), pl.getRight()); }
    // the active lane's window
    {
        const float x0 = pl.getX() + pl.getWidth() * (float) (w0.start / w0.ref), x1 = pl.getX() + pl.getWidth() * (float) ((w0.start + w0.len) / w0.ref);
        g.setColour (laneColour (lane).withAlpha (0.05f)); g.fillRect (Rectangle<float> (x0, pl.getY(), x1 - x0, pl.getHeight()).getIntersection (pl));
    }
    // curves: the others faint, the active one bright with its points
    auto pathOf = [&] (int i)
    {
        const auto w = window (i);
        const auto& sh = cur[i];
        Path path; bool started = false;
        const int cols = (int) pl.getWidth();
        int seg = 0;
        for (int c = 0; c <= cols; ++c)
        {
            const double ms = w.ref * c / cols;
            const double x = re::ShapeLanes::laneX (p.lane[(size_t) i], ms, tm.gateEnd, w.ref);
            const float v = sh.eval ((float) (1000.0 * x), seg);
            const float px = pl.getX() + (float) c, py = pl.getBottom() - pl.getHeight() * v;
            if (! started) { path.startNewSubPath (px, py); started = true; } else path.lineTo (px, py);
        }
        return path;
    };
    for (int i = 0; i < 4; ++i) if (i != lane) { g.setColour (laneColour (i).withAlpha (p.lane[(size_t) i].on ? 0.35f : 0.12f)); g.strokePath (pathOf (i), PathStrokeType (1.2f)); }
    {
        auto path = pathOf (lane);
        g.setColour (laneColour (lane).withAlpha (p.lane[(size_t) lane].on ? 1.f : 0.45f)); g.strokePath (path, PathStrokeType (2.f));
        const auto& sh = cur[lane];
        for (int i = 0; i < sh.n; ++i)
        {
            const auto s = toScreen (sh.pts[(size_t) i], w0);
            const float rad = (i == hover || i == drag) ? 6.5f : 5.f;
            g.setColour (tk.ground); g.fillEllipse (s.x - rad, s.y - rad, 2 * rad, 2 * rad);
            g.setColour (Colours::white); g.drawEllipse (s.x - rad, s.y - rad, 2 * rad, 2 * rad, 1.8f);
        }
        if (! p.lane[(size_t) lane].on)
        {
            g.setFont (lf.font (12.f, SemiBold)); g.setColour (tk.textMute);
            g.drawText ("this lane is OFF - switch it ON below to hear it", pl.reduced (8.f).withHeight (20.f), Justification::topLeft);
        }
    }
}
