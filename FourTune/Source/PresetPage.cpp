// 4TUNE - PRESETS page (GUI only; files through tune::lib::Library and the processor)
#include "PluginEditor.h"

using namespace juce;
using namespace doomsday::flat;
using namespace tuneui;

PresetPage::PresetPage (TuneProcessor& p) : Page (p), lib (p.makeLibrary())
{
    StringArray cats { "ALL", "FAVOURITES" }; cats.addArray (tune::lib::categories());
    for (auto& c : cats)
    {
        auto b = std::make_unique<TextButton> (c);
        b->setClickingTogglesState (true); b->setRadioGroupId (4021);
        b->setColour (TextButton::buttonOnColourId, (c == "FAVOURITES" ? fortune() : groupB()).withAlpha (0.75f));
        b->onClick = [this, c] { category = c; rebuild(); };
        addAndMakeVisible (*b); catButtons.push_back (std::move (b));
    }
    catButtons.front()->setToggleState (true, dontSendNotification);
    search.setTextToShowWhenEmpty ("Search name, category, author...", tokens().textMute);
    search.onTextChange = [this] { rebuild(); };
    list.setRowHeight (30);
    list.setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    for (auto* c : std::initializer_list<Component*> { &search, &list, &load, &fav, &rename, &del, &dup, &saveName, &author, &saveCat, &save }) addAndMakeVisible (*c);
    for (auto* t : { &search, &saveName, &author })
    {
        t->setColour (TextEditor::backgroundColourId, Colour (0x55000000));
        t->setColour (TextEditor::outlineColourId, Colours::white.withAlpha (0.12f));
        t->setColour (TextEditor::textColourId, tokens().text);
    }
    saveName.setTextToShowWhenEmpty ("Preset name", tokens().textMute);
    author.setTextToShowWhenEmpty ("Author", tokens().textMute);
    author.setText (proc.lastAuthor(), false);
    saveCat.addItemList (tune::lib::categories(), 1); saveCat.setSelectedItemIndex (0, dontSendNotification);
    load.setColour (TextButton::buttonColourId, fortune().withAlpha (0.85f)); load.setColour (TextButton::textColourOffId, Colour (0xff0c0a10));
    save.setColour (TextButton::buttonColourId, groupB().withAlpha (0.85f));

    load.onClick = [this] { loadSelected(); };
    fav.onClick = [this] { if (auto* e = selected()) { lib.toggleFavourite (*e); rebuild(); if (onLoaded) onLoaded(); } };
    dup.onClick = [this] { if (auto* e = selected()) { lib.duplicate (*e); rebuild(); } };
    del.onClick = [this]
    {
        auto* e = selected(); if (e == nullptr || e->factory) return;
        const auto victim = *e;
        AlertWindow::showOkCancelBox (MessageBoxIconType::WarningIcon, "Delete preset", "Move \"" + victim.name + "\" to the trash?", "DELETE", "CANCEL", this,
            ModalCallbackFunction::create ([this, victim] (int r) { if (r == 1) { lib.remove (victim); rebuild(); if (onLoaded) onLoaded(); } }));
    };
    rename.onClick = [this]
    {
        auto* e = selected(); if (e == nullptr || e->factory) return;
        const auto victim = *e;
        auto* w = new AlertWindow ("Rename preset", "New name:", MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("n", victim.name); w->addButton ("RENAME", 1, KeyPress (KeyPress::returnKey)); w->addButton ("CANCEL", 0, KeyPress (KeyPress::escapeKey));
        w->enterModalState (true, ModalCallbackFunction::create ([this, w, victim] (int r)
        {
            if (r == 1) { lib.rename (victim, w->getTextEditorContents ("n")); rebuild(); if (onLoaded) onLoaded(); }
        }), true);
    };
    save.onClick = [this]
    {
        const auto n = saveName.getText().trim();
        if (n.isEmpty()) { saveName.grabKeyboardFocus(); return; }
        const auto full = saveCat.getText() == "USER" ? n : saveCat.getText() + " - " + n;
        auto doSave = [this, full] { proc.saveUserPreset (full, author.getText()); rebuild(); if (onLoaded) onLoaded(); };
        if (proc.io.folder().getChildFile (File::createLegalFileName (full) + "." + proc.io.extension).exists())
            AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "Preset exists", "\"" + full + "\" already exists. Overwrite it?", "OVERWRITE", "CANCEL", this,
                                          ModalCallbackFunction::create ([doSave] (int r) { if (r == 1) doSave(); }));
        else doSave();
    };
    for (auto* b : { &load, &fav, &rename, &del, &dup, &save })
        b->setTooltip (b == &load ? "Load (or double-click a preset)" : b == &fav ? "Favourite on / off" : b == &rename ? "Rename a user preset"
                       : b == &del ? "Move a user preset to the trash" : b == &dup ? "Copy a user preset" : "Save the current sound with this name, category and author");
}

void PresetPage::rebuild()
{
    lib.rescan();
    rows = lib.filter (category, search.getText().trim());
    list.updateContent(); list.repaint(); repaint();
}

const tune::lib::Entry* PresetPage::selected() const
{
    const int r = list.getSelectedRow();
    return isPositiveAndBelow (r, (int) rows.size()) ? &lib.entries()[(size_t) rows[(size_t) r]] : nullptr;
}

void PresetPage::loadSelected()
{
    auto* e = selected(); if (e == nullptr) return;
    if (e->factory) proc.setCurrentProgram (e->index); else proc.loadUserPresetFile (e->file);
    if (onLoaded) onLoaded();
    repaint();
}

void PresetPage::paintListBoxItem (int row, Graphics& g, int w, int h, bool sel)
{
    if (! isPositiveAndBelow (row, (int) rows.size())) return;
    auto& lnf = lnfOf (*this);
    const auto& e = lib.entries()[(size_t) rows[(size_t) row]];
    if (sel) { g.setColour (groupB().withAlpha (0.28f)); g.fillRoundedRectangle (Rectangle<float> (2.f, 1.f, (float) w - 4.f, (float) h - 2.f), 6.f); }
    const bool current = e.name == proc.currentPresetName();
    g.setColour (lib.isFavourite (e) ? fortune() : Colours::white.withAlpha (0.15f));
    g.setFont (lnf.font (15.f, Bold)); g.drawText (lib.isFavourite (e) ? "*" : ".", 8, 0, 18, h, Justification::centred);
    g.setColour (current ? fortune() : tokens().text); g.setFont (lnf.font (13.f, current ? Bold : SemiBold));
    g.drawText (tune::lib::displayName (e.name), 32, 0, w - 330, h, Justification::centredLeft);
    g.setColour (tokens().textMute); g.setFont (lnf.font (11.f, Bold, 0.08f));
    g.drawText (e.category, w - 290, 0, 120, h, Justification::centredLeft);
    g.setFont (lnf.font (11.5f, Medium));
    g.drawText (e.factory ? "FACTORY" : (e.author.isNotEmpty() ? e.author : String ("USER")), w - 160, 0, 150, h, Justification::centredRight);
}

void PresetPage::resized()
{
    cards.clear();
    const Rectangle<int> cat (0, 0, 220, getHeight()), mid (232, 0, 760, getHeight()), side (1004, 0, getWidth() - 1004, getHeight());
    cards.push_back ({ "CATEGORIES", groupB(), cat });
    cards.push_back ({ "PRESETS", fortune(), mid });
    cards.push_back ({ "PRESET", groupA(), side.withHeight (300) });
    cards.push_back ({ "SAVE AS", groupB(), side.withTrimmedTop (312).withHeight (300) });
    int y = cat.getY() + 46;
    for (auto& b : catButtons) { b->setBounds (cat.getX() + 14, y, cat.getWidth() - 28, 30); y += 36; }
    search.setBounds (mid.getX() + 16, mid.getY() + 44, mid.getWidth() - 32, 30);
    list.setBounds (mid.getX() + 10, mid.getY() + 84, mid.getWidth() - 20, mid.getHeight() - 94);
    const auto s = side.withHeight (300);
    load.setBounds (s.getX() + 16, s.getY() + 130, s.getWidth() - 32, 40);
    fav.setBounds (s.getX() + 16, s.getY() + 180, (s.getWidth() - 40) / 2, 30);
    dup.setBounds (s.getX() + 24 + (s.getWidth() - 40) / 2, s.getY() + 180, (s.getWidth() - 40) / 2, 30);
    rename.setBounds (s.getX() + 16, s.getY() + 220, (s.getWidth() - 40) / 2, 30);
    del.setBounds (s.getX() + 24 + (s.getWidth() - 40) / 2, s.getY() + 220, (s.getWidth() - 40) / 2, 30);
    const auto v = side.withTrimmedTop (312).withHeight (300);
    saveName.setBounds (v.getX() + 16, v.getY() + 50, v.getWidth() - 32, 30);
    saveCat.setBounds (v.getX() + 16, v.getY() + 92, v.getWidth() - 32, 28);
    author.setBounds (v.getX() + 16, v.getY() + 132, v.getWidth() - 32, 30);
    save.setBounds (v.getX() + 16, v.getY() + 180, v.getWidth() - 32, 40);
}

void PresetPage::paintOverChildren (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    const auto s = cards[2].area;
    if (auto* e = selected())
    {
        g.setColour (tokens().text); g.setFont (lnf.font (18.f, Bold));
        g.drawText (tune::lib::displayName (e->name), s.getX() + 16, s.getY() + 44, s.getWidth() - 32, 26, Justification::centredLeft);
        g.setColour (tokens().textMute); g.setFont (lnf.font (12.f, Medium));
        g.drawText (e->category + "   |   " + (e->factory ? String ("FACTORY") : "by " + (e->author.isNotEmpty() ? e->author : String ("you"))),
                    s.getX() + 16, s.getY() + 74, s.getWidth() - 32, 20, Justification::centredLeft);
        if (e->factory)
            g.drawText ("Factory presets can't be renamed or deleted", s.getX() + 16, s.getY() + 98, s.getWidth() - 32, 20, Justification::centredLeft);
    }
    else
    {
        g.setColour (tokens().textMute); g.setFont (lnf.font (12.f, Medium));
        g.drawText ("Pick a preset (double-click = load)", s.getX() + 16, s.getY() + 60, s.getWidth() - 32, 20, Justification::centredLeft);
    }
    const auto v = cards[3].area;
    g.setColour (tokens().textMute); g.setFont (lnf.font (11.f, Medium));
    g.drawText ("Saved in Documents / 4TUNE / Presets (the melody stays in the project)", v.getX() + 16, v.getBottom() - 34, v.getWidth() - 32, 20, Justification::centredLeft);
    g.drawText (String ((int) rows.size()) + " presets", cards[1].area.getRight() - 140, cards[1].area.getY() + 10, 124, 20, Justification::centredRight);
}
