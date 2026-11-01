// RE4GE - reading the SAMPLE source's file (message thread only; never the audio thread)
#include "SampleFile.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace re4ge
{
    // a WAV / AIFF / FLAC into a mono buffer (max 4 s)
    std::unique_ptr<re::Sample> readSampleFile (const juce::File& f)
    {
        juce::AudioFormatManager afm; afm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (afm.createReaderFor (f));
        if (r == nullptr) return nullptr;
        const int len = (int) std::min<long long> ((long long) r->lengthInSamples, (long long) (r->sampleRate * 4.0));
        const int nch = r->numChannels > 0 ? (int) r->numChannels : 1;
        juce::AudioBuffer<float> b (nch, len);
        r->read (&b, 0, len, 0, true, true);
        auto s = std::make_unique<re::Sample>();
        s->rate = r->sampleRate; s->name = f.getFileNameWithoutExtension();
        s->data.resize ((size_t) len);
        for (int i = 0; i < len; ++i)
        {
            float v = 0.f;
            for (int c = 0; c < b.getNumChannels(); ++c) v += b.getSample (c, i);
            s->data[(size_t) i] = v / (float) b.getNumChannels();
        }
        return s;
    }
}
