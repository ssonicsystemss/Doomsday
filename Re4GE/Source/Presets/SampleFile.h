#pragma once
#include <juce_core/juce_core.h>
#include "../DSP/Oscillator.h"
#include <memory>

namespace re4ge { std::unique_ptr<re::Sample> readSampleFile (const juce::File&); }
