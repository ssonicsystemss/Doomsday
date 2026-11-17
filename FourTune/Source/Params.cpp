#include "Params.h"
#include "DoomsdayParams.h"

namespace tune
{
    using namespace juce;

    String formatValue (Fm f, float v)
    {
        namespace fm = doomsday::fmt;
        switch (f)
        {
            case Fm::Pct:   return fm::pct (v, 0);
            case Fm::Bip:   return fm::bip (v, 0);
            case Fm::Hz:    return v < 20.f ? String ("OFF") : fm::hz (v, 0);
            case Fm::Ms:    return fm::ms (v, 0);
            case Fm::Sec:   return String (v, 2) + " s";
            case Fm::Db:    return fm::db (v, 0);
            case Fm::Semi:  return (v > 0.05f ? "+" : "") + String (v, std::abs (v - std::round (v)) < 0.01f ? 0 : 1) + " st";
            case Fm::Cents: return (v > 0.5f ? "+" : "") + String ((int) std::round (v)) + " ct";
            case Fm::Oct:   return fm::oct (v, 0);
            case Fm::Pan:   return fm::pan (v, 0);
            case Fm::LfoHz: return fm::lfoHz (v, 0);
            case Fm::Int:   return String ((int) std::round (v));
            case Fm::Num:
            default:        return String (v, 2);
        }
    }

    static float parseValue (Fm f, const String& t)
    {
        namespace fm = doomsday::fmt;
        switch (f)
        {
            case Fm::Pct: case Fm::Bip: return fm::pctIn (t);
            case Fm::Hz:  return fm::hzIn (t);
            case Fm::Ms:  return fm::msIn (t);
            case Fm::Pan: return fm::panIn (t);
            default:      return fm::num (t);
        }
    }

    AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        AudioProcessorValueTreeState::ParameterLayout l;
        auto addF = [&l] (const char* id, const char* name, float lo, float hi, float def, float centre, float interval, Fm f)
        {
            NormalisableRange<float> r (lo, hi, interval);
            if (centre > lo && centre < hi) r.setSkewForCentre (centre);
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                       AudioParameterFloatAttributes().withStringFromValueFunction ([f] (float v, int) { return formatValue (f, v); })
                                                      .withValueFromStringFunction ([f] (const String& t) { return parseValue (f, t); })));
        };
        auto addC = [&l] (const char* id, const char* name, const StringArray& items, int def)
        { l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };
        auto addB = [&l] (const char* id, const char* name, bool def)
        { l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
#define TUNE_ADD_F(e, id, name, lo, hi, def, centre, interval, fm) addF (id, name, lo, hi, def, centre, interval, fm);
#define TUNE_ADD_C(e, id, name, items, def) addC (id, name, items, def);
#define TUNE_ADD_B(e, id, name, def) addB (id, name, def);
        TUNE_PARAMS (TUNE_ADD_F, TUNE_ADD_C, TUNE_ADD_B)
#undef TUNE_ADD_F
#undef TUNE_ADD_C
#undef TUNE_ADD_B
        return l;
    }
}
