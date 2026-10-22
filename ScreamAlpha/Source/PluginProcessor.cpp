#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

// ------------------------------------------------------------ parameters (ID = engine field name; never rename)
namespace
{
    String fmtHz (float v, int)  { return v >= 1000.f ? String (v / 1000.f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; }
    String fmtMs (float v, int)  { return v >= 1000.f ? String (v / 1000.f, 2) + " s" : (v < 10.f ? String (v, 1) : String (roundToInt (v))) + " ms"; }
    String fmtDb (float v, int)  { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; }
    String fmtPct (float v, int) { return String (roundToInt (v * 100.f)) + "%"; }
    String fmtSt (float v, int)  { return (v > 0.05f ? "+" : "") + String (v, 1) + " st"; }
    String fmtCt (float v, int)  { return String (roundToInt (v)) + " ct"; }
    String fmtX (float v, int)   { return String (v, 2) + "x"; }
    String fmtOct (float v, int) { return (v > 0.005f ? "+" : "") + String (v, 1) + " oct"; }
    String fmtNum (float v, int) { return String (v, 2); }
    float parse (const String& t) { auto v = t.retainCharacters ("0123456789.-+").getFloatValue(); return t.containsIgnoreCase ("k") ? v * 1000.f : v; }
    float parsePct (const String& t) { return parse (t) / 100.f; }

    struct Def { const char* id; const char* name; float lo, hi, def, centre; String (*fmt) (float, int); bool pct = false; };
    const Def floats[] {
        { "scoopSt", "Scoop", -24.f, 24.f, 7.f, 0.f, fmtSt },
        { "scoopMs", "Scoop Time", 5.f, 2000.f, 70.f, 150.f, fmtMs },
        { "scoopCurve", "Scoop Curve", 0.f, 1.f, 0.5f, 0.f, fmtPct, true },
        { "glideMs", "Glide", 0.f, 1000.f, 25.f, 100.f, fmtMs },
        { "tuneSt", "Tune", -12.f, 12.f, 0.f, 0.f, fmtSt },
        { "coreLevel", "Core Level", 0.f, 1.5f, 0.8f, 0.f, fmtPct, true },
        { "coreShape", "Core Shape", 0.f, 1.f, 0.15f, 0.f, fmtPct, true },
        { "hashLevel", "Hash Level", 0.f, 2.f, 0.5f, 0.f, fmtPct, true },
        { "hashDecayMs", "Hash Decay", 10.f, 5000.f, 400.f, 400.f, fmtMs },
        { "hashSustain", "Hash Sustain", 0.f, 1.f, 1.f, 0.f, fmtPct, true },
        { "detuneCents", "Detune", 0.f, 100.f, 25.f, 0.f, fmtCt },
        { "carrierSaw", "Saw", 0.f, 1.f, 0.3f, 0.f, fmtPct, true },
        { "ratio", "FM Ratio", 0.25f, 8.f, 1.f, 1.5f, fmtX },
        { "index", "FM Amount", 0.f, 12.f, 3.f, 0.f, fmtNum },
        { "chaos", "Chaos", 0.f, 1.f, 0.4f, 0.f, fmtPct, true },
        { "chaosHz", "Chaos Speed", 1.f, 100.f, 12.f, 12.f, fmtHz },
        { "grit", "Grit", 0.f, 0.3f, 0.f, 0.f, fmtPct, true },
        { "hashHp", "Hash Low Cut", 20.f, 5000.f, 700.f, 500.f, fmtHz },
        { "drive1Db", "Drive", 0.f, 48.f, 24.f, 0.f, fmtDb },
        { "combMix", "Comb", 0.f, 1.f, 0.f, 0.f, fmtPct, true },
        { "combFb", "Comb Feedback", -0.95f, 0.95f, 0.6f, 0.f, fmtNum },
        { "combRatio", "Comb Tune", 0.25f, 4.f, 1.f, 1.f, fmtX },
        { "formHz", "Formant", 200.f, 8000.f, 1500.f, 1200.f, fmtHz },
        { "formGainDb", "Formant Gain", -12.f, 24.f, 6.f, 0.f, fmtDb },
        { "formQ", "Formant Q", 0.3f, 12.f, 2.f, 2.f, fmtNum },
        { "drive2Db", "Drive 2", 0.f, 36.f, 6.f, 0.f, fmtDb },
        { "toneHz", "Tone", 500.f, 20000.f, 9000.f, 5000.f, fmtHz },
        { "lowCut", "Low Cut", 20.f, 1000.f, 150.f, 150.f, fmtHz },
        { "attackMs", "Attack", 0.5f, 2000.f, 2.f, 50.f, fmtMs },
        { "decayMs", "Decay", 5.f, 5000.f, 300.f, 400.f, fmtMs },
        { "sustain", "Sustain", 0.f, 1.f, 0.9f, 0.f, fmtPct, true },
        { "releaseMs", "Release", 5.f, 5000.f, 80.f, 300.f, fmtMs },
        { "chorus", "Chorus", 0.f, 1.f, 0.f, 0.f, fmtPct, true },
        { "outDb", "Volume", -36.f, 12.f, -6.f, 0.f, fmtDb },
        // FILTER + cutoff automation, FORTUNE macro (v0.3)
        { "cutoffHz", "Cutoff", 20.f, 20000.f, 20000.f, 1200.f, fmtHz },
        { "reso", "Resonance", 0.f, 1.f, 0.f, 0.f, fmtPct, true },
        { "autoOct", "Auto Depth", -6.f, 6.f, 0.f, 0.f, fmtOct },
        { "fortune", "FORTUNE", 0.f, 1.f, 0.f, 0.f, fmtPct, true },
        // FX (after the voice): OTT -> delay -> reverb
        { "ott", "OTT", 0.f, 1.f, 0.f, 0.f, fmtPct, true },
        { "dlyMix", "Delay Mix", 0.f, 1.f, 0.f, 0.f, fmtPct, true },
        { "dlyFb", "Delay Feedback", 0.f, 0.9f, 0.35f, 0.f, fmtPct, true },
        { "revMix", "Reverb Mix", 0.f, 1.f, 0.f, 0.f, fmtPct, true },
        { "revSize", "Reverb Size", 0.f, 1.f, 0.7f, 0.f, fmtPct, true },
        { "revDamp", "Reverb Damp", 0.f, 1.f, 0.45f, 0.f, fmtPct, true },
    };
    const StringArray curves { "SOFT", "HARD", "TUBE", "FOLD", "CUBIC" };
    const StringArray dlyNames { "1/16", "1/8", "1/8D", "1/4", "1/4D", "1/2" };
    const float dlyBeats[] { 0.25f, 0.5f, 0.75f, 1.f, 1.5f, 2.f };
    const StringArray filtModes { "LP 24", "LP 12", "BAND", "HIGH" };
    const StringArray autoShapes { "OFF", "RISE", "FALL", "UP-DOWN", "WOBBLE", "STEPS" };
    // cutoff automation cycle (owner: max 1 bar, short rhythmic values) - 1/8T = FL's "1/3 beat"
    const StringArray autoDivs { "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 BAR" };
    const float autoBeatsOf[] { 0.25f, 1.f / 3.f, 0.5f, 2.f / 3.f, 1.f, 2.f, 4.f };

    struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; };
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p {
            { "INIT", {} },
    { "HOOVER - Classic Juno", { { "scoopSt", -5.0f }, { "scoopMs", 220.0f }, { "scoopCurve", 0.35f }, { "glideMs", 110.0f }, { "coreLevel", 0.55f }, { "coreShape", 1.0f }, { "coreOct", -1.0f }, { "hashLevel", 0.9f }, { "hashDecayMs", 600.0f }, { "hashSustain", 0.8f }, { "voices", 7.0f }, { "detuneCents", 38.0f }, { "carrierSaw", 1.0f }, { "ratio", 0.5f }, { "index", 0.35f }, { "chaos", 0.3f }, { "chaosHz", 5.0f }, { "grit", 0.0f }, { "hashHp", 90.0f }, { "drive1Db", 9.0f }, { "drive1Curve", 0.0f }, { "combMix", 0.0f }, { "formHz", 900.0f }, { "formGainDb", 3.0f }, { "formQ", 1.0f }, { "drive2Db", 6.0f }, { "drive2Curve", 0.0f }, { "toneHz", 7500.0f }, { "lowCut", 50.0f }, { "attackMs", 3.0f }, { "decayMs", 600.0f }, { "sustain", 0.85f }, { "releaseMs", 120.0f }, { "chorus", 0.85f }, { "outDb", -6.0f }, { "ott", 0.25f }, { "dlyMix", 0.26f }, { "dlyDiv", 2.f }, { "dlyFb", 0.4f }, { "revMix", 0.42f }, { "revSize", 0.8f }, { "revDamp", 0.45f } } },
    { "HOOVER - Rave Dive", { { "scoopSt", 12.0f }, { "scoopMs", 420.0f }, { "scoopCurve", 0.2f }, { "glideMs", 110.0f }, { "coreLevel", 0.55f }, { "coreShape", 1.0f }, { "coreOct", -1.0f }, { "hashLevel", 0.9f }, { "hashDecayMs", 600.0f }, { "hashSustain", 0.8f }, { "voices", 7.0f }, { "detuneCents", 48.0f }, { "carrierSaw", 1.0f }, { "ratio", 0.5f }, { "index", 0.35f }, { "chaos", 0.3f }, { "chaosHz", 5.0f }, { "grit", 0.0f }, { "hashHp", 90.0f }, { "drive1Db", 16.0f }, { "drive1Curve", 0.0f }, { "combMix", 0.0f }, { "formHz", 1300.0f }, { "formGainDb", 7.0f }, { "formQ", 1.5f }, { "drive2Db", 6.0f }, { "drive2Curve", 0.0f }, { "toneHz", 7500.0f }, { "lowCut", 50.0f }, { "attackMs", 3.0f }, { "decayMs", 600.0f }, { "sustain", 0.85f }, { "releaseMs", 120.0f }, { "chorus", 0.7f }, { "outDb", -6.0f }, { "ott", 0.3f }, { "dlyMix", 0.3f }, { "dlyDiv", 3.f }, { "dlyFb", 0.42f }, { "revMix", 0.46f }, { "revSize", 0.82f }, { "revDamp", 0.4f } } },
    { "HOOVER - Hardstyle Scream", { { "scoopSt", 5.0f }, { "scoopMs", 70.0f }, { "scoopCurve", 0.5f }, { "glideMs", 40.0f }, { "coreLevel", 0.55f }, { "coreShape", 0.7f }, { "coreOct", 0.0f }, { "hashLevel", 0.9f }, { "hashDecayMs", 600.0f }, { "hashSustain", 0.8f }, { "voices", 7.0f }, { "detuneCents", 38.0f }, { "carrierSaw", 1.0f }, { "ratio", 2.0f }, { "index", 2.2f }, { "chaos", 0.3f }, { "chaosHz", 5.0f }, { "grit", 0.0f }, { "hashHp", 250.0f }, { "drive1Db", 24.0f }, { "drive1Curve", 1.0f }, { "combMix", 0.35f }, { "formHz", 1200.0f }, { "formGainDb", 10.0f }, { "formQ", 2.0f }, { "drive2Db", 6.0f }, { "drive2Curve", 0.0f }, { "toneHz", 7500.0f }, { "lowCut", 150.0f }, { "attackMs", 3.0f }, { "decayMs", 600.0f }, { "sustain", 0.85f }, { "releaseMs", 120.0f }, { "chorus", 0.5f }, { "outDb", -6.0f }, { "combFb", 0.55f }, { "combRatio", 1.0f }, { "ott", 0.45f }, { "dlyMix", 0.22f }, { "dlyDiv", 2.f }, { "dlyFb", 0.38f }, { "revMix", 0.38f }, { "revSize", 0.75f }, { "revDamp", 0.5f } } },
    { "SCREECH - Short Shot", { { "scoopSt", 4.0f }, { "scoopMs", 300.0f }, { "scoopCurve", 0.3f }, { "coreLevel", 1.0817f }, { "coreShape", 0.27687f }, { "hashLevel", 1.0334f }, { "hashDecayMs", 30.0f }, { "hashSustain", 0.6969f }, { "detuneCents", 7.5758f }, { "carrierSaw", 0.84853f }, { "ratio", 1.0402f }, { "index", 2.2164f }, { "chaos", 0.74066f }, { "chaosHz", 15.772f }, { "grit", 0.0323f }, { "hashHp", 154.97f }, { "drive1Db", 13.178f }, { "combMix", 0.07655f }, { "combFb", 0.36048f }, { "combRatio", 2.0424f }, { "formHz", 1639.3f }, { "formGainDb", 6.7692f }, { "formQ", 0.59283f }, { "drive2Db", 8.9699f }, { "toneHz", 17689.0f }, { "lowCut", 256.16f }, { "outDb", -8.01f }, { "ott", 0.3f }, { "dlyMix", 0.22f }, { "dlyDiv", 2.0f }, { "dlyFb", 0.4f }, { "revMix", 0.4f }, { "revSize", 0.78f }, { "revDamp", 0.45f } } },
    { "SCREECH - Massive Dive", { { "scoopSt", 16.0f }, { "scoopMs", 650.0f }, { "scoopCurve", 0.25f }, { "coreLevel", 0.55242f }, { "coreShape", 0.4186f }, { "hashLevel", 1.7127f }, { "hashDecayMs", 105.95f }, { "hashSustain", 0.92077f }, { "detuneCents", 72.737f }, { "carrierSaw", 0.52383f }, { "ratio", 2.6336f }, { "index", 3.2063f }, { "chaos", 0.92422f }, { "chaosHz", 3.1914f }, { "grit", 0.0412f }, { "hashHp", 180.35f }, { "drive1Db", 27.805f }, { "combMix", 0.12674f }, { "combFb", -0.84735f }, { "combRatio", 0.71743f }, { "formHz", 553.3f }, { "formGainDb", 11.366f }, { "formQ", 0.93042f }, { "drive2Db", 20.628f }, { "toneHz", 17867.0f }, { "lowCut", 400.0f }, { "outDb", -4.39f }, { "ott", 0.3f }, { "dlyMix", 0.22f }, { "dlyDiv", 2.0f }, { "dlyFb", 0.4f }, { "revMix", 0.4f }, { "revSize", 0.78f }, { "revDamp", 0.45f } } },
    { "SCREECH - Loop Hash", { { "scoopSt", 5.0f }, { "scoopMs", 60.0f }, { "scoopCurve", 0.4f }, { "attackMs", 1.0f }, { "releaseMs", 30.0f }, { "coreLevel", 0.08974f }, { "coreShape", 0.04476f }, { "hashLevel", 1.5014f }, { "hashDecayMs", 135.33f }, { "hashSustain", 0.79734f }, { "detuneCents", 16.636f }, { "carrierSaw", 0.71515f }, { "ratio", 2.4633f }, { "index", 4.7985f }, { "chaos", 0.70109f }, { "chaosHz", 2.6255f }, { "grit", 0.0436f }, { "hashHp", 664.44f }, { "drive1Db", 28.794f }, { "combMix", 0.87448f }, { "combFb", -0.04826f }, { "combRatio", 0.5f }, { "formHz", 536.6f }, { "formGainDb", 14.33f }, { "formQ", 1.4988f }, { "drive2Db", 2.756f }, { "toneHz", 16989.0f }, { "lowCut", 51.278f }, { "outDb", -4.62f }, { "ott", 0.3f }, { "dlyMix", 0.22f }, { "dlyDiv", 2.0f }, { "dlyFb", 0.4f }, { "revMix", 0.4f }, { "revSize", 0.78f }, { "revDamp", 0.45f } } },
    { "SCREECH - Bright Talker", { { "scoopSt", -2.0f }, { "scoopMs", 90.0f }, { "scoopCurve", 0.3f }, { "coreLevel", 0.04002f }, { "coreShape", 0.16523f }, { "hashLevel", 1.1673f }, { "hashDecayMs", 217.03f }, { "hashSustain", 0.51286f }, { "detuneCents", 12.953f }, { "carrierSaw", 0.73669f }, { "ratio", 1.1205f }, { "index", 3.9496f }, { "chaos", 0.85323f }, { "chaosHz", 9.404f }, { "grit", 0.0279f }, { "hashHp", 1045.5f }, { "drive1Db", 26.473f }, { "combMix", 0.11527f }, { "combFb", -0.3682f }, { "combRatio", 2.8747f }, { "formHz", 963.01f }, { "formGainDb", 18.863f }, { "formQ", 1.4922f }, { "drive2Db", 1.8085f }, { "toneHz", 14070.0f }, { "lowCut", 83.332f }, { "outDb", -6.69f }, { "ott", 0.3f }, { "dlyMix", 0.22f }, { "dlyDiv", 2.0f }, { "dlyFb", 0.4f }, { "revMix", 0.4f }, { "revSize", 0.78f }, { "revDamp", 0.45f } } },
    { "SCREECH - Classic Wail", { { "scoopSt", 2.0f }, { "scoopMs", 50.0f }, { "scoopCurve", 0.4f }, { "coreLevel", 0.42107f }, { "coreShape", 0.10828f }, { "hashLevel", 1.7054f }, { "hashDecayMs", 64.266f }, { "hashSustain", 0.61442f }, { "detuneCents", 13.383f }, { "carrierSaw", 0.64781f }, { "ratio", 0.65738f }, { "index", 0.13637f }, { "chaos", 0.83402f }, { "chaosHz", 4.0016f }, { "grit", 0.034f }, { "hashHp", 252.52f }, { "drive1Db", 6.4484f }, { "combMix", 0.40452f }, { "combFb", 0.5971f }, { "combRatio", 2.1403f }, { "formHz", 418.78f }, { "formGainDb", 5.8749f }, { "formQ", 3.5568f }, { "drive2Db", 23.709f }, { "toneHz", 17551.0f }, { "lowCut", 135.93f }, { "outDb", -4.08f }, { "ott", 0.3f }, { "dlyMix", 0.22f }, { "dlyDiv", 2.0f }, { "dlyFb", 0.4f }, { "revMix", 0.4f }, { "revSize", 0.78f }, { "revDamp", 0.45f } } },
    { "SCREECH - Digital Rise", { { "scoopSt", 3.0f }, { "scoopMs", 120.0f }, { "scoopCurve", 0.4f }, { "coreLevel", 0.10263f }, { "coreShape", 0.42512f }, { "hashLevel", 1.8588f }, { "hashDecayMs", 49.392f }, { "hashSustain", 0.76292f }, { "detuneCents", 55.596f }, { "carrierSaw", 0.67598f }, { "ratio", 3.6291f }, { "index", 4.0769f }, { "chaos", 0.92048f }, { "chaosHz", 2.4096f }, { "grit", 0.0457f }, { "hashHp", 397.67f }, { "drive1Db", 33.722f }, { "combMix", 0.5254f }, { "combFb", -0.31815f }, { "combRatio", 1.025f }, { "formHz", 863.09f }, { "formGainDb", 14.04f }, { "formQ", 0.77744f }, { "drive2Db", 22.558f }, { "toneHz", 16446.0f }, { "lowCut", 80.236f }, { "outDb", -3.1f }, { "ott", 0.3f }, { "dlyMix", 0.22f }, { "dlyDiv", 2.0f }, { "dlyFb", 0.4f }, { "revMix", 0.4f }, { "revSize", 0.78f }, { "revDamp", 0.45f } } },
    { "SCREECH - Owner 6", { { "attackMs", 4.59282f }, { "autoDiv", 4.0f }, { "autoOct", 6.0f }, { "autoShape", 4.0f }, { "carrierSaw", 0.74013f }, { "chaos", 0.57937f }, { "chaosHz", 10.68747f }, { "chorus", 0.0f }, { "combFb", 0.05527f }, { "combMix", 0.0f }, { "combRatio", 1.0f }, { "coreLevel", 0.59668f }, { "coreOct", 0.0f }, { "coreShape", 0.40122f }, { "cutoffHz", 20000.0f }, { "decayMs", 302.64859f }, { "detuneCents", 45.82917f }, { "dlyDiv", 1.0f }, { "dlyFb", 0.27968f }, { "dlyMix", 0.21476f }, { "drive1Curve", 3.0f }, { "drive1Db", 19.60166f }, { "drive2Curve", 2.0f }, { "drive2Db", 1.72554f }, { "filtMode", 0.0f }, { "formGainDb", 3.94618f }, { "formHz", 1331.17493f }, { "formQ", 4.55479f }, { "fortune", 0.44063f }, { "glideMs", 42.47207f }, { "grit", 0.0f }, { "hashDecayMs", 315.50885f }, { "hashHp", 114.47311f }, { "hashLevel", 1.23466f }, { "hashSustain", 0.96526f }, { "index", 0.13756f }, { "lowCut", 89.72073f }, { "ott", 0.46901f }, { "outDb", -1.13176f }, { "ratio", 3.0f }, { "releaseMs", 58.72669f }, { "reso", 0.0f }, { "revDamp", 0.30301f }, { "revMix", 0.27634f }, { "revSize", 0.58544f }, { "scoopCurve", 0.44061f }, { "scoopMs", 449.61313f }, { "scoopSt", -5.9746f }, { "sustain", 0.77724f }, { "toneHz", 6098.16113f }, { "tuneSt", 0.0f }, { "voices", 7.0f } } },
    { "SCREECH - Owner 7", { { "attackMs", 0.5f }, { "autoDiv", 4.0f }, { "autoOct", 6.0f }, { "autoShape", 4.0f }, { "carrierSaw", 0.89803f }, { "chaos", 0.33332f }, { "chaosHz", 10.05459f }, { "chorus", 0.28014f }, { "combFb", -0.63844f }, { "combMix", 0.0f }, { "combRatio", 1.0f }, { "coreLevel", 0.12919f }, { "coreOct", 0.0f }, { "coreShape", 0.24594f }, { "cutoffHz", 20000.0f }, { "decayMs", 637.70654f }, { "detuneCents", 52.27443f }, { "dlyDiv", 5.0f }, { "dlyFb", 0.36579f }, { "dlyMix", 0.20954f }, { "drive1Curve", 0.0f }, { "drive1Db", 23.04198f }, { "drive2Curve", 2.0f }, { "drive2Db", 11.03983f }, { "filtMode", 0.0f }, { "formGainDb", 1.09591f }, { "formHz", 1368.21545f }, { "formQ", 2.19943f }, { "fortune", 0.8625f }, { "glideMs", 545.9787f }, { "grit", 0.0f }, { "hashDecayMs", 571.54688f }, { "hashHp", 1241.60767f }, { "hashLevel", 0.43816f }, { "hashSustain", 0.63218f }, { "index", 0.49875f }, { "lowCut", 20.0f }, { "ott", 0.35033f }, { "outDb", 2.40181f }, { "ratio", 1.0f }, { "releaseMs", 75.70741f }, { "reso", 0.0f }, { "revDamp", 0.44691f }, { "revMix", 0.35393f }, { "revSize", 0.80465f }, { "scoopCurve", 0.15203f }, { "scoopMs", 278.01672f }, { "scoopSt", 2.99883f }, { "sustain", 0.66806f }, { "toneHz", 14912.3252f }, { "tuneSt", 0.0f }, { "voices", 4.0f } } },
    { "SCREECH - Owner 8", { { "attackMs", 1.05805f }, { "autoDiv", 4.0f }, { "autoOct", 0.0f }, { "autoShape", 0.0f }, { "carrierSaw", 0.52441f }, { "chaos", 0.50878f }, { "chaosHz", 12.33119f }, { "chorus", 0.44114f }, { "combFb", 0.11437f }, { "combMix", 0.28136f }, { "combRatio", 2.0f }, { "coreLevel", 0.85072f }, { "coreOct", 0.0f }, { "coreShape", 0.77418f }, { "cutoffHz", 20000.0f }, { "decayMs", 417.39474f }, { "detuneCents", 13.8796f }, { "dlyDiv", 5.0f }, { "dlyFb", 0.34091f }, { "dlyMix", 0.29333f }, { "drive1Curve", 4.0f }, { "drive1Db", 6.1625f }, { "drive2Curve", 2.0f }, { "drive2Db", 9.75888f }, { "filtMode", 0.0f }, { "formGainDb", 10.13943f }, { "formHz", 1075.46289f }, { "formQ", 1.97028f }, { "fortune", 1.0f }, { "glideMs", 95.80713f }, { "grit", 0.0f }, { "hashDecayMs", 395.45511f }, { "hashHp", 738.13214f }, { "hashLevel", 0.55314f }, { "hashSustain", 0.50664f }, { "index", 0.66957f }, { "lowCut", 125.60382f }, { "ott", 0.00869f }, { "outDb", -6.77846f }, { "ratio", 3.0f }, { "releaseMs", 68.73552f }, { "reso", 0.0f }, { "revDamp", 0.36856f }, { "revMix", 0.35659f }, { "revSize", 0.82867f }, { "scoopCurve", 0.44212f }, { "scoopMs", 269.9816f }, { "scoopSt", 5.04457f }, { "sustain", 0.91591f }, { "toneHz", 5672.23193f }, { "tuneSt", 0.0f }, { "voices", 3.0f } } },
        };
        return p;
    }
}

AudioProcessorValueTreeState::ParameterLayout AlphaProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    for (auto& d : floats)
    {
        NormalisableRange<float> r (d.lo, d.hi);
        if (d.centre > d.lo && d.centre < d.hi) r.setSkewForCentre (d.centre);
        auto attrs = AudioParameterFloatAttributes().withStringFromValueFunction (d.fmt)
                         .withValueFromStringFunction (d.pct ? parsePct : parse);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { d.id, 1 }, d.name, r, d.def, attrs));
    }
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "voices", 1 }, "Voices", 1, 7, 5));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "coreOct", 1 }, "Core Octave", StringArray { "-1 OCT", "0", "+1 OCT" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "drive1Curve", 1 }, "Drive Curve", curves, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "drive2Curve", 1 }, "Drive 2 Curve", curves, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "dlyDiv", 1 }, "Delay Time", dlyNames, 2));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "filtMode", 1 }, "Filter Mode", filtModes, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "autoShape", 1 }, "Auto Shape", autoShapes, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "autoDiv", 1 }, "Auto Rate", autoDivs, 4));
    return l;
}

AlphaProcessor::AlphaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "SCREAMALPHA", createLayout())
{
    setCurrentProgram (1);      // start on the classic hoover
}

alpha::Params AlphaProcessor::gather() const
{
    alpha::Params p;
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    p.scoopSt = v ("scoopSt"); p.scoopMs = v ("scoopMs"); p.scoopCurve = v ("scoopCurve"); p.glideMs = v ("glideMs"); p.tuneSt = v ("tuneSt");
    p.coreLevel = v ("coreLevel"); p.coreShape = v ("coreShape"); p.coreOct = roundToInt (v ("coreOct")) - 1;
    p.hashLevel = v ("hashLevel"); p.hashDecayMs = v ("hashDecayMs"); p.hashSustain = v ("hashSustain");
    p.voices = roundToInt (v ("voices")); p.detuneCents = v ("detuneCents"); p.carrierSaw = v ("carrierSaw");
    p.ratio = v ("ratio"); p.index = v ("index"); p.chaos = v ("chaos"); p.chaosHz = v ("chaosHz"); p.grit = v ("grit");
    p.hashHp = v ("hashHp"); p.drive1Db = v ("drive1Db"); p.drive1Curve = roundToInt (v ("drive1Curve"));
    p.combMix = v ("combMix"); p.combFb = v ("combFb"); p.combRatio = v ("combRatio");
    p.formHz = v ("formHz"); p.formGainDb = v ("formGainDb"); p.formQ = v ("formQ");
    p.drive2Db = v ("drive2Db"); p.drive2Curve = roundToInt (v ("drive2Curve")); p.toneHz = v ("toneHz"); p.lowCut = v ("lowCut");
    p.attackMs = v ("attackMs"); p.decayMs = v ("decayMs"); p.sustain = v ("sustain"); p.releaseMs = v ("releaseMs");
    p.chorus = v ("chorus"); p.outDb = v ("outDb");
    p.cutoffHz = v ("cutoffHz"); p.reso = v ("reso"); p.filtMode = roundToInt (v ("filtMode"));
    p.autoShape = roundToInt (v ("autoShape")); p.autoBeats = autoBeatsOf[jlimit (0, 6, roundToInt (v ("autoDiv")))]; p.autoOct = v ("autoOct");

    // FORTUNE (from Fortune of Seekers): one big knob pushes everything that makes a screech scream, level kept steady
    const float f = jlimit (0.f, 1.f, v ("fortune"));
    if (f > 0.f)
    {
        p.hashLevel *= 1.f + 0.6f * f;
        p.drive1Db += 12.f * f;
        p.detuneCents = jmin (100.f, p.detuneCents + 25.f * f);
        p.chaos = jmin (1.f, p.chaos + 0.3f * f);
        p.index += p.index > 0.2f ? 1.5f * f : 0.f;
        p.formGainDb += 6.f * f;
        p.toneHz = jmin (20000.f, p.toneHz * std::pow (2.f, 0.8f * f));
        p.cutoffHz = jmin (20000.f, p.cutoffHz * std::pow (2.f, 1.5f * f));
        p.scoopSt *= 1.f + 0.5f * f;
        p.chorus = jmin (1.f, p.chorus + 0.3f * f);
        p.outDb -= 4.f * f;
    }
    return p;
}

int AlphaProcessor::getNumPrograms() { return (int) presets().size(); }
const String AlphaProcessor::getProgramName (int i) { return isPositiveAndBelow (i, getNumPrograms()) ? String (presets()[(size_t) i].name) : String(); }

void AlphaProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    currentName = presets()[(size_t) i].name;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm)) rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& [id, val] : presets()[(size_t) i].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (String (id) == "coreOct" ? val + 1.f : val));   // octave stored as -1 / 0 / +1
    undo.clearUndoHistory();
}

bool AlphaProcessor::isBusesLayoutSupported (const BusesLayout& l) const { return l.getMainOutputChannelSet() == AudioChannelSet::stereo(); }

void AlphaProcessor::prepareToPlay (double sr, int block)
{
    engine.setParams (gather());
    engine.prepare (sr, block);
    ott.prepare (sr);
    fx.prepare (sr, block);
}

void AlphaProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals nd;
    buffer.clear();
    double bpmNow = 150.0, ppq = 0.0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpmNow = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
        }
    engine.setParams (gather());
    engine.setTransport (ppq, bpmNow, playing);
    engine.process (buffer, midi);

    // FX: OTT -> delay (tempo synced, ping-pong) -> reverb
    double bpm = 150.0;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto b = pos->getBpm()) bpm = *b;
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    const float ottAmt = v ("ott");
    if (ottAmt > 0.001f) for (int i = 0; i < buffer.getNumSamples(); ++i) ott.process (L[i], R[i], ottAmt);
    doomsday::dsp::FxParams fp;
    fp.dlyMix = v ("dlyMix"); fp.dlyOn = fp.dlyMix > 0.001f; fp.dlyFb = v ("dlyFb"); fp.dlyPP = true;
    fp.dlyBeats = dlyBeats[jlimit (0, 5, roundToInt (v ("dlyDiv")))];
    const float fort = v ("fortune");
    fp.revMix = jmin (1.f, v ("revMix") + 0.15f * fort); fp.revOn = fp.revMix > 0.001f; fp.revSize = v ("revSize"); fp.revDamp = v ("revDamp");
    fx.setup (fp, bpm);
    fx.process (L, R, buffer.getNumSamples(), -1.0, 0);
    int w = scopeWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < buffer.getNumSamples(); ++i) { scope[(size_t) w] = 0.5f * (L[i] + R[i]); w = (w + 1) & (scopeSize - 1); }
    scopeWrite.store (w, std::memory_order_release);
}

// ------------------------------------------------------------ RANDOM + user presets
float AlphaProcessor::measureLoudnessDb()
{
    auto prm = gather();
    constexpr double rate = 48000.0; constexpr int block = 512, total = (int) (rate * 0.7), skip = (int) (rate * 0.05);
    double sum = 0.0; long n = 0;
    for (int note : { 62, 74 })
    {
        auto eng = std::make_unique<alpha::Engine>();
        eng->setParams (prm); eng->prepare (rate, block);
        AudioBuffer<float> b (2, block);
        for (int pos = 0; pos < total; pos += block)
        {
            b.clear(); MidiBuffer m;
            if (pos == 0) m.addEvent (MidiMessage::noteOn (1, note, (uint8) 110), 0);
            eng->process (b, m);
            for (int i = 0; i < block; ++i)
                if (pos + i >= skip) { const float v = 0.5f * (b.getSample (0, i) + b.getSample (1, i)); sum += (double) v * v; ++n; }
        }
    }
    return n > 0 ? (float) (10.0 * std::log10 (sum / (double) n + 1e-12)) : -100.f;
}

void AlphaProcessor::randomize()
{
    auto& r = rng;
    auto set = [this] (const char* id, float v)
    { if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); } };
    auto lin = [&r] (float a, float b) { return a + r.nextFloat() * (b - a); };
    auto lg = [&r] (float a, float b) { return a * std::pow (b / a, r.nextFloat()); };
    auto pick = [&r] (std::initializer_list<float> o) { return *(o.begin() + r.nextInt ((int) o.size())); };
    auto chance = [&r] (float pr) { return r.nextFloat() < pr; };

    const float roll = r.nextFloat();
    if (roll < 0.45f)
    {
        // the owner's taste A (his presets "6" / "7", 2026-10-22): wide detuned saws with chaos, soft / fold drive into a tube,
        // FORTUNE up, no grit, no comb, light formant around 1.3 kHz, OTT + delay + reverb
        set ("scoopSt", chance (0.6f) ? lin (2.f, 5.f) : lin (-7.f, -3.f)); set ("scoopMs", lin (220.f, 480.f)); set ("scoopCurve", lin (0.1f, 0.5f));
        set ("glideMs", chance (0.15f) ? lin (300.f, 600.f) : lg (30.f, 150.f)); set ("tuneSt", 0.f);
        set ("coreLevel", lin (0.1f, 0.7f)); set ("coreShape", lin (0.15f, 0.5f)); set ("coreOct", 1.f);
        set ("hashLevel", lin (0.4f, 1.3f)); set ("hashDecayMs", lin (250.f, 650.f)); set ("hashSustain", lin (0.6f, 1.f));
        set ("voices", (float) (4 + r.nextInt (4))); set ("detuneCents", lin (35.f, 58.f)); set ("carrierSaw", lin (0.65f, 1.f));
        set ("ratio", pick ({ 1.f, 3.f })); set ("index", lin (0.1f, 0.6f));
        set ("chaos", lin (0.25f, 0.65f)); set ("chaosHz", lin (7.f, 14.f)); set ("grit", 0.f); set ("hashHp", lg (100.f, 1300.f));
        set ("drive1Db", lin (16.f, 25.f)); set ("drive1Curve", pick ({ 0.f, 3.f, 2.f }));
        set ("combMix", 0.f); set ("combFb", 0.f); set ("combRatio", 1.f);
        set ("formHz", lin (1100.f, 1600.f)); set ("formGainDb", lin (0.f, 5.f)); set ("formQ", lin (2.f, 5.f));
        set ("drive2Db", lin (1.f, 11.f)); set ("drive2Curve", 2.f); set ("toneHz", lg (6000.f, 15000.f)); set ("lowCut", lg (20.f, 100.f));
        set ("attackMs", lin (0.5f, 5.f)); set ("decayMs", lin (300.f, 700.f)); set ("sustain", lin (0.65f, 0.8f)); set ("releaseMs", lin (55.f, 80.f));
        set ("chorus", chance (0.5f) ? lin (0.1f, 0.4f) : 0.f);
        set ("ott", lin (0.3f, 0.5f)); set ("dlyMix", lin (0.15f, 0.25f)); set ("dlyDiv", pick ({ 1.f, 2.f, 5.f })); set ("dlyFb", lin (0.25f, 0.4f));
        set ("revMix", lin (0.25f, 0.38f)); set ("revSize", lin (0.55f, 0.82f)); set ("revDamp", lin (0.3f, 0.5f));
        set ("fortune", lin (0.35f, 0.9f));
        set ("cutoffHz", 20000.f); set ("reso", 0.f); set ("autoShape", 0.f); set ("autoOct", 0.f);
    }
    else if (roll < 0.75f)
    {
        // the owner's taste B (his preset "8", 2026-10-22): "vocal" – few, tight voices, strong core, comb an octave up,
        // a loud open formant ~1 kHz, gentle drive 1 + tube drive 2, FORTUNE full, almost no OTT, chorus, big reverb
        set ("scoopSt", chance (0.75f) ? lin (3.f, 7.f) : lin (-6.f, -3.f)); set ("scoopMs", lin (200.f, 380.f)); set ("scoopCurve", lin (0.3f, 0.55f));
        set ("glideMs", lg (50.f, 160.f)); set ("tuneSt", 0.f);
        set ("coreLevel", lin (0.6f, 1.f)); set ("coreShape", lin (0.55f, 0.95f)); set ("coreOct", 1.f);
        set ("hashLevel", lin (0.4f, 0.8f)); set ("hashDecayMs", lin (300.f, 500.f)); set ("hashSustain", lin (0.4f, 0.65f));
        set ("voices", (float) (2 + r.nextInt (3))); set ("detuneCents", lin (8.f, 22.f)); set ("carrierSaw", lin (0.4f, 0.7f));
        set ("ratio", pick ({ 2.f, 3.f })); set ("index", lin (0.4f, 0.9f));
        set ("chaos", lin (0.35f, 0.6f)); set ("chaosHz", lin (9.f, 15.f)); set ("grit", 0.f); set ("hashHp", lg (400.f, 1000.f));
        set ("drive1Db", lin (4.f, 10.f)); set ("drive1Curve", pick ({ 4.f, 3.f }));
        set ("combMix", lin (0.15f, 0.4f)); set ("combFb", lin (0.f, 0.3f)); set ("combRatio", 2.f);
        set ("formHz", lin (850.f, 1300.f)); set ("formGainDb", lin (7.f, 12.f)); set ("formQ", lin (1.5f, 2.6f));
        set ("drive2Db", lin (7.f, 12.f)); set ("drive2Curve", 2.f); set ("toneHz", lg (4800.f, 7500.f)); set ("lowCut", lg (90.f, 160.f));
        set ("attackMs", lin (0.5f, 2.f)); set ("decayMs", lin (300.f, 550.f)); set ("sustain", lin (0.8f, 0.95f)); set ("releaseMs", lin (55.f, 80.f));
        set ("chorus", lin (0.3f, 0.55f));
        set ("ott", lin (0.f, 0.1f)); set ("dlyMix", lin (0.2f, 0.32f)); set ("dlyDiv", pick ({ 5.f, 2.f })); set ("dlyFb", lin (0.28f, 0.4f));
        set ("revMix", lin (0.3f, 0.4f)); set ("revSize", lin (0.7f, 0.85f)); set ("revDamp", lin (0.3f, 0.45f));
        set ("fortune", lin (0.8f, 1.f));
        set ("cutoffHz", 20000.f); set ("reso", 0.f); set ("autoShape", 0.f); set ("autoOct", 0.f);
    }
    else
    {
        set ("scoopSt", chance (0.7f) ? lin (2.f, 14.f) : lin (-8.f, -3.f));
        set ("scoopMs", lg (40.f, 500.f)); set ("scoopCurve", lin (0.2f, 0.6f)); set ("glideMs", lg (10.f, 150.f)); set ("tuneSt", 0.f);
        set ("coreLevel", lin (0.3f, 1.f)); set ("coreShape", r.nextFloat());
        set ("coreOct", chance (0.25f) ? 0.f : (chance (0.12f) ? 2.f : 1.f));                       // choice index: 0 = -1 oct
        set ("hashLevel", lin (0.4f, 1.4f)); set ("hashDecayMs", lg (100.f, 2000.f)); set ("hashSustain", lin (0.3f, 1.f));
        set ("voices", (float) (3 + r.nextInt (5))); set ("detuneCents", lin (10.f, 60.f)); set ("carrierSaw", r.nextFloat());
        set ("ratio", pick ({ 0.5f, 1.f, 1.5f, 2.f, 3.f, 4.f })); set ("index", chance (0.3f) ? lin (0.f, 0.6f) : lg (0.6f, 5.f));
        set ("chaos", lin (0.f, 0.6f)); set ("chaosHz", lg (2.f, 30.f)); set ("grit", chance (0.3f) ? lin (0.f, 0.04f) : 0.f);
        set ("hashHp", lg (100.f, 1500.f));
        set ("drive1Db", lin (6.f, 32.f)); set ("drive1Curve", (float) r.nextInt (5));
        set ("combMix", chance (0.4f) ? lin (0.1f, 0.6f) : 0.f); set ("combFb", lin (-0.7f, 0.8f)); set ("combRatio", pick ({ 0.5f, 1.f, 2.f }));
        set ("formHz", lg (500.f, 3000.f)); set ("formGainDb", lin (0.f, 14.f)); set ("formQ", lg (0.7f, 5.f));
        set ("drive2Db", lin (0.f, 12.f)); set ("drive2Curve", pick ({ 0.f, 2.f })); set ("toneHz", lg (5000.f, 14000.f)); set ("lowCut", lg (60.f, 250.f));
        set ("attackMs", lin (0.5f, 5.f)); set ("decayMs", lg (150.f, 2000.f)); set ("sustain", lin (0.6f, 1.f)); set ("releaseMs", lg (40.f, 300.f));
        set ("chorus", chance (0.6f) ? lin (0.2f, 0.85f) : 0.f);
        set ("ott", lin (0.f, 0.6f)); set ("dlyMix", lin (0.f, 0.3f)); set ("dlyDiv", (float) r.nextInt (6)); set ("dlyFb", lin (0.2f, 0.5f));
        set ("revMix", lin (0.1f, 0.4f)); set ("revSize", lin (0.5f, 0.85f)); set ("revDamp", lin (0.3f, 0.6f));
        set ("fortune", chance (0.5f) ? lin (0.f, 0.6f) : 0.f);
    }

    // level: every random sound about as loud as the presets (2 passes: the drives are not linear)
    set ("outDb", 0.f);
    for (int pass = 0; pass < 2; ++pass)
    {
        const float lvl = measureLoudnessDb();
        if (lvl < -90.f) break;
        set ("outDb", jlimit (-24.f, 12.f, apvts.getRawParameterValue ("outDb")->load() + (randomTargetDb - lvl)));
    }
    currentName = "Random #" + String (1000 + r.nextInt (9000));
    currentProgram = -1;
}

File AlphaProcessor::userFolder()
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("5CR34M 4LPH4").getChildFile ("Presets");
}

Array<File> AlphaProcessor::userPresets() const
{
    auto f = userFolder().findChildFiles (File::findFiles, false, "*.sapreset");
    f.sort();
    return f;
}

bool AlphaProcessor::saveUserPreset (const String& name)
{
    const auto clean = File::createLegalFileName (name.trim());
    if (clean.isEmpty() || ! userFolder().createDirectory()) return false;
    auto st = apvts.copyState();
    if (auto xml = st.createXml())
        if (xml->writeTo (userFolder().getChildFile (clean + ".sapreset"))) { currentName = clean; currentProgram = -1; return true; }
    return false;
}

bool AlphaProcessor::loadUserPreset (const File& f)
{
    auto xml = XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm)) rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto* c : xml->getChildWithTagNameIterator ("PARAM"))
        if (auto* rp = apvts.getParameter (c->getStringAttribute ("id")))
            rp->setValueNotifyingHost (rp->convertTo0to1 ((float) c->getDoubleAttribute ("value")));
    currentName = f.getFileNameWithoutExtension(); currentProgram = -1;
    undo.clearUndoHistory();
    return true;
}

AudioProcessorEditor* AlphaProcessor::createEditor() { return new AlphaEditor (*this); }

void AlphaProcessor::getStateInformation (MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("program", currentProgram, nullptr);
    st.setProperty ("presetName", currentName, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void AlphaProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = ValueTree::fromXml (*xml);
            currentProgram = vt.getProperty ("program", 0);
            currentName = vt.getProperty ("presetName", "INIT").toString();
            apvts.replaceState (vt);
        }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AlphaProcessor(); }
