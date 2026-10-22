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
    const auto prm = proc.gather();
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
    auto prm = proc.gather();
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
    auto& lnf = lnfOf (*this);
    auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff050303)); g.fillRoundedRectangle (b, 9.f);
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
    g.setColour (ForgeTheme::magma.withAlpha (0.25f)); g.drawRoundedRectangle (b.reduced (0.5f), 9.f, 1.f);
}

// ======================================================================= REVERSE BASS lane
void BassLane::tick()
{
    const auto p = proc.gather();
    const float sig = (float) p.rb.stepBeats * 7.f + p.rb.in * 3.f + p.rb.out * 5.f + p.rb.curve * 0.37f + p.rb.riseSt * 0.011f
                    + p.k.lengthMs * 0.001f + (p.rbOn ? 100.f : 0.f) + proc.getEngine().tempoNow.load() * 0.0001f;
    if (std::abs (sig - lastSig) > 1e-5f) { lastSig = sig; repaint(); }
}

void BassLane::paint (Graphics& g)
{
    using namespace ForgeTheme;
    auto& lnf = dynamic_cast<doomsday::ui::StageLookAndFeel&> (getLookAndFeel());
    const auto p = proc.gather();
    const double bpm = proc.getEngine().tempoNow.load(), beatMs = 60000.0 / bpm;
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r, 8.f);
    g.setColour (magma.withAlpha (0.25f)); g.drawRoundedRectangle (r.reduced (0.5f), 8.f, 1.f);
    auto plot = r.reduced (12.f, 10.f).withTrimmedTop (16.f);
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

// ======================================================================= canvas
Canvas::Section& Canvas::section (const String& title, int accent)
{
    sections.push_back ({});
    sections.back().title = title; sections.back().accent = accent;
    return sections.back();
}

Canvas::Canvas (ForgeProcessor& p)
    : proc (p), lnf (ForgeTheme::make()), anvil (p), presets (p, true), bassLane (p)
{
    using namespace ParamIDs;
    setLookAndFeel (&lnf);
    addAndMakeVisible (anvil); addAndMakeVisible (presets); addAndMakeVisible (meter);
    sections.reserve (16);
    auto add = [&] (Section& sec, const char* id, const char* name, bool bip, const char* tip)
    {
        sec.knobs.push_back (std::make_unique<Knob> (p.apvts, id, name, sec.accent, bip));
        sec.knobs.back()->slider.setTooltip (tip);
        addAndMakeVisible (*sec.knobs.back());
    };
    // ---- row A: TAIL | STAGE 1 | MID EQ | STAGE 2
    {
        auto& s = section ("TAIL", 0);
        add (s, tailLevel, "LEVEL", false, "Level of the distorted tail");
        add (s, tailStart, "START", false, "Pitch at the hit = START x the played note. x1 = no drop (zaag kicks), high = hard 'tok'");
        add (s, tailDrop, "DROP", false, "How long the pitch falls from START down to the note");
        add (s, tailHold, "HOLD", false, "The pitch stays up at START this long before it drops (the 'piercing' high hit of uptempo / electro kicks)");
        add (s, tailCurve, "CURVE", false, "Shape of the pitch fall: low = slow glide, high = fast snap then a long settle");
        add (s, tailWave, "WAVE", false, "Oscillator before the distortion: sine > triangle > saw (more buzz)");
        add (s, tune, "TUNE", true, "Tune the whole kick in semitones (the note you play still sets the key)");
        pitchMode = std::make_unique<Choice> (p.apvts, ParamIDs::pitchMode, Choices::pitchModes, 0);
        pitchMode->setTooltip ("CURVE = pitch from the knobs, DRAWN = pitch from the PITCH envelope on the screen");
        addAndMakeVisible (*pitchMode);
    }
    {
        auto& s = section ("STAGE 1", 2);
        distType = std::make_unique<Choice> (p.apvts, ParamIDs::distType, Choices::distTypes, 2, true);
        distType->setTooltip ("Distortion character: SOFT round, TUBE warm and uneven, HARD square, FOLD metallic, RAW buzzing");
        addAndMakeVisible (*distType);
        s.side = distType.get(); s.sideW = 92;
        add (s, drive, "DRIVE", false, "How hard the tail hits the first distortion (the DRIVE envelope moves it over time)");
        add (s, tone, "TONE", true, "Before the distortion: left = more low end, right = more mids (harder, brighter distortion)");
    }
    {
        auto& s = section ("MID EQ", 2);
        add (s, midHz, "FREQ", false, "A bell between the two distortions: shapes what STAGE 2 bites into (honk, body, bite)");
        add (s, midGain, "GAIN", true, "Boost = that range gets distorted harder (more honk), cut = smoother, rounder tail");
        add (s, midQ, "Q", false, "Width of the bell");
    }
    {
        auto& s = section ("STAGE 2", 2);
        dist2Type = std::make_unique<Choice> (p.apvts, ParamIDs::dist2Type, Choices::dist2Types, 2, true);
        dist2Type->setTooltip ("Second distortion after the MID EQ. SOFT / TUBE round and thicken the tail = the modern sound. OFF = one stage only");
        addAndMakeVisible (*dist2Type);
        s.side = dist2Type.get(); s.sideW = 92;
        add (s, drive2, "DRIVE", false, "How hard STAGE 2 is pushed");
    }
    // ---- row B: FILTER | TEXTURE | IMPACT | ROOM | OTT
    {
        auto& s = section ("FILTER", 2);
        add (s, lowCut, "LOW CUT", false, "Removes rumble under the tail");
        add (s, highCut, "HIGH CUT", false, "Darkens the distortion: low = round and warm, high = open and fizzy");
        add (s, body, "BODY", true, "More or less low end (the fundamental of the tail)");
        add (s, cleanSub, "CLEAN SUB", false, "Below this frequency the distortion is replaced by a clean sine: tight, mono-safe sub that never mushes (OFF = all distorted)");
    }
    {
        auto& s = section ("TEXTURE", 2);
        add (s, scream, "SCREAM", false, "A resonant peak pushed into saturation: the 'yell' of raw kicks");
        add (s, screamHz, "SCREAM HZ", false, "Where the scream peak sits");
        add (s, crunch, "CRUNCH", false, "Extra high distortion at the start of the tail (the 'crack')");
        add (s, crunchDec, "CRUNCH DEC", false, "How long the crunch lasts");
        add (s, grit, "GRIT", false, "Noisy texture riding on the tail: the dirty 'sand'");
    }
    {
        auto& s = section ("IMPACT", 1);
        add (s, chaos, "CHAOS", false, "Noise burst pushed INTO the distortion at the start: the dense, messy impact of modern kicks (0 = clean old-school square)");
        add (s, chaosDec, "DECAY", false, "How long the chaos lasts");
        add (s, chaosHz, "TONE", false, "Colour of the chaos: low = dark rumble, high = bright crackle");
    }
    {
        auto& s = section ("ROOM", 0);
        add (s, room, "ROOM", false, "Small room. INTO DIST on: the reflections are distorted with the tail (dense raw / psy tails). Off: space around the kick");
        add (s, roomSize, "SIZE", false, "Room size: small = thick, big = long tail");
        roomInto = std::make_unique<Switch> (p.apvts, ParamIDs::roomInto, "INTO DIST", Switch::Chip, 0);
        roomInto->setTooltip ("ON: the room goes INTO the distortion (modern raw trick). OFF: the room comes after (space)");
        addAndMakeVisible (*roomInto);
    }
    {
        auto& s = section ("OTT", 0);
        add (s, ott, "OTT", false, "3-band upward + downward compression on the whole kick: dense, loud, in your face");
    }
    // ---- row C: PUNCH | SAMPLE | CLICK | MASTER
    {
        auto& s = section ("PUNCH", 1);
        add (s, punchLevel, "LEVEL", false, "Sine punch on top: the body of the hit");
        add (s, punchStart, "START", false, "Punch pitch at the hit");
        add (s, punchEnd, "END", false, "Punch pitch at the end of its drop");
        add (s, punchDrop, "DROP", false, "How fast the punch pitch falls");
        add (s, punchDecay, "DECAY", false, "How long the punch rings");
        add (s, punchDrive, "DRIVE", false, "Distorts the punch: from a clean sine to a hard, modern 'tok'");
        punchOn = std::make_unique<Switch> (p.apvts, ParamIDs::punchOn, "ON", Switch::Power, 1);
        addAndMakeVisible (*punchOn);
    }
    {
        auto& s = section ("SAMPLE", 1);
        add (s, smpLevel, "LEVEL", false, "Level of your own sample (LOAD or drop a WAV here)");
        add (s, smpPitch, "PITCH", true, "Pitch of the sample in semitones (it does not follow the note)");
        add (s, smpStart, "START", false, "Skips the start of the sample");
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
    }
    {
        auto& s = section ("CLICK", 1);
        add (s, clickLevel, "LEVEL", false, "Short noise click at the very start: helps the kick cut through");
        add (s, clickHz, "TONE", false, "Colour of the click");
        add (s, clickLen, "LENGTH", false, "Length of the click");
    }
    {
        auto& s = section ("MASTER", 0);
        add (s, length, "LENGTH", false, "Length of the whole kick: the envelopes stretch with it");
        add (s, clipDrive, "CLIP", false, "Pushes the kick into the master clipper: louder, harder, flatter");
        add (s, clipSoft, "SOFT", false, "Clipper shape: 0 = hard (aggressive), 1 = soft (round)");
        add (s, velocity, "VELOCITY", false, "0 = every hit full level, 1 = MIDI velocity sets the level");
        add (s, output, "OUTPUT", true, "Output level");
        s.sideW = -30;      // room for the meter on the right
    }
    // ---- row D: REVERSE BASS (v2.1, from Damian's dkd)
    {
        auto& s = section ("REVERSE BASS", 1);
        rbPattern = std::make_unique<Choice> (p.apvts, ParamIDs::rbPattern, Choices::rbPatterns, 1, true);
        rbPattern->setTooltip ("How many bass swells per beat: 1/4 = one, 1/8 = two, 1/16 = four (locked to the FL tempo)");
        addAndMakeVisible (*rbPattern);
        s.side = rbPattern.get(); s.sideW = 70;
        add (s, rbIn, "IN", false, "Where in each step the bass starts to swell");
        add (s, rbOut, "OUT", false, "Where it stops - the rest of the step is a clean gap before the next kick");
        add (s, rbCurve, "CURVE", false, "Swell shape: low = rises early and long, high = a late, hard suck-in");
        add (s, rbRise, "RISE", false, "Pitch rise at the end of every swell (like a kick tail played backwards)");
        add (s, rbTune, "TUNE", true, "Bass pitch against the kick note in semitones (12 = an octave up)");
        add (s, rbDrive, "DRIVE", false, "Distortion of the bass: from a clean sub to a buzzing saw");
        add (s, rbTone, "TONE", false, "Low-pass of the bass: low = round and deep, high = bright and present");
        add (s, rbLevel, "LEVEL", false, "Bass level against the kick (both go into the master clipper)");
        rbOn = std::make_unique<Switch> (p.apvts, ParamIDs::rbOn, "ON", Switch::Power, 1);
        rbOn->setTooltip ("Reverse bass: every kick starts one beat of bass swells on the same note");
        addAndMakeVisible (*rbOn);
        addAndMakeVisible (bassLane);
    }
    setSize (W, H);
    timerCallback();
    startTimerHz (30);
}
Canvas::~Canvas() { setLookAndFeel (nullptr); }

void Canvas::resized()
{
    const int m = 20, gap = 14, rowH = 176;
    presets.setBounds (W - m - 300, 34, 300, 90);
    anvil.setBounds (m, 150, W - 2 * m, 250);
    // widths per row (sum + gaps = W - 2m)
    const std::vector<std::vector<int>> rows { { 560, 320, 262, 276 }, { 340, 392, 286, 220, 166 }, { 480, 300, 236, 402 }, { W - 2 * m } };
    size_t si = 0;
    int y = 414;
    for (auto& r : rows)
    {
        int x = m;
        for (int w : r) { if (si < sections.size()) sections[si++].area = { x, y, w, rowH }; x += w + gap; }
        y += rowH + gap;
    }
    for (auto& sec : sections)
    {
        auto a = sec.area.reduced (12, 8).withTrimmedTop (36);
        if (sec.side != nullptr) { sec.side->setBounds (a.removeFromLeft (sec.sideW).withSizeKeepingCentre (sec.sideW, jmin (a.getHeight(), 128))); a.removeFromLeft (8); }
        if (sec.sideW < 0) { meter.setBounds (a.removeFromRight (22).reduced (0, 6)); a.removeFromRight (8); }
        if (sec.title == "SAMPLE") { sampleNameArea = a.removeFromBottom (18); }
        if (sec.title == "REVERSE BASS") { bassLane.setBounds (a.removeFromRight (470).reduced (0, 2)); a.removeFromRight (12); }
        const float cw = (float) a.getWidth() / (float) jmax<size_t> (1, sec.knobs.size());
        for (size_t i = 0; i < sec.knobs.size(); ++i)
            sec.knobs[i]->setBounds (Rectangle<int> (a.getX() + (int) (cw * (float) i), a.getY(), (int) cw, a.getHeight()).withSizeKeepingCentre (jmin ((int) cw - 2, 100), a.getHeight()));
    }
    auto head = [] (const Section& s) { return Rectangle<int> (s.area.getX(), s.area.getY() + 9, s.area.getWidth() - 14, 26); };
    pitchMode->setBounds (head (sections[0]).removeFromRight (150));
    roomInto->setBounds (head (sections[7]).removeFromRight (104));
    punchOn->setBounds (head (sections[9]).removeFromRight (100));
    rbOn->setBounds (head (sections[13]).removeFromRight (100));
    {
        auto h = head (sections[10]);
        clearBtn.setBounds (h.removeFromRight (30));
        h.removeFromRight (6);
        loadBtn.setBounds (h.removeFromRight (90));
    }
    renderBackground();
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
void Canvas::paintOverChildren (Graphics& g)
{
    const String name = proc.sampleName();
    g.setFont (lnf.valueFont (12.f));
    g.setColour (name.isEmpty() ? lnf.theme.dim : ForgeTheme::magmaHot);
    g.drawText (name.isEmpty() ? "NO SAMPLE  -  LOAD OR DROP A WAV" : name, sampleNameArea, Justification::centred, true);
    if (dragHover)
    {
        g.setColour (ForgeTheme::magma.withAlpha (0.12f)); g.fillRect (getLocalBounds());
        g.setColour (ForgeTheme::magmaHot); g.setFont (lnf.titleFont (22.f, 0.2f));
        g.drawText ("DROP TO LOAD THE SAMPLE", getLocalBounds(), Justification::centred);
    }
}

void Canvas::renderBackground()
{
    const float scale = 2.f, ui = 1200.f / (float) W;
    background = Image (Image::RGB, (int) (W * scale), (int) (H * scale), true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (scale));
    using namespace ForgeTheme;
    g.fillAll (lnf.theme.bg);
    {
        // heat rising from below the header + embers
        ColourGradient haze (magma.withAlpha (0.16f), W * 0.5f, 120.f, magma.withAlpha (0.f), W * 0.5f + 560.f, 120.f, true);
        g.setGradientFill (haze); g.fillRect (0, 0, W, 180);
        ColourGradient low (ember.withAlpha (0.10f), W * 0.5f, (float) H + 60.f, ember.withAlpha (0.f), W * 0.5f, (float) H - 360.f, true);
        g.setGradientFill (low); g.fillRect (0, H - 400, W, 400);
        Random r (11);
        for (int i = 0; i < 90; ++i)
        {
            const float x = r.nextFloat() * W, y = 6.f + r.nextFloat() * 130.f, s = 0.8f + r.nextFloat() * 1.8f;
            g.setColour ((r.nextBool() ? magmaHot : magma).withAlpha (0.10f + 0.35f * r.nextFloat() * (1.f - y / 140.f)));
            g.fillEllipse (x, y, s, s);
        }
        ColourGradient vig (Colours::transparentBlack, W * 0.5f, H * 0.45f, Colours::black.withAlpha (0.7f), 0.f, -80.f, true);
        g.setGradientFill (vig); g.fillRect (0, 0, W, H);
        // molten seam under the header
        ColourGradient line (magma.withAlpha (0.f), 0.f, 0.f, magma.withAlpha (0.f), (float) W, 0.f, false);
        line.addColour (0.5, magmaHot.withAlpha (0.9f)); line.addColour (0.3, magma.withAlpha (0.5f)); line.addColour (0.7, magma.withAlpha (0.5f));
        g.setGradientFill (line); g.fillRect (0.f, 136.f, (float) W, 1.2f);
        ColourGradient glow (magma.withAlpha (0.18f), W * 0.5f, 136.f, magma.withAlpha (0.f), W * 0.5f, 150.f, false);
        g.setGradientFill (glow); g.fillRect (0, 128, W, 18);
    }
    // title: forged metal, glowing from inside
    {
        const String title ("FORGE OF WRATH");
        const auto area = Rectangle<float> (0.f, 26.f, (float) W, 60.f);
        g.setFont (lnf.titleFont (40.f, 0.26f));
        for (int i = 5; i >= 1; --i)
        {
            g.setColour (magma.withAlpha (0.09f / (float) i));
            for (auto o : { Point<float> (-1.8f * (float) i, 0.f), Point<float> (1.8f * (float) i, 0.f), Point<float> (0.f, 1.8f * (float) i), Point<float> (0.f, -1.4f * (float) i) })
                g.drawText (title, area.translated (o.x, o.y), Justification::centred);
        }
        g.setColour (Colours::black.withAlpha (0.85f)); g.drawText (title, area.translated (0.f, 2.f), Justification::centred);
        ColourGradient tg (Colour (0xfffff3d6), 0.f, area.getY() + 12.f, ember, 0.f, area.getBottom() - 8.f, false);
        tg.addColour (0.45, magmaHot); tg.addColour (0.7, magma);
        g.setGradientFill (tg); g.drawText (title, area, Justification::centred);
        // anvil + hammer spark mark above the title
        const float cx = W * 0.5f, cy = 20.f;
        Path anvilP;
        anvilP.startNewSubPath (cx - 26.f, cy - 4.f); anvilP.lineTo (cx + 18.f, cy - 4.f); anvilP.lineTo (cx + 30.f, cy - 8.f);
        anvilP.lineTo (cx + 22.f, cy + 1.f); anvilP.lineTo (cx + 8.f, cy + 2.f); anvilP.lineTo (cx + 12.f, cy + 8.f);
        anvilP.lineTo (cx - 12.f, cy + 8.f); anvilP.lineTo (cx - 8.f, cy + 2.f); anvilP.lineTo (cx - 22.f, cy + 1.f); anvilP.closeSubPath();
        g.setColour (magma.withAlpha (0.75f)); g.strokePath (anvilP, PathStrokeType (1.3f, PathStrokeType::mitered));
        for (int i = 0; i < 5; ++i)
        {
            const float a = -2.5f + (float) i * 0.35f;
            g.setColour (magmaHot.withAlpha (0.7f));
            g.drawLine (cx + std::cos (a) * 7.f, cy - 8.f + std::sin (a) * 7.f, cx + std::cos (a) * 14.f, cy - 8.f + std::sin (a) * 14.f, 1.1f);
        }
        g.setFont (lnf.labelFont (12.5f, 0.6f)); g.setColour (magma.withAlpha (0.85f));
        g.drawText ("HARDSTYLE  KICK  SYNTHESIZER", 0, 104, W, 18, Justification::centred);
    }
    const int tx = (int) doomsday::brand::headerTextX (ui);
    g.setFont (lnf.titleFont (10.f, 0.25f)); g.setColour (lnf.theme.text.withAlpha (0.75f));
    g.drawText ("V2.1", tx, 40, 300, 14, Justification::left);
    g.setFont (lnf.labelFont (12.f, 0.28f)); g.setColour (lnf.theme.label);
    g.drawText ("PUNCH  /  TAIL  /  DISTORTION  /  CLIP", tx, 58, 340, 16, Justification::left);
    g.setColour (magma);
    g.drawText ("HAMMERED, NOT MIXED", tx, 76, 320, 16, Justification::left);
    doomsday::brand::drawHeaderBadge (g, ui, 0.55f);
    for (auto& sec : sections) paint::glassPanel (g, lnf, sec.area, sec.title, sec.accent);
}

void Canvas::timerCallback()
{
    anvil.tick();
    bassLane.tick();
    presets.refresh();
    meter.setLevel (proc.getEngine().outPeak.exchange (0.f));
    if (proc.getSampleSerial() != lastSampleSerial) { lastSampleSerial = proc.getSampleSerial(); repaint (sampleNameArea.expanded (4)); }
}

void Canvas::paint (Graphics& g)
{
    if (background.isValid()) g.drawImage (background, getLocalBounds().toFloat(), RectanglePlacement::stretchToFit);
}

ForgeEditor::ForgeEditor (ForgeProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (900, 708, 2250, 1770);
    getConstrainer()->setFixedAspectRatio ((double) Canvas::W / (double) Canvas::H);
    setSize (1200, 944);
}
