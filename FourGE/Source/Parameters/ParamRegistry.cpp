#include <map>
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
        const juce::StringArray& stageTypes()     { static const juce::StringArray a { "OFF", "SOFT CLIP", "HARD CLIP", "TUBE", "DIODE", "WAVEFOLD", "BITCRUSH",
                                                                                       "RECTIFIER", "ASYMMETRIC", "FOLDBACK", "DIGITAL CLIP" }; return a; }
        const juce::StringArray& oscWaves()       { static const juce::StringArray a { "SINE", "TRIANGLE", "SAW", "SQUARE", "PULSE", "NOISE", "CUSTOM", "WAVETABLE", "HARMONIC" }; return a; }
        const juce::StringArray& noiseTypes()     { static const juce::StringArray a { "WHITE", "PINK", "BROWN", "METALLIC", "DIGITAL" }; return a; }
        const juce::StringArray& oscTracks()      { static const juce::StringArray a { "PUNCH", "BODY", "TAIL", "SUB", "FIXED Hz" }; return a; }
        const juce::StringArray& phaseModes()     { static const juce::StringArray a { "RESET", "RANDOM", "FREE RUN", "SYNCED" }; return a; }
        const juce::StringArray& oscSelect()      { static const juce::StringArray a { "OSC 1", "OSC 2", "OSC 3", "OSC 4" }; return a; }
        const juce::StringArray& uniModes()       { static const juce::StringArray a { "CENTERED", "SYMMETRIC", "ASYMMETRIC", "RANDOM" }; return a; }
        const juce::StringArray& uniPhases()      { static const juce::StringArray a { "RESET", "RANDOM", "FREE" }; return a; }
        const juce::StringArray& ringSources()    { static const juce::StringArray a { "OFF", "OSC 1", "OSC 2", "OSC 3", "OSC 4", "SINE" }; return a; }
        const juce::StringArray& syncSources()    { static const juce::StringArray a { "OFF", "OSC 1", "OSC 2", "OSC 3", "OSC 4" }; return a; }
        const juce::StringArray& syncModes()      { static const juce::StringArray a { "HARD", "SOFT" }; return a; }
        const juce::StringArray& wtTables()       { static const juce::StringArray a { "BASIC", "HARDSTYLE", "FORMANT", "METAL", "PWM SWEEP", "USER" }; return a; }
        const juce::StringArray& wtModes()        { static const juce::StringArray a { "STATIC", "LINEAR SCAN", "ENVELOPE SCAN", "LFO SCAN", "RANDOM SCAN" }; return a; }
        const juce::StringArray& noiseKinds()     { static const juce::StringArray a { "WHITE", "PINK", "BROWN", "BLUE", "VIOLET", "METALLIC", "DIGITAL", "CUSTOM" }; return a; }
        const juce::StringArray& filterTypes()    { static const juce::StringArray a { "LOW PASS", "HIGH PASS", "BAND PASS", "NOTCH" }; return a; }
        const juce::StringArray& noiseEnvSources(){ static const juce::StringArray a { "OWN", "PUNCH AMP", "BODY AMP", "TAIL AMP", "SUB AMP", "DRAWN" }; return a; }
        const juce::StringArray& randomModes()    { static const juce::StringArray a { "FIXED", "PER TRIGGER", "FREE RUN" }; return a; }
        const juce::StringArray& trSources()      { static const juce::StringArray a { "SINE CLICK", "NOISE CLICK", "IMPULSE", "SHORT PULSE", "METALLIC CLICK", "OSC SNAP", "CUSTOM" }; return a; }
        const juce::StringArray& trSyncs()        { static const juce::StringArray a { "KICK START", "PUNCH START", "BODY START", "TAIL START" }; return a; }
        const juce::StringArray& busOutputs()     { static const juce::StringArray a { "PUNCH BUS", "BODY BUS", "TAIL BUS", "SUB BUS", "TEXTURE BUS", "CUSTOM BUS", "MASTER BUS" }; return a; }
        const juce::StringArray& layerSources()   { static const juce::StringArray a { "SINE", "TRIANGLE", "SAW", "SQUARE", "NOISE A", "NOISE B", "TRANSIENT", "OSC 1", "OSC 2", "OSC 3", "OSC 4" }; return a; }
        const juce::StringArray& layerGroups()    { static const juce::StringArray a { "PUNCH", "BODY", "TAIL", "SUB", "TEXTURE", "CUSTOM" }; return a; }
        const juce::StringArray& layerEnvModes()  { static const juce::StringArray a { "ADSR", "ONE SHOT", "MULTIPOINT" }; return a; }
        const juce::StringArray& routeModes()     { static const juce::StringArray a { "SINGLE (GROUP)", "MULTI (SENDS)" }; return a; }
        const juce::StringArray& layerLinks()     { static const juce::StringArray a { "INDEPENDENT", "LAYER 1", "LAYER 2", "LAYER 3", "LAYER 4", "LAYER 5", "LAYER 6", "LAYER 7", "LAYER 8" }; return a; }
        const juce::StringArray& soloModes()      { static const juce::StringArray a { "SOLO PROCESSED", "SOLO SOURCE" }; return a; }
        const juce::StringArray& distTypes()      { static const juce::StringArray a { "SOFT CLIP", "HARD CLIP", "TUBE", "DIODE", "WAVEFOLD", "BITCRUSH", "RECTIFIER",
                                                                                       "ASYMMETRIC", "FOLDBACK", "DIGITAL CLIP", "CUSTOM" }; return a; }
        const juce::StringArray& distLocations()  { static const juce::StringArray a { "GLOBAL", "PUNCH", "BODY", "TAIL", "SUB", "TEXTURE", "LAYER 1", "LAYER 2", "LAYER 3",
                                                                                       "LAYER 4", "LAYER 5", "LAYER 6", "LAYER 7", "LAYER 8" }; return a; }
        const juce::StringArray& distPaths()      { static const juce::StringArray a { "SERIAL", "PARALLEL A", "PARALLEL B", "PARALLEL C" }; return a; }
        const juce::StringArray& distBands()      { static const juce::StringArray a { "FULL BAND", "LOW", "MID", "HIGH" }; return a; }
        const juce::StringArray& driveCurves()    { static const juce::StringArray a { "LINEAR", "EXPONENTIAL", "LOGARITHMIC" }; return a; }
        const juce::StringArray& ditherModes()    { static const juce::StringArray a { "OFF", "TPDF", "NOISE" }; return a; }
        const juce::StringArray& rectModes()      { static const juce::StringArray a { "HALF WAVE", "FULL WAVE", "POSITIVE", "NEGATIVE", "CUSTOM" }; return a; }
        const juce::StringArray& curveInterps()   { static const juce::StringArray a { "LINEAR", "CUBIC", "BEZIER" }; return a; }
        const juce::StringArray& stageOversampling(){ static const juce::StringArray a { "4x (ENGINE)", "8x", "16x" }; return a; }
        const juce::StringArray& distFilterTypes(){ static const juce::StringArray a { "LOW PASS", "HIGH PASS", "BAND PASS", "NOTCH", "LADDER LP" }; return a; }
        const juce::StringArray& filterSlopes()   { static const juce::StringArray a { "6 dB", "12 dB", "24 dB", "48 dB" }; return a; }
        const juce::StringArray& distModSources() { static const juce::StringArray a { "OWN ENV", "DRAWN ENV", "PITCH ENV", "INPUT LEVEL", "VELOCITY" }; return a; }
        const juce::StringArray& bandSolo()       { static const juce::StringArray a { "ALL BANDS", "LOW ONLY", "MID ONLY", "HIGH ONLY" }; return a; }
        const juce::StringArray& globalDistPos()  { static const juce::StringArray a { "BEFORE MASTER CLIP (SUB CLEAN)", "AFTER MASTER CLIP (WITH SUB)" }; return a; }
        const juce::StringArray& offlineQuality() { static const juce::StringArray a { "SAME AS REALTIME", "HIGHER (EXPORT)" }; return a; }
        const juce::StringArray& m06Locations()   { static const juce::StringArray a { "GLOBAL", "PUNCH", "BODY", "TAIL", "SUB", "TEXTURE", "LAYER 1", "LAYER 2", "LAYER 3",
                                                                                       "LAYER 4", "LAYER 5", "LAYER 6", "LAYER 7", "LAYER 8", "SCREECH" }; return a; }
        const juce::StringArray& eqTypes()        { static const juce::StringArray a { "BELL", "LOW SHELF", "HIGH SHELF", "LOW CUT", "HIGH CUT", "BAND PASS", "NOTCH", "TILT", "RESONANT PEAK" }; return a; }
        const juce::StringArray& eqStereo()       { static const juce::StringArray a { "STEREO", "LEFT", "RIGHT", "MID", "SIDE" }; return a; }
        const juce::StringArray& deqModes()       { static const juce::StringArray a { "DYNAMIC CUT", "DYNAMIC BOOST", "UPWARD", "DOWNWARD" }; return a; }
        const juce::StringArray& deqSources()     { static const juce::StringArray a { "THIS BAND", "FULL SIGNAL", "LOWS (< 200 Hz)", "HIGHS (> 3 kHz)" }; return a; }
        const juce::StringArray& deqDetectors()   { static const juce::StringArray a { "PEAK", "RMS" }; return a; }
        const juce::StringArray& mbCrossovers()   { static const juce::StringArray a { "LINKWITZ-RILEY 12", "LINKWITZ-RILEY 24", "LINKWITZ-RILEY 48", "BUTTERWORTH 12", "BESSEL 12" }; return a; }
        const juce::StringArray& mbListen()       { static const juce::StringArray a { "PROCESSED", "UNPROCESSED (BEFORE)" }; return a; }
        const juce::StringArray& mbDynModes()     { static const juce::StringArray a { "OFF", "COMPRESSOR", "EXPANDER", "GATE", "LIMITER", "TRANSIENT" }; return a; }
        const juce::StringArray& excModes()       { static const juce::StringArray a { "WARM", "BRIGHT", "CRISP", "METALLIC", "AGGRESSIVE", "CUSTOM" }; return a; }
        const juce::StringArray& excHarm()        { static const juce::StringArray a { "EVEN", "ODD", "BOTH" }; return a; }
        const juce::StringArray& toneOrders()     { static const juce::StringArray a { "EQ > DYN EQ > MULTIBAND > SPECTRAL", "DYN EQ > EQ > MULTIBAND > SPECTRAL",
                                                                                       "MULTIBAND > EQ > DYN EQ > SPECTRAL", "SPECTRAL > EQ > DYN EQ > MULTIBAND" }; return a; }
        const juce::StringArray& stwLocations()   { static const juce::StringArray a { "GLOBAL", "PUNCH", "BODY", "TAIL", "SUB", "TEXTURE", "SCREECH" }; return a; }
        const juce::StringArray& m06Positions()   { static const juce::StringArray a { "BEFORE DISTORTION", "AFTER DISTORTION" }; return a; }
        const juce::StringArray& m06FilterTypes() { static const juce::StringArray a { "LOW PASS", "HIGH PASS", "BAND PASS", "NOTCH", "LADDER", "DIODE LADDER",
                                                                                       "STATE VARIABLE", "COMB", "FORMANT", "ALL PASS" }; return a; }
        const juce::StringArray& stereoModes()    { static const juce::StringArray a { "STEREO", "MONO", "MID ONLY", "SIDE ONLY" }; return a; }
        const juce::StringArray& resModes()       { static const juce::StringArray a { "CLEAN", "METALLIC", "SCREECH", "RUBBER", "FORMANT", "CUSTOM" }; return a; }
        const juce::StringArray& resSources()     { static const juce::StringArray a { "SIGNAL (INPUT)", "SINE", "SAW", "SQUARE", "NOISE", "IMPULSE", "SCREECH OSC" }; return a; }
        const juce::StringArray& scrModes()       { static const juce::StringArray a { "TONAL", "METALLIC", "FORMANT", "FM SCREECH", "NOISE SCREECH", "FEEDBACK", "HYBRID", "CUSTOM" }; return a; }
        const juce::StringArray& scrWaves()       { static const juce::StringArray a { "SINE", "SAW", "SQUARE", "TRIANGLE", "PULSE", "OSC TABLE" }; return a; }
        const juce::StringArray& scrOscSel()      { static const juce::StringArray a { "OFF", "OSC 1", "OSC 2", "OSC 3", "OSC 4" }; return a; }
        const juce::StringArray& scrOutputs()     { static const juce::StringArray a { "TAIL", "TEXTURE", "MASTER", "PUNCH", "BODY", "SUB", "INSIDE TAIL (GLUED)" }; return a; }
        const juce::StringArray& fbFilters()      { static const juce::StringArray a { "LOW PASS", "HIGH PASS", "BAND PASS" }; return a; }
        const juce::StringArray& scrPitchModes()  { static const juce::StringArray a { "RELATIVE (NOTE)", "MIDI TRACKED (+ PITCH ENGINE)", "ABSOLUTE (BASE HZ)" }; return a; }
        const juce::StringArray& scrStartModes()  { static const juce::StringArray a { "MANUAL", "BODY END", "BODY END + OFFSET", "TAIL START", "KICK START", "HOST SYNC" }; return a; }
        const juce::StringArray& quantizeModes()  { static const juce::StringArray a { "OFF", "1 ms", "5 ms", "10 ms" }; return a; }
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
            case Unit::Bip:
                if (String (d.name).endsWithIgnoreCase ("pan"))           // a position: L / C / R
                    return std::abs (v) < 0.005f ? String ("C") : (v < 0.f ? "L " : "R ") + String ((int) std::lround (std::abs (v) * 100.f));
                return (v > 0.005f ? "+" : "") + String ((int) std::lround (v * 100.f)) + "%";   // an amount: -100 .. +100 %
            case Unit::Semi:  return (v > 0.05f ? "+" : "") + String (v, 1) + " st";
            case Unit::Cents: return (v > 0.5f ? "+" : "") + String ((int) std::lround (v)) + " ct";
            case Unit::Deg:   return String ((int) std::lround (v)) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0"));
            case Unit::None:  break;
        }
        return (std::abs (v) >= 10.f || std::abs (v - std::round (v)) < 1.0e-3f) ? String ((int) std::lround (v)) : String (v, 2);
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
            case Unit::Bip: return (juce::String (d.name).endsWithIgnoreCase ("pan") && s.containsIgnoreCase ("l") ? -std::abs (n) : n) / 100.f;
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
            case tailPOffset:  return "HOLD: the pitch stays at START this long, then drops - the tonal 'tok' of a hardstyle kick (short TIME = a sharp jump).";
            case tailHoldLvl:  return "Level of the tail while the pitch HOLDs (the tok). Lower = the drop into the bass hits harder.";
            case tailChar:     return "CLEAN / TONAL / DIRTY / SCREAM / METALLIC / BUZZ / CRUSHED - a different saturation algorithm each.";
            case tailPw:       return "Pulse width of the PULSE source.";
            case punchGlide:   return "How fast the punch falls from its own PITCH into the kick's pitch.";
            case bodyGlide:    return "How fast the body falls from its own PITCH into the kick's pitch.";
            case subPitch:     return "Sub frequency when PITCH TRACKING is off (with tracking the sub sits on the note).";
            case tailUniVoices:return "Unison: extra detuned copies of the tail on the left / right that drift slowly - the distortion turns them into a dense, moving tail.";
            case tailUniDetune:return "How far the unison copies are detuned.";
            case tailUniDrift: return "How much the unison copies slowly drift (movement).";
            case tailUniMix:   return "Level of the unison copies.";
            case tailWidth:    return "Stereo width of the tail's mids / highs (the lows stay mono).";
            case tailWidthFrom:return "Below this frequency the tail stays mono.";
            case tailSt2Type:  return "A second distortion stage after the CHARACTER (10 different curves).";
            case tailSt2Drive: return "Drive into stage 2.";
            case tailSt2Bias:  return "Shifts stage 2's curve: even harmonics.";
            case tailSt2Mix:   return "Dry / wet of stage 2.";
            case tailMidHz:    return "Frequency of the bell between the CHARACTER and stage 2.";
            case tailMidGain:  return "Boost / cut of the mid bell - shapes what the stage 2 distorts.";
            case tailMidQ:     return "Width of the mid bell.";
            case tailLowCut:   return "Removes the tail's low end so the clean SUB carries it (20 Hz = off).";
            case tailHighCut:  return "Removes the tail's top (fizz).";
            case clipDrive:    return "Master clipper: louder and denser. The SUB goes around it and stays clean.";
            case clipSoft:     return "Softness of the master clipper's knee.";
            default: break;
        }
        const auto& d = def (id);
        juce::String n (d.name);
        // module 05: the distortion stages (one text per setting, the same for all 16 stages)
        if (const juce::String pid (d.id); pid.startsWith ("dist") && pid.containsChar ('_'))
        {
            static const std::map<juce::String, const char*> t {
                { "on", "Switch this distortion stage on." },
                { "location", "Where the stage works: GLOBAL = the whole kick, PUNCH / BODY / TAIL / SUB = that part (group), TEXTURE = the texture bus, LAYER n = one layer alone." },
                { "order", "Position in its chain (lower = earlier). The order changes the sound." },
                { "path", "SERIAL = one after another. PARALLEL A / B / C = branches fed by the same signal and summed (level compensated)." },
                { "band", "FULL BAND, or only the LOW / MID / HIGH band of a multiband block (the bands sum flat when nothing distorts)." },
                { "type", "The distortion algorithm (each one its own curve)." },
                { "morph_type", "The second algorithm MORPH blends to (both run, the outputs crossfade)." },
                { "morph", "Blend from TYPE to MORPH TO." },
                { "drive", "How hard the signal hits the curve." },
                { "drive_curve", "How DRIVE grows: LINEAR, EXPONENTIAL (slow at first) or LOGARITHMIC (fast at first)." },
                { "input", "Level into the stage (before the curve)." },
                { "output", "Level out of the stage." },
                { "mix", "Dry / distorted blend (the dry path is delay-matched - no comb filter)." },
                { "auto_gain", "Keeps the loudness about the same as the input (slow RMS, never per sample)." },
                { "bias", "DC offset before the curve: more even harmonics, an asymmetric sound (the offset itself is removed)." },
                { "symmetry", "Drives the positive and the negative half differently." },
                { "threshold", "Where the curve starts to bend / clip / fold." },
                { "shape", "The type's own amount: KNEE (soft), WARMTH (tube), ASYMMETRY (diode), FOLD AMOUNT, QUANTIZATION (bitcrush), RECTIFY AMOUNT, STEEPNESS (foldback), HARDNESS (digital)." },
                { "ceiling", "The clip level (HARD / DIGITAL clip)." },
                { "even", "Even harmonics (TUBE)." },
                { "odd", "Odd harmonics (TUBE)." },
                { "fold_stages", "How many times the wave may fold (WAVEFOLD)." },
                { "neg_drive", "Drive of the negative half (ASYMMETRIC)." },
                { "neg_threshold", "Threshold of the negative half (ASYMMETRIC)." },
                { "bits", "Bit depth (BITCRUSH)." },
                { "sr_reduce", "Sample rate reduction - the value is held for longer (BITCRUSH)." },
                { "dither", "Noise added before the quantization (BITCRUSH)." },
                { "rect_mode", "Which half is flipped / kept (RECTIFIER)." },
                { "curve_interp", "How the CUSTOM curve goes between its points." },
                { "oversampling", "Extra oversampling for this stage (on top of the engine's 4x) - less aliasing on hard curves, more CPU." },
                { "pre_on", "A filter before the curve (what gets distorted)." }, { "post_on", "A filter after the curve (what the distortion made)." },
                { "pre_type", "Pre filter type." }, { "post_type", "Post filter type." },
                { "pre_cutoff", "Pre filter cutoff." }, { "post_cutoff", "Post filter cutoff." },
                { "pre_resonance", "Pre filter resonance." }, { "post_resonance", "Post filter resonance." },
                { "pre_drive", "Saturation inside the pre filter." }, { "post_drive", "Saturation inside the post filter." },
                { "pre_slope", "Pre filter slope." }, { "post_slope", "Post filter slope." },
                { "post_mix", "Filtered / unfiltered blend after the curve." },
                { "mod_source", "What moves the stage in time: its own envelope, a drawn envelope, the pitch envelope, the input level or the velocity." },
                { "mod_attack", "Rise time of the stage's own modulation envelope." }, { "mod_decay", "Fall time of the stage's own modulation envelope." },
                { "mod_drive", "Modulation -> DRIVE." }, { "mod_bias", "Modulation -> BIAS." }, { "mod_threshold", "Modulation -> THRESHOLD." },
                { "mod_fold", "Modulation -> SHAPE / FOLD (the curve really changes)." }, { "mod_mix", "Modulation -> MIX." },
                { "mod_output", "Modulation -> OUTPUT (+- 12 dB)." }, { "mod_cutoff", "Modulation -> POST filter cutoff (+- 5 octaves)." },
                { "xover_low", "Multiband: the LOW / MID crossover." }, { "xover_high", "Multiband: the MID / HIGH crossover." },
                { "band_solo", "Listen to one band of the multiband distortion." },
                { "global_position", "GLOBAL stages before the master clip (the sub stays clean) or after it (with the sub)." },
                { "offline_quality", "Export / offline render: the same as realtime, or one oversampling step higher." } };
            if (auto it = t.find (pid.fromFirstOccurrenceOf ("_", false, false)); it != t.end()) return it->second;
        }
        // module 07: EQ / dynamic EQ / multiband / spectral / stereo (one text per setting)
        {
            const juce::String pid (d.id);
            auto num = [&] (const char* pre) { const juce::String p (pre); return pid.startsWith (p) && pid.length() > p.length() && juce::CharacterFunctions::isDigit (pid[p.length()]); };
            static const std::map<juce::String, const char*> eq {
                { "on", "Switch this EQ band on." }, { "location", "Where the band works: the whole kick, a part (group), the texture, one layer or the screech." },
                { "position", "Before or after that place's distortion (module 05)." }, { "type", "Bell, shelves, cuts (6-48 dB), band pass, notch, tilt or a resonant peak - each its own response." },
                { "freq", "Frequency of the band." }, { "gain", "Boost / cut (TILT: how much brighter the top and darker the bottom)." },
                { "q", "Width of the band (cuts: the resonance at the cutoff)." }, { "slope", "Steepness of the cut / band pass / notch." },
                { "mix", "Dry / processed blend." }, { "stereo", "Both channels, only left / right, only the mid or only the side." } };
            static const std::map<juce::String, const char*> deq {
                { "on", "Switch this dynamic band on: a bell whose gain follows the real level." }, { "location", "Where the band works." }, { "position", "Before or after that place's distortion." },
                { "mode", "CUT / BOOST when the level goes ABOVE the threshold, UPWARD (lift) / DOWNWARD (cut) when it falls BELOW it." },
                { "detector_source", "What the detector listens to: this band, the full signal, the lows or the highs." }, { "detector", "PEAK follows every peak, RMS the average over the window." },
                { "window", "RMS averaging time." }, { "freq", "Frequency of the band (and of its detector)." }, { "q", "Width of the band." }, { "threshold", "Level where the band starts to act." },
                { "range", "The most the band may cut / boost." }, { "ratio", "How strongly it acts past the threshold." }, { "knee", "Soft start around the threshold." },
                { "attack", "How fast it reacts." }, { "release", "How fast it lets go." }, { "mix", "Dry / processed blend." } };
            static const std::map<juce::String, const char*> mb {
                { "gain", "Level of this band." }, { "width", "Stereo width of this band (0 = mono)." }, { "invert", "Flip this band's polarity." }, { "mute", "Silence this band." },
                { "solo", "Hear only the soloed bands." }, { "dynamics", "The band's own dynamics: compressor, expander, gate, limiter or transient control (real envelope)." },
                { "threshold", "Where the dynamics start." }, { "ratio", "How strongly they act." }, { "attack", "Reaction time (TRANSIENT: how long the attack counts)." },
                { "release", "Recovery time." }, { "knee", "Soft start around the threshold." }, { "makeup", "Gain after the dynamics." }, { "mix", "Parallel blend of the dynamics." },
                { "tr_attack", "TRANSIENT: lift / lower the attack of this band." }, { "tr_sustain", "TRANSIENT: lift / lower the sustain of this band." } };
            static const std::map<juce::String, const char*> other {
                { "mb_on", "Switch the 4-band multiband engine on." }, { "mb_location", "Where the multiband works." }, { "mb_position", "Before or after that place's distortion." },
                { "mb_crossover", "The crossover filters (Linkwitz-Riley bands sum flat; Butterworth / Bessel do not)." },
                { "mb_xover1", "SUB / LOW split." }, { "mb_xover2", "LOW / MID split." }, { "mb_xover3", "MID / HIGH split." }, { "mb_listen", "PROCESSED or the bands as they come in (for solo listening before the processing)." },
                { "sp_on", "Switch the spectral shaper on." }, { "sp_location", "Where it works." }, { "sp_position", "Before or after that place's distortion." },
                { "sp_tilt", "Darker (-) / brighter (+) around the pivot." }, { "sp_tilt_pivot", "The frequency the tilt turns around." }, { "sp_tilt_intensity", "How strong the tilt is." }, { "sp_tilt_mix", "Tilt dry / wet." },
                { "sp_harm_on", "New even / odd harmonics in a low and a high band." }, { "sp_harm_even", "Even harmonics (warm)." }, { "sp_harm_odd", "Odd harmonics (hard)." },
                { "sp_harm_low", "Harmonics made from the low band." }, { "sp_harm_high", "Harmonics made from the high band." }, { "sp_harm_split", "Low / high band split." },
                { "sp_harm_drive", "How hard the harmonics are generated." }, { "sp_harm_mix", "How much of the new harmonics is added." }, { "sp_harm_balance", "Darker / brighter new harmonics." },
                { "sp_exc_on", "Adds new harmonics only inside its range (not an EQ boost)." }, { "sp_exc_mode", "The curve: warm, bright, crisp, metallic, aggressive, custom." },
                { "sp_exc_harmonics", "Even, odd or both kinds of new harmonics." }, { "sp_exc_low", "Bottom of the range." }, { "sp_exc_high", "Top of the range." },
                { "sp_exc_amount", "How much new content." }, { "sp_exc_color", "Darker / brighter new content." }, { "sp_exc_drive", "How hard the range is driven." }, { "sp_exc_mix", "Dry / wet." },
                { "sp_comp_on", "24 dynamic bands: only frequencies that stick out are pulled down." }, { "sp_comp_sensitivity", "How strongly a peak is pulled down." },
                { "sp_comp_threshold", "How far above the average a band may go before it is pulled down." }, { "sp_comp_reduction", "The most a band is pulled down." },
                { "sp_comp_attack", "Reaction time." }, { "sp_comp_release", "Recovery time." }, { "sp_comp_low", "Bottom of the range." }, { "sp_comp_high", "Top of the range." },
                { "sp_comp_smoothing", "Wider (smoother) or narrower bands." }, { "sp_comp_mix", "Dry / wet." },
                { "sp_freeze", "Captures the spectrum at CAPTURE AT and keeps that texture going." }, { "sp_freeze_capture", "When (after the hit) the spectrum is captured." },
                { "sp_freeze_decay", "How long the frozen texture lasts." }, { "sp_freeze_smoothing", "Smooths the captured spectrum." }, { "sp_freeze_pitch", "Shifts the frozen texture." },
                { "sp_freeze_mix", "Original / frozen blend after the capture." }, { "tone_order", "The order of EQ, dynamic EQ, multiband and spectral at one place." },
                { "stw_on", "Switch the stereo engine on." }, { "stw_location", "Where it works." }, { "stw_width", "Overall width (0 = mono, 100 % = as is, 200 % = wider)." },
                { "stw_low_width", "Width of the lows." }, { "stw_mid_width", "Width of the mids." }, { "stw_high_width", "Width of the highs." },
                { "stw_xover_low", "Low / mid split." }, { "stw_xover_high", "Mid / high split." }, { "stw_mono_below", "Everything below is mono - WIDTH never widens the sub." },
                { "stw_mix", "Dry / processed blend." }, { "stw_low_mid_gain", "Mid level of the lows." }, { "stw_low_side_gain", "Side level of the lows." },
                { "stw_mid_mid_gain", "Mid level of the mids." }, { "stw_mid_side_gain", "Side level of the mids." }, { "stw_high_mid_gain", "Mid level of the highs." },
                { "stw_high_side_gain", "Side level of the highs." }, { "stw_mono_listen", "Hear the kick in mono (check what survives)." } };
            const juce::String k = pid.fromFirstOccurrenceOf ("_", false, false);
            if (num ("eq")) if (auto it = eq.find (k); it != eq.end()) return it->second;
            if (num ("deq")) if (auto it = deq.find (k); it != deq.end()) return it->second;
            if (num ("mb")) if (auto it = mb.find (k); it != mb.end()) return it->second;
            if (auto it = other.find (pid); it != other.end()) return it->second;
        }
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
