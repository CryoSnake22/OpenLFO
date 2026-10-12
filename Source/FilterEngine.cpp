#include "FilterEngine.h"

#include <cmath>

namespace {
constexpr float minFreq = 20.0f;
constexpr float pi = juce::MathConstants<float>::pi;

// Vowel formants (A E I O U) for three voices, in Hz.
constexpr float formants[3][5][3] = {
    {{600, 1040, 2250}, {400, 1620, 2400}, {250, 1750, 2600},
     {400, 750, 2400}, {350, 600, 2400}},
    {{650, 1080, 2650}, {400, 1700, 2600}, {290, 1870, 2800},
     {400, 800, 2600}, {350, 600, 2700}},
    {{800, 1150, 2900}, {350, 2000, 2800}, {270, 2140, 2950},
     {450, 800, 2830}, {325, 700, 2700}}};
constexpr float formantGains[3] = {1.0f, 0.5f, 0.25f};
constexpr float reverbCombs[4] = {1.0f, 1.13f, 1.27f, 1.41f};

float warped(float freq, double sampleRate) {
  return std::tan(pi * juce::jlimit(minFreq, (float)sampleRate * 0.45f, freq) /
                  (float)sampleRate);
}

float sweep(float normalised) { // 0..1 -> 20 Hz..20 kHz
  return minFreq * std::pow(1000.0f, juce::jlimit(0.0f, 1.0f, normalised));
}
} // namespace

//==============================================================================
const std::vector<FilterEngine::Type> &FilterEngine::types() {
  static const std::vector<Type> list = [] {
    std::vector<Type> t;
    const juce::String none{"-"};

    t.push_back({"SVF LP", "Fat", lowpass, 2, 0});
    t.push_back({"SVF BP", "Fat", bandpass, 1, 0});
    t.push_back({"SVF HP", "Fat", highpass, 2, 0});
    t.push_back({"SVF Notch", "Fat", notch, 1, 0});
    t.push_back({"SVF Peak", "Fat", peak, 1, 0});

    for (int poles : {1, 3, 4})
      t.push_back({"Low " + juce::String(poles * 6), "Fat", lowpass, poles, 0});
    for (int poles : {1, 3, 4})
      t.push_back({"High " + juce::String(poles * 6), "Fat", highpass, poles, 0});
    t.push_back({"Band 24", "Fat", bandpass, 2, 0});
    t.push_back({"Notch 24", "Fat", notch, 2, 0});
    t.push_back({"Peak 24", "Fat", peak, 2, 0});

    for (int poles = 1; poles <= 4; ++poles)
      t.push_back({"Mg " + juce::String(poles * 6), "Fat", ladder, poles, 0});

    // Two filters in series, the second with its own cutoff.
    const char letters[] = "LHBPN";
    for (const auto *pair : {"LH", "LB", "LP", "LN", "HB", "HP", "HN", "BP",
                             "BN", "PP", "PN", "NN"}) {
      const auto first = (int)(std::strchr(letters, pair[0]) - letters);
      const auto second = (int)(std::strchr(letters, pair[1]) - letters);
      t.push_back({pair, "Cutoff 2", dual, first, second});
    }

    int set = 0;
    for (const auto *name : {"LBH", "LPH", "LNH", "BPN"})
      t.push_back({name, "Morph", morph, set++, 0});

    // Comb / flanger / phaser with a filter inside the feedback loop.
    const char *const loops[] = {"L", "H", "HL"};
    const char *const loopVars[] = {"LP Cut", "HP Cut", "HL Width"};
    for (int f = 0; f < 3; ++f) {
      t.push_back({juce::String("Comb") + loops[f], loopVars[f], combFb, f, 0});
      t.push_back({juce::String("Flange") + loops[f], loopVars[f], flangeFb, f, 0});
      t.push_back({juce::String("Phase") + loops[f], loopVars[f], phaseFb, f, 0});
    }

    for (int sign : {1, -1})
      for (int stages : {2, 4, 6, 8})
        t.push_back({"Phs " + juce::String(stages * 6) + (sign > 0 ? "+" : "-"),
                     none, phaser, stages, sign});

    t.push_back({"Shelf L", "dB Gain", lowShelf, 0, 0});
    t.push_back({"Shelf H", "dB Gain", highShelf, 0, 0});
    t.push_back({"Peak EQ", "dB Gain", bell, 0, 0});

    t.push_back({"Comb +", "Damping", comb, 0, 1});
    t.push_back({"Comb -", "Damping", comb, 0, -1});
    t.push_back({"Allpass", "Damping", allpass, 0, 0});
    t.push_back({"Reverb", "Damping", reverb, 0, 0});

    for (int voice = 0; voice < 3; ++voice)
      t.push_back({"Formant " + juce::String(voice + 1), "Formant", formant, voice, 0});

    t.push_back({"Ring Mod", none, ringMod, 0, 0});
    t.push_back({"Downsample", "Crush", downsample, 0, 0});
    return t;
  }();

  return list;
}

juce::StringArray FilterEngine::typeNames() {
  juce::StringArray names;
  for (const auto &type : types())
    names.add(type.name);
  return names;
}

juce::String FilterEngine::varName(int type) {
  return juce::isPositiveAndBelow(type, (int)types().size())
             ? types()[(size_t)type].varName
             : juce::String("-");
}

juce::String FilterEngine::groupName(int type) {
  if (!juce::isPositiveAndBelow(type, (int)types().size()))
    return {};

  switch (types()[(size_t)type].family) {
  case lowpass:
  case highpass:
  case bandpass:
  case peak:
  case notch:
    return "SVF";
  case ladder:
    return "Ladder";
  case dual:
    return "Dual";
  case morph:
    return "Morph";
  case combFb:
  case flangeFb:
  case phaseFb:
    return "Comb / Flange / Phase";
  case phaser:
    return "Phaser";
  case lowShelf:
  case highShelf:
  case bell:
    return "EQ";
  case comb:
  case allpass:
  case reverb:
    return "Delay";
  case formant:
    return "Formant";
  case ringMod:
  case downsample:
    return "Special";
  }
  return {};
}

//==============================================================================
FilterEngine::Coeffs FilterEngine::makeCoeffs(int type, float freq, float res,
                                              float var, double sampleRate) {
  const auto &info =
      types()[(size_t)juce::jlimit(0, (int)types().size() - 1, type)];
  const auto sr = (float)sampleRate;

  Coeffs c;
  c.family = info.family;
  c.a = info.a;
  c.b = info.b;
  c.var = var;

  freq = juce::jlimit(minFreq, sr * 0.45f, freq);
  const auto g = warped(freq, sampleRate);
  const auto k = 1.0f / (0.7071f * std::pow(30.0f, res));
  const auto maxDelay = sr / minFreq;

  c.g.fill(g);
  c.k.fill(k);
  c.onePole = g / (1.0f + g);
  c.delay = juce::jlimit(1.0f, maxDelay, sr / freq);
  c.allpassCoeff = (g - 1.0f) / (g + 1.0f);

  switch (info.family) {
  case ladder:
    c.feedback = res * 3.8f;
    c.fat = var;
    c.preGain = 1.0f + var * res * 2.0f;
    break;

  case lowpass:
  case highpass:
  case bandpass:
  case peak:
  case notch: {
    // Stacked stages share the resonance between them.
    const auto stacked = (info.family == lowpass || info.family == highpass)
                             ? info.a == 4
                             : info.a == 2;
    if (stacked)
      c.k.fill(std::sqrt(k));
    c.fat = var;
    c.preGain = 1.0f + var * res;
    break;
  }

  case dual:
    // Two resonant stages in series, so each takes half the resonance.
    c.g[1] = warped(sweep(var), sampleRate);
    c.k.fill(std::sqrt(k));
    break;

  case morph:
    break;

  case combFb:
  case flangeFb:
  case phaseFb: {
    // L / H: Var is the loop filter's cutoff. HL: Var is how far apart the
    // highpass and lowpass sit around 1 kHz.
    const auto spread = std::pow(2.0f, 0.5f + var * 4.5f);
    const auto gA = warped(info.a == fbBand ? 1000.0f * spread : sweep(var),
                           sampleRate);
    const auto gB = warped(1000.0f / spread, sampleRate);
    c.fbPoleA = gA / (1.0f + gA);
    c.fbPoleB = gB / (1.0f + gB);
    c.feedback = res * 0.95f;
    break;
  }

  case phaser:
    c.feedback = res * 0.9f * (float)info.b;
    break;

  case lowShelf:
  case highShelf:
  case bell: {
    const auto gain = std::pow(10.0f, (var - 0.5f) * 36.0f / 40.0f);
    const auto q = 0.5f * std::pow(16.0f, res);
    c.shelfGain = gain;
    c.k[0] = info.family == bell ? 1.0f / (q * gain) : 1.0f / q;
    if (info.family == lowShelf)
      c.g[0] = g / std::sqrt(gain);
    else if (info.family == highShelf)
      c.g[0] = g * std::sqrt(gain);
    break;
  }

  case comb:
    c.feedback = res * 0.95f * (float)info.b;
    c.damp = 1.0f - var * 0.95f;
    break;

  case allpass:
    c.feedback = res * 0.9f;
    c.damp = 1.0f - var * 0.95f;
    break;

  case reverb:
    c.delay = juce::jlimit(1.0f, maxDelay, sr / freq);
    c.feedback = 0.7f + res * 0.28f;
    c.damp = 1.0f - var * 0.95f;
    break;

  case formant: {
    // The cutoff walks through the vowels, Var shifts them all up or down.
    const auto position =
        juce::jlimit(0.0f, 3.999f,
                     std::log(freq / minFreq) / std::log(1000.0f) * 4.0f);
    const auto vowel = (int)position;
    const auto blend = position - (float)vowel;
    const auto shift = std::pow(2.0f, (var - 0.5f) * 2.0f);
    const auto q = 4.0f * std::pow(8.0f, res);

    for (size_t i = 0; i < 3; ++i) {
      const auto f0 = formants[info.a][vowel][i];
      const auto f1 = formants[info.a][vowel + 1][i];
      c.g[i] = warped((f0 + (f1 - f0) * blend) * shift, sampleRate);
      c.k[i] = 1.0f / q;
    }
    break;
  }

  case ringMod:
    c.phaseInc = freq / sr;
    break;

  case downsample:
    c.holdPeriod = juce::jmax(1.0f, sr / (freq * 4.0f));
    c.quantSteps = std::pow(2.0f, 15.0f - 13.0f * var);
    break;
  }

  return c;
}

//==============================================================================
void FilterEngine::Svf::tick(float x, float g, float k, float &lp, float &bp,
                             float &hp) {
  const auto a1 = 1.0f / (1.0f + g * (g + k));
  const auto a2 = g * a1;
  const auto a3 = g * a2;
  const auto v3 = x - ic2;
  const auto v1 = a1 * ic1 + a2 * v3;
  const auto v2 = ic2 + a2 * ic1 + a3 * v3;
  ic1 = 2.0f * v1 - ic1;
  ic2 = 2.0f * v2 - ic2;
  lp = v2;
  bp = v1;
  hp = x - k * v1 - v2;
}

float FilterEngine::Delay::read(float delaySamples) const {
  const auto size = (int)buffer.size();
  auto position = (float)pos - delaySamples;
  if (position < 0.0f)
    position += (float)size;

  const auto i0 = juce::jmin(size - 1, (int)position);
  const auto i1 = i0 + 1 < size ? i0 + 1 : 0;
  const auto frac = position - (float)i0;
  return buffer[(size_t)i0] + frac * (buffer[(size_t)i1] - buffer[(size_t)i0]);
}

void FilterEngine::Delay::write(float x) {
  buffer[(size_t)pos] = x;
  if (++pos >= (int)buffer.size())
    pos = 0;
}

void FilterEngine::prepare(double sampleRate) {
  // Room for the lowest cutoff, plus the longest reverb comb.
  const auto length = (size_t)(sampleRate / minFreq * 1.5) + 8;
  for (auto &delay : delays)
    delay.buffer.assign(length, 0.0f);
  reset();
}

void FilterEngine::reset() {
  svf = {};
  pole.fill(0.0f);
  allpassState.fill(0.0f);
  for (auto &delay : delays) {
    std::fill(delay.buffer.begin(), delay.buffer.end(), 0.0f);
    delay.pos = 0;
  }
  lastOut = phase = holdCount = held = 0.0f;
}

//==============================================================================
float FilterEngine::pick(Mode mode, float x, float lp, float bp, float hp,
                         float k) {
  switch (mode) {
  case low:
    return lp;
  case high:
    return hp;
  case band:
    return bp * k;
  case peaking:
    return lp - hp;
  case notching:
    return x - bp * k;
  }
  return x;
}

float FilterEngine::lowpassPole(float x, float gain, float &state) {
  const auto v = gain * (x - state);
  const auto y = v + state;
  state = y + v;
  return y;
}

float FilterEngine::feedbackFilter(float x, const Coeffs &c) {
  const auto lowpassed = lowpassPole(x, c.fbPoleA, pole[4]);
  if (c.a == fbLow)
    return lowpassed;
  if (c.a == fbHigh)
    return x - lowpassed;
  return lowpassed - lowpassPole(lowpassed, c.fbPoleB, pole[5]);
}

float FilterEngine::process(float x, const Coeffs &c) {
  float lp, bp, hp;
  auto fatten = [&c](float y) { return y + c.fat * (std::tanh(y) - y); };

  switch (c.family) {
  case ladder: {
    auto stage = std::tanh(x * c.preGain - c.feedback * lastOut);
    auto out = stage;
    for (int i = 0; i < 4; ++i) {
      stage = lowpassPole(stage, c.onePole, pole[(size_t)i]);
      if (i == c.a - 1)
        out = stage;
    }
    lastOut = stage;
    return fatten(out);
  }

  case lowpass:
  case highpass: {
    const auto mode = c.family == lowpass ? low : high;
    auto y = x * c.preGain;

    if (c.a >= 2) {
      svf[0].tick(y, c.g[0], c.k[0], lp, bp, hp);
      y = pick(mode, y, lp, bp, hp, c.k[0]);
    }
    if (c.a == 4) {
      svf[1].tick(y, c.g[0], c.k[0], lp, bp, hp);
      y = pick(mode, y, lp, bp, hp, c.k[0]);
    }
    if (c.a == 1 || c.a == 3) {
      const auto lowpassed = lowpassPole(y, c.onePole, pole[0]);
      y = mode == low ? lowpassed : y - lowpassed;
    }
    return fatten(y);
  }

  case bandpass:
  case peak:
  case notch: {
    const auto mode =
        c.family == bandpass ? band : c.family == peak ? peaking : notching;
    auto y = x * c.preGain;
    for (int i = 0; i < c.a; ++i) {
      svf[(size_t)i].tick(y, c.g[0], c.k[0], lp, bp, hp);
      y = pick(mode, y, lp, bp, hp, c.k[0]);
    }
    return fatten(y);
  }

  case dual: {
    svf[0].tick(x, c.g[0], c.k[0], lp, bp, hp);
    const auto y = pick((Mode)c.a, x, lp, bp, hp, c.k[0]);
    svf[1].tick(y, c.g[1], c.k[1], lp, bp, hp);
    return pick((Mode)c.b, y, lp, bp, hp, c.k[1]);
  }

  case morph: {
    static constexpr Mode sets[4][3] = {{low, band, high},
                                        {low, peaking, high},
                                        {low, notching, high},
                                        {band, peaking, notching}};
    svf[0].tick(x, c.g[0], c.k[0], lp, bp, hp);
    const auto &set = sets[c.a];
    const auto first = pick(set[0], x, lp, bp, hp, c.k[0]);
    const auto middle = pick(set[1], x, lp, bp, hp, c.k[0]);
    const auto last = pick(set[2], x, lp, bp, hp, c.k[0]);
    return c.var < 0.5f ? first + (middle - first) * (c.var * 2.0f)
                        : middle + (last - middle) * ((c.var - 0.5f) * 2.0f);
  }

  case combFb:
  case flangeFb: {
    const auto delayed = delays[0].read(c.delay);
    const auto y = x + c.feedback * feedbackFilter(delayed, c);
    delays[0].write(y);
    // The flanger is only the delayed signal: Mix at 50% makes the flange.
    return c.family == combFb ? y * (1.0f - 0.5f * c.feedback) : delayed;
  }

  case phaseFb:
  case phaser: {
    const auto stages = c.family == phaser ? c.a : 4;
    const auto input =
        x + c.feedback *
                (c.family == phaser ? lastOut : feedbackFilter(lastOut, c));
    auto y = input;
    for (int i = 0; i < stages; ++i) {
      auto &state = allpassState[(size_t)i];
      const auto out = c.allpassCoeff * y + state;
      state = y - c.allpassCoeff * out;
      y = out;
    }
    lastOut = y;
    return 0.5f * (input + y);
  }

  case lowShelf:
    svf[0].tick(x, c.g[0], c.k[0], lp, bp, hp);
    return x + c.k[0] * (c.shelfGain - 1.0f) * bp +
           (c.shelfGain * c.shelfGain - 1.0f) * lp;

  case highShelf:
    svf[0].tick(x, c.g[0], c.k[0], lp, bp, hp);
    return c.shelfGain * c.shelfGain * x +
           c.k[0] * (1.0f - c.shelfGain) * c.shelfGain * bp +
           (1.0f - c.shelfGain * c.shelfGain) * lp;

  case bell:
    svf[0].tick(x, c.g[0], c.k[0], lp, bp, hp);
    return x + c.k[0] * (c.shelfGain * c.shelfGain - 1.0f) * bp;

  case comb: {
    pole[0] += c.damp * (delays[0].read(c.delay) - pole[0]);
    const auto y = x + c.feedback * pole[0];
    delays[0].write(y);
    return y * (1.0f - 0.5f * std::abs(c.feedback));
  }

  case allpass: {
    pole[0] += c.damp * (delays[0].read(c.delay) - pole[0]);
    const auto y = x + c.feedback * pole[0];
    delays[0].write(y);
    return pole[0] - c.feedback * y;
  }

  case reverb: {
    auto sum = 0.0f;
    for (size_t i = 0; i < 4; ++i) {
      pole[i] += c.damp * (delays[i].read(c.delay * reverbCombs[i]) - pole[i]);
      delays[i].write(x + c.feedback * pole[i]);
      sum += pole[i];
    }

    auto y = sum * 0.25f;
    for (size_t i = 4; i < 6; ++i) {
      const auto delayed =
          delays[i].read(juce::jmax(1.0f, c.delay * (i == 4 ? 0.31f : 0.11f)));
      const auto in = y + 0.5f * delayed;
      delays[i].write(in);
      y = delayed - 0.5f * in;
    }
    return y;
  }

  case formant: {
    auto y = 0.0f;
    for (size_t i = 0; i < 3; ++i) {
      svf[i].tick(x, c.g[i], c.k[i], lp, bp, hp);
      y += bp * c.k[i] * formantGains[i];
    }
    return y * 2.0f;
  }

  case ringMod:
    phase += c.phaseInc;
    if (phase >= 1.0f)
      phase -= 1.0f;
    return x * std::sin(phase * juce::MathConstants<float>::twoPi);

  case downsample:
    holdCount += 1.0f;
    if (holdCount >= c.holdPeriod) {
      holdCount -= c.holdPeriod;
      held = std::round(x * c.quantSteps) / c.quantSteps;
    }
    return held;
  }

  return x;
}
