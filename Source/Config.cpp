#include "Config.h"

namespace {
juce::String colourFields(juce::Colour colour) {
  return "[" + juce::String(colour.getRed()) + "][" +
         juce::String(colour.getGreen()) + "][" +
         juce::String(colour.getBlue()) + "][" +
         juce::String(colour.getAlpha()) + "]";
}

juce::String describe(const PluginConfig &config) {
  juce::StringArray lines;
  lines.add("[" + juce::String(config.ccThinSamples) +
            "] thin CC's that are LESS than this many samples apart (1 = max "
            "MIDI data)");
  lines.add("[" + config.defaultPreset +
            "] default preset, relative to the Presets folder e.g. "
            "Factory/Sidechain 1-4.lfopreset (empty = init)");
  lines.add("[" + juce::String((int)config.scopeByDefault) +
            "] Scope enabled by default (0 or 1)");
  lines.add("[" + juce::String(config.redrawMs) +
            "] update rate in milliseconds for graphic redraw (4 to 200)");
  lines.add("[" + juce::String((int)config.graphNotesHavePriority) +
            "] MIDI notes 0-59 (to change graph assignments) have priority "
            "over retrig, etc.");
  lines.add("[" + juce::String(config.defaultWidth) +
            "] default plug-in width in pixels (708 is 1x, the height "
            "follows)");
  lines.add(colourFields(config.scopeColour) +
            " RGBA for scope waveform display");
  lines.add(colourFields(config.bypassColour) + " RGBA for 'bypass' display");
  return lines.joinIntoString("\n") + "\n";
}

// The bracketed values of one line, in order.
juce::StringArray fields(const juce::String &line) {
  juce::StringArray values;
  for (auto rest = line; rest.contains("[") && rest.contains("]");
       rest = rest.fromFirstOccurrenceOf("]", false, false))
    values.add(rest.fromFirstOccurrenceOf("[", false, false)
                   .upToFirstOccurrenceOf("]", false, false)
                   .trim());
  return values;
}

juce::Colour toColour(const juce::StringArray &values, juce::Colour fallback) {
  if (values.size() < 4)
    return fallback;

  auto channel = [&values](int index) {
    return (juce::uint8)juce::jlimit(0, 255, values[index].getIntValue());
  };
  return {channel(0), channel(1), channel(2), channel(3)};
}

// The plugin used to be called "LFO Tool": carry its presets, shapes and
// settings over the first time the renamed plugin runs.
void migrateLegacyData() {
  const auto folder = PluginConfig::getDataFolder();
  const auto legacy = folder.getSiblingFile("LFO Tool");
  if (folder.exists() || !legacy.isDirectory())
    return;

  if (legacy.copyDirectoryTo(folder))
    folder.getChildFile("LFOToolConfig.txt")
        .moveFileTo(PluginConfig::getFile());
}

PluginConfig load() {
  migrateLegacyData();

  PluginConfig config;
  const auto file = PluginConfig::getFile();

  if (!file.existsAsFile()) {
    file.getParentDirectory().createDirectory();
    file.replaceWithText(describe(config));
    return config;
  }

  // Settings are identified by position, as in the file we write.
  juce::Array<juce::StringArray> settings;
  for (const auto &line : juce::StringArray::fromLines(file.loadFileAsString()))
    if (const auto values = fields(line); !values.isEmpty())
      settings.add(values);

  if (settings.size() < 8)
    return config;

  config.ccThinSamples = juce::jmax(1, settings[0][0].getIntValue());
  config.defaultPreset = settings[1][0];
  config.scopeByDefault = settings[2][0].getIntValue() != 0;
  config.redrawMs = juce::jlimit(4, 200, settings[3][0].getIntValue());
  config.graphNotesHavePriority = settings[4][0].getIntValue() != 0;
  config.defaultWidth = juce::jlimit(400, 4000, settings[5][0].getIntValue());
  config.scopeColour = toColour(settings[6], config.scopeColour);
  config.bypassColour = toColour(settings[7], config.bypassColour);
  return config;
}
} // namespace

//==============================================================================
const PluginConfig &PluginConfig::get() {
  static const PluginConfig config = load();
  return config;
}

juce::File PluginConfig::getDataFolder() {
  // On macOS JUCE's "application data" folder is ~/Library itself.
  return juce::File::getSpecialLocation(
             juce::File::userApplicationDataDirectory)
#if JUCE_MAC
      .getChildFile("Application Support")
#endif
      .getChildFile("JezzterInc/OpenLFO");
}

juce::File PluginConfig::getFile() {
  return getDataFolder().getChildFile("OpenLFOConfig.txt");
}
