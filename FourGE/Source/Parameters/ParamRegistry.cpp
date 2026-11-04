#include "ParamRegistry.h"
#include <cmath>

namespace fge
{
    namespace Choices
    {
        const juce::StringArray& punchSources()   { static const juce::StringArray a { "SINE", "TRIANGLE", "SAW", "SQUARE", "NOISE", "CLICK" }; return a; }
        const juce::StringArray& bodySources()    { static const juce::StringArray a { "SINE", "TRIANGLE", "SAW", "SQUARE", "NOISE" }; return a; }
        const juce::StringArray& tailSources()    { static const juce::StringArray a { "SINE", "TRIANGLE", "SAW", "SQUARE", "PULSE", "NOISE" }; return a; }
        const juce::StringArray& subSources()     { static const juce::StringArray a { "SINE", "TRIANGLE" }; return a; }
        const juce::StringArray& tailCharacters() { static const juce::StringArray a { "CLEAN", "TONAL", "DIRTY", "SCREAM", "METALLIC", "BUZZ", "CRUSHED" }; return a; }
        const juce::StringArray& pitchModes()     { static const juce::StringArray a { "CLASSIC DROP", "FAST DROP", "SLOW DROP", "DOUBLE DROP", "RISE", "STABLE TONE",
                                                                                       "DROP + RISE", "HARDSTYLE KICK", "CUSTOM" }; return a; }
        const juce::StringArray& voiceModes()     { static const juce::StringArray a { "CHOKE", "POLY" }; return a; }
        const juce::StringArray& envModes()       { static const juce::StringArray a { "ADSR", "DRAWN" }; return a; }
        const juce::StringArray& polarities()     { static const juce::StringArray a { "UNIPOLAR", "BIPOLAR" }; return a; }
        const juce::StringArray& triggers()       { static const juce::StringArray a { "ONE SHOT", "RETRIGGER", "LEGATO" }; return a; }
        const juce::StringArray& linkModes()      { static const juce::StringArray a { "INDEPENDENT", "FOLLOW", "LINKED" }; return a; }
        const juce::StringArray& parts()          { static const juce::StringArray a { "PUNCH", "BODY", "TAIL", "SUB" }; return a; }
        const juce::StringArray& none()           { static const juce::StringArray a; return a; }
    }

    const std::array<ParamDef, P::count>& registry()
    {
        using namespace Choices;
#define FGE_DEF_F(e, id, name, unit, lo, hi, d, c, fl) ParamDef { id, name, Kind::Float, unit, lo, hi, d, c, &Choices::none, (uint32_t) (fl) },
#define FGE_DEF_B(e, id, name, d, fl)                   ParamDef { id, name, Kind::Bool, Unit::None, 0.f, 1.f, (d) ? 1.f : 0.f, 0.f, &Choices::none, (uint32_t) (fl) },
#define FGE_DEF_C(e, id, name, list, d, fl)             ParamDef { id, name, Kind::Choice, Unit::None, 0.f, 0.f, (float) (d), 0.f, &Choices::list, (uint32_t) (fl) },
        static const std::array<ParamDef, P::count> table { { FGE_PARAMS (FGE_DEF_F, FGE_DEF_B, FGE_DEF_C) } };
#undef FGE_DEF_F
#undef FGE_DEF_B
#undef FGE_DEF_C
        return table;
    }

    juce::String formatValue (const ParamDef& d, float v)
    {
        using juce::String;
        switch (d.kind)
        {
            case Kind::Bool:   return v > 0.5f ? "ON" : "OFF";
            case Kind::Choice: return d.choices()[juce::jlimit (0, d.choices().size() - 1, (int) std::lround (v))];
            case Kind::Float:  break;
        }
        switch (d.unit)
        {
            case Unit::Hz:    return v >= 1000.f ? String (v / 1000.f, 2) + " kHz" : (v < 100.f ? String (v, 1) : String ((int) std::lround (v))) + " Hz";
            case Unit::Ms:    return v >= 1000.f ? String (v / 1000.f, 2) + " s" : (v < 10.f ? String (v, 1) : String ((int) std::lround (v))) + " ms";
            case Unit::Db:    return v <= d.min + 0.05f && d.min <= -59.f ? String ("-inf") : (v > 0.05f ? "+" : "") + String (v, 1) + " dB";
            case Unit::Pct:   return String ((int) std::lround (v * 100.f)) + "%";
            case Unit::Bip:   return std::abs (v) < 0.005f ? String ("C") : (v < 0.f ? "L " : "R ") + String ((int) std::lround (std::abs (v) * 100.f));
            case Unit::Semi:  return (v > 0.05f ? "+" : "") + String (v, 1) + " st";
            case Unit::Cents: return (v > 0.5f ? "+" : "") + String ((int) std::lround (v)) + " ct";
            case Unit::Deg:   return String ((int) std::lround (v)) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0"));
            case Unit::None:  break;
        }
        return std::abs (v) >= 10.f ? String ((int) std::lround (v)) : String (v, 2);
    }

    float parseValue (const ParamDef& d, const juce::String& t)
    {
        const auto s = t.trim();
        if (d.kind == Kind::Bool) return s.equalsIgnoreCase ("on") || s.getFloatValue() > 0.5f ? 1.f : 0.f;
        if (d.kind == Kind::Choice)
        {
            const int i = d.choices().indexOf (s, true);
            return (float) (i >= 0 ? i : s.getIntValue());
        }
        const float n = s.retainCharacters ("0123456789.-+").getFloatValue();
        switch (d.unit)
        {
            case Unit::Hz:  return s.containsIgnoreCase ("k") ? n * 1000.f : n;
            case Unit::Ms:  return (s.endsWithIgnoreCase ("s") && ! s.containsIgnoreCase ("ms")) ? n * 1000.f : n;
            case Unit::Db:  return s.containsIgnoreCase ("inf") ? d.min : n;
            case Unit::Pct: return n / 100.f;
            case Unit::Bip: return (s.containsIgnoreCase ("l") ? -std::abs (n) : n) / 100.f;
            default:        return n;
        }
    }

    juce::String tooltip (int id)
    {
        using namespace P;
        switch (id)
        {
            case coreLength:   return "Target length of the whole kick. With AUTO LENGTH the body, tail and sub follow it (the punch stays short).";
            case coreMaster:   return "Level of the whole kick at the very end of the chain.";
            case coreAutoLen:  return "ON: BODY / TAIL / SUB lengths and decays scale with LENGTH. OFF: every part keeps its own length (LENGTH only caps the kick).";
            case corePhaseRst: return "ON: every kick starts the oscillators at their PHASE (each hit identical). OFF: they run on from the previous kick.";
            case coreKeyTrack: return "ON: the kick lands on the played MIDI note (END PITCH follows the note). OFF: fixed START / END pitch in Hz.";
            case coreVoice:    return "CHOKE: a new kick fades the previous one out (clean low end). POLY: kicks overlap.";
            case coreVelocity: return "How much the MIDI velocity changes the kick level.";
            case peMode:       return "Shape of the pitch movement from START to END.";
            case peStart:      return "Where the pitch starts (= the TAIL pitch at the hit).";
            case peEnd:        return "Where the pitch lands. With PITCH TRACKING on, the played note sets it.";
            case peTime:       return "How long the pitch takes to land.";
            case peCurve:      return "Shape of the drop: low = falls early and fast, high = stays up longer.";
            case peAmount:     return "Extra pitch at the hit in semitones, fading with the drop.";
            case peFine:       return "Fine tune of the whole kick.";
            case tailChar:     return "CLEAN / TONAL / DIRTY / SCREAM / METALLIC / BUZZ / CRUSHED - a different saturation algorithm each.";
            case tailPw:       return "Pulse width of the PULSE source.";
            case punchGlide:   return "How fast the punch falls from its own PITCH into the kick's pitch.";
            case bodyGlide:    return "How fast the body falls from its own PITCH into the kick's pitch.";
            case subPitch:     return "Sub frequency when PITCH TRACKING is off (with tracking the sub sits on the note).";
            default: break;
        }
        const auto& d = def (id);
        juce::String n (d.name);
        if (n.endsWith ("Mute"))   return "Silence this part.";
        if (n.endsWith ("Solo"))   return "Hear only the soloed parts.";
        if (n.endsWith ("Invert")) return "Flip the polarity of this part in the mixer.";
        if (n.endsWith ("Pan"))    return "Position of this part left / right.";
        if (n.contains ("Mixer"))  return "Fader of this part in the core mixer.";
        if (n.endsWith ("Source")) return "Waveform of this part.";
        if (n.endsWith ("Level"))  return "Output level of this part's generator.";
        if (n.endsWith ("Pitch"))  return "Start pitch of this part - it glides into the kick's pitch.";
        if (n.endsWith ("Length")) return "Total time of this part (its envelope ends here).";
        if (n.endsWith ("Attack")) return "Fade-in at the start of this part.";
        if (n.endsWith ("Decay"))  return "Time to fall from the peak to the sustain level.";
        if (n.endsWith ("Sustain"))return "Level held after the decay, until the release.";
        if (n.endsWith ("Release"))return "Fade-out at the end of this part's length.";
        if (n.endsWith ("Tone"))   return "Low-pass filter: how bright this part is.";
        if (n.endsWith ("Resonance")) return "Peak at the tone filter's cutoff.";
        if (n.endsWith ("Drive"))  return "Saturation of this part (more harmonics, not just louder).";
        if (n.endsWith ("Mix"))    return "Dry / processed (tone + drive) blend.";
        if (n.endsWith ("Phase"))  return "Start phase of the oscillator (with PHASE RESET).";
        return {};
    }
}
