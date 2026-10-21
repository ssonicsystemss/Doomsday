#include "PluginEditor.h"
#include "../Shared/Source/DoomsdayBrand.h"

using namespace juce;
using namespace doomsday::flat;

Colour oscAccent (int o) { return o == 0 ? tokens().hot : tokens().cool; }

namespace
{
    constexpr float minHz = 20.f, maxHz = 20000.f, minDb = -36.f, maxDb = 30.f;
    const Colour neutral { 0xffb4bac3 };   // global sections: calm, the colour belongs to the oscillators
    const String dot (CharPointer_UTF8 (" \xc2\xb7 "));   // middle dot
    float sq (float x) { return x * x; }
}

// ======================================================================= ResponseGraph
ResponseGraph::ResponseGraph (ProphecyProcessor& p) : proc (p)
{
    setTooltip ("Drag the dots: CUTOFF (left/right = frequency, up/down = resonance) and FORMANT "
                "(left/right = frequency, up/down = gain, mouse wheel = Q). Double-click a dot = default.");
}

RangedAudioParameter* ResponseGraph::param (const char* base) const { return proc.apvts.getParameter (ParamIDs::osc (osc, base)); }

void ResponseGraph::setParam (const char* base, float value)
{
    if (auto* p = param (base)) p->setValueNotifyingHost (p->convertTo0to1 (value));
}

ResponseGraph::OscView ResponseGraph::view (int o) const
{
    using namespace ParamIDs;
    auto v = [this, o] (const char* base) { return proc.apvts.getRawParameterValue (ParamIDs::osc (o, base))->load(); };
    return { v (on) > 0.5f, v (filtOn) > 0.5f, v (formOn) > 0.5f, (int) std::round (v (ftype)),
             v (cutoff), v (reso), v (formHz), v (formGain), v (formQ) };
}

// the engine's filters as analog prototypes (s = j f / fc), same k as ScreechEngine
float ResponseGraph::responseDb (const OscView& v, float hz) const
{
    float mag = 1.f;
    if (v.filtOn)
    {
        const float w = hz / jmax (20.f, v.cutoff);
        const float k = 2.f - 1.96f * v.reso;
        const float den = std::sqrt (sq (1.f - w * w) + sq (k * w));
        switch (v.ftype)
        {
            case 0:  mag = 1.f / den; break;                           // LP 12
            case 1:  mag = 1.f / (den * den); break;                   // LP 24
            case 2:  mag = (0.6f + 0.5f * k) * w / den; break;         // BAND
            default: mag = w * w / den; break;                         // HIGH
        }
    }
    if (v.formOn)
    {
        const float w = hz / jmax (30.f, v.formHz);
        const float k = 1.f / jmax (0.3f, v.formQ);
        const float G = Decibels::decibelsToGain (v.formGain);
        const float num = std::sqrt (sq (1.f - w * w) + sq (G * k * w));
        const float den = std::sqrt (sq (1.f - w * w) + sq (k * w));
        mag *= num / den / (1.f + (G - 1.f) * 0.18f);
    }
    return Decibels::gainToDecibels (mag, -100.f);
}

float ResponseGraph::xOfHz (float hz) const { return plot.getX() + plot.getWidth() * std::log (hz / minHz) / std::log (maxHz / minHz); }
float ResponseGraph::hzOfX (float x) const { return minHz * std::pow (maxHz / minHz, jlimit (0.f, 1.f, (x - plot.getX()) / plot.getWidth())); }
float ResponseGraph::yOfDb (float db) const { return jmap (jlimit (minDb, maxDb, db), maxDb, minDb, plot.getY(), plot.getBottom()); }

Point<float> ResponseGraph::nodePos (int o, Node n) const
{
    const auto v = view (o);
    const float hz = n == Cut ? v.cutoff : v.formHz;
    const float x = xOfHz (jlimit (minHz, maxHz, hz));
    const bool active = n == Cut ? v.filtOn : v.formOn;
    return { x, active ? yOfDb (responseDb (v, hz)) : yOfDb (0.f) };
}

ResponseGraph::Node ResponseGraph::nodeAt (Point<float> p) const
{
    Node best = None;
    float bestD = 16.f;
    for (auto n : { Form, Cut })
    {
        const float d = nodePos (osc, n).getDistanceFrom (p);
        if (d < bestD) { bestD = d; best = n; }
    }
    return best;
}

Rectangle<float> ResponseGraph::chipBounds (int o) const { return { 16.f + 68.f * (float) o, 12.f, 62.f, 26.f }; }

void ResponseGraph::selectOsc (int o)
{
    if (o == osc) return;
    osc = o;
    repaint();
}

void ResponseGraph::resized()
{
    plot = getLocalBounds().toFloat().withTrimmedTop (48.f).withTrimmedBottom (24.f).reduced (16.f, 0.f);
}

void ResponseGraph::tick()
{
    // spectrum of the latest output
    const int w = proc.scopeWritePos.load (std::memory_order_acquire);
    bool changed = false;
    if (w != lastWrite)
    {
        lastWrite = w;
        constexpr int N = ProphecyProcessor::scopeSize;
        for (int i = 0; i < fftSize; ++i)
            fftData[(size_t) i] = proc.scopeBuffer[(size_t) ((w - fftSize + i + N) & (N - 1))];
        std::fill (fftData.begin() + fftSize, fftData.end(), 0.f);
        window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());
        const double sr = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
        for (int b = 0; b < numBars; ++b)
        {
            const float f0 = minHz * std::pow (maxHz / minHz, (float) b / numBars);
            const float f1 = minHz * std::pow (maxHz / minHz, (float) (b + 1) / numBars);
            int i0 = (int) (f0 / sr * fftSize), i1 = jmax (i0 + 1, (int) (f1 / sr * fftSize));
            float m = 0.f;
            for (int i = i0; i <= i1 && i < fftSize / 2; ++i) m = jmax (m, fftData[(size_t) i]);
            const float db = Decibels::gainToDecibels (m / (fftSize * 0.25f), -100.f);
            const float target = jlimit (0.f, 1.f, (db + 84.f) / 84.f);
            auto& bar = bars[(size_t) b];
            bar = target > bar ? target : bar * 0.86f + target * 0.14f;
        }
        changed = true;
    }
    else
    {
        float sum = 0.f;
        for (auto& b : bars) { b *= 0.86f; sum += b; }
        changed = sum > 0.01f;
    }

    // parameters shown by the curves
    String key;
    for (int o = 0; o < 2; ++o)
    {
        const auto v = view (o);
        key << (int) v.on << (int) v.filtOn << (int) v.formOn << v.ftype << ' ' << v.cutoff << ' ' << v.reso << ' '
            << v.formHz << ' ' << v.formGain << ' ' << v.formQ << ';';
    }
    if (key != lastKey) { lastKey = key; changed = true; }
    if (changed) repaint();
}

void ResponseGraph::paint (Graphics& g)
{
    const auto& t = tokens();
    auto& lnf = lnfOf (*this);
    g.setColour (t.surface);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 12.f);

    // grid
    g.setFont (lnf.font (10.5f, Medium));
    for (float f : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
    {
        const float x = xOfHz (f);
        g.setColour (t.divider);
        g.fillRect (x, plot.getY(), 1.f, plot.getHeight());
        g.setColour (t.textMute);
        const String txt = f >= 1000.f ? String ((int) (f / 1000.f)) + "k" : String ((int) f);
        g.drawText (txt, Rectangle<float> (x - 20.f, plot.getBottom() + 4.f, 40.f, 16.f), Justification::centred);
    }
    for (float db : { -24.f, -12.f, 0.f, 12.f, 24.f })
    {
        const float y = yOfDb (db);
        g.setColour (db == 0.f ? t.border : t.divider);
        g.fillRect (plot.getX(), y, plot.getWidth(), 1.f);
        g.setColour (t.textMute);
        g.drawText ((db > 0 ? "+" : "") + String ((int) db), Rectangle<float> (plot.getRight() - 30.f, y - 15.f, 28.f, 14.f), Justification::centredRight);
    }

    // spectrum bars (output), behind everything
    {
        const float bw = plot.getWidth() / (float) numBars;
        g.setColour (Colour (0xff252a31));
        for (int b = 0; b < numBars; ++b)
        {
            const float h = bars[(size_t) b] * plot.getHeight() * 0.7f;
            if (h < 1.f) continue;
            g.fillRoundedRectangle (plot.getX() + bw * (float) b + 1.f, plot.getBottom() - h, jmax (1.f, bw - 2.f), h, 1.5f);
        }
    }

    // curves: the other oscillator thin and dim, the selected one with a soft fill
    {
    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (plot.toNearestInt().expanded (0, 2));
    for (int pass = 0; pass < 2; ++pass)
    {
        const int o = pass == 0 ? 1 - osc : osc;
        const auto v = view (o);
        if (! v.on) continue;
        Path curve;
        const int steps = 240;
        for (int i = 0; i <= steps; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) steps;
            const float y = yOfDb (responseDb (v, hzOfX (x)));
            if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }
        const Colour c = oscAccent (o);
        if (pass == 1)
        {
            Path fill (curve);
            fill.lineTo (plot.getRight(), plot.getBottom());
            fill.lineTo (plot.getX(), plot.getBottom());
            fill.closeSubPath();
            g.setColour (c.withAlpha (0.10f));
            g.fillPath (fill);
        }
        g.setColour (pass == 1 ? c : c.withAlpha (0.45f));
        g.strokePath (curve, PathStrokeType (pass == 1 ? 2.5f : 1.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // nodes of the selected oscillator
    const auto v = view (osc);
    struct Tag { Node n; Point<float> p; bool active, hot; String text; };
    std::vector<Tag> tags;
    for (auto n : { Cut, Form })
    {
        const auto p = nodePos (osc, n);
        const bool active = n == Cut ? v.filtOn : v.formOn;
        const bool hot = n == hover || n == drag;
        const float r = hot ? 9.f : 7.5f;
        g.setColour (t.ground);
        g.fillEllipse (Rectangle<float> (r * 2.f, r * 2.f).withCentre (p));
        g.setColour (active ? Colours::white : t.textMute);
        g.drawEllipse (Rectangle<float> (r * 2.f, r * 2.f).withCentre (p), 2.f);
        if (hot) { g.setColour (oscAccent (osc)); g.fillEllipse (Rectangle<float> (6.f, 6.f).withCentre (p)); }

        auto fmtHz = [] (float hz) { return hz >= 1000.f ? String (hz / 1000.f, 2) + " kHz" : String (roundToInt (hz)) + " Hz"; };
        String text;
        if (n == Cut)
            text = active ? "CUTOFF  " + fmtHz (v.cutoff) + dot + "reso " + String (roundToInt (v.reso * 100.f)) + "%" : "FILTER off" + dot + "drag to use";
        else
            text = active ? "FORMANT  " + fmtHz (v.formHz) + dot + "+" + String (v.formGain, 1) + " dB" + dot + "Q " + String (v.formQ, 1)
                          : "FORMANT off" + dot + "drag to use";
        if (hot || drag == None) tags.push_back ({ n, p, active, hot, text });
    }

    // value tags: the touched dot first, then switched-on ones; a tag that would cover another moves below its dot or is left out
    std::stable_sort (tags.begin(), tags.end(), [] (const Tag& a, const Tag& b) { return (a.hot ? 2 : a.active ? 1 : 0) > (b.hot ? 2 : b.active ? 1 : 0); });
    std::vector<Rectangle<float>> used;
    const auto f = lnf.font (11.5f, SemiBold);
    for (auto& tg : tags)
    {
        const float tw = GlyphArrangement::getStringWidth (f, tg.text) + 16.f;
        auto place = [&] (bool below)
        {
            auto box = Rectangle<float> (tw, 22.f).withPosition (tg.p.x + 14.f, below ? tg.p.y + 12.f : tg.p.y - 30.f);
            if (box.getRight() > plot.getRight()) box.setX (tg.p.x - 14.f - tw);
            if (box.getY() < plot.getY()) box.setY (tg.p.y + 12.f);
            if (box.getBottom() > plot.getBottom()) box.setY (tg.p.y - 30.f);
            return box;
        };
        auto clash = [&used] (Rectangle<float> b) { for (auto& u : used) if (u.expanded (4.f).intersects (b)) return true; return false; };
        auto box = place (false);
        if (clash (box)) box = place (true);
        if (clash (box)) continue;
        used.push_back (box);
        g.setColour (tg.hot ? t.border : t.control.withAlpha (0.92f));
        g.fillRoundedRectangle (box, 6.f);
        g.setFont (f);
        g.setColour (tg.active ? t.text : t.textDim);
        g.drawText (tg.text, box, Justification::centred);
    }
    }

    // OSC chips + hint
    for (int o = 0; o < 2; ++o)
    {
        auto c = chipBounds (o);
        const bool sel = o == osc;
        g.setColour (sel ? t.border : t.control);
        g.fillRoundedRectangle (c, 7.f);
        g.setColour (oscAccent (o).withAlpha (view (o).on ? 1.f : 0.35f));
        g.fillEllipse (c.getX() + 10.f, c.getCentreY() - 3.f, 6.f, 6.f);
        g.setFont (lnf.font (12.f, sel ? Bold : SemiBold));
        g.setColour (sel ? t.text : t.textDim);
        g.drawText ("OSC " + String (o + 1), c.withTrimmedLeft (20.f), Justification::centredLeft);
    }
    g.setFont (lnf.font (11.5f, Medium));
    g.setColour (t.textMute);
    g.drawText ("Filter + formant response " + dot + " drag the dots", Rectangle<float> (160.f, 12.f, (float) getWidth() - 176.f, 26.f), Justification::centredRight);
}

void ResponseGraph::mouseMove (const MouseEvent& e)
{
    const auto n = nodeAt (e.position);
    bool onChip = chipBounds (0).contains (e.position) || chipBounds (1).contains (e.position);
    setMouseCursor (n != None ? MouseCursor::DraggingHandCursor : (onChip ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor));
    if (n != hover) { hover = n; repaint(); }
}

void ResponseGraph::mouseExit (const MouseEvent&) { if (hover != None) { hover = None; repaint(); } }

void ResponseGraph::mouseDown (const MouseEvent& e)
{
    for (int o = 0; o < 2; ++o)
        if (chipBounds (o).contains (e.position)) { selectOsc (o); if (onNodeTouched) onNodeTouched (o, -1); return; }

    drag = nodeAt (e.position);
    if (drag == None) return;
    using namespace ParamIDs;
    lastDrag = e.position;
    const bool cut = drag == Cut;
    for (auto* b : cut ? std::initializer_list<const char*> { cutoff, reso, filtOn } : std::initializer_list<const char*> { formHz, formGain, formOn })
        if (auto* p = param (b)) p->beginChangeGesture();
    setParam (cut ? filtOn : formOn, 1.f);       // dragging a node switches its section on
    if (onNodeTouched) onNodeTouched (osc, cut ? 2 : 4);
    repaint();
}

void ResponseGraph::mouseDrag (const MouseEvent& e)
{
    if (drag == None) return;
    using namespace ParamIDs;
    const float hz = hzOfX (jlimit (plot.getX(), plot.getRight(), e.position.x));
    const float dy = e.position.y - lastDrag.y;
    lastDrag = e.position;
    const auto v = view (osc);
    if (drag == Cut)
    {
        setParam (cutoff, jlimit (20.f, 20000.f, hz));
        setParam (reso, jlimit (0.f, 1.f, v.reso - dy / 220.f));
    }
    else
    {
        setParam (formHz, jlimit (100.f, 8000.f, hz));
        setParam (formGain, jlimit (0.f, 30.f, v.formGain - dy * 30.f / 220.f));
    }
    repaint();
}

void ResponseGraph::mouseUp (const MouseEvent&)
{
    if (drag == None) return;
    using namespace ParamIDs;
    for (auto* b : drag == Cut ? std::initializer_list<const char*> { cutoff, reso, filtOn } : std::initializer_list<const char*> { formHz, formGain, formOn })
        if (auto* p = param (b)) p->endChangeGesture();
    drag = None;
    repaint();
}

void ResponseGraph::mouseDoubleClick (const MouseEvent& e)
{
    using namespace ParamIDs;
    const auto n = nodeAt (e.position);
    if (n == None) return;
    for (auto* b : n == Cut ? std::initializer_list<const char*> { cutoff, reso } : std::initializer_list<const char*> { formHz, formGain, formQ })
        if (auto* p = param (b)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
}

void ResponseGraph::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wd)
{
    using namespace ParamIDs;
    const auto n = nodeAt (e.position);
    if (n == None) return;
    const float d = wd.deltaY * (wd.isReversed ? -1.f : 1.f);
    auto* p = param (n == Cut ? reso : formQ);
    if (p == nullptr) return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (jlimit (0.f, 1.f, p->getValue() + d * 0.12f));
    p->endChangeGesture();
}

// ======================================================================= Scope / Meter
void Scope::paint (Graphics& g)
{
    const auto& t = tokens();
    auto r = getLocalBounds().toFloat();
    g.setColour (t.ground.withAlpha (0.6f));
    g.fillRoundedRectangle (r, 8.f);
    g.setColour (t.divider);
    g.fillRect (r.getX() + 6.f, r.getCentreY(), r.getWidth() - 12.f, 1.f);

    constexpr int N = ProphecyProcessor::scopeSize;
    constexpr int show = 420;
    std::array<float, N> local;
    const int w = proc.scopeWritePos.load (std::memory_order_acquire);
    for (int i = 0; i < N; ++i) local[(size_t) i] = proc.scopeBuffer[(size_t) ((w + i) & (N - 1))];
    int start = N - show - 1;
    for (int i = N - show - 1; i > N - show - 1 - 1500 && i > 1; --i)
        if (local[(size_t) i - 1] < 0.f && local[(size_t) i] >= 0.f) { start = i; break; }

    Path wave;
    const float midY = r.getCentreY(), amp = r.getHeight() * 0.42f;
    for (int i = 0; i < show; ++i)
    {
        const float xx = r.getX() + 6.f + (r.getWidth() - 12.f) * (float) i / (float) (show - 1);
        const float yy = midY - jlimit (-1.f, 1.f, local[(size_t) (start + i)]) * amp;
        if (i == 0) wave.startNewSubPath (xx, yy); else wave.lineTo (xx, yy);
    }
    g.setColour (t.hot);
    g.strokePath (wave, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
}

void Meter::setLevel (float peak)
{
    const float db = Decibels::gainToDecibels (peak, -60.f);
    const float v = jlimit (0.f, 1.f, (db + 60.f) / 60.f);
    const float nl = v > level ? v : level * 0.88f + v * 0.12f;
    if (v >= hold) { hold = v; holdCount = 40; }
    else if (--holdCount < 0) hold = jmax (0.f, hold - 0.01f);
    if (std::abs (nl - level) > 0.002f || holdCount >= 0) { level = nl; repaint(); }
}

void Meter::paint (Graphics& g)
{
    const auto& t = tokens();
    auto r = getLocalBounds().toFloat();
    g.setColour (t.border);
    g.fillRoundedRectangle (r, r.getWidth() * 0.5f);
    auto fill = r.withTop (r.getBottom() - r.getHeight() * level);
    g.setColour (level > 0.99f ? t.hot : neutral);
    g.fillRoundedRectangle (fill, r.getWidth() * 0.5f);
    if (hold > 0.02f)
    {
        g.setColour (hold > 0.99f ? t.hot : t.text);
        g.fillRect (r.getX(), r.getBottom() - r.getHeight() * hold, r.getWidth(), 2.f);
    }
}

// ======================================================================= OscCard
void OscCard::Page::resized()
{
    auto r = getLocalBounds();
    if (seg != nullptr || chip != nullptr)
    {
        auto top = r.removeFromTop (30);
        if (seg != nullptr) seg->setBounds (top.removeFromLeft (jmin (top.getWidth() - 110, 340)));
        if (chip != nullptr) chip->setBounds (top.removeFromRight (84));
        r.removeFromTop (6);
    }
    const int n = (int) knobs.size();
    if (n == 0) return;
    const int rows = (n + columns - 1) / columns;
    const int kh = FlatKnob::heightFor (44);
    const int totalH = rows * kh + (rows - 1) * 8;
    r = r.withSizeKeepingCentre (r.getWidth(), jmin (r.getHeight(), totalH));
    const float cw = (float) r.getWidth() / (float) columns;
    for (int i = 0; i < n; ++i)
    {
        const int row = i / columns, col = i % columns;
        auto cell = Rectangle<int> (r.getX() + (int) (cw * (float) col), r.getY() + row * (kh + 8), (int) cw, kh);
        knobs[(size_t) i]->setBounds (cell);
    }
    if (overlay != nullptr) overlay->setBounds (knobs[0]->getBounds());
}

OscCard::OscCard (ProphecyProcessor& p, int o)
    : proc (p), osc (o), tabs ({ "OSC", "DIST", "FILTER", "LFO", "FORMANT" }, oscAccent (o))
{
    using namespace ParamIDs;
    const Colour ac = oscAccent (o);
    auto id = [o] (const char* base) { return ParamIDs::osc (o, base); };
    auto knob = [&p, &id, ac] (const char* base, const char* name, bool bip = false)
    { return std::make_unique<FlatKnob> (p.apvts, id (base), name, ac, bip); };

    // OSC
    for (auto [b, n, bip] : std::initializer_list<std::tuple<const char*, const char*, bool>> {
             { wave, "Wave", false }, { unison, "Unison", false }, { detune, "Detune", false }, { width, "Width", false },
             { fm, "FM", false }, { fmRatio, "Ratio", false }, { oct, "Octave", true }, { semi, "Semi", true },
             { fine, "Fine", true }, { penv, "Pitch env", true }, { pdecay, "P. decay", false }, { level, "Level", false } })
        pages[0].knobs.push_back (knob (b, n, bip));

    // DIST
    pages[1].seg = std::make_unique<Segmented> (p.apvts, id (dtype), StringArray { "Hard", "Tube", "Fold", "Scream" }, ac);
    pages[1].knobs.push_back (knob (drive, "Drive"));
    pages[1].knobs.push_back (knob (bite, "Bite"));
    pages[1].knobs.push_back (knob (noise, "Noise"));
    pages[1].knobs.push_back (knob (dmix, "Mix"));

    // FILTER
    pages[2].seg = std::make_unique<Segmented> (p.apvts, id (ftype), StringArray { "LP 12", "LP 24", "Band", "High" }, ac);
    pages[2].knobs.push_back (knob (cutoff, "Cutoff"));
    pages[2].knobs.push_back (knob (reso, "Reso"));
    pages[2].knobs.push_back (knob (fenv, "Env", true));
    pages[2].knobs.push_back (knob (fdecay, "Decay"));
    pages[2].knobs.push_back (knob (keytrack, "Key"));

    // LFO (RATE: synced division or free Hz in the same slot)
    pages[3].seg = std::make_unique<Segmented> (p.apvts, id (lshape), StringArray { "Sine", "Tri", "Saw", "Square", "S&H" }, ac);
    pages[3].chip = std::make_unique<FlatToggle> (p.apvts, id (lsync), "Sync", FlatToggle::Chip, ac);
    pages[3].knobs.push_back (knob (lfoDiv, "Rate"));
    pages[3].overlay = knob (lrate, "Rate");
    pages[3].knobs.push_back (knob (lcut, "> Cutoff", true));
    pages[3].knobs.push_back (knob (lpitch, "> Pitch", true));
    pages[3].knobs.push_back (knob (lamp, "> Amp"));
    divKnob = dynamic_cast<FlatKnob*> (pages[3].knobs[0].get());
    rateKnob = dynamic_cast<FlatKnob*> (pages[3].overlay.get());

    // FORMANT
    pages[4].knobs.push_back (knob (formHz, "Freq"));
    pages[4].knobs.push_back (knob (formGain, "Gain"));
    pages[4].knobs.push_back (knob (formQ, "Q"));
    pages[4].knobs.push_back (knob (formEnv, "Env", true));
    pages[4].knobs.push_back (knob (formDec, "Decay"));
    pages[4].knobs.push_back (knob (formKey, "Key"));
    pages[4].knobs.push_back (knob (formLfo, "LFO", true));

    const char* onIds[] = { on, distOn, filtOn, lfoOn, formOn };
    const char* onText[] = { "On", "Active", "Active", "Active", "Active" };
    for (int i = 0; i < numTabs; ++i)
    {
        auto& pg = pages[(size_t) i];
        pg.columns = i == 0 ? 6 : (int) pg.knobs.size();
        if (pg.seg) pg.addAndMakeVisible (*pg.seg);
        if (pg.chip) pg.addAndMakeVisible (*pg.chip);
        for (auto& k : pg.knobs) pg.addAndMakeVisible (*k);
        if (pg.overlay) pg.addChildComponent (*pg.overlay);
        addChildComponent (pg);
        powers[(size_t) i] = std::make_unique<FlatToggle> (p.apvts, id (onIds[i]), onText[i], FlatToggle::Pill, ac);
        addChildComponent (*powers[(size_t) i]);
        onFlags[(size_t) i] = p.apvts.getRawParameterValue (id (onIds[i]));
    }
    syncFlag = p.apvts.getRawParameterValue (id (lsync));
    pages[0].setVisible (true);
    powers[0]->setVisible (true);

    tabs.isOn = [this] (int i) { return onFlags[(size_t) i]->load() > 0.5f; };
    tabs.onSelect = [this] (int i) { selectTab (i); if (onTab) onTab (i); };
    addAndMakeVisible (tabs);
}

void OscCard::selectTab (int i)
{
    if (! isPositiveAndBelow (i, numTabs)) return;
    tabs.setSelected (i);
    if (i == current) return;
    pages[(size_t) current].setVisible (false);
    powers[(size_t) current]->setVisible (false);
    current = i;
    pages[(size_t) current].setVisible (true);
    powers[(size_t) current]->setVisible (true);
}

void OscCard::resized()
{
    auto r = getLocalBounds().reduced (16, 12);
    auto head = r.removeFromTop (32);
    head.removeFromLeft (76);                                  // "OSC 1" title painted
    const auto pwr = head.removeFromRight (96).withSizeKeepingCentre (96, 24);
    for (auto& pw : powers) pw->setBounds (pwr);
    head.removeFromRight (12);
    tabs.setBounds (head);
    r.removeFromTop (10);
    for (auto& pg : pages) pg.setBounds (r);
}

void OscCard::paint (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    g.setColour (tokens().surface);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 12.f);
    g.setColour (oscAccent (osc));
    g.fillEllipse (16.f, 25.f, 7.f, 7.f);
    g.setFont (lnf.font (15.f, Bold));
    g.setColour (tokens().text);
    g.drawText ("OSC " + String (osc + 1), 30, 12, 60, 32, Justification::centredLeft);
}

void OscCard::refresh()
{
    tabs.refresh();
    for (int i = 0; i < numTabs; ++i)
    {
        // a switched-off section (or a switched-off oscillator) dims
        const bool oscOn = onFlags[0]->load() > 0.5f;
        const bool on = onFlags[(size_t) i]->load() > 0.5f && (i == 0 || oscOn);
        const float a = on ? 1.f : 0.35f;
        if (std::abs (pages[(size_t) i].getAlpha() - a) > 0.001f) pages[(size_t) i].setAlpha (a);
    }
    const bool sync = syncFlag->load() > 0.5f;
    if (divKnob->isVisible() != sync) divKnob->setVisible (sync);
    if (rateKnob->isVisible() == sync) rateKnob->setVisible (! sync);
}

// ======================================================================= Canvas
FlatKnob* Canvas::knob (const String& id, const String& label, Colour c, bool bipolar, int d)
{
    auto k = std::make_unique<FlatKnob> (proc.apvts, id, label, c, bipolar, d);
    auto* raw = k.get();
    addAndMakeVisible (*raw);
    owned.push_back (std::move (k));
    return raw;
}

FlatToggle* Canvas::toggle (const String& id, const String& label, FlatToggle::Style st, Colour c)
{
    auto t = std::make_unique<FlatToggle> (proc.apvts, id, label, st, c);
    auto* raw = t.get();
    addAndMakeVisible (*raw);
    owned.push_back (std::move (t));
    return raw;
}

Canvas::Canvas (ProphecyProcessor& p) : proc (p), graph (p), scope (p)
{
    setLookAndFeel (&lnf);
    using namespace ParamIDs;

    addAndMakeVisible (graph);
    osc1 = std::make_unique<OscCard> (p, 0);
    osc2 = std::make_unique<OscCard> (p, 1);
    addAndMakeVisible (*osc1);
    addAndMakeVisible (*osc2);
    osc1->onTab = [this] (int) { graph.selectOsc (0); };
    osc2->onTab = [this] (int) { graph.selectOsc (1); };
    graph.onNodeTouched = [this] (int o, int tab) { if (tab >= 0) (o == 0 ? osc1 : osc2)->selectTab (tab); };

    screamKnob = knob (scream, "", tokens().hot, false, 108);
    addAndMakeVisible (scope);
    addAndMakeVisible (meter);

    const Colour n = neutral;
    sections.push_back ({ "Voice", {}, { knob (glide, "Glide", n), knob (bend, "Bend", n), toggle (legato, "Legato", FlatToggle::Chip, n),
                                         knob (sweep, "Sweep", n, true), knob (sweepTime, "S. time", n) } });
    sections.push_back ({ "Amp", {}, { knob (attack, "Attack", n), knob (decay, "Decay", n), knob (sustain, "Sustain", n), knob (release, "Release", n) } });
    sections.push_back ({ "Delay", {}, { knob (dtime, "Time", n), knob (dfb, "Feedback", n), knob (dfx, "Mix", n),
                                         toggle (pingpong, "Ping pong", FlatToggle::Chip, n) }, toggle (dlyOn, "", FlatToggle::Pill, tokens().hot) });
    sections.push_back ({ "Reverb", {}, { knob (rsize, "Size", n), knob (rdamp, "Damp", n), knob (rmix, "Mix", n) }, toggle (revOn, "", FlatToggle::Pill, tokens().hot) });
    sections.push_back ({ "Output", {}, { knob (ott, "OTT", n), knob (spread, "Width", n), knob (volume, "Volume", n) } });

    refreshPresetList();
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        if (id < userIdBase) proc.setCurrentProgram (id - 1);
        else if (isPositiveAndBelow (id - userIdBase, userPresetFiles.size())) proc.loadUserPreset (userPresetFiles[id - userIdBase]);
        lastPresetName = proc.getPresetName();
    };
    presetBox.setTooltip ("Factory presets and your saved presets");
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { showSaveDialog(); };
    randomButton.onClick = [this] { proc.randomize(); };
    saveButton.setTooltip ("Save the current sound as a preset");
    randomButton.setTooltip ("A random screech within sensible ranges");
    FlatLnf::makePrimary (saveButton);
    for (auto* c : { (Component*) &presetBox, (Component*) &prevButton, (Component*) &nextButton,
                     (Component*) &saveButton, (Component*) &randomButton })
        addAndMakeVisible (c);

    setSize (W, H);
    timerCallback();
    startTimerHz (30);
}

Canvas::~Canvas()
{
    if (saveDialog != nullptr) saveDialog->exitModalState (0);
    saveDialog.reset();
    setLookAndFeel (nullptr);
}

void Canvas::refreshPresetList()
{
    presetBox.clear (dontSendNotification);
    presetBox.addSectionHeading ("FACTORY");
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);

    userPresetFiles = proc.getUserPresets();
    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("USER");
        for (int i = 0; i < userPresetFiles.size(); ++i)
            presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(), userIdBase + i);
    }
    lastPresetName = {};
}

void Canvas::stepPreset (int delta)
{
    Array<int> ids;
    for (int i = 0; i < presetBox.getNumItems(); ++i) ids.add (presetBox.getItemId (i));
    if (ids.isEmpty()) return;
    int at = ids.indexOf (presetBox.getSelectedId());
    at = at < 0 ? 0 : (at + delta + ids.size()) % ids.size();
    presetBox.setSelectedId (ids[at], sendNotificationSync);
}

void Canvas::showSaveDialog()
{
    if (saveDialog != nullptr) return;
    saveDialog = std::make_unique<AlertWindow> ("Save preset", "Preset name (Documents / Prophecy of Scream / Presets):",
                                                MessageBoxIconType::NoIcon, this);
    saveDialog->setLookAndFeel (&lnf);
    saveDialog->addTextEditor ("name", proc.getPresetName().startsWith ("Random") ? String ("My Screech") : proc.getPresetName());
    saveDialog->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));

    SafePointer<Canvas> safe (this);
    saveDialog->enterModalState (true, ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr || safe->saveDialog == nullptr) return;
        const auto name = safe->saveDialog->getTextEditorContents ("name");
        MessageManager::callAsync ([safe] { if (safe != nullptr) safe->saveDialog.reset(); });
        if (result == 1)
        {
            if (safe->proc.saveUserPreset (name)) safe->refreshPresetList();
            else AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Save preset", "Could not save the preset.");
        }
    }), false);
}

void Canvas::timerCallback()
{
    osc1->refresh();
    osc2->refresh();
    graph.tick();

    for (auto& s : sections)
        if (s.power != nullptr)
        {
            const float a = s.power->isOn() ? 1.f : 0.35f;
            for (auto* c : s.cells) if (std::abs (c->getAlpha() - a) > 0.001f) c->setAlpha (a);
        }

    const auto name = proc.getPresetName();
    if (name != lastPresetName)
    {
        lastPresetName = name;
        int found = 0;
        for (int i = 0; i < presetBox.getNumItems(); ++i)
            if (presetBox.getItemText (i) == name) { found = presetBox.getItemId (i); break; }
        if (found != 0) presetBox.setSelectedId (found, dontSendNotification);
        else            presetBox.setText (name, dontSendNotification);
    }

    const int w = proc.scopeWritePos.load (std::memory_order_acquire);
    if (w != lastScopePos)
    {
        constexpr int N = ProphecyProcessor::scopeSize;
        const int count = lastScopePos < 0 ? 1024 : jlimit (1, N, (w - lastScopePos + N) & (N - 1));
        float peak = 0.f;
        for (int i = 1; i <= count; ++i) peak = jmax (peak, std::abs (proc.scopeBuffer[(size_t) ((w - i + N) & (N - 1))]));
        lastScopePos = w;
        meter.setLevel (peak);
        scope.repaint();
    }
    else
        meter.setLevel (0.f);
}

void Canvas::resized()
{
    const int m = 20, gap = 12;
    graphArea  = { m, 72, 1000, 264 };
    screamArea = { graphArea.getRight() + gap, 72, W - m - graphArea.getRight() - gap, 264 };
    graph.setBounds (graphArea);

    {
        auto a = screamArea.reduced (16, 12);
        a.removeFromTop (30);
        scope.setBounds (a.removeFromBottom (54));
        a.removeFromBottom (8);
        screamKnob->setBounds (a.withSizeKeepingCentre (200, a.getHeight()));
    }

    const int oscW = (W - 2 * m - gap) / 2;
    osc1->setBounds (m, 348, oscW, 252);
    osc2->setBounds (m + oscW + gap, 348, oscW, 252);

    // bottom row: width shared by the number of cells
    const int by = 612, bh = 168;
    int cells = 0;
    for (auto& s : sections) cells += (int) s.cells.size();
    const int usable = W - 2 * m - gap * ((int) sections.size() - 1) - 24 * (int) sections.size() - 26;   // 26: meter
    const float cw = (float) usable / (float) cells;
    int x = m;
    for (auto& s : sections)
    {
        const int extra = s.title == "Output" ? 26 : 0;
        const int w = (int) (cw * (float) s.cells.size()) + 24 + extra;
        s.area = { x, by, (s.title == "Output") ? W - m - x : w, bh };
        x += w + gap;

        auto a = s.area.reduced (12, 12);
        a.removeFromTop (30);
        if (s.power != nullptr) s.power->setBounds (s.area.getRight() - 16 - 40, s.area.getY() + 10, 40, 22);
        if (extra) { meter.setBounds (a.removeFromRight (10).reduced (0, 6)); a.removeFromRight (16); }
        const float cellW = (float) a.getWidth() / (float) s.cells.size();
        for (size_t i = 0; i < s.cells.size(); ++i)
        {
            auto cell = Rectangle<int> (a.getX() + (int) (cellW * (float) i), a.getY(), (int) cellW, a.getHeight());
            if (dynamic_cast<FlatToggle*> (s.cells[i]) != nullptr) s.cells[i]->setBounds (cell.withSizeKeepingCentre (cell.getWidth() - 6, 30).translated (0, -10));
            else s.cells[i]->setBounds (cell.withSizeKeepingCentre (cell.getWidth(), FlatKnob::heightFor (44)));
        }
    }

    // top bar
    const int bx = W - m;
    randomButton.setBounds (bx - 190, 14, 100, 30);
    saveButton.setBounds (bx - 80, 14, 80, 30);
    nextButton.setBounds (bx - 236, 14, 34, 30);
    presetBox.setBounds (bx - 236 - 6 - 240, 14, 240, 30);
    prevButton.setBounds (presetBox.getX() - 6 - 34, 14, 34, 30);

    renderBackground();
}

void Canvas::renderBackground()
{
    const float scale = 2.f;
    background = Image (Image::RGB, (int) (W * scale), (int) (H * scale), true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (scale));
    const auto& t = tokens();

    g.fillAll (t.ground);

    // top bar
    g.setColour (t.divider);
    g.fillRect (0, 56, W, 1);
    doomsday::brand::drawBadge (g, doomsday::brand::headerBadgeArea (1.f, 28.f), 0.6f,
                                RectanglePlacement::xLeft | RectanglePlacement::yMid);
    const float tx = doomsday::brand::headerTextX (1.f);
    g.setFont (lnf.font (17.f, Bold));
    g.setColour (t.text);
    g.drawText ("Prophecy of Scream", Rectangle<float> (tx, 0.f, 220.f, 56.f), Justification::centredLeft);
    g.setFont (lnf.font (12.f, Medium));
    g.setColour (t.textMute);
    g.drawText ("v3.1 " + dot + " hardstyle screech synth", Rectangle<float> (tx + 182.f, 0.f, 260.f, 56.f), Justification::centredLeft);

    // SCREAM card
    card (g, lnf, screamArea, "Scream", t.hot);

    for (auto& s : sections) card (g, lnf, s.area, s.title);
}

void Canvas::paint (Graphics& g)
{
    if (background.isValid())
        g.drawImage (background, getLocalBounds().toFloat(), RectanglePlacement::stretchToFit);
}

// ======================================================================= Editor
ProphecyEditor::ProphecyEditor (ProphecyProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (816, 480, 2040, 1200);
    getConstrainer()->setFixedAspectRatio ((double) Canvas::W / (double) Canvas::H);
    setSize (1156, 680);
}

void ProphecyEditor::resized()
{
    canvas.setBounds (0, 0, Canvas::W, Canvas::H);
    canvas.setTransform (AffineTransform::scale ((float) getWidth() / (float) Canvas::W));
}
