#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
namespace Theme {
const juce::Colour background{0xff0a0a0a};
const juce::Colour panel{0xff121212};
const juce::Colour border{0xff262626};
const juce::Colour well{0xff1a1a1a};
const juce::Colour track{0xff2c2c2c};
const juce::Colour accent{0xff3b9eff};
const juce::Colour accentAlt{0xffd97757};
const juce::Colour text{0xffededed};
const juce::Colour textDim{0xff8b8b8b};

inline juce::FontOptions font(float height, bool bold = false) {
  return juce::FontOptions("Helvetica Neue", height,
                           bold ? juce::Font::bold : juce::Font::plain);
}
} // namespace Theme

//==============================================================================
class LfoLookAndFeel final : public juce::LookAndFeel_V4 {
public:
  LfoLookAndFeel() {
    setColour(juce::ComboBox::textColourId, Theme::text);
    setColour(juce::Label::textColourId, Theme::text);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff161616));
    setColour(juce::PopupMenu::textColourId, Theme::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,
              juce::Colour(0xff2a2a2a));
    setColour(juce::PopupMenu::highlightedTextColourId, Theme::text);
  }

  int getSliderThumbRadius(juce::Slider &) override { return 4; }

  void drawLinearSlider(juce::Graphics &g, int x, int y, int width, int height,
                        float sliderPos, float, float,
                        juce::Slider::SliderStyle, juce::Slider &slider) override {
    const auto track = juce::Rectangle<float>((float)x, (float)y, (float)width,
                                              (float)height)
                           .withSizeKeepingCentre((float)width, 3.0f);
    g.setColour(Theme::track);
    g.fillRoundedRectangle(track, 1.5f);

    // Bipolar sliders fill from the centre, the rest from the left.
    const auto origin =
        slider.getMinimum() < 0.0 ? track.getCentreX() : track.getX();
    g.setColour(Theme::accent.withAlpha(slider.isEnabled() ? 1.0f : 0.3f));
    g.fillRoundedRectangle(
        juce::Rectangle<float>::leftTopRightBottom(
            juce::jmin(origin, sliderPos), track.getY(),
            juce::jmax(origin, sliderPos), track.getBottom()),
        1.5f);

    const juce::Point<float> thumb(sliderPos, track.getCentreY());
    if (slider.isEnabled() && slider.isMouseOverOrDragging()) {
      g.setColour(Theme::accent.withAlpha(0.25f));
      g.fillEllipse(juce::Rectangle<float>(12.0f, 12.0f).withCentre(thumb));
    }

    g.setColour(Theme::text.withAlpha(slider.isEnabled() ? 1.0f : 0.4f));
    g.fillEllipse(juce::Rectangle<float>(8.0f, 8.0f).withCentre(thumb));
  }

  void drawComboBox(juce::Graphics &g, int width, int height, bool, int, int,
                    int, int, juce::ComboBox &) override {
    const auto bounds =
        juce::Rectangle<float>((float)width, (float)height).reduced(0.5f);
    g.setColour(Theme::well);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(Theme::border);
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    // Chevron on the right edge
    const auto centre = juce::Point<float>(bounds.getRight() - 7.0f,
                                           bounds.getCentreY() + 1.0f);
    juce::Path chevron;
    chevron.startNewSubPath(centre.x - 2.5f, centre.y - 2.0f);
    chevron.lineTo(centre.x, centre.y + 0.5f);
    chevron.lineTo(centre.x + 2.5f, centre.y - 2.0f);
    g.setColour(Theme::textDim);
    g.strokePath(chevron, juce::PathStrokeType(1.0f));
  }

  juce::Font getComboBoxFont(juce::ComboBox &) override {
    return Theme::font(11.0f);
  }

  void positionComboBoxText(juce::ComboBox &box, juce::Label &label) override {
    label.setBounds(1, 0, box.getWidth() - 12, box.getHeight());
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centred);
  }

  juce::Font getPopupMenuFont() override { return Theme::font(13.0f); }

  void drawPopupMenuBackground(juce::Graphics &g, int width,
                               int height) override {
    g.fillAll(findColour(juce::PopupMenu::backgroundColourId));
    g.setColour(Theme::border);
    g.drawRect(0, 0, width, height);
  }
};
