#include "PianoRoll.h"
#include "DoomsdayFlat.h"

using namespace juce;
using tune::melody::Note;

PianoRoll::PianoRoll (TuneProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    leadColour = [] { return Colour (0xffa66bff); };
    bounceColour = [] { return Colour (0xffb6f23e); };
    accent = [] { return Colour (0xffc8ff3c); };
    reload();
    startTimerHz (30);
}

void PianoRoll::reload()
{
    // the selection follows the notes (the processor returns them sorted, the indices change)
    std::vector<Note> picked;
    for (int i : sel) if (i < (int) notes.size()) picked.push_back (notes[(size_t) i]);
    notes = proc.melodyNotes();
    lengthBeats = proc.melodyLengthBeats();
    version = proc.melodyVersion.load();
    sel.clear();
    for (auto& q : picked)
        for (int i = 0; i < (int) notes.size(); ++i)
            if (! sel.count (i) && notes[(size_t) i].pitch == q.pitch && std::abs (notes[(size_t) i].start - q.start) < 1e-6) { sel.insert (i); break; }
    // keep the melody in view: centre the rows on the notes the first time
    if (! notes.empty() && lowPitch == 52)
    {
        int lo = 127, hi = 0; for (auto& n : notes) { lo = jmin (lo, n.pitch); hi = jmax (hi, n.pitch); }
        lowPitch = jlimit (0, 127 - rows, (lo + hi) / 2 - rows / 2);
    }
    repaint();
}

void PianoRoll::timerCallback()
{
    if (proc.melodyVersion.load() != version && drag == None) reload();
    if (proc.previewPlaying()) repaint();
}

Rectangle<float> PianoRoll::rollArea() const { return getLocalBounds().toFloat().withTrimmedLeft (46.f).withTrimmedBottom (78.f); }
Rectangle<float> PianoRoll::velArea() const  { return getLocalBounds().toFloat().withTrimmedLeft (46.f).withTop ((float) getHeight() - 70.f); }
float PianoRoll::beatToX (double b) const { const auto r = rollArea(); return r.getX() + (float) (b / jmax (1.0, lengthBeats)) * r.getWidth(); }
double PianoRoll::xToBeat (float x) const { const auto r = rollArea(); return (double) ((x - r.getX()) / r.getWidth()) * lengthBeats; }
float PianoRoll::pitchToY (int p) const { const auto r = rollArea(); const float h = r.getHeight() / (float) rows; return r.getBottom() - (float) (p - lowPitch + 1) * h; }
int PianoRoll::yToPitch (float y) const { const auto r = rollArea(); const float h = r.getHeight() / (float) rows; return lowPitch + (int) std::floor ((r.getBottom() - y) / h); }
Rectangle<float> PianoRoll::noteRect (const Note& n) const
{
    const float h = rollArea().getHeight() / (float) rows;
    const float x0 = beatToX (n.start), x1 = beatToX (n.start + n.len);
    return { x0, pitchToY (n.pitch) + 1.f, jmax (3.f, x1 - x0 - 1.f), h - 2.f };
}

void PianoRoll::paint (Graphics& g)
{
    auto& lnf = doomsday::flat::lnfOf (*this);
    const auto r = rollArea();
    const float h = r.getHeight() / (float) rows;
    const auto ms = proc.melodySettings();
    // rows: scale notes lit, black keys darker, C rows labelled
    for (int i = 0; i < rows; ++i)
    {
        const int p = lowPitch + i;
        const float y = r.getBottom() - (float) (i + 1) * h;
        const bool black = MidiMessage::isMidiNoteBlack (p);
        const bool scale = tune::melody::inScale (ms, p);
        g.setColour (scale ? Colour (0x22a66bff) : (black ? Colour (0x40000000) : Colour (0x14ffffff)));
        g.fillRect (r.getX(), y, r.getWidth(), h);
        g.setColour (Colour (0x10ffffff)); g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        // keyboard
        g.setColour (black ? Colour (0xff15131a) : Colour (0xffd9d6e0));
        g.fillRect (2.f, y, 40.f, h - 1.f);
        if (p % 12 == 0 || h > 13.f)
        {
            g.setColour (black ? Colour (0xffb8b2c4) : Colour (0xff302a3a));
            g.setFont (lnf.font (jmin (11.f, h * 0.8f), p % 12 == 0 ? doomsday::flat::Bold : doomsday::flat::Medium));
            g.drawText (String (tune::melody::keyName (p)) + String (p / 12 - 1), Rectangle<float> (4.f, y, 36.f, h), Justification::centredRight);
        }
    }
    // beats / bars
    for (double b = 0; b <= lengthBeats + 1e-6; b += grid)
    {
        const bool bar = std::fmod (b, 4.0) < 1e-6, beat = std::fmod (b, 1.0) < 1e-6;
        g.setColour (Colours::white.withAlpha (bar ? 0.28f : beat ? 0.12f : 0.045f));
        g.drawVerticalLine ((int) beatToX (b), r.getY(), getHeight() - 2.f);
    }
    // notes: lead violet, the low bounce lime, selection outlined
    int mid = 0; for (auto& n : notes) mid += n.pitch; mid = notes.empty() ? 60 : mid / (int) notes.size();
    for (size_t i = 0; i < notes.size(); ++i)
    {
        const auto& n = notes[i];
        if (n.pitch < lowPitch || n.pitch >= lowPitch + rows) continue;
        const auto nr = noteRect (n);
        const Colour c = n.pitch < 12 * (ms.octLow + 1) - 6 ? bounceColour() : leadColour();
        g.setColour (c.withAlpha (0.45f + 0.5f * (float) n.vel / 127.f));
        g.fillRoundedRectangle (nr, 3.f);
        if (sel.count ((int) i)) { g.setColour (Colours::white); g.drawRoundedRectangle (nr, 3.f, 1.6f); }
    }
    // velocity lane
    const auto va = velArea();
    g.setColour (Colour (0x26000000)); g.fillRoundedRectangle (va, 6.f);
    for (size_t i = 0; i < notes.size(); ++i)
    {
        const auto& n = notes[i];
        const float x = beatToX (n.start), hh = (va.getHeight() - 8.f) * (float) n.vel / 127.f;
        g.setColour ((sel.count ((int) i) ? Colours::white : leadColour()).withAlpha (0.85f));
        g.fillRect (x, va.getBottom() - 4.f - hh, 3.f, hh);
        g.fillEllipse (x - 2.f, va.getBottom() - 6.f - hh, 7.f, 7.f);
    }
    g.setColour (doomsday::flat::tokens().textMute); g.setFont (lnf.font (10.f, doomsday::flat::Bold, 0.08f));
    g.drawText ("VEL", Rectangle<float> (0.f, va.getY(), 42.f, va.getHeight()), Justification::centred);
    // marquee + playhead
    if (drag == Marquee) { g.setColour (accent().withAlpha (0.18f)); g.fillRect (marquee); g.setColour (accent()); g.drawRect (marquee, 1.f); }
    if (proc.previewPlaying())
    {
        const float x = beatToX (proc.previewPosition());
        g.setColour (accent()); g.fillRect (x - 1.f, r.getY(), 2.f, (float) getHeight() - r.getY());
    }
}

int PianoRoll::hit (Point<float> p, bool& edge) const
{
    edge = false;
    for (int i = (int) notes.size(); --i >= 0;)
    {
        const auto nr = noteRect (notes[(size_t) i]);
        if (nr.expanded (0.f, 1.f).contains (p)) { edge = p.x > nr.getRight() - jmin (7.f, nr.getWidth() * 0.35f); return i; }
    }
    return -1;
}

std::vector<int> PianoRoll::targets() const
{
    std::vector<int> t;
    if (sel.empty()) for (int i = 0; i < (int) notes.size(); ++i) t.push_back (i);
    else t.assign (sel.begin(), sel.end());
    return t;
}

void PianoRoll::commit (bool newTransaction, const String& name)
{
    proc.setMelodyNotes (notes, newTransaction, name);
    version = proc.melodyVersion.load();
    lengthBeats = proc.melodyLengthBeats();
    repaint();
}

void PianoRoll::mouseDown (const MouseEvent& e)
{
    grabKeyboardFocus();
    downPos = e.position;
    dragStart = notes;
    if (velArea().contains (e.position)) { drag = Velocity; proc.undo.beginNewTransaction ("Velocity"); mouseDrag (e); return; }
    bool edge = false;
    const int i = hit (e.position, edge);
    if (e.mods.isPopupMenu())
    {
        if (i >= 0) { notes.erase (notes.begin() + i); sel.clear(); commit (true, "Delete note"); }
        drag = None; return;
    }
    if (i >= 0)
    {
        if (e.mods.isShiftDown()) { if (sel.count (i)) sel.erase (i); else sel.insert (i); }
        else if (! sel.count (i)) { sel.clear(); sel.insert (i); }
        anchor = i;
        drag = edge ? Resize : Move;
        proc.undo.beginNewTransaction (edge ? "Note length" : "Move notes");
        lastLen = notes[(size_t) i].len;
    }
    else
    {
        if (! e.mods.isShiftDown()) sel.clear();
        drag = Marquee; marquee = { e.position, e.position };
    }
    repaint();
}

void PianoRoll::mouseDrag (const MouseEvent& e)
{
    const bool free = e.mods.isAltDown();
    if (drag == Move && anchor >= 0)
    {
        double db = xToBeat (e.position.x) - xToBeat (downPos.x);
        const int dp = yToPitch (e.position.y) - yToPitch (downPos.y);
        const auto& a = dragStart[(size_t) anchor];
        if (! free) db = snap (a.start + db) - a.start;
        for (int i : sel)
        {
            notes[(size_t) i].start = jmax (0.0, dragStart[(size_t) i].start + db);
            notes[(size_t) i].pitch = jlimit (0, 127, dragStart[(size_t) i].pitch + dp);
        }
        commit (false);
    }
    else if (drag == Resize && anchor >= 0)
    {
        const double end = xToBeat (e.position.x);
        for (int i : sel)
        {
            const auto& d = dragStart[(size_t) i];
            const double newLen = (free ? end : snap (end)) - dragStart[(size_t) anchor].start + (d.len - dragStart[(size_t) anchor].len);
            notes[(size_t) i].len = jmax (free ? 1.0 / 64.0 : grid, newLen);
        }
        lastLen = notes[(size_t) anchor].len;
        commit (false);
    }
    else if (drag == Marquee)
    {
        marquee = Rectangle<float> (downPos, e.position);
        for (int i = 0; i < (int) notes.size(); ++i) if (marquee.intersects (noteRect (notes[(size_t) i]))) sel.insert (i);
        repaint();
    }
    else if (drag == Velocity)
    {
        const auto va = velArea();
        const int v = jlimit (1, 127, (int) std::round (127.f * (va.getBottom() - 4.f - e.position.y) / (va.getHeight() - 8.f)));
        const double b = xToBeat (e.position.x);
        bool changed = false;
        for (int i = 0; i < (int) notes.size(); ++i)
        {
            if (! sel.empty() && ! sel.count (i)) continue;
            if (std::abs (notes[(size_t) i].start - b) < jmax (grid * 0.5, 0.1)) { notes[(size_t) i].vel = v; changed = true; }
        }
        if (changed) commit (false);
    }
}

void PianoRoll::mouseUp (const MouseEvent&) { drag = None; anchor = -1; reload(); }

void PianoRoll::mouseDoubleClick (const MouseEvent& e)
{
    if (! rollArea().contains (e.position)) return;
    bool edge; if (hit (e.position, edge) >= 0) return;
    Note n; n.start = jmax (0.0, std::floor (xToBeat (e.position.x) / grid) * grid); n.len = lastLen; n.pitch = yToPitch (e.position.y); n.vel = 100;
    notes.push_back (n);
    sel.clear(); sel.insert ((int) notes.size() - 1);
    commit (true, "Add note");
}

void PianoRoll::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    lowPitch = jlimit (0, 127 - rows, lowPitch + (w.deltaY > 0 ? 2 : -2));
    repaint();
}

bool PianoRoll::keyPressed (const KeyPress& k)
{
    const bool cmd = k.getModifiers().isCommandDown();
    if (k == KeyPress::deleteKey || k == KeyPress::backspaceKey) { deleteSel(); return true; }
    if (cmd && k.getKeyCode() == 'A') { selectAll(); return true; }
    if (cmd && k.getKeyCode() == 'C') { copySel(); return true; }
    if (cmd && k.getKeyCode() == 'V') { paste(); return true; }
    if (cmd && k.getKeyCode() == 'D') { duplicateSel(); return true; }
    if (cmd && k.getKeyCode() == 'Z') { proc.undo.undo(); reload(); return true; }
    if (cmd && k.getKeyCode() == 'Y') { proc.undo.redo(); reload(); return true; }
    if (k.getKeyCode() == 'Q') { quantizeSel(); return true; }
    if (k.getKeyCode() == KeyPress::upKey)   { transposeSel (k.getModifiers().isShiftDown() ? 12 : 1); return true; }
    if (k.getKeyCode() == KeyPress::downKey) { transposeSel (k.getModifiers().isShiftDown() ? -12 : -1); return true; }
    return false;
}

// ---------------------------------------------------------------- toolbar actions
void PianoRoll::quantizeSel()
{
    for (int i : targets())
    {
        auto& n = notes[(size_t) i];
        n.start = snap (n.start); n.len = jmax (grid, snap (n.len));
    }
    commit (true, "Quantize");
}
void PianoRoll::humanizeSel()
{
    std::vector<Note> sub; auto t = targets();
    for (int i : t) sub.push_back (notes[(size_t) i]);
    tune::melody::humanize (sub, (uint32_t) Time::getMillisecondCounter(), 0.015, 14);
    for (size_t k = 0; k < t.size(); ++k) notes[(size_t) t[k]] = sub[k];
    commit (true, "Humanize");
}
void PianoRoll::transposeSel (int semis)
{
    for (int i : targets()) notes[(size_t) i].pitch = jlimit (0, 127, notes[(size_t) i].pitch + semis);
    commit (true, semis % 12 == 0 ? "Octave" : "Transpose");
}
void PianoRoll::duplicateSel()
{
    auto t = targets(); if (t.empty()) return;
    double a = 1e9, b = 0;
    for (int i : t) { a = jmin (a, notes[(size_t) i].start); b = jmax (b, notes[(size_t) i].start + notes[(size_t) i].len); }
    const double span = jmax (grid, std::ceil ((b - a) / 4.0 - 1e-6) * 4.0);     // whole bars
    std::set<int> added;
    for (int i : t) { auto n = notes[(size_t) i]; n.start += span; notes.push_back (n); added.insert ((int) notes.size() - 1); }
    sel = added;
    commit (true, "Duplicate");
}
void PianoRoll::deleteSel()
{
    if (sel.empty()) return;
    std::vector<Note> keep;
    for (int i = 0; i < (int) notes.size(); ++i) if (! sel.count (i)) keep.push_back (notes[(size_t) i]);
    notes = keep; sel.clear();
    commit (true, "Delete notes");
}
void PianoRoll::clearAll() { notes.clear(); sel.clear(); commit (true, "Clear melody"); }
void PianoRoll::copySel() { clipboard.clear(); for (int i : targets()) clipboard.push_back (notes[(size_t) i]); }
void PianoRoll::paste()
{
    if (clipboard.empty()) return;
    double a = 1e9; for (auto& n : clipboard) a = jmin (a, n.start);
    double at = 0; for (int i : targets()) at = jmax (at, notes[(size_t) i].start + notes[(size_t) i].len);
    at = std::ceil (at / 4.0 - 1e-6) * 4.0;
    sel.clear();
    for (auto n : clipboard) { n.start = n.start - a + at; notes.push_back (n); sel.insert ((int) notes.size() - 1); }
    commit (true, "Paste");
}
void PianoRoll::selectAll() { sel.clear(); for (int i = 0; i < (int) notes.size(); ++i) sel.insert (i); repaint(); }
