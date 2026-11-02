#include "MorphBar.h"

using namespace juce;
using namespace doomsday::flat;

std::array<Rectangle<float>, 5> MorphBar::zones() const
{
    auto a = getLocalBounds().toFloat().reduced (0.f, 2.f);
    std::array<Rectangle<float>, 5> z;
    z[0] = a.removeFromLeft (30.f); a.removeFromLeft (4.f);
    z[1] = a.removeFromLeft (30.f); a.removeFromLeft (6.f);
    z[2] = a.removeFromLeft (52.f); a.removeFromLeft (10.f);
    z[3] = a.removeFromLeft (70.f); a.removeFromLeft (10.f);
    z[4] = a;
    return z;
}
int MorphBar::hit (Point<float> p) const { auto z = zones(); for (int i = 0; i < 5; ++i) if (z[(size_t) i].contains (p)) return i; return -1; }

void MorphBar::paint (Graphics& g)
{
    const auto& t = tokens();
    auto& lnf = lnfOf (*this);
    const auto z = zones();
    const int active = proc.morphActive();
    const bool on = proc.apvts.getRawParameterValue (ParamIDs::morphOn)->load() > 0.5f;
    const float amt = proc.apvts.getRawParameterValue (ParamIDs::morph)->load();
    auto button = [&] (Rectangle<float> r, const String& txt, bool lit, bool hov, bool dim)
    {
        g.setColour (lit ? t.hot.withAlpha (0.2f) : hov ? t.control.brighter (0.08f) : t.control); g.fillRoundedRectangle (r, 6.f);
        g.setColour (lit ? t.hot : t.border); g.drawRoundedRectangle (r.reduced (0.5f), 6.f, 1.f);
        g.setFont (lnf.font (12.f, Bold)); g.setColour (lit ? t.text : dim ? t.textMute.withAlpha (0.6f) : t.textDim);
        g.drawText (txt, r, Justification::centred);
    };
    button (z[0], "A", active == 0, hover == 0, ! proc.morphHas (0));
    button (z[1], "B", active == 1, hover == 1, ! proc.morphHas (1));
    button (z[2], active == 0 ? "A>B" : "B>A", false, hover == 2, false);
    button (z[3], "MORPH", on, hover == 3, false);
    // the bar: A ... B
    auto r = z[4].withSizeKeepingCentre (z[4].getWidth(), 6.f);
    g.setColour (t.control); g.fillRoundedRectangle (r, 3.f);
    g.setColour ((on ? t.hot : t.textMute).withAlpha (on ? 1.f : 0.5f)); g.fillRoundedRectangle (r.withWidth (r.getWidth() * amt), 3.f);
    g.fillEllipse (Rectangle<float> (12.f, 12.f).withCentre ({ r.getX() + r.getWidth() * amt, r.getCentreY() }));
    g.setFont (lnf.font (10.f, Bold)); g.setColour (t.textMute);
    g.drawText ("A", z[4].withWidth (10.f).translated (-12.f, -10.f), Justification::centred);
    g.drawText ("B", z[4].withX (z[4].getRight() + 2.f).withWidth (10.f).translated (0.f, -10.f), Justification::centred);
}

void MorphBar::mouseDown (const MouseEvent& e)
{
    const int c = hit (e.position);
    dragBar = c == 4;
    if (c == 0 || c == 1) proc.morphSelect (c);
    else if (c == 2) proc.morphCopy();
    else if (c == 3)
    {
        if (auto* a = proc.apvts.getParameter (ParamIDs::morphOn))
        {
            if (! proc.morphHas (0) || ! proc.morphHas (1)) proc.morphCopy();          // first use: both slots exist (= the current sound)
            a->beginChangeGesture(); a->setValueNotifyingHost (a->getValue() > 0.5f ? 0.f : 1.f); a->endChangeGesture();
        }
    }
    else if (dragBar) { if (auto* a = proc.apvts.getParameter (ParamIDs::morph)) a->beginChangeGesture(); mouseDrag (e); }
    repaint();
}
void MorphBar::mouseDrag (const MouseEvent& e)
{
    if (! dragBar) return;
    const auto r = zones()[4];
    if (auto* a = proc.apvts.getParameter (ParamIDs::morph)) a->setValueNotifyingHost (jlimit (0.f, 1.f, (e.position.x - r.getX()) / r.getWidth()));
    repaint();
}
void MorphBar::mouseUp (const MouseEvent&) { if (dragBar) if (auto* a = proc.apvts.getParameter (ParamIDs::morph)) a->endChangeGesture(); dragBar = false; }
void MorphBar::mouseMove (const MouseEvent& e)
{
    const int c = hit (e.position);
    if (c == hover) return;
    hover = c;
    static const char* tips[5] { "Sound A: click = edit A (empty = takes the current sound)", "Sound B: click = edit B (empty = takes the current sound)",
                                 "Copy the sound you edit into the other slot", "MORPH ON: the bass plays between A and B (the bar, automatable in FL)",
                                 "MORPH: 0 % = A, 100 % = B (Hz / ms knobs move musically, switches change halfway)" };
    setTooltip (c >= 0 ? tips[c] : "");
    repaint();
}
void MorphBar::tick()
{
    if (++syncCount % 3 == 0 && (proc.morphHas (0) || proc.morphHas (1))) proc.morphSync();     // ~10 x a second
    const float amt = proc.apvts.getRawParameterValue (ParamIDs::morph)->load(), on = proc.apvts.getRawParameterValue (ParamIDs::morphOn)->load();
    if (amt != lastAmt || on != lastOn || proc.morphActive() != lastActive) { lastAmt = amt; lastOn = on; lastActive = proc.morphActive(); repaint(); }
}
