// TikTok promo renderer for PROPHECY OF SCREAM
// Renders the real plugin (audio + GUI) frame by frame and writes raw BGRA 1080x1920 frames into a pipe for ffmpeg (see render.sh).
// argv[1] = output dir (final.wav + control stills), argv[2] = kick track (Promo/assets/kick.flac), argv[3] = frame pipe
// Timeline, captions, presets and camera moves: see `sections` below. Song timing: song.h
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
#define private public        // local promo tool only: lets us drive the editor's timer and tabs
#include "PluginEditor.h"
#undef private
#include "song.h"

using namespace juce;

static constexpr int VW = 1080, VH = 1920;

// ------------------------------------------------------------------ timeline
struct Section { int bar; const char* title; const char* sub; Rectangle<float> cam; };
static const Rectangle<float> camFull   { 0.f, 0.f, 1360.f, 800.f };
static const Rectangle<float> camIntro  { 40.f, 24.f, 1280.f, 752.f };
static const Rectangle<float> camOscs   { 10.f, 120.f, 1340.f, 490.f };
static const Rectangle<float> camLeft   { 12.f, 124.f, 548.f, 484.f };
static const Rectangle<float> camScream { 548.f, 124.f, 264.f, 268.f };

static const Section sections[] {
    { 0,  "PROPHECY\nOF SCREAM",  "HARDSTYLE SCREECH SYNTH", camFull },
    { 2,  "2 OSCILLATORS",        "LAYER YOUR SCREECH",      camOscs },
    { 4,  "FX ON EVERY OSC",      "DIST  /  FILTER  /  LFO", camLeft },
    { 6,  "ONE KNOB.",            "TURN UP THE SCREAM",      camScream },
    { 8,  "14 PRESETS",           "FROM EUPHORIC TO RAW",    camFull },
    { 10, "PROPHECY\nOF SCREAM",  "VST3  /  FL STUDIO",      camFull },
};
static int sectionAt (int bar) { int s = 0; for (int i = 0; i < 6; ++i) if (bar >= sections[i].bar) s = i; return s; }

// melody: 2-bar phrase in D minor, eighth-note grid, slight overlaps -> legato slides
struct Note { int start, len, note; };
static const Note phrase[] { { 0, 2, 62 }, { 2, 1, 65 }, { 3, 1, 69 }, { 4, 2, 67 }, { 6, 1, 65 }, { 7, 1, 64 },
                             { 8, 2, 62 }, { 10, 1, 60 }, { 11, 1, 62 }, { 12, 3, 69 }, { 15, 1, 72 } };
struct Ev { int pos; bool on; int note; };
static std::vector<Ev> buildMidi()
{
    std::vector<Ev> ev;
    const int eighth = BEAT / 2, overlap = 420;
    for (int rep = 0; rep < BARS / 2; ++rep)
        for (size_t i = 0; i < std::size (phrase); ++i)
        {
            const auto& n = phrase[i];
            const int on = rep * 2 * BAR + n.start * eighth;
            const bool last = rep == BARS / 2 - 1 && i == std::size (phrase) - 1;
            const int off = on + n.len * eighth + (last ? 9000 : overlap);
            ev.push_back ({ on, true, n.note });
            ev.push_back ({ off, false, n.note });
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

// ------------------------------------------------------------------ drawing helpers
static void drawBeamV (Graphics& g, Point<float> src, float angle, float spread, float len, Colour col, float alpha)
{
    for (int layer = 0; layer < 3; ++layer)
    {
        const float s = spread * (1.f - 0.3f * (float) layer);
        const auto dir = Point<float> (std::sin (angle), std::cos (angle));
        const auto perp = Point<float> (dir.y, -dir.x);
        const auto end = src + dir * len;
        const float hw = std::tan (s) * len;
        Path p; p.startNewSubPath (src - perp * 2.f); p.lineTo (src + perp * 2.f); p.lineTo (end + perp * hw); p.lineTo (end - perp * hw); p.closeSubPath();
        ColourGradient gr (col.withAlpha (alpha * (0.4f + 0.3f * (float) layer)), src.x, src.y, col.withAlpha (0.f), end.x, end.y, false);
        g.setGradientFill (gr);
        g.fillPath (p);
    }
}

static void glowText (Graphics& g, const String& t, Rectangle<float> r, const Font& f, Colour core, Colour glow, float glowAmt, Justification j)
{
    g.setFont (f);
    for (int i = 5; i >= 1; --i)
    {
        g.setColour (glow.withAlpha (glowAmt * 0.16f / (float) i));
        const float o = (float) i * 2.2f;
        for (auto d : { Point<float> (-o, 0.f), Point<float> (o, 0.f), Point<float> (0.f, -o), Point<float> (0.f, o) })
            g.drawFittedText (t, r.translated (d.x, d.y).toNearestInt(), j, 3, 1.f);
    }
    g.setColour (core);
    g.drawFittedText (t, r.toNearestInt(), j, 3, 1.f);
}

static void drawCursor (Graphics& g, Point<float> p, float alpha)
{
    Path a;
    a.startNewSubPath (0, 0); a.lineTo (0, 34); a.lineTo (9, 26); a.lineTo (15, 40); a.lineTo (21, 37); a.lineTo (15, 24); a.lineTo (27, 24); a.closeSubPath();
    a.applyTransform (AffineTransform::translation (p));
    g.setColour (Colours::black.withAlpha (0.6f * alpha));
    g.fillPath (a, AffineTransform::translation (2.f, 3.f));
    g.setColour (Colours::white.withAlpha (alpha));
    g.fillPath (a);
    g.setColour (Colours::black.withAlpha (0.8f * alpha));
    g.strokePath (a, PathStrokeType (1.5f));
}

// ------------------------------------------------------------------ main
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    if (argc < 4) { fprintf (stderr, "usage: promotool outdir kick.wav frames.fifo\n"); return 1; }
    const File outDir (argv[1]);

    ProphecyProcessor proc;
    proc.setPlayConfigDetails (0, 2, SR, SPF);
    proc.prepareToPlay (SR, SPF);
    auto program = [&proc] (const String& name) { for (int i = 0; i < proc.getNumPrograms(); ++i) if (proc.getProgramName (i) == name) proc.setCurrentProgram (i); };
    program ("Dutchie");

    std::unique_ptr<AudioProcessorEditor> edBase (proc.createEditor());
    auto* ed = dynamic_cast<ProphecyEditor*> (edBase.get());
    ed->setSize (1360, 800);
    auto& canvas = ed->canvas;
    StageLookAndFeel lnf;

    const auto midi = buildMidi();
    size_t mi = 0;
    AudioBuffer<float> synth (2, TOTAL); synth.clear();

    const int frames = TOTAL / SPF;
    Image frame (Image::ARGB, VW, VH, true);
    Random rng (7);
    int lastSection = -1, sectionStartFrame = 0;
    Rectangle<float> camFrom = camFull, camTo = camFull;
    FILE* out = fopen (argv[3], "wb");
    if (out == nullptr) { fprintf (stderr, "cannot open frame output\n"); return 1; }
    const char* presetCycle[] = { "Sexy Nightmare", "Mad Machine", "Horn of Fallen", "Blippy Blippy" };

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
            switch (sec)
            {
                case 0: program ("Dutchie"); break;
                case 1: program ("Twin Prophecy"); break;
                case 2: program ("Devil's Scream"); break;
                case 3: program ("Cardio"); break;
                case 4: program (presetCycle[0]); break;
                case 5: program ("Dutchie"); break;
            }
        }
        const int inSec = f - sectionStartFrame;
        const float secT = (float) inSec / (float) FPS;

        // ---- scripted moves inside sections
        if (sec == 2)
        {
            const int tab = (inSec / 24) % 3;            // switch tab every half bar
            canvas.fx1->selectTab (tab);
            canvas.fx2->selectTab ((tab + 1) % 3);
        }
        else
        {
            canvas.fx1->selectTab (0);
            canvas.fx2->selectTab (0);
        }
        float screamSweep = -1.f;
        if (sec == 3)
        {
            screamSweep = ease ((float) inSec / (float) (2 * BAR / SPF - 6));
            if (auto* p = proc.apvts.getParameter ("scream")) p->setValueNotifyingHost (p->convertTo0to1 (0.08f + 0.92f * screamSweep));
        }
        if (sec == 4 && inSec > 0 && inSec % 24 == 0)
            program (presetCycle[jmin (3, inSec / 24)]);

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
        canvas.timerCallback();     // meters, title glow, preset name, dimming

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

        // ---- camera
        const float camT = ease (secT / 0.45f);
        const auto cam = lerpRect (camFrom, camTo, camT);
        const float vpW = 1000.f;
        const float vpH = jmin (1000.f, vpW * cam.getHeight() / cam.getWidth());
        const float drawScale = vpW / cam.getWidth();

        Image shot = ed->createComponentSnapshot (cam.getSmallestIntegerContainer(), true, drawScale);
        Image bgShot = ed->createComponentSnapshot (ed->getLocalBounds(), true, 0.18f);

        // ---- compose
        {
            Graphics g (frame);
            g.fillAll (Colour (0xff050304));

            // blurred plugin backdrop
            g.setImageResamplingQuality (Graphics::highResamplingQuality);
            g.setOpacity (0.30f);
            const float bgH = (float) VH, bgW = bgH * 1360.f / 800.f;
            g.drawImage (bgShot, Rectangle<float> ((VW - bgW) * 0.5f - 120.f * std::sin (tSec * 0.3f), 0.f, bgW, bgH), RectanglePlacement::stretchToFit);
            g.setOpacity (1.f);

            // stage light
            ColourGradient haze (Colour (0xff7a0817).withAlpha (0.75f), VW * 0.5f, -80.f, Colours::transparentBlack, VW * 0.5f, 1100.f, true);
            g.setGradientFill (haze); g.fillRect (0, 0, VW, VH);
            const float sway = std::sin (tSec * 1.3f) * 0.08f;
            for (int i = 0; i < 4; ++i)
            {
                const float a = -0.55f + 0.3f * (float) i + sway;
                drawBeamV (g, { 90.f, -30.f }, -a - 0.3f, 0.05f, 2300.f, Colour (0xffff2a45), 0.07f + 0.05f * pulse);
                drawBeamV (g, { VW - 90.f, -30.f }, a + 0.3f, 0.05f, 2300.f, Colour (0xffff2a45), 0.07f + 0.05f * pulse);
            }
            drawBeamV (g, { VW * 0.5f, -80.f }, 0.f, 0.12f, 1500.f, Colour (0xffffd9de), 0.05f + 0.04f * pulse);

            // LED dots behind captions
            for (int y = 150; y < 560; y += 12)
                for (int x = 6; x < VW; x += 12)
                {
                    const float dx = ((float) x - VW * 0.5f) / (VW * 0.5f);
                    const float a = 0.10f * (1.f - (float) (y - 150) / 410.f) * (1.f - 0.6f * dx * dx) * (0.7f + 0.3f * pulse);
                    g.setColour (Colour (0xffff2440).withAlpha (jmax (0.f, a)));
                    g.fillRect ((float) x, (float) y, 3.f, 3.f);
                }

            ColourGradient vig (Colours::transparentBlack, VW * 0.5f, VH * 0.5f, Colours::black.withAlpha (0.8f), 0.f, -200.f, true);
            g.setGradientFill (vig); g.fillRect (0, 0, VW, VH);

            // ---- plugin viewport
            const float zoom = 1.f + 0.022f * pulse;
            const Point<float> shake (pulse * 5.f * (rng.nextFloat() - 0.5f), pulse * 5.f * (rng.nextFloat() - 0.5f));
            const float cy = 1010.f;
            auto vp = Rectangle<float> (vpW, vpH).withCentre ({ VW * 0.5f, cy }).translated (shake.x, shake.y);
            vp = vp.withSizeKeepingCentre (vp.getWidth() * zoom, vp.getHeight() * zoom);

            Path vpPath; vpPath.addRoundedRectangle (vp, 18.f);
            DropShadow (Colour (0xffff2440).withAlpha (0.35f + 0.35f * pulse), 60, { 0, 0 }).drawForPath (g, vpPath);
            DropShadow (Colours::black.withAlpha (0.8f), 40, { 0, 18 }).drawForPath (g, vpPath);
            {
                Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (vpPath);
                // the snapshot covers the integer-rounded camera rect: position it so the float camera fills the viewport
                const auto grab = cam.getSmallestIntegerContainer().toFloat();
                const float sc = vp.getWidth() / cam.getWidth();
                const Rectangle<float> dst (vp.getX() - (cam.getX() - grab.getX()) * sc, vp.getY() - (cam.getY() - grab.getY()) * sc,
                                            grab.getWidth() * sc, grab.getHeight() * sc);
                g.drawImage (shot, dst, RectanglePlacement::stretchToFit);
                g.setColour (Colour (0xffff2440).withAlpha (0.10f * pulse));
                g.fillRect (vp);
            }
            ColourGradient border (Colour (0xffff6b7d).withAlpha (0.9f), vp.getCentreX(), vp.getY(), Colour (0xff7a0016).withAlpha (0.6f), vp.getCentreX(), vp.getBottom(), false);
            g.setGradientFill (border);
            g.strokePath (vpPath, PathStrokeType (2.f));

            // fake cursor turning THE SCREAM knob
            if (screamSweep >= 0.f)
            {
                const Point<float> knobUI (680.f, 262.f);
                const float sc = vp.getWidth() / cam.getWidth();
                const Point<float> kp (vp.getX() + (knobUI.x - cam.getX()) * sc, vp.getY() + (knobUI.y - cam.getY()) * sc);
                const float appear = ease (secT / 0.5f);
                drawCursor (g, kp + Point<float> (70.f, 140.f - 260.f * screamSweep), appear);
            }

            // ---- captions (slide up + fade at section start)
            const auto& S = sections[sec];
            const float in = ease (secT / 0.35f);
            const float y0 = 205.f + (1.f - in) * 50.f;
            const bool twoLines = String (S.title).containsChar ('\n');
            const float titleH = twoLines ? 88.f : 80.f;
            auto titleFont = lnf.titleFont (twoLines ? 82.f : (String (S.title).length() > 12 ? 62.f : 76.f), 0.06f);
            {
                Graphics::ScopedSaveState ss (g);
                g.setOpacity (in);
                const float th = twoLines ? titleH * 2.f + 10.f : titleH;
                glowText (g, S.title, { 50.f, y0, (float) VW - 100.f, th }, titleFont, Colours::white, Colour (0xffff2440), 0.8f + 0.6f * pulse, Justification::centred);
                g.setFont (lnf.labelFont (40.f, 0.42f));
                g.setColour (Colour (0xffff3a52));
                g.drawText (S.sub, Rectangle<float> (40.f, y0 + th + 14.f, (float) VW - 80.f, 48.f), Justification::centred);
            }

            // ---- preset tag under the viewport
            {
                const String name = proc.getPresetName().toUpperCase();
                auto font = lnf.labelFont (34.f, 0.22f);
                auto lab = lnf.labelFont (24.f, 0.3f);
                const float labW = (float) GlyphArrangement::getStringWidthInt (lab, "PRESET");
                const float nameW = (float) GlyphArrangement::getStringWidthInt (font, name);
                const float tw = 56.f + labW + 22.f + nameW + 40.f;
                auto pill = Rectangle<float> (tw, 70.f).withCentre ({ VW * 0.5f, jmin (1580.f, cy + vpH * 0.5f * zoom + 80.f) });
                g.setColour (Colour (0xcc0c0809)); g.fillRoundedRectangle (pill, 35.f);
                g.setColour (Colour (0xffff2440).withAlpha (0.8f)); g.drawRoundedRectangle (pill, 35.f, 2.f);
                auto led = Rectangle<float> (14.f, 14.f).withCentre ({ pill.getX() + 34.f, pill.getCentreY() });
                g.setColour (Colour (0xffff2440).withAlpha (0.4f + 0.5f * pulse)); g.fillEllipse (led.expanded (6.f));
                g.setColour (Colour (0xffff6b7d)); g.fillEllipse (led);
                g.setFont (lab);
                g.setColour (Colour (0xff9a8a8d));
                g.drawText ("PRESET", Rectangle<float> (pill.getX() + 56.f, pill.getY(), labW + 10.f, pill.getHeight()), Justification::centredLeft);
                g.setFont (font);
                g.setColour (Colours::white);
                g.drawText (name, Rectangle<float> (pill.getX() + 56.f + labW + 22.f, pill.getY(), nameW + 20.f, pill.getHeight()), Justification::centredLeft);
            }

            // beat flash + fades
            g.setColour (Colour (0xffff2440).withAlpha (0.06f * pulse));
            g.fillRect (0, 0, VW, VH);
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
        if (f == 70 || f == 170 || f == 260 || f == 360 || f == 430 || f == 560)
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
    if (rd) rd->read (&kick, 0, jmin (TOTAL, (int) rd->lengthInSamples), 0, true, true);

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
        for (int ch = 0; ch < 2; ++ch)
        {
            const float v = synth.getSample (ch, i) * 0.55f * duck + kick.getSample (ch, i) * 0.80f;
            // gentle peak control only above -3 dBFS
            const float a = std::abs (v);
            const float y = a < 0.7f ? v : (v > 0.f ? 1.f : -1.f) * (0.7f + 0.3f * std::tanh ((a - 0.7f) / 0.3f));
            mix.setSample (ch, i, y);
        }
    }
    const float pk = mix.getMagnitude (0, TOTAL);
    if (pk > 0.f) mix.applyGain (0.89f / pk);
    auto wavF = outDir.getChildFile ("final.wav"); wavF.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (wavF), SR, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (mix, 0, TOTAL);
    fprintf (stderr, "done, synth peak %.3f, mix peak before norm %.3f\n", synth.getMagnitude (0, TOTAL), pk);
    return 0;
}
