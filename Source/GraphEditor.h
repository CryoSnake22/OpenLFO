#pragma once

#include "PluginProcessor.h"

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// The point + tension-curve editor for one of the processor's LFO graphs.
//
//   double-click empty space   add a node
//   double-click a node        remove it (also ctrl / right-click)
//   drag a node                move it (hold alt to snap to the grid)
//   drag a hollow handle       bend the segment (alt bends every segment,
//                              double-click straightens)
//   shift-click empty space    add a flat step across the clicked grid cell
//
// The same area doubles as the oscilloscope and the filter response display.
class GraphEditor final : public juce::Component, private juce::Timer {
public:
  explicit GraphEditor(AudioPluginAudioProcessor &);

  void setGraph(int graph);
  int getGraph() const { return graph; }

  // Re-reads the graph from the processor, e.g. after a preset load.
  void refresh();

  const LfoShape &getShape() const { return shape; }
  void setShape(const LfoShape &newShape);

  //==============================================================================
  void paint(juce::Graphics &) override;
  void mouseMove(const juce::MouseEvent &) override;
  void mouseExit(const juce::MouseEvent &) override;
  void mouseDown(const juce::MouseEvent &) override;
  void mouseDrag(const juce::MouseEvent &) override;
  void mouseUp(const juce::MouseEvent &) override;
  void mouseDoubleClick(const juce::MouseEvent &) override;

private:
  void timerCallback() override;

  juce::Rectangle<float> getPlotArea() const;
  juce::Point<float> toScreen(float x, float y) const;
  juce::Point<float> fromScreen(juce::Point<float> position) const;
  juce::Point<float> handlePosition(int segment) const;
  bool hasHandle(int segment) const;
  int nodeAt(juce::Point<float> position) const;
  int handleAt(juce::Point<float> position) const;
  int getSnap() const;
  juce::Path createCurve() const;
  juce::Path createOutputFill() const;
  void commit();

  int getDisplay() const;
  void paintGraph(juce::Graphics &);
  void paintScope(juce::Graphics &);
  void paintFilterResponse(juce::Graphics &);
  void paintMeter(juce::Graphics &);

  AudioPluginAudioProcessor &processorRef;
  std::atomic<float> *snapParam = nullptr;
  std::atomic<float> *displayParam = nullptr;
  std::atomic<float> *phaseParam = nullptr;
  std::atomic<float> *pwmParam = nullptr;
  std::atomic<float> *filterTypeParam = nullptr;
  std::atomic<float> *mixParam = nullptr;

  // The response view measures a private copy of the filter.
  static constexpr int responseOrder = 13;
  juce::dsp::FFT responseFft{responseOrder};
  std::vector<float> responseData;
  FilterEngine responseFilter;
  double responseRate = 0.0;

  LfoShape shape;
  int graph = 0;
  int draggedNode = -1;
  int draggedHandle = -1;
  int hoveredNode = -1;
  int hoveredHandle = -1;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraphEditor)
};
