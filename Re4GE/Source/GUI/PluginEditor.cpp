#include "PluginEditor.h"
#include "DoomsdayBrand.h"
#include "../Presets/ShapeFiles.h"

using namespace juce;
namespace mod = doomsday::mod;

using namespace doomsday::flat;

namespace
{
    float raw (ReProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
    const String dotSep (CharPointer_UTF8 (" \xc2\xb7 "));
}

// ======================================================================= THE STEP view
void StepView::tick()
{
    const auto p = proc.gather();
    const float sig = (float) p.stepBeats * 7.f + p.delayPct * 0.013f + p.holdPct * 0.017f + p.decayPct * 0.019f + p.gatePct * 0.023f + p.delayMs * 0.0013f
                    + p.holdMs * 0.00017f + p.decayMs * 0.00019f + p.gateMs * 0.00023f + p.duck * 3.1f + p.initial * 3.7f + p.swellMs * 0.011f + p.curve * 5.3f
                    + p.sustain * 2.9f + p.decayCurve * 0.71f + p.fadeMs * 0.031f + (float) p.mask * 11.f + p.offsetPct * 0.07f + (p.gate ? 100.f : 0.f) + (p.sync ? 200.f : 0.f)
                    + (float) p.pitchMode * 13.f + p.pitchSt * 0.37f + p.pitchMs * 0.0041f + proc.getEngine().tempoNow.load() * 0.0001f;
    auto& e = proc.getEngine();
    const int hs = e.hitSerial.load(), len = e.guiLength.load();
    if (std::abs (sig - lastSig) > 1e-5f || hs != lastHit || len != lastLen) { lastSig = sig; lastHit = hs; lastLen = len; repaint(); }
}

void StepView::paint (Graphics& g)
{
    using namespace ReTheme;
    auto& lf = lnfOf (*this);
    const auto& tk = tokens();
    const auto p = proc.gather();
    auto& eng = proc.getEngine();
    const double bpm = eng.tempoNow.load(), beatMs = 60000.0 / bpm, stepMs = p.stepBeats * beatMs;
    auto r = getLocalBounds().toFloat();
    g.setColour (tk.surface); g.fillRoundedRectangle (r, 12.f);
    auto plot = r.reduced (16.f, 12.f).withTrimmedTop (20.f).withTrimmedBottom (16.f);
    // window: PATTERN = one beat, GATE = a 600 ms note (and its fade)
    const double winMs = p.gate ? 800.0 : beatMs;
    auto xOf = [&] (double ms) { return plot.getX() + plot.getWidth() * (float) (ms / winMs); };
    // ms scale
    g.setFont (lf.font (10.5f, Medium));
    const double labelEvery = winMs > 700.0 ? 100.0 : 50.0;
    for (double ms = 0.0; ms <= winMs + 0.01; ms += 10.0)
    {
        const float x = xOf (ms);
        const bool major = std::fmod (ms + 0.01, labelEvery) < 0.1;
        g.setColour (tk.textMute.withAlpha (major ? 0.22f : 0.07f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom() + (major ? 4.f : 2.f));
        if (major) { g.setColour (tk.textMute); g.drawText (String ((int) std::round (ms)) + " ms", Rectangle<float> (x - 30.f, plot.getBottom() + 3.f, 60.f, 13.f), Justification::centred); }
    }
    // envelope (+ the kick at every beat start in PATTERN mode)
    Path env; bool started = false;
    const int cols = jmax (2, (int) plot.getWidth());
    const re::Times tm = re::times (p, stepMs);
    const int stepsPerBeat = jmax (1, (int) std::round (1.0 / p.stepBeats));
    const double offMs = p.offsetPct * 0.01 * stepMs;
    Path pitchP;
    for (int c = 0; c <= cols; ++c)
    {
        const double ms = winMs * c / cols;
        double a = 0.0, st = 0.0;
        if (p.gate)
        {
            const double noteOff = 600.0;
            double gt = 1.0;
            if (ms > noteOff) gt = 0.5 + 0.5 * std::cos (jmin (1.0, (ms - noteOff) / jmax (0.5f, p.fadeMs)) * MathConstants<double>::pi);
            a = re::envAt (p, tm, ms) * gt; st = re::pitchAt (p, tm, ms);
        }
        else
        {
            const double tt = ms - offMs;
            if (tt >= 0.0)
            {
                const int s = (int) std::floor (tt / stepMs);
                const double t = tt - s * stepMs;
                if ((p.mask >> (s % stepsPerBeat)) & 1) { a = re::envAt (p, tm, t) * re::gateAt (p, tm, t); st = re::pitchAt (p, tm, t); }
            }
        }
        const float x = plot.getX() + (float) c, y = plot.getBottom() - plot.getHeight() * 0.9f * (float) a;
        if (! started) { env.startNewSubPath (x, plot.getBottom()); started = true; }
        env.lineTo (x, y);
        const float py = plot.getCentreY() - plot.getHeight() * 0.42f * (float) jlimit (-1.0, 1.0, st / 24.0);
        if (c == 0) pitchP.startNewSubPath (x, py); else pitchP.lineTo (x, py);
    }
    env.lineTo (plot.getRight(), plot.getBottom()); env.closeSubPath();
    ColourGradient fill (bass.withAlpha (0.40f), 0.f, plot.getY(), bassDeep.withAlpha (0.25f), 0.f, plot.getBottom(), false);
    g.setGradientFill (fill); g.fillPath (env);
    g.setColour (bass); g.strokePath (env, PathStrokeType (1.8f));
    if (p.pitchMode != re::Stable)
    {
        g.setColour (Colours::white.withAlpha (0.45f));
        PathStrokeType (1.f).createDashedStroke (pitchP, pitchP, std::array<float, 2> { 5.f, 4.f }.data(), 2);
        g.fillPath (pitchP);
    }
    // kicks: PATTERN = the note (kick) at 0 and the next beat
    if (! p.gate)
    {
        for (double k : { 0.0, beatMs })
        {
            const float x = xOf (k);
            g.setColour (kick.withAlpha (0.85f)); g.fillRect (Rectangle<float> (x - 1.5f, plot.getY() - 4.f, 3.f, plot.getHeight() + 4.f));
        }
        for (int i = 1; i < stepsPerBeat; ++i) { g.setColour (tk.textMute.withAlpha (0.4f)); g.drawVerticalLine ((int) xOf (offMs + i * stepMs), plot.getY(), plot.getBottom()); }
    }
    else { g.setColour (tk.textMute); g.drawVerticalLine ((int) xOf (600.0), plot.getY(), plot.getBottom()); }
    g.setFont (lf.font (11.f, SemiBold)); g.setColour (tk.textDim);
    g.drawText (p.gate ? String ("GATE  -  one note of 600 ms (note off = the line), then FADE")
                       : "ONE BEAT AT " + String (roundToInt (bpm)) + " BPM  -  red = the kick (your note), blue = the bass" + (p.pitchMode != re::Stable ? String ("  -  dashed = pitch") : String()),
                r.reduced (16.f, 6.f).withHeight (14.f), Justification::topLeft);
}

// ======================================================================= canvas
Canvas::Card& Canvas::card (const String& title, Colour dot, int pg)
{
    cards.push_back ({});
    auto& c = cards.back(); c.title = title; c.dot = dot; c.page = pg;
    return c;
}
Component* Canvas::knob (Card& c, const char* id, const char* label, const char* tip, bool bipolar)
{
    auto k = std::make_unique<Knob> (proc.apvts, id, label, c.dot, bipolar);
    k->slider.setTooltip (tip);
    auto* kp = k.get(); addAndMakeVisible (*kp); owned.push_back (std::move (k));
    c.knobs.push_back (kp);
    return kp;
}

Canvas::Canvas (ReProcessor& p) : proc (p), view (p), designer (p), shaperView (p), matrix (p), kickView (p), randomPanel (p), seqView (p), eqView (p), exportPanel (p)
{
    using namespace ParamIDs;
    setLookAndFeel (&lnf);
    const Colour acc = ReTheme::accent, hot = ReTheme::accent, cool = tokens().cool, n { 0xffb4bac3 };
    cards.reserve (40);
    auto seg = [this] (const char* id, StringArray labels, Colour c, const char* tip)
    {
        auto s = std::make_unique<Segmented> (proc.apvts, id, labels, c);
        s->setTooltip (tip);
        { int w = 8; for (auto& l : labels) w += 14 + 7 * l.length(); segW[s.get()] = jmax (84, w); }
        auto* sp = s.get(); addAndMakeVisible (*sp); owned.push_back (std::move (s)); return (Component*) sp;
    };
    auto toggle = [this] (const char* id, const char* text, Toggle::Style st, Colour c, const char* tip)
    {
        auto s = std::make_unique<Toggle> (proc.apvts, id, text, st, c);
        s->setTooltip (tip);
        auto* sp = s.get(); addAndMakeVisible (*sp); owned.push_back (std::move (s)); return (Component*) sp;
    };
    auto templButtons = [this] (Card& c)
    {
        std::vector<Component*> made;
        for (int i = 0; i < ReProcessor::templateNames().size(); ++i)
        {
            auto b = std::make_unique<TextButton> (ReProcessor::templateNames()[i]);
            b->setTooltip ("Template: sets the envelope (and the step for PULSE), the sound stays");
            b->onClick = [this, i] { proc.applyTemplate (i); };
            addAndMakeVisible (*b); made.push_back (b.get());
            templ.push_back (std::move (b));
        }
        for (int i = (int) made.size() - 1; i >= 0; --i) c.head.push_back (made[(size_t) i]);
    };
    const char* srcTip = "OSC 1 source. SYNTH: saw / pulse / sine with PHASE WARP. TAIL: a kick tail played backwards (the classic). SAMPLE: your own WAV";
    const char* distTip = "Distortion type";
    // ---- always: MASTER + MACROS
    {
        auto& c = card ("Master", n, -1); cMaster = (int) cards.size() - 1;
        knob (c, level, "Output", "Level of the bass");
        knob (c, clip, "Clip", "Soft clip at the end: evens out the peaks of the bass");
        knob (c, velocity, "Velocity", "0 = every note full level, 1 = MIDI velocity sets the level");
    }
    {
        auto& c = card ("Macros", acc, -1); cMacros = (int) cards.size() - 1;
        const char* tips[8] {
            "BODY: +6 dB at 220 Hz - the 150-400 Hz 'meat'",
            "WEIGHT: the clean SUB louder (+6 dB) and higher (+40 Hz)",
            "REVERSE: the reverse-bass set - a later, slower swell, a rise into the kick (+7 st), harmonics opening late",
            "PITCH: a drop from +12 st at the start of every step (bounce)",
            "GRIT: more distortion (DRIVE +18 dB, STAGE 2 +12 dB)",
            "BITE: presence at 2 kHz (+8 dB) and a brighter top",
            "MOVEMENT: unison detune and drift - the bass moves and breathes",
            "SPACE: stereo width of the mids / highs (the lows stay mono)" };
        for (int k = 0; k < 8; ++k) knob (c, macros[k], macroNames[k].toRawUTF8(), tips[k]);
    }
    // ---- MAIN: the 80 % screen
    {
        auto& c = card ("Source", acc, PMain); cMSource = (int) cards.size() - 1;
        knob (c, osc1Level, "OSC 1", "Level of OSC 1 (the source on the left)");
        knob (c, oscId (2, "level"), "OSC 2", "Level of OSC 2 (OFF = not playing) - its wave / pitch on the OSC page");
        knob (c, oscId (3, "level"), "OSC 3", "Level of OSC 3");
        knob (c, oscId (4, "level"), "OSC 4", "Level of OSC 4");
        knob (c, tune, "Tune", "Bass pitch against the note (12 = an octave up; the SUB stays on the note)", true);
        c.head.push_back (seg (source, { "Synth", "Tail", "Sample" }, acc, srcTip));
    }
    {
        auto& c = card ("Envelope", acc, PMain); cMEnv = (int) cards.size() - 1;
        knob (c, delayPct, "Delay", "How long after the kick the bass starts (% of a step). More = later, the kick's tail stays clean");
        knob (c, initial, "Initial", "Where the swell starts (0 = from silence)");
        knob (c, swell, "Swell", "How long the bass takes to come up (ms)");
        knob (c, curve, "Curve", "+ = fast, then landing (sidechain release), - = slow start, late swell", true);
        knob (c, sustain, "Sustain", "Where DECAY ends (100% = a level shelf like PUMP)");
        knob (c, gatePct, "Gate", "% of a step where the bass is gone (SYNC timing)");
    }
    {
        auto& c = card ("Tone", hot, PMain); cMTone = (int) cards.size() - 1;
        knob (c, drive, "Drive", "Distortion of the bass (stage 1)");
        knob (c, dist2Drive, "Stage 2", "A second distortion after the EQ - turns the presence into a dense, bright top (OFF = none)");
        knob (c, preHz, "Filter", "The filter before the distortion (type on the FILTER page)");
        knob (c, highCut, "High cut", "Darkness of the top");
    }
    {
        auto& c = card ("Low end", n, PMain); cMLow = (int) cards.size() - 1;
        knob (c, subHz, "Sub Hz", "Below this the bass is a clean sine on the note (never cut, never distorted). OFF = all from the source");
        knob (c, subLevel, "Sub", "Level of the clean sub - the 'bass punch'");
        knob (c, stereoWidth, "Width", "Stereo width of the mids / highs (your reference basses: about 25 - 45%)");
    }
    // ---- OSC
    {
        auto& c = card ("OSC 1", acc, POsc); cSource = (int) cards.size() - 1;
        knob (c, osc1Level, "Level", "Level of OSC 1 (OFF = only OSC 2-4 play)");
        knob (c, blend, "Saw/Pulse", "SYNTH: 0 = saw, 100% = pulse");
        knob (c, width, "Width", "SYNTH: pulse width (50% = square)");
        knob (c, sine, "Sine", "SYNTH: a sine on the note for the body (OFF = none)");
        c.head.push_back (seg (source, { "Synth", "Tail", "Sample" }, acc, srcTip));
    }
    {
        auto& c = card ("Phase warp", acc, POsc); cWarp = (int) cards.size() - 1;
        knob (c, warpAmt, "Amount", "SYNTH: how much the phase is bent - other harmonics than drive, the 'springy' bass");
        knob (c, warpEnv, "Env", "0 = the warp is constant, 100% = it opens with the envelope");
        c.head.push_back (seg (warp, { "Off", "Bend", "Squeeze", "Sync", "Mirror" }, acc, "BEND: sine -> saw. SQUEEZE: nasal, square-ish. SYNC: talking. MIRROR: even harmonics, metallic"));
    }
    {
        auto& c = card ("Phase", acc, POsc); cPhase = (int) cards.size() - 1;
        c.head.push_back (seg (phaseMode, { "Reset", "Free" }, acc, "RESET (phase lock): every step starts from the same phase - every hit sounds the same. FREE: the phases run on (each hit a bit different)"));
    }
    for (int o = 2; o <= 4; ++o)
    {
        auto& c = card ("OSC " + String (o), acc, POsc); cOsc[o - 2] = (int) cards.size() - 1;
        knob (c, oscId (o, "level"), "Level", "Level of this oscillator (OFF = not playing). All oscillators go through the distortion together - that makes the density");
        knob (c, oscId (o, "oct"), "Oct", "Octaves up / down (your basses: synth + an octave up + sub)", true);
        knob (c, oscId (o, "semi"), "Semi", "Semitones", true);
        knob (c, oscId (o, "fine"), "Fine", "Cents - a few cents against OSC 1 = beating, movement", true);
        knob (c, oscId (o, "phase"), "Phase", "Start phase on every step (PHASE RESET)");
        knob (c, oscId (o, "width"), "Width", "PULSE: pulse width. NOISE: brightness");
        knob (c, oscId (o, "pm"), "PM", "Phase modulation by the note's sine - metallic, talking harmonics");
        c.head.push_back (toggle (oscId (o, "sync"), "Sync", Toggle::Chip, acc, "Hard sync to the note: tune it up (SEMI / OCT) and it 'talks'"));
        c.head.push_back (seg (oscId (o, "wave"), { "Sine", "Tri", "Saw", "Sqr", "Pulse", "Noise" }, acc, "Wave of this oscillator"));
    }
    // ---- PITCH / ENVELOPE / SHAPE
    {
        auto& c = card ("Pitch", acc, PPitch); cPitch = (int) cards.size() - 1;
        knob (c, pitchAmt, "Amount", "How far the pitch moves (semitones): DROP from it down, RISE up to it at the end", true);
        knob (c, pitchTime, "Time", "How long the move takes");
        knob (c, tune, "Tune", "Bass pitch against the note (12 = an octave up)", true);
        knob (c, fine, "Fine", "Fine tune in cents", true);
        knob (c, fixedNote, "Note", "KEY TRACK off: the bass always plays this note (the notes only give the rhythm)");
        knob (c, glide, "Glide", "The pitch slides from the last note while the bass still sounds (OFF = jumps)");
        c.head.push_back (toggle (keyTrack, "Key track", Toggle::Chip, acc, "ON: the bass plays the note you play. OFF: always on NOTE"));
        c.head.push_back (seg (pitchMode, { "Stable", "Drop", "Rise", "Bounce" }, acc, "STABLE on the note, DROP from above at the start, RISE at the end (the classic reverse), BOUNCE both"));
    }
    {
        auto& c = card ("Envelope", acc, PEnv); cEnv = (int) cards.size() - 1;
        knob (c, duck, "Duck", "Level before DELAY (0 = silent while the kick plays)");
        knob (c, initial, "Initial", "Where the swell starts (0 = from silence)");
        knob (c, swell, "Swell", "How long the bass takes to come up (always ms)");
        knob (c, curve, "Curve", "+ = fast, then landing (sidechain release), - = slow start, late swell, 0 = straight", true);
        knob (c, sustain, "Sustain", "Where DECAY ends (100% = no decay, a level shelf like PUMP)");
        knob (c, decayCurve, "Dec curve", "Shape of the decay: low = fast drop then a long tail, high = holds then falls");
        knob (c, fade, "Fade", "The cut at the end of the gate (smooth, no click before the kick)");
        templButtons (c);
    }
    {
        auto& c = card ("Times (sync)", acc, PEnv); cTimeSync = (int) cards.size() - 1;
        knob (c, delayPct, "Delay", "% of a step before the bass starts (the kick plays here)");
        knob (c, holdPct, "Hold", "% of a step the bass stays at the top after the swell");
        knob (c, decayPct, "Decay", "% of a step it falls to SUSTAIN (OFF = stays)");
        knob (c, gatePct, "Gate", "% of a step where the bass is gone (FADE ends here)");
        c.shown = [this] { return raw (proc, ParamIDs::timing) < 0.5f; };
    }
    {
        auto& c = card ("Times (ms)", acc, PEnv); cTimeMs = (int) cards.size() - 1;
        knob (c, delayMs, "Delay", "ms before the bass starts");
        knob (c, holdMs, "Hold", "ms at the top after the swell");
        knob (c, decayMs, "Decay", "ms to fall to SUSTAIN (OFF = stays)");
        knob (c, gateMs, "Gate", "PATTERN: ms where the bass is gone (never past the next step). GATE mode: the note-off ends it");
        c.shown = [this] { return raw (proc, ParamIDs::timing) > 0.5f; };
    }
    {
        auto& c = card ("Trigger", acc, PShape); cTrig = (int) cards.size() - 1;
        knob (c, offset, "Offset", "PATTERN: shifts the steps later (50% of a step = exactly between the grid lines)");
        knob (c, beats, "Beats", "PATTERN: how many beats of bass one note plays");
        c.head.push_back (seg (ParamIDs::step, { "1/4", "1/8", "1/16" }, acc, "PATTERN: the steps on the FL tempo"));
        c.head.push_back (seg (timing, { "Sync", "Ms" }, acc, "SYNC: delay / hold / decay / gate in % of a step (follows the tempo). MS: in milliseconds"));
        c.head.push_back (seg (trigger, { "Pattern", "Gate" }, acc, "PATTERN: a note = the kick, the bass plays the steps between. GATE: a note = one bass note"));
    }
    {
        auto& c = card ("Steps", acc, PShape); cSteps = (int) cards.size() - 1;
        const char* ids[] { step1, step2, step3, step4 };
        for (int i = 0; i < 4; ++i)
            c.knobs.push_back (toggle (ids[i], String (i + 1).toRawUTF8(), Toggle::Chip, acc, "This step of the beat plays (1/4: always step 1, 1/8: 1 + 2, 1/16: 1 - 4)"));
    }
    // ---- PROCESS
    {
        auto& c = card ("Pre filter", cool, PFilter); cPre = (int) cards.size() - 1;
        knob (c, preHz, "Cutoff", "Before the distortion: decides what it bites into. With ENV this is the cutoff at the peak of the swell");
        knob (c, preReso, "Reso", "Resonance: a peak the drive turns into a voice");
        knob (c, preEnv, "Env", "+ = the filter opens as the bass swells (harmonics open late - the reverse feel), - = it closes", true);
        knob (c, preKey, "Key", "Key track: higher notes open the filter more");
        c.head.push_back (seg (preType, { "Off", "LP", "HP", "BP", "Notch" }, cool, "Type of the pre filter"));
    }
    {
        auto& c = card ("Stage 1", hot, PDist); cDrive = (int) cards.size() - 1;
        knob (c, drive, "Drive", "Distortion of the bass (the same stages as 4GE)");
        knob (c, driveEnv, "Env", "0 = the same dirt all the time, 100% = clean at the start, dirtiest at the peak");
        knob (c, bias, "Asym", "Asymmetry: even harmonics, a raw edge", true);
        knob (c, mix, "Mix", "Clean / distorted (harmonic mix)");
        c.head.push_back (seg (distType, { "Soft", "Tube", "Hard", "Fold", "Raw", "Crush" }, hot, distTip));
    }
    {
        auto& c = card ("Stage 2", hot, PDist); cDrive2 = (int) cards.size() - 1;
        knob (c, dist2Drive, "Drive", "The second distortion, after the EQ bell (OFF = none): the presence you boost gets dense and bright");
        knob (c, dist2Mix, "Mix", "Clean / distorted");
        knob (c, dist2Bias, "Asym", "Asymmetry of stage 2: even harmonics", true);
        c.head.push_back (seg (dist2Type, { "Soft", "Tube", "Hard", "Fold", "Raw", "Crush" }, hot, distTip));
    }
    // ---- DISTORTION LAB per stage (CLASSIC = the types on the left)
    for (int s = 0; s < 2; ++s)
    {
        auto& c = card ("Lab " + String (s + 1), hot, PDist); cLab[s] = (int) cards.size() - 1;
        knob (c, labId (s, "hard"), "Hardness", "Character of the LAB curve: SAT / DIODE / FOLD = soft -> hard clip, RECT = plain -> half-wave -> full-wave (octave up), CRUSH = fewer bits + lower rate");
        knob (c, labId (s, "tone"), "Tone", "Tilt around 800 Hz: - darker, + brighter (PRE = what goes into the curve, POST = after it)", true);
        knob (c, labId (s, "out"), "Output", "Level after this stage", true);
        c.head.push_back (seg (labId (s, "tone_post"), { "Pre", "Post" }, hot, "TONE before the curve (changes what distorts) or after it (only the colour)"));
        c.head.push_back (seg (labId (s, "lab"), { "Classic", "Sat", "Diode", "Fold", "Rect", "Crush", "Custom" }, hot,
                               "CLASSIC = the 4GE types on the left. SAT = tanh / clip, DIODE = asymmetric (even harmonics), FOLD = wavefolding, RECT = rectifier (octave), CRUSH = real bitcrusher (aliases on purpose), CUSTOM = your drawn curve (WAVESHAPER page). All but CRUSH are anti-aliased (ADAA)"));
    }
    // ---- WAVESHAPER: the drawn CUSTOM curves
    for (int s = 0; s < 2; ++s)
    {
        auto& c = card ("Custom curve " + String (s + 1), hot, PWaveshaper); cCurve[s] = (int) cards.size() - 1;
        c.shown = [this, s] { return shaperView.stage == s; };
        c.head.push_back (toggle (labId (s, "sym"), "Symmetric", Toggle::Chip, hot, "ON: draw the right half (positive input), the left half mirrors it. OFF: both halves are free (asymmetric = even harmonics)"));
        auto mk = [this, &c] (const char* text, const char* tip, std::function<void()> fn)
        {
            auto b = std::make_unique<TextButton> (text); b->setTooltip (tip); b->onClick = std::move (fn);
            addAndMakeVisible (*b); c.head.push_back (b.get()); templ.push_back (std::move (b));
        };
        mk ("Reset", "Back to the default curve", [this, s] { proc.undo.beginNewTransaction(); proc.resetShape (4 + s); });
        mk ("Save", "Save this curve to Documents / RE4GE / Shapes", [this, s]
        {
            auto* w = new AlertWindow ("Save curve", "Name:", MessageBoxIconType::NoIcon, this);
            w->addTextEditor ("name", "My Curve"); w->addButton ("Save", 1, KeyPress (KeyPress::returnKey)); w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
            w->enterModalState (true, ModalCallbackFunction::create ([this, s, w] (int r)
            {
                if (r == 1) re4ge::shapefiles::save (w->getTextEditorContents ("name"), proc.getShape (4 + s));
            }), true);
        });
        mk ("Load", "Load one of your curves (Documents / RE4GE / Shapes)", [this, s]
        {
            PopupMenu m; const auto files = re4ge::shapefiles::list();
            if (files.isEmpty()) m.addItem (-1, "No curves saved yet", false);
            for (int i = 0; i < files.size(); ++i) m.addItem (i + 1, files[i].getFileNameWithoutExtension());
            m.showMenuAsync (PopupMenu::Options(), [this, s, files] (int r)
            {
                mod::Shape sh;
                if (r > 0 && re4ge::shapefiles::load (files[r - 1], sh)) { proc.undo.beginNewTransaction(); proc.setShape (4 + s, sh); }
            });
        });
    }
    // ---- RE4GE FX: the kicks come in as audio
    if (proc.isFx)
    {
        {
            auto& c = card ("Kick detection", hot, PFx); cFxDet = (int) cards.size() - 1;
            knob (c, fxThreshold, "Threshold", "How loud a hit must be to count as a kick (watch the HITS light)");
            knob (c, fxSens, "Sensitivity", "How sharp the jump must be: more = softer kicks count too, less = only clear hits");
            knob (c, fxHoldoff, "Holdoff", "After a kick, no new one for this long (no double triggers on raw tails)");
            knob (c, fxInput, "Input", "Gain of the detection signal (not of the sound)", true);
            c.head.push_back (seg (fxDetect, { "Input", "Sidechain" }, hot, "INPUT: find the kicks in this channel. SIDECHAIN: in the sidechain input (route the kick channel to it in FL)"));
        }
        {
            auto& c = card ("FX output", hot, PFx); cFxOut = (int) cards.size() - 1;
            knob (c, fxMix, "Bass", "Level of the reverse bass added to the input");
            knob (c, fixedNote, "Note", "The note of the bass (the key of your track)");
            knob (c, smpStart, "Live from", "LIVE: where in the previous kick the reverse starts (skips the click / punch)");
            c.head.push_back (toggle (fxLive, "Live reverse", Toggle::Chip, hot, "ON + SOURCE SAMPLE: the bass is the PREVIOUS kick's tail played backwards (the first kick after silence has none)"));
            c.head.push_back (toggle (fxDry, "Dry", Toggle::Chip, hot, "ON: the kick passes through untouched (the bass is added). OFF: only the bass"));
        }
    }
    // ---- LFO 1-4
    for (int i = 0; i < 4; ++i)
    {
        auto& c = card ("LFO " + String (i + 1), cool, PLfo); cLfo[i] = (int) cards.size() - 1;
        knob (c, lfoId (i, "rate"), "Rate", "Speed in Hz (when SYNC is off)");
        knob (c, lfoId (i, "div"), "Sync", "Speed in note values on the FL tempo (when SYNC is on)");
        knob (c, lfoId (i, "phase"), "Phase", "Start point of the wave");
        c.head.push_back (toggle (lfoId (i, "retrig"), "Retrig", Toggle::Chip, cool, "ON: the LFO starts again on every step (every bass hit moves the same). OFF: it runs freely"));
        c.head.push_back (toggle (lfoId (i, "sync"), "Sync", Toggle::Chip, cool, "ON: the rate follows the FL tempo (note values). OFF: Hz"));
        c.head.push_back (seg (lfoId (i, "wave"), { "Sine", "Tri", "Saw+", "Saw-", "Sqr", "S&H", "Smooth", "Chaos" }, cool,
                               "The wave. S&H = a new random value every cycle, SMOOTH = gliding random, CHAOS = never repeats. Route it on the MATRIX page"));
    }
    // ---- SEQUENCER + GATE (v0.12)
    {
        auto& c = card ("Sequencer", cool, PSeq); cSeq = (int) cards.size() - 1;
        knob (c, seqLen, "Length", "How many steps the pattern has (it repeats). The step size is the trigger STEP (1/4, 1/8, 1/16)");
        knob (c, seqSwing, "Swing", "Every 2nd step comes later (its swell is squeezed, so it still ends before the next step)");
        c.head.push_back (toggle (seqOn, "SEQ", Toggle::Chip, cool, "ON: this 16-step pattern decides which steps play, their level and pitch (on the FL grid). OFF: the trigger STEPS 1-4"));
    }
    {
        auto& c = card ("Gate", hot, PGate); cGate = (int) cards.size() - 1;
        knob (c, gateDepth, "Depth", "How deep the gate cuts (100 % = silence between the hits)");
        knob (c, gateDuty, "Length", "How long each hit of the gate is open (part of the gate step)");
        knob (c, gateSmooth, "Smooth", "Softens the edges of the gate (ms): low = choppy, high = round");
        c.head.push_back (seg (gateBand, { "Full", "Highs" }, hot, "FULL: the whole bass is chopped. HIGHS: only above ~250 Hz - the low end keeps running"));
        c.head.push_back (seg (gateShape, { "Hard", "Soft", "Trance" }, hot, "HARD: on / off. SOFT: rounded hits. TRANCE: each hit opens and fades"));
        c.head.push_back (seg (gateDiv, { "1/8", "1/16", "1/32", "1/8T", "1/16T" }, hot, "Gate speed on the FL tempo"));
        c.head.push_back (toggle (gateOn, "Gate", Toggle::Chip, hot, "ON: cuts the finished bass in rhythm (it does not trigger it)"));
    }
    {
        auto& c = card ("Compressor", hot, PDyn); cComp = (int) cards.size() - 1;
        knob (c, compThresh, "Threshold", "Above this level the bass is turned down");
        knob (c, compRatio, "Ratio", "How hard: 2:1 gentle, 10:1 almost a limiter");
        knob (c, compAttack, "Attack", "How fast it grabs (short = flatter peaks, long = the start punches through)");
        knob (c, compRelease, "Release", "How fast it lets go (long = smoother, short = pumping)");
        knob (c, compMakeup, "Makeup", "Turns the compressed bass up again");
        knob (c, compMix, "Mix", "Parallel: 100 % = only the compressed bass, less = mixed with the original");
        c.head.push_back (toggle (compOn, "Comp", Toggle::Chip, hot, "ON: a compressor on the finished bass (before CLIP / OUTPUT): a denser, more even reverse"));
    }
    {
        auto& c = card ("Quality", n, PSettings); cSettings = (int) cards.size() - 1;
        c.head.push_back (toggle (autoGain, "Auto gain", Toggle::Chip, n, "ON: more DRIVE changes the character, not the loudness (the ear is not fooled by 'louder = better')"));
        c.head.push_back (seg (qualityExport, { "2x", "4x", "8x", "16x" }, n, "Oversampling when FL renders / exports (offline): more = cleaner top, slower export"));
        c.head.push_back (seg (qualityRt, { "2x", "4x", "8x", "16x" }, n, "Oversampling while you play: more = less aliasing on hard distortion, more CPU (4x is the default)"));
    }
    {
        auto& c = card ("EQ", cool, PEq); cPost = (int) cards.size() - 1;
        knob (c, lowCut, "Low cut", "Cuts the DISTORTED part only (the clean SUB is never cut). OFF by default - the low end stays");
        knob (c, highCut, "High cut", "Darkness of the top (after stage 2)");
        knob (c, bellHz, "Bell Hz", "A bell for presence or to cut a ringing / boxy area (before stage 2)");
        knob (c, bellGain, "Bell", "Boost / cut of the bell", true);
        knob (c, bellQ, "Q", "Width of the bell");
    }
    // ---- SOURCE
    {
        auto& c = card ("Kick tail", hot, PTail); cTail = (int) cards.size() - 1;
        knob (c, tailStart, "Start", "TAIL: pitch at the start of the kick tail = START x the note. Backwards it becomes the rise at the end");
        knob (c, tailDrop, "Drop", "TAIL: how long the tail's pitch falls");
        knob (c, tailCurve, "Curve", "TAIL: shape of the fall");
        knob (c, tailWave, "Wave", "TAIL: sine > triangle > saw before the distortion");
        knob (c, tailDrive, "Drive", "TAIL: distortion of the kick tail");
        knob (c, tailTone, "Tone", "TAIL: before the distortion - left = more low end, right = more mids", true);
        knob (c, tailHighCut, "High cut", "TAIL: darkens the tail's distortion");
        knob (c, tailScream, "Scream", "TAIL: the resonant 'yell' of raw kicks");
        knob (c, tailScreamHz, "Scream Hz", "TAIL: where the scream sits");
        knob (c, tailLen, "Length", "TAIL in GATE mode: how much tail is played backwards (in PATTERN mode it fits the step)");
        knob (c, tailRoom, "Room", "TAIL: a room on the kick tail BEFORE it is reversed - backwards it swells up into the bass");
        knob (c, tailRoomSize, "Size", "TAIL: size of that room");
        c.head.push_back (seg (tailDist, { "Soft", "Tube", "Hard", "Fold", "Raw", "Crush" }, hot, "TAIL: the kick's distortion type"));
    }
    {
        auto& c = card ("Sample", acc, PSample); cSample = (int) cards.size() - 1;
        knob (c, smpRoot, "Root", "SAMPLE: the note of your WAV (with KEY TRACK it is played on the note you play). DETECT sets it");
        knob (c, smpStart, "Start", "Where the region that plays begins (backwards: where the bass ends, on the gate). DETECT sets it");
        knob (c, smpEnd, "End", "Where the region ends (backwards: where the bass starts). DETECT sets it to the end of the tail");
        c.head.push_back (seg (smpPitch, { "Varispeed", "Keep len" }, acc, "VARISPEED: another note = faster / slower (like a record). KEEP LENGTH: another note, the same length (grains, like Ancient Drums)"));
        c.head.push_back (seg (smpMode, { "Original", "Processed" }, acc, "ORIGINAL: the kick as it is (the distortion stages are skipped). PROCESSED: through the whole RE4GE chain"));
        for (auto [label, use, tip] : { std::tuple<const char*, int, const char*> { "All", 3, "DETECT: the whole kick plays (reversed: the click lands on the gate end)" },
                                        { "Punch+", 2, "DETECT: everything after the click" },
                                        { "Body+Tail", 1, "DETECT: the body and the tail (no click, no punch)" },
                                        { "Tail", 0, "DETECT: only the tail - the classic reverse bass from a kick" } })
        {
            auto b = std::make_unique<TextButton> (label); b->setTooltip (tip);
            const int u = use;
            b->onClick = [this, u] { proc.undo.beginNewTransaction(); proc.detectKick (u); if (auto* s = proc.apvts.getParameter (ParamIDs::source)) s->setValueNotifyingHost (s->convertTo0to1 (2.f)); };
            addAndMakeVisible (*b); c.head.push_back (b.get()); templ.push_back (std::move (b));
        }
        loadBtn.setTooltip ("SAMPLE source: load a WAV (e.g. a kick from 4GE). You can also drag a file onto the plugin");
        clearBtn.setTooltip ("Remove the sample");
        loadBtn.onClick = [this]
        {
            chooser = std::make_unique<FileChooser> ("Load a sample", File::getSpecialLocation (File::userDocumentsDirectory), "*.wav;*.aif;*.aiff;*.flac");
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                                  [this] (const FileChooser& fc) { if (fc.getResult().existsAsFile() && proc.loadSample (fc.getResult())) proc.detectKick (0); });
        };
        clearBtn.onClick = [this] { proc.clearSample(); };
        addAndMakeVisible (loadBtn); addAndMakeVisible (clearBtn);
        c.head.push_back (&clearBtn); c.head.push_back (&loadBtn);
        c.head.push_back (toggle (smpReverse, "Reverse", Toggle::Chip, acc, "ON: the WAV plays backwards and ends on the gate. OFF: forwards from the swell"));
    }
    {
        auto& c = card ("Sub", n, PSub); cSub = (int) cards.size() - 1;
        knob (c, subHz, "Sub Hz", "Below this the bass is a clean sine on the note (never cut, never distorted). OFF = all from the source");
        knob (c, subLevel, "Level", "Level of the clean sub - the 'bass punch'");
        knob (c, subHarm, "Harmonics", "Clean 2nd / 3rd harmonics of the sub: it is heard on small speakers too");
        knob (c, subSat, "Saturation", "Rounds the sub sine (louder, a bit dirtier). 0 = a pure sine");
        c.head.push_back (seg (subOct, { "0", "-1 oct" }, n, "The sub on the note or an octave below"));
    }
    {
        auto& c = card ("Click", hot, PTransient); cClick = (int) cards.size() - 1;
        knob (c, clickLevel, "Level", "A short noise tick at the start (or the peak) of every bass step - outside the envelope and the distortion. OFF = none");
        knob (c, clickTone, "Tone", "The tick's own high-pass: higher = thinner, sharper");
        knob (c, clickLen, "Length", "How long the tick rings");
        c.head.push_back (seg (clickAt, { "Start", "Peak" }, hot, "START: when the bass begins (after DELAY). PEAK: at the top of the swell"));
    }
    {
        auto& c = card ("Unison / width", acc, PStereo); cUni = (int) cards.size() - 1;
        knob (c, uniVoices, "Voices", "Detuned, slowly drifting copies on the left / right: a full, moving bass (OFF = plain mono). The lows stay mono");
        knob (c, uniDetune, "Detune", "Spread of the copies in cents");
        knob (c, uniDrift, "Drift", "Slow pitch wander of each copy");
        knob (c, uniMix, "Mix", "Level of the copies");
        knob (c, stereoWidth, "Width", "Stereo width of the mids / highs (your reference basses: about 25 - 45%)");
        knob (c, widthSplit, "Mono below", "Below this the bass stays mono");
        knob (c, smear, "Smear", "A short diffuser on the mids / highs: fills the gaps between the harmonics - dense, like the reverb inside reversed-kick samples");
        knob (c, smearSize, "Size", "Size of the smear (short = grainy, long = washed)");
    }
    // ---- SHAPE DESIGNER: one card per lane (the active lane's card is shown)
    for (int i = 0; i < 4; ++i)
    {
        auto& c = card (String ("Lane ") + ParamIDs::laneNames[i], ShapeDesigner::laneColour (i), PDesigner); cLane[i] = (int) cards.size() - 1;
        const char* tips[4] { "How much the drawn AMP replaces the knob envelope (0 = the knobs, 100% = only the drawing)",
                              "Range of the drawn pitch: the top of the lane = +RANGE semitones, the middle = the note, the bottom = -RANGE",
                              "How much the drawn HARMONIC curve opens / closes the bass: it scales the DRIVE and moves the PRE FILTER (when it is on) down by up to 4 octaves",
                              "How much the drawn PHASE curve scales PHASE WARP and the OSC 2-4 PM" };
        knob (c, laneId (i, "depth"), i == 1 ? "Range" : "Depth", tips[i]);
        knob (c, laneId (i, "len"), "Length", "How long the lane is, in % of the step (or of the gate in GATE mode)");
        c.shown = [this, i] { return designer.lane == i; };
        if (i == 0)
        {
            auto b = std::make_unique<TextButton> ("From knobs");
            b->setTooltip ("Draws the current knob envelope as a curve - a start for editing");
            b->onClick = [this] { proc.undo.beginNewTransaction(); proc.shapeFromKnobs (0); };
            addAndMakeVisible (*b); c.head.push_back (b.get()); templ.push_back (std::move (b));
        }
        {
            auto b = std::make_unique<TextButton> ("Reset");
            b->setTooltip ("Back to the default curve of this lane");
            b->onClick = [this, i] { proc.undo.beginNewTransaction(); proc.resetShape (i); };
            addAndMakeVisible (*b); c.head.push_back (b.get()); templ.push_back (std::move (b));
        }
        c.head.push_back (seg (laneId (i, "mode"), { "One shot", "Loop" }, ShapeDesigner::laneColour (i), "ONE SHOT: the curve plays once per step. LOOP: it repeats inside the step"));
        c.head.push_back (seg (laneId (i, "anchor"), { "Start", "End" }, ShapeDesigner::laneColour (i), "START: the lane begins with the step. END: it ends exactly where the bass ends (before the kick) - the reverse way"));
        c.head.push_back (toggle (laneId (i, "on"), "On", Toggle::Chip, ShapeDesigner::laneColour (i), "This lane plays (OFF = the knobs do it)"));
    }
    designer.onLaneChanged = [this] { showPage (page); };
    shaperView.onStageChanged = [this] { showPage (page); };
    addAndMakeVisible (shaperView);
    addAndMakeVisible (matrix);
    addAndMakeVisible (kickView);
    settingsB.setTooltip ("Quality (oversampling while playing / for export), auto gain");
    settingsB.onClick = [this] { showPage (PSettings); };
    addAndMakeVisible (settingsB);
    rndPageB.setTooltip ("RANDOMIZER: mode (which modules), style (the direction), amount, locks");
    rndPageB.onClick = [this] { showPage (PRandom); };
    addAndMakeVisible (rndPageB);
    randomPanel.onRandomized = [this] { lastName = {}; };
    addChildComponent (randomPanel);
    addChildComponent (seqView);
    addChildComponent (eqView);
    addChildComponent (exportPanel);
    exportB.setTooltip ("EXPORT: render the bass to a WAV (and drag it into FL)");
    exportB.onClick = [this] { showPage (PExport); };
    addAndMakeVisible (exportB);
    addAndMakeVisible (designer);
    addAndMakeVisible (view);

    // ---- navigation (the owner's 5 blocks; only working modules are listed)
    auto on = [this] (const char* id, float above) { return [this, id, above] { return raw (proc, id) > above; }; };
    auto oscOn = [this] { for (int o = 2; o <= 4; ++o) if (raw (proc, ParamIDs::oscId (o, "level")) > -59.f) return true; return raw (proc, ParamIDs::osc1Level) > -59.f; };
    nav.blocks = {
        { "01", "SOUND",      { { "Main", PMain, {} }, { "Osc", POsc, oscOn }, { "Pitch", PPitch, [this] { return raw (proc, ParamIDs::pitchMode) > 0.5f; } },
                                { "Envelope", PEnv, [] { return true; } }, { "Shape", PDesigner, [this] { for (int i = 0; i < 4; ++i) if (raw (proc, ParamIDs::laneId (i, "on")) > 0.5f) return true; return false; } } }, {} },
        { "02", "PROCESS",    { { "Filter", PFilter, on (preType, 0.5f) }, { "Distortion", PDist, on (drive, 0.05f) }, { "Waveshaper", PWaveshaper, [this] { return roundToInt (raw (proc, ParamIDs::labId (0, "lab"))) == 6 || roundToInt (raw (proc, ParamIDs::labId (1, "lab"))) == 6; } }, { "Transient", PTransient, on (clickLevel, -59.f) }, { "EQ", PEq, [this] { return std::abs (raw (proc, ParamIDs::bellGain)) > 0.05f || raw (proc, ParamIDs::lowCut) > 20.5f; } } }, {} },
        { "03", "MODULATION", { { "LFO", PLfo, [this] { for (int r = 0; r < 16; ++r) if (raw (proc, ParamIDs::mtxId (r, "on")) > 0.5f && roundToInt (raw (proc, ParamIDs::mtxId (r, "src"))) >= 1 && roundToInt (raw (proc, ParamIDs::mtxId (r, "src"))) <= 4) return true; return false; } },
                                { "Matrix", PMatrix, [this] { for (int r = 0; r < 16; ++r) if (raw (proc, ParamIDs::mtxId (r, "on")) > 0.5f) return true; return false; } },
                                { "Sequencer", PSeq, [this] { return raw (proc, ParamIDs::seqOn) > 0.5f; } },
                                { "Gate", PGate, [this] { return raw (proc, ParamIDs::gateOn) > 0.5f; } },
                                { "Steps", PShape, [] { return true; } } }, {} },
        { "04", "SOURCE",     { { "Audio Input", PFx, [] { return true; } }, { "Kick Tail", PTail, [this] { return roundToInt (raw (proc, ParamIDs::source)) == 1; } }, { "Kick > Reverse", PSample, [this] { return roundToInt (raw (proc, ParamIDs::source)) == 2; } },
                                { "Sub", PSub, on (subHz, 20.5f) }, { "Stereo", PStereo, on (uniVoices, 1.5f) } }, {} } };
    if (! proc.isFx) nav.blocks[3].items.erase (nav.blocks[3].items.begin());      // AUDIO INPUT: only in RE4GE FX
    nav.onSelect = [this] (int pg) { showPage (pg); };
    addAndMakeVisible (nav);

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
    randomB.setTooltip ("A new bass with the RANDOMIZER settings (mode / style / amount / locks: the Randomizer page). One click = one Undo step");
    saveB.setTooltip ("Save to Documents / RE4GE / Presets");
    folderB.setTooltip ("Open the folder with your own presets");
    playB.setTooltip ("Play one note (G1)");
    FlatLookAndFeel::makePrimary (saveB);
    for (auto* c : { (Component*) &presetBox, (Component*) &prev, (Component*) &next, (Component*) &randomB, (Component*) &saveB, (Component*) &folderB,
                     (Component*) &playB, (Component*) &undoB, (Component*) &redoB })
        addAndMakeVisible (c);
    setSize (W, H);
    showPage (proc.isFx ? PFx : PMain);
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
    // FACTORY presets grouped by their category (the part before " - ": CLASSIC, PUMP, SWELL, ...)
    StringArray cats;
    for (int i = 0; i < proc.getNumPrograms(); ++i) cats.addIfNotAlreadyThere (ReProcessor::presetCategory (proc.getProgramName (i)));
    for (auto& cat : cats)
    {
        presetBox.addSectionHeading (cat);
        for (int i = 0; i < proc.getNumPrograms(); ++i) if (ReProcessor::presetCategory (proc.getProgramName (i)) == cat) presetBox.addItem (proc.getProgramName (i), i + 1);
    }
    userFiles = proc.userPresetFiles();
    if (! userFiles.isEmpty())
    {
        presetBox.addSeparator();
        StringArray ucats;
        for (auto& f : userFiles) ucats.addIfNotAlreadyThere (ReProcessor::presetCategory (f.getFileNameWithoutExtension()));
        ucats.sort (true);
        for (auto& cat : ucats)
        {
            presetBox.addSectionHeading ("USER - " + cat);
            for (int i = 0; i < userFiles.size(); ++i)
                if (ReProcessor::presetCategory (userFiles[i].getFileNameWithoutExtension()) == cat) presetBox.addItem (userFiles[i].getFileNameWithoutExtension(), userBase + i);
        }
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
    dialog = std::make_unique<AlertWindow> ("Save preset", "Name (Documents / RE4GE / Presets):", MessageBoxIconType::NoIcon, this);
    dialog->setLookAndFeel (&lnf);
    const String curName = proc.currentPresetName();
    dialog->addTextEditor ("name", curName.startsWith ("Random") ? String ("My Bass") : curName.fromFirstOccurrenceOf (" - ", false, false).isNotEmpty() ? curName.fromFirstOccurrenceOf (" - ", false, false) : curName);
    dialog->addComboBox ("cat", ReProcessor::categoryNames(), "Category");
    if (auto* cb = dialog->getComboBoxComponent ("cat")) cb->setSelectedItemIndex (jmax (0, ReProcessor::categoryNames().indexOf (ReProcessor::presetCategory (curName))), dontSendNotification);
    dialog->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    SafePointer<Canvas> safe (this);
    dialog->enterModalState (true, ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr || safe->dialog == nullptr) return;
        String name = safe->dialog->getTextEditorContents ("name").trim();
        if (auto* cb = safe->dialog->getComboBoxComponent ("cat"))
            if (cb->getText().isNotEmpty() && cb->getText() != "NONE") name = cb->getText() + " - " + name;   // the category is the name's prefix
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
    page = jlimit (0, NumPages - 1, pg);
    nav.page = page;
    for (int bI = 0; bI < (int) nav.blocks.size(); ++bI) for (auto& it : nav.blocks[(size_t) bI].items) if (it.page == page) nav.open = bI;
    nav.repaint();
    view.setVisible (page == PMain || page == PPitch || page == PEnv || page == PShape);
    designer.setVisible (page == PDesigner);
    shaperView.setVisible (page == PWaveshaper);
    matrix.setVisible (page == PMatrix);
    kickView.setVisible (page == PSample);
    randomPanel.setVisible (page == PRandom);
    seqView.setVisible (page == PSeq);
    eqView.setVisible (page == PEq);
    exportPanel.setVisible (page == PExport);
    for (auto& c : cards)
    {
        const bool on = cardOn (c);
        for (auto* k : c.knobs) k->setVisible (on);
        for (auto* h : c.head) h->setVisible (on);
    }
    resized();
    repaint();
}

void Canvas::layoutCard (Card& c, int kw)
{
    const int kh = Knob::heightFor (44);
    auto a = c.area.reduced (16, 10);
    auto head = a.removeFromTop (28);
    for (auto* h : c.head)
    {
        const bool isSeg = dynamic_cast<Segmented*> (h) != nullptr;
        const int w = isSeg ? jmin (segW[h], head.getWidth() - 70) : dynamic_cast<TextButton*> (h) != nullptr ? (h == &clearBtn ? 28 : jmax (64, 9 * dynamic_cast<TextButton*> (h)->getButtonText().length() + 18)) : 92;
        h->setBounds (head.removeFromRight (w).withSizeKeepingCentre (w, 24));
        head.removeFromRight (6);
    }
    a.removeFromTop (4);
    const int cols = jmax (1, (int) c.knobs.size());
    const int perRow = jmax (1, jmin (cols, a.getWidth() / kw));
    for (int i = 0; i < (int) c.knobs.size(); ++i)
    {
        const int row = i / perRow, col = i % perRow;
        Rectangle<int> r (a.getX() + col * kw, a.getY() + row * (kh + 4), kw, kh);
        if (dynamic_cast<Toggle*> (c.knobs[(size_t) i]) != nullptr) r = r.withSizeKeepingCentre (kw - 10, 30);
        c.knobs[(size_t) i]->setBounds (r);
    }
}

void Canvas::resized()
{
    const int m = 16, gap = 12, kw = 74, top = 57, bottomH = 150;
    // top bar: presets centred-right, undo / redo at the right
    const int bx = W - m;
    redoB.setBounds (bx - 62, 13, 62, 30);
    undoB.setBounds (bx - 130, 13, 62, 30);
    next.setBounds (bx - 250, 13, 34, 30);
    presetBox.setBounds (next.getX() - 6 - 300, 13, 300, 30);
    prev.setBounds (presetBox.getX() - 40, 13, 34, 30);
    // left: NAV + SYSTEM; right: the work area; bottom: MACROS + MASTER
    navArea = { 0, top, 200, H - top - bottomH };
    sysArea = navArea.removeFromBottom (208);
    nav.setBounds (navArea);
    {
        auto s = sysArea.reduced (12, 0).withTrimmedTop (34);
        const int bw = (s.getWidth() - 6) / 2;
        auto r1 = s.removeFromTop (34); s.removeFromTop (6);
        saveB.setBounds (r1.removeFromLeft (bw)); r1.removeFromLeft (6); folderB.setBounds (r1);
        auto r2 = s.removeFromTop (34);
        randomB.setBounds (r2.removeFromLeft (bw)); r2.removeFromLeft (6); playB.setBounds (r2);
        s.removeFromTop (6);
        auto r3 = s.removeFromTop (34);
        rndPageB.setBounds (r3.removeFromLeft (bw)); r3.removeFromLeft (6); settingsB.setBounds (r3);
        s.removeFromTop (6);
        auto r4 = s.removeFromTop (34);
        exportB.setBounds (r4.removeFromLeft (bw));
    }
    workArea = { 200 + m, top + 12, W - 200 - 2 * m, H - top - bottomH - 24 };
    for (auto& c : cards) c.area = {};
    // bottom row
    {
        const int y = H - bottomH + 6, h = bottomH - 18;
        cards[(size_t) cMacros].area = { m, y, W - 2 * m - 360 - gap, h };
        cards[(size_t) cMaster].area = { W - m - 360, y, 360, h };
    }
    auto wa = workArea;
    if (view.isVisible()) { view.setBounds (wa.removeFromTop (page == PMain ? 260 : 220)); wa.removeFromTop (gap); }
    const int rowH = 134;
    auto row = [&] (std::initializer_list<std::pair<int, int>> list)
    {
        auto r = wa.removeFromTop (rowH); wa.removeFromTop (gap);
        int x = r.getX();
        for (auto [idx, w] : list) { cards[(size_t) idx].area = { x, r.getY(), w == 0 ? r.getRight() - x : w, rowH }; x += cards[(size_t) idx].area.getWidth() + gap; }
    };
    switch (page)
    {
        case PMain:   { const int half = (wa.getWidth() - gap) / 2 + 40; row ({ { cMSource, half }, { cMEnv, 0 } }); row ({ { cMTone, half }, { cMLow, 0 } }); break; }
        case POsc:    row ({ { cSource, 400 }, { cWarp, 400 }, { cPhase, 0 } }); row ({ { cOsc[0], 0 } }); row ({ { cOsc[1], 0 } }); break;
        case PPitch:  row ({ { cPitch, 0 } }); break;
        case PEnv:    row ({ { cEnv, 0 } }); row ({ { raw (proc, ParamIDs::timing) < 0.5f ? cTimeSync : cTimeMs, 420 } }); break;
        case PShape:  row ({ { cTrig, 520 }, { cSteps, 0 } }); break;
        case PFilter: row ({ { cPre, 520 } }); break;
        case PDist:   row ({ { cDrive, 470 }, { cLab[0], 0 } }); row ({ { cDrive2, 470 }, { cLab[1], 0 } }); break;
        case PWaveshaper: { shaperView.setBounds (wa.removeFromTop (wa.getHeight() - rowH - gap)); wa.removeFromTop (gap); row ({ { cCurve[shaperView.stage], 0 } }); break; }
        case PSettings: row ({ { cSettings, 0 } }); break;
        case PRandom: randomPanel.setBounds (wa.removeFromTop (330)); break;
        case PExport: exportPanel.setBounds (wa.removeFromTop (300)); break;
        case PSeq:    row ({ { cSeq, 520 } }); seqView.setBounds (wa.removeFromTop (240)); break;
        case PGate:   row ({ { cGate, 0 } }); break;
        case PLfo:    for (int i = 0; i < 4; ++i) row ({ { cLfo[i], 0 } }); break;
        case PMatrix: matrix.setBounds (wa); break;
        case PFx:     if (cFxDet >= 0) { row ({ { cFxDet, 0 } }); row ({ { cFxOut, 0 } }); fxLedArea = cards[(size_t) cFxDet].area.reduced (16, 10).removeFromRight (120).withTrimmedTop (40).withHeight (60); } break;
        case PEq:     { eqView.setBounds (wa.removeFromTop (wa.getHeight() - rowH - gap)); wa.removeFromTop (gap); row ({ { cPost, 520 } }); break; }
        case PDyn:    { row ({ { cComp, 0 } }); grArea = cards[(size_t) cComp].area.reduced (16, 10).removeFromRight (150).withTrimmedTop (44).withHeight (60); break; }
        case PTail:   row ({ { cTail, 0 } }); break;
        case PSample: { kickView.setBounds (wa.removeFromTop (wa.getHeight() - rowH - gap)); wa.removeFromTop (gap); row ({ { cSample, 0 } }); break; }
        case PSub:    row ({ { cSub, 420 } }); break;
        case PStereo: row ({ { cUni, 0 } }); break;
        case PTransient: row ({ { cClick, 420 } }); break;
        case PDesigner: { designer.setBounds (wa.removeFromTop (wa.getHeight() - rowH - gap)); wa.removeFromTop (gap); row ({ { cLane[designer.lane], 0 } }); break; }
        default: break;
    }
    if (page == POsc) { auto r = wa.removeFromTop (rowH); cards[(size_t) cOsc[2]].area = r; }
    for (auto& c : cards) if (! c.area.isEmpty() && cardOn (c) && c.page >= 0) layoutCard (c, kw);
    // MACROS: 8 big knobs
    {
        auto a = cards[(size_t) cMacros].area.reduced (16, 8).withTrimmedTop (22);
        auto& k = cards[(size_t) cMacros].knobs;
        const int w = a.getWidth() / 8;
        for (int i = 0; i < 8; ++i) k[(size_t) i]->setBounds (a.getX() + i * w, a.getY(), w, a.getHeight());
    }
    {
        auto a = cards[(size_t) cMaster].area.reduced (16, 8).withTrimmedTop (22);
        meterArea = a.removeFromRight (14).reduced (0, 4);
        a.removeFromRight (8);
        auto& k = cards[(size_t) cMaster].knobs;
        const int w = a.getWidth() / 3;
        for (int i = 0; i < 3; ++i) k[(size_t) i]->setBounds (a.getX() + i * w, a.getY(), w, a.getHeight());
    }
    if (page == PSample) sampleNameArea = cards[(size_t) cSample].area.reduced (16, 10).removeFromTop (28).withTrimmedLeft (90).withWidth (220);
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
    g.drawText (proc.isFx ? "RE4GE FX" : "RE4GE", Rectangle<float> (tx, 0.f, 240.f, 56.f), Justification::centredLeft);
    g.setFont (lnf.font (12.f, Medium)); g.setColour (t.textMute);
    g.drawText ("v" + String (ParamIDs::versionText) + dotSep + "reverse bass", Rectangle<float> (tx + (proc.isFx ? 112.f : 80.f), 0.f, 240.f, 56.f), Justification::centredLeft);
    // NAV / SYSTEM / work area dividers
    g.setColour (t.divider);
    g.fillRect (200, 57, 1, H - 57 - 150);
    g.fillRect (0, H - 150, W, 1);
    g.fillRect (12, sysArea.getY(), 176, 1);
    g.setFont (lnf.font (11.f, Bold, 0.15f)); g.setColour (t.textDim);
    g.drawText ("05  SYSTEM", Rectangle<int> (22, sysArea.getY() + 8, 160, 20), Justification::centredLeft);
    // BPM (from FL)
    g.setFont (lnf.font (12.f, Medium)); g.setColour (t.textMute);
    g.drawText ("BPM " + String (roundToInt (proc.getEngine().tempoNow.load())), Rectangle<int> (W - 16 - 240, 13, 100, 30), Justification::centred);
    for (auto& c : cards) if (! c.area.isEmpty() && cardOn (c)) doomsday::flat::card (g, lnf, c.area, c.title, c.dot);
    if (page == PRandom) doomsday::flat::card (g, lnf, randomPanel.getBounds(), {}, ReTheme::accent);
    if (page == PExport) doomsday::flat::card (g, lnf, exportPanel.getBounds(), {}, ReTheme::accent);
    if (page == PSeq) doomsday::flat::card (g, lnf, seqView.getBounds(), {}, tokens().cool);
    if (page == PEq) doomsday::flat::card (g, lnf, eqView.getBounds(), {}, tokens().cool);
    if (page == PDyn && ! grArea.isEmpty())
    {
        // GAIN REDUCTION of the compressor
        const auto& tk = tokens();
        auto a = grArea.toFloat();
        g.setFont (lnf.font (10.5f, Bold, 0.1f)); g.setColour (tk.textDim);
        g.drawText ("GAIN REDUCTION", a.removeFromTop (16.f), Justification::centredLeft);
        auto bar = a.removeFromTop (10.f);
        g.setColour (tk.control); g.fillRoundedRectangle (bar, 3.f);
        g.setColour (ReTheme::accent); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * jlimit (0.f, 1.f, -grShown / 24.f)), 3.f);
        g.setFont (lnf.font (13.f, SemiBold)); g.setColour (tk.text);
        g.drawText (String (grShown, 1) + " dB", a.withTrimmedTop (4.f), Justification::centredLeft);
    }
}

// ======================================================================= NAV
int NavBar::hit (Point<float> p) const { for (auto& [r, code] : zones) if (r.contains (p)) return code; return -1; }
void NavBar::mouseDown (const MouseEvent& e)
{
    const int h = hit (e.position);
    if (h >= 1000) { const int b = h - 1000; open = b; auto& items = blocks[(size_t) b].items; repaint(); if (! items.empty()) { bool inside = false; for (auto& it : items) inside |= it.page == page; if (! inside && onSelect) onSelect (items.front().page); } }
    else if (h >= 0 && onSelect) onSelect (h);
}
void NavBar::paint (Graphics& g)
{
    auto& lf = lnfOf (*this);
    const auto& tk = tokens();
    zones.clear();
    float y = 12.f;
    const float x = 12.f, w = (float) getWidth() - 24.f;
    for (int b = 0; b < (int) blocks.size(); ++b)
    {
        const auto& bl = blocks[(size_t) b];
        const bool isOpen = b == open, empty = bl.items.empty();
        Rectangle<float> r (x, y, w, 34.f);
        zones.push_back ({ r, 1000 + b });
        if (isOpen && ! empty) { g.setColour (tk.control); g.fillRoundedRectangle (r, 8.f); }
        else if (hover == 1000 + b && ! empty) { g.setColour (tk.control.withAlpha (0.5f)); g.fillRoundedRectangle (r, 8.f); }
        g.setFont (lf.font (12.f, Bold, 0.12f));
        g.setColour (isOpen && ! empty ? ReTheme::accent : tk.textMute);
        g.drawText (bl.num, r.withTrimmedLeft (10.f).withWidth (24.f), Justification::centredLeft);
        g.setColour (empty ? tk.textMute.withAlpha (0.6f) : isOpen ? tk.text : tk.textDim);
        g.drawText (bl.name, r.withTrimmedLeft (36.f), Justification::centredLeft);
        if (! empty) { g.setColour (tk.textMute); g.drawText (isOpen ? "v" : ">", r.withTrimmedRight (10.f), Justification::centredRight); }
        y += 38.f;
        if (empty && bl.later.isNotEmpty())
        {
            g.setFont (lf.font (10.5f, Medium)); g.setColour (tk.textMute.withAlpha (0.7f));
            g.drawText (bl.later, Rectangle<float> (x + 36.f, y - 8.f, w - 36.f, 14.f), Justification::centredLeft);
            y += 10.f;
        }
        if (isOpen)
            for (auto& it : bl.items)
            {
                Rectangle<float> ir (x + 26.f, y, w - 26.f, 28.f);
                zones.push_back ({ ir, it.page });
                const bool act = it.page == page;
                if (act) { g.setColour (tk.surface); g.fillRect (ir); }
                g.setColour (act ? ReTheme::accent : tk.divider); g.fillRect (ir.withWidth (2.f));
                g.setFont (lf.font (12.5f, act ? SemiBold : Medium));
                g.setColour (act ? tk.text : hover == it.page ? tk.textDim : tk.textMute);
                g.drawText (it.name, ir.withTrimmedLeft (12.f), Justification::centredLeft);
                if (it.lit) { g.setColour (it.lit() ? ReTheme::accent : tk.border); g.fillEllipse (ir.getRight() - 14.f, ir.getCentreY() - 3.f, 6.f, 6.f); }
                y += 30.f;
            }
        y += 4.f;
    }
}

void Canvas::paintOverChildren (Graphics& g)
{
    const auto& t = tokens();
    if (! meterArea.isEmpty())
    {
        auto r = meterArea.toFloat();
        g.setColour (t.control); g.fillRoundedRectangle (r, 4.f);
        const float lv = jlimit (0.f, 1.f, (Decibels::gainToDecibels (meterLevel, -48.f) + 48.f) / 50.f);
        g.setColour (meterLevel > 0.99f ? t.hot : Colour (0xffb4bac3)); g.fillRoundedRectangle (r.withTop (r.getBottom() - r.getHeight() * lv), 4.f);
    }
    if (! sampleNameArea.isEmpty())
    {
        const String name = proc.sampleName();
        g.setFont (lnf.font (11.5f, Medium));
        g.setColour (name.isEmpty() ? t.textMute : ReTheme::bassHot);
        g.drawText (name.isEmpty() ? "No WAV - Load or drop one" : name, sampleNameArea, Justification::centredLeft, true);
    }
    if (page == PFx && ! fxLedArea.isEmpty())
    {
        auto led = fxLedArea;
        g.setColour (ReTheme::accent.withAlpha (0.15f + 0.85f * fxBlink)); g.fillEllipse (led.removeFromLeft (24).withSizeKeepingCentre (18, 18).toFloat());
        g.setColour (t.text); g.setFont (lnf.font (12.f, SemiBold));
        g.drawText ("HITS " + String (proc.fxHits.load()), led.withTrimmedLeft (6), Justification::centredLeft);
    }
    if (dragHover)
    {
        g.setColour (ReTheme::accent.withAlpha (0.10f)); g.fillRect (getLocalBounds());
        g.setColour (t.text); g.setFont (lnf.font (22.f, Bold));
        g.drawText ("Drop to use it as the SAMPLE source", getLocalBounds(), Justification::centred);
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
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav;aif;aiff;flac"))
        {
            if (proc.loadSample (File (f)))
            {
                proc.detectKick (0);       // a dropped kick: its TAIL plays backwards right away (DETECT buttons for other parts)
                if (auto* s = proc.apvts.getParameter (ParamIDs::source)) { s->beginChangeGesture(); s->setValueNotifyingHost (s->convertTo0to1 (2.f)); s->endChangeGesture(); }
            }
            break;
        }
    repaint();
}

void Canvas::timerCallback()
{
    view.tick();
    if (designer.isVisible()) designer.tick();
    if (shaperView.isVisible()) shaperView.tick();
    if (kickView.isVisible()) kickView.tick();
    if (seqView.isVisible()) seqView.tick();
    if (eqView.isVisible()) eqView.tick();
    if (page == PDyn)
    {
        const float gr = proc.getEngine().compGrDb();
        grShown = gr < grShown ? gr : grShown + (gr - grShown) * 0.25f;
        repaint (grArea.expanded (4));
    }
    const int mode = raw (proc, ParamIDs::timing) > 0.5f ? 1 : 0;
    if (mode != lastMode) { lastMode = mode; showPage (page); }
    nav.repaint();   // the module dots follow the knobs
    if (proc.isFx) { const int h = proc.fxHits.load(); fxBlink = h != lastFxHits ? 1.f : fxBlink * 0.8f; lastFxHits = h; if (page == PFx) repaint (fxLedArea.expanded (40)); }
    repaint (Rectangle<int> (W - 16 - 240, 13, 100, 30));
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
ReEditor::ReEditor (ReProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (Canvas::W * 6 / 10, Canvas::H * 6 / 10, Canvas::W * 16 / 10, Canvas::H * 16 / 10);
    getConstrainer()->setFixedAspectRatio ((double) Canvas::W / (double) Canvas::H);
    setSize (Canvas::W, Canvas::H);
}
void ReEditor::resized()
{
    canvas.setBounds (0, 0, Canvas::W, Canvas::H);
    canvas.setTransform (AffineTransform::scale ((float) getWidth() / (float) Canvas::W));
}
