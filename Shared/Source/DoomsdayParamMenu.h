#pragma once
// =====================================================================
//  DOOMSDAY shared: right-click menu of a control + FREEZE + animated RANDOM (namespace doomsday::parammenu)
//  Taken from Damian's dkd / FISSION (ssonicsystemss/dkd, 2026-10-22: src/FISSION_shared/Fission.cpp, Session),
//  rewritten for our Shared/.
//
//   show (comp, param, state, onStatus)
//       the host's own menu items for the parameter (VST3 IContextMenu: Cubase, Reaper, Bitwig... "automation lane",
//       "link to controller") + our items. FL does not offer an automation item to plugins, so when the host gave
//       none we add "Create automation clip": it marks the parameter as FL's LAST TWEAKED control and tells the user
//       FL's own command (Tools > Last tweaked > Create automation clip).
//       + FREEZE (RANDOM leaves it alone) + Reset to default.
//   isFrozen / toggleFrozen      the frozen list lives in the plugin state ("randomLocks" = ids separated by spaces),
//                                so it is saved with the project and the presets
//   touchForHost (param)         an empty gesture: the host now sees this parameter as "last tweaked"
//   RandomMover                  RANDOM / UNDO as a 0.5 s glide of every knob inside one host gesture each, so FL
//                                can record the move as automation and the user SEES what RANDOM changed;
//                                keeps 20 undo snapshots
//
//  Message thread only.
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

namespace doomsday::parammenu
{
    inline bool isFrozen (const juce::ValueTree& state, const juce::String& id)
    {
        return juce::StringArray::fromTokens (state.getProperty ("randomLocks").toString(), " ", "").contains (id);
    }

    inline void toggleFrozen (juce::ValueTree state, const juce::String& id)
    {
        auto locks = juce::StringArray::fromTokens (state.getProperty ("randomLocks").toString(), " ", "");
        if (locks.contains (id)) locks.removeString (id); else locks.add (id);
        locks.removeEmptyStrings();
        state.setProperty ("randomLocks", locks.joinIntoString (" "), nullptr);
    }

    inline void touchForHost (juce::RangedAudioParameter& p)
    {
        p.beginChangeGesture();
        p.setValueNotifyingHost (p.getValue());
        p.endChangeGesture();
    }

    // onStatus (bold, rest): optional, for a status line / tooltip text
    inline void show (juce::Component& c, juce::RangedAudioParameter& p, juce::ValueTree state,
                      std::function<void (const juce::String&, const juce::String&)> onStatus = {},
                      std::function<void()> changed = {})
    {
        using namespace juce;
        PopupMenu hostItems;
        auto* editor = dynamic_cast<AudioProcessorEditor*> (&c);
        if (editor == nullptr) editor = c.findParentComponentOfClass<AudioProcessorEditor>();
        if (editor != nullptr)
            if (auto* ctx = editor->getHostContext())
                if (auto menu = ctx->getContextMenuForParameter (&p))
                    hostItems = menu->getEquivalentPopupMenu();
        bool hostAutomation = false;
        for (PopupMenu::MenuItemIterator it (hostItems, true); it.next();)
            hostAutomation = hostAutomation || it.getItem().text.containsIgnoreCase ("automat");

        Component::SafePointer<Component> safe (&c);
        auto say = [onStatus] (const String& b, const String& r) { if (onStatus) onStatus (b, r); };
        PopupMenu m;
        m.addSectionHeader (p.getName (64).toUpperCase());
        if (! hostAutomation)
            m.addItem ("Create automation clip", [safe, &p, say]
            {
                if (safe == nullptr) return;
                touchForHost (p);
                say ("AUTOMATION", p.getName (64) + " is now FL's last tweaked control  -  in FL: Tools > Last tweaked > Create automation clip");
            });
        if (hostItems.getNumItems() > 0)
        {
            m.addSeparator();
            for (PopupMenu::MenuItemIterator it (hostItems); it.next();) m.addItem (it.getItem());
        }
        m.addSeparator();
        const bool frozen = isFrozen (state, p.paramID);
        m.addItem (frozen ? "Unfreeze (RANDOM may change it again)" : "Freeze (RANDOM leaves it alone)", true, frozen,
                   [safe, &p, state, say, changed]
                   {
                       if (safe == nullptr) return;
                       toggleFrozen (state, p.paramID);
                       if (changed) changed();
                       say (p.getName (64).toUpperCase(), isFrozen (state, p.paramID) ? "FROZEN - RANDOM leaves it alone" : "unfrozen - RANDOM may change it again");
                   });
        m.addItem ("Reset to default", [safe, &p]
        {
            if (safe == nullptr) return;
            p.beginChangeGesture(); p.setValueNotifyingHost (p.getDefaultValue()); p.endChangeGesture();
        });
        m.showMenuAsync (PopupMenu::Options().withMousePosition());
    }

    // ------------------------------------------------------------------ RandomMover
    //  mover.glideTo ({ { param, newValue01 }, ... });   // RANDOM computed by the plugin (its own musical ranges)
    //  mover.undo();                                     // back to the sound before
    class RandomMover : private juce::Timer
    {
    public:
        using Targets = std::vector<std::pair<juce::RangedAudioParameter*, float>>;
        double glideMs = 500.0;
        std::function<void()> onDone;

        // snapshot = every parameter the plugin wants restorable by undo (usually all of them)
        void glideTo (const Targets& targets, const std::vector<juce::RangedAudioParameter*>& snapshotOf, bool stagger = true)
        {
            finish();
            Targets snap;
            for (auto* p : snapshotOf) snap.push_back ({ p, p->getValue() });
            undoStack.push_back (std::move (snap));
            if (undoStack.size() > 20) undoStack.erase (undoStack.begin());
            start (targets, stagger);
        }
        bool canUndo() const { return ! undoStack.empty(); }
        void undo()
        {
            if (undoStack.empty()) return;
            finish();
            auto snap = std::move (undoStack.back());
            undoStack.pop_back();
            Targets changed;
            for (auto& [p, v] : snap) if (! juce::approximatelyEqual (p->getValue(), v)) changed.push_back ({ p, v });
            start (changed, false);
        }
        bool moving() const { return ! moves.empty(); }
        // jump to the end now (e.g. before saving a preset, or in tests)
        void finish()
        {
            for (auto& m : moves)
            {
                if (! juce::approximatelyEqual (m.p->getValue(), m.to)) m.p->setValueNotifyingHost (m.to);
                m.p->endChangeGesture();
            }
            const bool had = ! moves.empty();
            moves.clear();
            stopTimer();
            if (had && onDone) onDone();
        }
        ~RandomMover() override { finish(); }

    private:
        struct Move { juce::RangedAudioParameter* p; float from, to; double delay; };
        void start (const Targets& targets, bool stagger)
        {
            int i = 0;
            for (auto& [p, to] : targets)
            {
                p->beginChangeGesture();   // so FL records the whole sweep as automation
                moves.push_back ({ p, p->getValue(), to, stagger ? (double) ((i++ % 24) * 16) : 0.0 });
            }
            startMs = juce::Time::getMillisecondCounterHiRes();
            if (moves.empty()) { if (onDone) onDone(); return; }
            startTimerHz (60);
        }
        void timerCallback() override
        {
            const double now = juce::Time::getMillisecondCounterHiRes();
            bool running = false;
            for (auto& m : moves)
            {
                const double k = juce::jlimit (0.0, 1.0, (now - startMs - m.delay) / glideMs);
                const float e = 1.f - (float) std::pow (1.0 - k, 3.0);        // ease out
                // switches / choices jump once (half way) instead of flicking through every option
                const float v = m.p->isDiscrete() ? (e < 0.5f ? m.from : m.to) : m.from + (m.to - m.from) * e;
                if (! juce::approximatelyEqual (m.p->getValue(), v)) m.p->setValueNotifyingHost (v);
                running = running || k < 1.0;
            }
            if (! running) finish();
        }
        std::vector<Move> moves;
        std::vector<Targets> undoStack;
        double startMs = 0.0;
    };
}
