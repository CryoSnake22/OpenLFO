#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Config.h"
#include "FilterEngine.h"
#include "LfoShape.h"

#include <array>
#include <atomic>

//==============================================================================
namespace ParamID {
// Filter
inline constexpr auto filterOn = "filterOn";
inline constexpr auto filterType = "filterType";
inline constexpr auto cutoff = "cutoff";
inline constexpr auto reso = "reso";
inline constexpr auto mix = "mix";
inline constexpr auto var = "var";
inline constexpr auto drive = "drive";
inline constexpr auto offset = "offset";
inline constexpr auto display = "display";

// MIDI
inline constexpr auto retrig = "retrig";
inline constexpr auto gate = "gate";
inline constexpr auto velPwm = "velPwm";
inline constexpr auto noteRate = "noteRate";
inline constexpr auto noteCutoff = "noteCutoff";
inline constexpr auto ccOut = "ccOut";
inline constexpr auto midiLock = "midiLock";

// LFO
inline constexpr auto anchor = "anchor";
inline constexpr auto sync = "sync";
inline constexpr auto triplet = "triplet";
inline constexpr auto dotted = "dotted";
inline constexpr auto rateSync = "rateSync";
inline constexpr auto rateHz = "rateHz";
inline constexpr auto swing = "swing";
inline constexpr auto phase = "phase";
inline constexpr auto pwm = "pwm";
inline constexpr auto smooth = "smooth";
inline constexpr auto snap = "snap";

// Routing
inline constexpr auto depth = "depth";

// Split
inline constexpr auto splitOn = "splitOn";
inline constexpr auto splitHigh = "splitHigh";
inline constexpr auto splitFreq = "splitFreq";
} // namespace ParamID

//==============================================================================
class AudioPluginAudioProcessor final
    : public juce::AudioProcessor,
      public juce::ChangeBroadcaster,
      private juce::AudioProcessorValueTreeState::Listener,
      private juce::Timer {
public:
  static constexpr int numGraphs = 12;
  static constexpr int tableSize = 2048;
  static constexpr int scopeSize = 256;

  // LFO destinations, in the order they appear in the routing panel.
  enum Destination { destCut, destRes, destVol, destPan, destVar, numDests };
  static juce::String destName(int dest);
  static juce::String destGraphID(int dest) { return destName(dest) + "Graph"; }
  static juce::String destAmountID(int dest) { return destName(dest) + "Amt"; }

  // What the graph area shows.
  enum Display { displayGraph, displayScope, displayFilter };

  // Every length the synced Rate slider can land on, longest first.
  struct RateEntry {
    enum Kind { plain, dotted, triplet };
    juce::String name;
    double beats;
    Kind kind;
  };
  static const std::vector<RateEntry> &rateEntries();

  // Notes below this pick the graph for each destination.
  static constexpr int firstPlayNote = 60;

  // The first few points of graph 1 are host parameters (X, Y, curve each).
  static constexpr int numAutomatedPoints = 8;
  static juce::String pointParamID(int point, int field);

  //==============================================================================
  AudioPluginAudioProcessor();
  ~AudioPluginAudioProcessor() override;

  //==============================================================================
  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
  using AudioProcessor::processBlock;

  //==============================================================================
  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override;

  //==============================================================================
  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  //==============================================================================
  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String &newName) override;

  //==============================================================================
  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;

  //==============================================================================
  // Graphs. The point lists are owned by the message thread; the audio thread
  // only ever reads the rendered lookup tables.
  LfoShape getShape(int graph) const;
  void setShape(int graph, const LfoShape &shape);

  static const std::array<float, 7> &rateMultipliers();
  static juce::String multiplierText(float multiplier);
  float getMultiplier(int graph) const;
  void setMultiplier(int graph, float multiplier);

  //==============================================================================
  // Presets (whole plugin state) and user shapes live next to each other in
  // the user's application data folder.
  static juce::File getPresetFolder();
  static juce::File getShapeFolder();
  static void installFactoryPresets();
  bool savePreset(const juce::File &file);
  bool loadPreset(const juce::File &file);
  void loadInitPreset();
  juce::File getPresetFile() const;

  // Writes one cycle of the viewed graph as a MIDI CC clip, for dragging into
  // the host.
  juce::File createMidiClip() const;

  //==============================================================================
  juce::AudioProcessorValueTreeState apvts;

  // Written by the audio thread for the editor to draw.
  std::atomic<int> viewedGraph{0};
  std::atomic<float> viewPhase{0.0f};
  std::atomic<float> viewValue{0.0f};
  std::array<std::atomic<float>, numDests> finalValues{}; // 0..1 per dest
  struct ScopeTrace {
    std::array<std::atomic<float>, scopeSize> low{}, high{};
  };
  ScopeTrace scopeIn, scopeOut;

  float editorScale = 1.3f;

private:
  //==============================================================================
  static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

  void timerCallback() override;
  void parameterChanged(const juce::String &parameterID,
                        float newValue) override;
  float readTable(int graph, float phase) const;
  void renderTable(int graph);

  // Graph 1 <-> its point parameters, both on the message thread.
  void pushShapeToPointParams();
  void applyPointParamsToShape();

  juce::ValueTree createState();
  void applyState(const juce::ValueTree &state, bool honourMidiLock);

  //==============================================================================
  mutable juce::CriticalSection shapeLock;
  std::array<LfoShape, numGraphs> shapes;
  std::array<std::array<std::atomic<float>, tableSize + 1>, numGraphs> tables{};
  std::array<std::atomic<float>, numGraphs> multipliers{};

  // Cached parameter values
  struct Params {
    std::atomic<float> *filterOn, *filterType, *cutoff, *reso, *mix, *var,
        *drive, *offset, *retrig, *gate, *velPwm, *noteRate, *noteCutoff,
        *ccOut, *anchor, *sync, *rateSync, *rateHz, *swing, *phase, *pwm,
        *smooth, *depth, *splitOn, *splitHigh, *splitFreq;
    std::array<std::atomic<float> *, numDests> graph, amount;
  } params{};

  // Audio thread state
  double sampleRate = 44100.0;
  double lfoPos = 0.0; // in cycles
  std::array<float, numDests> smoothed{};
  std::array<FilterEngine, 2> filters;
  FilterEngine::Coeffs filterCoeffs;
  juce::dsp::LinkwitzRileyFilter<float> crossover;
  juce::MidiBuffer midiOut;
  int heldNotes = 0;
  float noteNorm = -1.0f; // last played note as a 0..1 slider position
  float lastVelocity = 0.5f;
  int lastCcValue = -1;
  int samplesSinceCc = 0;
  int scopeBin = 0;
  std::array<float, 4> scopeRange{}; // in low/high, out low/high

  // Graph picked by a MIDI note, until the message thread has moved the
  // routing parameter to match.
  std::array<std::atomic<int>, numDests> noteGraph;

  std::array<std::array<juce::RangedAudioParameter *, 3>, numAutomatedPoints>
      pointParams{};
  std::atomic<bool> pointsDirty{false};
  std::atomic<bool> syncingPoints{false};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
