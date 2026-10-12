#pragma once

#include <juce_graphics/juce_graphics.h>

//==============================================================================
// Global settings that have no control in the interface. They live in a
// hand-editable "OpenLFOConfig.txt" next to the presets, one setting per line
// with the values in [brackets], and are read once when the plugin loads.
struct PluginConfig {
  int ccThinSamples = 1;      // minimum gap between outgoing CCs
  juce::String defaultPreset; // relative to the Presets folder, empty = init
  bool scopeByDefault = false;
  int redrawMs = 16;
  bool graphNotesHavePriority = false; // notes 0-59 skip retrig, gate, etc.
  int defaultWidth = 920;
  juce::Colour scopeColour{(juce::uint8)229, (juce::uint8)72, (juce::uint8)77,
                           (juce::uint8)153};
  juce::Colour bypassColour{(juce::uint8)139, (juce::uint8)139,
                            (juce::uint8)139, (juce::uint8)115};

  static const PluginConfig &get();

  // Where presets, shapes and the config file are kept.
  static juce::File getDataFolder();
  static juce::File getFile();
};
