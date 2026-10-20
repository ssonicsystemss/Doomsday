#include "PluginEditor.h"
#include "DoomsdayBrand.h"

using namespace juce;
using namespace doomsday::ui;

Theme TitanTheme::make()
{
    Theme t;
    t.bg = Colour (0xff030304);
    t.panel = Colour (0xff07080a);
    t.label = Colour (0xffa9afba);
    t.knobLight = Colour (0xff353a42);
    t.knobDark = Colour (0xff06070a);
    t.accents = { { plat, platDeep, platHot }, { band[0], band[0].darker (0.8f), band[0].brighter (0.4f) },
                  { band[1], platDeep, platHot }, { band[2], band[2].darker (0.8f), band[2].brighter (0.4f) } };
    t.crown = { platDeep.brighter (0.5f), plat, platHot, Colours::white };
    return t;
}

namespace
{
    const double fMin = 20.0, fMax = 20000.0;
    float raw (TitanProcessor& p, const String& id) { return p.apvts.getRawParameterValue (id)->load(); }
    String hzText (double hz) { return hz >= 1000.0 ? String (hz / 1000.0, hz >= 10000.0 ? 1 : 2) + " kHz" : String ((int) std::round (hz)) + " Hz"; }
    String lufsText (float v) { return v <= -69.9f ? String ("-inf") : String (v, 1); }
}

// ======================================================================= display
TitanDisplay::TitanDisplay (TitanProcessor& p, std::function<void (int)> cb) : proc (p), onSelect (std::move (cb))
{
    pre.setSampleRate (48000.0); post.setSampleRate (48000.0);
    scratch.resize ((size_t) doomsday::dsp::Spectrum::size);
}
void TitanDisplay::resized() { plot = getLocalBounds().toFloat().reduced (14.f, 12.f).withTrimmedTop (22.f).withTrimmedBottom (18.f); }
float TitanDisplay::xOf (double hz) const { return plot.getX() + plot.getWidth() * (float) (std::log (hz / fMin) / std::log (fMax / fMin)); }
double TitanDisplay::hzOf (float x) const { return fMin * std::pow (fMax / fMin, (double) jlimit (0.f, 1.f, (x - plot.getX()) / plot.getWidth())); }
int TitanDisplay::xoAt (float x) const
{
    const float a = xOf (raw (proc, ParamIDs::xoLow)), b = xOf (raw (proc, ParamIDs::xoHigh));
    if (std::abs (x - a) < 8.f) return 0;
    if (std::abs (x - b) < 8.f) return 1;
    return -1;
}
void TitanDisplay::mouseMove (const MouseEvent& e)
{
    const int h = xoAt (e.position.x);
    if (h != hoverXo) { hoverXo = h; repaint(); }
    setMouseCursor (h >= 0 ? MouseCursor::LeftRightResizeCursor : MouseCursor::PointingHandCursor);
}
void TitanDisplay::mouseDown (const MouseEvent& e)
{
    dragXo = xoAt (e.position.x);
    if (dragXo < 0)
    {
        const double hz = hzOf (e.position.x);
        const int b = hz < raw (proc, ParamIDs::xoLow) ? 0 : hz < raw (proc, ParamIDs::xoHigh) ? 1 : 2;
        if (onSelect) onSelect (b);
    }
}
void TitanDisplay::mouseDrag (const MouseEvent& e)
{
    if (dragXo < 0) return;
    auto* prm = proc.apvts.getParameter (dragXo == 0 ? ParamIDs::xoLow : ParamIDs::xoHigh);
    prm->setValueNotifyingHost (prm->convertTo0to1 ((float) hzOf (e.position.x)));
}
void TitanDisplay::tick()
{
    auto& e = proc.getEngine();
    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
    pre.setSampleRate (sr); post.setSampleRate (sr);
    e.preTap.latest (scratch.data(), doomsday::dsp::Spectrum::size);  pre.feed (scratch.data(), 1.5f);
    e.postTap.latest (scratch.data(), doomsday::dsp::Spectrum::size); post.feed (scratch.data(), 1.5f);
    for (int b = 0; b < 3; ++b)
    {
        const float g = e.bandGrDb[(size_t) b].load(), u = e.bandUpDb[(size_t) b].load();
        gr[(size_t) b] = g < gr[(size_t) b] ? g : gr[(size_t) b] * 0.85f + g * 0.15f;
        up[(size_t) b] = u > up[(size_t) b] ? u : up[(size_t) b] * 0.85f + u * 0.15f;
    }
    repaint();
}
void TitanDisplay::paint (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff040506)); g.fillRoundedRectangle (b, 8.f);
    const float lo = raw (proc, ParamIDs::xoLow), hi = raw (proc, ParamIDs::xoHigh);
    const float xs[4] = { plot.getX(), xOf (lo), xOf (hi), plot.getRight() };
    // band regions
    for (int k = 0; k < 3; ++k)
    {
        auto r = Rectangle<float> (xs[k], plot.getY(), xs[k + 1] - xs[k], plot.getHeight());
        g.setColour (TitanTheme::band[k].withAlpha (k == selected ? 0.07f : 0.025f));
        g.fillRect (r);
    }
    // grid
    g.setFont (lnf.valueFont (10.f));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = xOf (f);
        g.setColour (Colours::white.withAlpha (0.05f)); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (lnf.theme.dim); g.drawText (f >= 1000 ? String ((int) (f / 1000)) + "k" : String ((int) f), (int) x - 20, (int) plot.getBottom() + 2, 40, 14, Justification::centred);
    }
    auto yOf = [this] (float db) { return plot.getBottom() - plot.getHeight() * jlimit (0.f, 1.f, (db + 84.f) / 84.f); };
    for (float d : { -72.f, -48.f, -24.f })
    {
        g.setColour (Colours::white.withAlpha (0.04f)); g.drawHorizontalLine ((int) yOf (d), plot.getX(), plot.getRight());
    }
    // spectra (tilted +4.5 dB / oct like the ear hears a mix)
    auto curveOf = [&] (const doomsday::dsp::Spectrum& sp)
    {
        Path p; bool first = true;
        for (float x = plot.getX(); x <= plot.getRight(); x += 2.f)
        {
            const double hz = hzOf (x);
            const float db = sp.dbAt (hz) + 4.5f * (float) std::log2 (hz / 1000.0) - 6.f;
            if (first) { p.startNewSubPath (x, yOf (db)); first = false; } else p.lineTo (x, yOf (db));
        }
        return p;
    };
    {
        auto p = curveOf (post);
        Path fill (p); fill.lineTo (plot.getRight(), plot.getBottom()); fill.lineTo (plot.getX(), plot.getBottom()); fill.closeSubPath();
        ColourGradient fg (TitanTheme::plat.withAlpha (0.30f), 0.f, plot.getY(), TitanTheme::plat.withAlpha (0.02f), 0.f, plot.getBottom(), false);
        g.setGradientFill (fg); g.fillPath (fill);
        g.setColour (TitanTheme::plat.withAlpha (0.85f)); g.strokePath (p, PathStrokeType (1.4f));
        g.setColour (TitanTheme::band[0].withAlpha (0.45f)); g.strokePath (curveOf (pre), PathStrokeType (1.f));
    }
    // crossovers
    for (int k = 0; k < 2; ++k)
    {
        const float x = xs[k + 1];
        const bool h = hoverXo == k || dragXo == k;
        g.setColour (Colours::white.withAlpha (h ? 0.8f : 0.4f));
        g.drawLine (x, plot.getY(), x, plot.getBottom(), h ? 2.f : 1.2f);
        auto knobR = Rectangle<float> (12.f, 24.f).withCentre ({ x, plot.getCentreY() });
        g.setColour (Colour (0xff15171c)); g.fillRoundedRectangle (knobR, 4.f);
        g.setColour (Colours::white.withAlpha (h ? 0.9f : 0.5f)); g.drawRoundedRectangle (knobR, 4.f, 1.f);
        g.setFont (lnf.labelFont (10.f, 0.1f));
        g.drawText (hzText (k == 0 ? lo : hi), (int) x - 40, (int) plot.getY() + 2, 80, 14, Justification::centred);
    }
    // gain reduction (down = colour bar from the top, up = thin bar)
    for (int k = 0; k < 3; ++k)
    {
        const float cx = 0.5f * (xs[k] + xs[k + 1]);
        const float h = plot.getHeight() * 0.42f * jlimit (0.f, 1.f, -gr[(size_t) k] / 24.f);
        auto bar = Rectangle<float> (cx - 9.f, plot.getY() + 22.f, 18.f, h);
        g.setColour (TitanTheme::band[k].withAlpha (0.15f)); g.fillRoundedRectangle (Rectangle<float> (cx - 9.f, plot.getY() + 22.f, 18.f, plot.getHeight() * 0.42f), 3.f);
        g.setColour (TitanTheme::band[k].withAlpha (0.85f)); g.fillRoundedRectangle (bar, 3.f);
        if (up[(size_t) k] > 0.1f)
        {
            const float uh = plot.getHeight() * 0.42f * jlimit (0.f, 1.f, up[(size_t) k] / 24.f);
            g.setColour (Colour (0xff7dffb0).withAlpha (0.8f));
            g.fillRoundedRectangle (Rectangle<float> (cx + 12.f, plot.getY() + 22.f + plot.getHeight() * 0.42f - uh, 4.f, uh), 2.f);
        }
        g.setFont (lnf.titleFont (10.f, 0.2f));
        g.setColour (TitanTheme::band[k].withAlpha (k == selected ? 1.f : 0.6f));
        g.drawText (Choices::bandNames[k], Rectangle<float> (xs[k], plot.getY() + 4.f, xs[k + 1] - xs[k], 14.f).toNearestInt(), Justification::centred);
        g.setFont (lnf.valueFont (11.f));
        g.drawText (String (gr[(size_t) k], 1) + " dB", Rectangle<float> (cx - 40.f, plot.getY() + 26.f + plot.getHeight() * 0.42f, 80.f, 14.f).toNearestInt(), Justification::centred);
    }
    g.setFont (lnf.titleFont (11.f, 0.3f));
    g.setColour (TitanTheme::plat);
    g.drawText ("THE THRONE", b.reduced (16.f, 6.f).removeFromTop (16.f).toNearestInt(), Justification::centredLeft);
    g.setFont (lnf.labelFont (10.5f, 0.2f));
    g.setColour (lnf.theme.dim);
    g.drawText ("DRAG THE LINES = CROSSOVERS   /   CLICK A BAND = EDIT IT   /   ORANGE LINE = INPUT", b.reduced (16.f, 6.f).removeFromTop (16.f).toNearestInt(), Justification::centredRight);
}

// ======================================================================= curve
void CurveView::paint (Graphics& g)
{
    using namespace ParamIDs;
    auto& lnf = lnfOf (*this);
    auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff040506)); g.fillRoundedRectangle (b, 8.f);
    auto p = b.reduced (14.f).withTrimmedTop (8.f);
    auto pos = [&] (float inDb, float outDb) { return Point<float> (p.getX() + p.getWidth() * (inDb + 60.f) / 60.f, p.getBottom() - p.getHeight() * jlimit (0.f, 1.f, (outDb + 60.f) / 60.f)); };
    g.setColour (Colours::white.withAlpha (0.05f));
    for (float d : { -48.f, -36.f, -24.f, -12.f }) { g.drawLine ({ pos (d, -60.f), pos (d, 0.f) }); g.drawLine ({ pos (-60.f, d), pos (0.f, d) }); }
    g.setColour (Colours::white.withAlpha (0.12f)); g.drawLine ({ pos (-60.f, -60.f), pos (0.f, 0.f) }, 1.f);
    doomsday::dyn::CompParams c;
    const int bi = this->band;
    c.threshDb = raw (proc, ParamIDs::band (bi, thresh)); c.ratio = raw (proc, ParamIDs::band (bi, ratio)); c.upRatio = raw (proc, ParamIDs::band (bi, up));
    c.kneeDb = raw (proc, ParamIDs::band (bi, knee));
    const bool on = raw (proc, ParamIDs::band (bi, ParamIDs::on)) > 0.5f;
    Path cv;
    for (float x = -60.f; x <= 0.01f; x += 0.5f)
    {
        const float y = x + (on ? doomsday::dyn::BandComp::curve (x, c) : 0.f);
        if (x <= -60.f) cv.startNewSubPath (pos (x, y)); else cv.lineTo (pos (x, y));
    }
    const auto col = TitanTheme::band[this->band];
    g.setColour (col.withAlpha (0.25f)); g.strokePath (cv, PathStrokeType (5.f));
    g.setColour (col); g.strokePath (cv, PathStrokeType (1.8f));
    // threshold marker
    const auto t = pos (c.threshDb, c.threshDb);
    g.setColour (Colours::white.withAlpha (0.25f));
    for (float y = p.getY(); y < p.getBottom(); y += 6.f) g.drawLine (t.x, y, t.x, y + 3.f, 1.f);
    g.setFont (lnf.labelFont (10.f, 0.2f));
    g.setColour (lnf.theme.dim);
    g.drawText ("IN", p.toNearestInt().removeFromBottom (14), Justification::centredRight);
    g.drawText ("OUT", p.toNearestInt().removeFromTop (14), Justification::centredLeft);
    g.setColour (col);
    g.drawText ("GR " + String (liveGr, 1) + " dB", p.toNearestInt().removeFromTop (14), Justification::centredRight);
}

// ======================================================================= loudness
LoudnessPanel::LoudnessPanel (TitanProcessor& p) : proc (p)
{
    matchSw = std::make_unique<Switch> (p.apvts, ParamIDs::match, "MATCH", Switch::Chip);
    matchSw->setTooltip ("MATCH - the output is turned to the loudness of the input: hear if the processing is really better, not just louder");
    targetChoice = std::make_unique<Choice> (p.apvts, ParamIDs::target, Choices::targets);
    resetBtn.onClick = [this] { proc.getEngine().resetRequest.store (true); };
    resetBtn.setTooltip ("Restart the integrated loudness and the true peak hold");
    learnBtn.onClick = [this]
    {
        if (learnPhase >= 0) { learnPhase = -1; learnMsg = "LEARN STOPPED"; learnMsgTicks = 90; return; }
        learnPhase = 0; learnTicks = 0; learnSum = 0.0; learnCount = 0;
    };
    learnBtn.setTooltip ("LEARN - play the loudest part (the drop) and click: LIMIT is set by itself so the track reaches the TARGET loudness");
    addAndMakeVisible (*matchSw); addAndMakeVisible (*targetChoice); addAndMakeVisible (resetBtn); addAndMakeVisible (learnBtn);
}
float LoudnessPanel::currentTarget() const
{
    const int tIdx = jlimit (0, 4, (int) std::round (raw (proc, ParamIDs::target)));
    return tIdx > 0 ? Choices::targetLufs[tIdx] : -7.f;
}
// LEARN: 3 rounds of  wait for the short-term meter to settle -> measure -> move LIMIT by the difference
void LoudnessPanel::learnTick()
{
    if (learnPhase < 0) return;
    const bool playing = inS > -45.f;
    if (! playing) { learnBtn.setButtonText ("PLAY THE DROP"); return; }
    ++learnTicks;
    const int settle = 100, measure = 45;           // ~3.3 s settle (short-term is a 3 s window), ~1.5 s measure
    learnBtn.setButtonText ("LEARNING " + String (learnPhase + 1) + "/3");
    if (learnTicks <= settle) return;
    learnSum += s; ++learnCount;
    if (learnTicks < settle + measure) return;
    const float measured = (float) (learnSum / jmax (1, learnCount));
    const float diff = currentTarget() - measured;
    auto* lp = proc.apvts.getParameter (ParamIDs::limit);
    const float now = proc.apvts.getRawParameterValue (ParamIDs::limit)->load();
    const float next = jlimit (0.f, 18.f, now + diff * 0.85f);
    lp->beginChangeGesture(); lp->setValueNotifyingHost (lp->convertTo0to1 (next)); lp->endChangeGesture();
    ++learnPhase; learnTicks = 0; learnSum = 0.0; learnCount = 0;
    const bool done = std::abs (diff) < 0.4f || learnPhase >= 3 || next >= 17.9f || next <= 0.01f;
    if (done)
    {
        learnPhase = -1;
        learnMsg = next >= 17.9f ? "LIMIT AT MAX - THE MIX IS TOO QUIET" : "LEARNED: LIMIT " + String (next, 1) + " dB";
        learnMsgTicks = 150;
    }
}
void LoudnessPanel::resized()
{
    auto b = getLocalBounds().reduced (6);
    auto bottom = b.removeFromBottom (30);
    b.removeFromBottom (4);
    auto t = b.removeFromBottom (28);
    t.removeFromLeft (64);
    targetChoice->setBounds (t);
    const int w3 = bottom.getWidth() / 3;
    matchSw->setBounds (bottom.removeFromLeft (w3).reduced (4, 1));
    learnBtn.setBounds (bottom.removeFromLeft (w3 + 20).reduced (4, 1));
    resetBtn.setBounds (bottom.reduced (4, 1));
}
void LoudnessPanel::tick()
{
    auto& e = proc.getEngine();
    m = e.lufsM.load(); s = e.lufsS.load(); i = e.lufsI.load(); inS = e.lufsIn.load();
    tp = e.truePeakDb.exchange (-120.f); tpMax = e.truePeakMaxDb.load();
    const float l = e.limGrDb.exchange (0.f); lim = l < lim ? l : lim * 0.85f + l * 0.15f;
    const float c = e.clipDb.exchange (0.f); clip = c > clip ? c : clip * 0.85f;
    inPk = jmax (e.inPeak.exchange (0.f), inPk * 0.9f); outPk = jmax (e.outPeak.exchange (0.f), outPk * 0.9f);
    matchDb = e.matchGainDb.load();
    // slow averages for the advice line
    limAvg += 0.03f * (lim - limAvg); clipAvg += 0.03f * (clip - clipAvg);
    learnTick();
    if (learnPhase < 0 && learnBtn.getButtonText() != "LEARN") learnBtn.setButtonText ("LEARN");
    if (learnMsgTicks > 0) --learnMsgTicks;
    repaint();
}
void LoudnessPanel::paint (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    auto b = getLocalBounds().toFloat().reduced (6.f);
    {
        auto t = b.removeFromBottom (62.f).removeFromTop (28.f);
        g.setFont (lnf.labelFont (10.f, 0.2f)); g.setColour (lnf.theme.label);
        g.drawText ("TARGET", t.removeFromLeft (64.f).toNearestInt(), Justification::centredLeft);
    }
    const int tIdx = jlimit (0, 4, (int) std::round (raw (proc, ParamIDs::target)));
    const float target = Choices::targetLufs[tIdx];
    // big integrated + short-term
    auto top = b.removeFromTop (80.f);
    g.setFont (lnf.labelFont (10.5f, 0.25f)); g.setColour (lnf.theme.label);
    g.drawText ("SHORT-TERM LUFS", top.removeFromTop (14.f).toNearestInt(), Justification::centredLeft);
    const bool onTarget = tIdx > 0 && std::abs (s - target) < 1.f;
    g.setFont (lnf.titleFont (32.f, 0.05f));
    g.setColour (onTarget ? Colour (0xff7dffb0) : TitanTheme::platHot);
    g.drawText (lufsText (s), top.removeFromTop (42.f).toNearestInt(), Justification::centredLeft);
    g.setFont (lnf.valueFont (13.f)); g.setColour (lnf.theme.value);
    String sub = "M " + lufsText (m) + "     I " + lufsText (i);
    if (tIdx > 0) sub << "     TARGET " << String (target, 0) << " (" << (s - target > 0 ? "+" : "") << String (s - target, 1) << ")";
    g.drawText (sub, top.toNearestInt(), Justification::centredLeft);
    b.removeFromTop (2.f);
    // bars: IN / OUT peak, LIMITER GR, CLIP, TRUE PEAK
    auto bar = [&] (const String& name, float frac, Colour col, const String& value)
    {
        auto r = b.removeFromTop (21.f);
        g.setFont (lnf.labelFont (10.f, 0.2f)); g.setColour (lnf.theme.label);
        g.drawText (name, r.removeFromLeft (64.f).toNearestInt(), Justification::centredLeft);
        g.setColour (lnf.theme.value); g.setFont (lnf.valueFont (11.5f));
        g.drawText (value, r.removeFromRight (70.f).toNearestInt(), Justification::centredRight);
        auto track = r.reduced (4.f, 7.f);
        g.setColour (Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (track, 3.f);
        g.setColour (col); g.fillRoundedRectangle (track.withWidth (track.getWidth() * jlimit (0.f, 1.f, frac)), 3.f);
        b.removeFromTop (1.f);
    };
    auto dbFrac = [] (float pk) { return jlimit (0.f, 1.f, (doomsday::dyn::gainToDb (pk) + 48.f) / 48.f); };
    bar ("IN", dbFrac (inPk), TitanTheme::band[0].withAlpha (0.8f), String (doomsday::dyn::gainToDb (inPk), 1) + " dB");
    bar ("OUT", dbFrac (outPk), TitanTheme::plat, String (doomsday::dyn::gainToDb (outPk), 1) + " dB");
    bar ("LIMITER", -lim / 12.f, Colour (0xffffc04d), String (lim, 1) + " dB");
    bar ("CLIP", clip / 6.f, Colour (0xffff5a3c), String (clip, 1) + " dB");      // how much the clipper shaves off the peaks
    bar ("TRUE PEAK", jlimit (0.f, 1.f, (tpMax + 12.f) / 12.f), tpMax > raw (proc, ParamIDs::ceiling) + 0.05f ? Colour (0xffff5a3c) : TitanTheme::band[2],
         tpMax < -100.f ? String ("-inf") : String (tpMax, 1) + " dBTP");
    // one line of advice: what to do now
    {
        String adv; Colour col = lnf.theme.label;
        const bool playing = inS > -45.f;
        if (learnPhase >= 0)                          { adv = "LEARN: KEEP THE DROP PLAYING..."; col = Colour (0xffffc04d); }
        else if (learnMsgTicks > 0)                   { adv = learnMsg; col = Colour (0xff7dffb0); }
        else if (raw (proc, ParamIDs::match) > 0.5f)  { adv = "MATCH: " + String (matchDb, 1) + " dB to equal the input"; col = Colour (0xff7dffb0); }
        else if (! playing)                           adv = "PLAY THE DROP, THEN PRESS LEARN";
        else if (limAvg < -6.f || clipAvg > 3.f)      { adv = "TOO HOT - DISTORTION! TURN LIMIT DOWN"; col = Colour (0xffff5a3c); }
        else if (tIdx > 0 && s < target - 1.5f)       adv = "QUIET - TURN LIMIT UP OR PRESS LEARN";
        else if (tIdx > 0 && s > target + 1.5f)       { adv = "LOUDER THAN THE TARGET - TURN LIMIT DOWN"; col = Colour (0xffffc04d); }
        else                                          { adv = "SOUNDS GOOD"; col = Colour (0xff7dffb0); }
        g.setFont (lnf.labelFont (10.5f, 0.2f)); g.setColour (col);
        g.drawText (adv, b.removeFromTop (14.f).toNearestInt(), Justification::centredLeft);
    }
}

// ======================================================================= band controls
BandControls::BandControls (TitanProcessor& p, int b) : proc (p), band (b)
{
    using namespace ParamIDs;
    const int acc = b + 1;
    onSw = std::make_unique<Switch> (p.apvts, ParamIDs::band (b, on), "", Switch::Power, acc);
    soloSw = std::make_unique<Switch> (p.apvts, ParamIDs::band (b, solo), "SOLO", Switch::Chip, acc);
    addAndMakeVisible (*onSw); addAndMakeVisible (*soloSw);
    const std::pair<const char*, const char*> ks[] = { { pre, "PRE" }, { thresh, "THRESH" }, { ratio, "RATIO" }, { up, "UPWARD" }, { knee, "KNEE" },
                                                       { attack, "ATTACK" }, { release, "RELEASE" }, { sat, "SATURATE" }, { width, "WIDTH" }, { post, "POST" } };
    for (auto& [id, name] : ks)
    {
        const bool bip = String (id) == pre || String (id) == post;
        knobs.push_back (std::make_unique<Knob> (p.apvts, ParamIDs::band (b, id), name, acc, bip));
        addAndMakeVisible (*knobs.back());
    }
    knobs[3]->slider.setTooltip ("UPWARD - quiet parts are lifted towards the threshold (like OTT). OFF = 1 : 1");
    knobs[8]->slider.setTooltip ("WIDTH - stereo width of this band only: 0 % = mono (great for LOW), 100 % = as is, 200 % = wider");
}
void BandControls::resized()
{
    auto b = getLocalBounds();
    const float cw = (float) b.getWidth() / 5.f, rh = (float) b.getHeight() / 2.f;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int r = (int) i / 5, c = (int) i % 5;
        knobs[i]->setBounds (Rectangle<int> (b.getX() + (int) (cw * (float) c), b.getY() + (int) (rh * (float) r), (int) cw, (int) rh).withSizeKeepingCentre (jmin ((int) cw - 2, 92), (int) rh));
    }
}
void BandControls::refresh()
{
    const float a = onSw->isOn() ? 1.f : 0.35f;
    for (auto& k : knobs) if (std::abs (k->getAlpha() - a) > 0.001f) k->setAlpha (a);
}

// ======================================================================= canvas
Canvas::Canvas (TitanProcessor& p)
    : proc (p), lnf (TitanTheme::make()), display (p, [this] (int b) { select (b); }), curve (p), loud (p), presets (p, false, false)
{
    using namespace ParamIDs;
    setLookAndFeel (&lnf);
    addAndMakeVisible (display); addAndMakeVisible (curve); addAndMakeVisible (loud); addAndMakeVisible (presets);
    for (int b = 0; b < 3; ++b)
    {
        bands[(size_t) b] = std::make_unique<BandControls> (p, b);
        addChildComponent (*bands[(size_t) b]);
        addChildComponent (*bands[(size_t) b]->onSw); addChildComponent (*bands[(size_t) b]->soloSw);
    }
    limitKnob = std::make_unique<Knob> (p.apvts, limit, "", 0, false, true);
    limitKnob->slider.setTooltip ("LIMIT - how hard the master is pushed into the limiter = how loud it gets (watch LUFS and the LIMITER bar)");
    addAndMakeVisible (*limitKnob);
    const std::pair<const char*, const char*> mk[] = { { input, "INPUT" }, { tilt, "TILT" }, { monoBass, "MONO BASS" }, { clip, "CLIP" }, { ceiling, "CEILING" }, { limRel, "RELEASE" } };
    for (auto& [id, name] : mk)
    {
        masterKnobs.push_back (std::make_unique<Knob> (p.apvts, id, name, 0, String (id) == input || String (id) == tilt));
        addAndMakeVisible (*masterKnobs.back());
    }
    masterKnobs[3]->slider.setTooltip ("CLIP - a soft clipper shaves the peaks before the limiter: louder and punchier, the limiter works less");
    select (1);
    setSize (W, H);
    timerCallback();
    startTimerHz (30);
}
Canvas::~Canvas() { setLookAndFeel (nullptr); }

Rectangle<int> Canvas::chipBounds (int b) const
{
    const int w = (chipArea.getWidth() - 2 * 10) / 3;
    return { chipArea.getX() + b * (w + 10), chipArea.getY(), w, chipArea.getHeight() };
}

void Canvas::select (int b)
{
    display.selected = b; curve.band = b;
    if (b != shown)
    {
        for (int i = 0; i < 3; ++i)
        {
            bands[(size_t) i]->setVisible (i == b);
            bands[(size_t) i]->onSw->setVisible (i == b);
            bands[(size_t) i]->soloSw->setVisible (i == b);
        }
        shown = b;
    }
    repaint (chipArea.expanded (4));
    display.repaint(); curve.repaint();
}

void Canvas::mouseDown (const MouseEvent& e)
{
    for (int b = 0; b < 3; ++b) if (chipBounds (b).contains (e.getPosition())) select (b);
}

void Canvas::resized()
{
    panels.clear();
    const int m = 20, gap = 14;
    presets.setBounds (W - m - 300, 34, 300, 90);
    const Rectangle<int> dispA (m, 150, 1000, 330), loudA (dispA.getRight() + gap, 150, W - m - dispA.getRight() - gap, 330);
    display.setBounds (dispA);
    panels.push_back ({ "LOUDNESS", loudA });
    loud.setBounds (loudA.reduced (12, 10).withTrimmedTop (26));
    const Rectangle<int> bandA (m, dispA.getBottom() + gap, 1000, H - m - dispA.getBottom() - gap), mastA (bandA.getRight() + gap, bandA.getY(), loudA.getWidth(), bandA.getHeight());
    panels.push_back ({ "BAND", bandA });
    panels.push_back ({ "MASTER", mastA });
    {
        auto b = bandA.reduced (16, 10);
        auto head = b.removeFromTop (30);
        for (auto& bc : bands) { bc->onSw->setBounds (head.getRight() - 56, head.getY() + 2, 56, 26); bc->soloSw->setBounds (head.getRight() - 56 - 12 - 90, head.getY() + 1, 90, 28); }
        b.removeFromTop (8);
        chipArea = b.removeFromTop (40);
        b.removeFromTop (12);
        curve.setBounds (b.removeFromLeft (290));
        b.removeFromLeft (14);
        for (auto& bc : bands) bc->setBounds (b);
    }
    {
        auto b = mastA.reduced (14, 10);
        b.removeFromTop (30);
        auto top = b.removeFromTop (170);
        limitKnob->setBounds (top.withSizeKeepingCentre (170, 170).withTrimmedTop (20));
        const float cw = (float) b.getWidth() / 3.f, rh = (float) b.getHeight() / 2.f;
        for (size_t i = 0; i < masterKnobs.size(); ++i)
        {
            const int r = (int) i / 3, c = (int) i % 3;
            masterKnobs[i]->setBounds (Rectangle<int> (b.getX() + (int) (cw * (float) c), b.getY() + (int) (rh * (float) r), (int) cw, (int) rh).reduced (2, 0));
        }
    }
    renderBackground();
}

void Canvas::renderBackground()
{
    const float scale = 2.f, ui = 1120.f / (float) W;
    background = Image (Image::RGB, (int) (W * scale), (int) (H * scale), true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (scale));
    const auto plat = TitanTheme::plat;
    g.fillAll (lnf.theme.bg);
    {
        ColourGradient haze (plat.withAlpha (0.10f), W * 0.5f, 70.f, plat.withAlpha (0.f), W * 0.5f + 480.f, 70.f, true);
        g.setGradientFill (haze); g.fillRect (0, 0, W, 170);
        for (int i = 0; i < 4; ++i)
        {
            const float a = -0.5f + (float) i * 0.2f;
            paint::beam (g, { 160.f, -20.f }, -a - 0.2f, 0.05f, 1200.f, plat, 0.03f);
            paint::beam (g, { W - 160.f, -20.f }, a + 0.2f, 0.05f, 1200.f, plat, 0.03f);
        }
        ColourGradient vig (Colours::transparentBlack, W * 0.5f, H * 0.45f, Colours::black.withAlpha (0.7f), 0.f, -80.f, true);
        g.setGradientFill (vig); g.fillRect (0, 0, W, H);
        ColourGradient line (plat.withAlpha (0.f), 0.f, 0.f, plat.withAlpha (0.f), (float) W, 0.f, false);
        line.addColour (0.5, plat.withAlpha (0.7f));
        g.setGradientFill (line); g.fillRect (0.f, 136.f, (float) W, 1.f);
    }
    // title: engraved platinum with a crown line
    {
        const String title ("THRONE OF TITANS");
        const auto area = Rectangle<float> (0.f, 28.f, (float) W, 56.f);
        g.setFont (lnf.titleFont (38.f, 0.24f));
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (plat.withAlpha (0.07f / (float) i));
            for (auto o : { Point<float> (-1.6f * (float) i, 0.f), Point<float> (1.6f * (float) i, 0.f), Point<float> (0.f, 1.6f * (float) i), Point<float> (0.f, -1.6f * (float) i) })
                g.drawText (title, area.translated (o.x, o.y), Justification::centred);
        }
        g.setColour (Colours::black.withAlpha (0.8f)); g.drawText (title, area.translated (0.f, 2.f), Justification::centred);
        ColourGradient tg (Colours::white, 0.f, area.getY() + 10.f, Colour (0xff8d95a3), 0.f, area.getBottom() - 8.f, false);
        g.setGradientFill (tg); g.drawText (title, area, Justification::centred);
        // crown: three small peaks above the middle of the title
        Path crown;
        const float cx = W * 0.5f, cy = 22.f;
        crown.startNewSubPath (cx - 30.f, cy + 8.f); crown.lineTo (cx - 22.f, cy - 4.f); crown.lineTo (cx - 11.f, cy + 4.f); crown.lineTo (cx, cy - 8.f);
        crown.lineTo (cx + 11.f, cy + 4.f); crown.lineTo (cx + 22.f, cy - 4.f); crown.lineTo (cx + 30.f, cy + 8.f);
        g.setColour (plat.withAlpha (0.7f)); g.strokePath (crown, PathStrokeType (1.4f, PathStrokeType::mitered));
        g.setFont (lnf.labelFont (12.5f, 0.6f)); g.setColour (plat.withAlpha (0.8f));
        g.drawText ("MULTIBAND  MASTER  /  LIMITER  /  LOUDNESS", 0, 104, W, 18, Justification::centred);
    }
    const int tx = (int) doomsday::brand::headerTextX (ui);
    g.setFont (lnf.titleFont (10.f, 0.25f)); g.setColour (lnf.theme.text.withAlpha (0.75f));
    g.drawText ("V1.1", tx, 40, 300, 14, Justification::left);
    g.setFont (lnf.labelFont (12.f, 0.28f)); g.setColour (lnf.theme.label);
    g.drawText ("3 BANDS  /  UP + DOWN  /  TRUE PEAK", tx, 58, 320, 16, Justification::left);
    g.setColour (plat);
    g.drawText ("RULE THE LOUDNESS", tx, 76, 320, 16, Justification::left);
    doomsday::brand::drawHeaderBadge (g, ui, 0.55f);
    for (auto& [t, a] : panels) paint::glassPanel (g, lnf, a, t, 0);
    {
        auto a = panels[2].second.reduced (14, 10);
        g.setFont (lnf.labelFont (11.f, 0.3f)); g.setColour (lnf.theme.label);
        g.drawText ("LIMIT", a.getX(), a.getY() + 30, a.getWidth(), 16, Justification::centred);
    }
    g.setColour (plat.withAlpha (0.25f)); g.drawRoundedRectangle (display.getBounds().toFloat().expanded (1.f), 9.f, 1.f);
}

void Canvas::timerCallback()
{
    display.tick();
    loud.tick();
    presets.refresh();
    auto& e = proc.getEngine();
    for (int b = 0; b < 3; ++b) chipGr[(size_t) b] = e.bandGrDb[(size_t) b].load();
    curve.liveGr = chipGr[(size_t) curve.band];
    curve.repaint();
    for (auto& b : bands) if (b->isVisible()) b->refresh();
    repaint (chipArea.expanded (4));
}

void Canvas::paint (Graphics& g)
{
    if (background.isValid()) g.drawImage (background, getLocalBounds().toFloat(), RectanglePlacement::stretchToFit);
}

void Canvas::paintOverChildren (Graphics& g)
{
    // band chips (LOW / MID / HIGH): select, show on / solo state and live gain reduction
    for (int b = 0; b < 3; ++b)
    {
        auto r = chipBounds (b).toFloat();
        const bool sel = b == shown;
        const bool on = raw (proc, ParamIDs::band (b, ParamIDs::on)) > 0.5f, solo = raw (proc, ParamIDs::band (b, ParamIDs::solo)) > 0.5f;
        const auto col = TitanTheme::band[b];
        g.setColour (sel ? col.withAlpha (0.16f) : Colours::white.withAlpha (0.03f)); g.fillRoundedRectangle (r, 7.f);
        g.setColour (sel ? col.withAlpha (0.8f) : Colours::white.withAlpha (0.10f)); g.drawRoundedRectangle (r.reduced (0.5f), 7.f, sel ? 1.4f : 1.f);
        g.setColour (on ? col : Colour (0xff3a3d44)); g.fillEllipse (Rectangle<float> (7.f, 7.f).withCentre ({ r.getX() + 16.f, r.getCentreY() }));
        g.setFont (lnf.titleFont (12.f, 0.25f)); g.setColour (on ? lnf.theme.text : lnf.theme.dim);
        g.drawText (Choices::bandNames[b] + (solo ? "  /  SOLO" : ""), r.withTrimmedLeft (30.f).toNearestInt(), Justification::centredLeft);
        g.setFont (lnf.valueFont (13.f)); g.setColour (col.withAlpha (on ? 1.f : 0.4f));
        g.drawText (String (chipGr[(size_t) b], 1) + " dB", r.withTrimmedRight (14.f).toNearestInt(), Justification::centredRight);
    }
}

TitanEditor::TitanEditor (TitanProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (840, 540, 2100, 1350);
    getConstrainer()->setFixedAspectRatio ((double) Canvas::W / (double) Canvas::H);
    setSize (1120, 720);
}
