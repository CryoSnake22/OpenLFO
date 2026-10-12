#include "GraphEditor.h"
#include "LookAndFeel.h"

namespace {
constexpr float nodeRadius = 3.0f;
constexpr float hitRadius = 7.0f;
constexpr int gridRows = 4;
constexpr int ySnapSteps = 8;
constexpr float meterWidth = 3.0f;
constexpr float responseRangeDb = 36.0f;
const juce::Colour responseColour{0xff4cc38a};
} // namespace

//==============================================================================
GraphEditor::GraphEditor(AudioPluginAudioProcessor &p) : processorRef(p) {
  snapParam = p.apvts.getRawParameterValue(ParamID::snap);
  displayParam = p.apvts.getRawParameterValue(ParamID::display);
  phaseParam = p.apvts.getRawParameterValue(ParamID::phase);
  pwmParam = p.apvts.getRawParameterValue(ParamID::pwm);
  filterTypeParam = p.apvts.getRawParameterValue(ParamID::filterType);
  mixParam = p.apvts.getRawParameterValue(ParamID::mix);
  responseData.resize((size_t)2 << responseOrder);
  refresh();
  startTimer(PluginConfig::get().redrawMs);
}

void GraphEditor::setGraph(int newGraph) {
  graph = juce::jlimit(0, AudioPluginAudioProcessor::numGraphs - 1, newGraph);
  processorRef.viewedGraph.store(graph);
  refresh();
}

void GraphEditor::refresh() {
  shape = processorRef.getShape(graph);
  draggedNode = draggedHandle = hoveredNode = hoveredHandle = -1;
  repaint();
}

void GraphEditor::setShape(const LfoShape &newShape) {
  shape = newShape;
  shape.sanitise();
  commit();
}

void GraphEditor::commit() {
  processorRef.setShape(graph, shape);
  repaint();
}

void GraphEditor::timerCallback() { repaint(); }

//==============================================================================
juce::Rectangle<float> GraphEditor::getPlotArea() const {
  // The strip on the right is the LFO output meter.
  return getLocalBounds().toFloat().reduced(6.0f, 5.0f).withTrimmedRight(
      meterWidth + 4.0f);
}

juce::Point<float> GraphEditor::toScreen(float x, float y) const {
  const auto area = getPlotArea();
  return {area.getX() + x * area.getWidth(),
          area.getBottom() - y * area.getHeight()};
}

juce::Point<float> GraphEditor::fromScreen(juce::Point<float> position) const {
  const auto area = getPlotArea();
  return {juce::jlimit(0.0f, 1.0f,
                       (position.x - area.getX()) / area.getWidth()),
          juce::jlimit(0.0f, 1.0f,
                       (area.getBottom() - position.y) / area.getHeight())};
}

bool GraphEditor::hasHandle(int segment) const {
  const auto &a = shape.points[(size_t)segment];
  const auto &b = shape.points[(size_t)segment + 1];
  return b.x - a.x > 0.01f && std::abs(b.y - a.y) > 0.01f;
}

juce::Point<float> GraphEditor::handlePosition(int segment) const {
  const auto &a = shape.points[(size_t)segment];
  const auto &b = shape.points[(size_t)segment + 1];
  return toScreen((a.x + b.x) * 0.5f,
                  a.y + (b.y - a.y) * LfoShape::curveFraction(a.curve, 0.5f));
}

int GraphEditor::nodeAt(juce::Point<float> position) const {
  // Search backwards so the later of two stacked nodes wins.
  for (int i = (int)shape.points.size(); --i >= 0;) {
    const auto &p = shape.points[(size_t)i];
    if (toScreen(p.x, p.y).getDistanceFrom(position) <= hitRadius)
      return i;
  }
  return -1;
}

int GraphEditor::handleAt(juce::Point<float> position) const {
  for (int i = 0; i < (int)shape.points.size() - 1; ++i)
    if (hasHandle(i) &&
        handlePosition(i).getDistanceFrom(position) <= hitRadius)
      return i;
  return -1;
}

int GraphEditor::getDisplay() const {
  return juce::roundToInt(displayParam->load());
}

int GraphEditor::getSnap() const {
  return juce::roundToInt(snapParam->load());
}

juce::Path GraphEditor::createCurve() const {
  juce::Path path;
  const auto &points = shape.points;
  path.startNewSubPath(toScreen(points.front().x, points.front().y));

  for (size_t i = 0; i + 1 < points.size(); ++i) {
    const auto &a = points[i];
    const auto &b = points[i + 1];

    if (b.x - a.x > 1.0e-6f && std::abs(a.curve) > 1.0e-3f) {
      const int steps = 32;
      for (int s = 1; s < steps; ++s) {
        const auto t = (float)s / (float)steps;
        path.lineTo(
            toScreen(a.x + (b.x - a.x) * t,
                     a.y + (b.y - a.y) * LfoShape::curveFraction(a.curve, t)));
      }
    }

    path.lineTo(toScreen(b.x, b.y));
  }

  return path;
}

// The shaded area is what the LFO actually outputs once Phase and PWM have
// been applied, which is why it can slide away from the line.
juce::Path GraphEditor::createOutputFill() const {
  const auto area = getPlotArea();
  const auto phase = phaseParam->load() * 0.01f;
  const auto pwm = pwmParam->load() * 0.01f;
  const auto steps = juce::jmax(2, juce::roundToInt(area.getWidth()));

  juce::Path path;
  path.startNewSubPath(area.getBottomLeft());
  for (int i = 0; i <= steps; ++i) {
    const auto x = juce::jmin(0.9999f, (float)i / (float)steps);
    const auto shifted = x + phase;
    path.lineTo(toScreen((float)i / (float)steps,
                         shape.evaluate(LfoShape::applyPwm(
                             shifted - std::floor(shifted), pwm))));
  }
  path.lineTo(area.getBottomRight());
  path.closeSubPath();
  return path;
}

//==============================================================================
void GraphEditor::paint(juce::Graphics &g) {
  const auto bounds = getLocalBounds().toFloat();

  g.setColour(Theme::background);
  g.fillRoundedRectangle(bounds, 5.0f);
  g.setColour(Theme::border);
  g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);

  if (getDisplay() == AudioPluginAudioProcessor::displayFilter) {
    paintFilterResponse(g);
  } else {
    paintGraph(g);
    if (getDisplay() == AudioPluginAudioProcessor::displayScope)
      paintScope(g);
  }

  paintMeter(g);
}

void GraphEditor::paintGraph(juce::Graphics &g) {
  const auto area = getPlotArea();

  // Grid
  const auto columns = getSnap() > 0 ? getSnap() : 4;
  g.setColour(juce::Colours::white.withAlpha(0.06f));
  for (int i = 1; i < columns; ++i) {
    const auto x = area.getX() + area.getWidth() * (float)i / (float)columns;
    g.drawVerticalLine(juce::roundToInt(x), area.getY(), area.getBottom());
  }
  for (int i = 1; i < gridRows; ++i) {
    const auto y = area.getY() + area.getHeight() * (float)i / (float)gridRows;
    g.drawHorizontalLine(juce::roundToInt(y), area.getX(), area.getRight());
  }

  g.setGradientFill(juce::ColourGradient(Theme::accent.withAlpha(0.28f), 0.0f,
                                         area.getY(),
                                         Theme::accent.withAlpha(0.02f), 0.0f,
                                         area.getBottom(), false));
  g.fillPath(createOutputFill());

  const auto curve = createCurve();
  const juce::PathStrokeType::JointStyle joint = juce::PathStrokeType::curved;
  g.setColour(Theme::accent.withAlpha(0.14f));
  g.strokePath(curve, juce::PathStrokeType(5.0f, joint));
  g.setColour(Theme::accent);
  g.strokePath(curve, juce::PathStrokeType(1.6f, joint));

  // Whatever is under the mouse, or being dragged, is drawn larger.
  const auto activeNode = draggedNode >= 0 ? draggedNode : hoveredNode;
  const auto activeHandle = draggedHandle >= 0 ? draggedHandle : hoveredHandle;
  const auto dot = [](juce::Point<float> centre, float radius) {
    return juce::Rectangle<float>(radius * 2.0f, radius * 2.0f)
        .withCentre(centre);
  };

  for (int i = 0; i < (int)shape.points.size() - 1; ++i)
    if (hasHandle(i)) {
      const auto active = i == activeHandle;
      const auto bounds =
          dot(handlePosition(i), active ? nodeRadius + 1.5f : nodeRadius);
      g.setColour(Theme::background);
      g.fillEllipse(bounds);
      g.setColour(active ? Theme::text : Theme::accent);
      g.drawEllipse(bounds, 1.2f);
    }

  for (int i = 0; i < (int)shape.points.size(); ++i) {
    const auto &p = shape.points[(size_t)i];
    const auto active = i == activeNode;
    const auto radius = active ? nodeRadius + 1.5f : nodeRadius;
    const auto centre = toScreen(p.x, p.y);
    g.setColour(Theme::background);
    g.fillEllipse(dot(centre, radius + 1.5f));
    g.setColour(active ? Theme::text : Theme::accent);
    g.fillEllipse(dot(centre, radius));
  }

  // Playhead
  const auto phase = processorRef.viewPhase.load();
  const auto head = toScreen(phase, shape.evaluate(phase));
  g.setColour(Theme::accent.withAlpha(0.5f));
  g.drawVerticalLine(juce::roundToInt(head.x), area.getY(), area.getBottom());
  g.setColour(Theme::accent.withAlpha(0.3f));
  g.fillEllipse(dot(head, 5.5f));
  g.setColour(juce::Colours::white);
  g.fillEllipse(dot(head, 2.5f));
}

// Tempo-synced oscilloscope: the dry input in grey under the output in red.
void GraphEditor::paintScope(juce::Graphics &g) {
  const auto area = getPlotArea();
  const auto size = AudioPluginAudioProcessor::scopeSize;

  auto trace = [&](const AudioPluginAudioProcessor::ScopeTrace &source) {
    auto point = [&](int i, float level) {
      return juce::Point<float>(
          area.getX() + area.getWidth() * ((float)i + 0.5f) / (float)size,
          area.getCentreY() -
              juce::jlimit(-1.0f, 1.0f, level) * area.getHeight() * 0.5f);
    };

    juce::Path path;
    path.startNewSubPath(point(0, source.high[0].load()));
    for (int i = 1; i < size; ++i)
      path.lineTo(point(i, source.high[(size_t)i].load()));
    for (int i = size; --i >= 0;)
      path.lineTo(point(i, source.low[(size_t)i].load()));
    path.closeSubPath();
    return path;
  };

  // Both colours, alpha included, come from the config file.
  const auto &config = PluginConfig::get();
  const juce::PathStrokeType stroke(1.0f);

  const auto input = trace(processorRef.scopeIn);
  g.setColour(config.bypassColour);
  g.fillPath(input);
  g.strokePath(input, stroke);

  const auto output = trace(processorRef.scopeOut);
  g.setColour(config.scopeColour);
  g.fillPath(output);
  g.strokePath(output, stroke);
}

// What the filter is doing right now, LFO modulation included: the response
// is measured by running an impulse through a copy of the filter.
void GraphEditor::paintFilterResponse(juce::Graphics &g) {
  const auto area = getPlotArea();
  const auto size = 1 << responseOrder;
  const auto sampleRate =
      processorRef.getSampleRate() > 0.0 ? processorRef.getSampleRate() : 44100.0;

  if (!juce::approximatelyEqual(sampleRate, responseRate)) {
    responseRate = sampleRate;
    responseFilter.prepare(sampleRate);
  }

  using Processor = AudioPluginAudioProcessor;
  const auto coeffs = FilterEngine::makeCoeffs(
      juce::roundToInt(filterTypeParam->load()),
      20.0f * std::pow(1000.0f, processorRef.finalValues[Processor::destCut].load()),
      processorRef.finalValues[Processor::destRes].load(),
      processorRef.finalValues[Processor::destVar].load(), sampleRate);
  const auto mix = mixParam->load() * 0.01f;
  const auto impulse = 0.25f;

  responseFilter.reset();
  std::fill(responseData.begin(), responseData.end(), 0.0f);
  for (int i = 0; i < size; ++i) {
    const auto x = i == 0 ? impulse : 0.0f;
    responseData[(size_t)i] =
        (x + mix * (responseFilter.process(x, coeffs) - x)) / impulse;
  }
  responseFft.performFrequencyOnlyForwardTransform(responseData.data());

  const auto yForDb = [&](float db) {
    return area.getCentreY() -
           juce::jlimit(-1.0f, 1.0f, db / responseRangeDb) * area.getHeight() *
               0.5f;
  };
  const auto xForFreq = [&](float freq) {
    return area.getX() +
           area.getWidth() * std::log(freq / 20.0f) / std::log(1000.0f);
  };

  // Grid: 12 dB rows, log frequency columns with the decades picked out.
  for (float db = -24.0f; db <= 24.0f; db += 12.0f) {
    g.setColour(juce::Colours::white.withAlpha(
        juce::exactlyEqual(db, 0.0f) ? 0.0f : 0.06f));
    g.drawHorizontalLine(juce::roundToInt(yForDb(db)), area.getX(),
                         area.getRight());
  }
  for (float decade = 10.0f; decade < 20000.0f; decade *= 10.0f)
    for (int step = 1; step < 10; ++step) {
      const auto freq = decade * (float)step;
      if (freq <= 20.0f || freq >= 20000.0f)
        continue;
      g.setColour(juce::Colours::white.withAlpha(step == 1 ? 0.12f : 0.045f));
      g.drawVerticalLine(juce::roundToInt(xForFreq(freq)), area.getY(),
                         area.getBottom());
    }

  const auto steps = juce::roundToInt(area.getWidth());
  const auto zero = yForDb(0.0f);
  juce::Path curve;
  for (int i = 0; i <= steps; ++i) {
    const auto freq = 20.0f * std::pow(1000.0f, (float)i / (float)steps);
    const auto bin = juce::jlimit(0.0f, (float)(size / 2 - 2),
                                  freq / (float)sampleRate * (float)size);
    const auto index = (size_t)bin;
    const auto magnitude =
        responseData[index] +
        (bin - (float)index) * (responseData[index + 1] - responseData[index]);
    const juce::Point<float> point(
        area.getX() + (float)i,
        yForDb(juce::Decibels::gainToDecibels(magnitude, -100.0f)));

    if (i == 0)
      curve.startNewSubPath(point);
    else
      curve.lineTo(point);
  }

  auto fill = curve;
  fill.lineTo(area.getRight(), zero);
  fill.lineTo(area.getX(), zero);
  fill.closeSubPath();
  g.setColour(responseColour.withAlpha(0.3f));
  g.fillPath(fill);
  g.drawHorizontalLine(juce::roundToInt(zero), area.getX(), area.getRight());
  g.setColour(responseColour);
  g.strokePath(curve, juce::PathStrokeType(1.5f));
}

// LFO output meter down the right-hand edge.
void GraphEditor::paintMeter(juce::Graphics &g) {
  const auto area = getPlotArea();
  const auto track = juce::Rectangle<float>(area.getRight() + 3.0f, area.getY(),
                                            meterWidth, area.getHeight());
  g.setColour(Theme::track);
  g.fillRoundedRectangle(track, 1.5f);
  g.setColour(Theme::accent);
  g.fillRoundedRectangle(
      track.withTop(track.getBottom() -
                    track.getHeight() *
                        juce::jlimit(0.0f, 1.0f, processorRef.viewValue.load())),
      1.5f);
}

//==============================================================================
void GraphEditor::mouseMove(const juce::MouseEvent &e) {
  hoveredNode = nodeAt(e.position);
  hoveredHandle = hoveredNode < 0 ? handleAt(e.position) : -1;
}

void GraphEditor::mouseExit(const juce::MouseEvent &) {
  hoveredNode = hoveredHandle = -1;
}

void GraphEditor::mouseDown(const juce::MouseEvent &e) {
  if (getDisplay() == AudioPluginAudioProcessor::displayFilter)
    return;

  auto &points = shape.points;
  const auto node = nodeAt(e.position);
  const auto lastNode = (int)points.size() - 1;

  if (e.mods.isPopupMenu()) {
    if (node > 0 && node < lastNode) {
      points.erase(points.begin() + node);
      commit();
    }
    return;
  }

  if (node >= 0) {
    draggedNode = node;
    return;
  }

  draggedHandle = handleAt(e.position);
  if (draggedHandle >= 0)
    return;

  if (e.mods.isShiftDown() && getSnap() > 0) {
    // A flat step spanning the grid cell under the mouse.
    const auto snap = (float)getSnap();
    const auto position = fromScreen(e.position);
    const auto cell = juce::jmin(snap - 1.0f, std::floor(position.x * snap));
    const auto x0 = cell / snap;
    const auto x1 = (cell + 1.0f) / snap;

    points.erase(std::remove_if(points.begin() + 1, points.end() - 1,
                                [=](const LfoPoint &p) {
                                  return p.x > x0 && p.x < x1;
                                }),
                 points.end() - 1);

    auto insertAt = std::upper_bound(
        points.begin() + 1, points.end() - 1, x0,
        [](float x, const LfoPoint &p) { return x < p.x; });
    insertAt = points.insert(insertAt, {x0, position.y, 0.0f});
    points.insert(insertAt + 1, {x1, position.y, 0.0f});
    commit();
  }
}

void GraphEditor::mouseDrag(const juce::MouseEvent &e) {
  auto &points = shape.points;

  if (draggedNode >= 0) {
    auto position = fromScreen(e.position);

    if (e.mods.isAltDown()) {
      if (const auto snap = (float)getSnap(); snap > 0.0f)
        position.x = std::round(position.x * snap) / snap;
      position.y = std::round(position.y * (float)ySnapSteps) / (float)ySnapSteps;
    }

    const auto lastNode = (int)points.size() - 1;
    auto &point = points[(size_t)draggedNode];
    point.y = position.y;

    if (draggedNode == 0 || draggedNode == lastNode) {
      // The ends stay put in time and share a level so the cycle loops cleanly.
      points.front().y = points.back().y = position.y;
    } else {
      point.x = juce::jlimit(points[(size_t)draggedNode - 1].x,
                             points[(size_t)draggedNode + 1].x, position.x);
    }

    commit();
  } else if (draggedHandle >= 0) {
    auto &a = points[(size_t)draggedHandle];
    const auto &b = points[(size_t)draggedHandle + 1];
    const auto height = b.y - a.y;
    if (std::abs(height) > 1.0e-4f) {
      a.curve = LfoShape::curveFromMidFraction(
          (fromScreen(e.position).y - a.y) / height);

      // Alt bends every segment by the same amount.
      if (e.mods.isAltDown())
        for (auto &point : points)
          point.curve = a.curve;

      commit();
    }
  }
}

void GraphEditor::mouseUp(const juce::MouseEvent &e) {
  draggedNode = draggedHandle = -1;
  mouseMove(e);
}

void GraphEditor::mouseDoubleClick(const juce::MouseEvent &e) {
  if (e.mods.isPopupMenu() ||
      getDisplay() == AudioPluginAudioProcessor::displayFilter)
    return;

  auto &points = shape.points;
  const auto node = nodeAt(e.position);
  draggedNode = draggedHandle = -1;

  if (node >= 0) {
    if (node > 0 && node < (int)points.size() - 1)
      points.erase(points.begin() + node);
  } else if (const auto handle = handleAt(e.position); handle >= 0) {
    points[(size_t)handle].curve = 0.0f;
  } else {
    const auto position = fromScreen(e.position);
    const auto insertAt = std::upper_bound(
        points.begin() + 1, points.end() - 1, position.x,
        [](float x, const LfoPoint &p) { return x < p.x; });
    points.insert(insertAt, {position.x, position.y, 0.0f});
  }

  commit();
  mouseMove(e);
}
