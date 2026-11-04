#pragma once
// =====================================================================
//  4GE module 02 - the multipoint envelope EDITOR (GUI only).
//  Edits a copy of one env::Shape (2..64 points) and hands the result back on mouse-up (onCommit = one undo step).
//  The curve is drawn with the DSP's own env::Shape::eval, so what you see is exactly what the engine plays.
//  Mouse: double-click empty = add point, double-click point = delete, drag = move (time order kept),
//  shift-click / drag on empty = select several, ALT-drag on a segment = TENSION, right-click = curve type / value / delete,
//  wheel = horizontal zoom, shift + wheel = vertical zoom. SNAP to the grid can be switched off.
// =====================================================================
#include <juce_gui_basics/juce_gui_basics.h>
#include "../DSP/Env/MultiEnv.h"
#include <functional>
#include <set>

namespace fge::gui
{
    class EnvEditor : public juce::Component
    {
    public:
        struct Axis
        {
            float spanMs = 100.f;                                   // t = 1 is this many ms
            float offsetMs = 0.f;                                   // the shape starts this late (delay / offset)
            std::function<juce::String (float v)> label;            // value -> text on the vertical scale ("+12 st", "75 %")
            std::function<float (float v)> toUser;                  // value -> number the user types
            std::function<float (float x)> fromUser;                // and back
            juce::String unit;                                      // "st", "%"
            juce::String timeUnit { " ms" };                         // " ms" (envelopes) / the cycle in degrees
        };

        EnvEditor();
        void setShape (const env::Shape&, float vMin, float vMax);  // ignored while dragging
        const env::Shape& shape() const { return sh; }
        void setAxis (Axis a) { axis = std::move (a); repaint(); }
        void setColour (juce::Colour c) { col = c; repaint(); }
        void setActive (bool on, const juce::String& why) { active = on; inactiveText = why; repaint(); }
        void setSnap (bool on) { snap = on; }
        bool snapOn() const { return snap; }
        void fit();
        bool isDragging() const { return dragging; }
        int selectedCount() const { return (int) sel.size(); }

        std::function<void (const env::Shape&)> onCommit;

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        bool keyPressed (const juce::KeyPress&) override;

        // tests / page: point edits without the mouse
        void addPoint (float t, float v);
        void deletePoint (int i);

    private:
        juce::Rectangle<float> plot() const;
        float xOf (float t) const;
        float yOf (float v) const;
        float tOf (float x) const;
        float vOf (float y) const;
        int hitPoint (juce::Point<float>) const;
        int segmentAt (float t) const;
        int hitHandle (juce::Point<float>, int& which) const;
        float snapT (float t) const;
        float snapV (float v) const;
        void commit();
        void showMenu (int point, float t);
        void enterValue (int point);
        void clampAll();

        env::Shape sh, dragStart;
        float vMin = 0.f, vMax = 1.f;
        float t0 = 0.f, t1 = 1.f, vLo = 0.f, vHi = 1.f;           // the visible window (zoom)
        Axis axis;
        juce::Colour col { 0xff9b70d5 };
        bool active = true, snap = true, dragging = false, banding = false, tensionDrag = false;
        juce::String inactiveText;
        std::set<int> sel;
        int hover = -1, tensionSeg = -1, handleSeg = -1, handleWhich = 0;
        juce::Point<float> downPos;
        juce::Rectangle<float> band;
        float tensionStart = 0.f;
    };
}
