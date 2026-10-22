#include "PluginEditor.h"
#include "DoomsdayBrand.h"

using namespace juce;

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
    ColourGradient fill (violet.withAlpha (0.45f), 0.f, plot.getY(), violetDeep.withAlpha (0.25f), 0.f, plot.getBottom(), false);
    g.setGradientFill (fill); g.fillPath (env);
    g.setColour (violetHot); g.strokePath (env, PathStrokeType (1.6f));
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
                       : "ONE BEAT AT " + String (roundToInt (bpm)) + " BPM  -  orange = the kick (your note), violet = the bass" + (p.pitchMode != re::Stable ? String ("  -  dashed = pitch") : String()),
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

Canvas::Canvas (ReProcessor& p) : proc (p), view (p)
{
    using namespace ParamIDs;
    setLookAndFeel (&lnf);
    const Colour vio = ReTheme::violet, hot = tokens().hot, cool = tokens().cool, n { 0xffb4bac3 };
    cards.reserve (20);
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
    // ---- MASTER
    {
        auto& c = card ("Master", n, -1); cMaster = (int) cards.size() - 1;
        knob (c, level, "Level", "Level of the bass");
        knob (c, clip, "Clip", "Soft clip at the end: evens out the peaks of the bass");
        knob (c, subHz, "Sub Hz", "Below this the bass is a clean sine on the note (never cut, never distorted). OFF = all from the source");
        knob (c, subLevel, "Sub level", "Level of the clean sub - the 'bass punch'");
        knob (c, velocity, "Velocity", "0 = every note full level, 1 = MIDI velocity sets the level");
    }
    // ---- page 0: SOURCE
    {
        auto& c = card ("Source", vio, 0); cSource = (int) cards.size() - 1;
        knob (c, blend, "Saw/Pulse", "SYNTH: 0 = saw, 100% = pulse");
        knob (c, width, "Width", "SYNTH: pulse width (50% = square)");
        knob (c, sine, "Sine", "SYNTH: a sine on the note for the body (OFF = none)");
        c.head.push_back (seg (source, { "Synth", "Tail", "Sample" }, vio, "SYNTH: saw / pulse / sine with PHASE WARP. TAIL: a kick tail played backwards (the classic). SAMPLE: your own WAV"));
    }
    {
        auto& c = card ("Phase warp", vio, 0); cWarp = (int) cards.size() - 1;
        knob (c, warpAmt, "Amount", "SYNTH: how much the phase is bent - other harmonics than drive, the 'springy' bass");
        knob (c, warpEnv, "Env", "0 = the warp is constant, 100% = it opens with the envelope (the bass opens as it swells)");
        c.head.push_back (seg (warp, { "Off", "Bend", "Squeeze", "Sync", "Mirror" }, vio, "BEND: sine -> saw. SQUEEZE: nasal, square-ish. SYNC: talking, like hard sync. MIRROR: even harmonics, metallic"));
    }
    {
        auto& c = card ("Sample", vio, 0); cSample = (int) cards.size() - 1;
        knob (c, smpRoot, "Root", "SAMPLE: the note of your WAV (with KEY TRACK it is played on the note you play)");
        knob (c, smpStart, "Start", "SAMPLE: skips the start of the WAV");
        loadBtn.setTooltip ("SAMPLE source: load a WAV (e.g. a kick from 4GE). You can also drag a file onto the plugin");
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
        c.head.push_back (toggle (smpReverse, "Reverse", Toggle::Chip, vio, "ON: the WAV plays backwards and ends on the gate. OFF: forwards from the swell"));
    }
    {
        auto& c = card ("Kick tail", hot, 0); cTail = (int) cards.size() - 1;
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
        c.head.push_back (seg (tailDist, { "Soft", "Tube", "Hard", "Fold", "Raw", "Crush" }, hot, "TAIL: the kick's distortion type"));
    }
    // ---- page 1: SHAPE
    {
        auto& c = card ("Trigger", vio, 1); cTrig = (int) cards.size() - 1;
        knob (c, offset, "Offset", "PATTERN: shifts the steps later (50% of a step = exactly between the grid lines)");
        knob (c, beats, "Beats", "PATTERN: how many beats of bass one note plays (write a note on every kick, or one long pattern)");
        c.head.push_back (seg (ParamIDs::step, { "1/4", "1/8", "1/16" }, vio, "PATTERN: the steps on the FL tempo"));
        c.head.push_back (seg (timing, { "Sync", "Ms" }, vio, "SYNC: delay / hold / decay / gate in % of a step (follows the tempo). MS: in milliseconds. SWELL is always in ms"));
        c.head.push_back (seg (trigger, { "Pattern", "Gate" }, vio, "PATTERN: a note = the kick, the bass plays the steps between. GATE: a note = one bass note (a bass line in the piano roll)"));
    }
    {
        auto& c = card ("Steps", vio, 1);
        const char* ids[] { step1, step2, step3, step4 };
        for (int i = 0; i < 4; ++i)
            c.knobs.push_back (toggle (ids[i], String (i + 1).toRawUTF8(), Toggle::Chip, vio, "This step of the beat plays (1/4: always step 1, 1/8: 1 + 2, 1/16: 1 - 4)"));
    }
    {
        auto& c = card ("Pitch", vio, 1); cPitch = (int) cards.size() - 1;
        knob (c, pitchAmt, "Amount", "How far the pitch moves (semitones): DROP from it down, RISE up to it at the end", true);
        knob (c, pitchTime, "Time", "How long the move takes");
        knob (c, tune, "Tune", "Bass pitch against the note (12 = an octave up)", true);
        knob (c, fine, "Fine", "Fine tune in cents", true);
        knob (c, fixedNote, "Note", "KEY TRACK off: the bass always plays this note (the notes only give the rhythm)");
        c.head.push_back (toggle (keyTrack, "Key track", Toggle::Chip, vio, "ON: the bass plays the note you play. OFF: always on NOTE"));
        c.head.push_back (seg (pitchMode, { "Stable", "Drop", "Rise", "Bounce" }, vio, "STABLE on the note, DROP from above at the start, RISE at the end (the classic reverse), BOUNCE both"));
    }
    {
        auto& c = card ("Envelope", vio, 1); cEnv = (int) cards.size() - 1;
        knob (c, duck, "Duck", "Level before DELAY (0 = silent while the kick plays)");
        knob (c, initial, "Initial", "Where the swell starts (0 = from silence)");
        knob (c, swell, "Swell", "How long the bass takes to come up (always ms; FHSP references: 50 - 80 ms)");
        knob (c, curve, "Curve", "+ = fast, then landing (sidechain release), - = slow start, late swell, 0 = straight", true);
        knob (c, sustain, "Sustain", "Where DECAY ends (100% = no decay, a level shelf like PUMP)");
        knob (c, decayCurve, "Dec curve", "Shape of the decay: low = fast drop then a long tail, high = holds then falls");
        knob (c, fade, "Fade", "The cut at the end of the gate (smooth, no click before the kick)");
        for (int i = 0; i < ReProcessor::templateNames().size(); ++i)
        {
            auto b = std::make_unique<TextButton> (ReProcessor::templateNames()[i]);
            b->setTooltip ("Template: sets the envelope (and the step for PULSE), the sound stays");
            b->onClick = [this, i] { proc.applyTemplate (i); };
            addAndMakeVisible (*b);
            templ.push_back (std::move (b));
        }
        for (int i = (int) templ.size() - 1; i >= 0; --i) c.head.push_back (templ[(size_t) i].get());
    }
    {
        auto& c = card ("Times (sync)", vio, 1); cTimeSync = (int) cards.size() - 1;
        knob (c, delayPct, "Delay", "% of a step before the bass starts (the kick plays here)");
        knob (c, holdPct, "Hold", "% of a step the bass stays at the top after the swell");
        knob (c, decayPct, "Decay", "% of a step it falls to SUSTAIN (OFF = stays)");
        knob (c, gatePct, "Gate", "% of a step where the bass is gone (FADE ends here)");
        c.shown = [this] { return raw (proc, ParamIDs::timing) < 0.5f; };
    }
    {
        auto& c = card ("Times (ms)", vio, 1); cTimeMs = (int) cards.size() - 1;
        knob (c, delayMs, "Delay", "ms before the bass starts");
        knob (c, holdMs, "Hold", "ms at the top after the swell");
        knob (c, decayMs, "Decay", "ms to fall to SUSTAIN (OFF = stays)");
        knob (c, gateMs, "Gate", "PATTERN: ms where the bass is gone (never past the next step). GATE mode: the note-off ends it");
        c.shown = [this] { return raw (proc, ParamIDs::timing) > 0.5f; };
    }
    // ---- page 2: TONE
    {
        auto& c = card ("Pre filter", cool, 2); cPre = (int) cards.size() - 1;
        knob (c, preHz, "Cutoff", "Before the drive: decides what the distortion bites into (HP = it bites the mids, not the low end)");
        knob (c, preReso, "Reso", "Resonance: a peak the drive turns into a voice");
        c.head.push_back (seg (preType, { "Off", "LP", "HP", "BP", "Notch" }, cool, "Type of the pre filter"));
    }
    {
        auto& c = card ("Drive", hot, 2); cDrive = (int) cards.size() - 1;
        knob (c, drive, "Drive", "Distortion of the bass (the same stages as 4GE)");
        knob (c, driveEnv, "Env", "0 = the same dirt all the time, 100% = clean at the start, dirtiest at the peak");
        knob (c, bias, "Asym", "Asymmetry: even harmonics, a raw edge", true);
        knob (c, mix, "Mix", "Clean / distorted (harmonic mix)");
        c.head.push_back (seg (distType, { "Soft", "Tube", "Hard", "Fold", "Raw", "Crush" }, hot, "Distortion type"));
    }
    {
        auto& c = card ("Post EQ", cool, 2); cPost = (int) cards.size() - 1;
        knob (c, lowCut, "Low cut", "Cuts the DISTORTED part only (the clean SUB is never cut). OFF by default - the low end stays");
        knob (c, highCut, "High cut", "Darkness: the FHSP basses have little above 1 - 2.5 kHz");
        knob (c, bellHz, "Bell Hz", "A bell for presence (500 - 1000 Hz) or to cut a ringing peak");
        knob (c, bellGain, "Bell", "Boost / cut of the bell (gently: too much 1 - 2.5 kHz sounds 'flat')", true);
        knob (c, bellQ, "Q", "Width of the bell");
    }
    addAndMakeVisible (view);
    tabs = std::make_unique<TabStrip> (StringArray { "Source", "Shape", "Tone" }, vio);
    tabs->onSelect = [this] (int i) { showPage (i); };
    addAndMakeVisible (*tabs);

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
    randomB.setTooltip ("A new bass: a random template + source + tone (the low end stays)");
    saveB.setTooltip ("Save to Documents / RE4GE / Presets");
    folderB.setTooltip ("Open the folder with your own presets");
    playB.setTooltip ("Play one note (G1)");
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
    dialog = std::make_unique<AlertWindow> ("Save preset", "Name (Documents / RE4GE / Presets):", MessageBoxIconType::NoIcon, this);
    dialog->setLookAndFeel (&lnf);
    dialog->addTextEditor ("name", proc.currentPresetName().startsWith ("Random") ? String ("My Bass") : proc.currentPresetName());
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
        const int w = isSeg ? jmin (segW[h], head.getWidth() - 70) : dynamic_cast<TextButton*> (h) != nullptr ? (h == &clearBtn ? 28 : 64) : 92;
        h->setBounds (head.removeFromRight (w).withSizeKeepingCentre (w, 24));
        head.removeFromRight (6);
    }
    a.removeFromTop (4);
    const int cols = jmax (1, (int) c.knobs.size());
    const int perRow = jmax (1, jmin (cols, a.getWidth() / kw));
    for (int i = 0; i < cols; ++i)
    {
        const int row = i / perRow, col = i % perRow;
        Rectangle<int> r (a.getX() + col * kw, a.getY() + row * (kh + 4), kw, kh);
        if (dynamic_cast<Toggle*> (c.knobs[(size_t) i]) != nullptr) r = r.withSizeKeepingCentre (kw - 10, 30);
        c.knobs[(size_t) i]->setBounds (r);
    }
}

void Canvas::resized()
{
    const int m = 20, gap = 12, kw = 74;
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
    auto& master = cards[(size_t) cMaster];
    master.area = { W - m - 430, 68, 430, 200 };
    view.setBounds (m, 68, master.area.getX() - gap - m, 200);
    tabs->setBounds (m, 280, 330, 30);
    const int y1 = 318, rowH = 134, y2 = y1 + rowH + gap;
    auto place = [&] (std::initializer_list<std::pair<int, int>> list, int y)
    {
        int x = m;
        for (auto [idx, w] : list) { cards[(size_t) idx].area = { x, y, w == 0 ? W - m - x : w, rowH }; x += cards[(size_t) idx].area.getWidth() + gap; }
    };
    for (auto& c : cards) if (c.page >= 0) c.area = {};
    const int cSteps = cTrig + 1;
    if (page == 0)
    {
        place ({ { cSource, 340 }, { cWarp, 420 }, { cSample, 0 } }, y1);
        place ({ { cTail, 0 } }, y2);
    }
    else if (page == 1)
    {
        place ({ { cTrig, 440 }, { cSteps, 340 }, { cPitch, 0 } }, y1);
        const int tCard = raw (proc, ParamIDs::timing) < 0.5f ? cTimeSync : cTimeMs;
        place ({ { cEnv, W - 2 * m - 340 - gap }, { tCard, 0 } }, y2);
    }
    else
    {
        place ({ { cPre, 440 }, { cDrive, 0 } }, y1);
        place ({ { cPost, 0 } }, y2);
    }
    for (auto& c : cards) if (! c.area.isEmpty() && cardOn (c)) layoutCard (c, c.page < 0 ? 96 : kw);
    {
        auto a = master.area.reduced (16, 10);
        a.removeFromTop (32);
        meterArea = a.removeFromRight (14).reduced (0, 4);
        a.removeFromRight (8);
        const int w = a.getWidth() / 5;
        for (int i = 0; i < 5; ++i) master.knobs[(size_t) i]->setBounds (a.getX() + i * w, a.getY() + 20, w, Knob::heightFor (44));
    }
    if (page == 0) sampleNameArea = cards[(size_t) cSample].area.reduced (16, 10).removeFromTop (28).withTrimmedLeft (90).withTrimmedRight (210);
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
    g.drawText ("RE4GE", Rectangle<float> (tx, 0.f, 240.f, 56.f), Justification::centredLeft);
    g.setFont (lnf.font (12.f, Medium)); g.setColour (t.textMute);
    g.drawText ("v" + String (ParamIDs::versionText) + dotSep + "reverse bass", Rectangle<float> (tx + 80.f, 0.f, 240.f, 56.f), Justification::centredLeft);
    for (auto& c : cards) if (! c.area.isEmpty() && cardOn (c)) doomsday::flat::card (g, lnf, c.area, c.title, c.dot);
}

void Canvas::paintOverChildren (Graphics& g)
{
    const auto& t = tokens();
    if (! meterArea.isEmpty())
    {
        auto r = meterArea.toFloat();
        g.setColour (t.control); g.fillRoundedRectangle (r, 4.f);
        const float lv = jlimit (0.f, 1.f, (Decibels::gainToDecibels (meterLevel, -48.f) + 48.f) / 50.f);
        g.setColour (meterLevel > 0.99f ? t.hot : ReTheme::violet); g.fillRoundedRectangle (r.withTop (r.getBottom() - r.getHeight() * lv), 4.f);
    }
    if (! sampleNameArea.isEmpty())
    {
        const String name = proc.sampleName();
        g.setFont (lnf.font (11.5f, Medium));
        g.setColour (name.isEmpty() ? t.textMute : ReTheme::violetHot);
        g.drawText (name.isEmpty() ? "No WAV - Load or drop one" : name, sampleNameArea, Justification::centredLeft, true);
    }
    if (dragHover)
    {
        g.setColour (ReTheme::violet.withAlpha (0.10f)); g.fillRect (getLocalBounds());
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
                if (auto* s = proc.apvts.getParameter (ParamIDs::source)) { s->beginChangeGesture(); s->setValueNotifyingHost (s->convertTo0to1 (2.f)); s->endChangeGesture(); }
            break;
        }
    repaint();
}

void Canvas::timerCallback()
{
    view.tick();
    const int mode = raw (proc, ParamIDs::timing) > 0.5f ? 1 : 0;
    if (mode != lastMode) { lastMode = mode; showPage (page); }
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
