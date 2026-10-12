#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
constexpr double lfoWrap = 1024.0; // cycles before the free-running LFO wraps
constexpr float minFreq = 20.0f;
constexpr float maxFreq = 20000.0f;
const float logFreqRange = std::log(maxFreq / minFreq);
constexpr float minRateHz = 0.02f;
constexpr float maxRateHz = 40.0f;

constexpr int controlInterval = 4; // samples between filter / CC updates

const juce::Identifier shapesTag{"Shapes"};
const juce::Identifier shapeTag{"Shape"};
const juce::Identifier pointsProp{"points"};
const juce::Identifier multProp{"mult"};
const juce::Identifier presetPathProp{"presetPath"};
const juce::Identifier editorScaleProp{"editorScale"};

const juce::String presetExtension{".lfopreset"};

juce::NormalisableRange<float> logRange(float low, float high) {
  const auto ratio = std::log(high / low);
  return {low, high,
          [=](float, float, float n) { return low * std::exp(n * ratio); },
          [=](float, float, float v) { return std::log(v / low) / ratio; }};
}

juce::String wholeNumber(float value, int) {
  return juce::String(juce::roundToInt(value));
}

std::unique_ptr<juce::AudioParameterFloat> percent(const char *id,
                                                   const char *name, float low,
                                                   float defaultValue) {
  return std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{id, 1}, name,
      juce::NormalisableRange<float>(low, 100.0f), defaultValue,
      juce::AudioParameterFloatAttributes().withStringFromValueFunction(
          wholeNumber));
}

std::unique_ptr<juce::AudioParameterBool> toggle(const char *id,
                                                 const char *name,
                                                 bool defaultValue) {
  return std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id, 1},
                                                    name, defaultValue);
}
} // namespace

//==============================================================================
juce::String AudioPluginAudioProcessor::destName(int dest) {
  static const char *const names[numDests] = {"cut", "res", "vol", "pan",
                                              "var"};
  return names[dest];
}

juce::String AudioPluginAudioProcessor::pointParamID(int point, int field) {
  static const char *const fields[] = {"x", "y", "c"};
  return "pt" + juce::String(point + 1) + fields[field];
}

const std::vector<AudioPluginAudioProcessor::RateEntry> &
AudioPluginAudioProcessor::rateEntries() {
  static const std::vector<RateEntry> entries = [] {
    const std::pair<const char *, double> lengths[] = {
        {"8 bar", 32.0}, {"4 bar", 16.0}, {"2 bar", 8.0}, {"1 bar", 4.0},
        {"1/2", 2.0},    {"1/4", 1.0},    {"1/8", 0.5},   {"1/16", 0.25},
        {"1/32", 0.125}, {"1/64", 0.0625}};

    std::vector<RateEntry> list;
    for (const auto &[name, beats] : lengths) {
      list.push_back({juce::String(name) + ".", beats * 1.5, RateEntry::dotted});
      list.push_back({name, beats, RateEntry::plain});
      list.push_back({juce::String(name) + "T", beats * 2.0 / 3.0,
                      RateEntry::triplet});
    }

    std::stable_sort(list.begin(), list.end(),
                     [](const RateEntry &x, const RateEntry &y) {
                       return x.beats > y.beats;
                     });
    return list;
  }();

  return entries;
}

const std::array<float, 7> &AudioPluginAudioProcessor::rateMultipliers() {
  static const std::array<float, 7> values{0.25f, 0.5f, 1.0f, 2.0f,
                                           3.0f,  4.0f, 8.0f};
  return values;
}

juce::String AudioPluginAudioProcessor::multiplierText(float multiplier) {
  if (multiplier < 0.3f)
    return "1/4x";
  if (multiplier < 0.75f)
    return "1/2x";
  return juce::String(juce::roundToInt(multiplier)) + "x";
}

juce::AudioProcessorValueTreeState::ParameterLayout
AudioPluginAudioProcessor::createLayout() {
  using namespace juce;
  AudioProcessorValueTreeState::ParameterLayout layout;

  StringArray graphNames;
  for (int i = 1; i <= numGraphs; ++i)
    graphNames.add(String(i));

  // Filter
  layout.add(toggle(ParamID::filterOn, "Filter On", false));
  layout.add(std::make_unique<AudioParameterChoice>(
      ParameterID{ParamID::filterType, 1}, "Filter Type",
      FilterEngine::typeNames(), 0));
  layout.add(std::make_unique<AudioParameterFloat>(
      ParameterID{ParamID::cutoff, 1}, "Cutoff", logRange(minFreq, maxFreq),
      632.0f,
      AudioParameterFloatAttributes().withStringFromValueFunction(
          [](float v, int) { return String(roundToInt(v)) + " Hz"; })));
  layout.add(percent(ParamID::reso, "Reso", 0.0f, 0.0f));
  layout.add(percent(ParamID::mix, "Mix", 0.0f, 100.0f));
  layout.add(percent(ParamID::var, "Var", 0.0f, 50.0f));
  layout.add(percent(ParamID::drive, "Drive", 0.0f, 0.0f));
  layout.add(std::make_unique<AudioParameterFloat>(
      ParameterID{ParamID::offset, 1}, "Offset",
      NormalisableRange<float>(-4096.0f, 4096.0f, 1.0f), 0.0f,
      AudioParameterFloatAttributes().withStringFromValueFunction(
          wholeNumber)));
  layout.add(std::make_unique<AudioParameterChoice>(
      ParameterID{ParamID::display, 1}, "Display",
      StringArray{"Graph", "Scope", "Filter"}, 0));

  // MIDI
  layout.add(std::make_unique<AudioParameterChoice>(
      ParameterID{ParamID::retrig, 1}, "Note Retrig",
      StringArray{"Off", "Retrig", "Envelope"}, 0));
  layout.add(toggle(ParamID::gate, "Note Gate", false));
  layout.add(toggle(ParamID::velPwm, "Vel->PWM", false));
  layout.add(toggle(ParamID::noteRate, "Note->Rate", false));
  layout.add(toggle(ParamID::noteCutoff, "Note->Cutoff", false));
  layout.add(std::make_unique<AudioParameterInt>(
      ParameterID{ParamID::ccOut, 1}, "CC Out", -1, 127, -1,
      AudioParameterIntAttributes().withStringFromValueFunction(
          [](int v, int) { return v < 0 ? String("OFF") : String(v); })));
  layout.add(toggle(ParamID::midiLock, "CC Lock", false));

  // LFO
  layout.add(toggle(ParamID::anchor, "Anchor", true));
  layout.add(toggle(ParamID::sync, "BPM Sync", true));
  layout.add(toggle(ParamID::triplet, "Triplet Rates", true));
  layout.add(toggle(ParamID::dotted, "Dotted Rates", true));

  StringArray rateNames;
  for (const auto &entry : rateEntries())
    rateNames.add(entry.name);
  layout.add(std::make_unique<AudioParameterChoice>(
      ParameterID{ParamID::rateSync, 1}, "Rate", rateNames,
      rateNames.indexOf("1/4")));
  layout.add(std::make_unique<AudioParameterFloat>(
      ParameterID{ParamID::rateHz, 1}, "Rate Hz", logRange(minRateHz, maxRateHz), 2.0f,
      AudioParameterFloatAttributes().withStringFromValueFunction(
          [](float v, int) { return String(v, 2) + " Hz"; })));
  layout.add(percent(ParamID::swing, "Swing", -100.0f, 0.0f));
  layout.add(percent(ParamID::phase, "Phase", 0.0f, 0.0f));
  layout.add(percent(ParamID::pwm, "PWM", -100.0f, 0.0f));
  layout.add(percent(ParamID::smooth, "Smooth", 0.0f, 0.0f));
  layout.add(std::make_unique<AudioParameterInt>(
      ParameterID{ParamID::snap, 1}, "Snap", 0, 32, 8,
      AudioParameterIntAttributes().withAutomatable(false)));

  // Routing
  for (int d = 0; d < numDests; ++d) {
    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID{destGraphID(d), 1}, destName(d) + " graph", graphNames,
        0));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{destAmountID(d), 1}, destName(d) + " amount",
        NormalisableRange<float>(-100.0f, 100.0f),
        d == destVol ? 100.0f : 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction(
            wholeNumber)));
  }
  layout.add(percent(ParamID::depth, "Depth", 0.0f, 100.0f));

  // The first points of graph 1, for host automation
  const auto firstShape = LfoShape().points;
  for (int i = 0; i < numAutomatedPoints; ++i) {
    const auto point = (size_t)i < firstShape.size() ? firstShape[(size_t)i]
                                                     : LfoPoint{};
    const auto name = "Point " + String(i + 1);
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{pointParamID(i, 0), 1}, name + " X",
        NormalisableRange<float>(0.0f, 1.0f), point.x));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{pointParamID(i, 1), 1}, name + " Y",
        NormalisableRange<float>(0.0f, 1.0f), point.y));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{pointParamID(i, 2), 1}, name + " Curve",
        NormalisableRange<float>(-1.0f, 1.0f), point.curve));
  }

  // Split
  layout.add(toggle(ParamID::splitOn, "Split On", false));
  layout.add(toggle(ParamID::splitHigh, "Split High Band", false));
  layout.add(std::make_unique<AudioParameterFloat>(
      ParameterID{ParamID::splitFreq, 1}, "Split Freq",
      logRange(minFreq, maxFreq), 632.0f,
      AudioParameterFloatAttributes().withStringFromValueFunction(
          wholeNumber)));

  return layout;
}

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      // The state tag keeps the name the plugin first shipped under, so presets
      // and sessions saved before the rename still load.
      apvts(*this, nullptr, "LFOTool", createLayout()) {
  auto raw = [this](const juce::String &id) {
    auto *value = apvts.getRawParameterValue(id);
    jassert(value != nullptr);
    return value;
  };

  params.filterOn = raw(ParamID::filterOn);
  params.filterType = raw(ParamID::filterType);
  params.cutoff = raw(ParamID::cutoff);
  params.reso = raw(ParamID::reso);
  params.mix = raw(ParamID::mix);
  params.var = raw(ParamID::var);
  params.drive = raw(ParamID::drive);
  params.offset = raw(ParamID::offset);
  params.retrig = raw(ParamID::retrig);
  params.gate = raw(ParamID::gate);
  params.velPwm = raw(ParamID::velPwm);
  params.noteRate = raw(ParamID::noteRate);
  params.noteCutoff = raw(ParamID::noteCutoff);
  params.ccOut = raw(ParamID::ccOut);
  params.anchor = raw(ParamID::anchor);
  params.sync = raw(ParamID::sync);
  params.rateSync = raw(ParamID::rateSync);
  params.rateHz = raw(ParamID::rateHz);
  params.swing = raw(ParamID::swing);
  params.phase = raw(ParamID::phase);
  params.pwm = raw(ParamID::pwm);
  params.smooth = raw(ParamID::smooth);
  params.depth = raw(ParamID::depth);
  params.splitOn = raw(ParamID::splitOn);
  params.splitHigh = raw(ParamID::splitHigh);
  params.splitFreq = raw(ParamID::splitFreq);

  for (int d = 0; d < numDests; ++d) {
    params.graph[(size_t)d] = raw(destGraphID(d));
    params.amount[(size_t)d] = raw(destAmountID(d));
  }

  for (int g = 0; g < numGraphs; ++g) {
    multipliers[(size_t)g].store(1.0f);
    renderTable(g);
  }

  for (auto &graph : noteGraph)
    graph.store(-1);

  for (int i = 0; i < numAutomatedPoints; ++i)
    for (int field = 0; field < 3; ++field) {
      const auto id = pointParamID(i, field);
      pointParams[(size_t)i][(size_t)field] = apvts.getParameter(id);
      apvts.addParameterListener(id, this);
    }

  // Settings from the config file
  const auto &config = PluginConfig::get();
  editorScale = (float)config.defaultWidth / 708.0f;
  if (config.scopeByDefault)
    if (auto *display = apvts.getParameter(ParamID::display))
      display->setValueNotifyingHost(
          display->convertTo0to1((float)displayScope));
  if (config.defaultPreset.isNotEmpty())
    loadPreset(getPresetFolder().getChildFile(config.defaultPreset));

  startTimerHz(60);
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor() {
  stopTimer();
  for (int i = 0; i < numAutomatedPoints; ++i)
    for (int field = 0; field < 3; ++field)
      apvts.removeParameterListener(pointParamID(i, field), this);
}

void AudioPluginAudioProcessor::timerCallback() {
  // Graph changes played from MIDI notes land in the routing parameters here,
  // off the audio thread.
  for (int d = 0; d < numDests; ++d) {
    auto picked = noteGraph[(size_t)d].load();
    if (picked < 0)
      continue;

    if (auto *parameter = apvts.getParameter(destGraphID(d)))
      parameter->setValueNotifyingHost(
          parameter->convertTo0to1((float)picked));
    noteGraph[(size_t)d].compare_exchange_strong(picked, -1);
  }

  if (pointsDirty.exchange(false))
    applyPointParamsToShape();
}

// May arrive on the audio thread while the host plays automation, so the
// graph itself is only touched later from the timer.
void AudioPluginAudioProcessor::parameterChanged(const juce::String &, float) {
  if (!syncingPoints.load())
    pointsDirty.store(true);
}

void AudioPluginAudioProcessor::pushShapeToPointParams() {
  const auto points = getShape(0).points;
  const auto wasSyncing = syncingPoints.exchange(true);

  for (size_t i = 0; i < pointParams.size() && i < points.size(); ++i) {
    const std::array<float, 3> values{points[i].x, points[i].y,
                                      points[i].curve};
    for (size_t field = 0; field < 3; ++field) {
      auto *parameter = pointParams[i][field];
      const auto normalised = parameter->convertTo0to1(values[field]);
      if (!juce::approximatelyEqual(parameter->getValue(), normalised))
        parameter->setValueNotifyingHost(normalised);
    }
  }

  syncingPoints.store(wasSyncing);
}

void AudioPluginAudioProcessor::applyPointParamsToShape() {
  auto shape = getShape(0);
  auto &points = shape.points;
  const auto count = juce::jmin(pointParams.size(), points.size());
  const auto last = points.size() - 1;
  const auto differs = [](float a, float b) { return std::abs(a - b) > 1.0e-5f; };
  auto value = [this](size_t point, size_t field) {
    auto *parameter = pointParams[point][field];
    return parameter->convertFrom0to1(parameter->getValue());
  };

  bool changed = false;

  // The two ends share a level. Whichever end's parameter moved wins, and the
  // other end's parameter is brought along afterwards.
  const auto endLevel = points[0].y;
  auto newEndLevel = endLevel;
  if (differs(value(0, 1), endLevel))
    newEndLevel = value(0, 1);
  else if (last < count && differs(value(last, 1), endLevel))
    newEndLevel = value(last, 1);
  if (differs(newEndLevel, endLevel)) {
    points[0].y = points[last].y = newEndLevel;
    changed = true;
  }

  for (size_t i = 0; i < count; ++i) {
    auto &point = points[i];

    if (i > 0 && i < last) {
      // Inner points stay between their neighbours.
      const auto x = juce::jlimit(points[i - 1].x, points[i + 1].x, value(i, 0));
      const auto y = value(i, 1);
      changed |= differs(point.x, x) || differs(point.y, y);
      point.x = x;
      point.y = y;
    }

    const auto curve = value(i, 2);
    changed |= differs(point.curve, curve);
    point.curve = curve;
  }

  if (!changed)
    return;

  {
    const juce::ScopedLock lock(shapeLock);
    shapes[0] = shape;
    renderTable(0);
  }

  syncingPoints.store(true);
  for (const auto end : {(size_t)0, last})
    if (end < count)
      pointParams[end][1]->setValueNotifyingHost(newEndLevel);
  syncingPoints.store(false);

  sendChangeMessage();
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const {
  return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const { return true; }
bool AudioPluginAudioProcessor::producesMidi() const { return true; }
bool AudioPluginAudioProcessor::isMidiEffect() const { return false; }
double AudioPluginAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int AudioPluginAudioProcessor::getNumPrograms() {
  return 1; // NB: some hosts don't cope very well if you tell them there are 0
            // programs, so this should be at least 1, even if you're not really
            // implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram() { return 0; }

void AudioPluginAudioProcessor::setCurrentProgram(int index) {
  juce::ignoreUnused(index);
}

const juce::String AudioPluginAudioProcessor::getProgramName(int index) {
  juce::ignoreUnused(index);
  return {};
}

void AudioPluginAudioProcessor::changeProgramName(int index,
                                                  const juce::String &newName) {
  juce::ignoreUnused(index, newName);
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay(double newSampleRate,
                                              int samplesPerBlock) {
  sampleRate = newSampleRate;

  crossover.prepare({newSampleRate, (juce::uint32)samplesPerBlock, 2});
  crossover.reset();

  for (auto &filter : filters)
    filter.prepare(newSampleRate);

  midiOut.ensureSize(4096);
  midiOut.clear();
  smoothed.fill(0.0f);
  lfoPos = 0.0;
  heldNotes = 0;
  lastCcValue = -1;
}

void AudioPluginAudioProcessor::releaseResources() {}

bool AudioPluginAudioProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
  const auto &out = layouts.getMainOutputChannelSet();
  if (out != juce::AudioChannelSet::mono() &&
      out != juce::AudioChannelSet::stereo())
    return false;

  return out == layouts.getMainInputChannelSet();
}

//==============================================================================
float AudioPluginAudioProcessor::readTable(int graph, float phase) const {
  const auto &table = tables[(size_t)graph];
  const auto position = phase * (float)tableSize;
  const auto index = juce::jlimit(0, tableSize - 1, (int)position);
  const auto a = table[(size_t)index].load(std::memory_order_relaxed);
  const auto b = table[(size_t)index + 1].load(std::memory_order_relaxed);
  return a + (position - (float)index) * (b - a);
}

void AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                             juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;

  const auto numSamples = buffer.getNumSamples();
  const auto numChannels = juce::jmin(2, getTotalNumInputChannels());
  for (auto ch = numChannels; ch < getTotalNumOutputChannels(); ++ch)
    buffer.clear(ch, 0, numSamples);

  // Host transport
  double bpm = 120.0, ppq = 0.0;
  bool playing = false, hasPpq = false;
  if (auto *playHead = getPlayHead()) {
    if (const auto position = playHead->getPosition()) {
      if (const auto hostBpm = position->getBpm())
        bpm = *hostBpm;
      if (const auto hostPpq = position->getPpqPosition()) {
        ppq = *hostPpq;
        hasPpq = true;
      }
      playing = position->getIsPlaying();
    }
  }

  auto flag = [](const std::atomic<float> *p) { return p->load() > 0.5f; };

  // LFO timing
  const auto sync = flag(params.sync);
  const auto anchor = flag(params.anchor);
  const auto retrigMode = juce::roundToInt(params.retrig->load());
  const auto envelope = retrigMode == 2;
  const auto gate = flag(params.gate);
  const auto noteRate = flag(params.noteRate);
  const auto noteCutoff = flag(params.noteCutoff);
  const auto velPwm = flag(params.velPwm);

  const auto &rates = rateEntries();
  const auto beatsPerCycle =
      rates[(size_t)juce::jlimit(0, (int)rates.size() - 1,
                                 juce::roundToInt(params.rateSync->load()))]
          .beats;
  const auto cyclesPerSecond =
      sync ? bpm / (60.0 * beatsPerCycle) : (double)params.rateHz->load();
  const auto baseIncrement = cyclesPerSecond / sampleRate;

  // Note->Rate puts the Rate slider wherever the last note says (0-127
  // across its travel) without touching the parameter itself.
  auto noteIncrement = [&] {
    if (!noteRate || noteNorm < 0.0f)
      return baseIncrement;

    if (sync) {
      const auto index =
          juce::roundToInt(noteNorm * (float)(rates.size() - 1));
      return bpm / (60.0 * rates[(size_t)index].beats) / sampleRate;
    }
    return (double)(minRateHz * std::pow(maxRateHz / minRateHz, noteNorm)) /
           sampleRate;
  };
  auto increment = noteIncrement();
  const auto offsetCycles = (double)params.offset->load() * baseIncrement;

  // Locked to the song position only when nothing else is steering the phase.
  if (anchor && sync && playing && hasPpq && retrigMode == 0 && !gate &&
      !noteRate) {
    lfoPos = std::fmod(ppq / beatsPerCycle, lfoWrap);
    if (lfoPos < 0.0)
      lfoPos += lfoWrap;
  }

  const auto swing =
      sync && anchor ? (double)params.swing->load() * 0.005 : 0.0;
  const auto phaseOffset = (double)params.phase->load() * 0.01;
  const auto pwmParam = params.pwm->load() * 0.01f;
  const auto smooth = params.smooth->load() * 0.01f;
  const auto smoothCoeff =
      smooth <= 0.0f
          ? 1.0f
          : 1.0f - std::exp(-1.0f /
                            (smooth * smooth * 0.1f * (float)sampleRate));

  // Routing
  const auto depth = params.depth->load() * 0.01f;
  std::array<int, numDests> graphs;
  std::array<float, numDests> amounts, graphMults;
  for (size_t d = 0; d < numDests; ++d) {
    const auto picked = noteGraph[d].load();
    graphs[d] = picked >= 0 ? picked
                            : juce::jlimit(0, numGraphs - 1,
                                           juce::roundToInt(
                                               params.graph[d]->load()));
    amounts[d] = params.amount[d]->load() * 0.01f * depth;
    graphMults[d] = multipliers[(size_t)graphs[d]].load();
  }

  const auto viewed = juce::jlimit(0, numGraphs - 1, viewedGraph.load());
  const auto viewedMult = multipliers[(size_t)viewed].load();

  // Filter
  const auto filterOn = flag(params.filterOn);
  const auto filterType = juce::roundToInt(params.filterType->load());
  const auto cutoffSlider =
      std::log(params.cutoff->load() / minFreq) / logFreqRange;
  // Note->Cutoff does the same for the Cutoff slider.
  auto cutoffNorm = noteCutoff && noteNorm >= 0.0f ? noteNorm : cutoffSlider;
  const auto resoBase = params.reso->load() * 0.01f;
  const auto varBase = params.var->load() * 0.01f;
  const auto mix = params.mix->load() * 0.01f;
  const auto drive = params.drive->load() * 0.01f;
  const auto driveGain = 1.0f + drive * 15.0f;
  const auto driveMakeup = 1.0f / std::sqrt(driveGain);
  const auto driveBlend = juce::jmin(1.0f, drive * 8.0f);
  const auto topFreq = juce::jmin(maxFreq, (float)sampleRate * 0.45f);

  // Split
  const auto splitOn = flag(params.splitOn);
  const auto splitHigh = flag(params.splitHigh);
  crossover.setCutoffFrequency(
      juce::jlimit(minFreq, topFreq, params.splitFreq->load()));

  // MIDI
  const auto ccNumber = juce::roundToInt(params.ccOut->load());
  midiOut.clear();

  // Notes 0-59 choose graphs, unless a switch that reads the note number or
  // velocity is on, in which case every note plays the LFO instead.
  const auto graphNotes = !(velPwm || noteRate || noteCutoff);
  const auto graphNotesOnly =
      graphNotes && PluginConfig::get().graphNotesHavePriority;
  const auto ccThin = PluginConfig::get().ccThinSamples;

  auto handleMidi = [&](const juce::MidiMessage &message) {
    const auto note = message.getNoteNumber();
    const auto picksGraph = graphNotes && note < firstPlayNote;

    if (message.isNoteOn()) {
      if (picksGraph) {
        // One octave per destination: cut, res, vol, pan, var.
        const auto dest = (size_t)(note / 12);
        graphs[dest] = note % 12;
        graphMults[dest] = multipliers[(size_t)graphs[dest]].load();
        noteGraph[dest].store(graphs[dest]);
        if (graphNotesOnly)
          return;
      }

      ++heldNotes;
      noteNorm = (float)note / 127.0f;
      lastVelocity = message.getFloatVelocity();
      increment = noteIncrement();
      if (noteCutoff)
        cutoffNorm = noteNorm;
      if (retrigMode > 0)
        lfoPos = 0.0;
    } else if (message.isNoteOff()) {
      if (!(picksGraph && graphNotesOnly))
        heldNotes = juce::jmax(0, heldNotes - 1);
    } else if (message.isAllNotesOff() || message.isAllSoundOff()) {
      heldNotes = 0;
    }
  };

  // Where we are within the global LFO cycle, 0..1.
  auto cycleFraction = [=](double position) {
    if (envelope)
      return juce::jlimit(0.0, 0.999999, position);

    if (swing != 0.0) {
      // Swing moves the boundary between each pair of cycles.
      const auto pair = position - 2.0 * std::floor(position * 0.5);
      const auto boundary = 1.0 + swing;
      position = pair < boundary ? pair / boundary
                                 : 1.0 + (pair - boundary) / (2.0 - boundary);
    }

    return position - std::floor(position);
  };

  // A graph's own read position: its speed multiplier restarts with the
  // global cycle, then Phase and PWM are applied.
  auto graphPhase = [=](double fraction, float multiplier, float pwm) {
    const auto phase = fraction * (double)multiplier + phaseOffset;
    return LfoShape::applyPwm((float)(phase - std::floor(phase)), pwm);
  };

  auto *left = buffer.getWritePointer(0);
  auto *right = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;
  std::array<float *, 2> channelData{left, right};

  auto midiIt = midiMessages.cbegin();
  const auto midiEnd = midiMessages.cend();
  float viewedPhase = viewPhase.load(std::memory_order_relaxed);
  float cutFinal = 0.0f, resFinal = 0.0f, varFinal = 0.0f, gain = 1.0f,
        pan = 0.0f;

  for (int i = 0; i < numSamples; ++i) {
    for (; midiIt != midiEnd && (*midiIt).samplePosition <= i; ++midiIt)
      handleMidi((*midiIt).getMessage());

    // Advance the LFO. Note Gate holds the cycle while no key is down.
    if (!(gate && heldNotes == 0)) {
      lfoPos += increment;
      if (envelope)
        lfoPos = juce::jmin(lfoPos, 2.0);
      else if (lfoPos >= lfoWrap)
        lfoPos -= lfoWrap;
    }

    auto position = lfoPos + offsetCycles;
    if (position < 0.0)
      position = envelope ? 0.0 : position + lfoWrap;
    const auto fraction = cycleFraction(position);

    const auto pwm =
        velPwm ? juce::jlimit(-1.0f, 1.0f,
                              pwmParam + lastVelocity * 2.0f - 1.0f)
               : pwmParam;

    for (size_t d = 0; d < numDests; ++d) {
      const auto target =
          readTable(graphs[d], graphPhase(fraction, graphMults[d], pwm));
      smoothed[d] += smoothCoeff * (target - smoothed[d]);
    }

    // Filter settings and CC out move at control rate.
    if (i % controlInterval == 0) {
      cutFinal = juce::jlimit(
          0.0f, 1.0f, cutoffNorm + amounts[destCut] * smoothed[destCut]);
      resFinal = juce::jlimit(0.0f, 1.0f,
                              resoBase + amounts[destRes] * smoothed[destRes]);
      varFinal = juce::jlimit(0.0f, 1.0f,
                              varBase + amounts[destVar] * smoothed[destVar]);

      if (filterOn)
        filterCoeffs = FilterEngine::makeCoeffs(
            filterType, minFreq * std::exp(cutFinal * logFreqRange), resFinal,
            varFinal, sampleRate);

      // CC out follows the final cutoff, whether or not the filter is on.
      samplesSinceCc += controlInterval;
      if (ccNumber >= 0 && samplesSinceCc >= ccThin) {
        const auto value = juce::roundToInt(cutFinal * 127.0f);
        if (value != lastCcValue) {
          lastCcValue = value;
          samplesSinceCc = 0;
          midiOut.addEvent(
              juce::MidiMessage::controllerEvent(1, ccNumber, value), i);
        }
      }
    }

    // Volume and pan are applied per sample.
    const auto volAmount = amounts[destVol];
    gain = volAmount >= 0.0f ? 1.0f - volAmount * (1.0f - smoothed[destVol])
                             : 1.0f + volAmount * smoothed[destVol];
    pan = amounts[destPan] * (smoothed[destPan] * 2.0f - 1.0f);
    const std::array<float, 2> channelGain{
        gain * (right != nullptr && pan > 0.0f ? 1.0f - pan : 1.0f),
        gain * (pan < 0.0f ? 1.0f + pan : 1.0f)};

    float inputSum = 0.0f, outputSum = 0.0f;

    for (size_t ch = 0; ch < (size_t)numChannels; ++ch) {
      const auto input = channelData[ch][i];

      // Only one band is processed when split is on; the other passes dry.
      float low = 0.0f, high = 0.0f;
      crossover.processSample((int)ch, input, low, high);
      auto wet = splitOn ? (splitHigh ? high : low) : input;
      const auto untouched = splitOn ? (splitHigh ? low : high) : 0.0f;

      if (filterOn) {
        auto driven = wet;
        if (driveBlend > 0.0f)
          driven += driveBlend *
                    (std::tanh(wet * driveGain) * driveMakeup - wet);

        wet += mix * (filters[ch].process(driven, filterCoeffs) - wet);
      }

      const auto output = wet * channelGain[ch] + untouched;
      channelData[ch][i] = output;
      inputSum += input;
      outputSum += output;
    }

    // Scope: the range each signal covers per slice of the viewed graph.
    viewedPhase = graphPhase(fraction, viewedMult, pwm);
    const auto bin =
        juce::jmin(scopeSize - 1, (int)(viewedPhase * (float)scopeSize));
    const auto inputLevel = inputSum / (float)numChannels;
    const auto outputLevel = outputSum / (float)numChannels;

    if (bin != scopeBin) {
      const auto index = (size_t)scopeBin;
      scopeIn.low[index].store(scopeRange[0], std::memory_order_relaxed);
      scopeIn.high[index].store(scopeRange[1], std::memory_order_relaxed);
      scopeOut.low[index].store(scopeRange[2], std::memory_order_relaxed);
      scopeOut.high[index].store(scopeRange[3], std::memory_order_relaxed);
      scopeBin = bin;
      scopeRange = {inputLevel, inputLevel, outputLevel, outputLevel};
    } else {
      scopeRange[0] = juce::jmin(scopeRange[0], inputLevel);
      scopeRange[1] = juce::jmax(scopeRange[1], inputLevel);
      scopeRange[2] = juce::jmin(scopeRange[2], outputLevel);
      scopeRange[3] = juce::jmax(scopeRange[3], outputLevel);
    }
  }

  crossover.snapToZero();

  viewPhase.store(viewedPhase, std::memory_order_relaxed);
  viewValue.store(readTable(viewed, viewedPhase), std::memory_order_relaxed);
  finalValues[destCut].store(cutFinal, std::memory_order_relaxed);
  finalValues[destRes].store(resFinal, std::memory_order_relaxed);
  finalValues[destVol].store(juce::jlimit(0.0f, 1.0f, gain),
                             std::memory_order_relaxed);
  finalValues[destPan].store(0.5f + 0.5f * pan, std::memory_order_relaxed);
  finalValues[destVar].store(varFinal, std::memory_order_relaxed);

  // Incoming notes only steer the LFO; the output carries just our CCs.
  midiMessages.swapWith(midiOut);
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const {
  return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor *AudioPluginAudioProcessor::createEditor() {
  return new AudioPluginAudioProcessorEditor(*this);
}

//==============================================================================
LfoShape AudioPluginAudioProcessor::getShape(int graph) const {
  const juce::ScopedLock lock(shapeLock);
  return shapes[(size_t)juce::jlimit(0, numGraphs - 1, graph)];
}

void AudioPluginAudioProcessor::setShape(int graph, const LfoShape &shape) {
  if (!juce::isPositiveAndBelow(graph, numGraphs))
    return;

  {
    const juce::ScopedLock lock(shapeLock);
    shapes[(size_t)graph] = shape;
    renderTable(graph);
  }

  if (graph == 0 && !syncingPoints.load())
    pushShapeToPointParams();
}

void AudioPluginAudioProcessor::renderTable(int graph) {
  const juce::ScopedLock lock(shapeLock);
  const auto &shape = shapes[(size_t)graph];
  auto &table = tables[(size_t)graph];

  for (int i = 0; i < tableSize; ++i)
    table[(size_t)i].store(shape.evaluate((float)i / (float)tableSize),
                           std::memory_order_relaxed);

  // Guard sample so interpolation wraps back to the start of the cycle.
  table[(size_t)tableSize].store(table[0].load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
}

float AudioPluginAudioProcessor::getMultiplier(int graph) const {
  return multipliers[(size_t)juce::jlimit(0, numGraphs - 1, graph)].load();
}

void AudioPluginAudioProcessor::setMultiplier(int graph, float multiplier) {
  if (juce::isPositiveAndBelow(graph, numGraphs))
    multipliers[(size_t)graph].store(juce::jlimit(0.25f, 8.0f, multiplier));
}

//==============================================================================
juce::ValueTree AudioPluginAudioProcessor::createState() {
  auto state = apvts.copyState();
  state.removeChild(state.getChildWithName(shapesTag), nullptr);
  state.setProperty(editorScaleProp, editorScale, nullptr);

  juce::ValueTree shapesTree{shapesTag};
  for (int g = 0; g < numGraphs; ++g) {
    juce::ValueTree shape{shapeTag};
    shape.setProperty(pointsProp, getShape(g).toString(), nullptr);
    shape.setProperty(multProp, getMultiplier(g), nullptr);
    shapesTree.appendChild(shape, nullptr);
  }
  state.appendChild(shapesTree, nullptr);

  return state;
}

void AudioPluginAudioProcessor::applyState(const juce::ValueTree &state,
                                           bool honourMidiLock) {
  if (!state.hasType(apvts.state.getType()))
    return;

  // A locked CC number survives preset changes.
  auto *ccParam = apvts.getParameter(ParamID::ccOut);
  const auto keepCc =
      honourMidiLock &&
      apvts.getRawParameterValue(ParamID::midiLock)->load() > 0.5f;
  const auto lockedCc = ccParam->getValue();

  // The graphs come from the state, not from whatever its point parameters
  // happen to hold, so those are re-derived at the end.
  syncingPoints.store(true);

  const auto shapesTree = state.getChildWithName(shapesTag);
  for (int g = 0; g < numGraphs; ++g) {
    const auto child = shapesTree.getChild(g);
    LfoShape shape;
    if (child.isValid())
      LfoShape::fromString(child[pointsProp].toString(), shape);
    setShape(g, shape);
    setMultiplier(g, child.getProperty(multProp, 1.0f));
  }

  editorScale = state.getProperty(editorScaleProp, editorScale);
  apvts.replaceState(state.createCopy());

  if (keepCc) {
    ccParam->setValueNotifyingHost(lockedCc);
    apvts.getParameter(ParamID::midiLock)->setValueNotifyingHost(1.0f);
  }

  syncingPoints.store(false);
  pushShapeToPointParams();
  pointsDirty.store(false);

  sendChangeMessage();
}

void AudioPluginAudioProcessor::getStateInformation(
    juce::MemoryBlock &destData) {
  if (const auto xml = createState().createXml())
    copyXmlToBinary(*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation(const void *data,
                                                    int sizeInBytes) {
  if (const auto xml = getXmlFromBinary(data, sizeInBytes))
    applyState(juce::ValueTree::fromXml(*xml), false);
}

//==============================================================================
juce::File AudioPluginAudioProcessor::getPresetFolder() {
  return PluginConfig::getDataFolder().getChildFile("Presets");
}

juce::File AudioPluginAudioProcessor::getShapeFolder() {
  return PluginConfig::getDataFolder().getChildFile("Shapes");
}

bool AudioPluginAudioProcessor::savePreset(const juce::File &file) {
  const auto path = file.isAChildOf(getPresetFolder())
                        ? file.getRelativePathFrom(getPresetFolder())
                        : file.getFullPathName();
  auto state = createState();
  state.setProperty(presetPathProp, path, nullptr);

  const auto xml = state.createXml();
  if (xml == nullptr || !file.getParentDirectory().createDirectory() ||
      !xml->writeTo(file))
    return false;

  apvts.state.setProperty(presetPathProp, path, nullptr);
  return true;
}

bool AudioPluginAudioProcessor::loadPreset(const juce::File &file) {
  const auto xml = juce::parseXML(file);
  if (xml == nullptr)
    return false;

  auto state = juce::ValueTree::fromXml(*xml);
  if (!state.hasType(apvts.state.getType()))
    return false;

  state.setProperty(presetPathProp,
                    file.isAChildOf(getPresetFolder())
                        ? file.getRelativePathFrom(getPresetFolder())
                        : file.getFullPathName(),
                    nullptr);
  applyState(state, true);
  return true;
}

void AudioPluginAudioProcessor::loadInitPreset() {
  const auto keepCc =
      apvts.getRawParameterValue(ParamID::midiLock)->load() > 0.5f;

  syncingPoints.store(true);

  for (auto *parameter : getParameters()) {
    if (auto *ranged = dynamic_cast<juce::RangedAudioParameter *>(parameter)) {
      const auto locked = ranged->paramID == ParamID::ccOut ||
                          ranged->paramID == ParamID::midiLock;
      if (!(keepCc && locked))
        ranged->setValueNotifyingHost(ranged->getDefaultValue());
    }
  }

  for (int g = 0; g < numGraphs; ++g) {
    setShape(g, LfoShape());
    setMultiplier(g, 1.0f);
  }

  syncingPoints.store(false);
  pushShapeToPointParams();
  pointsDirty.store(false);

  apvts.state.removeProperty(presetPathProp, nullptr);
  sendChangeMessage();
}

juce::File AudioPluginAudioProcessor::getPresetFile() const {
  const auto path = apvts.state.getProperty(presetPathProp).toString();
  if (path.isEmpty())
    return {};

  return juce::File::isAbsolutePath(path) ? juce::File(path)
                                          : getPresetFolder().getChildFile(path);
}

void AudioPluginAudioProcessor::installFactoryPresets() {
  const auto folder = getPresetFolder().getChildFile("Factory");
  if (folder.exists())
    return;

  enum Shape { sidechain = 1, sine, triangle, sawUp, sawDown, square };

  AudioPluginAudioProcessor preset;
  auto set = [&preset](const juce::String &id, float value) {
    if (auto *parameter = preset.apvts.getParameter(id))
      parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
  };
  auto choose = [&preset](const juce::String &id, const juce::String &name) {
    if (auto *choice = dynamic_cast<juce::AudioParameterChoice *>(
            preset.apvts.getParameter(id)))
      choice->setValueNotifyingHost(
          choice->convertTo0to1((float)choice->choices.indexOf(name)));
  };
  auto begin = [&](Shape shape, const juce::String &rate) {
    preset.loadInitPreset();
    preset.setShape(0, LfoShape::preset(shape));
    choose(ParamID::rateSync, rate);
    set(destAmountID(destVol), 0.0f);
  };
  auto filter = [&](const juce::String &type, float cutoff, float reso,
                    float amount) {
    set(ParamID::filterOn, 1.0f);
    choose(ParamID::filterType, type);
    set(ParamID::cutoff, cutoff);
    set(ParamID::reso, reso);
    set(destAmountID(destCut), amount);
  };
  auto save = [&](const juce::String &name) {
    preset.savePreset(folder.getChildFile(name + presetExtension));
  };

  begin(sidechain, "1/4");
  set(destAmountID(destVol), 100.0f);
  save("Sidechain 1-4");

  begin(sine, "1/8");
  set(destAmountID(destVol), 60.0f);
  save("Tremolo 1-8");

  begin(sine, "1/2");
  set(destAmountID(destPan), 100.0f);
  save("AutoPan 1-2");

  begin(square, "1/16");
  set(destAmountID(destVol), 100.0f);
  set(ParamID::smooth, 15.0f);
  save("Trance Gate 1-16");

  begin(triangle, "1/8");
  filter("SVF LP", 150.0f, 55.0f, 55.0f);
  save("Wobble LP");

  begin(sine, "1 bar");
  filter("Phs 36+", 300.0f, 75.0f, 45.0f);
  save("Phaser Sweep");

  begin(sawUp, "1/2");
  filter("Formant 2", 20.0f, 60.0f, 100.0f);
  save("Formant Talk");

  begin(sawDown, "1/4");
  filter("Mg 24", 80.0f, 70.0f, 65.0f);
  set(ParamID::drive, 35.0f);
  save("Acid Pluck");
}

juce::File AudioPluginAudioProcessor::createMidiClip() const {
  const auto graph = viewedGraph.load();
  const auto shape = getShape(graph);
  const auto cc =
      juce::jmax(1, juce::roundToInt(params.ccOut->load())); // mod wheel if off
  const auto &rates = rateEntries();
  const auto beats =
      params.sync->load() > 0.5f
          ? rates[(size_t)juce::jlimit(0, (int)rates.size() - 1,
                                       juce::roundToInt(
                                           params.rateSync->load()))]
                .beats
          : 4.0;

  const int ticksPerBeat = 960;
  const int steps = 512;
  const auto length = beats * ticksPerBeat;

  juce::MidiMessageSequence sequence;
  int lastValue = -1;
  for (int i = 0; i < steps; ++i) {
    const auto value =
        juce::roundToInt(shape.evaluate((float)i / (float)steps) * 127.0f);
    if (value != lastValue) {
      lastValue = value;
      sequence.addEvent(juce::MidiMessage::controllerEvent(1, cc, value),
                        length * i / steps);
    }
  }
  // Repeat the last value at the very end so the clip spans the whole cycle.
  sequence.addEvent(juce::MidiMessage::controllerEvent(1, cc, lastValue),
                    length - 1.0);

  juce::MidiFile midiFile;
  midiFile.setTicksPerQuarterNote(ticksPerBeat);
  midiFile.addTrack(sequence);

  const auto file =
      juce::File::getSpecialLocation(juce::File::tempDirectory)
          .getChildFile("OpenLFO graph " + juce::String(graph + 1) + ".mid");
  file.deleteFile();
  if (juce::FileOutputStream stream(file); stream.openedOk())
    midiFile.writeTo(stream);

  return file;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new AudioPluginAudioProcessor();
}
