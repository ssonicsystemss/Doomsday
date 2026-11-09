#include "PresetPage.h"
#include "Palette.h"

using namespace juce;

namespace fge::gui
{
    PresetPage::PresetPage (GeProcessor& p, doomsday::ui::PresetBar& b) : proc (p), bar (b), lib (p)
    {
        const auto& c = palette();
        search.setTextToShowWhenEmpty ("search: name, tag, notes...", c.textDim);
        search.onTextChange = [this] { apply(); };
        search.setTooltip ("Every word must be in the name, the tags or the notes.");
        styleBox.addItem ("ALL STYLES", 1); for (int s = 0; s < 4; ++s) styleBox.addItem (fge::lib::styleNames()[s], s + 2);
        lengthBox.addItem ("ALL LENGTHS", 1); for (int s = 0; s < 4; ++s) lengthBox.addItem (fge::lib::lengthNames()[s], s + 2);
        sourceBox.addItemList ({ "ALL", "FACTORY", "USER", "FAVORITES", "RECENT" }, 1);
        tagBox.addItem ("ALL TAGS", 1);
        for (auto* cb : { &styleBox, &lengthBox, &sourceBox, &tagBox }) { cb->setSelectedId (1, dontSendNotification); cb->onChange = [this] { apply(); }; addAndMakeVisible (cb); }
        styleBox.setTooltip ("Only this STYLE."); lengthBox.setTooltip ("Only this LENGTH (SHORT < 250 ms, MEDIUM < 500, LONG < 1000, EXTRA LONG).");
        sourceBox.setTooltip ("FACTORY / USER presets, your FAVORITES (the star), RECENTLY USED."); tagBox.setTooltip ("Character / kick type tags (made from the settings + your own).");
        list.setRowHeight (24); list.setColour (ListBox::backgroundColourId, c.ground);
        addAndMakeVisible (search); addAndMakeVisible (list);
        tagsEd.setTextToShowWhenEmpty ("your tags: festival, intro, hard...", c.textDim);
        notesEd.setMultiLine (true); notesEd.setReturnKeyStartsNewLine (true); notesEd.setTextToShowWhenEmpty ("notes...", c.textDim);
        tagsEd.setTooltip ("Your own tags (comma separated) - saved inside the user preset file."); notesEd.setTooltip ("Notes saved inside the user preset file.");
        addAndMakeVisible (tagsEd); addAndMakeVisible (notesEd);

        loadBtn.onClick = [this] { loadRow (list.getSelectedRow()); };
        previewBtn.onClick = [this] { previewRow (list.getSelectedRow()); };
        stopBtn.onClick = [this] { proc.samples().stopPreview(); };
        saveBtn.onClick = [this]
        {
            const String n = proc.currentPresetName();
            const bool user = proc.userPresetFolder().getChildFile (n + ".4gp").existsAsFile();
            if (user) { proc.saveUserPresetAs (n); msg = "saved " + n; refresh(); }
            else saveAsBtn.onClick();
        };
        saveAsBtn.onClick = [this]
        {
            ask ("SAVE AS", "Name of the new user preset:", [this] (const String& n)
            {
                if (n.trim().isEmpty()) return;
                if (proc.userPresetFolder().getChildFile (File::createLegalFileName (n.trim()) + ".4gp").exists()) { msg = "a preset with this name exists - choose another name"; repaint(); return; }
                msg = proc.saveUserPresetAs (n) ? "saved as " + proc.currentPresetName() : "could not save";
                refresh();
            });
        };
        dupBtn.onClick = [this]
        {
            if (auto* e = entry (list.getSelectedRow())) { const auto f = lib.duplicate (*e); msg = f.existsAsFile() ? "duplicated as " + f.getFileNameWithoutExtension() : "could not duplicate"; refresh(); }
        };
        renBtn.onClick = [this]
        {
            auto* e = entry (list.getSelectedRow()); if (e == nullptr) return;
            const auto copy = *e;
            ask ("RENAME", "New name for " + copy.name + ":", [this, copy] (const String& n) { String err; msg = lib.rename (copy, n, &err) ? "renamed to " + n : err; refresh(); });
        };
        delBtn.onClick = [this]
        {
            auto* e = entry (list.getSelectedRow()); if (e == nullptr) return;
            if (e->source != fge::lib::SrcUser) { msg = "factory presets cannot be deleted"; repaint(); return; }
            const auto copy = *e;
            SafePointer<PresetPage> self (this);
            AlertWindow::showOkCancelBox (MessageBoxIconType::WarningIcon, "DELETE", "Move \"" + copy.name + "\" to the trash?", "DELETE", "CANCEL", this,
                                          ModalCallbackFunction::create ([self, copy] (int r) { if (self != nullptr && r == 1) { self->msg = self->lib.remove (copy) ? "deleted " + copy.name : "could not delete"; self->refresh(); } }));
        };
        impBtn.onClick = [this]
        {
            chooser = std::make_unique<FileChooser> ("IMPORT 4GE presets", File::getSpecialLocation (File::userDocumentsDirectory), "*.4gp");
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::canSelectMultipleItems, [this] (const FileChooser& fc)
            {
                StringArray probs; const int n = lib.import (fc.getResults(), &probs);
                msg = String (n) + " preset(s) imported" + (probs.isEmpty() ? String() : "  -  " + probs.joinIntoString ("; ")); refresh();
            });
        };
        expBtn.onClick = [this]
        {
            auto* e = entry (list.getSelectedRow()); if (e == nullptr) return;
            const auto copy = *e;
            chooser = std::make_unique<FileChooser> ("EXPORT preset", File::getSpecialLocation (File::userDocumentsDirectory).getChildFile (File::createLegalFileName (copy.name) + ".4gp"), "*.4gp");
            chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting, [this, copy] (const FileChooser& fc)
            {
                if (fc.getResult() == File()) return;
                msg = lib.exportTo (copy, fc.getResult().withFileExtension ("4gp")) ? "exported to " + fc.getResult().getFullPathName() + (copy.missing.isEmpty() ? String() : "  (missing samples are not inside!)") : "could not export";
                repaint();
            });
        };
        tagsBtn.onClick = [this]
        {
            auto* e = entry (list.getSelectedRow()); if (e == nullptr) return;
            StringArray t; t.addTokens (tagsEd.getText(), ",", {});
            msg = lib.setTags (*e, t, notesEd.getText()) ? "tags saved" : "tags only on user presets (DUPLICATE a factory preset first)";
            refresh();
        };
        closeBtn.onClick = [this] { if (onClose) onClose(); };
        relinkBtn.onClick = [this] { if (onRelink) onRelink(); };
        aBtn.onClick = [this] { proc.abSwitch (0); msg = "A"; repaint(); };
        bBtn.onClick = [this] { proc.abSwitch (1); msg = "B"; repaint(); };
        copyBtn.onClick = [this] { proc.abCopyToOther(); msg = proc.abActive() == 0 ? "A copied to B" : "B copied to A"; repaint(); };
        aBtn.setTooltip ("A / B: two complete sounds - switch to compare (an empty slot starts as a copy of the other).");
        bBtn.setTooltip (aBtn.getTooltip()); copyBtn.setTooltip ("Copy the sound you hear into the other slot.");
        loadBtn.setTooltip ("Load the selected preset (or double-click it)."); previewBtn.setTooltip ("Listen to the selected preset without loading it.");
        saveBtn.setTooltip ("Save the current sound into its user preset (a factory sound asks for a name)."); saveAsBtn.setTooltip ("Save the current sound as a new user preset.");
        dupBtn.setTooltip ("Copy the selected preset as a new user preset (factory presets become editable this way).");
        renBtn.setTooltip ("Rename the selected user preset."); delBtn.setTooltip ("Move the selected user preset to the trash.");
        impBtn.setTooltip ("Copy .4gp files into your presets (never overwrites)."); expBtn.setTooltip ("Save a copy of the selected preset anywhere.");
        relinkBtn.setTooltip ("Missing samples: open the SMP page and use RELINK.");
        for (auto* bt : { &loadBtn, &previewBtn, &stopBtn, &saveBtn, &saveAsBtn, &dupBtn, &renBtn, &delBtn, &impBtn, &expBtn, &tagsBtn, &closeBtn, &aBtn, &bBtn, &copyBtn, &relinkBtn })
            addAndMakeVisible (bt);
        aBtn.setClickingTogglesState (false); bBtn.setClickingTogglesState (false);
        proc.onPresetLoaded = [this] (const String& key) { lib.touchRecent (key); };
        startTimerHz (5);
    }
    PresetPage::~PresetPage() { *alive = false; proc.onPresetLoaded = nullptr; stopTimer(); }

    void PresetPage::refresh()
    {
        lib.scan();
        const String keepTag = tagBox.getText();
        tagBox.clear (dontSendNotification); tagBox.addItem ("ALL TAGS", 1);
        int id = 2; for (auto& t : lib.allTags()) tagBox.addItem (t, id++);
        bool found = false; for (int i = 0; i < tagBox.getNumItems(); ++i) if (tagBox.getItemText (i) == keepTag) { tagBox.setSelectedItemIndex (i, dontSendNotification); found = true; }
        if (! found) tagBox.setSelectedId (1, dontSendNotification);
        apply();
    }
    void PresetPage::setFilter (const String& s, int style, int length, int source, const String& tag)
    {
        search.setText (s, dontSendNotification);
        styleBox.setSelectedId (style + 2, dontSendNotification); lengthBox.setSelectedId (length + 2, dontSendNotification);
        sourceBox.setSelectedId (source + 1, dontSendNotification);
        tagBox.setSelectedId (1, dontSendNotification); for (int i = 0; i < tagBox.getNumItems(); ++i) if (tagBox.getItemText (i) == tag) tagBox.setSelectedItemIndex (i, dontSendNotification);
        apply();
    }
    void PresetPage::apply()
    {
        const String tag = tagBox.getSelectedId() > 1 ? tagBox.getText() : String();
        shown = lib.filter (search.getText(), styleBox.getSelectedId() - 2, lengthBox.getSelectedId() - 2, sourceBox.getSelectedId() - 1, tag,
                            [this] (const fge::lib::Entry& e) { return bar.isFavourite (e.name); });
        list.updateContent(); list.repaint();
        int cur = -1; for (int r = 0; r < (int) shown.size(); ++r) if (lib.all()[(size_t) shown[(size_t) r]].name == proc.currentPresetName()) cur = r;
        if (list.getSelectedRow() < 0 && cur >= 0) list.selectRow (cur);
        showSelected();
    }
    void PresetPage::showSelected()
    {
        if (auto* e = entry (list.getSelectedRow())) { tagsEd.setText (e->userTags.joinIntoString (", "), false); notesEd.setText (e->notes, false); }
        const auto* e = entry (list.getSelectedRow());
        const bool user = e != nullptr && e->source == fge::lib::SrcUser;
        for (auto* bt : { &renBtn, &delBtn, &tagsBtn }) bt->setEnabled (user);
        tagsEd.setEnabled (user); notesEd.setEnabled (user);
        relinkBtn.setVisible (e != nullptr && ! e->missing.isEmpty());
        repaint();
    }
    bool PresetPage::loadRow (int row)
    {
        auto* e = entry (row); if (e == nullptr) return false;
        const auto copy = *e;
        const bool ok = lib.load (copy);
        msg = ok ? "loaded " + copy.name + (copy.missing.isEmpty() ? String() : "  -  MISSING SAMPLES: " + copy.missing.joinIntoString (", ")) : "could not load " + copy.name;
        bar.refresh();
        repaint();
        return ok;
    }
    void PresetPage::previewRow (int row)
    {
        auto* e = entry (row); if (e == nullptr || previewing.load()) return;
        // the snapshot is taken here (message thread) from a private processor - the plugin is not touched
        fge::dsp::xport::Snapshot snap;
        if (e->source == fge::lib::SrcFactory) snap = proc.exportSnapshotOfProgram (e->factory);
        else { auto tmp = std::make_unique<GeProcessor>(); tmp->loadUserPresetFile (e->file); snap = tmp->exportSnapshot(); }
        fge::dsp::xport::Spec spec; spec.note = proc.auditionNote; spec.rate = 44100.0; spec.seed = snap.presetSeed; spec.bpm = proc.hostBpm();
        previewing.store (true);
        msg = "PREVIEW " + e->name + " (not loaded)";
        auto life = alive; GeProcessor* pr = &proc; const double host = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
        SafePointer<PresetPage> self (this);
        Thread::launch ([snap, spec, life, pr, host, self]
        {
            auto r = fge::dsp::xport::render (snap, spec);
            if (! *life) return;
            if (! r.cancelled) pr->samples().previewAudio (r.audio, spec.rate, host);
            MessageManager::callAsync ([self, life] { if (*life && self != nullptr) self->previewing.store (false); });
        });
        repaint();
    }
    void PresetPage::listBoxItemClicked (int row, const MouseEvent& e)
    {
        if (e.x > list.getWidth() - 34)                                  // the star = favorite
            if (auto* en = entry (row)) { bar.toggleFavourite (en->name); list.repaint(); }
    }
    void PresetPage::ask (const String& title, const String& text, std::function<void (const String&)> done)
    {
        auto* w = new AlertWindow (title, text, MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", proc.currentPresetName());
        w->addButton ("OK", 1, KeyPress (KeyPress::returnKey)); w->addButton ("CANCEL", 0, KeyPress (KeyPress::escapeKey));
        w->enterModalState (true, ModalCallbackFunction::create ([w, done] (int r) { if (r == 1) done (w->getTextEditorContents ("name")); }), true);
    }
    void PresetPage::timerCallback() { if (isShowing()) { repaint (infoArea); aBtn.setToggleState (proc.abActive() == 0, dontSendNotification); bBtn.setToggleState (proc.abActive() == 1, dontSendNotification); } }

    void PresetPage::paintListBoxItem (int row, Graphics& g, int w, int h, bool sel)
    {
        auto* e = entry (row); if (e == nullptr) return;
        const auto& c = palette();
        const bool cur = e->name == proc.currentPresetName();
        if (sel) { g.setColour (c.control.brighter (0.25f)); g.fillRect (0, 0, w, h); }
        g.setColour (cur ? c.red : c.silver); g.setFont (FontOptions (13.f, cur ? Font::bold : Font::plain));
        g.drawText (e->name, 8, 0, 300, h, Justification::centredLeft, true);
        g.setFont (FontOptions (11.f)); g.setColour (c.textDim);
        g.drawText (e->source == fge::lib::SrcFactory ? "FACTORY" : "USER", 310, 0, 70, h, Justification::centredLeft);
        g.drawText (e->style >= 0 ? fge::lib::styleNames()[e->style] : String ("-"), 380, 0, 100, h, Justification::centredLeft);
        g.drawText (fge::lib::lengthNames()[e->lengthCat] + " " + String (roundToInt (e->lengthMs)), 480, 0, 120, h, Justification::centredLeft);
        StringArray t (e->tags); for (auto* x : { "EARLY", "MODERN", "RAW", "EXPERIMENTAL", "SHORT", "MEDIUM", "LONG", "EXTRA LONG" }) t.removeString (x);
        g.setColour (e->missing.isEmpty() ? c.textDim : c.hot);
        g.drawText (t.joinIntoString ("  "), 600, 0, w - 640, h, Justification::centredLeft, true);
        const bool fav = bar.isFavourite (e->name);
        g.setColour (fav ? c.part[1] : c.grid); g.setFont (FontOptions (16.f));
        g.drawText (fav ? "*" : "+", w - 30, 0, 24, h, Justification::centred);
    }

    void PresetPage::paint (Graphics& g)
    {
        const auto& c = palette();
        g.setColour (c.panel); g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.f);
        g.setColour (c.textDim); g.setFont (FontOptions (11.f, Font::bold));
        g.drawText ("PRESET BROWSER  -  " + String ((int) shown.size()) + " of " + String ((int) lib.all().size()) + " presets", 12, 6, 500, 14, Justification::left);
        g.drawText ("NOW: " + proc.currentPresetName() + "   (A / B: " + String (proc.abActive() == 0 ? "A" : "B") + ")", getWidth() - 520, 6, 380, 14, Justification::right);
        // the selected preset
        g.setColour (c.ground); g.fillRoundedRectangle (infoArea.toFloat(), 4.f);
        if (auto* e = entry (list.getSelectedRow()))
        {
            auto r = infoArea.reduced (10, 8);
            g.setColour (c.silver); g.setFont (FontOptions (15.f, Font::bold)); g.drawText (e->name, r.removeFromTop (20), Justification::left, true);
            g.setFont (FontOptions (11.f)); g.setColour (c.textDim);
            g.drawText ((e->source == fge::lib::SrcFactory ? "FACTORY" : e->file.getFullPathName()), r.removeFromTop (16), Justification::left, true);
            r.removeFromTop (6);
            g.setColour (c.silver);
            for (auto& line : e->modules) { g.setColour (line.startsWith ("MISSING") ? c.hot : c.silver); g.drawFittedText (line, r.removeFromTop (30), Justification::topLeft, 2); }
            r.removeFromTop (4);
            g.setColour (c.textDim); g.drawFittedText ("TAGS: " + e->tags.joinIntoString (", "), r.removeFromTop (34), Justification::topLeft, 2);
        }
        else { g.setColour (c.textDim); g.setFont (FontOptions (12.f)); g.drawText ("select a preset", infoArea, Justification::centred); }
        g.setColour (previewing.load() ? c.red : c.silver); g.setFont (FontOptions (12.f));
        g.drawText (msg, 12, getHeight() - 22, getWidth() - 24, 16, Justification::left, true);
    }

    void PresetPage::resized()
    {
        const int W = getWidth(), H = getHeight();
        filterArea = { 12, 26, W - 24, 26 };
        search.setBounds (12, 26, 300, 26);
        styleBox.setBounds (320, 26, 140, 26); lengthBox.setBounds (466, 26, 140, 26); sourceBox.setBounds (612, 26, 120, 26); tagBox.setBounds (738, 26, 170, 26);
        aBtn.setBounds (W - 290, 26, 40, 26); bBtn.setBounds (W - 246, 26, 40, 26); copyBtn.setBounds (W - 202, 26, 100, 26); closeBtn.setBounds (W - 96, 26, 84, 26);
        const int listW = W - 24 - 380;
        list.setBounds (12, 60, listW, H - 60 - 64);
        int x = 12; const int by = H - 58;
        for (auto* bt : { &loadBtn, &previewBtn, &stopBtn, &saveBtn, &saveAsBtn, &dupBtn, &renBtn, &delBtn, &impBtn, &expBtn })
        { const int w = bt == &stopBtn ? 56 : 84; bt->setBounds (x, by, w, 28); x += w + 6; }
        infoArea = { listW + 24, 60, 368, H - 60 - 230 };
        relinkBtn.setBounds (infoArea.getX() + 10, infoArea.getBottom() - 34, 100, 24);
        tagsEd.setBounds (listW + 24, infoArea.getBottom() + 10, 368, 26);
        notesEd.setBounds (listW + 24, infoArea.getBottom() + 42, 368, 100);
        tagsBtn.setBounds (listW + 24, infoArea.getBottom() + 150, 120, 28);
    }
}
