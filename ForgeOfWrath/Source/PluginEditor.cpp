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
    const Colour envCol[2] = { ForgeTheme::magmaHot, ForgeTheme::heat };
    const char* envName[2] = { "AMP", "DRIVE" };
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
Rectangle<float> AnvilDisplay::tabBounds (int i) const { return { 18.f + (float) i * 92.f, 10.f, 86.f, 26.f }; }

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
    for (int i = 0; i < 2; ++i) if (tabBounds (i).contains (e.position)) { setMouseCursor (MouseCursor::PointingHandCursor); return; }
    const int h = pointAt (e.position);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? MouseCursor::DraggingHandCursor : plot.expanded (6.f).contains (e.position) ? MouseCursor::CrosshairCursor : MouseCursor::NormalCursor);
}
void AnvilDisplay::mouseDown (const MouseEvent& e)
{
    for (int i = 0; i < 2; ++i) if (tabBounds (i).contains (e.position)) { editing = i; hover = drag = -1; repaint(); return; }
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
    sh.pts[(size_t) drag] = { jlimit (lo, jmax (lo, hi), pt.ms), pt.v };
    shape[editing] = sh;
    proc.setShape (editing, sh);
    repaint();
}
void AnvilDisplay::mouseUp (const MouseEvent&) { drag = -1; }
void AnvilDisplay::mouseDoubleClick (const MouseEvent& e)
{
    const int hit = pointAt (e.position);
    if (hit > 0) { auto sh = shape[editing]; sh.remove (hit); proc.setShape (editing, sh); hover = -1; }
}

void AnvilDisplay::renderPreview()
{
    auto prm = proc.gather();
    shape[0] = proc.getShape (0); shape[1] = proc.getShape (1);
    prm.k.amp = shape[0]; prm.k.drive = shape[1];
    previewMs = prm.k.lengthMs;
    const double rate = 96000.0;
    const int total = (int) (rate * previewMs / 1000.0);
    const int cols = jmax (2, (int) plot.getWidth());
    lo.assign ((size_t) cols, 0.f); hi.assign ((size_t) cols, 0.f); pitch.assign ((size_t) cols, 0.f);
    doomsday::kick::KickVoice v;
    v.prepare (rate);
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
        pitch[(size_t) c] = endHz * (1.f + (prm.k.startRatio - 1.f) * std::exp (-ms / jmax (1.f, prm.k.dropMs) * prm.k.curve));
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
    for (int i = 0; i < 2; ++i)
    {
        auto r = tabBounds (i);
        const bool on = editing == i;
        g.setColour (on ? envCol[i].withAlpha (0.16f) : Colours::white.withAlpha (0.03f)); g.fillRoundedRectangle (r, 6.f);
        g.setColour (on ? envCol[i].withAlpha (0.85f) : Colours::white.withAlpha (0.12f)); g.drawRoundedRectangle (r.reduced (0.5f), 6.f, on ? 1.4f : 1.f);
        g.setFont (lnf.titleFont (11.f, 0.25f)); g.setColour (on ? lnf.theme.text : lnf.theme.dim);
        g.drawText (envName[i], r, Justification::centred);
    }
    g.setFont (lnf.labelFont (11.5f, 0.15f)); g.setColour (lnf.theme.dim);
    g.drawText (editing == 0 ? "AMP ENVELOPE  -  volume of the tail over the kick" : "DRIVE ENVELOPE  -  how hard the distortion is pushed over time (swell it up for a reverse bass)",
                Rectangle<float> (tabBounds (1).getRight() + 16.f, 10.f, 760.f, 26.f), Justification::centredLeft);
    g.drawText ("CLICK: ADD DOT    DRAG: MOVE    DOUBLE / RIGHT CLICK: DELETE", Rectangle<float> (b.getRight() - 520.f, 10.f, 500.f, 26.f), Justification::centredRight);

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
    const int other = 1 - editing;
    g.setColour (envCol[other].withAlpha (0.28f)); g.strokePath (envPath (shape[other]), PathStrokeType (1.2f));
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
                g.drawText (String ((int) ms) + " ms  /  " + String ((int) std::round (sh.pts[(size_t) i].v * 100.f)) + " %",
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

// ======================================================================= canvas
Canvas::Canvas (ForgeProcessor& p)
    : proc (p), lnf (ForgeTheme::make()), anvil (p), presets (p, true)
{
    using namespace ParamIDs;
    setLookAndFeel (&lnf);
    addAndMakeVisible (anvil); addAndMakeVisible (presets); addAndMakeVisible (meter);
    auto add = [&] (std::vector<std::unique_ptr<Knob>>& into, const char* id, const char* name, int acc, bool bip, const char* tip)
    {
        into.push_back (std::make_unique<Knob> (p.apvts, id, name, acc, bip));
        into.back()->slider.setTooltip (tip);
        addAndMakeVisible (*into.back());
    };
    add (tailK, tailLevel, "LEVEL", 0, false, "Level of the distorted tail");
    add (tailK, tailStart, "START", 0, false, "Pitch at the hit = START x the played note. Higher = harder, more 'tok'");
    add (tailK, tailDrop, "DROP", 0, false, "How long the pitch falls from START down to the note");
    add (tailK, tailCurve, "CURVE", 0, false, "Shape of the pitch fall: low = slow glide, high = fast snap then a long settle");
    add (tailK, tailWave, "WAVE", 0, false, "Oscillator before the distortion: sine > triangle > saw (more buzz)");
    add (tailK, tune, "TUNE", 0, true, "Tune the whole kick in semitones (the note you play still sets the key)");
    distType = std::make_unique<Choice> (p.apvts, ParamIDs::distType, Choices::distTypes, 2, true);
    distType->setTooltip ("Distortion character: SOFT round, TUBE warm and uneven, HARD square, FOLD metallic, RAW buzzing (raw kicks)");
    addAndMakeVisible (*distType);
    add (distK, drive, "DRIVE", 2, false, "How hard the tail hits the distortion (the DRIVE envelope moves it over time)");
    add (distK, tone, "TONE", 2, true, "Before the distortion: left = more low end, right = more mids (harder, brighter distortion)");
    add (distK, lowCut, "LOW CUT", 2, false, "Removes rumble under the tail after the distortion");
    add (distK, highCut, "HIGH CUT", 2, false, "Darkens the distortion: low = muffled, high = open and fizzy");
    add (distK, scream, "SCREAM", 2, false, "A resonant peak fed back into the distortion: the 'yell' of raw kicks");
    add (distK, screamHz, "SCREAM HZ", 2, false, "Where the scream peak sits");
    add (distK, crunch, "CRUNCH", 2, false, "Extra high distortion at the start of the tail (the 'crack' of raw kicks)");
    add (distK, crunchDec, "CRUNCH DEC", 2, false, "How long the crunch lasts");
    add (distK, body, "BODY", 2, true, "After the distortion: more or less low end (the fundamental of the tail)");
    add (distK, grit, "GRIT", 2, false, "Noisy texture riding on the tail: the dirty 'sand' of raw kicks");
    punchOn = std::make_unique<Switch> (p.apvts, ParamIDs::punchOn, "ON", Switch::Power, 1);
    addAndMakeVisible (*punchOn);
    add (punchK, punchLevel, "LEVEL", 1, false, "Clean sine punch on top: the body of the hit");
    add (punchK, punchStart, "START", 1, false, "Punch pitch at the hit");
    add (punchK, punchEnd, "END", 1, false, "Punch pitch at the end of its drop");
    add (punchK, punchDrop, "DROP", 1, false, "How fast the punch pitch falls");
    add (punchK, punchDecay, "DECAY", 1, false, "How long the punch rings");
    add (clickK, clickLevel, "LEVEL", 1, false, "Short noise click at the very start: helps the kick cut through");
    add (clickK, clickHz, "TONE", 1, false, "Colour of the click");
    add (clickK, clickLen, "LENGTH", 1, false, "Length of the click");
    add (masterK, length, "LENGTH", 0, false, "Length of the whole kick: the envelopes stretch with it");
    add (masterK, clipDrive, "CLIP", 0, false, "Pushes the kick into the master clipper: louder, harder, flatter");
    add (masterK, clipSoft, "SOFT", 0, false, "Clipper shape: 0 = hard (aggressive), 1 = soft (round)");
    add (masterK, velocity, "VELOCITY", 0, false, "0 = every hit full level, 1 = MIDI velocity sets the level");
    add (masterK, output, "OUTPUT", 0, true, "Output level");
    setSize (W, H);
    timerCallback();
    startTimerHz (30);
}
Canvas::~Canvas() { setLookAndFeel (nullptr); }

void Canvas::resized()
{
    panels.clear();
    const int m = 20, gap = 14;
    presets.setBounds (W - m - 300, 34, 300, 90);
    anvil.setBounds (m, 150, W - 2 * m, 300);
    auto row = [&] (std::vector<std::unique_ptr<Knob>>& ks, Rectangle<int> area)
    {
        const float cw = (float) area.getWidth() / (float) ks.size();
        for (size_t i = 0; i < ks.size(); ++i)
            ks[i]->setBounds (Rectangle<int> (area.getX() + (int) (cw * (float) i), area.getY(), (int) cw, area.getHeight()).withSizeKeepingCentre (jmin ((int) cw - 2, 100), area.getHeight()));
    };
    const int y2 = 464, h2 = 200, y3 = y2 + h2 + gap, h3 = H - m - y3;
    const Rectangle<int> tailA (m, y2, 440, h2), distA (tailA.getRight() + gap, y2, W - m - tailA.getRight() - gap, h2);
    const Rectangle<int> punchA (m, y3, 560, h3), clickA (punchA.getRight() + gap, y3, 330, h3), mastA (clickA.getRight() + gap, y3, W - m - clickA.getRight() - gap, h3);
    panels.push_back ({ "TAIL", tailA }); panels.push_back ({ "DISTORTION", distA });
    panels.push_back ({ "PUNCH", punchA }); panels.push_back ({ "CLICK", clickA }); panels.push_back ({ "MASTER", mastA });
    auto inner = [] (Rectangle<int> a) { return a.reduced (12, 10).withTrimmedTop (34); };
    row (tailK, inner (tailA));
    {
        auto a = inner (distA);
        distType->setBounds (a.removeFromLeft (96).withTrimmedTop (20).withSizeKeepingCentre (96, 128));
        a.removeFromLeft (10);
        row (distK, a);
    }
    punchOn->setBounds (punchA.getRight() - 16 - 110, punchA.getY() + 10, 110, 26);
    row (punchK, inner (punchA));
    row (clickK, inner (clickA));
    {
        auto a = inner (mastA);
        meter.setBounds (a.removeFromRight (22).reduced (0, 8));
        a.removeFromRight (8);
        row (masterK, a);
    }
    renderBackground();
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
    g.drawText ("V1.0", tx, 40, 300, 14, Justification::left);
    g.setFont (lnf.labelFont (12.f, 0.28f)); g.setColour (lnf.theme.label);
    g.drawText ("PUNCH  /  TAIL  /  DISTORTION  /  CLIP", tx, 58, 340, 16, Justification::left);
    g.setColour (magma);
    g.drawText ("HAMMERED, NOT MIXED", tx, 76, 320, 16, Justification::left);
    doomsday::brand::drawHeaderBadge (g, ui, 0.55f);
    for (size_t i = 0; i < panels.size(); ++i) paint::glassPanel (g, lnf, panels[i].second, panels[i].first, i == 1 ? 2 : (i == 2 || i == 3) ? 1 : 0);
    {
        auto a = panels[1].second.reduced (12, 10).withTrimmedTop (34);
        g.setFont (lnf.labelFont (11.f, 0.3f)); g.setColour (lnf.theme.label);
        g.drawText ("TYPE", a.getX(), a.getY() - 2, 96, 14, Justification::centred);
    }
}

void Canvas::timerCallback()
{
    anvil.tick();
    presets.refresh();
    meter.setLevel (proc.getEngine().outPeak.exchange (0.f));
}

void Canvas::paint (Graphics& g)
{
    if (background.isValid()) g.drawImage (background, getLocalBounds().toFloat(), RectanglePlacement::stretchToFit);
}

ForgeEditor::ForgeEditor (ForgeProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (900, 540, 2250, 1350);
    getConstrainer()->setFixedAspectRatio ((double) Canvas::W / (double) Canvas::H);
    setSize (1200, 720);
}
