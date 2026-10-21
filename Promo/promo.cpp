// TikTok promo renderer for PROPHECY OF SCREAM v3.1 (Minimal / flat look)
// Renders the real plugin (audio + GUI) frame by frame and writes raw BGRA 1080x1920 frames into a pipe for ffmpeg (see render.sh).
// argv[1] = output dir (final.wav + control stills), argv[2] = kick track (Promo/assets/kick.flac), argv[3] = frame pipe
// Timeline, captions, presets, camera moves and the scripted mouse (dragging the graph dots): see `sections` and `script` below.
// Song timing: song.h
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>
#include <array>
#include <memory>
#include <atomic>
#include <functional>
#include <cstdio>
#define private public        // local promo tool only: lets us drive the editor's timer, graph nodes and tabs
#include "PluginEditor.h"
#undef private
#include "song.h"

using namespace juce;
using namespace doomsday::flat;

static constexpr int VW = 1080, VH = 1920;

// ------------------------------------------------------------------ timeline
struct Section { int bar; const char* title; const char* sub; Rectangle<float> cam; };
static const Rectangle<float> camFull    { 0.f, 0.f, 1360.f, 800.f };
static const Rectangle<float> camGraph   { 12.f, 62.f, 1016.f, 546.f };    // graph + OSC 1 card (knobs follow the dots)
static const Rectangle<float> camLayers  { 12.f, 62.f, 1336.f, 546.f };    // graph + both OSC cards
static const Rectangle<float> camOsc1    { 12.f, 340.f, 670.f, 268.f };
static const Rectangle<float> camScream  { 1024.f, 64.f, 324.f, 280.f };
static const Rectangle<float> camRandom  { 12.f, 0.f, 1336.f, 608.f };    // top bar (Randomize button) + graph + both OSC cards

static const String dotSep (CharPointer_UTF8 (" \xc2\xb7 "));
static const Section sections[] {
    { 0,  "PROPHECY\nOF SCREAM",      "V3.1  NEW LOOK",                   camFull },
    { 2,  "DRAG THE FILTER",          "CUTOFF + RESONANCE IN ONE MOVE",   camGraph },
    { 5,  "GRAB THE FORMANT",         "THE VOICE OF YOUR SCREECH",        camGraph },
    { 8,  "2 OSCILLATORS",            "EACH WITH ITS OWN CURVE",          camLayers },
    { 10, "EVERY VALUE\nIN NUMBERS",  "OSC  DIST  FILTER  LFO  FORMANT",  camOsc1 },
    { 12, "ONE KNOB.",                "TURN UP THE SCREAM",               camScream },
    { 14, "PRESETS",                  "FROM EUPHORIC TO RAW",             camLayers },
    { 16, "RANDOMIZE",                "A NEW SCREECH IN ONE CLICK",       camRandom },
    { 19, "PROPHECY\nOF SCREAM",      "VST3  /  FL STUDIO",               camFull },
};
static constexpr int numSections = (int) std::size (sections);
static int sectionAt (int bar) { int s = 0; for (int i = 0; i < numSections; ++i) if (bar >= sections[i].bar) s = i; return s; }
static int sectionEndBar (int s) { return s + 1 < numSections ? sections[s + 1].bar : BARS; }

// riff: 4-bar hardstyle screech riff in D minor on a 16th grid (bar A, bar B, bar A, bar C = variation that climbs),
// short retriggered notes, `slide` = overlap into the next note -> legato pitch slide
struct Note { int step, len, note; bool slide; };
// D4 = 62, E4 64, F4 65, G4 67, A4 69, Bb4 70, C5 72, D5 74, C4 60
static const std::vector<Note> barA { { 0, 1, 62, false }, { 1, 1, 62, false }, { 2, 1, 74, false }, { 3, 1, 62, false },
                                      { 5, 1, 62, false }, { 6, 1, 65, false }, { 7, 1, 69, false }, { 8, 2, 72, true },
                                      { 10, 1, 69, false }, { 11, 1, 67, false }, { 12, 1, 65, false }, { 13, 1, 64, false }, { 14, 2, 65, false } };
static const std::vector<Note> barB { { 0, 1, 62, false }, { 1, 1, 62, false }, { 2, 1, 74, false }, { 3, 1, 62, false },
                                      { 5, 1, 62, false }, { 6, 1, 65, false }, { 7, 1, 67, false }, { 8, 2, 69, true },
                                      { 10, 1, 70, false }, { 11, 1, 69, false }, { 12, 1, 67, false }, { 13, 1, 65, false },
                                      { 14, 1, 64, false }, { 15, 1, 60, true } };
static const std::vector<Note> barC { { 0, 1, 62, false }, { 1, 1, 62, false }, { 2, 1, 74, false }, { 3, 1, 62, false },
                                      { 5, 1, 62, false }, { 6, 1, 69, false }, { 7, 1, 72, false }, { 8, 2, 74, true },
                                      { 10, 1, 72, false }, { 11, 1, 69, false }, { 12, 1, 67, false }, { 13, 1, 69, false },
                                      { 14, 1, 65, false }, { 15, 1, 64, true } };
static const std::vector<Note>* riff[] { &barA, &barB, &barA, &barC };
struct Ev { int pos; bool on; int note; };
static std::vector<Ev> buildMidi()
{
    std::vector<Ev> ev;
    const int six = BEAT / 4, gap = 700, overlap = 300;      // gap: short silence before a retrigger, overlap: legato slide
    for (int bar = 0; bar < BARS; ++bar)
    {
        const auto& notes = *riff[bar % 4];
        for (size_t i = 0; i < notes.size(); ++i)
        {
            const auto& n = notes[i];
            const int on = bar * BAR + n.step * six;
            const bool last = bar == BARS - 1 && i == notes.size() - 1;
            const int off = on + n.len * six + (last ? 30000 : (n.slide ? overlap : -gap));
            ev.push_back ({ on, true, n.note });
            ev.push_back ({ off, false, n.note });
        }
    }
    std::sort (ev.begin(), ev.end(), [] (const Ev& a, const Ev& b) { return a.pos < b.pos || (a.pos == b.pos && ! a.on && b.on); });
    return ev;
}

static float ease (float t) { t = jlimit (0.f, 1.f, t); return t * t * (3.f - 2.f * t); }
static Rectangle<float> lerpRect (Rectangle<float> a, Rectangle<float> b, float t)
{
    return { a.getX() + (b.getX() - a.getX()) * t, a.getY() + (b.getY() - a.getY()) * t,
             a.getWidth() + (b.getWidth() - a.getWidth()) * t, a.getHeight() + (b.getHeight() - a.getHeight()) * t };
}
// value along eased keyframes (x = section progress 0..1, y = value)
static float keys (std::initializer_list<Point<float>> k, float p)
{
    const Point<float>* prev = nullptr;
    for (auto& pt : k)
    {
        if (p <= pt.x) return prev == nullptr ? pt.y : prev->y + (pt.y - prev->y) * ease ((p - prev->x) / (pt.x - prev->x));
        prev = &pt;
    }
    return prev != nullptr ? prev->y : 0.f;
}
static float logKeys (std::initializer_list<Point<float>> k, float p)
{
    std::vector<Point<float>> l;
    for (auto& pt : k) l.push_back ({ pt.x, std::log (pt.y) });
    const Point<float>* prev = nullptr;
    for (auto& pt : l)
    {
        if (p <= pt.x) return std::exp (prev == nullptr ? pt.y : prev->y + (pt.y - prev->y) * ease ((p - prev->x) / (pt.x - prev->x)));
        prev = &pt;
    }
    return std::exp (prev->y);
}

// ------------------------------------------------------------------ drawing helpers
static void drawCursor (Graphics& g, Point<float> p, float alpha, bool pressed, Colour accent)
{
    if (pressed)
    {
        g.setColour (accent.withAlpha (0.35f * alpha));
        g.fillEllipse (Rectangle<float> (46.f, 46.f).withCentre (p));
        g.setColour (accent.withAlpha (alpha));
        g.drawEllipse (Rectangle<float> (46.f, 46.f).withCentre (p), 3.f);
    }
    Path a;
    a.startNewSubPath (0, 0); a.lineTo (0, 34); a.lineTo (9, 26); a.lineTo (15, 40); a.lineTo (21, 37); a.lineTo (15, 24); a.lineTo (27, 24); a.closeSubPath();
    a.applyTransform (AffineTransform::scale (1.25f).translated (p));
    g.setColour (Colours::black.withAlpha (0.5f * alpha));
    g.fillPath (a, AffineTransform::translation (2.f, 4.f));
    g.setColour (Colours::white.withAlpha (alpha));
    g.fillPath (a);
    g.setColour (Colours::black.withAlpha (0.85f * alpha));
    g.strokePath (a, PathStrokeType (1.6f));
}

// ------------------------------------------------------------------ reverse bass
// between two kicks: a D bass that swells up from silence (reversed decay) and is cut just before the next kick -> the pump
static float reverseBass (int i)
{
    static double phase = 0.0;
    const int b = i / BEAT;
    if (! kickAtBeat (b) || ! kickAtBeat (b + 1)) return 0.f;
    const float t = (float) (i - b * BEAT) / (float) BEAT;            // 0..1 inside the beat
    const float start = 0.14f, cut = 0.965f;
    if (t < start || t > cut) return 0.f;
    const float x = (t - start) / (cut - start);                        // 0..1 swell
    float amp = std::pow (x, 2.4f);
    amp *= jmin (1.f, (cut - t) / 0.012f);                              // short fade so the cut does not click
    const double f = 73.42 * (0.985 + 0.03 * x);                        // D2, tiny rise like a reversed kick tail
    phase += f / SR;
    if (phase >= 1.0) phase -= 1.0;
    const float s = (float) std::sin (phase * MathConstants<double>::twoPi);
    const float drive = 1.5f + 2.5f * x;                                // gets dirtier as it swells
    return std::tanh (s * drive) / std::tanh (drive) * amp;
}

// ------------------------------------------------------------------ main
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    if (argc < 4) { fprintf (stderr, "usage: promotool outdir kick.wav frames.fifo\n"); return 1; }
    const File outDir (argv[1]);
    const auto& tk = tokens();

    ProphecyProcessor proc;
    proc.setPlayConfigDetails (0, 2, SR, SPF);
    proc.prepareToPlay (SR, SPF);
    // 16th riff: glide kept short (presets are made for slower lines; long glide would smear every note)
    auto program = [&proc] (const String& name)
    {
        for (int i = 0; i < proc.getNumPrograms(); ++i) if (proc.getProgramName (i) == name) proc.setCurrentProgram (i);
        if (auto* g = proc.apvts.getParameter ("glide")) g->setValueNotifyingHost (g->convertTo0to1 (jmin (28.f, g->convertFrom0to1 (g->getValue()))));
    };
    auto setP = [&proc] (const String& id, float v) { if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    program ("Dutchie");

    std::unique_ptr<AudioProcessorEditor> edBase (proc.createEditor());
    auto* ed = dynamic_cast<ProphecyEditor*> (edBase.get());
    ed->setSize (1360, 800);
    auto& canvas = ed->canvas;
    auto& graph = canvas.graph;
    FlatLookAndFeel lnf;

    // canvas coordinates of things the cursor touches
    auto inCanvas = [&canvas] (Component& c, Point<float> p) { return canvas.getLocalPoint (&c, p); };
    auto nodeAt = [&] (int osc, ResponseGraph::Node n) { return inCanvas (graph, graph.nodePos (osc, n)); };
    auto tabCentre = [&] (OscCard& card, int i)
    {
        auto b = card.tabs.getBounds().toFloat();
        return inCanvas (card, { b.getX() + b.getWidth() * ((float) i + 0.5f) / (float) OscCard::numTabs, b.getCentreY() });
    };
    auto screamCentre = [&]
    {
        auto& s = canvas.screamKnob->slider;
        auto b = s.getBounds().toFloat().withTrimmedBottom (24.f);
        return inCanvas (*canvas.screamKnob, b.getCentre());
    };

    const auto midi = buildMidi();
    size_t mi = 0;
    AudioBuffer<float> synth (2, TOTAL); synth.clear();

    const int frames = TOTAL / SPF;
    Image frame (Image::ARGB, VW, VH, true);
    Random rng (7);
    int lastSection = -1, sectionStartFrame = 0;
    Rectangle<float> camFrom = camFull, camTo = camFull;
    const bool audioOnly = getenv ("PROMO_AUDIO_ONLY") != nullptr;
    FILE* out = fopen (argv[3], "wb");
    if (out == nullptr) { fprintf (stderr, "cannot open frame output\n"); return 1; }
    const char* presetCycle[] = { "Sexy Nightmare", "Wail of Souls", "Mad Machine", "Talking Dead" };

    Point<float> cursor { 1500.f, 900.f };        // canvas coordinates (starts off screen)
    float cursorAlpha = 0.f;
    bool pressed = false;
    Colour pressColour = tk.hot;
    std::vector<int> stills;
    for (int s = 0; s < numSections; ++s)
        stills.push_back ((int) ((sections[s].bar + (sectionEndBar (s) - sections[s].bar) * 0.6) * BAR / SPF));

    for (int f = 0; f < frames; ++f)
    {
        const int s0 = f * SPF;
        const int bar = s0 / BAR;
        const int sec = sectionAt (jmin (bar, BARS - 1));
        const float tSec = (float) s0 / (float) SR;

        // ---- section changes (aligned to bar starts)
        if (sec != lastSection)
        {
            camFrom = lastSection < 0 ? sections[sec].cam : lerpRect (camFrom, camTo, 1.f);
            camTo = sections[sec].cam;
            lastSection = sec;
            sectionStartFrame = f;
            graph.drag = ResponseGraph::None;
            graph.hover = ResponseGraph::None;
            switch (sec)
            {
                case 0: program ("Dutchie"); break;
                case 1: graph.selectOsc (0); canvas.osc1->selectTab (2); break;       // keep Dutchie: one oscillator, wide open filter
                case 2: canvas.osc1->selectTab (4); break;
                case 3: program ("Twin Prophecy"); graph.selectOsc (0); canvas.osc1->selectTab (0); canvas.osc2->selectTab (0); break;
                case 4: canvas.osc1->selectTab (0); break;
                case 5: program ("Cardio"); break;
                case 6: program (presetCycle[0]); break;
                case 7: program ("Dutchie"); proc.randomGen.setSeed (getenv ("PROMO_SEED") ? (int64) atoll (getenv ("PROMO_SEED")) : (int64) 2026); graph.selectOsc (0); break;   // fixed seed = same 3 sounds every render
                case 8: program ("Dutchie"); canvas.randomButton.setState (Button::buttonNormal); graph.selectOsc (0); canvas.osc1->selectTab (0); canvas.osc2->selectTab (0); break;
            }
        }
        const int inSec = f - sectionStartFrame;
        const float secT = (float) inSec / (float) FPS;
        const float secLen = (float) ((sectionEndBar (sec) - sections[sec].bar) * BAR / SPF);
        const float p = (float) inSec / secLen;            // section progress 0..1

        // ---- the scripted mouse
        Point<float> target = cursor;
        bool follow = true;
        bool wantVisible = false;
        pressed = false;
        pressColour = tk.hot;
        if (sec == 1)       // drag CUTOFF: from wide open down to a dark growl, then back up with more resonance
        {
            wantVisible = true;
            const bool drag = p >= 0.16f && p <= 0.9f;
            if (drag)
            {
                setP ("cutoff", logKeys ({ { 0.16f, 20000.f }, { 0.45f, 650.f }, { 0.7f, 2400.f }, { 0.9f, 9000.f } }, p));
                setP ("reso",   keys ({ { 0.16f, 0.51f }, { 0.45f, 0.8f }, { 0.9f, 0.62f } }, p));
            }
            graph.drag = drag ? ResponseGraph::Cut : ResponseGraph::None;
            graph.hover = p > 0.1f ? ResponseGraph::Cut : ResponseGraph::None;
            target = nodeAt (0, ResponseGraph::Cut);
            follow = ! drag;
            pressed = drag;
        }
        else if (sec == 2)  // grab the (switched off) FORMANT dot: it turns on, then sweeps like a voice
        {
            wantVisible = true;
            const bool drag = p >= 0.14f && p <= 0.9f;
            if (drag)
            {
                setP ("o1_form_on", 1.f);
                setP ("o1_form_q", 5.f);
                setP ("o1_form_hz",   logKeys ({ { 0.14f, 900.f }, { 0.38f, 2600.f }, { 0.62f, 520.f }, { 0.9f, 1500.f } }, p));
                setP ("o1_form_gain", keys ({ { 0.14f, 12.f }, { 0.5f, 22.f }, { 0.9f, 18.f } }, p));
            }
            graph.drag = drag ? ResponseGraph::Form : ResponseGraph::None;
            graph.hover = p > 0.08f ? ResponseGraph::Form : ResponseGraph::None;
            target = nodeAt (0, ResponseGraph::Form);
            follow = ! drag;
            pressed = drag;
        }
        else if (sec == 3)  // click OSC 2, then drag its cutoff (OSC 1 stays as the dim curve)
        {
            wantVisible = true;
            if (p < 0.2f)
                target = inCanvas (graph, graph.chipBounds (1).getCentre());
            else
            {
                if (graph.osc != 1) { graph.selectOsc (1); canvas.osc2->selectTab (2); }
                const bool drag = p >= 0.34f && p <= 0.92f;
                if (drag)
                {
                    setP ("o2_cutoff", logKeys ({ { 0.34f, 1600.f }, { 0.62f, 7500.f }, { 0.92f, 2800.f } }, p));
                    setP ("o2_reso",   keys ({ { 0.34f, 0.3f }, { 0.62f, 0.72f }, { 0.92f, 0.55f } }, p));
                }
                graph.drag = drag ? ResponseGraph::Cut : ResponseGraph::None;
                graph.hover = ResponseGraph::Cut;
                target = nodeAt (1, ResponseGraph::Cut);
                follow = ! drag;
                pressed = drag;
                pressColour = tk.cool;
            }
            pressed = pressed || (p > 0.16f && p < 0.21f);      // the chip click
        }
        else if (sec == 4)  // click through the tabs of OSC 1
        {
            wantVisible = true;
            const int tab = jlimit (0, OscCard::numTabs - 1, (int) (p * 5.2f));
            target = tabCentre (*canvas.osc1, tab);
            const float local = p * 5.2f - (float) tab;
            if (local > 0.35f) canvas.osc1->selectTab (tab);
            pressed = local > 0.35f && local < 0.5f;
        }
        else if (sec == 5)  // THE SCREAM knob, dragged up
        {
            wantVisible = true;
            const float sweep = ease ((p - 0.12f) / 0.8f);
            setP ("scream", 0.08f + 0.92f * sweep);
            target = screamCentre() + Point<float> (34.f, 46.f - 90.f * sweep);
            follow = p < 0.12f;
            pressed = p >= 0.12f && p < 0.95f;
        }
        else if (sec == 7)  // RANDOMIZE clicked 3 times: every knob, curve and the sound change at once
        {
            wantVisible = true;
            target = inCanvas (canvas, canvas.randomButton.getBounds().toFloat().getCentre()) + Point<float> (-8.f, 2.f);
            static const float clicks[] { 0.12f, 0.43f, 0.74f };
            for (float c : clicks)
            {
                const float frameOfClick = std::round (c * secLen);
                if ((float) inSec == frameOfClick)
                {
                    proc.randomize();
                    if (audioOnly)
                    {
                        String dump;
                        for (auto* prm : proc.getParameters())
                            if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
                                dump << rp->getParameterID() << "=" << rp->getCurrentValueAsText() << "  ";
                        fprintf (stderr, "RANDOM: %s\n", dump.toRawUTF8());
                    }
                    if (auto* gl = proc.apvts.getParameter ("glide")) gl->setValueNotifyingHost (gl->convertTo0to1 (jmin (28.f, gl->convertFrom0to1 (gl->getValue()))));
                    // the plugin levels a random sound on a held note; short 16ths need a little more to sit with the presets
                    if (auto* vol = proc.apvts.getParameter ("volume")) vol->setValueNotifyingHost (vol->convertTo0to1 (jmin (6.f, vol->convertFrom0to1 (vol->getValue()) + 5.f)));
                }
                if ((float) inSec >= frameOfClick && (float) inSec < frameOfClick + 5.f) pressed = true;
            }
            canvas.randomButton.setState (pressed ? Button::buttonDown : Button::buttonOver);
        }
        else if (sec == 6 && inSec > 0 && inSec % (BAR / SPF / 2) == 0)
            program (presetCycle[jmin (3, inSec / (BAR / SPF / 2))]);

        if (follow) cursor += (target - cursor) * 0.22f;
        else        cursor = target;
        cursorAlpha += ((wantVisible ? 1.f : 0.f) - cursorAlpha) * 0.2f;

        // ---- audio for this frame
        {
            AudioBuffer<float> buf (2, SPF); buf.clear();
            MidiBuffer m;
            while (mi < midi.size() && midi[mi].pos < s0 + SPF)
            {
                const auto& e = midi[mi++];
                m.addEvent (e.on ? MidiMessage::noteOn (1, e.note, (uint8) 110) : MidiMessage::noteOff (1, e.note), jmax (0, e.pos - s0));
            }
            proc.processBlock (buf, m);
            for (int ch = 0; ch < 2; ++ch) synth.copyFrom (ch, s0, buf, ch, 0, jmin (SPF, TOTAL - s0));
        }
        canvas.timerCallback();     // spectrum, scope, meter, preset name, dimming

        // ---- kick pulse (kicks land on beat starts)
        float pulse = 0.f;
        {
            const int beat = s0 / BEAT;
            for (int b = beat; b >= jmax (0, beat - 2); --b)
                if (kickAtBeat (b))
                {
                    const float dt = (float) (s0 - b * BEAT) / (float) SR;
                    pulse = std::exp (-dt / 0.11f);
                    break;
                }
        }

        if (audioOnly) continue;                    // PROMO_AUDIO_ONLY=1: sound check without pictures (seconds instead of minutes)

        // ---- camera
        const float camT = ease (secT / 0.45f);
        const auto cam = lerpRect (camFrom, camTo, camT);
        const float vpW = 1000.f;
        const float vpH = jmin (1000.f, vpW * cam.getHeight() / cam.getWidth());
        const float drawScale = vpW / cam.getWidth();

        Image shot = ed->createComponentSnapshot (cam.getSmallestIntegerContainer(), true, drawScale);

        // ---- compose
        {
            Graphics g (frame);
            g.fillAll (Colour (0xff0f1114));

            // soft accent light (hot from the top, cool from below), breathing with the kick
            ColourGradient hotGlow (tk.hot.withAlpha (0.16f + 0.10f * pulse), VW * 0.2f, -120.f, Colours::transparentBlack, VW * 0.2f, 900.f, true);
            g.setGradientFill (hotGlow); g.fillRect (0, 0, VW, VH);
            ColourGradient coolGlow (tk.cool.withAlpha (0.10f + 0.05f * pulse), VW * 0.85f, VH + 100.f, Colours::transparentBlack, VW * 0.85f, VH - 900.f, true);
            g.setGradientFill (coolGlow); g.fillRect (0, 0, VW, VH);

            // dot grid
            g.setColour (Colours::white.withAlpha (0.035f));
            for (int y = 12; y < VH; y += 28)
                for (int x = 12; x < VW; x += 28)
                    g.fillRect ((float) x, (float) y, 2.f, 2.f);

            // ---- plugin viewport
            const float zoom = 1.f + 0.015f * pulse;
            const Point<float> shake (pulse * 3.f * (rng.nextFloat() - 0.5f), pulse * 3.f * (rng.nextFloat() - 0.5f));
            const float cy = 1010.f;
            auto vp = Rectangle<float> (vpW, vpH).withCentre ({ VW * 0.5f, cy }).translated (shake.x, shake.y);
            vp = vp.withSizeKeepingCentre (vp.getWidth() * zoom, vp.getHeight() * zoom);
            const float sc = vp.getWidth() / cam.getWidth();
            auto toVideo = [&] (Point<float> c) { return Point<float> (vp.getX() + (c.x - cam.getX()) * sc, vp.getY() + (c.y - cam.getY()) * sc); };

            Path vpPath; vpPath.addRoundedRectangle (vp, 20.f);
            DropShadow (Colours::black.withAlpha (0.75f), 50, { 0, 22 }).drawForPath (g, vpPath);
            DropShadow (tk.hot.withAlpha (0.10f + 0.22f * pulse), 70, { 0, 0 }).drawForPath (g, vpPath);
            {
                Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (vpPath);
                // the snapshot covers the integer-rounded camera rect: position it so the float camera fills the viewport
                const auto grab = cam.getSmallestIntegerContainer().toFloat();
                const Rectangle<float> dst (vp.getX() - (cam.getX() - grab.getX()) * sc, vp.getY() - (cam.getY() - grab.getY()) * sc,
                                            grab.getWidth() * sc, grab.getHeight() * sc);
                g.setImageResamplingQuality (Graphics::highResamplingQuality);
                g.setOpacity (1.f);                      // drawImage uses the alpha of the current colour
                g.drawImage (shot, dst, RectanglePlacement::stretchToFit);
            }
            g.setColour (tk.border);
            g.strokePath (vpPath, PathStrokeType (2.f));

            if (cursorAlpha > 0.01f)
            {
                Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (vp.expanded (40.f).toNearestInt());
                drawCursor (g, toVideo (cursor), cursorAlpha, pressed, pressColour);
            }

            // ---- captions (slide up + fade at section start)
            const auto& S = sections[sec];
            const float in = ease (secT / 0.35f);
            const float y0 = 190.f + (1.f - in) * 46.f;
            const bool twoLines = String (S.title).containsChar ('\n');
            const float titleSize = twoLines ? 92.f : (String (S.title).length() > 13 ? 76.f : 92.f);
            const float th = twoLines ? titleSize * 2.15f : titleSize * 1.15f;
            {
                Graphics::ScopedSaveState ss (g);
                g.setOpacity (in);
                g.setFont (lnf.font (titleSize, Bold, -0.01f));
                g.setColour (tk.text);
                g.drawFittedText (S.title, Rectangle<float> (50.f, y0, (float) VW - 100.f, th).toNearestInt(), Justification::centred, 2, 1.f);
                // accent bar + subline
                g.setColour (tk.hot);
                g.fillRoundedRectangle (Rectangle<float> (64.f, 6.f).withCentre ({ VW * 0.5f, y0 + th + 22.f }), 3.f);
                String sub (S.sub);
                if (sec == 4) sub = "OSC" + dotSep + "DIST" + dotSep + "FILTER" + dotSep + "LFO" + dotSep + "FORMANT";
                if (sec == 6) sub = String (proc.getNumPrograms()) + " FACTORY PRESETS" + dotSep + "EUPHORIC TO RAW";
                if (sec == 0) sub = "V3.1" + dotSep + "NEW LOOK";
                if (sec == 8) sub = "VST3" + dotSep + "FL STUDIO";
                g.setFont (lnf.font (34.f, SemiBold, 0.1f));
                g.setColour (tk.textDim);
                g.drawText (sub, Rectangle<float> (30.f, y0 + th + 40.f, (float) VW - 60.f, 46.f), Justification::centred);
            }

            // ---- preset tag under the viewport
            {
                const String name = proc.getPresetName();
                auto font = lnf.font (34.f, Bold);
                auto lab = lnf.font (24.f, SemiBold, 0.12f);
                const float labW = (float) GlyphArrangement::getStringWidthInt (lab, "PRESET");
                const float nameW = (float) GlyphArrangement::getStringWidthInt (font, name);
                const float tw = 56.f + labW + 22.f + nameW + 40.f;
                auto pill = Rectangle<float> (tw, 72.f).withCentre ({ VW * 0.5f, jmin (1600.f, cy + vpH * 0.5f * zoom + 86.f) });
                g.setColour (tk.surface); g.fillRoundedRectangle (pill, 18.f);
                g.setColour (tk.border); g.drawRoundedRectangle (pill, 18.f, 2.f);
                auto led = Rectangle<float> (14.f, 14.f).withCentre ({ pill.getX() + 34.f, pill.getCentreY() });
                g.setColour (tk.hot.withAlpha (0.25f + 0.4f * pulse)); g.fillEllipse (led.expanded (5.f));
                g.setColour (tk.hot); g.fillEllipse (led);
                g.setFont (lab);
                g.setColour (tk.textMute);
                g.drawText ("PRESET", Rectangle<float> (pill.getX() + 56.f, pill.getY(), labW + 10.f, pill.getHeight()), Justification::centredLeft);
                g.setFont (font);
                g.setColour (tk.text);
                g.drawText (name, Rectangle<float> (pill.getX() + 56.f + labW + 22.f, pill.getY(), nameW + 20.f, pill.getHeight()), Justification::centredLeft);
            }

            // brand line at the bottom
            g.setFont (lnf.font (24.f, SemiBold, 0.2f));
            g.setColour (tk.textMute);
            g.drawText ("SONICSYSTEMS" + dotSep + "DOOMSDAY", Rectangle<float> (0.f, (float) VH - 150.f, (float) VW, 40.f), Justification::centred);

            // fades
            const float fadeIn = jlimit (0.f, 1.f, tSec / 0.35f);
            const float fadeOut = jlimit (0.f, 1.f, ((float) TOTAL / (float) SR - tSec) / 0.6f);
            const float black = 1.f - jmin (fadeIn, fadeOut);
            if (black > 0.f) { g.setColour (Colours::black.withAlpha (black)); g.fillRect (0, 0, VW, VH); }
        }

        // ---- write BGRA frame
        {
            Image::BitmapData bd (frame, Image::BitmapData::readOnly);
            for (int y = 0; y < VH; ++y) fwrite (bd.getLinePointer (y), 4, (size_t) VW, out);
        }
        if (f % 60 == 0) fprintf (stderr, "frame %d / %d\n", f, frames);
        if (std::find (stills.begin(), stills.end(), f) != stills.end())
        {
            FileOutputStream os (outDir.getChildFile ("still_" + String (f) + ".png"));
            PNGImageFormat().writeImageToStream (frame, os);
        }
    }
    fclose (out);

    // ---- audio: synth ducked by the kick + kick, soft limited
    AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> rd (fm.createReaderFor (File (argv[2])));
    AudioBuffer<float> kick (2, TOTAL); kick.clear();
    if (rd)
    {
        // the kick file (Promo/assets) starts its kicks at bar 2: cut one hit there and place it on every kick beat
        const int firstKick = 8 * BEAT;
        AudioBuffer<float> hit (2, BEAT);
        hit.clear();
        if (rd->lengthInSamples > firstKick + BEAT) rd->read (&hit, 0, BEAT, firstKick, true, true);
        for (int i = BEAT - 400; i < BEAT; ++i)               // tiny fade so the hit ends cleanly
            for (int ch = 0; ch < 2; ++ch) hit.setSample (ch, i, hit.getSample (ch, i) * (float) (BEAT - i) / 400.f);
        for (int b = 0; b * BEAT < TOTAL; ++b)
            if (kickAtBeat (b))
                for (int ch = 0; ch < 2; ++ch)
                    kick.addFrom (ch, b * BEAT, hit, ch, 0, jmin (BEAT, TOTAL - b * BEAT));
    }

    AudioBuffer<float> mix (2, TOTAL);
    for (int i = 0; i < TOTAL; ++i)
    {
        const int beat = i / BEAT;
        float env = 0.f;
        for (int b = beat; b >= jmax (0, beat - 1); --b)
            if (kickAtBeat (b))
            {
                const float dt = (float) (i - b * BEAT) / (float) SR;
                env = jmax (env, (dt < 0.004f ? dt / 0.004f : std::exp (-(dt - 0.004f) / 0.10f)));
            }
        const float duck = 1.f - 0.62f * env;
        const float rb = reverseBass (i);
        for (int ch = 0; ch < 2; ++ch)
        {
            const float v = synth.getSample (ch, i) * 0.55f * duck + kick.getSample (ch, i) * 0.80f + rb * 0.27f;
            const float a = std::abs (v);
            const float y = a < 0.7f ? v : (v > 0.f ? 1.f : -1.f) * (0.7f + 0.3f * std::tanh ((a - 0.7f) / 0.3f));
            mix.setSample (ch, i, y);
        }
    }
    const float pk = mix.getMagnitude (0, TOTAL);
    if (pk > 0.f) mix.applyGain (0.89f / pk);
    {   // the synth alone (before kick, bass and ducking) - handy to check the level of every scene
        auto sf = outDir.getChildFile ("synth_only.wav"); sf.deleteFile();
        WavAudioFormat sw;
        std::unique_ptr<AudioFormatWriter> sww (sw.createWriterFor (new FileOutputStream (sf), SR, 2, 24, {}, 0));
        if (sww) sww->writeFromAudioSampleBuffer (synth, 0, TOTAL);
    }
    auto wavF = outDir.getChildFile ("final.wav"); wavF.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (wavF), SR, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (mix, 0, TOTAL);
    fprintf (stderr, "done, synth peak %.3f, mix peak before norm %.3f\n", synth.getMagnitude (0, TOTAL), pk);
    return 0;
}
