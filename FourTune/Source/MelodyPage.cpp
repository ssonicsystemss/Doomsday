// 4TUNE - MELODY page (GUI only): generator controls -> TuneProcessor (state child MELODY), the piano roll, DRAG MIDI.
#include "PluginEditor.h"
#include "Melody/MelodyFile.h"

using namespace juce;
using namespace doomsday::flat;
using namespace tuneui;

namespace tuneui
{
    // a flat knob for a non-automatable setting (the generator): label above, value below
    class ValueKnob : public Component
    {
    public:
        ValueKnob (const String& text, Colour accent, std::function<void (float)> onSet, std::function<String (float)> fmt)
            : label (text), set (std::move (onSet)), format (std::move (fmt))
        {
            s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (Slider::TextBoxBelow, true, 72, 18);
            s.setRange (0.0, 1.0, 0.0);
            s.getProperties().set ("accent", (int64) accent.getARGB());
            s.getProperties().set ("valueSize", 13.f);
            s.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
            s.setColour (Slider::textBoxTextColourId, tokens().text);
            s.textFromValueFunction = [this] (double v) { return format ? format ((float) v) : String (roundToInt (v * 100)) + "%"; };
            s.onDragEnd = [this] { if (set) set ((float) s.getValue()); };
            s.setMouseDragSensitivity (220);
            addAndMakeVisible (s);
        }
        void setValueQuiet (float v) { if (! s.isMouseButtonDown()) s.setValue (v, dontSendNotification); s.updateText(); }
        void paint (Graphics& g) override
        {
            auto& lnf = lnfOf (*this);
            g.setColour (tokens().textMute); g.setFont (lnf.font (11.f, SemiBold, 0.06f));
            g.drawText (label, getLocalBounds().removeFromTop (16), Justification::centred);
        }
        void resized() override { s.setBounds (getLocalBounds().withTrimmedTop (16)); }
        Slider s;
    private:
        String label;
        std::function<void (float)> set;
        std::function<String (float)> format;
    };

    // drag this into FL Studio = the melody as a .mid file (pitch, length, timing, velocity, the whole phrase)
    class MidiDragSource : public Component, public SettableTooltipClient
    {
    public:
        explicit MidiDragSource (TuneProcessor& p) : proc (p) { setMouseCursor (MouseCursor::DraggingHandCursor); setTooltip ("Drag the melody into FL Studio (a .mid file)"); }
        void paint (Graphics& g) override
        {
            auto& lnf = lnfOf (*this);
            auto r = getLocalBounds().toFloat().reduced (1.f);
            g.setGradientFill (ColourGradient (groupA().withAlpha (0.85f), r.getX(), r.getY(), groupB().withAlpha (0.85f), r.getRight(), r.getBottom(), false));
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (Colour (0xff0c0a10)); g.setFont (lnf.font (12.5f, Bold, 0.1f));
            g.drawText ("DRAG MIDI  >>  FL", r, Justification::centred);
        }
        void mouseDrag (const MouseEvent& e) override
        {
            if (dragging || e.getDistanceFromDragStart() < 4) return;
            dragging = true;
            const auto m = proc.melodySettings();
            const String name = "4TUNE " + String (tune::melody::modeName (m.mode)) + " " + tune::melody::keyName (m.key) + " " + tune::melody::scaleName (m.scale);
            const auto f = tune::melody::exportForDrag (proc.melodyNotes(), proc.hostBpm(), name.toLowerCase().replace (" ", "_"));
            DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragging = false; });
        }
        void mouseUp (const MouseEvent&) override { dragging = false; }
    private:
        TuneProcessor& proc;
        bool dragging = false;
    };
}

MelodyPage::MelodyPage (TuneProcessor& p) : Page (p)
{
    auto combo = [this] (ComboBox& c, const StringArray& items, std::function<void (int)> onPick)
    {
        c.addItemList (items, 1);
        c.onChange = [&c, onPick] { onPick (c.getSelectedItemIndex()); };
        addAndMakeVisible (c);
    };
    StringArray keys; for (int k = 0; k < 12; ++k) keys.add (tune::melody::keyName (k));
    StringArray scales; for (int k = 0; k < tune::melody::numScales; ++k) scales.add (tune::melody::scaleName (k));
    StringArray modes; for (int k = 0; k < tune::melody::numModes; ++k) modes.add (tune::melody::modeName (k));
    auto regen = [this] { proc.generateMelody (false); };
    combo (key, keys, [this, regen] (int i) { proc.setMelodySetting ("key", i); regen(); });
    combo (scale, scales, [this, regen] (int i) { proc.setMelodySetting ("scale", i); resized(); regen(); });
    combo (mode, modes, [this, regen] (int i) { proc.setMelodySetting ("mode", i); regen(); });
    combo (bars, { "1 BAR", "2 BARS", "4 BARS", "8 BARS" }, [this, regen] (int i) { proc.setMelodySetting ("bars", 1 << i); regen(); });
    combo (range, { "OCT 3-4", "OCT 4-5", "OCT 5-6", "OCT 4-6", "OCT 3-5" }, [this, regen] (int i)
    {
        static const int lo[] { 3, 4, 5, 4, 3 }, hi[] { 4, 5, 6, 6, 5 };
        proc.setMelodySetting ("octLow", lo[i]); proc.setMelodySetting ("octHigh", hi[i]); regen();
    });
    key.setTooltip ("Key"); scale.setTooltip ("Scale (CUSTOM: pick the notes below)"); mode.setTooltip ("Hardstyle mode: changes the rules, not just the notes");
    bars.setTooltip ("Phrase length"); range.setTooltip ("Octave range of the melody");

    struct K { const char* label; const char* id; const char* tip; };
    static const K ks[] { { "DENSITY", "density", "How many notes" }, { "COMPLEXITY", "complexity", "Syncopation, passing notes, rhythm variety" },
                          { "REPETITION", "repetition", "How much the motif comes back" }, { "MOVEMENT", "movement", "Interval size and melodic range" },
                          { "RHYTHM", "rhythm", "Rhythm grid: 1/4 -> 1/8 -> shuffle 1/8T -> 1/16" } };
    for (auto& k : ks)
    {
        const Identifier id (k.id);
        auto vk = std::make_unique<ValueKnob> (k.label, String (k.id) == "rhythm" ? groupB() : groupA(),
                                               [this, id, regen] (float v) { proc.setMelodySetting (id, v); regen(); },
                                               String (k.id) == "rhythm" ? std::function<String (float)> ([] (float v)
                                               { return String (v < 0.25f ? "1/4" : v < 0.6f ? "1/8" : v < 0.8f ? "1/8T" : "1/16"); }) : nullptr);
        vk->s.setTooltip (k.tip);
        addAndMakeVisible (*vk); genKnobs.push_back (std::move (vk));
    }
    for (auto* b : { &generate, &newSeed, &bounce, &preview }) addAndMakeVisible (*b);
    generate.setTooltip ("Generate the melody from these settings (Ctrl+Z undoes it)");
    newSeed.setTooltip ("A new melody with the same settings");
    bounce.setTooltip ("The low chord root on the off-beats (the classic hardstyle bounce)");
    preview.setTooltip ("Play the melody through 4TUNE (on the FL grid when FL plays, on its own when stopped)");
    bounce.setClickingTogglesState (true); preview.setClickingTogglesState (true);
    generate.onClick = [this] { proc.generateMelody (false); };
    newSeed.onClick = [this] { proc.generateMelody (true); };
    bounce.onClick = [this, regen] { proc.setMelodySetting ("bounce", bounce.getToggleState()); regen(); };
    preview.onClick = [this] { proc.melodyPreview.store (preview.getToggleState()); };
    generate.setColour (TextButton::buttonColourId, fortune().withAlpha (0.85f));
    generate.setColour (TextButton::textColourOffId, Colour (0xff0c0a10));
    for (auto* b : { &bounce, &preview }) b->setColour (TextButton::buttonOnColourId, groupA().withAlpha (0.8f));
    for (int k = 0; k < 12; ++k)
    {
        auto& b = custom[(size_t) k];
        b.setButtonText (tune::melody::keyName (k)); b.setClickingTogglesState (true);
        b.setColour (TextButton::buttonOnColourId, groupB().withAlpha (0.8f));
        b.onClick = [this, regen]
        {
            int mask = 0; for (int i = 0; i < 12; ++i) if (custom[(size_t) i].getToggleState()) mask |= 1 << i;
            proc.setMelodySetting ("mask", mask); regen();
        };
        addChildComponent (b);
    }

    pianoRoll = std::make_unique<PianoRoll> (p);
    pianoRoll->leadColour = [] { return groupB(); };
    pianoRoll->bounceColour = [] { return groupA(); };
    pianoRoll->accent = [] { return fortune(); };
    addAndMakeVisible (*pianoRoll);
    gridBox.addItemList ({ "GRID 1/4", "GRID 1/8", "GRID 1/8T", "GRID 1/16", "GRID 1/32" }, 1);
    gridBox.onChange = [this] { static const double g[] { 1.0, 0.5, 1.0 / 3.0, 0.25, 0.125 }; pianoRoll->setGrid (g[jmax (0, gridBox.getSelectedItemIndex())]); };
    gridBox.setSelectedItemIndex (2, dontSendNotification); pianoRoll->setGrid (1.0 / 3.0);
    addAndMakeVisible (gridBox);

    struct T { const char* text; const char* tip; std::function<void()> fn; };
    const std::vector<T> ts {
        { "QUANTIZE", "Snap to the grid (Q)", [this] { pianoRoll->quantizeSel(); } },
        { "HUMANIZE", "A little timing / velocity life", [this] { pianoRoll->humanizeSel(); } },
        { "-1", "Transpose down a semitone (Down)", [this] { pianoRoll->transposeSel (-1); } },
        { "+1", "Transpose up a semitone (Up)", [this] { pianoRoll->transposeSel (1); } },
        { "OCT -", "An octave down (Shift+Down)", [this] { pianoRoll->transposeSel (-12); } },
        { "OCT +", "An octave up (Shift+Up)", [this] { pianoRoll->transposeSel (12); } },
        { "COPY", "Copy (Ctrl+C)", [this] { pianoRoll->copySel(); } },
        { "PASTE", "Paste after the selection (Ctrl+V)", [this] { pianoRoll->paste(); } },
        { "DUPLICATE", "Duplicate (Ctrl+D)", [this] { pianoRoll->duplicateSel(); } },
        { "DELETE", "Delete the selection (Del)", [this] { pianoRoll->deleteSel(); } },
        { "CLEAR", "Remove every note", [this] { pianoRoll->clearAll(); } },
        { "UNDO", "Ctrl+Z", [this] { proc.undo.undo(); pianoRoll->reload(); } },
        { "REDO", "Ctrl+Y", [this] { proc.undo.redo(); pianoRoll->reload(); } } };
    for (auto& t : ts)
    {
        auto b = std::make_unique<TextButton> (t.text);
        b->setTooltip (t.tip); b->onClick = t.fn;
        addAndMakeVisible (*b); tools.push_back (std::move (b));
    }
    dragSrc = std::make_unique<MidiDragSource> (p);
    addAndMakeVisible (*dragSrc);
    syncFromState();
    startTimerHz (8);
}

MelodyPage::~MelodyPage() { stopTimer(); }

void MelodyPage::syncFromState()
{
    const auto m = proc.melodySettings();
    key.setSelectedItemIndex (m.key, dontSendNotification);
    scale.setSelectedItemIndex (m.scale, dontSendNotification);
    mode.setSelectedItemIndex (m.mode, dontSendNotification);
    bars.setSelectedItemIndex (m.bars <= 1 ? 0 : m.bars <= 2 ? 1 : m.bars <= 4 ? 2 : 3, dontSendNotification);
    const int ri = m.octLow == 3 && m.octHigh == 4 ? 0 : m.octLow == 5 ? 2 : m.octLow == 4 && m.octHigh == 6 ? 3 : m.octLow == 3 ? 4 : 1;
    range.setSelectedItemIndex (ri, dontSendNotification);
    const float vals[] { m.density, m.complexity, m.repetition, m.movement, m.rhythm };
    for (size_t i = 0; i < genKnobs.size(); ++i) genKnobs[i]->setValueQuiet (vals[i]);
    bounce.setToggleState (m.rootBounce, dontSendNotification);
    preview.setToggleState (proc.melodyPreview.load(), dontSendNotification);
    for (int k = 0; k < 12; ++k) custom[(size_t) k].setToggleState ((m.customMask >> k) & 1, dontSendNotification);
    const bool showCustom = m.scale == tune::melody::Custom;
    for (auto& b : custom) b.setVisible (showCustom);
}

void MelodyPage::timerCallback() { syncFromState(); }

void MelodyPage::resized()
{
    cards.clear();
    const Rectangle<int> gen (0, 0, getWidth(), 186), roll (0, 196, getWidth(), getHeight() - 196);
    cards.push_back ({ "MELODY GENERATOR", fortune(), gen });
    cards.push_back ({ "PIANO ROLL", groupB(), roll });
    // generator: the musical choices on the left, the character knobs in the middle, GENERATE on the right
    int x = gen.getX() + 16; const int y = gen.getY() + 52;
    key.setBounds (x, y, 70, 28); x += 78;
    scale.setBounds (x, y, 168, 28); x += 176;
    mode.setBounds (x, y, 150, 28); x += 158;
    bars.setBounds (x, y, 96, 28); x += 104;
    range.setBounds (x, y, 98, 28); x += 106;
    bounce.setBounds (gen.getX() + 16, y + 40, 150, 28);
    for (int k = 0; k < 12; ++k) custom[(size_t) k].setBounds (gen.getX() + 176 + k * 44, y + 40, 40, 28);
    int kx = x + 8;
    for (auto& k : genKnobs) { k->setBounds (kx, gen.getY() + 34, 88, Knob::heightFor (40) + 16); kx += 92; }
    generate.setBounds (gen.getRight() - 196, gen.getY() + 46, 180, 50);
    newSeed.setBounds (gen.getRight() - 196, gen.getY() + 106, 180, 34);
    // piano roll: tools on top, DRAG MIDI on the right
    int tx = roll.getX() + 16; const int ty = roll.getY() + 44;
    preview.setBounds (tx, ty, 96, 28); tx += 104;
    gridBox.setBounds (tx, ty, 108, 28); tx += 118;
    for (auto& b : tools) { const int w = b->getButtonText().length() <= 2 ? 36 : 72; b->setBounds (tx, ty, w, 28); tx += w + 6; }
    dragSrc->setBounds (roll.getRight() - 196, ty - 2, 180, 32);
    pianoRoll->setBounds (roll.getX() + 12, ty + 40, roll.getWidth() - 24, roll.getBottom() - ty - 52);
}

void MelodyPage::paintOverChildren (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    g.setFont (lnf.font (11.f, Medium)); g.setColour (tokens().textMute);
    const auto gen = cards.front().area;
    const auto m = proc.melodySettings();
    g.drawText ("Double-click = add   drag = move   right edge = length   right-click = delete   bottom lane = velocity",
                Rectangle<int> (gen.getX() + 16, gen.getBottom() - 30, 760, 20), Justification::centredLeft);
    g.drawText (String (tune::melody::modeName (m.mode)) + "  |  " + tune::melody::keyName (m.key) + " " + tune::melody::scaleName (m.scale)
                    + "  |  " + String ((int) proc.melodyNotes().size()) + " notes",
                Rectangle<int> (gen.getRight() - 420, gen.getBottom() - 30, 404, 20), Justification::centredRight);
}
