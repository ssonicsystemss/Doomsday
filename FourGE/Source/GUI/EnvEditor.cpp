#include "EnvEditor.h"
#include "Palette.h"

using namespace juce;

namespace fge::gui
{
    EnvEditor::EnvEditor()
    {
        setWantsKeyboardFocus (true);
        axis.label = [] (float v) { return String (roundToInt (v * 100.f)) + " %"; };
        axis.toUser = [] (float v) { return v * 100.f; };
        axis.fromUser = [] (float x) { return x / 100.f; };
        axis.unit = "%";
    }

    void EnvEditor::setShape (const env::Shape& s, float lo, float hi)
    {
        if (dragging) return;
        const bool rangeChanged = lo != vMin || hi != vMax;
        vMin = lo; vMax = hi;
        if (rangeChanged) { vLo = vMin; vHi = vMax; }
        if (s.toString() == sh.toString()) return;
        sh = s;
        for (auto it = sel.begin(); it != sel.end();) it = *it >= sh.n ? sel.erase (it) : std::next (it);
        repaint();
    }

    void EnvEditor::fit() { t0 = 0.f; t1 = 1.f; vLo = vMin; vHi = vMax; repaint(); }

    Rectangle<float> EnvEditor::plot() const { return getLocalBounds().toFloat().withTrimmedLeft (58.f).withTrimmedBottom (22.f).reduced (6.f, 8.f); }
    float EnvEditor::xOf (float t) const { auto p = plot(); return p.getX() + (t - t0) / jmax (1.0e-4f, t1 - t0) * p.getWidth(); }
    float EnvEditor::yOf (float v) const { auto p = plot(); return p.getBottom() - (v - vLo) / jmax (1.0e-4f, vHi - vLo) * p.getHeight(); }
    float EnvEditor::tOf (float x) const { auto p = plot(); return t0 + (x - p.getX()) / jmax (1.f, p.getWidth()) * (t1 - t0); }
    float EnvEditor::vOf (float y) const { auto p = plot(); return vLo + (p.getBottom() - y) / jmax (1.f, p.getHeight()) * (vHi - vLo); }

    int EnvEditor::hitPoint (Point<float> m) const
    {
        int best = -1; float bd = 9.f;
        for (int i = 0; i < sh.n; ++i)
        {
            const float d = m.getDistanceFrom ({ xOf (sh.p[(size_t) i].t), yOf (sh.p[(size_t) i].v) });
            if (d < bd) { bd = d; best = i; }
        }
        return best;
    }
    int EnvEditor::segmentAt (float t) const
    {
        for (int i = 0; i < sh.n - 1; ++i) if (t >= sh.p[(size_t) i].t && t <= sh.p[(size_t) i + 1].t) return i;
        return -1;
    }
    // BEZIER handles live inside the segment box
    int EnvEditor::hitHandle (Point<float> m, int& which) const
    {
        for (int i = 0; i < sh.n - 1; ++i)
        {
            const auto& a = sh.p[(size_t) i]; const auto& b = sh.p[(size_t) i + 1];
            if (a.curve != env::Bezier) continue;
            for (int k = 0; k < 2; ++k)
            {
                const float hx = k == 0 ? a.h1x : a.h2x, hy = k == 0 ? a.h1y : a.h2y;
                const Point<float> h (xOf (a.t + hx * (b.t - a.t)), yOf (a.v + hy * (b.v - a.v)));
                if (m.getDistanceFrom (h) < 7.f) { which = k; return i; }
            }
        }
        return -1;
    }
    float EnvEditor::snapT (float t) const { return snap ? std::round (t * 64.f) / 64.f : t; }
    float EnvEditor::snapV (float v) const { const float step = (vMax - vMin) / 48.f; return snap ? vMin + std::round ((v - vMin) / step) * step : v; }

    void EnvEditor::clampAll() { sh.sanitize (vMin, vMax); }
    void EnvEditor::commit() { clampAll(); if (onCommit) onCommit (sh); repaint(); }

    void EnvEditor::addPoint (float t, float v)
    {
        if (sh.n >= env::maxPoints) return;
        t = jlimit (0.f, 1.f, t); v = jlimit (vMin, vMax, v);
        int at = sh.n;
        for (int i = 1; i < sh.n; ++i) if (sh.p[(size_t) i].t > t) { at = i; break; }
        if (at == 0) at = 1;
        for (int i = sh.n; i > at; --i) sh.p[(size_t) i] = sh.p[(size_t) i - 1];
        env::Point q = sh.p[(size_t) at - 1];                  // the new point keeps the segment's curve type
        q.t = t; q.v = v;
        sh.p[(size_t) at] = q;
        ++sh.n;
        sel.clear(); sel.insert (at);
        commit();
    }
    void EnvEditor::deletePoint (int i)
    {
        if (i <= 0 || i >= sh.n || sh.n <= 2) return;           // the first point stays (t = 0)
        for (int k = i; k < sh.n - 1; ++k) sh.p[(size_t) k] = sh.p[(size_t) k + 1];
        --sh.n;
        sel.clear();
        commit();
    }

    void EnvEditor::paint (Graphics& g)
    {
        const auto& c = palette();
        g.setColour (c.ground); g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.f);
        const auto p = plot();
        // grid: time in ms, the vertical scale in the slot's unit
        g.setFont (FontOptions (10.f));
        const float msVisible = (t1 - t0) * axis.spanMs;
        float stepMs = 1.f;
        for (float s : { 1.f, 2.f, 5.f, 10.f, 20.f, 25.f, 50.f, 100.f, 200.f, 250.f, 500.f, 1000.f }) { stepMs = s; if (msVisible / s <= 10.f) break; }
        const float msStart = t0 * axis.spanMs;
        for (float ms = std::ceil (msStart / stepMs) * stepMs; ms <= msStart + msVisible + 0.01f; ms += stepMs)
        {
            const float x = xOf (ms / jmax (1.0e-3f, axis.spanMs));
            g.setColour (c.grid); g.drawVerticalLine (roundToInt (x), p.getY(), p.getBottom());
            g.setColour (c.textDim);
            g.drawText (String (roundToInt (ms + axis.offsetMs)) + axis.timeUnit, jmin ((int) x - 30, getWidth() - 62), (int) p.getBottom() + 4, 60, 14, Justification::centred);
        }
        for (int k = 0; k <= 8; ++k)
        {
            const float v = vLo + (vHi - vLo) * (float) k / 8.f;
            const float y = yOf (v);
            g.setColour (std::abs (v) < 1.0e-4f && vMin < 0.f ? c.textDim.withAlpha (0.5f) : c.grid);
            g.drawHorizontalLine (roundToInt (y), p.getX(), p.getRight());
            g.setColour (c.textDim);
            if (axis.label) g.drawText (axis.label (v), 0, (int) y - 7, 58, 14, Justification::centredRight);
        }
        g.setColour (c.control); g.drawRect (p, 1.f);

        // the curve, evaluated with the DSP's own function
        Path curve;
        const int steps = jmax (64, (int) p.getWidth());
        int cur = 0;
        for (int i = 0; i <= steps; ++i)
        {
            const float t = t0 + (t1 - t0) * (float) i / (float) steps;
            const float v = sh.eval (jlimit (0.f, 1.f, t), cur);
            const float x = xOf (t), y = yOf (v);
            if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }
        const auto lineCol = active ? col : col.withSaturation (0.15f).withAlpha (0.6f);
        {
            Path fill (curve);
            fill.lineTo (xOf (t1), yOf (jmax (vLo, jmin (vHi, vMin < 0.f ? 0.f : vMin))));
            fill.lineTo (xOf (t0), yOf (jmax (vLo, jmin (vHi, vMin < 0.f ? 0.f : vMin))));
            fill.closeSubPath();
            g.setColour (lineCol.withAlpha (0.12f)); g.fillPath (fill);
        }
        g.setColour (lineCol); g.strokePath (curve, PathStrokeType (2.f));

        // BEZIER handles
        for (int i = 0; i < sh.n - 1; ++i)
        {
            const auto& a = sh.p[(size_t) i]; const auto& b = sh.p[(size_t) i + 1];
            if (a.curve != env::Bezier) continue;
            const Point<float> pa (xOf (a.t), yOf (a.v)), pb (xOf (b.t), yOf (b.v));
            const Point<float> h1 (xOf (a.t + a.h1x * (b.t - a.t)), yOf (a.v + a.h1y * (b.v - a.v)));
            const Point<float> h2 (xOf (a.t + a.h2x * (b.t - a.t)), yOf (a.v + a.h2y * (b.v - a.v)));
            g.setColour (c.silver.withAlpha (0.35f)); g.drawLine ({ pa, h1 }, 1.f); g.drawLine ({ pb, h2 }, 1.f);
            g.setColour (c.silver); g.fillRect (Rectangle<float> (6.f, 6.f).withCentre (h1)); g.fillRect (Rectangle<float> (6.f, 6.f).withCentre (h2));
        }
        // points
        for (int i = 0; i < sh.n; ++i)
        {
            const Point<float> q (xOf (sh.p[(size_t) i].t), yOf (sh.p[(size_t) i].v));
            if (! p.expanded (6.f).contains (q)) continue;
            const bool s = sel.count (i) > 0;
            const float r = s || i == hover ? 6.f : 4.5f;
            g.setColour (s ? c.silver : lineCol); g.fillEllipse (Rectangle<float> (2.f * r, 2.f * r).withCentre (q));
            g.setColour (c.ground); g.drawEllipse (Rectangle<float> (2.f * r, 2.f * r).withCentre (q), 1.2f);
        }
        if (banding) { g.setColour (c.silver.withAlpha (0.12f)); g.fillRect (band); g.setColour (c.silver.withAlpha (0.5f)); g.drawRect (band, 1.f); }

        // what is being edited / why it is silent
        g.setColour (c.textDim); g.setFont (FontOptions (10.5f));
        String info = String (sh.n) + " / " + String (env::maxPoints) + " points";
        if (hover >= 0 && hover < sh.n)
        {
            const auto& q = sh.p[(size_t) hover];
            info << "   |   " << String (q.t * axis.spanMs + axis.offsetMs, 1) << axis.timeUnit << ", " << (axis.label ? axis.label (q.v) : String (q.v, 2))
                 << ", " << env::curveName (q.curve) << (std::abs (q.tension) > 0.005f ? " " + String (q.tension, 2) : String());
        }
        g.drawText (info, p.reduced (8.f, 4.f).toNearestInt(), Justification::topRight);
        if (! active)
        {
            g.setColour (c.hot.withAlpha (0.9f)); g.setFont (FontOptions (11.5f, Font::bold));
            g.drawText (inactiveText, p.reduced (10.f, 6.f).toNearestInt(), Justification::topLeft);
        }
    }

    void EnvEditor::mouseMove (const MouseEvent& e)
    {
        const int h = hitPoint (e.position);
        if (h != hover) { hover = h; repaint(); }
    }

    void EnvEditor::mouseDown (const MouseEvent& e)
    {
        grabKeyboardFocus();
        downPos = e.position; dragStart = sh;
        dragging = false; banding = false; tensionDrag = false; handleSeg = -1;
        const int h = hitPoint (e.position);
        if (e.mods.isPopupMenu()) { showMenu (h, tOf (e.position.x)); return; }
        int which = 0;
        if (h < 0) { handleSeg = hitHandle (e.position, which); handleWhich = which; if (handleSeg >= 0) { dragging = true; return; } }
        if (h >= 0)
        {
            if (e.mods.isShiftDown()) { if (sel.count (h)) sel.erase (h); else sel.insert (h); }
            else if (! sel.count (h)) { sel.clear(); sel.insert (h); }
            dragging = true;
        }
        else if (e.mods.isAltDown() || e.mods.isCommandDown())
        {
            tensionSeg = segmentAt (tOf (e.position.x));
            if (tensionSeg >= 0) { tensionDrag = true; dragging = true; tensionStart = sh.p[(size_t) tensionSeg].tension; }
        }
        else
        {
            if (! e.mods.isShiftDown()) sel.clear();
            banding = true; band = { e.position, e.position };
        }
        repaint();
    }

    void EnvEditor::mouseDrag (const MouseEvent& e)
    {
        if (banding)
        {
            band = Rectangle<float> (downPos, e.position);
            sel.clear();
            for (int i = 0; i < sh.n; ++i) if (band.contains (xOf (sh.p[(size_t) i].t), yOf (sh.p[(size_t) i].v))) sel.insert (i);
            repaint(); return;
        }
        if (! dragging) return;
        if (handleSeg >= 0)
        {
            auto& a = sh.p[(size_t) handleSeg]; const auto& b = sh.p[(size_t) handleSeg + 1];
            const float w = jmax (1.0e-6f, b.t - a.t), hgt = b.v - a.v;
            const float hx = jlimit (0.f, 1.f, (tOf (e.position.x) - a.t) / w);
            const float hy = std::abs (hgt) > 1.0e-6f ? jlimit (-1.f, 2.f, (vOf (e.position.y) - a.v) / hgt) : 0.f;
            if (handleWhich == 0) { a.h1x = hx; a.h1y = hy; } else { a.h2x = hx; a.h2y = hy; }
            repaint(); return;
        }
        if (tensionDrag)
        {
            sh.p[(size_t) tensionSeg].tension = jlimit (-1.f, 1.f, tensionStart + (downPos.y - e.position.y) / 120.f);
            repaint(); return;
        }
        // move the selection: one delta for every selected point, time order kept, the first point stays at t = 0
        sh = dragStart;
        const float dt = tOf (e.position.x) - tOf (downPos.x), dv = vOf (e.position.y) - vOf (downPos.y);
        for (int i : sel)
        {
            auto& q = sh.p[(size_t) i];
            const auto& o = dragStart.p[(size_t) i];
            q.v = jlimit (vMin, vMax, snapV (o.v + dv));
            if (i == 0) continue;
            const float lo = sel.count (i - 1) ? 0.f : dragStart.p[(size_t) i - 1].t;
            const float hi = i + 1 < sh.n && ! sel.count (i + 1) ? dragStart.p[(size_t) i + 1].t : 1.f;
            q.t = jlimit (lo, hi, snapT (o.t + dt));
        }
        for (int i = 1; i < sh.n; ++i) sh.p[(size_t) i].t = jmax (sh.p[(size_t) i].t, sh.p[(size_t) i - 1].t);
        repaint();
    }

    void EnvEditor::mouseUp (const MouseEvent&)
    {
        if (banding) { banding = false; repaint(); return; }
        if (! dragging) return;
        dragging = false;
        if (sh.toString() != dragStart.toString()) commit();
    }

    void EnvEditor::mouseDoubleClick (const MouseEvent& e)
    {
        const int h = hitPoint (e.position);
        if (h >= 0) deletePoint (h);
        else addPoint (snapT (tOf (e.position.x)), snapV (vOf (e.position.y)));
    }

    void EnvEditor::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
    {
        const float k = w.deltaY > 0.f ? 0.8f : 1.25f;
        if (e.mods.isShiftDown())
        {
            const float c = vOf (e.position.y);
            vLo = jmax (vMin, c - (c - vLo) * k); vHi = jmin (vMax, c + (vHi - c) * k);
            if (vHi - vLo < (vMax - vMin) * 0.02f) { vLo = c - (vMax - vMin) * 0.01f; vHi = c + (vMax - vMin) * 0.01f; }
        }
        else
        {
            const float c = tOf (e.position.x);
            t0 = jmax (0.f, c - (c - t0) * k); t1 = jmin (1.f, c + (t1 - c) * k);
            if (t1 - t0 < 0.01f) { t0 = jmax (0.f, c - 0.005f); t1 = t0 + 0.01f; }
        }
        repaint();
    }

    bool EnvEditor::keyPressed (const KeyPress& k)
    {
        if (k == KeyPress::deleteKey || k == KeyPress::backspaceKey)
        {
            std::vector<int> v (sel.rbegin(), sel.rend());
            sel.clear();
            bool any = false;
            for (int i : v) if (i > 0 && sh.n > 2) { for (int q = i; q < sh.n - 1; ++q) sh.p[(size_t) q] = sh.p[(size_t) q + 1]; --sh.n; any = true; }
            if (any) commit();
            return true;
        }
        if (k == KeyPress ('a', ModifierKeys::commandModifier, 0)) { sel.clear(); for (int i = 0; i < sh.n; ++i) sel.insert (i); repaint(); return true; }
        return false;
    }

    void EnvEditor::showMenu (int point, float t)
    {
        PopupMenu m, curves;
        const int seg = point >= 0 ? jmin (point, sh.n - 2) : segmentAt (t);
        const bool many = point >= 0 && sel.count (point) && sel.size() > 1;
        for (int c = 0; c < env::NumCurves; ++c)
            curves.addItem (100 + c, env::curveName (c), seg >= 0, seg >= 0 && sh.p[(size_t) seg].curve == c);
        m.addSubMenu (many ? "Curve of the selected segments" : "Curve of this segment", curves, seg >= 0);
        m.addItem (3, "Reset tension", seg >= 0);
        m.addSeparator();
        m.addItem (1, "Enter value...", point >= 0);
        m.addItem (2, many ? "Delete selected" : "Delete point", point > 0 && sh.n > 2);
        m.addSeparator();
        m.addItem (4, "Select all");
        m.addItem (5, "Snap to grid", true, snap);
        m.addItem (6, "Fit to view");
        SafePointer<EnvEditor> self (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMousePosition(), [self, point, seg, many] (int r)
        {
            if (self == nullptr || r == 0) return;
            auto& ed = *self;
            if (r >= 100)
            {
                std::vector<int> segs;
                if (many) for (int i : ed.sel) { if (i < ed.sh.n - 1) segs.push_back (i); } else segs.push_back (seg);
                for (int s : segs) ed.sh.p[(size_t) s].curve = (uint8_t) (r - 100);
                ed.commit();
            }
            else if (r == 1) ed.enterValue (point);
            else if (r == 2)
            {
                if (! many) ed.deletePoint (point);
                else { ed.keyPressed (KeyPress (KeyPress::deleteKey)); }
            }
            else if (r == 3) { ed.sh.p[(size_t) seg].tension = 0.f; ed.commit(); }
            else if (r == 4) { ed.sel.clear(); for (int i = 0; i < ed.sh.n; ++i) ed.sel.insert (i); ed.repaint(); }
            else if (r == 5) ed.snap = ! ed.snap;
            else if (r == 6) ed.fit();
        });
    }

    // typing the exact TIME / VALUE of a point (several selected: the value goes to all of them)
    void EnvEditor::enterValue (int point)
    {
        if (point < 0 || point >= sh.n) return;
        auto* w = new AlertWindow ("Point " + String (point + 1), "Exact time and value", MessageBoxIconType::NoIcon, this);
        const auto& q = sh.p[(size_t) point];
        w->addTextEditor ("t", String (q.t * axis.spanMs, 2), "TIME (ms)");
        w->addTextEditor ("v", String (axis.toUser ? axis.toUser (q.v) : q.v, 2), "VALUE (" + axis.unit + ")");
        w->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
        w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        SafePointer<EnvEditor> self (this);
        w->enterModalState (true, ModalCallbackFunction::create ([self, w, point] (int r)
        {
            if (self == nullptr || r != 1) return;
            auto& ed = *self;
            if (point >= ed.sh.n) return;
            const float ms = w->getTextEditorContents ("t").getFloatValue();
            const float x = w->getTextEditorContents ("v").getFloatValue();
            const float v = ed.axis.fromUser ? ed.axis.fromUser (x) : x;
            std::set<int> targets = ed.sel.count (point) ? ed.sel : std::set<int> { point };
            for (int i : targets) ed.sh.p[(size_t) i].v = jlimit (ed.vMin, ed.vMax, v);
            if (point > 0)
            {
                const float lo = ed.sh.p[(size_t) point - 1].t, hi = point + 1 < ed.sh.n ? ed.sh.p[(size_t) point + 1].t : 1.f;
                ed.sh.p[(size_t) point].t = jlimit (lo, hi, ms / jmax (1.0e-3f, ed.axis.spanMs));
            }
            ed.commit();
        }), true);
    }
}
