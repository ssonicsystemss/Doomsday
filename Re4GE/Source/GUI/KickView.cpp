#include "KickView.h"

using namespace juce;
using namespace doomsday::flat;

double KickView::viewMs() const
{
    auto* s = proc.getEngine().currentSample();
    return s != nullptr && ! s->data.empty() ? jmax (50.0, s->data.size() * 1000.0 / s->rate) : 1000.0;
}
void KickView::setParam (const char* id, float v)
{
    if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}
void KickView::mouseDown (const MouseEvent& e)
{
    const float xs = xOf (param (ParamIDs::smpStart)), xe = xOf (param (ParamIDs::smpEnd));
    drag = std::abs (e.position.x - xs) < 10.f ? 0 : std::abs (e.position.x - xe) < 10.f ? 1 : -1;
    if (drag >= 0) if (auto* p = proc.apvts.getParameter (drag == 0 ? ParamIDs::smpStart : ParamIDs::smpEnd)) p->beginChangeGesture();
}
void KickView::mouseDrag (const MouseEvent& e)
{
    if (drag < 0) return;
    const double ms = jlimit (0.0, viewMs(), msOf (e.position.x));
    if (drag == 0) setParam (ParamIDs::smpStart, (float) jmin (ms, (double) param (ParamIDs::smpEnd) - 10.0));
    else setParam (ParamIDs::smpEnd, (float) jmax (ms, (double) param (ParamIDs::smpStart) + 10.0));
    repaint();
}
void KickView::tick()
{
    const float sig = param (ParamIDs::smpStart) + param (ParamIDs::smpEnd) * 0.37f + param (ParamIDs::smpRoot) * 3.1f + param (ParamIDs::smpReverse);
    if (proc.getSampleSerial() != lastSerial)
    {
        lastSerial = proc.getSampleSerial();
        lo.clear(); hi.clear();
        if (auto* s = proc.getEngine().currentSample(); s != nullptr && ! s->data.empty())
        {
            const int cols = jmax (1, (int) plot().getWidth());
            lo.assign ((size_t) cols, 0.f); hi.assign ((size_t) cols, 0.f);
            const size_t n = s->data.size();
            for (int c = 0; c < cols; ++c)
            {
                const size_t a = n * (size_t) c / (size_t) cols, b = jmax (a + 1, n * (size_t) (c + 1) / (size_t) cols);
                float mn = 0.f, mx = 0.f;
                for (size_t i = a; i < jmin (b, n); ++i) { mn = jmin (mn, s->data[i]); mx = jmax (mx, s->data[i]); }
                lo[(size_t) c] = mn; hi[(size_t) c] = mx;
            }
        }
        repaint();
    }
    else if (std::abs (sig - lastSig) > 1e-4f) repaint();
    lastSig = sig;
}
void KickView::paint (Graphics& g)
{
    auto& lf = lnfOf (*this);
    const auto& tk = tokens();
    g.setColour (tk.surface); g.fillRoundedRectangle (getLocalBounds().toFloat(), 12.f);
    const auto pl = plot();
    g.setFont (lf.font (11.f, Bold, 0.12f)); g.setColour (tk.textDim);
    g.drawText ("KICK-TO-REVERSE", getLocalBounds().toFloat().reduced (16.f, 8.f).withHeight (22.f), Justification::centredLeft);
    if (lo.empty())
    {
        g.setFont (lf.font (14.f, Medium)); g.setColour (tk.textMute);
        g.drawText ("Load or drop a kick WAV (e.g. one rendered from 4GE): it is split into TRANSIENT / PUNCH / BODY / TAIL and its note is found",
                    pl.toNearestInt(), Justification::centred);
        return;
    }
    const auto& k = proc.kickSections();
    // the sections as coloured bands
    if (k.ok)
    {
        const Colour cols[4] { Colour (0xffffc04d), Colour (0xffff3b3b), Colour (0xff9b6bff), Colour (0xff4ea1ff) };
        const char* names[4] { "TRANSIENT", "PUNCH", "BODY", "TAIL" };
        const double edges[5] { 0.0, k.transientEnd, k.punchEnd, k.bodyEnd, k.tailEnd };
        const float conf[4] { k.confTransient, k.confPunch, k.confBody, k.confTail };
        for (int i = 0; i < 4; ++i)
        {
            const float x0 = xOf (edges[i]), x1 = xOf (edges[i + 1]);
            if (x1 - x0 < 1.f) continue;
            g.setColour (cols[i].withAlpha (0.10f)); g.fillRect (x0, pl.getY(), x1 - x0, pl.getHeight());
            g.setColour (cols[i]); g.fillRect (x0, pl.getBottom() + 4.f, x1 - x0, 4.f);
            g.setFont (lf.font (10.f, SemiBold));
            if (x1 - x0 > 70.f) g.drawText (String (names[i]) + " " + String (roundToInt (conf[i] * 100.f)) + "%", Rectangle<float> (x0 + 2.f, pl.getBottom() + 10.f, jmax (60.f, x1 - x0), 14.f), Justification::centredLeft);
        }
    }
    // waveform
    g.setColour (tk.textDim);
    for (size_t c = 0; c < lo.size(); ++c)
        g.drawVerticalLine ((int) pl.getX() + (int) c, pl.getCentreY() - hi[c] * pl.getHeight() * 0.48f, pl.getCentreY() - lo[c] * pl.getHeight() * 0.48f + 1.f);
    // the region that plays
    const float xs = xOf (param (ParamIDs::smpStart)), xe = xOf (jmin ((double) param (ParamIDs::smpEnd), viewMs()));
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRect (pl.getX(), pl.getY(), jmax (0.f, xs - pl.getX()), pl.getHeight());
    g.fillRect (xe, pl.getY(), jmax (0.f, pl.getRight() - xe), pl.getHeight());
    for (float x : { xs, xe }) { g.setColour (Colours::white); g.fillRect (x - 1.f, pl.getY(), 2.f, pl.getHeight()); g.fillEllipse (x - 5.f, pl.getY() - 5.f, 10.f, 10.f); }
    const bool rev = param (ParamIDs::smpReverse) > 0.5f;
    g.setFont (lf.font (11.f, Medium)); g.setColour (tk.text);
    String info = (rev ? "plays BACKWARDS: " : "plays forwards: ") + String (roundToInt (param (ParamIDs::smpStart))) + " - "
                + (param (ParamIDs::smpEnd) > 3999.f ? String ("end") : String (roundToInt (param (ParamIDs::smpEnd))) + " ms");
    if (k.ok && k.rootNote > 0)
    {
        static const char* nn[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        info << "     detected note: " << nn[k.rootNote % 12] << (k.rootNote / 12 - 1) << " (" << String (k.rootHz, 1) << " Hz, "
             << (k.rootCents >= 0 ? "+" : "") << String (roundToInt (k.rootCents)) << " ct)";
    }
    if (proc.sampleIsEmbeddedOnly()) info << "     (the file is gone - the copy inside the project plays)";
    g.drawText (info, getLocalBounds().toFloat().reduced (16.f, 8.f).withHeight (22.f).withTrimmedLeft (150.f), Justification::centredLeft);
}
