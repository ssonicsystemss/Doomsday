#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ParamIDs.h"
#include "../DSP/ReParams.h"

namespace re4ge { re::Params gatherParams (const juce::AudioProcessorValueTreeState&); }
