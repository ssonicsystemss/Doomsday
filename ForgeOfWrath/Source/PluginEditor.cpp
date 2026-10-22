#include "PluginEditor.h"
#include "DoomsdayBrand.h"

using namespace juce;
using namespace doomsday::ui;
using doomsday::kick::Shape;

Theme ForgeTheme::make()
{
    Theme t;
    t.bg = Colour (0xff030202);
    t.panel = Colour (0xff090605);
    t.label = Colour (0xffb5a59c);
    t.value = Colour (0xffe6d6cc);
    t.knobLight = Colour (0xff3a2f2a);
    t.knobDark = Colour (0xff070404);
    t.accents = { { magma, magmaDeep, magmaHot }, { heat, Colour (0xff6b4a08), Colour (0xfffff0b8) }, { ember, Colour (0xff4a0904), Colour (0xffff9a80) } };
    t.crown = { magmaDeep.brighter (0.6f), ember, magma, magmaHot, heat };
    return t;
}

namespace
{
    // the anvil / lane drawing code asks for title / label / value fonts and text / dim / label colours: flat look
    struct ForgeLook
    {
        doomsday::flat::FlatLookAndFeel& f;
        struct Th { Colour text, dim, label; } theme;
        explicit ForgeLook (Component& c) : f (doomsday::flat::lnfOf (c)),
            theme { doomsday::flat::tokens().text, doomsday::flat::tokens().textMute, doomsday::flat::tokens().textDim } {}
        Font titleFont (float h, float = 0.f) const { return f.font (h, doomsday::flat::Bold, 0.04f); }
        Font labelFont (float h, float = 0.f) const { return f.font (h, doomsday::flat::SemiBold); }
        Font valueFont (float h) const { return f.font (h, doomsday::flat::Medium); }
    };
    float raw (ForgeProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
    const Colour envCol[3] = { ForgeTheme::magmaHot, ForgeTheme::heat, Colour (0xffffe6d2) };
    const char* envName[3] = { "AMP", "DRIVE", "PITCH" };
}

// ======================================================================= THE ANVIL
AnvilDisplay::AnvilDisplay (ForgeProcessor& p) : proc (p)
{
    for (auto* prm : proc.getParameters())
        if (auto* wp = dynamic_cast<AudioProcessorParameterWithID*> (prm)) proc.apvts.addParameterListener (wp->paramID, this);
    setRepaintsOnMouseActivity (false);
}
AnvilDisplay::~AnvilDisplay()
{
    for (auto* prm : proc.getParameters())
        if (auto* wp = dynamic_cast<AudioProcessorParameterWithID*> (prm)) proc.apvts.removeParameterListener (wp->paramID, this);
}
void AnvilDisplay::resized()
{
    plot = getLocalBounds().toFloat().reduced (18.f, 14.f).withTrimmedTop (34.f).withTrimmedBottom (20.f);
    dirty.store (true);
}
Rectangle<float> AnvilDisplay::tabBounds (int i) const { return { 18.f + (float) i * 92.f, 10.f, 86.f, 26.f }; }   // AMP / DRIVE / PITCH

Point<float> AnvilDisplay::toScreen (const Shape::Pt& pt) const
{
    return { plot.getX() + plot.getWidth() * pt.ms / 1000.f, plot.getBottom() - plot.getHeight() * pt.v };
}
Shape::Pt AnvilDisplay::fromScreen (Point<float> s) const
{
    return { jlimit (0.f, 1000.f, (s.x - plot.getX()) / plot.getWidth() * 1000.f), jlimit (0.f, 1.f, (plot.getBottom() - s.y) / plot.getHeight()) };
}
int AnvilDisplay::pointAt (Point<float> s) const
{
    const auto& sh = shape[editing];
    int best = -1; float bd = 11.f;
    for (int i = 0; i < sh.n; ++i)
    {
        const float d = toScreen (sh.pts[(size_t) i]).getDistanceFrom (s);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

void AnvilDisplay::mouseMove (const MouseEvent& e)
{
    for (int i = 0; i < 3; ++i) if (tabBounds (i).contains (e.position)) { setMouseCursor (MouseCursor::PointingHandCursor); return; }
    const int h = pointAt (e.position);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? MouseCursor::DraggingHandCursor : plot.expanded (6.f).contains (e.position) ? MouseCursor::CrosshairCursor : MouseCursor::NormalCursor);
}
void AnvilDisplay::mouseDown (const MouseEvent& e)
{
    for (int i = 0; i < 3; ++i) if (tabBounds (i).contains (e.position)) { editing = i; hover = drag = -1; repaint(); return; }
    if (editing == 2 && raw (proc, ParamIDs::pitchMode) < 0.5f) { convertCurveToDrawn(); return; }
    auto sh = shape[editing];
    const int hit = pointAt (e.position);
    if (e.mods.isPopupMenu())
    {
        if (hit > 0) { sh.remove (hit); proc.setShape (editing, sh); }
        return;
    }
    if (hit >= 0) { drag = hit; return; }
    if (! plot.expanded (6.f).contains (e.position)) return;
    const auto pt = fromScreen (e.position);
    if (pt.ms <= 0.5f || ! sh.insert (pt.ms, pt.v)) return;
    for (int i = 0; i < sh.n; ++i) if (sh.pts[(size_t) i].ms == pt.ms && sh.pts[(size_t) i].v == pt.v) drag = i;
    proc.setShape (editing, sh);
    shape[editing] = sh;
}
void AnvilDisplay::mouseDrag (const MouseEvent& e)
{
    if (drag < 0) return;
    auto sh = shape[editing];
    if (drag >= sh.n) return;
    auto pt = fromScreen (e.position);
    // the first dot stays on the hit; a dot never passes its neighbours (they do not move)
    const float lo = drag == 0 ? 0.f : sh.pts[(size_t) drag - 1].ms + 0.5f;
    const float hi = drag == 0 ? 0.f : (drag + 1 < sh.n ? sh.pts[(size_t) drag + 1].ms - 0.5f : 1000.f);
    sh.pts[(size_t) drag] = { jlimit (lo, jmax (lo, hi), pt.ms), pt.v, sh.pts[(size_t) drag].bend };
    shape[editing] = sh;
    proc.setShape (editing, sh);
    repaint();
}
void AnvilDisplay::mouseUp (const MouseEvent&) { drag = -1; }

// first touch on PITCH in CURVE mode: copy the knob curve into dots and switch to DRAWN (nothing jumps)
void AnvilDisplay::convertCurveToDrawn()
{
    const auto prm = proc.gatherForGui();
    Shape s; s.n = 0;
    for (float pm : { 0.f, 15.f, 40.f, 80.f, 150.f, 300.f, 600.f, 1000.f })
    {
        const float ms = pm * prm.k.lengthMs / 1000.f;
        const float ratio = 1.f + (prm.k.startRatio - 1.f) * std::exp (-jmax (0.f, ms - prm.k.holdMs) / jmax (1.f, prm.k.dropMs) * prm.k.curve);
        s.pts[(size_t) s.n++] = { pm, jlimit (0.f, 1.f, std::log2 (ratio) / 4.f) };
    }
    s.tidy();
    proc.setShape (2, s);
    if (auto* mp = proc.apvts.getParameter (ParamIDs::pitchMode)) { mp->beginChangeGesture(); mp->setValueNotifyingHost (1.f); mp->endChangeGesture(); }
}
void AnvilDisplay::mouseDoubleClick (const MouseEvent& e)
{
    const int hit = pointAt (e.position);
    if (hit > 0) { auto sh = shape[editing]; sh.remove (hit); proc.setShape (editing, sh); hover = -1; }
}

void AnvilDisplay::renderPreview()
{
    auto prm = proc.gatherForGui();
    shape[0] = proc.getShape (0); shape[1] = proc.getShape (1); shape[2] = proc.getShape (2);
    prm.k.amp = shape[0]; prm.k.drive = shape[1]; prm.k.pitch = shape[2];
    prm.k.cook (96000.0);
    previewMs = prm.k.lengthMs;
    const double rate = 96000.0;
    const int total = (int) (rate * previewMs / 1000.0);
    const int cols = jmax (2, (int) plot.getWidth());
    lo.assign ((size_t) cols, 0.f); hi.assign ((size_t) cols, 0.f); pitch.assign ((size_t) cols, 0.f);
    doomsday::kick::KickVoice v;
    v.prepare (rate);
    v.setRoom (prm.k.roomSize);
    v.trigger (440.f * std::pow (2.f, (float) (proc.auditionNote - 69) / 12.f), 1.f);
    const float g = std::pow (10.f, prm.clipDriveDb / 20.f), outG = std::pow (10.f, prm.outDb / 20.f);
    for (int i = 0; i < total; ++i)
    {
        const float y = doomsday::kick::masterClip (v.render (prm.k), g, prm.clipSoft) * outG;
        const auto c = (size_t) jmin (cols - 1, (int) ((int64) i * cols / jmax (1, total)));
        lo[c] = jmin (lo[c], y); hi[c] = jmax (hi[c], y);
    }
    const float endHz = 440.f * std::pow (2.f, ((float) proc.auditionNote - 69.f + prm.k.tuneSemis) / 12.f);
    for (int c = 0; c < cols; ++c)
    {
        const float ms = previewMs * (float) c / (float) cols;
        pitch[(size_t) c] = prm.k.pitchDrawn ? endHz * std::pow (2.f, prm.k.pitch.at ((float) c / (float) cols * 1000.f) * 4.f)
                                             : endHz * (1.f + (prm.k.startRatio - 1.f) * std::exp (-jmax (0.f, ms - prm.k.holdMs) / jmax (1.f, prm.k.dropMs) * prm.k.curve));
    }
}

void AnvilDisplay::tick()
{
    const int ss = proc.getShapeSerial();
    if (dirty.exchange (false) || ss != lastShapeSerial) { lastShapeSerial = ss; renderPreview(); repaint(); }
    auto& e = proc.getEngine();
    const int hs = e.hitSerial.load();
    if (hs != lastHit) { lastHit = hs; hitStart = Time::getMillisecondCounterHiRes(); }
    const float was = playMs;
    playMs = lastHit > 0 ? (float) (Time::getMillisecondCounterHiRes() - hitStart) : -1.f;
    if (playMs > previewMs + 60.f) playMs = -1.f;
    if (playMs >= 0.f || was >= 0.f) repaint();
}

void AnvilDisplay::paint (Graphics& g)
{
    ForgeLook lnf (*this);
    auto b = getLocalBounds().toFloat();
    g.setColour (doomsday::flat::tokens().surface); g.fillRoundedRectangle (b, 12.f);
    {
        ColourGradient floor (ForgeTheme::magma.withAlpha (0.10f), b.getCentreX(), b.getBottom() + 40.f, ForgeTheme::magma.withAlpha (0.f), b.getCentreX(), b.getY() + 40.f, true);
        g.setGradientFill (floor); g.fillRoundedRectangle (b, 9.f);
    }
    // tabs
    for (int i = 0; i < 3; ++i)
    {
        auto r = tabBounds (i);
        const bool on = editing == i;
        g.setColour (on ? envCol[i].withAlpha (0.16f) : Colours::white.withAlpha (0.03f)); g.fillRoundedRectangle (r, 6.f);
        g.setColour (on ? envCol[i].withAlpha (0.85f) : Colours::white.withAlpha (0.12f)); g.drawRoundedRectangle (r.reduced (0.5f), 6.f, on ? 1.4f : 1.f);
        g.setFont (lnf.titleFont (11.f, 0.25f)); g.setColour (on ? lnf.theme.text : lnf.theme.dim);
        g.drawText (envName[i], r, Justification::centred);
    }
    g.setFont (lnf.labelFont (11.5f, 0.15f)); g.setColour (lnf.theme.dim);
    const bool drawnPitch = raw (proc, ParamIDs::pitchMode) > 0.5f;
    g.drawText (editing == 0 ? "AMP  -  volume of the tail over the kick"
              : editing == 1 ? "DRIVE  -  how dirty the kick is over time (swell it up = reverse bass)"
              : drawnPitch ? "PITCH  -  drawn pitch of the tail: bottom = the note, top = +4 octaves"
                           : "PITCH  -  now set by the TAIL knobs. Click here to draw it yourself",
                Rectangle<float> (tabBounds (2).getRight() + 16.f, 10.f, 640.f, 26.f), Justification::centredLeft);
    g.drawText ("CLICK: ADD DOT    DRAG: MOVE    DOUBLE / RIGHT CLICK: DELETE", Rectangle<float> (b.getRight() - 470.f, 10.f, 450.f, 26.f), Justification::centredRight);

    // grid (time)
    g.setFont (lnf.valueFont (10.f));
    const float step = previewMs > 900.f ? 200.f : previewMs > 400.f ? 100.f : 50.f;
    for (float t = 0.f; t <= previewMs + 0.1f; t += step)
    {
        const float x = plot.getX() + plot.getWidth() * t / previewMs;
        g.setColour (Colours::white.withAlpha (0.05f)); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (lnf.theme.dim); g.drawText (String ((int) t) + " ms", (int) x - 30, (int) plot.getBottom() + 4, 60, 14, Justification::centred);
    }
    for (float v : { 0.25f, 0.5f, 0.75f })
    {
        g.setColour (Colours::white.withAlpha (0.035f)); g.drawHorizontalLine ((int) (plot.getBottom() - plot.getHeight() * v), plot.getX(), plot.getRight());
    }
    // waveform (centred, molten)
    const int cols = (int) lo.size();
    if (cols > 1)
    {
        const float cy = plot.getCentreY(), hh = plot.getHeight() * 0.5f;
        Path wave;
        wave.startNewSubPath (plot.getX(), cy);
        for (int c = 0; c < cols; ++c) wave.lineTo (plot.getX() + (float) c, cy - hi[(size_t) c] * hh);
        for (int c = cols - 1; c >= 0; --c) wave.lineTo (plot.getX() + (float) c, cy - lo[(size_t) c] * hh);
        wave.closeSubPath();
        ColourGradient mg (ForgeTheme::magmaHot.withAlpha (0.55f), 0.f, cy, ForgeTheme::ember.withAlpha (0.35f), 0.f, cy - hh, false);
        mg.addColour (0.55, ForgeTheme::magma.withAlpha (0.45f));
        g.setGradientFill (mg); g.fillPath (wave);
        g.setColour (ForgeTheme::magma.withAlpha (0.7f)); g.strokePath (wave, PathStrokeType (0.8f));
        // pitch (log scale 30 Hz .. 1 kHz)
        Path pp;
        for (int c = 0; c < cols; ++c)
        {
            const float y = plot.getBottom() - plot.getHeight() * jlimit (0.f, 1.f, std::log (pitch[(size_t) c] / 30.f) / std::log (1000.f / 30.f));
            if (c == 0) pp.startNewSubPath (plot.getX(), y); else pp.lineTo (plot.getX() + (float) c, y);
        }
        g.setColour (Colours::white.withAlpha (0.45f));
        PathStrokeType (1.f).createDashedStroke (pp, pp, std::array<float, 2> { 5.f, 4.f }.data(), 2);
        g.fillPath (pp);
        g.setFont (lnf.labelFont (10.5f, 0.1f));
        g.drawText ("PITCH  " + String ((int) pitch.front()) + " > " + String ((int) pitch.back()) + " Hz", (int) plot.getRight() - 220, (int) plot.getY() + 2, 216, 14, Justification::right);
    }
    // the other envelope, faint
    auto envPath = [this] (const Shape& sh)
    {
        Path p;
        for (int i = 0; i < sh.n; ++i) { auto s = toScreen (sh.pts[(size_t) i]); if (i == 0) p.startNewSubPath (s); else p.lineTo (s); }
        auto last = toScreen (sh.pts[(size_t) sh.n - 1]);
        if (last.x < plot.getRight()) p.lineTo (plot.getRight(), last.y);
        return p;
    };
    for (int other = 0; other < 2; ++other)
        if (other != editing) { g.setColour (envCol[other].withAlpha (0.28f)); g.strokePath (envPath (shape[other]), PathStrokeType (1.2f)); }
    const bool showEdit = editing < 2 || drawnPitch;
    if (showEdit)
    {
        auto p = envPath (shape[editing]);
        g.setColour (envCol[editing].withAlpha (0.22f)); g.strokePath (p, PathStrokeType (5.f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (envCol[editing]); g.strokePath (p, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        const auto& sh = shape[editing];
        for (int i = 0; i < sh.n; ++i)
        {
            auto s = toScreen (sh.pts[(size_t) i]);
            const bool h = i == hover || i == drag;
            g.setColour (Colours::black); g.fillEllipse (Rectangle<float> (h ? 13.f : 10.f, h ? 13.f : 10.f).withCentre (s));
            g.setColour (i == 0 ? Colours::white : envCol[editing]); g.drawEllipse (Rectangle<float> (h ? 13.f : 10.f, h ? 13.f : 10.f).withCentre (s), 1.8f);
            if (h)
            {
                g.setFont (lnf.valueFont (11.f)); g.setColour (lnf.theme.text);
                const auto ms = sh.pts[(size_t) i].ms * previewMs / 1000.f;
                g.drawText (String ((int) ms) + " ms  /  " + (editing == 2 ? "+" + String (sh.pts[(size_t) i].v * 48.f, 1) + " st"
                                                                                : String ((int) std::round (sh.pts[(size_t) i].v * 100.f)) + " %"),
                            Rectangle<float> (150.f, 16.f).withCentre (s.translated (0.f, s.y < plot.getY() + 30.f ? 18.f : -18.f)), Justification::centred);
            }
        }
    }
    // playhead of the last hit
    if (playMs >= 0.f && playMs <= previewMs)
    {
        const float x = plot.getX() + plot.getWidth() * playMs / previewMs;
        ColourGradient ph (ForgeTheme::magmaHot.withAlpha (0.f), x - 40.f, 0.f, ForgeTheme::magmaHot.withAlpha (0.25f), x, 0.f, false);
        g.setGradientFill (ph); g.fillRect (Rectangle<float> (x - 40.f, plot.getY(), 40.f, plot.getHeight()));
        g.setColour (ForgeTheme::magmaHot); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
    }
}

// ======================================================================= REVERSE BASS lane
void BassLane::tick()
{
    const auto p = proc.gatherForGui();
    const float sig = (float) p.rb.stepBeats * 7.f + p.rb.in * 3.f + p.rb.out * 5.f + p.rb.curve * 0.37f + p.rb.riseSt * 0.011f + p.rb.peak * 13.f + (p.rb.pump ? 50.f : 0.f) + p.rb.releaseMs * 0.003f + p.rb.fadeMs * 0.007f
                    + p.k.lengthMs * 0.001f + (p.rbOn ? 100.f : 0.f) + proc.getEngine().tempoNow.load() * 0.0001f;
    if (std::abs (sig - lastSig) > 1e-5f) { lastSig = sig; repaint(); }
}

void BassLane::paint (Graphics& g)
{
    using namespace ForgeTheme;
    ForgeLook lnf (*this);
    const auto p = proc.gatherForGui();
    const double bpm = proc.getEngine().tempoNow.load(), beatMs = 60000.0 / bpm;
    auto r = getLocalBounds().toFloat();
    g.setColour (doomsday::flat::tokens().surface); g.fillRoundedRectangle (r, 12.f);
    auto plot = r.reduced (12.f, 10.f).withTrimmedTop (16.f).withTrimmedBottom (16.f);    // bottom: the ms scale
    // ms scale: a line every 10 ms, longer every 50 ms with the number (every 100 ms when the beat is long)
    {
        const float x0 = plot.getX(), w = plot.getWidth();
        const double labelEvery = beatMs > 700.0 ? 100.0 : 50.0;
        g.setFont (lnf.valueFont (10.5f));
        for (double ms = 0.0; ms <= beatMs + 0.01; ms += 10.0)
        {
            const float x = x0 + w * (float) (ms / beatMs);
            const bool major = std::fmod (ms + 0.01, labelEvery) < 0.1;
            g.setColour (lnf.theme.dim.withAlpha (major ? 0.22f : 0.08f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom() + (major ? 4.f : 2.f));
            if (major)
            {
                g.setColour (lnf.theme.dim);
                g.drawText (String ((int) std::round (ms)) + " ms", Rectangle<float> (x - 30.f, plot.getBottom() + 3.f, 60.f, 13.f), Justification::centred);
            }
        }
    }
    const float kickH = plot.getHeight() * 0.42f;
    auto kickLane = plot.removeFromTop (kickH);
    plot.removeFromTop (6.f);
    auto bassLaneR = plot;
    // grid: the steps of the pattern
    const int steps = (int) std::round (1.0 / p.rb.stepBeats);
    for (int i = 1; i < steps; ++i)
    {
        const float x = r.getX() + 12.f + (r.getWidth() - 24.f) * (float) i / (float) steps;
        g.setColour (lnf.theme.dim.withAlpha (0.5f)); g.drawVerticalLine ((int) x, kickLane.getY(), bassLaneR.getBottom());
    }
    // the kick: its LENGTH against the beat
    const float kickFrac = (float) jmin (1.0, (double) p.k.lengthMs / beatMs);
    {
        Path k; k.startNewSubPath (kickLane.getX(), kickLane.getBottom());
        for (int i = 0; i <= 60; ++i)
        {
            const float t = (float) i / 60.f, x = kickLane.getX() + kickLane.getWidth() * kickFrac * t;
            k.lineTo (x, kickLane.getBottom() - kickLane.getHeight() * (t < 0.03f ? t / 0.03f : 1.f - 0.6f * t * t));
        }
        k.lineTo (kickLane.getX() + kickLane.getWidth() * kickFrac, kickLane.getBottom()); k.closeSubPath();
        g.setColour (magmaHot.withAlpha (0.35f)); g.fillPath (k);
        g.setColour (magmaHot); g.strokePath (k, PathStrokeType (1.2f));
    }
    // the bass swells
    Path bp; bool started = false;
    float firstIn = 2.f;
    for (int i = 0; i <= 400; ++i)
    {
        const double beat = (double) i / 400.0;
        const double q = beat / p.rb.stepBeats;
        float riseU = 0.f;
        const float a = doomsday::rbass::swell (p.rb, q - std::floor (q), p.rb.stepBeats * 60.0 / bpm, riseU);
        if (a > 0.001f) firstIn = jmin (firstIn, (float) beat);
        const float x = bassLaneR.getX() + bassLaneR.getWidth() * (float) beat, y = bassLaneR.getBottom() - bassLaneR.getHeight() * a;
        if (! started) { bp.startNewSubPath (x, y); started = true; } else bp.lineTo (x, y);
    }
    const Colour bc = p.rbOn ? magma : lnf.theme.dim;
    Path bf (bp); bf.lineTo (bassLaneR.getRight(), bassLaneR.getBottom()); bf.lineTo (bassLaneR.getX(), bassLaneR.getBottom()); bf.closeSubPath();
    g.setColour (bc.withAlpha (0.3f)); g.fillPath (bf);
    g.setColour (bc); g.strokePath (bp, PathStrokeType (1.6f));
    // labels + overlap warning
    g.setFont (lnf.labelFont (11.f, 0.2f));
    g.setColour (lnf.theme.label);
    g.drawText ("ONE BEAT AT " + String (roundToInt (bpm)) + " BPM", r.reduced (12.f, 4.f).withHeight (14.f), Justification::topLeft);
    g.drawText ("KICK", kickLane.reduced (4.f, 0.f), Justification::topRight);
    g.drawText ("BASS", bassLaneR.reduced (4.f, 0.f), Justification::topRight);
    if (p.rbOn && kickFrac > firstIn)
    {
        g.setColour (ember);
        g.drawText ("OVERLAP - SHORTEN THE KICK (LENGTH) OR MOVE IN", r.reduced (12.f, 4.f).withHeight (14.f), Justification::topRight);
    }
}

// ======================================================================= canvas (flat, like 5CR34M 4LPH4)
using namespace doomsday::flat;

namespace { const String dotSep (CharPointer_UTF8 (" \xc2\xb7 ")); }

Canvas::Card& Canvas::card (const String& title, Colour dot, int pg)
{
    cards.push_back ({});
    auto& c = cards.back(); c.title = title; c.dot = dot; c.page = pg;
    return c;
}

Component* Canvas::knob (Card& c, const char* id, const char* label, const char* tip, bool bipolar)
{
    auto k = std::make_unique<doomsday::flat::Knob> (proc.apvts, id, label, c.dot, bipolar);
    k->slider.setTooltip (tip);
    auto* kp = k.get(); addAndMakeVisible (*kp); owned.push_back (std::move (k));
    c.knobs.push_back (kp);
    return kp;
}

Canvas::Canvas (ForgeProcessor& p) : proc (p), anvil (p), bassLane (p)
{
    using namespace ParamIDs;
    setLookAndFeel (&lnf);
    const auto& t = tokens();
    const Colour hot = t.hot, cool = t.cool, n { 0xffb4bac3 }, gold { 0xffffc46b };
    cards.reserve (20);
    auto seg = [this] (const char* id, StringArray labels, Colour c, const char* tip)
    {
        auto s = std::make_unique<Segmented> (proc.apvts, id, labels, c);
        s->setTooltip (tip);
        auto* sp = s.get(); addAndMakeVisible (*sp); owned.push_back (std::move (s)); return (Component*) sp;
    };
    auto toggle = [this] (const char* id, const char* text, Toggle::Style st, Colour c, const char* tip)
    {
        auto s = std::make_unique<Toggle> (proc.apvts, id, text, st, c);
        s->setTooltip (tip);
        auto* sp = s.get(); addAndMakeVisible (*sp); owned.push_back (std::move (s)); return (Component*) sp;
    };

    // ---- always: MASTER (next to THE ANVIL)
    {
        auto& c = card ("Master", n, -1);
        knob (c, length, "Length", "Length of the whole kick: the envelopes stretch with it");
        knob (c, clipDrive, "Clip", "Pushes the kick into the master clipper: louder, harder, flatter");
        knob (c, clipSoft, "Soft", "Clipper shape: 0 = hard (aggressive), 1 = soft (round)");
        knob (c, velocity, "Velocity", "0 = every hit full level, 1 = MIDI velocity sets the level");
        knob (c, output, "Output", "Output level", true);
    }
    // ---- page 0: KICK
    {
        auto& c = card ("Tail", hot, 0);
        knob (c, tailLevel, "Level", "Level of the distorted tail");
        knob (c, tailStart, "Start", "Pitch at the hit = START x the played note. x1 = no drop (zaag kicks), high = hard 'tok'");
        knob (c, tailDrop, "Drop", "How long the pitch falls from START down to the note");
        knob (c, tailHold, "Hold", "The pitch stays up at START this long before it drops");
        knob (c, tailCurve, "Curve", "Shape of the pitch fall: low = slow glide, high = fast snap then a long settle");
        knob (c, tailWave, "Wave", "Oscillator before the distortion: sine > triangle > saw (more buzz)");
        knob (c, tune, "Pitch", "Pitch of the TAIL only, in semitones (PUNCH and CLICK keep their own pitch)", true);
        knob (c, tailFine, "Fine", "Fine pitch of the tail in cents", true);
        c.head.push_back (seg (pitchMode, { "Curve", "Drawn" }, hot, "CURVE = pitch from the knobs, DRAWN = pitch from the PITCH envelope on the screen"));
    }
    {
        auto& c = card ("Stage 1", hot, 0);
        c.head.push_back (seg (distType, { "Soft", "Tube", "Hard", "Fold", "Raw" }, hot, "Distortion character: SOFT round, TUBE warm, HARD square, FOLD metallic, RAW buzzing"));
        knob (c, ParamIDs::drive, "Drive", "How hard the tail hits the first distortion (the DRIVE envelope moves it over time)");
        knob (c, tone, "Tone", "Before the distortion: left = more low end, right = more mids", true);
    }
    {
        auto& c = card ("Mid EQ", hot, 0);
        knob (c, midHz, "Freq", "A bell between the two distortions: shapes what STAGE 2 bites into");
        knob (c, midGain, "Gain", "Boost = that range gets distorted harder (more honk), cut = smoother tail", true);
        knob (c, midQ, "Q", "Width of the bell");
    }
    {
        auto& c = card ("Stage 2", hot, 0);
        c.head.push_back (seg (dist2Type, { "Off", "Soft", "Tube", "Hard", "Fold", "Raw" }, hot, "Second distortion after the MID EQ. OFF = one stage only"));
        knob (c, drive2, "Drive", "How hard STAGE 2 is pushed");
    }
    {
        auto& c = card ("Filter", cool, 0);
        knob (c, lowCut, "Low cut", "Removes rumble under the tail");
        knob (c, highCut, "High cut", "Darkens the distortion: low = round and warm, high = open and fizzy");
        knob (c, body, "Body", "More or less low end (the fundamental of the tail)", true);
        knob (c, cleanSub, "Clean sub", "Below this frequency the distortion is replaced by a clean sine: a tight, mono-safe sub (OFF = all distorted)");
    }
    {
        auto& c = card ("Texture", cool, 0);
        knob (c, scream, "Scream", "A resonant peak pushed into saturation: the 'yell' of raw kicks");
        knob (c, screamHz, "Scream Hz", "Where the scream peak sits");
        knob (c, crunch, "Crunch", "Extra high distortion at the start of the tail (the 'crack')");
        knob (c, crunchDec, "Crunch dec", "How long the crunch lasts");
        knob (c, grit, "Grit", "Noisy texture riding on the tail: the dirty 'sand'");
    }
    {
        auto& c = card ("Impact", cool, 0);
        knob (c, chaos, "Chaos", "Noise burst pushed INTO the distortion at the start: the dense, messy impact of modern kicks");
        knob (c, chaosDec, "Decay", "How long the chaos lasts");
        knob (c, chaosHz, "Tone", "Colour of the chaos: low = dark rumble, high = bright crackle");
    }
    // ---- page 1: LAYERS
    {
        auto& c = card ("Punch", gold, 1);
        knob (c, punchLevel, "Level", "The punch on top: the body / tok of the hit");
        knob (c, punchStart, "Start", "Punch pitch at the hit (never follows the note)");
        knob (c, punchEnd, "End", "Punch pitch at the end of its drop (can be higher than START: a rising tok)");
        knob (c, punchDrop, "Drop", "How fast the punch pitch moves from START to END");
        knob (c, punchDecay, "Decay", "How fast the punch fades (TOK: how fast the spike settles on the shelf)");
        knob (c, punchDrive, "Drive", "Distorts the punch: from a clean sine to a hard tok");
        knob (c, punchLen, "Length", "TOK: the punch holds its level this long and is cut (100 - 150 ms like tok samples). DECAY (0) = the old fading punch");
        c.head.push_back (toggle (punchOn, "On", Toggle::Pill, gold, "Punch on / off"));
        c.head.push_back (toggle (punchInto, "Into dist", Toggle::Chip, gold, "ON: the punch gets its own pass through the tail's distortion - it sounds like the start of the tail. OFF: a clean sine on top"));
    }
    {
        auto& c = card ("Click", gold, 1);
        knob (c, clickLevel, "Level", "Short noise click at the very start: helps the kick cut through");
        knob (c, clickHz, "Tone", "Colour of the click");
        knob (c, clickLen, "Length", "Length of the click");
    }
    {
        auto& c = card ("Fixed head", gold, 1);
        knob (c, headLen, "Length", "How long the start of the kick stays the same on every note (reference kicks: 100 - 130 ms)");
        knob (c, headHz, "Pitch", "The note the head is always played on - after LENGTH the tail moves to the key you play");
        c.head.push_back (toggle (headOn, "On", Toggle::Pill, gold, "ON: the start of the kick is exactly the same on every note; only the tail follows the key"));
    }
    {
        auto& c = card ("Room", cool, 1);
        knob (c, room, "Room", "Small room. INTO DIST on: the reflections are distorted with the tail. Off: space around the kick");
        knob (c, roomSize, "Size", "Room size: small = thick, big = long tail");
        c.head.push_back (toggle (roomInto, "Into dist", Toggle::Chip, cool, "ON: the room goes INTO the distortion. OFF: the room comes after (space)"));
    }
    {
        auto& c = card ("OTT", cool, 1);
        knob (c, ott, "OTT", "3-band upward + downward compression on the whole kick: dense, loud, in your face");
    }
    {
        auto& c = card ("Sample", n, 1);
        knob (c, smpLevel, "Level", "Level of your own sample (LOAD or drop a WAV on the plugin)");
        knob (c, smpPitch, "Pitch", "Pitch of the sample in semitones (it does not follow the note)", true);
        knob (c, smpStart, "Start", "Skips the start of the sample");
        loadBtn.setTooltip ("Load your own punch / kick / layer (WAV, AIFF, FLAC). You can also drag a file onto the plugin");
        clearBtn.setTooltip ("Remove the sample");
        loadBtn.onClick = [this]
        {
            chooser = std::make_unique<FileChooser> ("Load a sample", File::getSpecialLocation (File::userDocumentsDirectory), "*.wav;*.aif;*.aiff;*.flac");
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                                  [this] (const FileChooser& fc) { if (fc.getResult().existsAsFile()) proc.loadSample (fc.getResult()); });
        };
        clearBtn.onClick = [this] { proc.clearSample(); };
        addAndMakeVisible (loadBtn); addAndMakeVisible (clearBtn);
        c.head.push_back (&clearBtn); c.head.push_back (&loadBtn);
    }
    // ---- page 2: REVERSE BASS
    {
        auto& c = card ("Reverse bass", hot, 2);
        knob (c, rbIn, "In", "Where in each step the bass starts (PUMP: where the duck ends)");
        knob (c, rbOut, "Out", "Where it stops - the rest of the step is a clean gap before the next kick");
        knob (c, rbCurve, "Curve", "SWELL: early / late rise. PUMP: how the release lands (high = fast, then smooth)");
        knob (c, rbRelease, "Release", "PUMP: how fast the bass comes back after the kick (like a sidechain release)");
        knob (c, rbFade, "Fade", "PUMP: how short the cut right before the next kick is");
        knob (c, rbPeak, "Peak", "SWELL only: where the bass is loudest, then it fades to silence at OUT");
        knob (c, rbRise, "Rise", "SYNTH bass only (FROM TAIL off): pitch rise at the end of every swell");
        knob (c, rbTune, "Tune", "Bass pitch against the kick note in semitones (12 = an octave up)", true);
        knob (c, rbSub, "Sub", "Clean sine on the bass note under the reverse bass - the bass punch (FHSP references: the note itself is the loudest part)");
        knob (c, rbDrive, "Drive", "Distortion of the bass: from a clean sub to a buzzing saw");
        knob (c, rbTone, "Tone", "Low-pass of the bass: low = round and deep, high = bright and present");
        knob (c, rbLevel, "Level", "Bass level against the kick (both go into the master clipper)");
        c.head.push_back (toggle (rbOn, "On", Toggle::Pill, hot, "Reverse bass: every kick starts one beat of bass on the same note"));
        c.head.push_back (toggle (rbPump, "Pump", Toggle::Chip, hot, "ON: sidechain shape - the kick ducks the bass, it comes back in RELEASE, holds and is cut in FADE. OFF: SWELL - a slow rise to PEAK"));
        c.head.push_back (toggle (rbTail, "From tail", Toggle::Chip, hot, "ON: the bass is the kick's own tail played backwards. OFF: a saw + sine bass with RISE"));
        c.head.push_back (seg (rbPattern, { "1/4", "1/8", "1/16" }, hot, "How many bass swells per beat (locked to the FL tempo)"));
    }
    addAndMakeVisible (anvil);
    addAndMakeVisible (bassLane);

    tabs = std::make_unique<TabStrip> (StringArray { "Kick", "Layers", "Reverse bass" }, hot);
    tabs->onSelect = [this] (int i) { showPage (i); };
    tabs->isOn = [this] (int i) { return i == 2 ? raw (proc, ParamIDs::rbOn) > 0.5f : i == 1 ? raw (proc, ParamIDs::punchOn) > 0.5f : true; };
    addAndMakeVisible (*tabs);

    // top bar: presets like 5CR34M 4LPH4 + Folder (opens your presets) + Play (hold)
    refreshList();
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        if (id < userBase) proc.setCurrentProgram (id - 1);
        else if (isPositiveAndBelow (id - userBase, userFiles.size())) proc.loadUserPresetFile (userFiles[id - userBase]);
        lastName = {};
    };
    randomB.onClick = [this] { proc.randomizeSound(); lastName = {}; };
    saveB.onClick = [this] { showSave(); };
    folderB.onClick = [this] { auto f = proc.userPresetFolder(); f.createDirectory(); f.startAsProcess(); };
    playB.onPress = [this] (bool d) { proc.audition (d); };
    prev.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    undoB.onClick = [this] { proc.undo.undo(); };
    redoB.onClick = [this] { proc.undo.redo(); };
    randomB.setTooltip ("A new kick in musical ranges");
    saveB.setTooltip ("Save the current kick to Documents / Forge of Wrath / Presets");
    folderB.setTooltip ("Open the folder with your own presets");
    playB.setTooltip ("Hold to play the kick");
    FlatLookAndFeel::makePrimary (saveB);
    for (auto* c : { (Component*) &presetBox, (Component*) &prev, (Component*) &next, (Component*) &randomB, (Component*) &saveB, (Component*) &folderB,
                     (Component*) &playB, (Component*) &undoB, (Component*) &redoB })
        addAndMakeVisible (c);

    setSize (W, H);
    showPage (0);
    timerCallback();
    startTimerHz (30);
}

Canvas::~Canvas()
{
    if (dialog != nullptr) dialog->exitModalState (0);
    dialog.reset();
    setLookAndFeel (nullptr);
}

void Canvas::refreshList()
{
    presetBox.clear (dontSendNotification);
    presetBox.addSectionHeading ("FACTORY");
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    userFiles = proc.userPresetFiles();
    if (! userFiles.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("USER  (Folder = open them)");
        for (int i = 0; i < userFiles.size(); ++i) presetBox.addItem (userFiles[i].getFileNameWithoutExtension(), userBase + i);
    }
    lastName = {};
}

void Canvas::step (int d)
{
    Array<int> ids;
    for (int i = 0; i < presetBox.getNumItems(); ++i) ids.add (presetBox.getItemId (i));
    if (ids.isEmpty()) return;
    int at = ids.indexOf (presetBox.getSelectedId());
    at = at < 0 ? 0 : (at + d + ids.size()) % ids.size();
    presetBox.setSelectedId (ids[at], sendNotificationSync);
}

void Canvas::showSave()
{
    if (dialog != nullptr) return;
    dialog = std::make_unique<AlertWindow> ("Save preset", "Name (Documents / Forge of Wrath / Presets):", MessageBoxIconType::NoIcon, this);
    dialog->setLookAndFeel (&lnf);
    dialog->addTextEditor ("name", proc.currentPresetName().startsWith ("Random") ? String ("My Kick") : proc.currentPresetName());
    dialog->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    SafePointer<Canvas> safe (this);
    dialog->enterModalState (true, ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr || safe->dialog == nullptr) return;
        const auto name = safe->dialog->getTextEditorContents ("name");
        MessageManager::callAsync ([safe] { if (safe != nullptr) safe->dialog.reset(); });
        if (result == 1)
        {
            if (safe->proc.saveUserPresetAs (name)) safe->refreshList();
            else AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Save preset", "Could not save the preset.");
        }
    }), false);
}

void Canvas::showPage (int pg)
{
    page = jlimit (0, 2, pg);
    if (tabs != nullptr && tabs->getSelected() != page) tabs->setSelected (page);
    for (auto& c : cards)
    {
        const bool on = c.page < 0 || c.page == page;
        for (auto* k : c.knobs) k->setVisible (on);
        if (c.side != nullptr) c.side->setVisible (on);
        for (auto* h : c.head) h->setVisible (on);
    }
    bassLane.setVisible (page == 2);
    resized();
    repaint();
}

void Canvas::layoutCard (Card& c, int kw)
{
    const int kh = doomsday::flat::Knob::heightFor (44);
    auto a = c.area.reduced (16, 10);
    auto head = a.removeFromTop (28);
    for (auto* h : c.head)
    {
        const int w = dynamic_cast<Segmented*> (h) != nullptr ? jmin (c.title == "Stage 2" ? 230 : 190, head.getWidth() - 90)
                    : dynamic_cast<TextButton*> (h) != nullptr ? (h == &clearBtn ? 28 : 64) : 92;
        h->setBounds (head.removeFromRight (w).withSizeKeepingCentre (w, 24));
        head.removeFromRight (6);
    }
    a.removeFromTop (4);
    const int cols = jmax (1, (int) c.knobs.size());
    const int perRow = jmax (1, jmin (cols, a.getWidth() / kw));
    for (int i = 0; i < cols; ++i)
    {
        const int row = i / perRow, col = i % perRow;
        c.knobs[(size_t) i]->setBounds (a.getX() + col * kw, a.getY() + row * (kh + 4), kw, kh);
    }
}

void Canvas::resized()
{
    const int m = 20, gap = 12, kw = 74;
    // top bar
    const int bx = W - m;
    redoB.setBounds (bx - 62, 14, 62, 30);
    undoB.setBounds (bx - 130, 14, 62, 30);
    playB.setBounds (bx - 206, 14, 70, 30);
    folderB.setBounds (bx - 284, 14, 72, 30);
    saveB.setBounds (bx - 362, 14, 72, 30);
    randomB.setBounds (bx - 454, 14, 86, 30);
    next.setBounds (bx - 498, 14, 34, 30);
    presetBox.setBounds (bx - 504 - 270, 14, 270, 30);
    prev.setBounds (presetBox.getX() - 40, 14, 34, 30);
    // THE ANVIL + MASTER
    auto& master = cards[0];
    master.area = { W - m - 430, 68, 430, 200 };
    anvil.setBounds (m, 68, master.area.getX() - gap - m, 200);
    tabs->setBounds (m, 280, 420, 30);
    // pages
    const int y1 = 318, rowH = 134, y2 = y1 + rowH + gap;
    auto place = [&] (std::initializer_list<std::pair<int, int>> list, int y)
    {
        int x = m;
        for (auto [idx, w] : list) { cards[(size_t) idx].area = { x, y, w == 0 ? W - m - x : w, rowH }; x += cards[(size_t) idx].area.getWidth() + gap; }
    };
    for (auto& c : cards) if (c.page >= 0) c.area = {};
    if (page == 0)
    {
        place ({ { 1, 8 * kw + 32 }, { 2, 360 }, { 4, 0 } }, y1);                          // Tail | Stage 1 | Stage 2
        place ({ { 3, 3 * kw + 32 }, { 5, 4 * kw + 32 }, { 6, 5 * kw + 32 }, { 7, 0 } }, y2);   // Mid EQ | Filter | Texture | Impact
    }
    else if (page == 1)
    {
        place ({ { 8, 7 * kw + 32 }, { 9, 3 * kw + 32 }, { 10, 0 } }, y1);      // Punch | Click | Fixed head
        place ({ { 11, 3 * kw + 32 }, { 12, 2 * kw + 32 }, { 13, 0 } }, y2);    // Room | OTT | Sample
    }
    else
    {
        cards[14].area = { m, y1, W - 2 * m, rowH };
        bassLane.setBounds (m, y2, W - 2 * m, H - m - y2);
    }
    for (auto& c : cards) if (! c.area.isEmpty()) layoutCard (c, c.page < 0 ? 96 : kw);
    // MASTER: 5 knobs in one row + the meter
    {
        auto a = master.area.reduced (16, 10);
        a.removeFromTop (32);
        meterArea = a.removeFromRight (14).reduced (0, 4);
        a.removeFromRight (8);
        const int w = a.getWidth() / 5;
        for (int i = 0; i < 5; ++i) master.knobs[(size_t) i]->setBounds (a.getX() + i * w, a.getY() + 20, w, doomsday::flat::Knob::heightFor (44));
    }
    if (! cards[13].area.isEmpty()) sampleNameArea = cards[13].area.reduced (16, 10).removeFromTop (28).withTrimmedLeft (110).withTrimmedRight (140);
    else sampleNameArea = {};
}

void Canvas::paint (Graphics& g)
{
    const auto& t = tokens();
    g.fillAll (t.ground);
    g.setColour (t.divider); g.fillRect (0, 56, W, 1);
    doomsday::brand::drawBadge (g, doomsday::brand::headerBadgeArea (1.f, 28.f), 0.6f, RectanglePlacement::xLeft | RectanglePlacement::yMid);
    const float tx = doomsday::brand::headerTextX (1.f);
    g.setFont (lnf.font (19.f, Bold, 0.06f)); g.setColour (t.text);
    g.drawText ("FORGE OF WRATH", Rectangle<float> (tx, 0.f, 240.f, 56.f), Justification::centredLeft);
    g.setFont (lnf.font (12.f, Medium)); g.setColour (t.textMute);
    g.drawText ("v2.1" + dotSep + "kick synth", Rectangle<float> (tx + 196.f, 0.f, 200.f, 56.f), Justification::centredLeft);
    for (auto& c : cards) if (! c.area.isEmpty()) doomsday::flat::card (g, lnf, c.area, c.title, c.dot);
}

void Canvas::paintOverChildren (Graphics& g)
{
    const auto& t = tokens();
    // output meter (MASTER)
    if (! meterArea.isEmpty())
    {
        auto r = meterArea.toFloat();
        g.setColour (t.control); g.fillRoundedRectangle (r, 4.f);
        const float lv = jlimit (0.f, 1.f, (Decibels::gainToDecibels (meterLevel, -48.f) + 48.f) / 50.f);
        auto fill = r.withTop (r.getBottom() - r.getHeight() * lv);
        g.setColour (meterLevel > 0.99f ? t.hot : t.cool); g.fillRoundedRectangle (fill, 4.f);
    }
    if (! sampleNameArea.isEmpty())
    {
        const String name = proc.sampleName();
        g.setFont (lnf.font (11.5f, Medium));
        g.setColour (name.isEmpty() ? t.textMute : t.hot);
        g.drawText (name.isEmpty() ? "No sample - Load or drop a WAV on the plugin" : name, sampleNameArea, Justification::centredLeft, true);
    }
    if (dragHover)
    {
        g.setColour (t.hot.withAlpha (0.10f)); g.fillRect (getLocalBounds());
        g.setColour (t.text); g.setFont (lnf.font (22.f, Bold));
        g.drawText ("Drop to load the sample", getLocalBounds(), Justification::centred);
    }
}

bool Canvas::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files) if (File (f).hasFileExtension ("wav;aif;aiff;flac")) return true;
    return false;
}
void Canvas::filesDropped (const StringArray& files, int, int)
{
    dragHover = false;
    for (auto& f : files) if (File (f).hasFileExtension ("wav;aif;aiff;flac")) { proc.loadSample (File (f)); break; }
    repaint();
}

void Canvas::timerCallback()
{
    anvil.tick();
    if (page == 2) bassLane.tick();
    tabs->refresh();
    if (proc.currentPresetName() != lastName)
    {
        lastName = proc.currentPresetName();
        int found = 0;
        for (int i = 0; i < presetBox.getNumItems(); ++i) if (presetBox.getItemText (i) == lastName) { found = presetBox.getItemId (i); break; }
        if (found != 0) presetBox.setSelectedId (found, dontSendNotification);
        else presetBox.setText (lastName, dontSendNotification);
    }
    const float pk = proc.getEngine().outPeak.exchange (0.f);
    meterLevel = jmax (pk, meterLevel * 0.85f);
    repaint (meterArea.expanded (2));
    if (proc.getSampleSerial() != lastSampleSerial) { lastSampleSerial = proc.getSampleSerial(); repaint (sampleNameArea.expanded (4)); }
}

// ======================================================================= editor
ForgeEditor::ForgeEditor (ForgeProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (Canvas::W * 6 / 10, Canvas::H * 6 / 10, Canvas::W * 16 / 10, Canvas::H * 16 / 10);
    getConstrainer()->setFixedAspectRatio ((double) Canvas::W / (double) Canvas::H);
    setSize (Canvas::W, Canvas::H);
}

void ForgeEditor::resized()
{
    canvas.setBounds (0, 0, Canvas::W, Canvas::H);
    canvas.setTransform (AffineTransform::scale ((float) getWidth() / (float) Canvas::W));
}
