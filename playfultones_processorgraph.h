/** BEGIN_JUCE_MODULE_DECLARATION
ID:               playfultones_processorgraph
vendor:           Playful Tones ApS
version:          1.0.0
name:             playfultones_processorgraph
description:      A handy module for creating audio processor graphs with built-in/statically linked DSP processors.
website:          https://github.com/playfultones
license:          GPL-3.0
dependencies:     juce_audio_processors, juce_gui_basics, juce_audio_utils, juce_gui_extra
END_JUCE_MODULE_DECLARATION
*/
#pragma once
#define PLAYFULTONES_PROCESSORGRAPH_H_INCLUDED

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "source/ModuleFactory.h"
#include "source/PluginWindow.h"
#include "source/PluginGraph.h"
#include "source/GraphEditor.h"
