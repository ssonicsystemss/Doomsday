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
        const juce::StringArray& dynLocations()   { static const juce::StringArray a { "GLOBAL", "PUNCH", "BODY", "TAIL", "SUB", "TEXTURE", "LAYER 1", "LAYER 2", "LAYER 3",
                                                                                       "LAYER 4", "LAYER 5", "LAYER 6", "LAYER 7", "LAYER 8", "SCREECH", "MASTER OUTPUT" }; return a; }
        const juce::StringArray& cmpTypes()       { static const juce::StringArray a { "VCA", "FET", "OPTO", "VARI-MU", "DIGITAL", "UPWARD", "PARALLEL", "SIDECHAIN" }; return a; }
        const juce::StringArray& cmpDetections()  { static const juce::StringArray a { "PEAK", "RMS", "HYBRID", "TRUE PEAK" }; return a; }
        const juce::StringArray& autoReleases()   { static const juce::StringArray a { "MANUAL", "FAST", "MEDIUM", "SLOW", "PROGRAM DEPENDENT" }; return a; }
        const juce::StringArray& scSources()      { static const juce::StringArray a { "SELF", "PUNCH", "BODY", "TAIL", "SUB", "TEXTURE" }; return a; }
        const juce::StringArray& tsDetections()   { static const juce::StringArray a { "FAST TRANSIENT", "SLOW TRANSIENT", "RMS", "PEAK", "HYBRID" }; return a; }
        const juce::StringArray& tsModes()        { static const juce::StringArray a { "PUNCH MODE", "SUSTAIN MODE", "BODY MODE", "TAIL MODE" }; return a; }
        const juce::StringArray& clipTypes()      { static const juce::StringArray a { "SOFT", "HARD", "ASYMMETRIC", "SINE", "CUBIC", "TANH", "CUSTOM CURVE" }; return a; }
        const juce::StringArray& limModes()       { static const juce::StringArray a { "TRANSPARENT", "AGGRESSIVE", "PUNCH", "LOUD", "TRUE PEAK" }; return a; }
        const juce::StringArray& limOversampling(){ static const juce::StringArray a { "1x", "2x", "4x", "8x" }; return a; }
        const juce::StringArray& autoGainModes()  { static const juce::StringArray a { "OFF", "PEAK MATCH", "RMS MATCH", "LOUDNESS MATCH" }; return a; }
        const juce::StringArray& dynOrders()      { static const juce::StringArray a { "COMP > TRANSIENT > CLIP > LIMIT > CREST", "TRANSIENT > COMP > CLIP > LIMIT > CREST",
                                                                                       "COMP > CLIP > TRANSIENT > LIMIT > CREST", "CLIP > COMP > TRANSIENT > LIMIT > CREST" }; return a; }
        // module 09
        const juce::StringArray& lfoWaves()       { static const juce::StringArray a { "SINE", "TRIANGLE", "SAW UP", "SAW DOWN", "SQUARE", "PULSE", "SAMPLE & HOLD", "SMOOTH RANDOM", "CUSTOM" }; return a; }
        const juce::StringArray& smpModes()       { static const juce::StringArray a { "ONE SHOT", "GATE", "LOOP", "PING-PONG" }; return a; }
        const juce::StringArray& spcModes()       { static const juce::StringArray a { "M/S WIDTH", "DECORRELATION", "HAAS (CREATIVE)", "DISABLED" }; return a; }
        const juce::StringArray& spcPolarity()    { static const juce::StringArray a { "OFF", "LEFT", "RIGHT", "BOTH" }; return a; }
        const juce::StringArray& lenSyncs()       { static const juce::StringArray a { "OFF", "1/4 BEAT", "1/2 BEAT", "1 BEAT", "2 BEATS" }; return a; }
        const juce::StringArray& tokModes()       { static const juce::StringArray a { "FOLLOW MIDI", "FIXED" }; return a; }
        const juce::StringArray& punchModes()     { static const juce::StringArray a { "PUNCH", "TOK", "EUPHORIC", "PVC", "ZAAG", "CLASSIC (EARLY)", "RAW" }; return a; }
        const juce::StringArray& rawPunchTypes()  { static const juce::StringArray a { "RAW CLASSIC", "RAW 2015-2017 CRACKING" }; return a; }
        const juce::StringArray& riseSyncs()      { static const juce::StringArray a { "BPM", "TAIL" }; return a; }
        const juce::StringArray& riseGrids()      { static const juce::StringArray a { "1/16", "1/8", "1/4", "1/2", "1 BAR" }; return a; }
        const juce::StringArray& riseCurves()     { static const juce::StringArray a { "LINEAR", "EXPONENTIAL", "S-CURVE", "FAST" }; return a; }
        const juce::StringArray& riseModes()      { static const juce::StringArray a { "POWER", "RAW", "PITCH", "HARMONIC", "HYBRID" }; return a; }
        const juce::StringArray& riseReleases()   { static const juce::StringArray a { "FADE", "HARD STOP", "TAIL FOLLOW" }; return a; }
        const juce::StringArray& crackPitchModes() { static const juce::StringArray a { "FOLLOW PUNCH", "FIXED" }; return a; }
        const juce::StringArray& crackShapes()    { static const juce::StringArray a { "SOFT", "HARD", "FOLD", "ASYM" }; return a; }
        const juce::StringArray& crackModels()    { static const juce::StringArray a { "TRANSIENT CRACK", "CLASSIC LAYER" }; return a; }
        const juce::StringArray& crackSources()   { static const juce::StringArray a { "IMPULSE", "CLICK", "SHORT SAW", "SHORT SQUARE", "NOISE BURST", "HYBRID" }; return a; }
        const juce::StringArray& crackListens()   { static const juce::StringArray a { "KICK", "SOLO SOURCE", "SOLO DRY", "SOLO WET", "SOLO REVERB", "SOLO FINAL" }; return a; }
        const juce::StringArray& crackRoutes()    { static const juce::StringArray a { "PRE-DISTORTION", "POST-DISTORTION", "PARALLEL" }; return a; }
        const juce::StringArray& crackRvDrives()  { static const juce::StringArray a { "SOFT", "HARD CLIP", "ASYMMETRIC", "FOLD" }; return a; }
        const juce::StringArray& hcSources()      { static const juce::StringArray a { "909", "909 HARD", "BD ANALOG", "SINE", "SINE + 909", "HYBRID" }; return a; }
        const juce::StringArray& hcCurves()       { static const juce::StringArray a { "LINEAR", "EXP", "FAST DROP", "S-CURVE", "CUSTOM" }; return a; }
        const juce::StringArray& hcKeyModes()     { static const juce::StringArray a { "FOLLOW MIDI", "FIXED KEY" }; return a; }
        const juce::StringArray& hcCrSources()    { static const juce::StringArray a { "909 / BD", "SINE", "SINE + 909", "SAW" }; return a; }
        const juce::StringArray& hcCrModes()      { static const juce::StringArray a { "OWN", "FOLLOW PUNCH", "FOLLOW BODY" }; return a; }
        const juce::StringArray& hcTextures()     { static const juce::StringArray a { "NOISE", "CRUSHED NOISE", "SHORT TRANSIENT" }; return a; }
        const juce::StringArray& hcClips()        { static const juce::StringArray a { "HARD", "SOFT", "FOLD", "ASYMMETRIC", "CUSTOM" }; return a; }
        const juce::StringArray& hcStages()       { static const juce::StringArray a { "1 STAGE", "2 STAGES", "3 STAGES" }; return a; }
        const juce::StringArray& hcLenModes()     { static const juce::StringArray a { "KICK LENGTH", "BPM GRID" }; return a; }
        const juce::StringArray& hcGrids()        { static const juce::StringArray a { "1/16", "1/8", "1/4", "1/2" }; return a; }
        const juce::StringArray& hcBursts()       { static const juce::StringArray a { "OFF", "SHORT", "PITCHED", "HEAVY", "BY VELOCITY" }; return a; }
        const juce::StringArray& hcMsTracks()     { static const juce::StringArray a { "FIXED HZ", "HARMONIC OF THE NOTE", "FOLLOW PITCH" }; return a; }
        const juce::StringArray& hcMsWaves()      { static const juce::StringArray a { "SINE", "TRIANGLE", "SAW", "SQUARE" }; return a; }
        const juce::StringArray& crunchSources()  { static const juce::StringArray a { "SINE", "SAW", "909 / BD", "SINE + SAW" }; return a; }
        const juce::StringArray& crunchWs()       { static const juce::StringArray a { "SOFT", "HARD", "FOLD", "CRUSH", "CUSTOM" }; return a; }
        const juce::StringArray& crackWs()        { static const juce::StringArray a { "SOFT", "HARD", "ASYMMETRIC", "FOLD", "RECTIFY" }; return a; }
        const juce::StringArray& tokNoteModes()   { static const juce::StringArray a { "FIXED", "FOLLOW MIDI", "OFFSET" }; return a; }
        const juce::StringArray& pshSources() { static const juce::StringArray a { "SNARE", "NOISE", "HYBRID" }; return a; }
        const juce::StringArray& pvcSources()   { static const juce::StringArray a { "SAW", "NOISE", "SNARE", "HYBRID" }; return a; }
        const juce::StringArray& pvcMaterials() { static const juce::StringArray a { "RING MOD", "FLANGER", "COMB" }; return a; }
        const juce::StringArray& tailEngines()  { static const juce::StringArray a { "CLASSIC", "PVC", "ZAAG", "RAW", "EXPERIMENTAL", "HARDCORE" }; return a; }
        const juce::StringArray& gtGateModes()  { static const juce::StringArray a { "SINGLE", "PULSE", "MOVEMENT" }; return a; }
        const juce::StringArray& pvcModels()    { static const juce::StringArray a { "KLOENK", "TUBE" }; return a; }
        const juce::StringArray& pvcPhModes()   { static const juce::StringArray a { "SWEEP", "LFO" }; return a; }
        const juce::StringArray& ptSources()    { static const juce::StringArray a { "KICK BODY", "SINE", "SAW", "HYBRID" }; return a; }
        const juce::StringArray& zgOcts()       { static const juce::StringArray a { "-1 OCT", "SAME", "+1 OCT" }; return a; }
        const juce::StringArray& zgFltTypes()   { static const juce::StringArray a { "LOWPASS", "BANDPASS" }; return a; }
        const juce::StringArray& zgSubSrcs()    { static const juce::StringArray a { "SAW", "SINE", "DERIVED" }; return a; }
        const juce::StringArray& zgVariants()   { static const juce::StringArray a { "CLASSIC", "RESONANT", "BOUNCE", "AGGRESSIVE" }; return a; }
        const juce::StringArray& zgRsnModes()   { static const juce::StringArray a { "PHASER", "RESONATOR", "COMB" }; return a; }
        const juce::StringArray& zgRsnPositions() { static const juce::StringArray a { "BEFORE DIST 1", "BEFORE DIST 2", "BEFORE DIST 3", "AFTER DIST 3" }; return a; }
        const juce::StringArray& rawOsc1s()     { static const juce::StringArray a { "SINE", "TRIANGLE", "SAW", "SQUARE" }; return a; }
        const juce::StringArray& rawOsc2s()     { static const juce::StringArray a { "SINE", "TRIANGLE", "SAW", "OFF" }; return a; }
        const juce::StringArray& rawNoises()    { static const juce::StringArray a { "WHITE", "PINK", "OFF" }; return a; }
        const juce::StringArray& rawStages()    { static const juce::StringArray a { "SOFT", "HARD", "ASYMMETRIC", "WAVE", "FOLD", "TUBE" }; return a; }
        const juce::StringArray& rawFltShapes() { static const juce::StringArray a { "DOWN", "UP", "DOWN-UP", "UP-DOWN", "CUSTOM" }; return a; }
        const juce::StringArray& tailSyncs()    { static const juce::StringArray a { "SYNC", "FREE", "BODY END" }; return a; }
        const juce::StringArray& expOscs()       { static const juce::StringArray a { "SINE", "TRIANGLE", "SAW", "SQUARE", "CUSTOM WAVETABLE" }; return a; }
        const juce::StringArray& expNoises()     { static const juce::StringArray a { "WHITE", "PINK", "DIGITAL" }; return a; }
        const juce::StringArray& expPitchCurves(){ static const juce::StringArray a { "LINEAR DOWN", "LINEAR UP", "EXPONENTIAL DOWN", "EXPONENTIAL UP", "MULTI-STEP", "SINE MODULATION", "RANDOM", "CUSTOM" }; return a; }
        const juce::StringArray& expFxTypes()    { static const juce::StringArray a { "OFF", "FORMANT", "FREQ SHIFTER", "RING MOD", "PITCH SHIFTER", "WAVESHAPER", "BITCRUSHER", "PHASER", "COMB", "STRETCH", "RESONATOR" }; return a; }
        const juce::StringArray& expModSrcs()    { static const juce::StringArray a { "NONE", "ENVELOPE", "LFO 1", "LFO 2", "RANDOM", "KICK POSITION", "MANUAL CURVE" }; return a; }
        const juce::StringArray& expSeeds()      { static const juce::StringArray a { "FIXED SEED", "FREE" }; return a; }
        const juce::StringArray& cpWsTypes()     { static const juce::StringArray a { "SOFT", "HARD", "ASYMMETRIC", "TUBE" }; return a; }
        const juce::StringArray& frontModes()    { static const juce::StringArray a { "AUTO", "ON", "OFF" }; return a; }
        const juce::StringArray& tokPitchModes() { static const juce::StringArray a { "FIXED", "FOLLOW MIDI", "RELATIVE" }; return a; }
        const juce::StringArray& tokTypes()     { static const juce::StringArray a { "SINE", "TRI", "SQUARE", "PUP", "909", "WOOD" }; return a; }
        const juce::StringArray& clickTypes()   { static const juce::StringArray a { "SOFT", "HARD", "DIGITAL", "NOISE", "METAL", "PUNCHY" }; return a; }
        const juce::StringArray& zgDistTypes()  { static const juce::StringArray a { "SOFT", "HARD", "WAVE", "FOLD", "ASYMMETRIC", "DIGITAL", "ZERO-SQUARE" }; return a; }
        const juce::StringArray& ptGateCurves() { static const juce::StringArray a { "HARD", "SOFT", "EXPONENTIAL", "STEPS" }; return a; }
        const juce::StringArray& gtGateDivs()   { static const juce::StringArray a { "1/8", "1/16", "1/32" }; return a; }
        const juce::StringArray& pshSubWaves() { static const juce::StringArray a { "SINE", "TRIANGLE" }; return a; }
        const juce::StringArray& colorTracks() { static const juce::StringArray a { "HZ", "KEY TRACK" }; return a; }
        const juce::StringArray& roomInputs() { static const juce::StringArray a { "TAIL", "TOK + TAIL" }; return a; }
        const juce::StringArray& tailStartModes() { static const juce::StringArray a { "AFTER TOK", "INDEPENDENT" }; return a; }
        const juce::StringArray& noteCutModes()   { static const juce::StringArray a { "LENGTH", "NOTE", "NOTE + FX" }; return a; }
        const juce::StringArray& outModes()       { static const juce::StringArray a { "STEREO", "MONO" }; return a; }
        const juce::StringArray& spcHaasCh()      { static const juce::StringArray a { "DELAY LEFT", "DELAY RIGHT" }; return a; }
        const juce::StringArray& smpLinks()       { static const juce::StringArray a { "INDEPENDENT", "SAMPLE 1", "SAMPLE 2", "SAMPLE 3", "SAMPLE 4", "SAMPLE 5", "SAMPLE 6", "SAMPLE 7", "SAMPLE 8" }; return a; }
        const juce::StringArray& folSources()     { static const juce::StringArray a { "KICK", "PUNCH", "BODY", "TAIL", "SUB", "TEXTURE" }; return a; }
        const juce::StringArray& folDetects()     { static const juce::StringArray a { "PEAK", "RMS", "HYBRID" }; return a; }
        const juce::StringArray& lfoInterps()     { static const juce::StringArray a { "LINEAR", "CUBIC" }; return a; }
        const juce::StringArray& modDivisions()   { static const juce::StringArray a { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/2 D", "1/4 D", "1/8 D", "1/16 D",
                                                                                       "1/2 T", "1/4 T", "1/8 T", "1/16 T", "1/32 T" }; return a; }
        const juce::StringArray& modRetrigs()     { static const juce::StringArray a { "OFF", "ON NOTE", "ON KICK TRIGGER", "ON BAR" }; return a; }
        const juce::StringArray& lfoModes()       { static const juce::StringArray a { "LOOP", "ONE SHOT - HOLD END", "ONE SHOT - BACK TO ZERO" }; return a; }
        const juce::StringArray& modPolarities()  { static const juce::StringArray a { "BIPOLAR", "UNIPOLAR" }; return a; }
        const juce::StringArray& seqDirections()  { static const juce::StringArray a { "FORWARD", "BACKWARD", "PING-PONG", "RANDOM", "ALTERNATE" }; return a; }
        const juce::StringArray& seqSkips()       { static const juce::StringArray a { "ZERO", "HOLD", "SKIP" }; return a; }
        const juce::StringArray& rndModes()       { static const juce::StringArray a { "FIXED SEED", "PER TRIGGER", "FREE RUN", "ONE SHOT", "CONTROLLED RANDOM" }; return a; }
        const juce::StringArray& mbLinks()        { static const juce::StringArray a { "OFF", "SUB + LOW", "MID + HIGH", "SUB + LOW / MID + HIGH", "ALL BANDS" }; return a; }
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

    // "G4" for a frequency (nearest note), and back: "G4" / "a#3" / "Db2" -> Hz (0 = not a note)
    static juce::String hzNote (float hz)
    {
        static const char* n[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int m = (int) std::lround (69.0 + 12.0 * std::log2 (juce::jmax (1.0f, hz) / 440.0));
        return juce::String (n[((m % 12) + 12) % 12]) + juce::String (m / 12 - 1);
    }
    float noteToHz (const juce::String& text)
    {
        const auto s = text.trim().toUpperCase();
        if (s.isEmpty() || s[0] < 'A' || s[0] > 'G') return 0.f;
        static const int base[7] { 9, 11, 0, 2, 4, 5, 7 };          // A B C D E F G
        int semi = base[s[0] - 'A'], k = 1;
        if (k < s.length() && s[k] == '#') { ++semi; ++k; } else if (k < s.length() && s[k] == 'B' && k + 1 < s.length()) { --semi; ++k; }
        const auto oct = s.substring (k).retainCharacters ("-0123456789");
        if (oct.isEmpty()) return 0.f;
        const int midi = (oct.getIntValue() + 1) * 12 + semi;
        return 440.f * std::exp2 ((float) (midi - 69) / 12.f);
    }
    juce::String formatValue (const ParamDef& d, float v)
    {
        using juce::String;
        if (d.unit == Unit::Hz && d.kind == Kind::Float && (String (d.id) == "pe_start" || String (d.id) == "pe_end" || String (d.id) == "punch_pitch"))
            return (v < 100.f ? String (v, 1) : String ((int) std::lround (v))) + " Hz " + hzNote (v);
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
            case Unit::Hz:  if (const float nh = noteToHz (s); nh > 0.f) return nh;
                            return s.containsIgnoreCase ("k") ? n * 1000.f : n;
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
            case rpType:       return "RAW PUNCH TYPE: RAW CLASSIC (a hard, punchy start with a controlled crack melting into the body - KRAK -> DUM) or RAW 2015-2017 CRACKING (a brutal snap, a dense crack generator, the body carrying its energy - KRAK! -> BRUTAL BODY). Two separate engines, each with its own knobs.";
            case rcSnap: case rkSnap:   return "SNAP: how hard the very start hits (the physical attack). Independent of CRACK: SNAP 80 / CRACK 30 = a strong but clean punch.";
            case rcCrack: case rkCrack: return "CRACK: how harmonically aggressive and cracking the start is - a separate harmonic process (fold -> clip -> band), not a drive. 0 = clean, 50 = a clear RAW crack, 100 = extreme.";
            case rcCrackDecay: case rkCrackDecay: return "CRACK DECAY: how long the crack lasts (CLASSIC prefers ~15-45 ms; CRACKING is shorter and follows the kick length).";
            case rcCoupling: case rkCoupling:     return "BODY COUPLING: how the crack joins the body. 0 = the crack ends before the body (KRAK | DUM), 100 = it melts into the body (KRAK -> DUM, no hole).";
            case rcStart: case rkStart: return "PUNCH START: where the punch's own pitch starts.";
            case rcDrop: case rkDrop:   return "PUNCH DROP: how fast the punch's pitch falls to its end.";
            case rcEnd: case rkEnd:     return "PUNCH END: where the punch's pitch ends, in semitones above the note.";
            case rcDecay: case rkDecay: return "PUNCH DECAY: how long the punch's own tone lasts.";
            case rcDensity:    return "SOURCE DENSITY (RAW CLASSIC): the punch source from a sine to a rounded square - more harmonics feeding the crack.";
            case rcCrackHz:    return "CRACK FREQ (RAW CLASSIC): the centre of the crack band.";
            case rcDrive:      return "PUNCH DRIVE (RAW CLASSIC): the punch's own small saturation (the crack is separate).";
            case rkDensity:    return "DENSITY (RAW CRACKING): how many harmonics the crack generator makes - LOW / MEDIUM / HIGH / EXTREME (1-4 fold stages). Never raises the punch level by itself.";
            case rkEdge:       return "EDGE (RAW CRACKING): the aggression of the crack's upper harmonics.";
            case rkTone:       return "DARK / BRIGHT (RAW CRACKING): moves the crack's character - lower-mid harmonics <-> upper-mid / high - on the crack source, not an EQ on the whole punch.";
            case rkDynamic:    return "DYNAMIC (RAW CRACKING): a harder input makes more crack.";
            case rkDrive:      return "BRIDGE DRIVE (RAW CRACKING): the distortion of the bridge that carries the crack's energy into the body.";
            case rsOn:      return "TAIL RISE: the end of the tail GROWS (level / pitch / drive / harmonics / brightness / noise) instead of only fading - synced to the tempo, the same in playback and render. NOTE OFF stays independent.";
            case rsSync:    return "RISE SYNC: BPM = the window is a part of the GRID at the project tempo; TAIL = a part of the kick length (one-shot).";
            case rsGrid:    return "RISE GRID (BPM): the musical length the rise lives in - 1/4 = one beat (e.g. 400 ms at 150 BPM).";
            case rsStart:   return "RISE START: where the rise begins, in % of the grid (BPM) or of the kick (TAIL). 75 % = the last quarter.";
            case rsAmount:  return "RISE AMOUNT: how strong the rise is - what it moves depends on the RISE MODE (never a plain volume knob).";
            case rsCurve:   return "RISE CURVE: LINEAR, EXPONENTIAL (soft start, strong end), S-CURVE (smooth both ends), FAST.";
            case rsMode:    return "RISE MODE: POWER (level, peak-controlled), RAW (drive + harmonics + noise), PITCH (the tail goes up), HARMONIC (harmonics + brightness), HYBRID (your own mix of the layers).";
            case rsRelease: return "RISE END: FADE (30 ms), HARD STOP (3 ms, no click), TAIL FOLLOW (holds until the kick ends).";
            case rsGain: case rsPitch: case rsDrive: case rsHarm: case rsFilter: case rsNoise:
                            return "HYBRID RISE layer: how much this layer grows (only in RISE MODE HYBRID; the other modes set the layers themselves).";
            case rsPitchRange: return "RISE PITCH RANGE: how far the tail's pitch goes up at the top of the rise (PITCH layer).";
            case ckOn:       return "CRACK (RAW PUNCH extension): an own harmonic CRACK layer + SNAP + BODY COUPLING - the RAW 2015-2017 'KRAK!'. Only with PUNCH TYPE RAW; OFF = the RAW punch as before. Mixed after the master clip: the kick's DRIVE never touches it.";
            case ckLevel:    return "CRACK LEVEL: the level of the crack layer (SNAP + CRACK) - never changes the PUNCH, BODY or TAIL.";
            case ckSnap:     return "SNAP: the sharpness and force of the very first hit (works with the crack, never replaces it).";
            case ckCrack:    return "CRACK: the characteristic harmonic crack - harmonics + drive + clip + level together. Not the kick's drive.";
            case ckDensity:  return "DENSITY: the harmonic density of the crack (fold stages).";
            case ckEdge:     return "EDGE: the sharpness of the crack - its upper harmonics.";
            case ckCoupling: return "BODY COUPLING: 0 = the crack stands alone; up = its end is synced to the start of the BODY and it decays into it (one hit, never a glued-on sound). The body's own envelope is never changed.";
            case ckPitchMode:return "CRACK PITCH: FOLLOW PUNCH = tuned against the RAW punch's own pitch (CRACK PITCH st above it), FIXED = from FIXED PITCH down to the note.";
            case ckPitch:    return "CRACK PITCH (FOLLOW PUNCH): semitones above / below the punch.";
            case ckToneHz:   return "FIXED PITCH: where the crack's tone starts in FIXED mode.";
            case ckPitchMod: return "PITCH MOD: a fast, rough pitch modulation of the crack - more aggressive.";
            case ckDrive: case ckShape: return "CRACK DRIVE (+ its type): the crack's own distortion - it never raises the distortion of the whole kick.";
            case ckClip:     return "CRACK CLIP: the crack's own clipper - a sharper, cracking corner, the output level kept.";
            case ckLow: case ckMid: case ckHigh: return "CRACK TONE: low / mid / high of the crack only (cut the low: room for the punch and the body; push the aggressive harmonics).";
            case ckSnapLen:  return "SNAP LENGTH: how long the first impulse lasts (1-15 ms).";
            case ckSnapTone: return "SNAP TONE: the tonal colour of the snap.";
            case ckSnapBody: return "SNAP BODY: how much low / mid the snap has (more = a hit, less = sharper).";
            case ckSnapEdge: return "SNAP EDGE: how fast / sharp the snap's attack is.";
            case ckSrcPunch: case ckSrcTrans: case ckSrcNoise:
                             return "CRACK SOURCE: its own harmonic tone (always) + a bit of the processed PUNCH, a TRANSIENT burst, a little NOISE texture (never the main source).";
            case ckAttack: case ckDecay: case ckSustain: case ckRelease: case ckLength:
                             return "CRACK ENVELOPE: the crack's own short envelope (attack 0-2 ms, decay 10-50 ms, sustain 0-30 %, release after LENGTH).";
            case cnOn:      return "DYNAMIC DISTORTED CRUNCH (RAW punch): a parallel path of the punch source - the RAW 2015-2017 'CZZZZ / GRRRR', a held dense distorted grind from the punch into the body (never a click, never a second DUM). OFF = the RAW punch as before.";
            case cnCrunch:  return "CRUNCH: how much of the crunch layer is in the kick.";
            case cnGrit:    return "GRIT: how aggressive and grainy the distortion is (the asymmetric shaper + its roughness in the body).";
            case cnDensity: return "DENSITY: the harmonic density - more saturation stages, a tighter, dirtier grind.";
            case cnTone:    return "TONE: the crunch darker <-> brighter (its mid focus and its top; never a hiss).";
            case cnFollow:  return "BODY FOLLOW: how far the crunch runs into the BODY - 0 = it ends with the punch, 100 = through the whole body phase (from the real envelopes, not fixed times).";
            case cnSource:  return "CRUNCH SOURCE: the broadband material it is made from - SNARE / NOISE / HYBRID (+ the punch's own tone).";
            case kvHumanize:   return "KICK VARIATION - HUMANIZE: every hit a tiny bit different (pitch up to +-12 cents, level up to +-0.8 dB) - like a real recorded kick, never the same twice. 0 = every kick identical.";
            case peMode:       return "Shape of the pitch movement from START to END.";
            case peStart:      return "Where the tail pitch starts (held for HOLD, then the drop to the note). Type a note (\"G4\"). The tok = PUNCH in TOK mode.";
            case punchMode:    return "TOK: the PUNCH is the tok - TRANSIENT + TOK CORE on one note for its LENGTH, then the TAIL starts (PUNCH / TOK -> TAIL). PUNCH: the classic short punch that glides into the kick, the tail from the hit.";
            case pshSource:    return "SHAPED PUNCH source: a BROADBAND sound the EQ + distortion turn into the character (a sine has nothing to shape). SNARE = noise + a short body, NOISE = white noise, HYBRID = both.";
            case pshTranspose: return "Transposes the SNARE / NOISE source (shorter and higher up). The classic: a snare +14 semitones.";
            case pshMidFreq:   return "CHARACTER: the mid boost BEFORE the distortion - this band is driven into the ceiling. ~700 Hz woody, ~1 kHz default, ~1.6 kHz sharp.";
            case pshMidGain:   return "CHARACTER: how much the mid band is boosted before the distortion.";
            case pshDrive:     return "CHARACTER: the distortion (asymmetric) after the mid boost - the punch's grit.";
            case pshAsym:      return "How one-sided the distortion is: 0 = symmetric, 1 = the negative half much softer (more even harmonics, a fuller middle).";
            case pshTape:      return "TAPE STOP: how far the punch slows / drops in pitch towards its end (0 = static).";
            case pshTapeStart: return "TAPE STOP: where in the punch the slow-down starts (part of its length).";
            case pshSpike:     return "VOLUME SHAPER: the transient spike at the start (ms).";
            case pshBody:      return "VOLUME SHAPER: the level of the punch body after the spike.";
            case pshBodyMs:    return "VOLUME SHAPER: how long the body holds (ms from the hit).";
            case pshEnd:       return "VOLUME SHAPER: where the shaped punch has faded out (ms).";
            case pshLowCut:    return "Corrective EQ: the lows out of the shaped punch (its low end comes from the SUB).";
            case pshClick:     return "CLICK: a separate very short transient (a distorted laser) on top of the punch.";
            case pshSub:       return "SUB of the punch: a mono sine on the played note under the punch.";
            case pshSubDelay:  return "SUB of the punch enters a little LATER: first the transient, then the push in the chest.";
            case pshVerb:      return "A very short stereo reverb on the shaped punch (a little - for stereo).";
            case pshTonal:     return "BLEND: the level of the TONAL layer (the shaped punch itself) against its CLICK and SUB.";
            case pshClickLen:  return "CLICK: how long the transient lasts (ms).";
            case pshClickTone: return "CLICK: its character - where the laser starts (low = a knock, high = a tick).";
            case pshSubWave:   return "SUB PUNCH: SINE (clean) or TRIANGLE (a little more mid).";
            case pshSubStart:  return "SUB PUNCH: its own pitch envelope - how far above the note it starts (semitones).";
            case pshSubDrop:   return "SUB PUNCH: how fast it falls to the note (ms).";
            case pshSubDecay:  return "SUB PUNCH: its own decay - where it has faded out (ms).";
            case pvcSource:    return "PVC source: SAW (a monotone saw at PITCH), NOISE, SNARE or HYBRID (saw + noise).";
            case pvcPitch:     return "PVC: the pitch of the saw source - the punch stays on it (monotone, no pitch drop: KLOENK, KLOENK).";
            case pvcResFreq:   return "PVC RESONANCE: where the 'tube' rings (150 Hz - 2.5 kHz) - find where the hit sounds most like plastic.";
            case pvcResQ:      return "PVC RESONANCE: how narrow / ringing the tube is.";
            case pvcResAmount: return "PVC RESONANCE: how much of the source goes through the tube before the material.";
            case pvcMaterial:  return "PVC MATERIAL: RING MOD (metallic, inharmonic), FLANGER (a moving comb = a hollow pipe), COMB (a fixed pipe).";
            case pvcMix:       return "PVC BLEND: the clean source against the resonance / material - the main PVC control (the crunchy KLOENK is the mix).";
            case pvcRingRatio: return "PVC RING MOD: its frequency = PITCH x RATIO (the harmonics line up differently per ratio).";
            case pvcComp:      return "PVC: a compressor BEFORE the distortion - an even, dense, saw-like saturation.";
            case pvcDrive:     return "PVC: the distortion after the compressor.";
            case pvcSat:       return "PVC: a softer saturation mixed in after the drive (rounder edges).";
            case pvcAsym:      return "PVC: how one-sided the distortion is (more even harmonics).";
            case pvcLowMid:    return "PVC EQ after the distortion: low-mid (~300 Hz) - boxy / plastic.";
            case pvcMid:       return "PVC EQ after the distortion: mid (~1 kHz).";
            case pvcHighMid:   return "PVC EQ after the distortion: high-mid (~3 kHz) - the attack.";
            case pvcAttack:    return "PVC SHAPE: the attack of the punch (ms).";
            case pvcLength:    return "PVC SHAPE: how long the punch holds (ms).";
            case pvcDecay:     return "PVC SHAPE: how fast it falls after that (ms).";
            case tailEngine:   return "TAIL ENGINE: CLASSIC (the tonal tail) or PVC (the KLOENK tail: a source - by default the kick BODY - into a REVERB, high cut, drive, clip, gate, drive again; low end mono, highs wide).";
            case gtComp:       return "PVC TAIL PRE-COMP: evens the reverb out BEFORE DRIVE 1 (a dense, even distortion).";
            case gtGate:       return "PVC TAIL GATE: SINGLE = one shape (CURVE over GATE LENGTH); PULSE = chopped in time with the song (RATE); MOVEMENT = irregular steps (the same on every hit).";
            case gtGateLen:    return "PVC TAIL GATE LENGTH: how long the gate shape takes (ms from the tail start).";
            case gtGateDepth:  return "PVC TAIL GATE DEPTH: how far the gate goes down (100 % = silence after GATE LENGTH).";
            case gtGateDiv:    return "PVC TAIL GATE RATE: the PULSE / MOVEMENT step, synced to the song's tempo.";
            case gtFilter:     return "PVC TAIL FILTER: a resonant low-pass after DRIVE 2 (moves with MOVE RATE / MOVE DEPTH).";
            case gtReso:       return "PVC TAIL FILTER: the resonance.";
            case gtMoveRate:   return "PVC TAIL FILTER: how fast it moves (Hz).";
            case gtMoveDepth:  return "PVC TAIL FILTER: how far it moves.";
            case pvcModel:     return "PVC model: KLOENK (resonance / ring / flanger / comb -> blend -> compressor) or TUBE (from the owner's samples: 2 tube tones + a pipe comb, a pitch jump at the start, a drop at the end, a phaser).";
            case pvcTubeHz:    return "TUBE: the pitch of the tube (Hz).";
            case pvcTubeComb:  return "TUBE: the 2 tube tones <-> the pipe comb (more = more plastic, hollow).";
            case pvcJump:      return "TUBE: how far above the pitch it starts (the 'ci' of 'ciup', semitones).";
            case pvcDrop:      return "TUBE: how far the pitch falls at the end of the punch (semitones).";
            case pvcDropTime:  return "TUBE: how long that fall takes (ms before the end of LENGTH).";
            case pvcPhaser:    return "TUBE: the phaser amount.";
            case pvcPhMode:    return "TUBE PHASER: SWEEP = one pass from high to low over the punch; LFO = a moving phaser (RATE).";
            case pvcPhRate:    return "TUBE PHASER: the LFO rate (Hz).";
            case pvcPhFb:      return "TUBE PHASER: feedback (a deeper, more resonant phaser).";
            case pvcBright:    return "TUBE: brightness - the low-pass after the drive (Hz).";
            case ptSource:     return "PVC TAIL SOURCE: KICK BODY (the body of this kick - plays the note, also with its fader down), SINE, SAW or HYBRID (body + sine).";
            case ptStart:      return "PVC TAIL PITCH (SINE / SAW / HYBRID): how far above the root it starts (semitones).";
            case ptHold:       return "PVC TAIL PITCH: how long it holds the start pitch (ms).";
            case ptDropTime:   return "PVC TAIL PITCH: how long the drop to the root takes (ms).";
            case ptRoot:       return "PVC TAIL ROOT: the note of the tail against the played note (semitones).";
            case ptSlide:      return "PVC TAIL SLIDE: a slow fall after the root, until the end of the tail (semitones).";
            case ptResFreq:    return "PVC TAIL RESONANCE: the frequency of a resonant peak on the source.";
            case ptResQ:       return "PVC TAIL RESONANCE: Q.";
            case ptResAmount:  return "PVC TAIL RESONANCE: amount.";
            case ptRevSize:    return "PVC TAIL REVERB: size (the reverb is the MATERIAL of the tail, not an effect at the end).";
            case ptRevDecay:   return "PVC TAIL REVERB: decay (ms).";
            case ptRevPre:     return "PVC TAIL REVERB: pre-delay (ms).";
            case ptRevCut:     return "PVC TAIL HIGH CUT: the low-pass right after the reverb - what stays is the bass that gets distorted.";
            case ptRevWet:     return "PVC TAIL REVERB: wet (100 % = only the reverb, the tutorial way).";
            case ptDrive1:     return "PVC TAIL DRIVE 1: the first, soft distortion stage (dB).";
            case ptClip:       return "PVC TAIL CLIP: a hard clip after DRIVE 1.";
            case ptDrive2:     return "PVC TAIL DRIVE 2: the distortion AFTER the gate (dB).";
            case ptAsym:       return "PVC TAIL ASYMMETRY: of DRIVE 2 (more even harmonics, a rougher growl).";
            case ptGateCurve:  return "PVC TAIL GATE CURVE: HARD (full, then down at GATE LENGTH), SOFT, EXPONENTIAL or STEPS (down in steps).";
            case ptMovement:   return "PVC TAIL MOVEMENT: how deep PULSE / MOVEMENT go.";
            case ptLow:        return "PVC TAIL SPECTRUM LOW: the weight (~40-120 Hz).";
            case ptLowMid:     return "PVC TAIL SPECTRUM LOW MID: the pipe (~150-500 Hz) - down against a muddy rumble.";
            case ptMid:        return "PVC TAIL SPECTRUM MID: the growl (~0.5-2 kHz).";
            case ptHigh:       return "PVC TAIL SPECTRUM HIGH: the distortion fizz (2 kHz +).";
            case ptMonoBelow:  return "PVC TAIL STEREO: everything below this stays mono.";
            case ptWidth:      return "PVC TAIL STEREO: the width of the highs (opens up over the tail).";
            case ptSide:       return "PVC TAIL STEREO: the reverb on the sides only (highs).";
            case ptDelay:      return "PVC TAIL START: when the tail comes in (ms after the hit) - usually at the end of the punch.";
            case ptAttack:     return "PVC TAIL FINAL SHAPE: attack (ms).";
            case ptLength:     return "PVC TAIL FINAL SHAPE: length from the start (ms).";
            case ptFade:       return "PVC TAIL FINAL SHAPE: the fade at the end (ms).";
            case ptGrow:       return "TAIL GROW: DRIVE 1 starts this much lower and rises to its value over the tail - the tail gets rougher and brighter as it goes (zaag).";
            case zgStart: return "ZAAG PITCH START: how far above the root the saw starts (semitones; CLASSIC zaag ~ +24, MODERN ~ +7).";
            case zgDrop: return "ZAAG DROP TIME: how long the fall to the root takes (ms).";
            case zgSaw: return "ZAAG SAW A: sine <-> saw.";
            case zgDrive: return "ZAAG 03 DESTROY: the wall - the strongest drive.";
            case zgDirt: return "ZAAG: noise mixed BEFORE the drive - the dirt (different left / right = width).";
            case zgDirtTone: return "ZAAG: how bright the dirt is.";
            case zgSub: return "ZAAG: a clean sine on the note under it (the weight, mono).";
            case zgPress: return "ZAAG SOFT CLIPPER: how hard it is pressed (the wall).";
            case zgWidth: return "ZAAG: the width of the highs (the lows stay mono).";
            case zgClick: return "ZAAG: a 2 ms noise click at the very start.";
            case zgPiep: return "ZAAG PIEP: a short tonal pip FIRST (falls to half its pitch).";
            case zgPiepHz: return "ZAAG PIEP: its pitch (Hz).";
            case zgPiepMs: return "ZAAG PIEP: its length (ms).";
            case zgTrask: return "ZAAG TRASK: the crunch AFTER the piep (the falling saw x3, hard, 700 Hz - 3.5 kHz).";
            case zgTraskDelay: return "ZAAG TRASK: when it comes in (ms) - after the piep.";
            case zgTraskMs: return "ZAAG TRASK: its length (ms).";
            case zgVerb: return "ZAAG PUNCH REVERB: a reverb on the start of the kick only (no lows) - the space.";
            case zgVerbDecay: return "ZAAG PUNCH REVERB: decay (ms).";
            case zgVerbSize: return "ZAAG PUNCH REVERB: size.";
            case zgVerbSend: return "ZAAG PUNCH REVERB: how much of the start goes in (ms; 30 = only the piep and the trask).";
            case zgVerbPre: return "ZAAG PUNCH REVERB: pre-delay (ms) - a cleaner punch.";
            case zgVerbGate: return "ZAAG PUNCH REVERB: GATED reverb - cut after this (ms; 0 = off).";
            case zgVerbDamp: return "ZAAG PUNCH REVERB: brightness (damping).";
            case zgVerbLowCut: return "ZAAG PUNCH REVERB: the low cut of what goes in.";
            case zgSawbOct: return "ZAAG SAW B: an octave down / the same / an octave up.";
            case zgSawbDetune: return "ZAAG SAW B: a small detune (cents) - dense, not a big unison.";
            case zgSawbLevel: return "ZAAG HIGH SAW LEVEL: the quiet saw 3 octaves above the low saw (the source itself is fixed: LOW SAW + HIGH SAW).";
            case zgCurve: return "ZAAG PITCH: the shape of the drop (0 = straight, 100 % = fast at the start).";
            case zgRoot: return "ZAAG ROOT: the note of the tail against the played note (semitones).";
            case zgLowCtrl: return "ZAAG LOW CONTROL: how much low end goes into the distortion stages (less = a steadier sub).";
            case zgS1Fund: return "ZAAG 01 GENERATE: boost the fundamental (follows the note).";
            case zgS1MidHz: return "ZAAG 01 GENERATE: the mid boost frequency.";
            case zgS1Mid: return "ZAAG 01 GENERATE: the mid boost.";
            case zgS1LowCut: return "ZAAG 01 GENERATE: an optional low cut (10 Hz = off).";
            case zgS1Drive: return "ZAAG 01 GENERATE: the first waveshaper (richer harmonics, not the aggression yet).";
            case zgS1Asym: return "ZAAG 01 GENERATE: asymmetry (even harmonics).";
            case zgS1Curve: return "ZAAG 01 GENERATE: the curve (soft tanh -> a harder knee).";
            case zgS2Low: return "ZAAG 02 CHARACTER: LOW = the weight.";
            case zgS2LowMid: return "ZAAG 02 CHARACTER: LOW-MID = the mass.";
            case zgS2Mid: return "ZAAG 02 CHARACTER: MID = the zaag growl.";
            case zgS2HiMid: return "ZAAG 02 CHARACTER: HIGH-MID = the aggression.";
            case zgS2Drive: return "ZAAG 02 CHARACTER: the second waveshaper.";
            case zgS3Asym: return "ZAAG 03 DESTROY: asymmetry.";
            case zgBits: return "ZAAG 03 DESTROY - EXPERIMENTAL: bit reduction (never randomized on).";
            case zgFold: return "ZAAG 03 DESTROY - EXPERIMENTAL: wavefold (never randomized on).";
            case zgSquash: return "ZAAG SUB PUSH (low shelf before the clipper): more low end into the soft clipper - a denser tail, fewer sticking-out transients, more energy in the middle.";
            case zgFinalTone: return "ZAAG FINAL EQ: darker / brighter before the soft clipper.";
            case zgKnee: return "ZAAG SOFT CLIPPER: the knee (gentle -> almost hard, never a sharp cut).";
            case zgFltType: return "ZAAG FILTER MOVEMENT: low-pass or band-pass after the distortion.";
            case zgFltStart: return "ZAAG FILTER MOVEMENT: where the filter starts.";
            case zgFltEnd: return "ZAAG FILTER MOVEMENT: where it ends (lower = AAARRR...OOOW, higher = it opens).";
            case zgFltTime: return "ZAAG FILTER MOVEMENT: how long the move takes.";
            case zgFltCurve: return "ZAAG FILTER MOVEMENT: the curve of the move.";
            case zgFltReso: return "ZAAG FILTER MOVEMENT: resonance.";
            case zgFltLfo: return "ZAAG FILTER MOVEMENT: an LFO on top (the tail lives).";
            case zgFltLfoRate: return "ZAAG FILTER MOVEMENT: the LFO rate.";
            case zgSubSrc: return "ZAAG SUB SUPPORT: SAW (the raw saw low end), SINE, or DERIVED (the processed low end) - mono.";
            case zgSubFreq: return "ZAAG SUB SUPPORT: the split between the sub and the rest.";
            case zgSubDecay: return "ZAAG SUB SUPPORT: how fast the sub dies in the tail.";
            case zgCharacter: return "ZAAG CHARACTER: a blend of distortion algorithms in the main waveshaper - from SOFT into the DISTORTION TYPE.";
            case zgGrit: return "ZAAG DRIVE: the strength of the nonlinearity - all distortion stages together (not the EQ, not the FX).";
            case zgTone: return "ZAAG TONE: every EQ an octave down (dark) / up (bright).";
            case zgFocus: return "ZAAG FOCUS: how narrow the EQs pick the harmonics (dense <-> a singing tail).";
            case zgLenFollow: return "ZAAG LENGTH: follow the kick length (LENGTH / SYNC) - it never ends between kicks.";
            case zgLength: return "ZAAG LENGTH (when it does not follow the kick).";
            case zgAttack: return "ZAAG SHAPE: attack.";
            case zgBody: return "ZAAG SHAPE: how long it holds full level (of the length).";
            case zgDecay: return "ZAAG SHAPE: how deep it goes down after the body.";
            case zgFade: return "ZAAG SHAPE: the fade curve.";
            case zgTraskTone: return "ZAAG TRASK TONE: how bright the trask is (the OPS zaags: a bright punch).";
            case zgVariant: return "ZAAG VARIANT: CLASSIC (the tutorial chain) / RESONANT (a louder, narrower sweep that reopens) / BOUNCE (the bass breathes in and out over the tail, a sharper pitch hit) / AGGRESSIVE (more drive, harder clip, the character pushed to zero-square).";
            case zgZaag: return "ZAAG: the main intensity - the distortion stages, the harmonics, the resonant EQ, the unique FX and the stereo together.";
            case zgResonance: return "ZAAG RESONANCE: the narrow resonant sweep (gain + Q) and the resonator's resonance.";
            case zgMovement: return "ZAAG MOVEMENT: how deep the EQ moves (the moving low shelf and the travel of the resonant sweep).";
            case zgBass: return "ZAAG BASS: the balance sub <-> mids. Left = more sub and more bass pushed into the clipper; right = less low end in the distortion, more mids.";
            case zgStereo: return "ZAAG STEREO: only the safe stereo - the highs wider (width, side boost, high reverb). The lows stay mono at any setting.";
            case zgCrush: return "ZAAG CRUSH: the final clipper from soft and round (left) to hard and square (right).";
            case zgS1Mix: return "ZAAG DIST 1 MIX: dry / distorted of the first (maximally aggressive) waveshaper.";
            case zgEqmShelf: return "ZAAG LOW SHELF AMOUNT: how many dB the moving low shelf (before DIST 2) reaches at 100 %. The shelf moves 0 -> 100 % over the punch, then 100 -> MOTION DEPTH over the tail.";
            case zgEqmDepth: return "ZAAG MOTION DEPTH: where the moving low shelf ends at the end of the kick (50 % = the tutorial).";
            case zgEqmTime: return "ZAAG MOTION TIMING: where the punch part ends, as a part of KICK LENGTH (the shelf peak and the end of the sweep's first fall). Scales with the kick length.";
            case zgPeak: return "ZAAG SWEEP GAIN: the narrow resonant peak before DIST 3 (a dynamic resonant frequency sweep locked to the kick envelope).";
            case zgPeakQ: return "ZAAG SWEEP Q: how narrow the resonant sweep is.";
            case zgSwStart: return "ZAAG SWEEP START: where the resonant peak starts, as a multiple of the fundamental (it starts high).";
            case zgSwPlateau: return "ZAAG SWEEP PLATEAU: where the peak holds after the punch (x the fundamental).";
            case zgSwEnd: return "ZAAG SWEEP END: where the peak ends after the second fall (x the fundamental).";
            case zgSwHold: return "ZAAG SWEEP HOLD UNTIL: where the plateau ends and the second fall starts (a part of KICK LENGTH).";
            case zgSwReopen: return "ZAAG SWEEP REOPEN: the peak opens up again at the end of the kick (0 = no).";
            case zgRsnMode: return "ZAAG RESONATOR: PHASER (allpass notches) / RESONATOR (a narrow ringing peak) / COMB (hollow, tube-like). Adds character, does not replace the kick.";
            case zgRsnFreq: return "ZAAG RESONATOR FREQ: x the fundamental (it moves with the pitch and the sweep).";
            case zgRsnReso: return "ZAAG RESONATOR RESONANCE: how much it rings.";
            case zgRsnWidth: return "ZAAG RESONATOR STEREO WIDTH: L and R a bit different - the stereo goes through the main distortion; the lows are made mono later (ZAAG STEREO).";
            case zgRsnMix: return "ZAAG RESONATOR MIX: low / moderate by design.";
            case zgRsnPos: return "ZAAG RESONATOR POSITION: where it sits in the cascade (the earlier, the more distortion stages chew its colour).";
            case zgS3Type: return "ZAAG DISTORTION TYPE: the main (third) waveshaper. ZAAG CHARACTER blends from SOFT into this type.";
            case zgS3Preamp: return "ZAAG DIST 3 PREAMP: how hot the signal goes into the main waveshaper.";
            case zgS3Mix: return "ZAAG DIST 3 MIX: dry / distorted of the main waveshaper.";
            case zgDistEnv: return "ZAAG DISTORTION ENVELOPE: the drives of all stages fall over the kick (scaled to KICK LENGTH).";
            case zgLmCut: return "ZAAG LOW-MID CUT: cleans 250-400 Hz before the soft clipper.";
            case zgSideCut: return "ZAAG SIDE LOW CUT: below this the side channel is cut (24 dB / oct) - the lows always mono.";
            case zgSideBoostHz: return "ZAAG SIDE BOOST FREQ: a band boosted on the side channel only - the stereo exactly there.";
            case zgSideBoost: return "ZAAG SIDE BOOST: how much (dB, side only).";
            case zgPup: return "ZAAG PUP: the raw-style click - a driven saw that dives down (\"pup\"); it goes into the punch reverb and the master clip.";
            case zgPupHz: return "ZAAG PUP PITCH: where the pup starts (1 kHz = \"pup\", 4 kHz = \"tjiu\").";
            case zgPupMs: return "ZAAG PUP LENGTH: how long the pup is (and how long it dives).";
            case zgPupOct: return "ZAAG PUP DIVE: how many octaves the pup falls over its length.";
            case zgPupDrive: return "ZAAG PUP DRIVE: how scratchy the pup is.";
            case tkLevel: return "TOK LEVEL: the TOK - its own layer BEFORE the punch (\"tok\"): shorter and more percussive than the punch, longer than the click. -60 dB = off.";
            case tkType: return "TOK TYPE: the source of the one TOK CORE - SINE (clean, neutral), TRI (soft, more harmonics), SQUARE (TONE: hollow / wooden <-> full / harsh), PUP (a saw on the pitch envelope = \"pup!\"), 909 (a sine body + a noise tik at its start), WOOD (a struck resonance, PITCH moves it).";
            case tkTune: return "TOK PITCH: FIXED = +-24 st around G3 (the same tok on every note); RELATIVE = semitones from the played note; not used with FOLLOW MIDI.";
            case tkLength: return "TOK LENGTH: EARLY ~25-45 ms, MODERN ~35-70, RAW ~30-65, EXPERIMENTAL 10-80. Longer than the click, shorter than the punch.";
            case tkPeak: return "TOK ATTACK: how long the tok rises to its peak (EARLY 2-6 ms, MODERN 2-8, RAW 1-7; a tick is instant).";
            case tkFade: return "TOK DECAY: a fast (left) <-> slow (right) fall after the peak, within the LENGTH.";
            case tkTone: return "TOK TONE: dark <-> bright.";
            case clkLevel: return "CLICK LEVEL: the CLICK - a module of its own, the \"ts\" = the spark of the attack (0 - 10 / 20 ms). It goes to the PUNCH BUS and never through the ZAAG cascade. -60 dB = off.";
            case clkType: return "CLICK TYPE: SOFT / HARD / DIGITAL / NOISE / METAL / PUNCHY (a raw saw diving down).";
            case clkPitch: return "CLICK PITCH: the click's pitch / centre.";
            case clkLength: return "CLICK LENGTH: usually 10 - 20 ms.";
            case clkDecay: return "CLICK DECAY: held then cut (left) <-> falls away at once (right).";
            case clkTone: return "CLICK TONE: dark <-> bright.";
            case clkDrive: return "CLICK DRIVE: the click's own distortion.";
            case clkMix: return "CLICK MIX: clean <-> driven.";
            case spkLevel: return "SUB PUNCH LEVEL: SUB PUNCH != SUB - a short impulse of the low / lower mid that gives the kick its hit (10 - 70 ms). It is gone before the SUB dominates. -60 dB = off.";
            case spkStart: return "SUB PUNCH PITCH START: where its pitch envelope starts.";
            case spkEnd: return "SUB PUNCH PITCH END: where it falls to (keep it at / above the sub's note, they must not fight).";
            case spkDelay: return "SUB PUNCH START: when it starts (~10 ms after the hit).";
            case spkLength: return "SUB PUNCH LENGTH: 10 - 70 ms typical.";
            case spkDrive: return "SUB PUNCH DRIVE: meat in the lower mid.";
            case spkTone: return "SUB PUNCH TONE: dark <-> brighter.";
            case tkPitchMode: return "TOK PITCH MODE: FIXED (default - G3 + PITCH on every note), FOLLOW MIDI (the played note, in the tok register), RELATIVE (the played note + PITCH, e.g. PUP -7 st).";
            case tkPenvStart: return "TOK PITCH START: where the pitch envelope starts, in st above the tok pitch (PUP: +12 / +24 = the \"pup!\").";
            case tkPenvEnd: return "TOK PITCH END: where the fast drop lands, in st from the tok pitch (0 / -12).";
            case tkPenvTime: return "TOK DROP TIME: how fast the pitch falls from START to END.";
            case tkDrive: return "TOK DRIVE: the TOK CORE saturation, after the amp envelope (it gets cleaner as the tok fades).";
            case tkMode: return "TOK MODE: AUTO (the STYLE / preset decides whether the tok plays and sets its character), ON (always), OFF (never). A style change updates AUTO only.";
            case clkMode: return "CLICK MODE: AUTO (the STYLE / preset decides whether the click plays and sets its character), ON (always), OFF (never). A style change updates AUTO only.";
            case spkMode: return "SUB PUNCH MODE: AUTO (the STYLE / preset decides whether the sub punch plays and sets its character), ON (always), OFF (never). A style change updates AUTO only.";
            case tkAuto: return "TOK AUTO ON: set by the STYLE / preset - whether the tok plays in AUTO mode.";
            case clkAuto: return "CLICK AUTO ON: set by the STYLE / preset - whether the click plays in AUTO mode.";
            case spkAuto: return "SUB PUNCH AUTO ON: set by the STYLE / preset - whether the sub punch plays in AUTO mode.";
            case rwOsc1: return "RAW OSC 1: the main source - SINE / TRIANGLE (default, the rawstyle start) / SAW / SQUARE.";
            case rwOsc2: return "RAW OSC 2: a second source - SINE / TRIANGLE / SAW / OFF (default).";
            case rwOsc2Lvl: return "RAW OSC 2 LEVEL: how loud the second oscillator is.";
            case rwOsc2St: return "RAW OSC 2 TUNE: semitones from OSC 1 (12 = an octave up).";
            case rwNoise: return "RAW NOISE: WHITE / PINK / OFF (default) - a noise source under the oscillators, into the distortion.";
            case rwNoiseLvl: return "RAW NOISE LEVEL: how much noise goes into the stack.";
            case rwStart: return "RAW PITCH START: where the tail starts, in st above the note (a controlled drop: the hit).";
            case rwDrop: return "RAW PITCH DROP: how long the drop to the note takes (ms - the hit stays the same at any length).";
            case rwCurve: return "RAW PITCH CURVE: linear <-> a fast, exponential drop.";
            case rwRoot: return "RAW ROOT: the tail note, in st from the MIDI note.";
            case rwPreOn: return "RAW PRE-EQ on / off (low cut + mid before DIST 1).";
            case rwLowCut: return "RAW PRE LOW CUT: how much low end goes into the distortion.";
            case rwPreMid: return "RAW PRE MID: a mid bell before DIST 1 (more = more mid harmonics).";
            case rwD1On: return "RAW DIST 1 on / off - the main harmonic generation (aggressive).";
            case rwD1Type: return "RAW DIST 1 TYPE: SOFT / HARD / ASYMMETRIC (default) / WAVE / FOLD / TUBE.";
            case rwD1Db: return "RAW DIST 1 DRIVE: the strongest stage - it shapes the character.";
            case rwShapeOn: return "RAW SHAPING EQ on / off (the GROWL bell between DIST 1 and DIST 2).";
            case rwGrowlHz: return "RAW GROWL FREQ: the shaping bell - ~400-500 Hz is the classic rawstyle area (a start for tuning, not a rule).";
            case rwGrowlDb: return "RAW GROWL GAIN: how much the growl bell lifts before DIST 2.";
            case rwGrowlQ: return "RAW GROWL Q: wide <-> narrow growl.";
            case rwD2On: return "RAW DIST 2 on / off - densifies the signal (strong).";
            case rwD2Type: return "RAW DIST 2 TYPE: SOFT / HARD (default) / ASYMMETRIC / WAVE / FOLD / TUBE.";
            case rwD2Db: return "RAW DIST 2 DRIVE: less than DIST 1 - it thickens what DIST 1 made.";
            case rwBpOn: return "RAW BAND-PASS SWEEP on / off - the resonant movement (the tonal motion while it rings out).";
            case rwFltShape: return "RAW SWEEP SHAPE: DOWN / UP / DOWN-UP / UP-DOWN / CUSTOM (START -> MID -> END).";
            case rwFltStart: return "RAW FILTER START: where the band-pass sweep starts.";
            case rwFltEnd: return "RAW FILTER END: where the sweep ends.";
            case rwFltMid: return "RAW FILTER MID: the middle point of the CUSTOM shape.";
            case rwFltReso: return "RAW FILTER RESONANCE: the peak of the sweep (the vowel of the growl).";
            case rwFltSpeed: return "RAW SWEEP SPEED (SYNC): the sweep lasts this part of the TAIL length - it scales with KICK LENGTH.";
            case rwFltTime: return "RAW SWEEP TIME (FREE): the sweep time in ms, independent of the length.";
            case rwFltCurve: return "RAW SWEEP CURVE: an early, fast move <-> a late one.";
            case rwFltMix: return "RAW FILTER MIX: the dry stack <-> the band-passed sweep.";
            case rwSync: return "RAW TIME SYNC: SYNC (the movements scale with the TAIL length), FREE (times in ms), BODY END (the sweep starts at the end of the BODY).";
            case rwD3On: return "RAW DIST 3 on / off - the final character (controlled).";
            case rwD3Type: return "RAW DIST 3 TYPE: SOFT (default) / HARD / ASYMMETRIC / WAVE / FOLD / TUBE.";
            case rwD3Db: return "RAW DIST 3 DRIVE: the smallest - it refines without destroying the texture.";
            case rwPostOn: return "RAW POST-EQ on / off (low cut + high shelf after DIST 3).";
            case rwPostLowCut: return "RAW POST LOW CUT: cleans the very low end after the stack.";
            case rwPostHigh: return "RAW POST HIGH: the brightness of the tail.";
            case rwClipOn: return "RAW SOFT CLIPPER on / off (peak control).";
            case rwClipHard: return "RAW CLIPPER HARDNESS: soft <-> hard.";
            case rwGrowl: return "RAW GROWL (macro): the mid harmonics + the resonance intensity (the growl bell and the sweep resonance together).";
            case rwCrunch: return "RAW CRUNCH (macro): the distortion + clipping aggression.";
            case rwVerb: return "RAW REVERB MIX: subtle by default; it starts after the attack and has no low end (never smears the hit).";
            case rwVerbSize: return "RAW REVERB SIZE.";
            case rwVerbDecay: return "RAW REVERB DECAY.";
            case rwVerbDamp: return "RAW REVERB DAMPING: the high cut inside the reverb.";
            case rwVerbPre: return "RAW REVERB PRE-DELAY.";
            case rwVerbLowCut: return "RAW REVERB LOW CUT: what goes into the reverb (no bass tail).";
            case rwSub: return "RAW SUB: the lows of the distorted tail <-> a clean sine on the tail pitch (mono).";
            case rwSubHz: return "RAW SUB FREQ: the crossover under which the tail is the (mono) sub.";
            case rwWidth: return "RAW WIDTH: a controlled width of the harmonics above ~250 Hz; the centre stays stable, the lows mono.";
            case rwAttack: return "RAW TAIL ATTACK: the fade-in of the tail.";
            case rwBody: return "RAW TAIL BODY: how much of the length stays at full level.";
            case rwDecay: return "RAW TAIL DECAY: how far it falls after the body.";
            case rwFade: return "RAW TAIL FADE: the shape of the fall.";
            case tlDrive: return "TAIL DRIVE (macro): RAW = the distortion aggression, EXPERIMENTAL = the nonlinearity intensity.";
            case tlTone: return "TAIL TONE (macro): RAW = brightness and the pitch of the growl, EXPERIMENTAL = colour and spectrum.";
            case tlMove: return "TAIL MOVEMENT (macro): RAW = the depth of the filter sweep, EXPERIMENTAL = every modulation depth.";
            case tlReso: return "TAIL RESONANCE (macro): RAW = the growl / mid peak resonance, EXPERIMENTAL = formant / comb / resonator.";
            case tlDensity: return "TAIL DENSITY (macro): RAW = the harmonic density (DIST 2), EXPERIMENTAL = the texture complexity (FM + noise).";
            case tlSpace: return "TAIL SPACE (macro): RAW = the subtle reverb, EXPERIMENTAL = the spatial FX spread.";
            case tlChaos: return "TAIL CHAOS (macro): RAW = distortion instability, EXPERIMENTAL = random / FM / glitch (deterministic with FIXED SEED).";
            case tlWidth: return "TAIL WIDTH (macro): RAW = the controlled stereo of the highs, EXPERIMENTAL = creative stereo (the lows always mono).";
            case exOsc: return "EXP OSCILLATOR: SINE / TRIANGLE / SAW (default) / SQUARE / CUSTOM WAVETABLE (a morph table: POSITION).";
            case exWtPos: return "EXP WAVETABLE POSITION: sine -> triangle -> saw -> square -> pulse (CUSTOM WAVETABLE).";
            case exOscLvl: return "EXP OSCILLATOR LEVEL.";
            case exFmLvl: return "EXP FM LEVEL: a REAL FM generator - a sine carrier phase-modulated by a sine modulator (0 = off).";
            case exFmAmt: return "EXP FM AMOUNT: the modulation index (more = more sidebands, metallic / bassy FM tails).";
            case exFmRatio: return "EXP FM RATIO: modulator / carrier frequency (whole = harmonic, odd ratios = inharmonic).";
            case exNoise: return "EXP NOISE: WHITE / PINK / DIGITAL (stepped, crushed).";
            case exNoiseLvl: return "EXP NOISE LEVEL.";
            case exRingRatio: return "EXP RING MOD SOURCE: the ring oscillator's ratio to the pitch (used by RING MOD slots).";
            case exPCurve: return "EXP PITCH CURVE: LINEAR DOWN / UP, EXPONENTIAL DOWN / UP, MULTI-STEP, SINE MODULATION, RANDOM, CUSTOM (START -> MID -> END).";
            case exStart: return "EXP START PITCH: st from the note where the curve starts.";
            case exEnd: return "EXP END PITCH: st from the note where it ends.";
            case exMid: return "EXP MID PITCH: the middle point of the CUSTOM curve.";
            case exRange: return "EXP PITCH RANGE: scales the whole curve (0 = flat, 2 = double).";
            case exPTime: return "EXP PITCH TIME: how long the curve takes.";
            case exPShape: return "EXP PITCH CURVE AMOUNT: how exponential / bent the curve is.";
            case exPmRate: return "EXP PITCH MOD RATE: the speed of the pitch modulation.";
            case exPmDepth: return "EXP PITCH MOD DEPTH: semitones of pitch modulation (any curve; SINE MODULATION uses it).";
            case exRoot: return "EXP ROOT: the tail note, st from the MIDI note.";
            case exLfo1: return "EXP LFO 1 RATE (a sine; also drives the phasers).";
            case exLfo2: return "EXP LFO 2 RATE (a triangle; also the FM MOD).";
            case exRndRate: return "EXP RANDOM RATE: how often RANDOM steps (CHAOS makes it faster).";
            case exCurve0: return "EXP MANUAL CURVE: the value at the start of the tail.";
            case exCurve1: return "EXP MANUAL CURVE: the value in the middle.";
            case exCurve2: return "EXP MANUAL CURVE: the value at the end.";
            case exSeed: return "EXP RANDOM SEED: FIXED SEED (the same note = the same render, always) or FREE (a new texture every hit).";
            case exFmMod: return "EXP FM MOD: LFO 2 moves the FM AMOUNT (independent of the slots).";
            case exFsMod: return "EXP FREQ SHIFT MOD: RANDOM moves every FREQ SHIFTER (independent of the slots).";
            case exSub: return "EXP SUB: the lows <-> a clean sine on the pitch (mono, never lost to phase tricks).";
            case exSubHz: return "EXP SUB FREQ: the crossover; everything under it is mono.";
            case exWidth: return "EXP WIDTH: the mid band width (user); the highs carry the stereo FX.";
            case exDrive: return "EXP OUTPUT DRIVE: the soft clip at the end.";
            case exAttack: return "EXP TAIL ATTACK.";
            case exBody: return "EXP TAIL BODY: how much of the length stays at full level.";
            case exDecay: return "EXP TAIL DECAY.";
            case exFade: return "EXP TAIL FADE: the shape of the fall.";
            case exS1Type: return "EXP SLOT 1 TYPE: the processor in slot 1 of the chain (the slot order = the processing order): OFF / FORMANT / FREQ SHIFTER / RING MOD / PITCH SHIFTER / WAVESHAPER / BITCRUSHER / PHASER / COMB / STRETCH / RESONATOR.";
            case exS1Mix: return "EXP SLOT 1 MIX: dry <-> the processor.";
            case exS1Amt: return "EXP SLOT 1 AMOUNT: the main control (vowel / shift / ring / semitones / drive / bits / depth / feedback / stretch / ring gain).";
            case exS1Par: return "EXP SLOT 1 PARAM: the characteristic control (formant shift / - / - / - / soft-fold / sample-rate / frequency / comb ratio / - / resonance ratio).";
            case exS1Mod: return "EXP SLOT 1 MOD SOURCE: ENVELOPE / LFO 1 / LFO 2 / RANDOM / KICK POSITION / MANUAL CURVE onto the AMOUNT.";
            case exS1Depth: return "EXP SLOT 1 MOD DEPTH (+ / -, scaled by MOVEMENT).";
            case exS2Type: return "EXP SLOT 2 TYPE: the processor in slot 2 of the chain (the slot order = the processing order): OFF / FORMANT / FREQ SHIFTER / RING MOD / PITCH SHIFTER / WAVESHAPER / BITCRUSHER / PHASER / COMB / STRETCH / RESONATOR.";
            case exS2Mix: return "EXP SLOT 2 MIX: dry <-> the processor.";
            case exS2Amt: return "EXP SLOT 2 AMOUNT: the main control (vowel / shift / ring / semitones / drive / bits / depth / feedback / stretch / ring gain).";
            case exS2Par: return "EXP SLOT 2 PARAM: the characteristic control (formant shift / - / - / - / soft-fold / sample-rate / frequency / comb ratio / - / resonance ratio).";
            case exS2Mod: return "EXP SLOT 2 MOD SOURCE: ENVELOPE / LFO 1 / LFO 2 / RANDOM / KICK POSITION / MANUAL CURVE onto the AMOUNT.";
            case exS2Depth: return "EXP SLOT 2 MOD DEPTH (+ / -, scaled by MOVEMENT).";
            case exS3Type: return "EXP SLOT 3 TYPE: the processor in slot 3 of the chain (the slot order = the processing order): OFF / FORMANT / FREQ SHIFTER / RING MOD / PITCH SHIFTER / WAVESHAPER / BITCRUSHER / PHASER / COMB / STRETCH / RESONATOR.";
            case exS3Mix: return "EXP SLOT 3 MIX: dry <-> the processor.";
            case exS3Amt: return "EXP SLOT 3 AMOUNT: the main control (vowel / shift / ring / semitones / drive / bits / depth / feedback / stretch / ring gain).";
            case exS3Par: return "EXP SLOT 3 PARAM: the characteristic control (formant shift / - / - / - / soft-fold / sample-rate / frequency / comb ratio / - / resonance ratio).";
            case exS3Mod: return "EXP SLOT 3 MOD SOURCE: ENVELOPE / LFO 1 / LFO 2 / RANDOM / KICK POSITION / MANUAL CURVE onto the AMOUNT.";
            case exS3Depth: return "EXP SLOT 3 MOD DEPTH (+ / -, scaled by MOVEMENT).";
            case exS4Type: return "EXP SLOT 4 TYPE: the processor in slot 4 of the chain (the slot order = the processing order): OFF / FORMANT / FREQ SHIFTER / RING MOD / PITCH SHIFTER / WAVESHAPER / BITCRUSHER / PHASER / COMB / STRETCH / RESONATOR.";
            case exS4Mix: return "EXP SLOT 4 MIX: dry <-> the processor.";
            case exS4Amt: return "EXP SLOT 4 AMOUNT: the main control (vowel / shift / ring / semitones / drive / bits / depth / feedback / stretch / ring gain).";
            case exS4Par: return "EXP SLOT 4 PARAM: the characteristic control (formant shift / - / - / - / soft-fold / sample-rate / frequency / comb ratio / - / resonance ratio).";
            case exS4Mod: return "EXP SLOT 4 MOD SOURCE: ENVELOPE / LFO 1 / LFO 2 / RANDOM / KICK POSITION / MANUAL CURVE onto the AMOUNT.";
            case exS4Depth: return "EXP SLOT 4 MOD DEPTH (+ / -, scaled by MOVEMENT).";
            case exS5Type: return "EXP SLOT 5 TYPE: the processor in slot 5 of the chain (the slot order = the processing order): OFF / FORMANT / FREQ SHIFTER / RING MOD / PITCH SHIFTER / WAVESHAPER / BITCRUSHER / PHASER / COMB / STRETCH / RESONATOR.";
            case exS5Mix: return "EXP SLOT 5 MIX: dry <-> the processor.";
            case exS5Amt: return "EXP SLOT 5 AMOUNT: the main control (vowel / shift / ring / semitones / drive / bits / depth / feedback / stretch / ring gain).";
            case exS5Par: return "EXP SLOT 5 PARAM: the characteristic control (formant shift / - / - / - / soft-fold / sample-rate / frequency / comb ratio / - / resonance ratio).";
            case exS5Mod: return "EXP SLOT 5 MOD SOURCE: ENVELOPE / LFO 1 / LFO 2 / RANDOM / KICK POSITION / MANUAL CURVE onto the AMOUNT.";
            case exS5Depth: return "EXP SLOT 5 MOD DEPTH (+ / -, scaled by MOVEMENT).";
            case exS6Type: return "EXP SLOT 6 TYPE: the processor in slot 6 of the chain (the slot order = the processing order): OFF / FORMANT / FREQ SHIFTER / RING MOD / PITCH SHIFTER / WAVESHAPER / BITCRUSHER / PHASER / COMB / STRETCH / RESONATOR.";
            case exS6Mix: return "EXP SLOT 6 MIX: dry <-> the processor.";
            case exS6Amt: return "EXP SLOT 6 AMOUNT: the main control (vowel / shift / ring / semitones / drive / bits / depth / feedback / stretch / ring gain).";
            case exS6Par: return "EXP SLOT 6 PARAM: the characteristic control (formant shift / - / - / - / soft-fold / sample-rate / frequency / comb ratio / - / resonance ratio).";
            case exS6Mod: return "EXP SLOT 6 MOD SOURCE: ENVELOPE / LFO 1 / LFO 2 / RANDOM / KICK POSITION / MANUAL CURVE onto the AMOUNT.";
            case exS6Depth: return "EXP SLOT 6 MOD DEPTH (+ / -, scaled by MOVEMENT).";
            case exS7Type: return "EXP SLOT 7 TYPE: the processor in slot 7 of the chain (the slot order = the processing order): OFF / FORMANT / FREQ SHIFTER / RING MOD / PITCH SHIFTER / WAVESHAPER / BITCRUSHER / PHASER / COMB / STRETCH / RESONATOR.";
            case exS7Mix: return "EXP SLOT 7 MIX: dry <-> the processor.";
            case exS7Amt: return "EXP SLOT 7 AMOUNT: the main control (vowel / shift / ring / semitones / drive / bits / depth / feedback / stretch / ring gain).";
            case exS7Par: return "EXP SLOT 7 PARAM: the characteristic control (formant shift / - / - / - / soft-fold / sample-rate / frequency / comb ratio / - / resonance ratio).";
            case exS7Mod: return "EXP SLOT 7 MOD SOURCE: ENVELOPE / LFO 1 / LFO 2 / RANDOM / KICK POSITION / MANUAL CURVE onto the AMOUNT.";
            case exS7Depth: return "EXP SLOT 7 MOD DEPTH (+ / -, scaled by MOVEMENT).";
            case exS8Type: return "EXP SLOT 8 TYPE: the processor in slot 8 of the chain (the slot order = the processing order): OFF / FORMANT / FREQ SHIFTER / RING MOD / PITCH SHIFTER / WAVESHAPER / BITCRUSHER / PHASER / COMB / STRETCH / RESONATOR.";
            case exS8Mix: return "EXP SLOT 8 MIX: dry <-> the processor.";
            case exS8Amt: return "EXP SLOT 8 AMOUNT: the main control (vowel / shift / ring / semitones / drive / bits / depth / feedback / stretch / ring gain).";
            case exS8Par: return "EXP SLOT 8 PARAM: the characteristic control (formant shift / - / - / - / soft-fold / sample-rate / frequency / comb ratio / - / resonance ratio).";
            case exS8Mod: return "EXP SLOT 8 MOD SOURCE: ENVELOPE / LFO 1 / LFO 2 / RANDOM / KICK POSITION / MANUAL CURVE onto the AMOUNT.";
            case exS8Depth: return "EXP SLOT 8 MOD DEPTH (+ / -, scaled by MOVEMENT).";
            case cpShape: return "CLASSIC PUNCH SHAPE: the tonal source - sine -> triangle -> a rounded saw (never a pure sine at max).";
            case cpTrans: return "CLASSIC PUNCH TRANSIENT: the punch's OWN transient - a fast knock above it (EARLY needs no global click).";
            case cpTransMs: return "CLASSIC PUNCH TRANSIENT LENGTH: how long the knock rings (a hit, never a click).";
            case cpTransHz: return "CLASSIC PUNCH TRANSIENT FREQ: the pitch of the knock (the references sit ~2-4 kHz in the first 4 ms).";
            case cpRes: return "CLASSIC PUNCH RESONANCE: a slightly resonant low-mid / mid ring excited by the source.";
            case cpResHz: return "CLASSIC PUNCH RESONANCE FREQ.";
            case cpResQ: return "CLASSIC PUNCH RESONANCE Q: wide <-> ringing.";
            case cpNoise: return "CLASSIC PUNCH NOISE: very controlled noise, only in the attack (~first 8 ms).";
            case cpNoiseHz: return "CLASSIC PUNCH NOISE FREQ: the band of the attack noise.";
            case cpStart: return "CLASSIC PUNCH START: where the pitch starts (the top of the hit).";
            case cpHit: return "CLASSIC PUNCH HIT FREQ: where the fast drop lands (~230 Hz at ~12 ms in the references).";
            case cpDrop: return "CLASSIC PUNCH DROP: the fast hit drop (START -> HIT) - short and clear, a physical hit.";
            case cpGlide: return "CLASSIC PUNCH GLIDE: the slower glide from HIT to the FUNDAMENTAL (~100 Hz at ~60 ms in the references).";
            case cpCurve: return "CLASSIC PUNCH CURVE: how exponential both drops are.";
            case cpEndSt: return "CLASSIC PUNCH END: the fundamental it settles on, in st over the played note (12 = an octave up).";
            case cpLowCut: return "CLASSIC PUNCH LOW CUT (pre-EQ).";
            case cpLmHz: return "CLASSIC PUNCH LOW-MID FREQ (pre-EQ: the meat).";
            case cpLmDb: return "CLASSIC PUNCH LOW-MID gain (pre-EQ).";
            case cpMidHz: return "CLASSIC PUNCH MID FREQ (pre-EQ: the character of the hit).";
            case cpMidDb: return "CLASSIC PUNCH MID gain (pre-EQ).";
            case cpHighCut: return "CLASSIC PUNCH HIGH CUT (post-EQ): the top only as much as the transient needs.";
            case cpSat: return "CLASSIC PUNCH SATURATION: the character saturation (it brings out the harmonics of the punch).";
            case cpAsym: return "CLASSIC PUNCH ASYMMETRY of the saturation.";
            case cpSatDecay: return "CLASSIC PUNCH SATURATION DECAY: the drive fades - harmonics on the hit, a cleaner fundamental after.";
            case cpWsType: return "CLASSIC PUNCH WAVESHAPER: SOFT / HARD (default) / ASYMMETRIC / TUBE, after the saturation.";
            case cpWsDb: return "CLASSIC PUNCH WAVESHAPER DRIVE.";
            case cpTransCtrl: return "CLASSIC PUNCH TRANSIENT CONTROL: less <-> more attack after the drive.";
            case cpAttack: return "CLASSIC PUNCH ATTACK.";
            case cpHold: return "CLASSIC PUNCH HOLD: full level before the decay.";
            case cpDecay: return "CLASSIC PUNCH DECAY: the punch hands over to the body / tail (~80-120 ms in all).";
            case cpDecayCurve: return "CLASSIC PUNCH DECAY CURVE.";
            case zgSend: return "ZAAG SEND: how much of the TOK + PUNCH goes INTO the ZAAG cascade (0 = beside it, as before; 100 % = the front melts into the zaag). The CLICK never does.";
            case zgHverb: return "ZAAG HIGH REVERB: a reverb on the top band only (above 2.5 kHz), the low end stays dry.";
            case zgThump: return "ZAAG THUMP: the hit at the start - a short driven body falling from THUMP HZ to the note.";
            case zgThumpHz: return "ZAAG THUMP: where it starts (Hz).";
            case zgThumpMs: return "ZAAG THUMP: how long it lasts (ms).";
            case zgThumpDrive: return "ZAAG THUMP: its drive (dB).";
            case pshTonalOnly: return "LAB: only the tonal layer (no CLICK, no SUB) - to design the character on its own.";
            case punchNoteMode:return "TOK note: FIXED = the tok plays PITCH on every MIDI note (e.g. always G); FOLLOW MIDI = it moves with the played note (keeping its distance to the tail note); OFFSET = the played note + NOTE OFFSET semitones.";
            case punchOverlap: return "The hand-over TOK -> BODY + TAIL: an equal-power crossfade this long (the tail starts from the tok note, in phase). ~20 ms = no audible seam, shorter = a harder cut.";
            case punchTrLevel: return "TRANSIENT: a very short attack / click at the start of the tok (inside the PUNCH - not a separate part).";
            case punchTrLength:return "How long the transient click lasts.";
            case punchTrTone:  return "The colour of the transient click (low = thump, high = tick).";
            case tailStartMode: return "AFTER TOK: body + tail start when the tok ends (minus OVERLAP). INDEPENDENT: they start at TAIL START, the tok keeps playing to its own end (a conscious overlap).";
            case tailStart:    return "INDEPENDENT: when body + tail start (ms after the hit). Earlier than the tok's end = both play together.";
            case punchTokDecay: return "How much the tok falls over its length: 0 dB = flat, -6 dB = a strong start that eases off, so the tail takes the energy at the tok's end.";
            case punchTokBend: return "The tok's own pitch envelope: it starts this many semitones away and glides to its note in GLIDE ms.";
            case punchNoteOffset: return "OFFSET mode: the tok plays the MIDI note + this many semitones (12 = an octave above the tail's note).";
            case tailRoom:     return "A small room on the TAIL before its distortion (the early-hardstyle trick: the drive eats the reverb and makes the full, gated tail). 0 = off.";
            case tailRoomDecay: return "How long the room rings (the drive after it makes it sound longer and denser).";
            case tailRoomTone: return "The room's brightness: low = a dark, warm room; high = a bright, metallic one.";
            case tailRoomInput: return "TAIL: the room only on the tail. TOK + TAIL: the tok goes through the room too, before its distortion - the drive turns the room into the crackle of the euphoric kicks (2009-12).";
            case tailRoomDelay: return "The room starts this much later (a pre-delay): 0 = with the hit; 150-300 ms = it comes in mid-beat (a bouncing, gated tail).";
            case tailColorGain: return "TAIL COLOR: an EQ AFTER the tail's distortion - it picks which harmonics stay. Low = BOOOM, ~300-600 Hz = GRRRAAANG, high = KRAAASH. 0 dB = off.";
            case tailColorFreq: return "Where the TAIL COLOR boosts (HZ mode).";
            case tailColorQ:   return "How narrow the TAIL COLOR is: high = one harmonic, a resonant / metallic ring; low = a wide colour.";
            case tailColorTrack: return "HZ: a fixed frequency. KEY TRACK: the boost follows the played note (HARMONIC x note) - the same colour on every note of a bass line.";
            case tailColorHarm: return "KEY TRACK: which harmonic of the note is boosted (e.g. 8 on a 60 Hz note = 480 Hz).";
            case peStartMode:  return "START of the tail drop: FIXED = the same pitch on every MIDI note; FOLLOW MIDI = moves with the played note. The tail still lands on the played note. (The tok itself = PUNCH in TOK mode.)";
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
        // module 12: master output
        {
            const juce::String pid (d.id);
            if (pid == "len_sync") return "LENGTH SYNC: the kick is as long as a part of the beat at the project tempo (1 BEAT at 150 BPM = 400 ms). OFF = the LENGTH knob.";
            if (pid.startsWith ("len_follow_")) return "How much this part grows / shrinks with the kick LENGTH (AUTO LENGTH on): 0 % = it keeps its own time (a punch stays short), 100 % = it follows fully.";
            if (pid == "core_note_cut") return "KICK END: LENGTH = the kick plays its whole length (the note length does not matter). NOTE = the kick stops where the note in the piano roll ends (5 ms fade, no click), the effects ring out (resonators, delays, compressor release). +FX (NOTE + FX) = the effects are cut at the note end too.";
            if (pid == "master_mute") return "MASTER MUTE: silence the output (5 ms fade - no click).";
            if (pid == "master_bypass") return "MASTER BYPASS: skip the master processing (MASTER CLIP, MASTER STEREO, MASTER OUTPUT dynamics, SAFETY) - to compare.";
            if (pid == "master_output_mode") return "OUTPUT MODE: STEREO, or MONO (L + R on both channels; a mono export writes a 1-channel file).";
            if (pid == "master_safety_clip") return "SAFETY CLIP (off by default): a soft clip that keeps the output under 0 dBFS. It changes the sound when it works - the page shows it.";
            if (pid == "master_safety_limiter") return "SAFETY LIMITER (off by default): a fast limiter at -0.2 dBFS without lookahead. The page lights up while it reduces.";
        }
        // module 11: the stereo units (one text per setting, the same for every group and the master)
        if (const juce::String pid (d.id); pid.startsWith ("spc_"))
        {
            if (pid == "spc_sub_mono_lock") return "SUB MONO LOCK: the SUB group always stays mono and centred, and the MASTER keeps everything below its crossover (at least 120 Hz) mono - stronger than any WIDTH. While it is on, the SUB group's PAN / WIDTH (LAYERS page) have no effect.";
            static const std::map<juce::String, const char*> t {
                { "on", "Switch this stereo unit on (OFF = bypass: the signal passes untouched)." },
                { "pan", "PAN: moves the position of the sound (its centre) left / right with constant power. Not the same as BALANCE." },
                { "balance", "BALANCE: only turns the opposite channel down (0 dB in the middle) - the stereo image stays, its weight moves." },
                { "width", "STEREO WIDTH: 0 % = mono, 100 % = as it is, 200 % = the widest of the chosen algorithm (the level does not jump)." },
                { "mode", "M/S WIDTH = predictable (side level); DECORRELATION = a wide side made from the highs (mono safe); HAAS = one channel later (creative, mono can suffer); DISABLED." },
                { "mix", "How much of the width algorithm is used (0 % = the original width)." },
                { "gain", "Output level of the unit." }, { "swap", "CHANNEL SWAP: left <-> right (check / creative)." },
                { "ms_on", "Switch the MID / SIDE engine on." }, { "mid", "Level of the MID (the centre)." }, { "side", "Level of the SIDE (the difference L - R)." },
                { "ms_width", "M/S WIDTH of the M/S engine: 0 % = mono, 100 % = nominal." },
                { "mid_solo", "Listen to the MID only." }, { "side_solo", "Listen to the SIDE only." },
                { "low_mono", "LOW FREQUENCY MONO: everything below the crossover goes to the centre (a stable low end)." },
                { "crossover", "Below this frequency the sound is mono." },
                { "polarity", "POLARITY INVERT: flip the left, the right or both channels." },
                { "delay", "CHANNEL DELAY: below 0 the left channel comes later, above 0 the right (precise correction / space)." },
                { "haas_time", "HAAS: how much later the chosen channel comes (1-40 ms)." }, { "haas_channel", "HAAS: which channel comes later." } };
            const juce::String key = pid.fromFirstOccurrenceOf ("_", false, false).fromFirstOccurrenceOf ("_", false, false);
            if (auto it = t.find (key); it != t.end()) return it->second;
        }
        // module 10: the sample slots (one text per setting, the same for all 8 slots)
        if (const juce::String pid (d.id); pid.startsWith ("smp") && pid.length() > 4 && juce::CharacterFunctions::isDigit (pid[3]))
        {
            static const std::map<juce::String, const char*> t {
                { "on", "Switch this sample slot on (load a file on the SAMPLES page)." },
                { "group", "The part group the sample belongs to: its level / pan / width / mute / solo and its processing (filters, EQ, distortion of that part's bus)." },
                { "level", "Level of the sample." }, { "pan", "Left / right." }, { "pitch", "Transposition in semitones (the playback speed changes with it)." },
                { "fine", "Fine tune in cents." }, { "length", "Length of the slot's envelope (ADSR / ONE SHOT envelope)." },
                { "offset", "Delay of the sample after the hit." }, { "phase", "Start a little later, as a fraction of one period of the ROOT NOTE (phase alignment with the kick)." },
                { "invert", "Flips the polarity (try it when the sample thins out the kick)." }, { "width", "Extra stereo width (the mono sum stays)." },
                { "filter_on", "Switch the slot's filter on." }, { "filter_type", "Low pass / high pass / band pass / notch." }, { "cutoff", "Filter frequency." },
                { "resonance", "Filter resonance." }, { "drive", "Saturation of the sample." },
                { "env_mode", "The slot's own amplitude envelope: ADSR, ONE SHOT or the drawn shape." }, { "attack", "Envelope attack." }, { "decay", "Envelope decay." },
                { "sustain", "Envelope sustain level (ADSR)." }, { "release", "Envelope release (ADSR)." }, { "env_delay", "The envelope starts this much later." },
                { "env_amount", "How much the envelope shapes the sample (0 = the sample plays as it is)." }, { "time_scale", "Stretches all envelope times." },
                { "mute", "Silences the slot (3 ms fade)." }, { "solo", "Plays only the soloed layers / samples." },
                { "route_mode", "GROUP = into its group's bus, MULTI = sends into several buses." }, { "send_pre", "MULTI: the sends take the sample before (PRE) or after its LEVEL." },
                { "send_punch", "MULTI: send into the PUNCH bus." }, { "send_body", "MULTI: send into the BODY bus." }, { "send_tail", "MULTI: send into the TAIL bus." },
                { "send_sub", "MULTI: send into the SUB bus." }, { "send_texture", "MULTI: send into the TEXTURE bus." }, { "send_custom", "MULTI: send into the CUSTOM bus." },
                { "send_master", "MULTI: send straight to the master." },
                { "link_source", "Follow another slot (its level / pitch / length / envelope as chosen)." }, { "link_level", "LINK: the level moves with the linked slot." },
                { "link_pitch", "LINK: the pitch moves with the linked slot." }, { "link_length", "LINK: the length moves with the linked slot." }, { "link_env", "LINK: use the linked slot's envelope." },
                { "start", "Where the playback starts (0 = the beginning of the file). A start inside the audio always gets a tiny fade - no click." },
                { "end", "Where the playback ends." }, { "fade_in", "Fade in at the start of the region." }, { "fade_out", "Fade out before END (ONE SHOT) or after the note (GATE)." },
                { "rate", "Playback speed (x2 = an octave up and half as long)." }, { "reverse", "Plays the region backwards (the file stays as it is)." },
                { "play_mode", "ONE SHOT = START to END once, GATE = while the note is held, LOOP = repeats LOOP START..END, PING-PONG = back and forth." },
                { "loop_start", "Loop start inside the region." }, { "loop_end", "Loop end inside the region." },
                { "loop_xfade", "Crossfade at the loop point (hides the seam)." }, { "root", "The note the sample was recorded at (KEY TRACKING plays it at the MIDI note)." },
                { "tracking", "ON: the sample follows the played note (relative to ROOT NOTE). OFF: always the same pitch." } };
            if (auto it = t.find (pid.fromFirstOccurrenceOf ("_", false, false)); it != t.end()) return it->second;
        }
        // module 09: modulation sources
        {
            const juce::String pid (d.id);
            static const std::map<juce::String, const char*> macro {
                { "macro_impact", "IMPACT - strength and character of the hit (default: punch level / drive, transient)." },
                { "macro_body", "BODY - mass of the body (level, decay, drive)." }, { "macro_tail", "TAIL - length of the tail (length, decay, release)." },
                { "macro_pitch", "PITCH - the pitch drop (start, time, curve)." }, { "macro_grit", "GRIT - dirt and aggression (tail / body drive, stage 2, distortion 1)." },
                { "macro_scream", "SCREAM - resonance and screech (tail resonance, screech level, mid bell)." },
                { "macro_weight", "WEIGHT - low-end energy (sub level, drive, length)." }, { "macro_chaos", "CHAOS - controlled instability (unison drift / detune, pulse width)." } };
            if (auto it = macro.find (pid); it != macro.end()) return juce::String (it->second) + " What it moves = the MACRO MAPPING (MOD page); it never changes the knobs' own values.";
            if (pid.startsWith ("macro_user")) return "USER MACRO - your own controller: connect it to anything on the MOD page.";
            static const std::map<juce::String, const char*> lfo {
                { "wave", "Shape of the LFO." }, { "rate", "Speed in Hz (free)." }, { "sync", "HOST SYNC: the speed follows the tempo (DIVISION)." },
                { "division", "Length of one cycle in notes (with HOST SYNC)." }, { "phase", "Start point of the cycle." },
                { "retrigger", "OFF = runs free (synced: follows the song), ON NOTE = restarts on the first of held notes, ON KICK TRIGGER = every kick, ON BAR = every bar." },
                { "mode", "LOOP, or ONE SHOT (one cycle, then hold the end / go back to zero)." }, { "polarity", "BIPOLAR -1..+1 or UNIPOLAR 0..1." },
                { "fade_in", "The LFO fades in after a restart." }, { "fade_out", "The LFO fades out after the note is released." },
                { "smooth", "Smooths the LFO output (against steps)." }, { "pw", "Pulse width (SQUARE / PULSE)." } };
            if (pid.startsWith ("lfo")) if (auto it = lfo.find (pid.fromFirstOccurrenceOf ("_", false, false)); it != lfo.end()) return it->second;
            static const std::map<juce::String, const char*> seq {
                { "seq_length", "Number of steps (1-64; the steps beyond stay saved)." }, { "seq_sync", "HOST SYNC: steps follow the tempo (DIVISION)." },
                { "seq_rate", "Steps per second (free)." }, { "seq_division", "Length of one step in notes." },
                { "seq_direction", "FORWARD, BACKWARD, PING-PONG, RANDOM (seeded) or ALTERNATE (odd steps, then even)." },
                { "seq_reset", "When it starts again from step 1: OFF, ON NOTE, ON KICK TRIGGER, ON BAR." }, { "seq_swing", "Every second step comes later." },
                { "seq_humanize", "Small fixed timing shifts per step (seeded - the same every time)." },
                { "seq_skip", "A step that did not pass its PROBABILITY: ZERO, HOLD the last value, or SKIP to the next step." },
                { "seq_polarity", "BIPOLAR -1..+1 or UNIPOLAR 0..1." }, { "seq_smooth", "Smooths the jumps between steps." },
                { "seq_accent", "Extra amount on ACCENT steps." }, { "seq_seed", "Seed of probability / random direction / humanize: the same seed = the same pattern." } };
            if (auto it = seq.find (pid); it != seq.end()) return it->second;
            static const std::map<juce::String, const char*> rnd {
                { "mode", "FIXED SEED = one value, PER TRIGGER = new every kick, FREE RUN = changes at RATE, ONE SHOT = one per playback, CONTROLLED = small steps (MAX STEP) per kick." },
                { "seed", "The same seed = the same random values (in the plugin and in the export)." }, { "rate", "FREE RUN speed." },
                { "smooth", "Glide between random values." }, { "amount", "How much of the random value goes out." }, { "deviation", "CONTROLLED RANDOM: the largest step per kick." },
                { "polarity", "BIPOLAR -1..+1 or UNIPOLAR 0..1." } };
            if (pid.startsWith ("rnd")) if (auto it = rnd.find (pid.fromFirstOccurrenceOf ("_", false, false)); it != rnd.end()) return it->second;
            if (pid.startsWith ("cc") && pid.endsWith ("_number")) return "MIDI CC number of this source (LEARN on the MOD page: move a knob on your controller).";
            if (pid.startsWith ("cc") && pid.endsWith ("_smooth")) return "Smooths the controller steps (CC has only 128 steps).";
            static const std::map<juce::String, const char*> fol {
                { "fol_source", "Which part the follower listens to (before the effects)." }, { "fol_detect", "PEAK = fast, RMS = average, HYBRID = both." },
                { "fol_attack", "How fast it rises." }, { "fol_release", "How fast it falls." }, { "fol_gain", "Input level of the follower." },
                { "fol_threshold", "Level where the follower starts (0 below)." }, { "fol_range", "dB above the threshold that give the full value." },
                { "fol_sensitivity", "GATE: smaller values become 0 (stable in silence)." }, { "fol_smooth", "Smooths the follower output." } };
            if (auto it = fol.find (pid); it != fol.end()) return it->second;
            if (pid.endsWith ("_interp")) return "CUSTOM wave: LINEAR lines or a CUBIC curve through the drawn points.";
            if (pid == "macro_morph_on") return "MACRO MORPH: the 12 macros follow the MORPH knob between the stored sets A and B.";
            if (pid == "macro_morph") return "MACRO MORPH: 0 = set A, 100 % = set B, in between a smooth blend.";
            if (pid == "mod_movement") return "MOVEMENT - scales every LFO / sequencer / random connection at once (0 = still, 200 % = double).";
        }
        // module 08: dynamics (one text per setting)
        {
            const juce::String pid (d.id);
            auto num = [&] (const char* pre) { const juce::String p (pre); return pid.startsWith (p) && pid.length() > p.length() && juce::CharacterFunctions::isDigit (pid[p.length()]); };
            static const std::map<juce::String, const char*> cmp {
                { "on", "Switch this compressor on." }, { "location", "Where it works: the whole kick, a part (group), the texture, a layer, the screech or the MASTER OUTPUT (the only place with lookahead)." },
                { "position", "Before or after that place's distortion (module 05)." },
                { "type", "VCA clean, FET fast and coloured, OPTO smooth, VARI-MU gentle (the ratio grows with the level), DIGITAL exact, UPWARD lifts the quiet parts, PARALLEL blends dry + compressed, SIDECHAIN ducks from another part." },
                { "detection", "What the detector follows: every peak, the average (RMS), both, or the true peak between the samples." },
                { "threshold", "Level where the compression starts." }, { "ratio", "How strongly it compresses past the threshold (4 = 4 dB in -> 1 dB out)." },
                { "attack", "How fast it grabs: slow = the punch passes through, fast = the punch is flattened." }, { "release", "How fast it lets go: short = pumping / more tail, long = smooth." },
                { "auto_release", "MANUAL = the RELEASE knob; FAST / MEDIUM / SLOW presets; PROGRAM DEPENDENT = longer release after heavy compression." },
                { "knee", "Soft start around the threshold (0 = hard)." }, { "input", "Gain into the compressor." }, { "output", "Gain after it (makeup)." }, { "mix", "Dry / compressed blend." },
                { "dry_level", "PARALLEL: level of the dry signal." }, { "wet_level", "PARALLEL: level of the compressed signal." },
                { "lookahead", "MASTER OUTPUT only: the audio waits so the compressor catches the very first peak (adds latency, reported to FL)." },
                { "stereo_link", "100 % = both channels get the same gain (no image shift), 0 % = each channel on its own." },
                { "sc_source", "What the detector listens to: the signal itself or another part (PUNCH ducks the TAIL...). Parts / groups only." },
                { "sc_hpf", "High-pass on the detector: the sub does not pump the compressor." }, { "sc_lpf", "Low-pass on the detector." },
                { "sc_gain", "Detector level (more = more compression)." }, { "sc_mix", "Blend of the own signal / sidechain source in the detector." } };
            static const std::map<juce::String, const char*> ts {
                { "on", "Switch this transient shaper on." }, { "location", "Where it works (by default one per part)." }, { "position", "Before or after that place's distortion." },
                { "detection", "How the transient is found: fast, slow, RMS, peak or hybrid." },
                { "mode", "PUNCH = the onset, SUSTAIN = what follows it, BODY = the first 100 ms, TAIL = the end of the kick." },
                { "attack", "More (+) or less (-) of the hit." }, { "sustain", "More (+) or less (-) of what comes after the hit." },
                { "sensitivity", "How small a transient still counts." }, { "time", "How long the attack part lasts." }, { "release", "How fast the shaper returns." },
                { "mix", "Dry / shaped blend." }, { "output", "Gain after it." } };
            static const std::map<juce::String, const char*> clp {
                { "on", "Switch this clipper on." }, { "location", "Where it clips." }, { "position", "Before or after that place's distortion." },
                { "type", "SOFT, HARD, ASYMMETRIC, SINE, CUBIC, TANH or your own drawn CUSTOM CURVE." },
                { "input", "Gain into the clipper." }, { "ceiling", "The level it never passes." }, { "drive", "How hard it is pushed into the ceiling." },
                { "symmetry", "Clip the top and the bottom differently (even harmonics)." }, { "softness", "Rounder corner at the ceiling." }, { "mix", "Dry / clipped blend." },
                { "output", "Gain after it." }, { "oversampling", "Higher = less aliasing (fizz) from hard clipping, more CPU." } };
            static const std::map<juce::String, const char*> lim {
                { "on", "Switch this limiter on." }, { "location", "Where it limits (MASTER OUTPUT = the last word, with lookahead)." }, { "position", "Before or after that place's distortion." },
                { "mode", "TRANSPARENT clean, AGGRESSIVE fast, PUNCH lets the hit through first, LOUD dense, TRUE PEAK safe between the samples." },
                { "ceiling", "The level the output never passes." }, { "input", "Gain into the limiter (louder)." }, { "release", "How fast it lets go." },
                { "lookahead", "MASTER OUTPUT only: catches the peak before it arrives (latency reported to FL)." }, { "attack", "0 = brickwall; more = the hit gets through softer." },
                { "knee", "Soft start under the ceiling." }, { "stereo_link", "100 % = the same gain on both channels." }, { "true_peak", "Also stops the peaks between the samples (safe for MP3 / streaming)." },
                { "oversampling", "How finely the true peak is checked." }, { "mix", "Dry / limited blend." } };
            static const std::map<juce::String, const char*> other {
                { "crest_on", "Crest factor control: pushes the peak-to-average ratio of the kick toward the TARGET." }, { "crest_location", "Where it works." },
                { "crest_position", "Before or after that place's distortion." }, { "crest_target", "Wanted peak / RMS in dB: low = dense, loud; high = spiky, punchy." },
                { "crest_punch", "Less (-) or more (+) peak in the punch." }, { "crest_body", "Less (-) or more (+) density in the body." }, { "crest_tail", "Less (-) or more (+) density in the tail." },
                { "crest_sensitivity", "How fast and how strongly it moves the kick toward the target." }, { "crest_mix", "Dry / processed blend." },
                { "dyn_auto_gain", "Every dynamics processor keeps its output level (measured over 50 ms, not per sample): hear the shape, not louder / quieter." },
                { "dyn_level_match", "Hear the UNPROCESSED signal at the processed level - an honest A / B." },
                { "dyn_order", "The order of compressor, transient shaper, clipper at one place (the limiter and crest always last)." },
                { "mb_link", "Multiband: linked bands share one detector - no tonal shift between them." } };
            const juce::String k = pid.fromFirstOccurrenceOf ("_", false, false);
            if (num ("cmp")) if (auto it = cmp.find (k); it != cmp.end()) return it->second;
            if (num ("ts")) if (auto it = ts.find (k); it != ts.end()) return it->second;
            if (num ("clp")) if (auto it = clp.find (k); it != clp.end()) return it->second;
            if (num ("lim")) if (auto it = lim.find (k); it != lim.end()) return it->second;
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
