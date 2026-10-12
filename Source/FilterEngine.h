#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <vector>

//==============================================================================
// One channel of the multi-mode filter. Coefficients are worked out once per
// control tick with makeCoeffs() and shared by every channel.
class FilterEngine {
public:
  enum Family {
    ladder,     // a = poles
    lowpass,    // a = poles
    highpass,   // a = poles
    bandpass,   // a = stages
    peak,       // a = stages
    notch,      // a = stages
    dual,       // a, b = Mode of each filter
    morph,      // a = MorphSet
    combFb,     // a = FeedbackFilter
    flangeFb,   // a = FeedbackFilter
    phaseFb,    // a = FeedbackFilter
    phaser,     // a = allpass stages, b = feedback sign
    lowShelf,
    highShelf,
    bell,
    comb,       // b = feedback sign
    allpass,
    reverb,
    formant,    // a = voice
    ringMod,
    downsample
  };

  enum Mode { low, high, band, peaking, notching };
  enum FeedbackFilter { fbLow, fbHigh, fbBand };

  struct Type {
    juce::String name;
    juce::String varName; // what the 4th slider does, "-" if nothing
    Family family;
    int a;
    int b;
  };

  static const std::vector<Type> &types();
  static juce::StringArray typeNames();
  static juce::String varName(int type);
  static juce::String groupName(int type); // menu section it belongs to

  struct Coeffs {
    Family family = lowpass;
    int a = 0, b = 0;
    std::array<float, 3> g{}, k{}; // state-variable stages
    float onePole = 0.0f;          // one-pole / ladder stage gain
    float fbPoleA = 0.0f, fbPoleB = 0.0f; // filters inside a feedback loop
    float allpassCoeff = 0.0f;
    float delay = 1.0f; // in samples
    float feedback = 0.0f;
    float damp = 1.0f;
    float shelfGain = 1.0f;
    float preGain = 1.0f;
    float fat = 0.0f;
    float var = 0.0f;
    float phaseInc = 0.0f;
    float holdPeriod = 1.0f;
    float quantSteps = 32768.0f;
  };

  static Coeffs makeCoeffs(int type, float freq, float res, float var,
                           double sampleRate);

  void prepare(double sampleRate);
  void reset();
  float process(float x, const Coeffs &c);

private:
  struct Svf {
    float ic1 = 0.0f, ic2 = 0.0f;
    void tick(float x, float g, float k, float &lp, float &bp, float &hp);
  };

  struct Delay {
    std::vector<float> buffer;
    int pos = 0;
    float read(float delaySamples) const;
    void write(float x);
  };

  static float pick(Mode mode, float x, float lp, float bp, float hp, float k);
  float lowpassPole(float x, float gain, float &state);
  float feedbackFilter(float x, const Coeffs &c);

  std::array<Svf, 3> svf;
  std::array<float, 6> pole{};
  std::array<float, 8> allpassState{};
  std::array<Delay, 6> delays;
  float lastOut = 0.0f;
  float phase = 0.0f;
  float holdCount = 0.0f;
  float held = 0.0f;
};
