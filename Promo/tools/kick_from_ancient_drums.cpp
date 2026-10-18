// Builds against the Ancient Drums sources (github.com/pepula-GSB/Ancient-Drums):
// renders Promo/assets/kick.wav from the "Monolith Rawstyle" preset, then convert to FLAC.
#include "PluginProcessor.h"   // Ancient Drums Source/
#include <juce_audio_formats/juce_audio_formats.h>
#include "song.h"
using namespace juce;

int main (int, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    AncientProcessor p;
    p.setPlayConfigDetails (0, 2, SR, 512);
    for (int i = 0; i < p.getNumPrograms(); ++i) if (p.getProgramName (i) == "Monolith Rawstyle") p.setCurrentProgram (i);
    p.prepareToPlay (SR, 512);

    AudioBuffer<float> out (2, TOTAL); out.clear();
    for (int pos = 0; pos < TOTAL; pos += 512)
    {
        const int n = jmin (512, TOTAL - pos);
        AudioBuffer<float> buf (2, n); buf.clear(); MidiBuffer m;
        for (int b = pos / BEAT; b <= (pos + n) / BEAT; ++b)
        {
            const int at = b * BEAT;
            if (kickAtBeat (b) && at >= pos && at < pos + n) m.addEvent (MidiMessage::noteOn (1, 62, (uint8) 120), at - pos);  // D
        }
        p.processBlock (buf, m);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, buf, ch, 0, n);
    }
    File f (argv[1]); f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), SR, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
    printf ("kick peak %.3f\n", out.getMagnitude (0, out.getNumSamples()));
    return 0;
}
