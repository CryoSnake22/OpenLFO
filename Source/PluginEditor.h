#pragma once

#include "GraphEditor.h"
#include "LookAndFeel.h"
#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

//==============================================================================
// Small flat button that draws one of the panel's icons, or a text caption.
class IconButton final : public juce::Button {
public:
  enum class Icon {
    text,
    power,
    note,
    anchor,
    copy,
    paste,
    save,
    lock,
    thru,
    scope,
    band,
    previous,
    next
  };

  explicit IconButton(Icon icon, const juce::String &caption = {});

  void setCaption(const juce::String &newCaption);
  void setOnColour(juce::Colour newColour);
  void setOffColour(juce::Colour newColour);

  // Called once when the mouse starts dragging away from the button.
  std::function<void()> onDrag;

  void paintButton(juce::Graphics &, bool highlighted, bool down) override;
  void mouseDrag(const juce::MouseEvent &) override;
  void mouseUp(const juce::MouseEvent &) override;

private:
  Icon icon;
  bool dragStarted = false;
  juce::String caption;
  juce::Colour onColour = Theme::accent;
  juce::Colour offColour = Theme::well;
};

//==============================================================================
// The synced Rate slider only stops on the lengths its filter allows, so the
// dot / triplet switches can hide those rates.
class RateSlider final : public juce::Slider {
public:
  std::function<bool(int index)> isAllowed;

  double snapValue(double attemptedValue, DragMode) override;
};

//==============================================================================
// Shows a parameter as text; drag up / down to change it (shift for fine).
class NumberBox final : public juce::Component {
public:
  NumberBox(juce::RangedAudioParameter &, bool drawBackground);

  void paint(juce::Graphics &) override;
  void mouseDown(const juce::MouseEvent &) override;
  void mouseDrag(const juce::MouseEvent &) override;
  void mouseUp(const juce::MouseEvent &) override;
  void mouseDoubleClick(const juce::MouseEvent &) override;

private:
  juce::RangedAudioParameter &parameter;
  juce::ParameterAttachment attachment;
  float dragStart = 0.0f;
  bool drawBackground;
};

//==============================================================================
// The whole interface, laid out at a fixed size and scaled by the editor.
class MainPanel final : public juce::Component,
                        private juce::Timer,
                        private juce::ChangeListener {
public:
  static constexpr int baseWidth = 708;
  static constexpr int baseHeight = 348;

  explicit MainPanel(AudioPluginAudioProcessor &);
  ~MainPanel() override;

  void paint(juce::Graphics &) override;

private:
  using APVTS = juce::AudioProcessorValueTreeState;

  struct StaticText {
    juce::String text;
    juce::Rectangle<int> area;
    juce::Justification justification;
    bool boxed;
  };

  void timerCallback() override;
  void changeListenerCallback(juce::ChangeBroadcaster *) override;

  void addText(const juce::String &text, juce::Rectangle<int> area,
               juce::Justification justification = juce::Justification::centredLeft,
               bool boxed = false);
  void addSlider(juce::Slider &, const juce::String &paramID,
                 juce::Rectangle<int> area, const juce::String &help);
  void addToggle(IconButton &, const juce::String &paramID,
                 juce::Rectangle<int> area, const juce::String &help);
  void addCombo(juce::ComboBox &, const juce::String &paramID,
                juce::Rectangle<int> area, const juce::String &help);
  void addButton(IconButton &, juce::Rectangle<int> area,
                 const juce::String &help);
  void addValueLabel(juce::Label &, juce::Slider &, juce::Rectangle<int> area);
  std::unique_ptr<NumberBox> addNumberBox(const juce::String &paramID,
                                          juce::Rectangle<int> area,
                                          bool drawBackground,
                                          const juce::String &help);

  void selectGraph(int graph);
  void showMultiplierMenu();
  void showShapeMenu();
  void showPresetMenu();
  void saveShape();
  void savePreset();
  void stepPreset(int delta);
  void copyShape();
  void pasteShape();
  void cycleChoice(const juce::String &paramID);
  void dragMidiClip();
  juce::Array<juce::File> findFiles(const juce::File &folder,
                                    const juce::String &pattern,
                                    bool recursive = false) const;

  //==============================================================================
  AudioPluginAudioProcessor &processorRef;
  APVTS &apvts;

  std::vector<StaticText> texts;
  juce::String helpText;
  int shownFilterType = -1;
  int shownRetrigMode = -1;
  std::array<float, AudioPluginAudioProcessor::numDests> shownFinals{};

  // Filter
  IconButton filterPower{IconButton::Icon::power};
  juce::ComboBox filterType;
  juce::Slider cutoff, reso, mix, var, drive;
  std::unique_ptr<NumberBox> offsetBox;
  IconButton displayButton{IconButton::Icon::scope};

  // MIDI
  IconButton retrigButton{IconButton::Icon::note};
  IconButton gateButton{IconButton::Icon::note};
  IconButton velPwmButton{IconButton::Icon::note};
  IconButton noteRateButton{IconButton::Icon::note};
  IconButton noteCutoffButton{IconButton::Icon::note};
  std::unique_ptr<NumberBox> ccBox;
  IconButton lockButton{IconButton::Icon::lock};
  IconButton midiDragButton{IconButton::Icon::thru};

  // Graph
  std::array<std::unique_ptr<IconButton>, AudioPluginAudioProcessor::numGraphs>
      tabs;
  IconButton multButton{IconButton::Icon::text, "1x"};
  IconButton copyButton{IconButton::Icon::copy};
  IconButton pasteButton{IconButton::Icon::paste};
  IconButton saveShapeButton{IconButton::Icon::save};
  IconButton shapeButton{IconButton::Icon::text, "shape"};
  GraphEditor graph;

  // LFO
  IconButton anchorButton{IconButton::Icon::anchor};
  IconButton dottedButton{IconButton::Icon::text, "."};
  IconButton tripletButton{IconButton::Icon::text, "3"};
  IconButton syncButton{IconButton::Icon::note};
  RateSlider rateSync;
  juce::Slider rateHz, swing, phase, pwm, smooth;
  juce::Label rateValue, swingValue, phaseValue, pwmValue, smoothValue;
  std::unique_ptr<NumberBox> snapBox;

  // Presets
  IconButton presetButton{IconButton::Icon::text, "- Presets -"};
  IconButton previousButton{IconButton::Icon::previous};
  IconButton nextButton{IconButton::Icon::next};
  IconButton savePresetButton{IconButton::Icon::save};

  // Routing
  std::array<juce::ComboBox, AudioPluginAudioProcessor::numDests> routeGraph;
  std::array<juce::Slider, AudioPluginAudioProcessor::numDests> routeAmount;
  std::array<juce::Label, AudioPluginAudioProcessor::numDests> routeValue;
  juce::Slider depth;
  juce::Label depthValue;

  // Split
  IconButton splitPower{IconButton::Icon::power};
  IconButton splitBand{IconButton::Icon::band};
  juce::Slider splitFreq;
  juce::Label splitValue;

  std::vector<std::pair<juce::Slider *, juce::Label *>> valueLabels;
  std::vector<std::unique_ptr<APVTS::SliderAttachment>> sliderAttachments;
  std::vector<std::unique_ptr<APVTS::ButtonAttachment>> buttonAttachments;
  std::vector<std::unique_ptr<APVTS::ComboBoxAttachment>> comboAttachments;
  std::unique_ptr<juce::FileChooser> fileChooser;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainPanel)
};

//==============================================================================
class AudioPluginAudioProcessorEditor final
    : public juce::AudioProcessorEditor {
public:
  explicit AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor &);
  ~AudioPluginAudioProcessorEditor() override;

  //==============================================================================
  void paint(juce::Graphics &) override;
  void resized() override;

private:
  // This reference is provided as a quick way for your editor to
  // access the processor object that created it.
  AudioPluginAudioProcessor &processorRef;

  LfoLookAndFeel lookAndFeel;
  MainPanel panel;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
