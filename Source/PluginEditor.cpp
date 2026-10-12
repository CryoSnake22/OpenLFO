#include "PluginEditor.h"

namespace {
const juce::Identifier helpProp{"help"};
const juce::Colour finalDotColour{0xff3b9eff};
const juce::Colour filterViewColour{0xff4cc38a};
const juce::String shapeClipboardTag{"lfoshape:"};
const juce::String shapeExtension{".lfoshape"};
const juce::String presetExtension{".lfopreset"};

enum MenuID { initPreset = 1, savePresetAs, showPresetFolder, firstFile = 100 };

juce::PopupMenu::Options menuOptions(juce::Component &target) {
  return juce::PopupMenu::Options().withTargetComponent(target);
}

// The app icon in miniature: one LFO cycle in a graph well.
void drawLogo(juce::Graphics &g, juce::Rectangle<float> area) {
  g.setColour(Theme::well);
  g.fillRoundedRectangle(area, 4.0f);
  g.setColour(Theme::border);
  g.drawRoundedRectangle(area.reduced(0.5f), 4.0f, 1.0f);

  const auto plot = area.reduced(3.0f, 4.0f);
  const auto at = [&plot](float x, float y) {
    return plot.getRelativePoint(x, y);
  };
  juce::Path wave;
  wave.startNewSubPath(at(0.0f, 0.5f));
  wave.cubicTo(at(0.12f, 0.5f), at(0.14f, 0.0f), at(0.25f, 0.0f));
  wave.cubicTo(at(0.4f, 0.0f), at(0.37f, 1.0f), at(0.56f, 1.0f));
  wave.cubicTo(at(0.7f, 1.0f), at(0.78f, 0.5f), at(1.0f, 0.5f));
  g.setColour(Theme::accent);
  g.strokePath(wave, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
}
} // namespace

//==============================================================================
IconButton::IconButton(Icon iconToUse, const juce::String &captionToUse)
    : juce::Button({}), icon(iconToUse), caption(captionToUse) {}

void IconButton::setCaption(const juce::String &newCaption) {
  if (caption != newCaption) {
    caption = newCaption;
    repaint();
  }
}

void IconButton::setOnColour(juce::Colour newColour) {
  if (onColour != newColour) {
    onColour = newColour;
    repaint();
  }
}

void IconButton::setOffColour(juce::Colour newColour) {
  offColour = newColour;
}

void IconButton::paintButton(juce::Graphics &g, bool highlighted, bool down) {
  const auto bounds = getLocalBounds().toFloat();
  const auto on = getToggleState() && icon != Icon::band;

  const auto shape = bounds.reduced(0.5f);
  g.setColour(on ? onColour.withAlpha(0.2f) : offColour);
  g.fillRoundedRectangle(shape, 4.0f);

  if (down || highlighted) {
    g.setColour(juce::Colours::white.withAlpha(down ? 0.1f : 0.05f));
    g.fillRoundedRectangle(shape, 4.0f);
  }

  if (on || !offColour.isTransparent()) {
    g.setColour(on ? onColour.withAlpha(0.7f) : Theme::border);
    g.drawRoundedRectangle(shape, 4.0f, 1.0f);
  }

  g.setColour(on ? Theme::text : Theme::textDim.brighter(0.3f));

  const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight()) - 5.0f;
  const auto r = juce::Rectangle<float>(side, side).withCentre(bounds.getCentre());
  const juce::PathStrokeType stroke(1.2f);
  juce::Path path;

  switch (icon) {
  case Icon::text:
    g.setFont(Theme::font(juce::jmin(11.0f, bounds.getHeight() - 2.0f)));
    g.drawFittedText(caption, getLocalBounds().reduced(2, 0),
                     juce::Justification::centred, 1, 0.8f);
    break;

  case Icon::power:
    path.addCentredArc(r.getCentreX(), r.getCentreY(), r.getWidth() * 0.5f,
                       r.getHeight() * 0.5f, 0.0f, 0.6f,
                       juce::MathConstants<float>::twoPi - 0.6f, true);
    path.startNewSubPath(r.getCentreX(), r.getY() - 1.0f);
    path.lineTo(r.getCentreX(), r.getCentreY());
    g.strokePath(path, stroke);
    break;

  case Icon::note:
    g.fillEllipse(r.getX(), r.getBottom() - r.getHeight() * 0.42f,
                  r.getWidth() * 0.5f, r.getHeight() * 0.42f);
    path.startNewSubPath(r.getX() + r.getWidth() * 0.5f - 0.6f,
                         r.getBottom() - r.getHeight() * 0.2f);
    path.lineTo(r.getX() + r.getWidth() * 0.5f - 0.6f, r.getY());
    path.lineTo(r.getRight(), r.getY() + r.getHeight() * 0.35f);
    g.strokePath(path, stroke);
    break;

  case Icon::anchor:
    path.addEllipse(r.getCentreX() - 1.5f, r.getY() - 1.0f, 3.0f, 3.0f);
    path.startNewSubPath(r.getCentreX(), r.getY() + 2.0f);
    path.lineTo(r.getCentreX(), r.getBottom());
    path.startNewSubPath(r.getX() + 1.5f, r.getY() + r.getHeight() * 0.4f);
    path.lineTo(r.getRight() - 1.5f, r.getY() + r.getHeight() * 0.4f);
    path.addCentredArc(r.getCentreX(), r.getCentreY(), r.getWidth() * 0.5f,
                       r.getHeight() * 0.5f, 0.0f,
                       juce::MathConstants<float>::halfPi,
                       juce::MathConstants<float>::pi * 1.5f, true);
    g.strokePath(path, stroke);
    break;

  case Icon::copy:
    g.drawRect(r.withTrimmedLeft(3.0f).withTrimmedBottom(3.0f), 1.0f);
    g.drawRect(r.withTrimmedRight(3.0f).withTrimmedTop(3.0f), 1.0f);
    break;

  case Icon::paste:
    g.drawRect(r.withTrimmedTop(1.5f), 1.0f);
    g.fillRect(r.withHeight(3.0f).reduced(2.5f, 0.0f));
    break;

  case Icon::save:
    g.fillRect(r);
    g.setColour(Theme::well);
    g.fillRect(r.withHeight(r.getHeight() * 0.35f).reduced(2.0f, 0.0f));
    g.fillRect(r.withTop(r.getCentreY() + 1.0f).reduced(1.5f, 1.0f));
    break;

  case Icon::lock:
    g.fillRect(r.withTop(r.getCentreY() - 0.5f));
    path.addCentredArc(r.getCentreX(), r.getCentreY() - 0.5f,
                       r.getWidth() * 0.28f, r.getHeight() * 0.42f, 0.0f,
                       -juce::MathConstants<float>::halfPi,
                       juce::MathConstants<float>::halfPi, true);
    g.strokePath(path, stroke);
    break;

  case Icon::thru:
    g.drawEllipse(r, 1.2f);
    for (const auto angle : {-1.0f, 0.0f, 1.0f})
      g.fillEllipse(juce::Rectangle<float>(1.8f, 1.8f)
                        .withCentre(r.getCentre().getPointOnCircumference(
                            r.getWidth() * 0.27f,
                            juce::MathConstants<float>::pi + angle)));
    break;

  case Icon::scope: {
    const auto wave = bounds.reduced(5.0f, 3.5f);
    for (int i = 0; i <= 24; ++i) {
      const auto t = (float)i / 24.0f;
      const juce::Point<float> p(
          wave.getX() + t * wave.getWidth(),
          wave.getCentreY() -
              std::sin(t * juce::MathConstants<float>::twoPi) *
                  wave.getHeight() * 0.5f);
      if (i == 0)
        path.startNewSubPath(p);
      else
        path.lineTo(p);
    }
    g.strokePath(path, stroke);
    break;
  }

  case Icon::band: {
    // The filled half is the band that gets processed.
    const auto area = bounds.reduced(1.5f);
    const auto high = getToggleState();
    path.startNewSubPath(high ? area.getBottomRight() : area.getBottomLeft());
    path.lineTo(high ? area.getTopRight() : area.getTopLeft());
    path.lineTo(area.getCentreX(), area.getY());
    path.lineTo(high ? area.getX() + 2.0f : area.getRight() - 2.0f,
                area.getBottom());
    path.closeSubPath();
    g.setColour(Theme::accent);
    g.fillPath(path);
    break;
  }

  case Icon::previous:
    path.addTriangle(r.getX(), r.getCentreY(), r.getRight(), r.getY(),
                     r.getRight(), r.getBottom());
    g.fillPath(path);
    break;

  case Icon::next:
    path.addTriangle(r.getRight(), r.getCentreY(), r.getX(), r.getY(),
                     r.getX(), r.getBottom());
    g.fillPath(path);
    break;
  }
}

void IconButton::mouseDrag(const juce::MouseEvent &e) {
  juce::Button::mouseDrag(e);

  // One drag per press.
  if (onDrag != nullptr && !dragStarted && e.getDistanceFromDragStart() > 6) {
    dragStarted = true;
    onDrag();
  }
}

void IconButton::mouseUp(const juce::MouseEvent &e) {
  dragStarted = false;
  juce::Button::mouseUp(e);
}

//==============================================================================
double RateSlider::snapValue(double attemptedValue, DragMode) {
  if (isAllowed == nullptr)
    return attemptedValue;

  // Nearest rate that the dot / triplet switches currently allow.
  const auto wanted = juce::roundToInt(attemptedValue);
  const auto last = juce::roundToInt(getMaximum());
  for (int distance = 0; distance <= last; ++distance)
    for (const auto index : {wanted - distance, wanted + distance})
      if (index >= 0 && index <= last && isAllowed(index))
        return index;

  return attemptedValue;
}

//==============================================================================
NumberBox::NumberBox(juce::RangedAudioParameter &p, bool shouldDrawBackground)
    : parameter(p), attachment(p, [this](float) { repaint(); }),
      drawBackground(shouldDrawBackground) {
  attachment.sendInitialUpdate();
  setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
}

void NumberBox::paint(juce::Graphics &g) {
  if (drawBackground) {
    const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(Theme::well);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(Theme::border);
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
  }

  g.setColour(Theme::text);
  g.setFont(Theme::font(11.0f));
  g.drawText(parameter.getCurrentValueAsText(), getLocalBounds(),
             juce::Justification::centred, false);
}

void NumberBox::mouseDown(const juce::MouseEvent &) {
  dragStart = parameter.getValue();
  attachment.beginGesture();
}

void NumberBox::mouseDrag(const juce::MouseEvent &e) {
  // Shift gives ten times the travel for fine adjustment.
  const auto travel = e.mods.isShiftDown() ? 1500.0f : 150.0f;
  const auto normalised = juce::jlimit(
      0.0f, 1.0f, dragStart - (float)e.getDistanceFromDragStartY() / travel);
  attachment.setValueAsPartOfGesture(parameter.convertFrom0to1(normalised));
}

void NumberBox::mouseUp(const juce::MouseEvent &) { attachment.endGesture(); }

void NumberBox::mouseDoubleClick(const juce::MouseEvent &) {
  attachment.setValueAsCompleteGesture(
      parameter.convertFrom0to1(parameter.getDefaultValue()));
}

//==============================================================================
MainPanel::MainPanel(AudioPluginAudioProcessor &p)
    : processorRef(p), apvts(p.apvts), graph(p) {
  using Dest = AudioPluginAudioProcessor;
  const auto centred = juce::Justification::centred;

  setSize(baseWidth, baseHeight);

  //============================================================================
  // Filter
  addToggle(filterPower, ParamID::filterOn, {10, 35, 14, 14},
            "Filter On - enables the filter");
  // The filter menu is long, so it is split into one submenu per family.
  {
    juce::PopupMenu section;
    juce::String sectionName;
    const auto names = FilterEngine::typeNames();
    for (int i = 0; i < names.size(); ++i) {
      const auto group = FilterEngine::groupName(i);
      if (i > 0 && group != sectionName) {
        filterType.getRootMenu()->addSubMenu(sectionName, section);
        section = {};
      }
      sectionName = group;
      section.addItem(i + 1, names[i]);
    }
    filterType.getRootMenu()->addSubMenu(sectionName, section);
  }
  addCombo(filterType, ParamID::filterType, {27, 35, 92, 14},
           "FilterType - select what filter to use. (to hear, it must be "
           "enabled/button to the left)");

  const std::array<int, 5> filterRows{61, 85, 110, 134, 158};
  addSlider(cutoff, ParamID::cutoff, {52, filterRows[0] - 6, 46, 12},
            "Cutoff - filter frequency");
  addSlider(reso, ParamID::reso, {52, filterRows[1] - 6, 46, 12},
            "Reso - filter resonance");
  addSlider(mix, ParamID::mix, {52, filterRows[2] - 6, 46, 12},
            "Mix - blend between the dry and the filtered signal");
  addSlider(var, ParamID::var, {52, filterRows[3] - 6, 46, 12},
            "Var - extra control whose job depends on the filter type");
  addSlider(drive, ParamID::drive, {52, filterRows[4] - 6, 46, 12},
            "Drive - saturation ahead of the filter");

  addText("Cutoff", {12, filterRows[0] - 7, 40, 14});
  addText("Reso", {12, filterRows[1] - 7, 40, 14});
  addText("Mix", {12, filterRows[2] - 7, 40, 14});
  addText("Drive", {12, filterRows[4] - 7, 40, 14});

  addText("offset", {12, 174, 36, 14});
  offsetBox = addNumberBox(ParamID::offset, {46, 174, 34, 14}, false,
                           "Offset - sync offset in samples, for hosts that "
                           "misreport latency. Drag, shift for fine");
  addButton(displayButton, {85, 173, 34, 16},
            "Display - click through: graph only, oscilloscope (input grey, "
            "output red), filter response");
  displayButton.onClick = [this] { cycleChoice(ParamID::display); };

  //============================================================================
  // MIDI
  addText("MIDI", {46, 197, 38, 12}, centred, true);

  const std::array<int, 5> midiRows{217, 236, 255, 274, 293};
  addButton(retrigButton, {20, midiRows[0] - 7, 14, 14},
            "Note Retrig - a new note restarts the LFO. Click again for Env: "
            "the graph plays once per note and stops");
  retrigButton.onClick = [this] { cycleChoice(ParamID::retrig); };
  addToggle(gateButton, ParamID::gate, {20, midiRows[1] - 7, 14, 14},
            "Note Gate - the LFO stops when the note is let go");
  addToggle(velPwmButton, ParamID::velPwm, {20, midiRows[2] - 7, 14, 14},
            "Vel->PWM - note velocity shifts the pulse width");
  addToggle(noteRateButton, ParamID::noteRate, {20, midiRows[3] - 7, 14, 14},
            "Note->Rate - the note number (0-127) sets the Rate slider");
  addToggle(noteCutoffButton, ParamID::noteCutoff,
            {20, midiRows[4] - 7, 14, 14},
            "Note->Cutoff - the note number (0-127) sets the Cutoff slider");

  addText("Note Gate", {42, midiRows[1] - 7, 80, 14});
  addText("Vel->PWM", {42, midiRows[2] - 7, 80, 14});
  addText("Note->Rate", {42, midiRows[3] - 7, 80, 14});
  addText("Note->Cutoff", {42, midiRows[4] - 7, 80, 14});

  ccBox = addNumberBox(ParamID::ccOut, {14, 310, 28, 14}, true,
                       "CC Out - sends the final cutoff value as this MIDI "
                       "CC. Drag up/down");
  addText("cc", {45, 310, 14, 14});
  addToggle(lockButton, ParamID::midiLock, {62, 310, 14, 14},
            "Lock - keep the CC number when changing presets");
  addButton(midiDragButton, {82, 310, 14, 14},
            "MIDI export - drag onto a MIDI track to drop this graph as a "
            "CC clip");
  midiDragButton.setOnColour(Theme::accent);
  midiDragButton.onDrag = [this] { dragMidiClip(); };

  //============================================================================
  // Graph
  for (int i = 0; i < Dest::numGraphs; ++i) {
    tabs[(size_t)i] = std::make_unique<IconButton>(IconButton::Icon::text,
                                                   juce::String(i + 1));
    auto &tab = *tabs[(size_t)i];
    tab.setOffColour(juce::Colours::transparentBlack);
    addButton(tab, {143 + 19 * i, 10, 18, 14},
              "Graph " + juce::String(i + 1) + " - select which graph to edit");
    tab.onClick = [this, i] { selectGraph(i); };
  }

  addButton(multButton, {386, 10, 28, 14},
            "Rate multiplier - how fast this graph runs relative to Rate");
  multButton.setOffColour(juce::Colours::transparentBlack);
  multButton.onClick = [this] { showMultiplierMenu(); };

  addButton(copyButton, {420, 10, 14, 14}, "Copy - copy this graph");
  copyButton.onClick = [this] { copyShape(); };
  addButton(pasteButton, {438, 10, 14, 14},
            "Paste - replace this graph with the copied one");
  pasteButton.onClick = [this] { pasteShape(); };
  addButton(saveShapeButton, {456, 10, 14, 14},
            "Save - save this graph to the shape menu");
  saveShapeButton.onClick = [this] { saveShape(); };
  addButton(shapeButton, {476, 10, 72, 14},
            "Shape - load a preset shape into this graph");
  shapeButton.onClick = [this] { showShapeMenu(); };

  graph.setBounds(141, 29, 408, 246);
  graph.getProperties().set(
      helpProp, "Double-click: add/remove point. Drag hollow handle: curve. "
                "Alt-drag: snap. Shift-click: add step");
  addAndMakeVisible(graph);

  //============================================================================
  // LFO
  addToggle(anchorButton, ParamID::anchor, {168, 285, 14, 14},
            "Anchor - lock the LFO to the host's song position");
  addToggle(dottedButton, ParamID::dotted, {138, 303, 14, 14},
            "Dotted - include dotted lengths in the Rate control");
  addToggle(tripletButton, ParamID::triplet, {153, 303, 14, 14},
            "Triplet - include triplet lengths in the Rate control");
  addToggle(syncButton, ParamID::sync, {168, 303, 14, 14},
            "BPM Sync - Rate in note lengths (on) or Hz (off)");

  const std::array<int, 5> columns{218, 284, 350, 417, 484};
  const auto sliderArea = [](int centre) {
    return juce::Rectangle<int>(centre - 23, 301, 46, 12);
  };
  const auto valueArea = [](int centre) {
    return juce::Rectangle<int>(centre - 28, 315, 56, 12);
  };

  addSlider(rateSync, ParamID::rateSync, sliderArea(columns[0]),
            "Rate - length of one LFO cycle");
  addSlider(rateHz, ParamID::rateHz, sliderArea(columns[0]),
            "Rate - LFO speed in Hz");
  addSlider(swing, ParamID::swing, sliderArea(columns[1]),
            "Swing - alternates faster / slower cycles (needs BPM Sync and "
            "Anchor)");
  addSlider(phase, ParamID::phase, sliderArea(columns[2]),
            "Phase - start position within the graph");
  addSlider(pwm, ParamID::pwm, sliderArea(columns[3]),
            "PWM - squeezes the graph towards its start or end");
  addSlider(smooth, ParamID::smooth, sliderArea(columns[4]),
            "Smooth - rounds off sharp edges in the LFO");

  rateSync.isAllowed = [this](int index) {
    using Kind = AudioPluginAudioProcessor::RateEntry;
    const auto kind =
        AudioPluginAudioProcessor::rateEntries()[(size_t)index].kind;
    const auto *filter = kind == Kind::dotted    ? ParamID::dotted
                         : kind == Kind::triplet ? ParamID::triplet
                                                 : nullptr;
    return filter == nullptr || apvts.getRawParameterValue(filter)->load() > 0.5f;
  };

  addValueLabel(rateValue, rateSync, valueArea(columns[0]));
  addValueLabel(swingValue, swing, valueArea(columns[1]));
  addValueLabel(phaseValue, phase, valueArea(columns[2]));
  addValueLabel(pwmValue, pwm, valueArea(columns[3]));
  addValueLabel(smoothValue, smooth, valueArea(columns[4]));

  const std::array<const char *, 5> columnNames{"Rate", "Swing", "Phase",
                                                "PWM", "Smooth"};
  for (size_t i = 0; i < columns.size(); ++i)
    addText(columnNames[i], {columns[i] - 28, 286, 56, 12}, centred);

  addText("Snap", {518, 286, 36, 12}, centred);
  snapBox = addNumberBox(ParamID::snap, {522, 301, 28, 14}, true,
                         "Snap - grid divisions. Drag up/down");

  //============================================================================
  // Presets
  addButton(presetButton, {569, 8, 74, 18}, "Presets - load or save a preset");
  presetButton.onClick = [this] { showPresetMenu(); };
  addButton(previousButton, {646, 8, 15, 18}, "Previous preset");
  previousButton.onClick = [this] { stepPreset(-1); };
  addButton(nextButton, {663, 8, 15, 18}, "Next preset");
  nextButton.onClick = [this] { stepPreset(1); };
  addButton(savePresetButton, {681, 8, 16, 18}, "Save preset");
  savePresetButton.onClick = [this] { savePreset(); };

  //============================================================================
  // Routing
  addText("LFO ROUTING", {590, 37, 86, 13}, centred, true);

  const std::array<const char *, Dest::numDests> destHelp{
      "filter cutoff", "filter resonance", "volume", "pan",
      "the filter's Var control"};
  for (int d = 0; d < Dest::numDests; ++d) {
    const auto row = 68 + 34 * d;
    const auto index = (size_t)d;
    const juce::String target = destHelp[index];

    addCombo(routeGraph[index], Dest::destGraphID(d), {576, row - 7, 36, 14},
             "Graph - which graph modulates " + target);
    addSlider(routeAmount[index], Dest::destAmountID(d),
              {619, row - 6, 46, 12}, "Amount - how much the LFO moves " + target);
    addValueLabel(routeValue[index], routeAmount[index],
                  {614, row + 7, 56, 11});
    addText(Dest::destName(d), {671, row - 8, 26, 14});
  }

  addText("depth", {576, 240, 40, 14});
  addSlider(depth, ParamID::depth, {619, 241, 46, 12},
            "Depth - scales every routing amount");
  addValueLabel(depthValue, depth, {667, 240, 30, 14});

  //============================================================================
  // Split
  addToggle(splitPower, ParamID::splitOn, {574, 272, 14, 14},
            "Split On - only process one side of the crossover");
  addText("SPLIT", {603, 272, 56, 14}, centred, true);
  addToggle(splitBand, ParamID::splitHigh, {671, 272, 16, 14},
            "Band - process the lows or the highs");
  addText("freq.", {573, 301, 32, 14});
  addSlider(splitFreq, ParamID::splitFreq, {607, 302, 46, 12},
            "Freq - crossover frequency");
  addValueLabel(splitValue, splitFreq, {656, 301, 40, 14});

  AudioPluginAudioProcessor::installFactoryPresets();
  selectGraph(processorRef.viewedGraph.load());
  processorRef.addChangeListener(this);
  timerCallback();
  startTimer(PluginConfig::get().redrawMs);
}

MainPanel::~MainPanel() { processorRef.removeChangeListener(this); }

//==============================================================================
void MainPanel::addText(const juce::String &text, juce::Rectangle<int> area,
                        juce::Justification justification, bool boxed) {
  texts.push_back({text, area, justification, boxed});
}

void MainPanel::addSlider(juce::Slider &slider, const juce::String &paramID,
                          juce::Rectangle<int> area, const juce::String &help) {
  slider.setSliderStyle(juce::Slider::LinearHorizontal);
  slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
  slider.setBounds(area);
  slider.setRepaintsOnMouseActivity(true);
  slider.getProperties().set(helpProp, help);
  addAndMakeVisible(slider);
  sliderAttachments.push_back(
      std::make_unique<APVTS::SliderAttachment>(apvts, paramID, slider));

  if (const auto *parameter = apvts.getParameter(paramID))
    slider.setDoubleClickReturnValue(
        true, parameter->convertFrom0to1(parameter->getDefaultValue()));
}

void MainPanel::addToggle(IconButton &button, const juce::String &paramID,
                          juce::Rectangle<int> area, const juce::String &help) {
  addButton(button, area, help);
  button.setClickingTogglesState(true);
  buttonAttachments.push_back(
      std::make_unique<APVTS::ButtonAttachment>(apvts, paramID, button));
}

void MainPanel::addCombo(juce::ComboBox &box, const juce::String &paramID,
                         juce::Rectangle<int> area, const juce::String &help) {
  if (const auto *choice = dynamic_cast<juce::AudioParameterChoice *>(
          apvts.getParameter(paramID));
      choice != nullptr && box.getNumItems() == 0)
    box.addItemList(choice->choices, 1);

  box.setBounds(area);
  box.getProperties().set(helpProp, help);
  addAndMakeVisible(box);
  comboAttachments.push_back(
      std::make_unique<APVTS::ComboBoxAttachment>(apvts, paramID, box));
}

void MainPanel::addButton(IconButton &button, juce::Rectangle<int> area,
                          const juce::String &help) {
  button.setBounds(area);
  button.getProperties().set(helpProp, help);
  addAndMakeVisible(button);
}

void MainPanel::addValueLabel(juce::Label &label, juce::Slider &slider,
                              juce::Rectangle<int> area) {
  label.setBounds(area);
  label.setFont(Theme::font(10.5f));
  label.setColour(juce::Label::textColourId, Theme::textDim);
  label.setJustificationType(juce::Justification::centred);
  label.setInterceptsMouseClicks(false, false);
  label.setBorderSize({});
  addAndMakeVisible(label);
  valueLabels.push_back({&slider, &label});
}

std::unique_ptr<NumberBox>
MainPanel::addNumberBox(const juce::String &paramID, juce::Rectangle<int> area,
                        bool drawBackground, const juce::String &help) {
  auto box = std::make_unique<NumberBox>(*apvts.getParameter(paramID),
                                         drawBackground);
  box->setBounds(area);
  box->getProperties().set(helpProp, help);
  addAndMakeVisible(*box);
  return box;
}

//==============================================================================
void MainPanel::paint(juce::Graphics &g) {
  g.fillAll(Theme::background);

  drawLogo(g, {8.0f, 7.0f, 20.0f, 20.0f});
  juce::AttributedString title;
  title.setJustification(juce::Justification::centredLeft);
  title.append("Open", Theme::font(15.0f), Theme::textDim.brighter(0.4f));
  title.append("LFO", Theme::font(15.0f, true), Theme::text);
  title.draw(g, {34.0f, 7.0f, 90.0f, 20.0f});

  // Cards
  for (const auto &card :
       {juce::Rectangle<int>{7, 32, 116, 159}, {7, 194, 116, 136},
        {134, 3, 422, 277}, {134, 283, 422, 47}, {569, 32, 128, 230},
        {569, 268, 128, 62}}) {
    const auto bounds = card.toFloat().reduced(0.5f);
    g.setColour(Theme::panel);
    g.fillRoundedRectangle(bounds, 7.0f);
    g.setColour(Theme::border);
    g.drawRoundedRectangle(bounds, 7.0f, 1.0f);
  }

  // Rules that set the last row of a card apart from the rows above it.
  g.setColour(Theme::border);
  g.drawHorizontalLine(170, 15.0f, 115.0f);
  g.drawHorizontalLine(232, 577.0f, 689.0f);

  for (const auto &text : texts) {
    // Boxed entries are section headings, the rest are control labels.
    g.setColour(text.boxed ? Theme::textDim : Theme::text.withAlpha(0.85f));
    g.setFont(Theme::font(text.boxed ? 10.0f : 11.5f));
    g.drawText(text.text, text.area, text.justification, false);
  }

  // The 4th filter slider is named after what it does for this filter type.
  g.setColour(Theme::text.withAlpha(0.85f));
  g.setFont(Theme::font(11.5f));
  g.drawFittedText(FilterEngine::varName(shownFilterType), 12, 127, 39, 14,
                   juce::Justification::centredLeft, 1, 0.7f);

  // Note Retrig reads "Env" once it is in its play-once mode.
  g.drawText(shownRetrigMode == 2 ? "Env" : "Note Retrig", 42, 210, 80, 14,
             juce::Justification::centredLeft, false);

  // Dots above the routing sliders show where each destination has ended up
  // once the LFO has been added to its own slider.
  g.setColour(finalDotColour);
  for (size_t d = 0; d < shownFinals.size(); ++d)
    g.fillEllipse(juce::Rectangle<float>(3.0f, 3.0f)
                      .withCentre({623.0f + 38.0f * shownFinals[d],
                                   59.0f + 34.0f * (float)d}));

  g.setColour(Theme::textDim.withAlpha(helpText.isEmpty() ? 0.55f : 1.0f));
  g.setFont(Theme::font(11.0f));
  g.drawText(helpText.isEmpty()
                 ? "OpenLFO " JucePlugin_VersionString
                   "  -  hover over a control to see what it does"
                 : helpText,
             0, 331, baseWidth, 14, juce::Justification::centred, true);
}

//==============================================================================
void MainPanel::timerCallback() {
  auto raw = [this](const char *id) {
    return apvts.getRawParameterValue(id)->load();
  };

  // Help bar: describe whatever is under the mouse.
  juce::String help;
  for (auto *c = juce::Desktop::getInstance()
                     .getMainMouseSource()
                     .getComponentUnderMouse();
       c != nullptr && c != this; c = c->getParentComponent()) {
    if (c->getProperties().contains(helpProp)) {
      if (isParentOf(c)) {
        help = c->getProperties()[helpProp].toString();
        if (auto *slider = dynamic_cast<juce::Slider *>(c))
          help << "  [" << slider->getTextFromValue(slider->getValue()) << "]";
      }
      break;
    }
  }

  const auto filterTypeNow = juce::roundToInt(raw(ParamID::filterType));
  const auto retrigMode = juce::roundToInt(raw(ParamID::retrig));
  if (help != helpText || filterTypeNow != shownFilterType ||
      retrigMode != shownRetrigMode) {
    helpText = help;
    shownFilterType = filterTypeNow;
    shownRetrigMode = retrigMode;
    repaint();
  }

  // Rate shows note lengths or Hz depending on BPM Sync.
  const auto synced = raw(ParamID::sync) > 0.5f;
  rateSync.setVisible(synced);
  rateHz.setVisible(!synced);
  tripletButton.setEnabled(synced);
  dottedButton.setEnabled(synced);

  for (const auto &[slider, label] : valueLabels) {
    auto *source = slider == &rateSync && !synced ? &rateHz : slider;
    label->setText(source->getTextFromValue(source->getValue()),
                   juce::dontSendNotification);
  }

  retrigButton.setToggleState(retrigMode > 0, juce::dontSendNotification);
  retrigButton.setOnColour(retrigMode == 2 ? Theme::accentAlt : Theme::accent);

  const auto display = juce::roundToInt(raw(ParamID::display));
  displayButton.setToggleState(display > 0, juce::dontSendNotification);
  displayButton.setOnColour(display == AudioPluginAudioProcessor::displayFilter
                                ? filterViewColour
                                : Theme::accent);

  std::array<float, AudioPluginAudioProcessor::numDests> finals;
  for (size_t d = 0; d < finals.size(); ++d)
    finals[d] = processorRef.finalValues[d].load();
  if (finals != shownFinals) {
    shownFinals = finals;
    repaint(616, 55, 52, 144);
  }

  multButton.setCaption(AudioPluginAudioProcessor::multiplierText(
      processorRef.getMultiplier(graph.getGraph())));

  const auto presetName =
      processorRef.getPresetFile().getFileNameWithoutExtension();
  presetButton.setCaption(presetName.isEmpty() ? "- Presets -" : presetName);
}

void MainPanel::changeListenerCallback(juce::ChangeBroadcaster *) {
  graph.refresh();
}

//==============================================================================
void MainPanel::selectGraph(int newGraph) {
  graph.setGraph(newGraph);
  for (size_t i = 0; i < tabs.size(); ++i)
    tabs[i]->setToggleState((int)i == graph.getGraph(),
                            juce::dontSendNotification);
}

void MainPanel::cycleChoice(const juce::String &paramID) {
  if (auto *parameter = dynamic_cast<juce::AudioParameterChoice *>(
          apvts.getParameter(paramID))) {
    const auto next = (parameter->getIndex() + 1) % parameter->choices.size();
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1((float)next));
    parameter->endChangeGesture();
  }
}

void MainPanel::dragMidiClip() {
  const auto clip = processorRef.createMidiClip();
  if (clip.existsAsFile())
    juce::DragAndDropContainer::performExternalDragDropOfFiles(
        {clip.getFullPathName()}, false, &midiDragButton);
}

void MainPanel::showMultiplierMenu() {
  const auto &values = AudioPluginAudioProcessor::rateMultipliers();
  const auto current = processorRef.getMultiplier(graph.getGraph());

  juce::PopupMenu menu;
  menu.setLookAndFeel(&getLookAndFeel());
  for (size_t i = 0; i < values.size(); ++i)
    menu.addItem((int)i + 1,
                 AudioPluginAudioProcessor::multiplierText(values[i]), true,
                 juce::approximatelyEqual(values[i], current));

  menu.showMenuAsync(
      menuOptions(multButton),
      [safeThis = juce::Component::SafePointer<MainPanel>(this)](int result) {
        if (safeThis != nullptr && result > 0)
          safeThis->processorRef.setMultiplier(
              safeThis->graph.getGraph(),
              AudioPluginAudioProcessor::rateMultipliers()[(size_t)result - 1]);
      });
}

juce::Array<juce::File> MainPanel::findFiles(const juce::File &folder,
                                             const juce::String &pattern,
                                             bool recursive) const {
  auto files = folder.findChildFiles(juce::File::findFiles, recursive, pattern);
  files.sort();
  return files;
}

void MainPanel::showShapeMenu() {
  const auto userShapes = findFiles(AudioPluginAudioProcessor::getShapeFolder(),
                                    "*" + shapeExtension);

  juce::PopupMenu menu;
  menu.setLookAndFeel(&getLookAndFeel());
  const auto names = LfoShape::presetNames();
  for (int i = 0; i < names.size(); ++i)
    menu.addItem(i + 1, names[i]);

  if (!userShapes.isEmpty()) {
    menu.addSeparator();
    for (int i = 0; i < userShapes.size(); ++i)
      menu.addItem(firstFile + i, userShapes[i].getFileNameWithoutExtension());
  }

  menu.showMenuAsync(
      menuOptions(shapeButton),
      [safeThis = juce::Component::SafePointer<MainPanel>(this),
       userShapes](int result) {
        if (safeThis == nullptr || result <= 0)
          return;

        if (result < firstFile) {
          safeThis->graph.setShape(LfoShape::preset(result - 1));
        } else if (LfoShape shape; LfoShape::fromString(
                       userShapes[result - firstFile].loadFileAsString(),
                       shape)) {
          safeThis->graph.setShape(shape);
        }
      });
}

void MainPanel::saveShape() {
  const auto folder = AudioPluginAudioProcessor::getShapeFolder();
  folder.createDirectory();

  fileChooser = std::make_unique<juce::FileChooser>(
      "Save shape", folder.getChildFile("My Shape" + shapeExtension),
      "*" + shapeExtension);
  fileChooser->launchAsync(
      juce::FileBrowserComponent::saveMode |
          juce::FileBrowserComponent::canSelectFiles |
          juce::FileBrowserComponent::warnAboutOverwriting,
      [safeThis = juce::Component::SafePointer<MainPanel>(this)](
          const juce::FileChooser &chooser) {
        const auto file = chooser.getResult();
        if (safeThis != nullptr && file != juce::File())
          file.withFileExtension(shapeExtension)
              .replaceWithText(safeThis->graph.getShape().toString());
      });
}

void MainPanel::copyShape() {
  juce::SystemClipboard::copyTextToClipboard(shapeClipboardTag +
                                             graph.getShape().toString());
}

void MainPanel::pasteShape() {
  const auto text = juce::SystemClipboard::getTextFromClipboard();
  if (LfoShape shape;
      text.startsWith(shapeClipboardTag) &&
      LfoShape::fromString(text.fromFirstOccurrenceOf(shapeClipboardTag, false,
                                                      false),
                           shape))
    graph.setShape(shape);
}

//==============================================================================
void MainPanel::showPresetMenu() {
  const auto folder = AudioPluginAudioProcessor::getPresetFolder();
  const auto presets = findFiles(folder, "*" + presetExtension, true);
  const auto current = processorRef.getPresetFile();

  juce::PopupMenu menu;
  menu.setLookAndFeel(&getLookAndFeel());
  menu.addItem(initPreset, "Init");
  menu.addSeparator();

  // Presets in subfolders get a submenu each; loose ones sit at the top level.
  std::map<juce::String, juce::PopupMenu> subfolders;
  for (int i = 0; i < presets.size(); ++i) {
    const auto &file = presets[i];
    const auto parent = file.getParentDirectory();
    auto &target = parent == folder
                       ? menu
                       : subfolders[parent.getRelativePathFrom(folder)];
    target.addItem(firstFile + i, file.getFileNameWithoutExtension(), true,
                   file == current);
  }
  for (auto &[name, submenu] : subfolders)
    menu.addSubMenu(name, submenu);

  menu.addSeparator();
  menu.addItem(savePresetAs, "Save preset...");
  menu.addItem(showPresetFolder, "Show preset folder");

  menu.showMenuAsync(
      menuOptions(presetButton),
      [safeThis = juce::Component::SafePointer<MainPanel>(this),
       presets](int result) {
        if (safeThis == nullptr)
          return;

        if (result == initPreset) {
          safeThis->processorRef.loadInitPreset();
        } else if (result == savePresetAs) {
          safeThis->savePreset();
        } else if (result == showPresetFolder) {
          AudioPluginAudioProcessor::getPresetFolder().revealToUser();
        } else if (result >= firstFile) {
          safeThis->processorRef.loadPreset(presets[result - firstFile]);
        }
      });
}

void MainPanel::savePreset() {
  const auto folder = AudioPluginAudioProcessor::getPresetFolder();
  folder.createDirectory();

  auto target = processorRef.getPresetFile();
  if (target == juce::File())
    target = folder.getChildFile("My Preset" + presetExtension);

  fileChooser = std::make_unique<juce::FileChooser>(
      "Save preset", target,
      "*" + presetExtension);
  fileChooser->launchAsync(
      juce::FileBrowserComponent::saveMode |
          juce::FileBrowserComponent::canSelectFiles |
          juce::FileBrowserComponent::warnAboutOverwriting,
      [safeThis = juce::Component::SafePointer<MainPanel>(this)](
          const juce::FileChooser &chooser) {
        const auto file = chooser.getResult();
        if (safeThis != nullptr && file != juce::File())
          safeThis->processorRef.savePreset(
              file.withFileExtension(presetExtension));
      });
}

void MainPanel::stepPreset(int delta) {
  // The arrows walk through the folder the current preset lives in.
  const auto current = processorRef.getPresetFile();
  const auto root = AudioPluginAudioProcessor::getPresetFolder();
  const auto inFolder = current != juce::File();
  const auto presets =
      findFiles(inFolder ? current.getParentDirectory() : root,
                "*" + presetExtension, !inFolder);
  if (presets.isEmpty())
    return;

  const auto count = presets.size();
  const auto index = presets.indexOf(current);
  const auto next = index < 0 ? (delta > 0 ? 0 : count - 1)
                              : ((index + delta) % count + count) % count;
  processorRef.loadPreset(presets[next]);
}

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(
    AudioPluginAudioProcessor &p)
    : AudioProcessorEditor(&p), processorRef(p), panel(p) {
  setLookAndFeel(&lookAndFeel);
  addAndMakeVisible(panel);

  const auto scale = juce::jlimit(0.8f, 3.0f, processorRef.editorScale);
  setResizable(true, true);
  getConstrainer()->setFixedAspectRatio((double)MainPanel::baseWidth /
                                        (double)MainPanel::baseHeight);
  setResizeLimits(juce::roundToInt(MainPanel::baseWidth * 0.8f),
                  juce::roundToInt(MainPanel::baseHeight * 0.8f),
                  MainPanel::baseWidth * 3, MainPanel::baseHeight * 3);
  setSize(juce::roundToInt((float)MainPanel::baseWidth * scale),
          juce::roundToInt((float)MainPanel::baseHeight * scale));
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {
  setLookAndFeel(nullptr);
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(Theme::background);
}

void AudioPluginAudioProcessorEditor::resized() {
  // The panel keeps its fixed layout and is simply scaled to fit the window.
  const auto scale = (float)getWidth() / (float)MainPanel::baseWidth;
  panel.setTransform(juce::AffineTransform::scale(scale));
  panel.setBounds(0, 0, MainPanel::baseWidth, MainPanel::baseHeight);
  processorRef.editorScale = scale;
}
