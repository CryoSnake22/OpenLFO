#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <vector>

//==============================================================================
// One node of an LFO graph. `curve` is the tension of the segment that leaves
// this node (-1..1, 0 = straight line).
struct LfoPoint {
  float x = 0.0f;
  float y = 0.0f;
  float curve = 0.0f;
};

//==============================================================================
// A single LFO graph: nodes sorted by x, first pinned at x=0, last at x=1.
// Two nodes may share an x to make a vertical step.
class LfoShape {
public:
  static constexpr float curveScale = 8.0f;

  LfoShape() : points(preset(0).points) {}
  explicit LfoShape(std::vector<LfoPoint> p) : points(std::move(p)) {
    sanitise();
  }

  std::vector<LfoPoint> points;

  //============================================================================
  // Maps t (0..1) along a segment to its eased position (0..1).
  static float curveFraction(float curve, float t) {
    const auto a = curve * curveScale;
    if (std::abs(a) < 1.0e-3f)
      return t;
    return (std::exp(a * t) - 1.0f) / (std::exp(a) - 1.0f);
  }

  // Inverse of curveFraction(curve, 0.5): the tension that puts the middle of
  // a segment at fraction q of its height.
  static float curveFromMidFraction(float q) {
    q = juce::jlimit(0.03f, 0.97f, q);
    return juce::jlimit(-1.0f, 1.0f,
                        2.0f * std::log((1.0f - q) / q) / curveScale);
  }

  // Squeezes a 0..1 phase towards the start (pwm < 0) or end (pwm > 0).
  static float applyPwm(float phase, float pwm) {
    if (pwm != 0.0f) {
      const auto pivot = 0.5f + pwm * 0.45f;
      phase = phase < pivot ? 0.5f * phase / pivot
                            : 0.5f + 0.5f * (phase - pivot) / (1.0f - pivot);
    }
    return juce::jlimit(0.0f, 0.999999f, phase);
  }

  float evaluate(float x) const {
    const auto n = (int)points.size();
    if (n == 0)
      return 0.0f;
    if (n == 1)
      return points[0].y;

    int i = 0;
    while (i < n - 2 && points[(size_t)i + 1].x <= x)
      ++i;

    const auto &p0 = points[(size_t)i];
    const auto &p1 = points[(size_t)i + 1];
    const auto width = p1.x - p0.x;
    if (width <= 1.0e-6f)
      return p1.y;

    const auto t = juce::jlimit(0.0f, 1.0f, (x - p0.x) / width);
    return p0.y + (p1.y - p0.y) * curveFraction(p0.curve, t);
  }

  void sanitise() {
    for (auto &p : points) {
      p.x = juce::jlimit(0.0f, 1.0f, p.x);
      p.y = juce::jlimit(0.0f, 1.0f, p.y);
      p.curve = juce::jlimit(-1.0f, 1.0f, p.curve);
    }

    std::stable_sort(
        points.begin(), points.end(),
        [](const LfoPoint &a, const LfoPoint &b) { return a.x < b.x; });

    if (points.size() < 2)
      points = {{0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};

    points.front().x = 0.0f;
    points.back().x = 1.0f;
  }

  //============================================================================
  juce::String toString() const {
    juce::StringArray parts;
    for (const auto &p : points)
      parts.add(juce::String(p.x, 5) + "," + juce::String(p.y, 5) + "," +
                juce::String(p.curve, 4));
    return parts.joinIntoString(";");
  }

  static bool fromString(const juce::String &text, LfoShape &result) {
    std::vector<LfoPoint> parsed;
    for (const auto &part : juce::StringArray::fromTokens(text, ";", "")) {
      const auto fields = juce::StringArray::fromTokens(part, ",", "");
      if (fields.size() < 2)
        continue;
      parsed.push_back({fields[0].getFloatValue(), fields[1].getFloatValue(),
                        fields.size() > 2 ? fields[2].getFloatValue() : 0.0f});
    }

    if (parsed.size() < 2)
      return false;

    result = LfoShape(std::move(parsed));
    return true;
  }

  //============================================================================
  static juce::StringArray presetNames() {
    return {"Default",  "Sidechain", "Sine",   "Triangle", "Saw Up",
            "Saw Down", "Square",    "Pluck",  "Stairs",   "Flat"};
  }

  static LfoShape preset(int index) {
    std::vector<LfoPoint> p;

    switch (index) {
    case 1: // Sidechain
      p = {{0.0f, 0.0f, -0.45f}, {0.6f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f},
           {1.0f, 0.0f, 0.0f}};
      break;
    case 2: { // Sine
      const int steps = 32;
      for (int i = 0; i <= steps; ++i) {
        const auto x = (float)i / (float)steps;
        p.push_back(
            {x,
             0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * x),
             0.0f});
      }
      break;
    }
    case 3: // Triangle
      p = {{0.0f, 0.0f, 0.0f}, {0.5f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
      break;
    case 4: // Saw Up
      p = {{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
      break;
    case 5: // Saw Down
      p = {{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};
      break;
    case 6: // Square
      p = {{0.0f, 1.0f, 0.0f}, {0.5f, 1.0f, 0.0f}, {0.5f, 0.0f, 0.0f},
           {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};
      break;
    case 7: // Pluck
      p = {{0.0f, 1.0f, -0.6f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};
      break;
    case 8: // Stairs
      p = {{0.0f, 0.0f, 0.0f},        {0.25f, 0.0f, 0.0f},
           {0.25f, 1.0f / 3.0f, 0.0f}, {0.5f, 1.0f / 3.0f, 0.0f},
           {0.5f, 2.0f / 3.0f, 0.0f},  {0.75f, 2.0f / 3.0f, 0.0f},
           {0.75f, 1.0f, 0.0f},        {1.0f, 1.0f, 0.0f},
           {1.0f, 0.0f, 0.0f}};
      break;
    case 9: // Flat
      p = {{0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};
      break;
    default: // Default
      p = {{0.0f, 0.3f, 0.3f}, {0.5f, 0.78f, -0.3f}, {1.0f, 1.0f, 0.0f},
           {1.0f, 0.3f, 0.0f}};
      break;
    }

    LfoShape shape{std::vector<LfoPoint>{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}};
    shape.points = std::move(p);
    return shape;
  }
};
