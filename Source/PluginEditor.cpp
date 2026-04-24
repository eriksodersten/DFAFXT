#include "PluginEditor.h"

namespace
{
constexpr int kEditorWidth = 1320;
constexpr int kEditorHeight = 800;
constexpr int kOuterPadding = 10;
constexpr int kPanelCorner = 12;
constexpr float kSketchWidth = 1713.0f;
constexpr float kSketchHeight = 906.0f;

constexpr int kPresetInitId = 1;
constexpr int kPresetFirstUserId = 2;
constexpr int kPresetMissingId = 999;

constexpr std::array<const char*, XTSequencer::numLaneRows> kSequencerLaneNames
{
    "PITCH", "VEL", "MOD A", "MOD B", "MOD C"
};

struct XTLayout
{
    juce::Rectangle<int> panel;
    juce::Rectangle<int> header;
    juce::Rectangle<int> presetControls;
    juce::Rectangle<int> topArea;
    juce::Rectangle<int> bottomArea;
    juce::Rectangle<int> oscillators;
    juce::Rectangle<int> mixTransient;
    juce::Rectangle<int> filter;
    juce::Rectangle<int> modulation;
    juce::Rectangle<int> lfo;
    juce::Rectangle<int> transport;
    juce::Rectangle<int> sequencer;
    int headerLineY = 0;
    int sectionDividerY = 0;
};

const juce::Colour kPanelBase (0xffeee2c8);
const juce::Colour kPanelShade(0xffe2d5ba);
const juce::Colour kInk       (0xff15140f);
const juce::Colour kDivider   (0xffb2a486);
const juce::Colour kAccentRed (0xffb8362a);
const juce::Colour kKnobBody  (0xff171615);
const juce::Colour kKnobTop   (0xff2a2827);
const juce::Colour kKnobEdge  (0xff050505);
const juce::Colour kDarkPlate (0xff141311);
const juce::Colour kDarkEdge  (0xff050505);
const juce::Colour kAmberText (0xffe89a2e);
const juce::Colour kLedOn     (0xffe8351c);
const juce::Colour kLedOff    (0xff4a1410);
const juce::Colour kLedGreen  (0xff30c04a);

juce::Rectangle<int> mapReferenceRect(const juce::Rectangle<int>& target, float x, float y, float w, float h)
{
    const auto sx = (float) target.getWidth() / kSketchWidth;
    const auto sy = (float) target.getHeight() / kSketchHeight;

    return {
        target.getX() + juce::roundToInt(x * sx),
        target.getY() + juce::roundToInt(y * sy),
        juce::roundToInt(w * sx),
        juce::roundToInt(h * sy)
    };
}

XTLayout createLayout(int width, int height)
{
    XTLayout layout;
    layout.panel = { kOuterPadding, kOuterPadding, width - kOuterPadding * 2, height - kOuterPadding * 2 };

    auto inner = layout.panel.reduced(22, 18);
    const int headerH = juce::roundToInt((float) inner.getHeight() * 0.08f);
    const int gapOne = juce::roundToInt((float) inner.getHeight() * 0.034f);
    const int topH = juce::roundToInt((float) inner.getHeight() * 0.405f);
    const int gapTwo = juce::roundToInt((float) inner.getHeight() * 0.048f);

    layout.header = inner.removeFromTop(headerH);
    layout.headerLineY = layout.header.getBottom() + 10;
    inner.removeFromTop(gapOne);

    layout.topArea = inner.removeFromTop(topH);
    layout.sectionDividerY = layout.topArea.getBottom() + 14;
    inner.removeFromTop(gapTwo);
    layout.bottomArea = inner;

    auto top = layout.topArea;
    const int topW = top.getWidth();
    layout.oscillators = top.removeFromLeft(juce::roundToInt((float) topW * 0.28f));
    layout.mixTransient = top.removeFromLeft(juce::roundToInt((float) topW * 0.19f));
    layout.filter = top.removeFromLeft(juce::roundToInt((float) topW * 0.23f));
    layout.modulation = top.removeFromLeft(juce::roundToInt((float) topW * 0.18f));
    layout.lfo = top;

    auto bottom = layout.bottomArea;
    layout.transport = bottom.removeFromLeft(juce::roundToInt((float) bottom.getWidth() * 0.19f));
    bottom.removeFromLeft(18);
    layout.sequencer = bottom;

    layout.presetControls = layout.header.removeFromRight(juce::roundToInt((float) layout.header.getWidth() * 0.27f)).withTrimmedTop(4);

    return layout;
}

juce::File getDefaultPresetDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("DFAFXT")
        .getChildFile("Presets");
}

juce::String makeStepParameterId(const char* prefix, int index)
{
    return juce::String(prefix) + juce::String(index);
}

juce::String makeModDestinationParameterId(int index)
{
    return "mod" + juce::String::charToString((juce_wchar) ('A' + index)) + "Dest";
}

juce::String makeModAmountParameterId(int index)
{
    return "mod" + juce::String::charToString((juce_wchar) ('A' + index)) + "Amt";
}

void drawMutedLabel(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& text,
                    juce::Justification justification = juce::Justification::centred)
{
    g.setColour(kInk.withAlpha(0.7f));
    g.setFont(juce::FontOptions(8.0f));
    g.drawFittedText(text, bounds, justification, 1);
}

void drawKnobBody(juce::Graphics& g, juce::Point<float> centre, float size, float normalizedValue, XTKnobStyle style)
{
    const float radius = size * 0.5f;
    const float x = centre.x - radius;
    const float y = centre.y - radius;

    // Drop shadow directly beneath — single soft ellipse, no chrome.
    const float shadowOffset = size * 0.08f;
    g.setColour(juce::Colour(0x55000000));
    g.fillEllipse(x + 1.0f, y + shadowOffset, size, size);

    // Body: near-black with a subtle top-down gradient so the edge reads.
    juce::ColourGradient bodyGrad(kKnobTop, centre.x, y + 1.0f,
                                  kKnobBody, centre.x, y + size, false);
    g.setGradientFill(bodyGrad);
    g.fillEllipse(x, y, size, size);

    // Sharp dark rim — one pixel, no bezel ring, no chrome.
    g.setColour(kKnobEdge);
    g.drawEllipse(x + 0.25f, y + 0.25f, size - 0.5f, size - 0.5f, 0.8f);

    // Tiny outer tick dots at min / center / max (three subtle marks on the panel).
    if (style != XTKnobStyle::sequencer)
    {
        const float dotRadius = juce::jmax(0.7f, size * 0.025f);
        auto dotAt = [&](float ang)
        {
            const float cx = centre.x + (radius + size * 0.14f) * std::cos(ang - juce::MathConstants<float>::halfPi);
            const float cy = centre.y + (radius + size * 0.14f) * std::sin(ang - juce::MathConstants<float>::halfPi);
            g.setColour(kInk.withAlpha(0.55f));
            g.fillEllipse(cx - dotRadius, cy - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
        };
        const float sweep = juce::MathConstants<float>::pi * 0.74f;
        dotAt(-sweep);
        dotAt(0.0f);
        dotAt(sweep);
    }

    // Pointer: single thin cream tick from centre to near-edge.
    const float pointerAngle = juce::jmap(normalizedValue, 0.0f, 1.0f,
                                          -juce::MathConstants<float>::pi * 0.74f,
                                          juce::MathConstants<float>::pi * 0.74f);
    const auto direction = juce::Point<float>(std::cos(pointerAngle - juce::MathConstants<float>::halfPi),
                                              std::sin(pointerAngle - juce::MathConstants<float>::halfPi));
    const float pointerInner = style == XTKnobStyle::sequencer ? 0.15f : 0.12f;
    const float pointerOuter = style == XTKnobStyle::sequencer ? 0.82f : 0.86f;
    const auto inner = centre + direction * (radius * pointerInner);
    const auto outer = centre + direction * (radius * pointerOuter);
    const float pointerThickness = style == XTKnobStyle::sequencer ? 1.6f
                                 : style == XTKnobStyle::main ? 2.4f
                                 : 2.0f;
    g.setColour(juce::Colour(0xfff5efe0));
    g.drawLine(inner.x, inner.y, outer.x, outer.y, pointerThickness);
}

void drawStaticKnob(juce::Graphics& g, juce::Point<float> centre, float size, XTKnobStyle style = XTKnobStyle::main)
{
    drawKnobBody(g, centre, size, 0.5f, style);
}

XTKnobStyle getKnobStyle(const juce::Slider& slider)
{
    if (const auto* xtSlider = dynamic_cast<const XTSlider*>(&slider))
        return xtSlider->style;

    return XTKnobStyle::main;
}

XTComboStyle getComboStyle(const juce::ComboBox& box)
{
    if (const auto* xtBox = dynamic_cast<const XTComboBox*>(&box))
        return xtBox->style;

    return XTComboStyle::led;
}

XTButtonStyle getButtonStyle(const juce::Button& button)
{
    if (const auto* xtButton = dynamic_cast<const XTTextButton*>(&button))
        return xtButton->style;

    return XTButtonStyle::utility;
}

}

XTLookAndFeel::XTLookAndFeel()
{
    setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffd4c7ae));
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff282826));
    setColour(juce::Slider::thumbColourId, juce::Colours::white);

    setColour(juce::ComboBox::backgroundColourId, kDarkPlate);
    setColour(juce::ComboBox::outlineColourId, kDarkEdge);
    setColour(juce::ComboBox::textColourId, juce::Colour(0xffefe7d8));
    setColour(juce::ComboBox::arrowColourId, juce::Colour(0xffefe7d8));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff201f1d));
    setColour(juce::PopupMenu::textColourId, juce::Colour(0xffefe7d8));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff9e2f24));

    setColour(juce::TextButton::buttonColourId, kDarkPlate);
    setColour(juce::TextButton::buttonOnColourId, kAccentRed);
    setColour(juce::TextButton::textColourOffId, juce::Colour(0xffefe7d8));
    setColour(juce::TextButton::textColourOnId, juce::Colour(0xffefe7d8));
}

void XTLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                     float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                     juce::Slider& slider)
{
    juce::ignoreUnused(rotaryStartAngle, rotaryEndAngle);
    const auto style = getKnobStyle(slider);
    const float size = (float) juce::jmin(width, height) - (style == XTKnobStyle::sequencer ? 2.0f : 4.0f);
    const auto centre = juce::Point<float>((float) x + (float) width * 0.5f, (float) y + (float) height * 0.5f);
    drawKnobBody(g, centre, size, sliderPos, style);
}

void XTLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                 int, int, int, int,
                                 juce::ComboBox& box)
{
    const auto style = getComboStyle(box);
    const auto bounds = juce::Rectangle<float>(0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);

    if (style == XTComboStyle::led)
    {
        // Near-black body with very faint inner glow from bottom
        g.setColour(juce::Colour(0xff080706));
        g.fillRoundedRectangle(bounds, 3.0f);
        juce::ColourGradient glow(kAmberText.withAlpha(0.07f), (float) width * 0.5f, (float) height,
                                  juce::Colours::transparentBlack, (float) width * 0.5f, 0.0f, false);
        g.setGradientFill(glow);
        g.fillRoundedRectangle(bounds, 3.0f);
        // Amber border
        g.setColour(kAmberText.withAlpha(0.45f));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
        return;
    }

    if (style == XTComboStyle::toggle)
    {
        // Dark housing
        g.setColour(juce::Colour(0xff0c0b0a));
        g.fillRoundedRectangle(bounds, 5.0f);
        g.setColour(juce::Colour(0xff282422));
        g.drawRoundedRectangle(bounds, 5.0f, 1.2f);

        // Slot line down the center
        const float slotX = (float) width * 0.5f - 1.5f;
        g.setColour(juce::Colour(0xff1e1c1a));
        g.fillRoundedRectangle(slotX, bounds.getY() + 6.0f, 3.0f, bounds.getHeight() - 12.0f, 1.5f);

        // Toggle tab: item 1 = OFF (top half), item 2 = ON (bottom half)
        const bool isOn = (box.getSelectedId() == 2);
        const float tabH = bounds.getHeight() * 0.42f;
        const float margin = bounds.getHeight() * 0.09f;
        const float tabY = isOn ? (bounds.getBottom() - tabH - margin) : (bounds.getY() + margin);
        const float tabX = bounds.getX() + bounds.getWidth() * 0.12f;
        const float tabW = bounds.getWidth() * 0.76f;

        // Tab drop shadow
        g.setColour(juce::Colour(0x55000000));
        g.fillRoundedRectangle(tabX + 1.0f, tabY + 2.0f, tabW, tabH, 4.0f);

        // Tab body gradient
        juce::ColourGradient tabGrad(juce::Colour(0xff404038), tabX, tabY,
                                     juce::Colour(0xff262421), tabX, tabY + tabH, false);
        g.setGradientFill(tabGrad);
        g.fillRoundedRectangle(tabX, tabY, tabW, tabH, 4.0f);
        g.setColour(juce::Colour(0xff171512));
        g.drawRoundedRectangle(tabX, tabY, tabW, tabH, 4.0f, 1.0f);

        // LED dot in center of tab
        const float ledCX = tabX + tabW * 0.5f;
        const float ledCY = tabY + tabH * 0.5f;
        const float ledR = tabW * 0.12f;
        g.setColour(isOn ? kLedOn.withAlpha(0.6f) : juce::Colour(0x00000000));
        if (isOn)
        {
            g.fillEllipse(ledCX - ledR * 2.2f, ledCY - ledR * 2.2f, ledR * 4.4f, ledR * 4.4f);
        }
        g.setColour(isOn ? kLedOn : kLedOff);
        g.fillEllipse(ledCX - ledR, ledCY - ledR, ledR * 2.0f, ledR * 2.0f);

        // OFF / ON text labels on housing
        g.setColour(juce::Colour(0xffefe7d8).withAlpha(0.55f));
        g.setFont(juce::FontOptions(7.5f).withStyle("Bold"));
        const float labelW = (float) width;
        if (! isOn)
        {
            g.setColour(juce::Colour(0xffefe7d8));
        }
        g.drawText("OFF", 0, (int) bounds.getY() + 3, width, 10, juce::Justification::centred, false);
        g.setColour(juce::Colour(0xffefe7d8).withAlpha(isOn ? 1.0f : 0.55f));
        g.drawText("ON", 0, (int) bounds.getBottom() - 13, width, 10, juce::Justification::centred, false);
        juce::ignoreUnused(labelW);
        return;
    }

    // preset style
    g.setColour(kDarkEdge.withAlpha(0.18f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 1.0f), 4.0f);
    g.setColour(kDarkPlate);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(kDarkEdge);
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    juce::Path chevron;
    chevron.startNewSubPath((float) width - 24.0f, (float) height * 0.42f);
    chevron.lineTo((float) width - 16.0f, (float) height * 0.62f);
    chevron.lineTo((float) width - 8.0f, (float) height * 0.42f);
    g.setColour(juce::Colour(0xffefe7d8));
    g.strokePath(chevron, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void XTLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    const auto style = getComboStyle(box);

    if (style == XTComboStyle::toggle)
    {
        label.setBounds(0, 0, 0, 0);
        return;
    }

    if (style == XTComboStyle::led)
    {
        label.setBounds(2, 0, box.getWidth() - 4, box.getHeight());
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, kAmberText);
        label.setFont(getComboBoxFont(box));
        return;
    }

    label.setBounds(10, 0, box.getWidth() - 28, box.getHeight());
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centred);
}

juce::Font XTLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    const auto style = getComboStyle(box);
    if (style == XTComboStyle::led)
        return juce::Font(juce::FontOptions((float) juce::jmin(12, box.getHeight() - 4)).withStyle("Bold"));
    if (style == XTComboStyle::preset)
        return juce::Font(juce::FontOptions(11.0f).withStyle("Bold"));
    return juce::Font(juce::FontOptions((float) juce::jmin(13, box.getHeight() - 6)).withStyle("Bold"));
}

void XTLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const auto style = getButtonStyle(button);

    if (style == XTButtonStyle::led)
    {
        const bool on = button.getToggleState() || shouldDrawButtonAsDown;
        const float w = bounds.getWidth();
        const float h = bounds.getHeight();

        // Dark square housing
        g.setColour(juce::Colour(0xff0c0b0a));
        g.fillRoundedRectangle(bounds, 3.0f);
        g.setColour(juce::Colour(0xff282422));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

        // LED dot — top third of the button, centered
        const float ledR = juce::jmin(w, h) * 0.14f;
        const float ledCX = bounds.getCentreX();
        const float ledCY = bounds.getY() + h * 0.32f;

        if (on)
        {
            g.setColour(kLedOn.withAlpha(0.35f));
            g.fillEllipse(ledCX - ledR * 2.8f, ledCY - ledR * 2.8f, ledR * 5.6f, ledR * 5.6f);
        }
        g.setColour(on ? kLedOn : kLedOff);
        g.fillEllipse(ledCX - ledR, ledCY - ledR, ledR * 2.0f, ledR * 2.0f);
        g.setColour(on ? juce::Colour(0xff7f1d15) : juce::Colour(0xff181818));
        g.drawEllipse(ledCX - ledR, ledCY - ledR, ledR * 2.0f, ledR * 2.0f, 0.8f);

        if (shouldDrawButtonAsHighlighted && !on)
        {
            g.setColour(juce::Colour(0x18ffffff));
            g.fillRoundedRectangle(bounds, 3.0f);
        }
        return;
    }

    auto fill = kDarkPlate;
    const bool isSquare = style == XTButtonStyle::square;

    if (shouldDrawButtonAsDown)
        fill = fill.brighter(0.12f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter(0.06f);

    if (style == XTButtonStyle::utility)
    {
        g.setColour(kDarkEdge.withAlpha(0.18f));
        g.fillRoundedRectangle(bounds.translated(0.0f, 1.0f), 6.0f);
    }

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, isSquare ? 3.0f : 6.0f);
    g.setColour(kDarkEdge);
    g.drawRoundedRectangle(bounds, isSquare ? 3.0f : 6.0f, 1.0f);
}

void XTLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    if (button.getButtonText().isEmpty())
        return;

    const auto style = getButtonStyle(button);
    if (style == XTButtonStyle::led)
    {
        // Label sits in the bottom ~55% of the button, below the LED
        const auto bounds = button.getLocalBounds();
        const int textY = bounds.getY() + juce::roundToInt((float) bounds.getHeight() * 0.52f);
        g.setFont(juce::FontOptions(7.5f).withStyle("Bold"));
        g.setColour(juce::Colour(0xffefe7d8).withAlpha(0.85f));
        g.drawFittedText(button.getButtonText(),
                         bounds.getX(), textY, bounds.getWidth(), bounds.getBottom() - textY,
                         juce::Justification::centred, 1);
        return;
    }

    g.setFont(juce::FontOptions(style == XTButtonStyle::square ? 7.5f : 8.5f).withStyle("Bold"));
    g.setColour(juce::Colour(0xffefe7d8));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, 1);
}

XTEditor::XTEditor(XTProcessor& p)
    : AudioProcessorEditor(&p), xtProcessor(p)
{
    setSize(kEditorWidth, kEditorHeight);
    setLookAndFeel(&laf);
    startTimerHz(24);

    setupChoice(presetBox);
    presetBox.setJustificationType(juce::Justification::centredLeft);
    presetBox.onChange = [this]()
    {
        if (isUpdatingPresetBox)
            return;

        const int selectedId = presetBox.getSelectedId();
        if (selectedId == kPresetMissingId)
            return;

        if (selectedId == kPresetInitId)
            xtProcessor.loadInitPreset();
        else
            xtProcessor.loadPreset(presetBox.getText());

        refreshPresetControls();
    };
    addAndMakeVisible(presetBox);

    auto styleButton = [this](XTTextButton& button, std::function<void()> onClick)
    {
        button.onClick = std::move(onClick);
        button.setClickingTogglesState(false);
        addAndMakeVisible(button);
    };

    styleButton(presetSaveButton, [this]() { promptSavePreset(); });
    styleButton(presetDeleteButton, [this]()
    {
        const auto presetName = xtProcessor.getCurrentPresetName();
        if (presetName.isEmpty() || presetName == "Init")
            return;

        if (juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon,
                                               "Delete Preset",
                                               "Delete preset \"" + presetName + "\"?",
                                               "Delete",
                                               "Cancel",
                                               this,
                                               nullptr))
        {
            xtProcessor.deletePreset(presetName);
            refreshPresetControls();
        }
    });
    styleButton(presetInitButton, [this]()
    {
        xtProcessor.loadInitPreset();
        refreshPresetControls();
    });

    resetButton.onClick = [this]()
    {
        xtProcessor.resetSequencer();
        currentLedStep = 0;
        resetLedActive = true;
        repaint();
    };
    resetButton.setButtonText({});
    resetButton.setTooltip("Reset Sequencer");
    addAndMakeVisible(resetButton);

    auto addKnob = [this](XTSlider& slider)
    {
        setupKnob(slider);
        addAndMakeVisible(slider);
    };

    auto addChoice = [this](XTComboBox& box)
    {
        setupChoice(box);
        addAndMakeVisible(box);
    };

    addKnob(vcoDecay);
    addKnob(vco1EgAmount);
    addKnob(vco1Frequency);
    addKnob(fmAmount);
    addKnob(vco2EgAmount);
    addKnob(vco2Frequency);
    addKnob(vco1Level);
    addKnob(vco2Level);
    addKnob(noiseLevel);
    addKnob(cutoff);
    addKnob(resonance);
    addKnob(vcfDecay);
    addKnob(vcfEgAmount);
    addKnob(noiseVcfMod);
    addKnob(vcaDecay);
    addKnob(vcaEg);
    addKnob(volume);

    seqPitchModBox.addItem("VCO 1&2", 1);
    seqPitchModBox.addItem("OFF", 2);
    seqPitchModBox.addItem("VCO 2", 3);
    addChoice(seqPitchModBox);

    hardSyncBox.addItem("OFF", 1);
    hardSyncBox.addItem("ON", 2);
    addChoice(hardSyncBox);

    vco1WaveBox.addItem("SQUARE", 1);
    vco1WaveBox.addItem("TRIANGLE", 2);
    addChoice(vco1WaveBox);

    vco2WaveBox.addItem("SQUARE", 1);
    vco2WaveBox.addItem("TRIANGLE", 2);
    addChoice(vco2WaveBox);

    vcfModeBox.addItem("LP", 1);
    vcfModeBox.addItem("HP", 2);
    addChoice(vcfModeBox);

    const auto destinationNames = XTProcessor::getModDestinationNames();
    for (int i = 0; i < 3; ++i)
    {
        addKnob(modAmount[i]);
        modDestBox[i].setJustificationType(juce::Justification::centred);
        for (int item = 0; item < destinationNames.size(); ++item)
            modDestBox[i].addItem(destinationNames[item], item + 1);
        addChoice(modDestBox[i]);
    }

    clockMultBox.addItem("1/8", 1);
    clockMultBox.addItem("1/5", 2);
    clockMultBox.addItem("1/4", 3);
    clockMultBox.addItem("1/3", 4);
    clockMultBox.addItem("1/2", 5);
    clockMultBox.addItem("1X", 6);
    clockMultBox.addItem("2X", 7);
    clockMultBox.addItem("3X", 8);
    clockMultBox.addItem("4X", 9);
    clockMultBox.addItem("5X", 10);
    addChoice(clockMultBox);

    for (int i = 0; i < XTSequencer::numSteps; ++i)
    {
        stepPitch[i].style = XTKnobStyle::sequencer;
        stepVelocity[i].style = XTKnobStyle::sequencer;
        stepModA[i].style = XTKnobStyle::sequencer;
        stepModB[i].style = XTKnobStyle::sequencer;
        stepModC[i].style = XTKnobStyle::sequencer;

        addKnob(stepPitch[i]);
        addKnob(stepVelocity[i]);
        addKnob(stepModA[i]);
        addKnob(stepModB[i]);
        addKnob(stepModC[i]);
    }

    auto& apvts = p.apvts;
    auto setDoubleClickToDefault = [&apvts](juce::Slider& slider, const juce::String& parameterId)
    {
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(parameterId)))
            slider.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
    };

    vcoDecayAtt = std::make_unique<SliderAttachment>(apvts, "vcoDecay", vcoDecay);
    vco1FreqAtt = std::make_unique<SliderAttachment>(apvts, "vco1Freq", vco1Frequency);
    vco1EgAmtAtt = std::make_unique<SliderAttachment>(apvts, "vco1EgAmt", vco1EgAmount);
    fmAmountAtt = std::make_unique<SliderAttachment>(apvts, "fmAmount", fmAmount);
    vco2FreqAtt = std::make_unique<SliderAttachment>(apvts, "vco2Freq", vco2Frequency);
    vco2EgAmtAtt = std::make_unique<SliderAttachment>(apvts, "vco2EgAmt", vco2EgAmount);
    vco1LevelAtt = std::make_unique<SliderAttachment>(apvts, "vco1Level", vco1Level);
    vco2LevelAtt = std::make_unique<SliderAttachment>(apvts, "vco2Level", vco2Level);
    noiseLevelAtt = std::make_unique<SliderAttachment>(apvts, "noiseLevel", noiseLevel);
    cutoffAtt = std::make_unique<SliderAttachment>(apvts, "cutoff", cutoff);
    resonanceAtt = std::make_unique<SliderAttachment>(apvts, "resonance", resonance);
    vcfDecayAtt = std::make_unique<SliderAttachment>(apvts, "vcfDecay", vcfDecay);
    vcfEgAmtAtt = std::make_unique<SliderAttachment>(apvts, "vcfEgAmt", vcfEgAmount);
    noiseVcfModAtt = std::make_unique<SliderAttachment>(apvts, "noiseVcfMod", noiseVcfMod);
    vcaDecayAtt = std::make_unique<SliderAttachment>(apvts, "vcaDecay", vcaDecay);
    vcaEgAtt = std::make_unique<SliderAttachment>(apvts, "vcaEg", vcaEg);
    volumeAtt = std::make_unique<SliderAttachment>(apvts, "volume", volume);

    seqPitchModBoxAtt = std::make_unique<ComboAttachment>(apvts, "seqPitchMod", seqPitchModBox);
    hardSyncBoxAtt = std::make_unique<ComboAttachment>(apvts, "hardSync", hardSyncBox);
    vco1WaveBoxAtt = std::make_unique<ComboAttachment>(apvts, "vco1Wave", vco1WaveBox);
    vco2WaveBoxAtt = std::make_unique<ComboAttachment>(apvts, "vco2Wave", vco2WaveBox);
    vcfModeBoxAtt = std::make_unique<ComboAttachment>(apvts, "vcfMode", vcfModeBox);
    clockMultBoxAtt = std::make_unique<ComboAttachment>(apvts, "clockMult", clockMultBox);

    for (int i = 0; i < 3; ++i)
    {
        const auto modDestId = makeModDestinationParameterId(i);
        const auto modAmtId = makeModAmountParameterId(i);

        modDestBoxAtt[i] = std::make_unique<ComboAttachment>(apvts, modDestId, modDestBox[i]);
        modAmountAtt[i] = std::make_unique<SliderAttachment>(apvts, modAmtId, modAmount[i]);
        setDoubleClickToDefault(modAmount[i], modAmtId);
    }

    for (int i = 0; i < XTSequencer::numSteps; ++i)
    {
        const auto stepPitchId = makeStepParameterId("stepPitch", i);
        const auto stepVelId = makeStepParameterId("stepVel", i);
        const auto stepModAId = makeStepParameterId("stepModA", i);
        const auto stepModBId = makeStepParameterId("stepModB", i);
        const auto stepModCId = makeStepParameterId("stepModC", i);

        stepPitchAtt[i] = std::make_unique<SliderAttachment>(apvts, stepPitchId, stepPitch[i]);
        stepVelAtt[i] = std::make_unique<SliderAttachment>(apvts, stepVelId, stepVelocity[i]);
        stepModAAtt[i] = std::make_unique<SliderAttachment>(apvts, stepModAId, stepModA[i]);
        stepModBAtt[i] = std::make_unique<SliderAttachment>(apvts, stepModBId, stepModB[i]);
        stepModCAtt[i] = std::make_unique<SliderAttachment>(apvts, stepModCId, stepModC[i]);

        setDoubleClickToDefault(stepPitch[i], stepPitchId);
        setDoubleClickToDefault(stepVelocity[i], stepVelId);
        setDoubleClickToDefault(stepModA[i], stepModAId);
        setDoubleClickToDefault(stepModB[i], stepModBId);
        setDoubleClickToDefault(stepModC[i], stepModCId);
    }

    vco1EgAmount.setDoubleClickReturnValue(true, 0.0);
    vco2EgAmount.setDoubleClickReturnValue(true, 0.0);
    vcfEgAmount.setDoubleClickReturnValue(true, 0.0);

    refreshPresetControls();
}

XTEditor::~XTEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void XTEditor::setupKnob(XTSlider& slider)
{
    slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange(0.0, 1.0);
    slider.setValue(0.5);
}

void XTEditor::setupChoice(XTComboBox& box)
{
    box.setJustificationType(juce::Justification::centred);

    if (box.style == XTComboStyle::led)
    {
        box.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff080706));
        box.setColour(juce::ComboBox::outlineColourId, kAmberText.withAlpha(0.45f));
        box.setColour(juce::ComboBox::textColourId, kAmberText);
        box.setColour(juce::ComboBox::arrowColourId, juce::Colours::transparentBlack);
    }
    else if (box.style == XTComboStyle::toggle)
    {
        box.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0c0b0a));
        box.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff282422));
        box.setColour(juce::ComboBox::textColourId, juce::Colours::transparentBlack);
        box.setColour(juce::ComboBox::arrowColourId, juce::Colours::transparentBlack);
    }
    else
    {
        box.setColour(juce::ComboBox::backgroundColourId, kDarkPlate);
        box.setColour(juce::ComboBox::outlineColourId, kDarkEdge);
        box.setColour(juce::ComboBox::textColourId, juce::Colour(0xffefe7d8));
        box.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xffefe7d8));
    }
}

void XTEditor::refreshPresetControls()
{
    const auto presetNames = xtProcessor.getAvailablePresetNames();
    const auto currentPreset = xtProcessor.getCurrentPresetName();

    isUpdatingPresetBox = true;
    presetBox.clear(juce::dontSendNotification);
    presetBox.addItem("INIT", kPresetInitId);

    for (int i = 0; i < presetNames.size(); ++i)
        presetBox.addItem(presetNames[i], kPresetFirstUserId + i);

    if (currentPreset.isEmpty() || currentPreset == "Init")
    {
        presetBox.setSelectedId(kPresetInitId, juce::dontSendNotification);
    }
    else
    {
        const int presetIndex = presetNames.indexOf(currentPreset);
        if (presetIndex >= 0)
            presetBox.setSelectedId(kPresetFirstUserId + presetIndex, juce::dontSendNotification);
        else
        {
            presetBox.addItem(currentPreset + " *", kPresetMissingId);
            presetBox.setSelectedId(kPresetMissingId, juce::dontSendNotification);
        }
    }

    isUpdatingPresetBox = false;
    lastPresetName = currentPreset;
    updatePresetButtonState();
}

void XTEditor::promptSavePreset()
{
    const auto currentPreset = xtProcessor.getCurrentPresetName();
    if (currentPreset.isNotEmpty()
        && currentPreset != "Init"
        && xtProcessor.getAvailablePresetNames().contains(currentPreset))
    {
        xtProcessor.saveCurrentPreset();
        refreshPresetControls();
        return;
    }

    auto presetDirectory = getDefaultPresetDirectory();
    presetDirectory.createDirectory();

    const auto suggestedName = currentPreset == "Init" ? juce::String("New Preset") : currentPreset;
    const auto suggestedFile = presetDirectory.getChildFile(suggestedName + ".dfafxtpreset");

    presetSaveChooser = std::make_unique<juce::FileChooser>("Save DFAF XT Preset",
                                                            suggestedFile,
                                                            "*.dfafxtpreset");

    presetSaveChooser->launchAsync(juce::FileBrowserComponent::saveMode
                                       | juce::FileBrowserComponent::canSelectFiles,
                                   [this](const juce::FileChooser& chooser)
                                   {
                                       const auto result = chooser.getResult();
                                       presetSaveChooser.reset();

                                       if (result == juce::File())
                                           return;

                                       xtProcessor.savePreset(result.getFileNameWithoutExtension());
                                       refreshPresetControls();
                                   });
}

void XTEditor::updatePresetButtonState()
{
    const auto currentPreset = xtProcessor.getCurrentPresetName();
    const bool hasSavedPreset = currentPreset.isNotEmpty()
        && currentPreset != "Init"
        && xtProcessor.getAvailablePresetNames().contains(currentPreset);

    presetDeleteButton.setEnabled(hasSavedPreset);
}

juce::String XTEditor::getModLaneSubtitle(int modLaneIndex) const
{
    if (! juce::isPositiveAndBelow(modLaneIndex, 3))
        return {};

    auto text = modDestBox[modLaneIndex].getText().toUpperCase().trim();
    if (text.isEmpty() || text == "OFF")
        text = "ASSIGN";

    return text;
}

void XTEditor::timerCallback()
{
    const auto currentPreset = xtProcessor.getCurrentPresetName();
    if (currentPreset != lastPresetName)
        refreshPresetControls();

    const int step = xtProcessor.getCurrentStep();
    if (step >= 0)
        resetLedActive = false;

    currentLedStep = resetLedActive ? 0 : step;
    repaint();
}

void XTEditor::paint(juce::Graphics& g)
{
    const auto layout = createLayout(getWidth(), getHeight());
    const auto ref = [&](float x, float y, float w, float h)
    {
        return mapReferenceRect(layout.panel, x, y, w, h);
    };

    g.fillAll(juce::Colour(0xff161513));

    juce::ColourGradient panelGrad(kPanelBase, (float) layout.panel.getX(), (float) layout.panel.getY(),
                                   kPanelShade, (float) layout.panel.getRight(), (float) layout.panel.getBottom(),
                                   false);
    g.setGradientFill(panelGrad);
    g.fillRoundedRectangle(layout.panel.toFloat(), (float) kPanelCorner);
    g.setColour(juce::Colour(0xff2d2a26));
    g.drawRoundedRectangle(layout.panel.toFloat(), (float) kPanelCorner, 2.0f);

    auto drawScrew = [&g](juce::Rectangle<int> r)
    {
        const auto c = r.getCentre().toFloat();
        g.setColour(juce::Colour(0xff3c3935));
        g.fillEllipse((float) r.getX(), (float) r.getY(), (float) r.getWidth(), (float) r.getHeight());
        g.setColour(juce::Colour(0xff1b1917));
        g.drawEllipse((float) r.getX(), (float) r.getY(), (float) r.getWidth(), (float) r.getHeight(), 1.2f);
        g.drawLine(c.x - 3.5f, c.y, c.x + 3.5f, c.y, 1.0f);
        g.drawLine(c.x, c.y - 3.5f, c.x, c.y + 3.5f, 1.0f);
    };

    drawScrew(ref(16, 13, 24, 24));
    drawScrew(ref(1687, 13, 24, 24));
    drawScrew(ref(16, 869, 24, 24));
    drawScrew(ref(1687, 869, 24, 24));

    g.setColour(kDivider);
    auto line = [&](float x1, float y1, float x2, float y2, float t = 1.0f)
    {
        const auto a = ref(x1, y1, 0, 0).getPosition();
        const auto b = ref(x2, y2, 0, 0).getPosition();
        g.drawLine((float) a.x, (float) a.y, (float) b.x, (float) b.y, t);
    };
    line(42, 98, 1670, 98);
    line(42, 442, 1670, 442);
    line(498, 109, 498, 432, 0.8f);
    line(806, 109, 806, 432, 0.8f);
    line(1360, 109, 1360, 432, 0.8f);
    line(1463, 109, 1463, 432, 0.8f);
    line(345, 454, 345, 844, 0.8f);

    auto drawTitle = [&](const juce::String& text, float x, float y, float w)
    {
        g.setColour(kInk);
        g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
        g.drawText(text, ref(x, y, w, 16), juce::Justification::centred, false);
    };
    auto drawLabel = [&](const juce::String& text, float x, float y, float w, juce::Justification j = juce::Justification::centred)
    {
        g.setColour(kInk);
        g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
        g.drawFittedText(text, ref(x, y, w, 12), j, 1);
    };
    auto drawMuted = [&](const juce::String& text, float x, float y, float w, juce::Justification j = juce::Justification::centred)
    {
        g.setColour(kInk.withAlpha(0.72f));
        g.setFont(juce::FontOptions(8.0f));
        g.drawFittedText(text, ref(x, y, w, 10), j, 1);
    };
    auto drawPlaceholderKnob = [&](float x, float y, float size, XTKnobStyle style = XTKnobStyle::small)
    {
        auto r = ref(x, y, size, size);
        drawStaticKnob(g, r.getCentre().toFloat(), (float) juce::jmin(r.getWidth(), r.getHeight()), style);
    };
    auto drawPlaceholderBox = [&](float x, float y, float w, float h, const juce::String& text = {})
    {
        auto r = ref(x, y, w, h).toFloat();
        g.setColour(kDarkPlate);
        g.fillRoundedRectangle(r, 3.0f);
        g.setColour(kDarkEdge);
        g.drawRoundedRectangle(r, 3.0f, 1.0f);
        if (text.isNotEmpty())
        {
            g.setColour(juce::Colour(0xffefe7d8));
            g.setFont(juce::FontOptions(8.5f).withStyle("Bold"));
            g.drawFittedText(text, r.toNearestInt(), juce::Justification::centred, 1);
        }
    };

    g.setColour(kInk);
    g.setFont(juce::FontOptions(38.0f).withStyle("Bold"));
    g.drawText("DFAF", ref(58, 25, 130, 44), juce::Justification::centredLeft, false);
    g.setColour(kAccentRed);
    g.drawText("XT", ref(197, 25, 78, 44), juce::Justification::centredLeft, false);
    g.setColour(kInk.withAlpha(0.92f));
    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    g.drawText("SINGLE-VOICE PERCUSSION SYNTHESIZER", ref(63, 72, 350, 16), juce::Justification::left, false);

    drawTitle("OSCILLATORS / INTERACTION", 150, 113, 294);
    drawTitle("MIX / TRANSIENT", 566, 113, 178);
    drawTitle("FILTER / AMP / DRIVE", 866, 113, 252);
    drawTitle("MODULATION", 1177, 113, 190);
    drawTitle("LFO", 1522, 113, 82);
    drawTitle("TRANSPORT", 95, 457, 138);

    g.setColour(kInk);
    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    g.drawText("ANALOG  -  16 STEP  -  3 MOD  -  1 LFO", ref(1458, 39, 198, 12), juce::Justification::right, false);
    g.setFont(juce::FontOptions(8.0f));
    g.drawText("SERIAL NO. XT-0001", ref(1548, 61, 108, 10), juce::Justification::right, false);
    g.drawText("MADE IN SWEDEN", ref(1550, 80, 106, 10), juce::Justification::right, false);

    drawLabel("VCO 1", 50, 148, 48, juce::Justification::left);
    line(119, 158, 347, 158, 0.8f);
    drawLabel("VCO 2", 50, 287, 48, juce::Justification::left);
    line(119, 297, 347, 297, 0.8f);

    drawLabel("HARD SYNC", 431, 160, 70);
    // VCO 1 labels — FREQ/EG AMT/VCO DECAY are live; WAVE is live combo; LEVEL has no param
    drawLabel("FREQ", 62, 248, 52);
    drawLabel("EG AMT", 135, 248, 62);
    drawLabel("WAVE", 208, 234, 58);
    drawLabel("LEVEL", 278, 248, 52);
    drawLabel("VCO DECAY", 351, 248, 52);
    drawPlaceholderKnob(278, 192, 46);     // VCO1 LEVEL — no param yet

    // VCO 2 labels — FREQ/EG AMT are live; WAVE is live combo; LEVEL+LVL have no param
    drawLabel("FREQ", 62, 388, 52);
    drawLabel("EG AMT", 135, 388, 62);
    drawLabel("WAVE", 208, 374, 58);
    drawLabel("LEVEL", 278, 388, 52);
    drawLabel("LVL", 351, 388, 46);
    drawPlaceholderKnob(278, 332, 46);     // VCO2 LEVEL — no param yet
    drawPlaceholderKnob(351, 332, 46);     // VCO2 LVL — no param yet

    // MIX section — top row knobs are LIVE (vco1Level, vco2Level, noiseLevel)
    drawLabel("VCO 1", 507, 218, 50);
    drawLabel("VCO 2", 580, 218, 50);
    drawLabel("NOISE", 652, 218, 58);
    // Bottom row: fmAmount is LIVE; CLK controls have no param
    drawLabel("FM AMT", 507, 326, 68);
    drawLabel("CLK TUNE", 580, 326, 72);
    drawLabel("CLK DEC", 653, 326, 68);
    drawLabel("CLK LVL", 725, 326, 64);
    drawPlaceholderKnob(592, 281, 42);     // CLK TUNE — no param
    drawPlaceholderKnob(665, 281, 42);     // CLK DEC — no param
    drawPlaceholderKnob(738, 281, 42);     // CLK LVL — no param
    drawLabel("SIGNAL FLOW", 608, 380, 90);
    g.setColour(kDivider.withAlpha(0.6f));
    g.drawRoundedRectangle(ref(526, 388, 249, 32).toFloat(), 3.0f, 0.8f);
    drawMuted("VCO1 + VCO2 + NOISE + CLICK → FILTER → DRIVE → VCA → OUT", 531, 399, 238);

    // FILTER — knobs and vcfModeBox are LIVE; labels centred under each knob centre
    drawLabel("MODE",         816, 134, 68);
    drawLabel("CUTOFF",       806, 250, 90);
    drawLabel("RESONANCE",    876, 250, 92);
    drawLabel("VCF EG AMT",   946, 250, 96);
    drawLabel("VOLUME",      1020, 250, 84);
    drawPlaceholderBox(819, 372, 24, 24);
    drawPlaceholderBox(849, 372, 24, 24);
    drawPlaceholderBox(879, 372, 24, 24);
    drawPlaceholderBox(969, 372, 24, 24);
    drawPlaceholderBox(1021, 372, 24, 24);
    drawPlaceholderBox(1051, 372, 24, 24);
    drawPlaceholderBox(1081, 372, 24, 24);
    drawLabel("ENV", 813, 402, 30);
    drawLabel("GATE", 844, 402, 34);
    drawLabel("MAN", 875, 402, 30);
    drawLabel("ENV", 1009, 402, 30);
    drawLabel("GATE", 1039, 402, 34);
    drawLabel("MAN", 1070, 402, 30);

    drawPlaceholderKnob(1190, 181, 46);
    drawPlaceholderKnob(1274, 181, 46);
    drawPlaceholderKnob(1358, 181, 46);
    drawLabel("MOD A", 1173, 154, 52);
    drawLabel("MOD B", 1256, 154, 52);
    drawLabel("MOD C", 1341, 154, 52);
    drawLabel("DEST", 1192, 243, 42);
    drawLabel("DEST", 1276, 243, 42);
    drawLabel("DEST", 1360, 243, 42);
    drawLabel("AMT", 1192, 349, 42);
    drawLabel("AMT", 1276, 349, 42);
    drawLabel("AMT", 1360, 349, 42);
    drawLabel("MODE", 1192, 402, 42);
    drawLabel("MODE", 1276, 402, 42);
    drawLabel("MODE", 1360, 402, 42);
    for (float x : { 1168.0f, 1198.0f, 1228.0f, 1252.0f, 1282.0f, 1312.0f, 1338.0f, 1368.0f, 1398.0f })
        drawPlaceholderBox(x, 388, 18, 18);

    drawPlaceholderKnob(1526, 205, 52);
    drawPlaceholderKnob(1608, 205, 52);
    drawLabel("ACTIVE", 1533, 142, 64);
    drawLabel("RATE", 1523, 265, 54);
    drawLabel("WAVE", 1607, 265, 54);
    drawPlaceholderBox(1512, 296, 28, 28);
    drawPlaceholderBox(1558, 296, 28, 28);
    drawPlaceholderBox(1605, 296, 28, 28);
    drawLabel("FREE", 1509, 337, 38);
    drawLabel("SYNC", 1553, 337, 38);
    drawLabel("STEP", 1601, 337, 38);
    drawMuted("SYNC", 1499, 399, 42);
    drawMuted("MIDI", 1548, 399, 42);
    drawMuted("INTERNAL", 1590, 399, 72);

    drawPlaceholderKnob(84, 541, 58, XTKnobStyle::transport);
    drawPlaceholderKnob(187, 546, 52, XTKnobStyle::transport);
    drawLabel("TEMPO", 78, 488, 56);
    drawLabel("SWING", 182, 488, 56);
    drawMuted("30", 71, 581, 24);
    drawMuted("300", 154, 581, 32);
    drawMuted("BPM", 101, 596, 32);
    drawPlaceholderBox(49, 618, 34, 24, "1X");
    drawPlaceholderBox(40, 679, 34, 24);
    drawPlaceholderBox(90, 679, 34, 24);
    drawPlaceholderBox(140, 679, 34, 24);
    drawPlaceholderBox(190, 679, 34, 24);
    drawMuted("RUN / STOP", 36, 705, 66, juce::Justification::left);
    drawMuted("TRIGGER", 89, 705, 50, juce::Justification::left);
    drawMuted("ADVANCE", 137, 705, 54, juce::Justification::left);
    drawMuted("RESET", 191, 705, 42, juce::Justification::left);
    drawMuted("STEP COUNT", 93, 757, 78);
    drawMuted("1 - 16", 104, 775, 56);
    drawPlaceholderKnob(180, 725, 38);

    drawLabel("SEQUENCER", 495, 454, 98, juce::Justification::left);
    drawMuted("16 STEPS  -  PITCH  -  VELOCITY  -  MOD A  -  MOD B  -  MOD C", 602, 456, 360, juce::Justification::left);
    drawMuted("PLAYHEAD", 407, 492, 64, juce::Justification::left);
    line(404, 509, 1650, 509, 0.8f);

    drawMutedLabel(g, { presetBox.getX() - 54, presetBox.getY() + 5, 46, 10 }, "PRESET",
                   juce::Justification::centredRight);

    const auto stepDisplayBounds = ref(74.0f, 754.0f, 196.0f, 78.0f);
    const auto stepDisplay = juce::String::formatted("%02d/16", juce::jlimit(1, XTSequencer::numSteps,
        currentLedStep >= 0 ? currentLedStep + 1 : 1));

    g.setColour(kDarkPlate);
    g.fillRoundedRectangle(stepDisplayBounds.toFloat(), 4.0f);
    g.setColour(juce::Colour(0xffefe7d8));
    g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    g.drawText("STEP", stepDisplayBounds.getX() + 16, stepDisplayBounds.getY() + 10, 48, 12, juce::Justification::left);
    g.setColour(juce::Colour(0xffdf5b31));
    g.setFont(juce::FontOptions(32.0f).withStyle("Bold"));
    g.drawText(stepDisplay, stepDisplayBounds.getX() + 18, stepDisplayBounds.getY() + 28,
               stepDisplayBounds.getWidth() - 36, 32, juce::Justification::centred);

    for (int i = 0; i < XTSequencer::numSteps; ++i)
    {
        const auto knobBounds = stepPitch[i].getBounds().toFloat();
        const float centreX = knobBounds.getCentreX();
        const float padY = knobBounds.getY() - 39.0f;
        const float ledY = knobBounds.getY() - 9.0f;
        const bool active = (i == currentLedStep);

        g.setColour(kInk);
        g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
        g.drawText(juce::String(i + 1), juce::Rectangle<float>(centreX - 12.0f, padY - 18.0f, 24.0f, 10.0f),
                   juce::Justification::centred, false);

        g.setColour(kDarkPlate);
        g.fillRoundedRectangle(centreX - 12.0f, padY, 24.0f, 24.0f, 3.0f);
        g.setColour(kDarkEdge.withAlpha(0.9f));
        g.drawRoundedRectangle(centreX - 12.0f, padY, 24.0f, 24.0f, 3.0f, 1.0f);

        g.setColour(kPanelBase);
        g.fillEllipse(centreX - 7.0f, ledY - 7.0f, 14.0f, 14.0f);

        if (active)
        {
            g.setColour(juce::Colour(0x22ff3a20));
            g.fillEllipse(centreX - 10.0f, ledY - 10.0f, 20.0f, 20.0f);
        }

        juce::ColourGradient ledGrad(active ? juce::Colour(0xffff5a34) : juce::Colour(0xff5b3326),
                                     centreX - 3.0f, ledY - 3.0f,
                                     active ? juce::Colour(0xff9b2014) : juce::Colour(0xff231512),
                                     centreX + 3.0f, ledY + 3.0f,
                                     true);
        g.setGradientFill(ledGrad);
        g.fillEllipse(centreX - 4.5f, ledY - 4.5f, 9.0f, 9.0f);
        g.setColour(active ? juce::Colour(0xff7f1d15) : juce::Colour(0xff181818));
        g.drawEllipse(centreX - 4.5f, ledY - 4.5f, 9.0f, 9.0f, 1.0f);
    }

    for (int row = 0; row < XTSequencer::numLaneRows; ++row)
    {
        juce::String subtitle = row == 0 ? "semitones" : row == 1 ? "accent" : getModLaneSubtitle(row - 2).toLowerCase();
        if (subtitle.isEmpty())
            subtitle = "assign";

        const auto first = stepPitch[0].getBounds().translated(0, row * 39);
        auto plate = juce::Rectangle<float>((float) first.getX() - 120.0f, (float) first.getY() - 3.0f, 62.0f, 40.0f);
        g.setColour(kDarkPlate);
        g.fillRoundedRectangle(plate, 3.0f);
        g.setColour(kDarkEdge);
        g.drawRoundedRectangle(plate, 3.0f, 1.0f);
        g.setColour(juce::Colour(0xffefe7d8));
        g.setFont(juce::FontOptions(10.5f).withStyle("Bold"));
        g.drawText(kSequencerLaneNames[(size_t) row], juce::Rectangle<int>((int) plate.getX() + 10, (int) plate.getY() + 7, (int) plate.getWidth() - 20, 12),
                   juce::Justification::left, false);
        g.setFont(juce::FontOptions(8.0f));
        g.drawText(subtitle, juce::Rectangle<int>((int) plate.getX() + 10, (int) plate.getY() + 21, (int) plate.getWidth() - 20, 10),
                   juce::Justification::left, false);
    }
}

void XTEditor::resized()
{
    const auto layout = createLayout(getWidth(), getHeight());
    const float uiScale = juce::jmin((float) getWidth() / 1720.0f, (float) getHeight() / 900.0f);
    auto ref = [&](float x, float y, float w, float h)
    {
        return mapReferenceRect(layout.panel, x, y, w, h);
    };

    auto presetArea = layout.presetControls.reduced(0, 1);
    presetArea.removeFromTop(2);
    presetArea.removeFromRight(juce::roundToInt(150.0f * uiScale));
    presetArea.removeFromLeft(4);
    presetBox.setBounds(presetArea.removeFromLeft(juce::roundToInt(122.0f * uiScale)).withHeight(juce::roundToInt(22.0f * uiScale)));
    presetArea.removeFromLeft(8);
    presetSaveButton.setBounds(presetArea.removeFromLeft(juce::roundToInt(38.0f * uiScale)).withHeight(juce::roundToInt(22.0f * uiScale)));
    presetArea.removeFromLeft(6);
    presetDeleteButton.setBounds(presetArea.removeFromLeft(juce::roundToInt(50.0f * uiScale)).withHeight(juce::roundToInt(22.0f * uiScale)));
    presetArea.removeFromLeft(6);
    presetInitButton.setBounds(presetArea.removeFromLeft(juce::roundToInt(38.0f * uiScale)).withHeight(juce::roundToInt(22.0f * uiScale)));

    // Controls with no clear sketch position — keep hidden
    seqPitchModBox.setVisible(false);
    noiseVcfMod.setVisible(false);
    vcfDecay.setVisible(false);
    vcaDecay.setVisible(false);
    vcaEg.setVisible(false);
    clockMultBox.setVisible(false);

    // --- OSCILLATORS ---
    // VCO 1 (LEVEL slot has no param → placeholder in paint)
    vco1Frequency.setBounds(ref(62.0f, 185.0f, 50.0f, 50.0f));
    vco1EgAmount.setBounds(ref(135.0f, 185.0f, 50.0f, 50.0f));
    vco1WaveBox.setBounds(ref(205.0f, 199.0f, 58.0f, 24.0f));
    vcoDecay.setBounds(ref(351.0f, 185.0f, 50.0f, 50.0f));

    // HARD SYNC toggle
    hardSyncBox.setBounds(ref(419.0f, 178.0f, 62.0f, 56.0f));

    // VCO 2 (LEVEL + LVL slots have no param → placeholder in paint)
    vco2Frequency.setBounds(ref(62.0f, 325.0f, 50.0f, 50.0f));
    vco2EgAmount.setBounds(ref(135.0f, 325.0f, 50.0f, 50.0f));
    vco2WaveBox.setBounds(ref(205.0f, 339.0f, 58.0f, 24.0f));

    // --- MIX / TRANSIENT ---
    // Top row: mixer levels (VCO1, VCO2, NOISE)
    vco1Level.setBounds(ref(507.0f, 166.0f, 50.0f, 50.0f));
    vco2Level.setBounds(ref(580.0f, 166.0f, 50.0f, 50.0f));
    noiseLevel.setBounds(ref(653.0f, 166.0f, 50.0f, 50.0f));
    // Bottom row: FM AMT live; CLK controls placeholder only
    fmAmount.setBounds(ref(507.0f, 272.0f, 50.0f, 50.0f));

    // --- FILTER / AMP / DRIVE ---
    // Filter section: x=806–1360. MOD controls start at x=1160 → filter knobs must end by ~1100.
    // Stride 72, size 62 → end at 820+3*72+62=1098.
    vcfModeBox.setBounds(ref(820.0f, 143.0f, 64.0f, 26.0f));
    cutoff.setBounds(ref(820.0f, 178.0f, 62.0f, 62.0f));
    resonance.setBounds(ref(892.0f, 178.0f, 62.0f, 62.0f));
    vcfEgAmount.setBounds(ref(964.0f, 178.0f, 62.0f, 62.0f));
    volume.setBounds(ref(1036.0f, 178.0f, 62.0f, 62.0f));

    // --- MODULATION ---
    modDestBox[0].setBounds(ref(1160.0f, 245.0f, 70.0f, 26.0f));
    modDestBox[1].setBounds(ref(1244.0f, 245.0f, 70.0f, 26.0f));
    modDestBox[2].setBounds(ref(1328.0f, 245.0f, 70.0f, 26.0f));
    modAmount[0].setBounds(ref(1173.0f, 328.0f, 54.0f, 54.0f));
    modAmount[1].setBounds(ref(1257.0f, 328.0f, 54.0f, 54.0f));
    modAmount[2].setBounds(ref(1341.0f, 328.0f, 54.0f, 54.0f));

    // --- TRANSPORT ---
    resetButton.setBounds(ref(191.0f, 679.0f, 34.0f, 24.0f));

    const float stepLeft = 525.0f;
    const float stepStride = 74.7f;
    const float stepKnobTop = 620.0f;
    const float stepKnobSize = 40.0f;
    const float stepRowStride = 52.0f;

    for (int i = 0; i < XTSequencer::numSteps; ++i)
    {
        const float x = stepLeft + (float) i * stepStride;
        stepPitch[i].setBounds(ref(x, stepKnobTop, stepKnobSize, stepKnobSize));
        stepVelocity[i].setBounds(ref(x, stepKnobTop + stepRowStride, stepKnobSize, stepKnobSize));
        stepModA[i].setBounds(ref(x, stepKnobTop + stepRowStride * 2.0f, stepKnobSize, stepKnobSize));
        stepModB[i].setBounds(ref(x, stepKnobTop + stepRowStride * 3.0f, stepKnobSize, stepKnobSize));
        stepModC[i].setBounds(ref(x, stepKnobTop + stepRowStride * 4.0f, stepKnobSize, stepKnobSize));
    }
}
