#include "RandomPanel.h"

using namespace juce;
using namespace doomsday::flat;
using RZ = re4ge::Randomizer;

namespace
{
    const char* modeTips[] { "Everything moves (inside the musical ranges)", "Only the sources: OSC 1-4, warp, kick tail, unison / width",
                             "Only the pitch movement", "Only the distortion stages (CLASSIC / LAB, drive, bias, hardness)",
                             "Only the pre filter + EQ (the low end is never cut)", "New LFO -> MATRIX routings (1-3 rows)",
                             "Only the reverse itself: envelope (start stays after the kick's tail), pitch, shape lanes" };
    const char* styleTips[] { "Classic reverse bass: moderate drive, pump / swell, few layers", "Modern: more layers, smear, width, lab saturation / diode",
                              "Raw: hard drive, asymmetry, fold / crush, kick tail source", "Experimental: wide ranges, warp, PM, wavefold, matrix" };
}

std::vector<std::pair<Rectangle<float>, int>> RandomPanel::layout() const
{
    std::vector<std::pair<Rectangle<float>, int>> z;
    auto a = getLocalBounds().toFloat().reduced (20.f, 16.f);
    const float label = 110.f, rowH = 34.f, gap = 18.f;
    auto row = [&] { auto r = a.removeFromTop (rowH); a.removeFromTop (gap); return r.withTrimmedLeft (label); };
    {
        auto r = row(); const float w = r.getWidth() / RZ::NumModes;
        for (int i = 0; i < RZ::NumModes; ++i) z.push_back ({ r.removeFromLeft (w).reduced (3.f, 0.f), 100 + i });
    }
    {
        auto r = row().removeFromLeft (4 * 150.f); const float w = r.getWidth() / RZ::NumStyles;
        for (int i = 0; i < RZ::NumStyles; ++i) z.push_back ({ r.removeFromLeft (w).reduced (3.f, 0.f), 200 + i });
    }
    z.push_back ({ row().removeFromLeft (450.f).reduced (3.f, 10.f), 400 });
    {
        auto r = row(); const float w = r.getWidth() / RZ::NumGroups;
        for (int i = 0; i < RZ::NumGroups; ++i) z.push_back ({ r.removeFromLeft (w).reduced (3.f, 0.f), 300 + i });
    }
    a.removeFromTop (4.f);
    z.push_back ({ a.removeFromTop (54.f).withTrimmedLeft (label).withWidth (260.f), 500 });
    return z;
}
int RandomPanel::hit (Point<float> p) const { for (auto& [r, c] : layout()) if (r.expanded (0.f, c == 400 ? 10.f : 0.f).contains (p)) return c; return -1; }

void RandomPanel::paint (Graphics& g)
{
    const auto& t = tokens();
    auto& lnf = lnfOf (*this);
    const auto st = RZ::read (proc.apvts.state);
    const Colour acc = t.hot;
    auto chip = [&] (Rectangle<float> r, const String& text, bool on, bool hov, Colour c)
    {
        g.setColour (on ? c.withAlpha (0.18f) : hov ? t.control.brighter (0.08f) : t.control); g.fillRoundedRectangle (r, 7.f);
        g.setColour (on ? c : t.border); g.drawRoundedRectangle (r.reduced (0.5f), 7.f, 1.f);
        g.setFont (lnf.font (12.f, SemiBold, 0.04f)); g.setColour (on ? t.text : t.textDim);
        g.drawText (text, r, Justification::centred);
    };
    auto a = getLocalBounds().toFloat().reduced (20.f, 16.f);
    g.setFont (lnf.font (11.f, Bold, 0.15f)); g.setColour (t.textDim);
    const char* rows[] { "MODE", "STYLE", "AMOUNT", "LOCK" };
    for (int i = 0; i < 4; ++i) g.drawText (rows[i], a.getX(), a.getY() + i * 52.f, 110.f, 34.f, Justification::centredLeft);
    for (auto& [r, c] : layout())
    {
        const bool hov = hover == c;
        if (c >= 100 && c < 200) chip (r, RZ::modeNames()[c - 100], st.mode == c - 100, hov, acc);
        else if (c >= 200 && c < 300) chip (r, RZ::styleNames()[c - 200], st.style == c - 200, hov, t.cool);
        else if (c >= 300 && c < 400)
        {
            const bool locked = st.lock[(size_t) (c - 300)];
            chip (r, (locked ? "LOCKED " : "") + RZ::groupNames()[c - 300], locked, hov, t.textDim);
        }
        else if (c == 400)
        {
            g.setColour (t.border); g.fillRoundedRectangle (r.withSizeKeepingCentre (r.getWidth(), 4.f), 2.f);
            g.setColour (acc); g.fillRoundedRectangle (r.withSizeKeepingCentre (r.getWidth(), 4.f).withWidth (r.getWidth() * st.amount), 2.f);
            g.fillEllipse (Rectangle<float> (12.f, 12.f).withCentre ({ r.getX() + r.getWidth() * st.amount, r.getCentreY() }));
            g.setFont (lnf.font (13.f, SemiBold)); g.setColour (t.text);
            g.drawText (String (roundToInt (st.amount * 100.f)) + " %", r.withX (r.getRight() + 16.f).withWidth (80.f).expanded (0.f, 10.f), Justification::centredLeft);
        }
        else if (c == 500)
        {
            g.setColour (hov ? acc.brighter (0.15f) : acc); g.fillRoundedRectangle (r, 10.f);
            g.setFont (lnf.font (15.f, Bold, 0.12f)); g.setColour (Colours::white);
            g.drawText ("RANDOMIZE", r, Justification::centred);
            g.setFont (lnf.font (12.f, Medium)); g.setColour (t.textMute);
            const String info = hasLevel ? "Level matched: " + String (proc.lastRandomDb, 1) + " dB RMS" + String (CharPointer_UTF8 (" \xc2\xb7 ")) + "Undo = back" : "One click = one Undo step. The level is matched (about -14 dB RMS)";
            g.drawText (info, r.withX (r.getRight() + 18.f).withWidth (520.f), Justification::centredLeft);
        }
    }
}

void RandomPanel::setAmountAt (float x)
{
    auto st = RZ::read (proc.apvts.state);
    st.amount = jlimit (0.f, 1.f, (x - amountBar.getX()) / jmax (1.f, amountBar.getWidth()));
    st.amount = std::round (st.amount * 20.f) / 20.f;                  // 5 % steps
    RZ::write (proc.apvts.state, st);
    repaint();
}

void RandomPanel::fire()
{
    proc.randomizeSound();
    hasLevel = true;
    if (onRandomized) onRandomized();
    repaint();
}

void RandomPanel::mouseDown (const MouseEvent& e)
{
    const int c = hit (e.position);
    if (c < 0) return;
    auto st = RZ::read (proc.apvts.state);
    if (c >= 100 && c < 200) st.mode = c - 100;
    else if (c >= 200 && c < 300) st.style = c - 200;
    else if (c >= 300 && c < 400) st.lock[(size_t) (c - 300)] = ! st.lock[(size_t) (c - 300)];
    else if (c == 400) { for (auto& [r, k] : layout()) if (k == 400) amountBar = r; setAmountAt (e.position.x); return; }
    else if (c == 500) { fire(); return; }
    RZ::write (proc.apvts.state, st);
    repaint();
}
void RandomPanel::mouseDrag (const MouseEvent& e) { if (! amountBar.isEmpty() && e.mouseDownPosition.y > amountBar.getY() - 12.f && e.mouseDownPosition.y < amountBar.getBottom() + 12.f && hit (e.mouseDownPosition) == 400) setAmountAt (e.position.x); }
void RandomPanel::mouseMove (const MouseEvent& e)
{
    const int c = hit (e.position);
    if (c == hover) return;
    hover = c;
    if (c >= 100 && c < 200) setTooltip (modeTips[c - 100]);
    else if (c >= 200 && c < 300) setTooltip (styleTips[c - 200]);
    else if (c >= 300 && c < 400) setTooltip ("LOCK: this module never moves when you randomize");
    else if (c == 400) setTooltip ("How far from the current sound: 0 = nothing changes, 100 % = a new sound (inside the musical ranges)");
    else if (c == 500) setTooltip ("A new bass in this direction. The level is matched, the low end stays, the bass starts after the kick's tail");
    else setTooltip ({});
    repaint();
}
