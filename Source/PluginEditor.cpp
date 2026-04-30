#include "PluginEditor.h"

namespace
{
constexpr int kEditorWidth  = 1320;
constexpr int kEditorHeight = 800;
constexpr int kOuterPadding = 10;
constexpr int kPanelCorner  = 12;
constexpr float kSketchWidth  = 1713.0f;
constexpr float kSketchHeight = 906.0f;

constexpr int kPresetInitId      = 1;
constexpr int kPresetFirstUserId = 2;
constexpr int kPresetMissingId   = 999;

constexpr std::array<const char*, XTSequencer::numLaneRows> kSequencerLaneNames
{
    "PITCH", "VEL", "MOD A", "MOD B"
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
    int headerLineY    = 0;
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

juce::Rectangle<int> mapReferenceRect(const juce::Rectangle<int>& target,
                                      float x, float y, float w, float h)
{
    const auto sx = (float)target.getWidth()  / kSketchWidth;
    const auto sy = (float)target.getHeight() / kSketchHeight;
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
    layout.panel = { kOuterPadding, kOuterPadding,
                     width  - kOuterPadding * 2,
                     height - kOuterPadding * 2 };

    auto inner  = layout.panel.reduced(22, 18);
    const int headerH = juce::roundToInt((float)inner.getHeight() * 0.08f);
    const int gapOne  = juce::roundToInt((float)inner.getHeight() * 0.034f);
    const int topH    = juce::roundToInt((float)inner.getHeight() * 0.405f);
    const int gapTwo  = juce::roundToInt((float)inner.getHeight() * 0.048f);

    layout.header       = inner.removeFromTop(headerH);
    layout.headerLineY  = layout.header.getBottom() + 10;
    inner.removeFromTop(gapOne);
    layout.topArea       = inner.removeFromTop(topH);
    layout.sectionDividerY = layout.topArea.getBottom() + 14;
    inner.removeFromTop(gapTwo);
    layout.bottomArea    = inner;

    auto top   = layout.topArea;
    const int topW = top.getWidth();
    layout.oscillators  = top.removeFromLeft(juce::roundToInt((float)topW * 0.28f));
    layout.mixTransient = top.removeFromLeft(juce::roundToInt((float)topW * 0.19f));
    layout.filter       = top.removeFromLeft(juce::roundToInt((float)topW * 0.23f));
    layout.modulation   = top.removeFromLeft(juce::roundToInt((float)topW * 0.18f));
    layout.lfo          = top;

    auto bottom = layout.bottomArea;
    layout.transport  = bottom.removeFromLeft(juce::roundToInt((float)bottom.getWidth() * 0.19f));
    bottom.removeFromLeft(18);
    layout.sequencer  = bottom;

    layout.presetControls = layout.header
        .removeFromRight(juce::roundToInt((float)layout.header.getWidth() * 0.27f))
        .withTrimmedTop(4);
    return layout;
}

juce::File getDefaultPresetDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("DFAFXT").getChildFile("Presets");
}

juce::String makeStepParameterId(const char* prefix, int index)
{
    return juce::String(prefix) + juce::String(index);
}
juce::String makeModDestinationParameterId(int index)
{
    return "mod" + juce::String::charToString((juce_wchar)('A' + index)) + "Dest";
}
void drawMutedLabel(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& text,
                    juce::Justification justification = juce::Justification::centred)
{
    g.setColour(kInk.withAlpha(0.7f));
    g.setFont(juce::FontOptions(8.0f));
    g.drawFittedText(text, bounds, justification, 1);
}

void drawKnobBody(juce::Graphics& g, juce::Point<float> centre, float size,
                  float normalizedValue, XTKnobStyle style)
{
    const float radius = size * 0.5f;
    const float x = centre.x - radius;
    const float y = centre.y - radius;

    g.setColour(juce::Colour(0x55000000));
    g.fillEllipse(x + 1.0f, y + size * 0.08f, size, size);

    juce::ColourGradient bodyGrad(kKnobTop, centre.x, y + 1.0f,
                                  kKnobBody, centre.x, y + size, false);
    g.setGradientFill(bodyGrad);
    g.fillEllipse(x, y, size, size);

    g.setColour(kKnobEdge);
    g.drawEllipse(x + 0.25f, y + 0.25f, size - 0.5f, size - 0.5f, 0.8f);

    if (style != XTKnobStyle::sequencer)
    {
        const float dotRadius = juce::jmax(0.7f, size * 0.025f);
        auto dotAt = [&](float ang) {
            const float cx = centre.x + (radius + size * 0.14f) * std::cos(ang - juce::MathConstants<float>::halfPi);
            const float cy = centre.y + (radius + size * 0.14f) * std::sin(ang - juce::MathConstants<float>::halfPi);
            g.setColour(kInk.withAlpha(0.55f));
            g.fillEllipse(cx - dotRadius, cy - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
        };
        const float sweep = juce::MathConstants<float>::pi * 0.74f;
        dotAt(-sweep); dotAt(0.0f); dotAt(sweep);
    }

    const float pointerAngle = juce::jmap(normalizedValue, 0.0f, 1.0f,
        -juce::MathConstants<float>::pi * 0.74f,
         juce::MathConstants<float>::pi * 0.74f);
    const auto dir   = juce::Point<float>(
        std::cos(pointerAngle - juce::MathConstants<float>::halfPi),
        std::sin(pointerAngle - juce::MathConstants<float>::halfPi));
    const float pInner = style == XTKnobStyle::sequencer ? 0.15f : 0.12f;
    const float pOuter = style == XTKnobStyle::sequencer ? 0.82f : 0.86f;
    const auto  inner  = centre + dir * (radius * pInner);
    const auto  outer  = centre + dir * (radius * pOuter);
    const float thick  = style == XTKnobStyle::sequencer ? 1.6f
                       : style == XTKnobStyle::main      ? 2.4f : 2.0f;
    g.setColour(juce::Colour(0xfff5efe0));
    g.drawLine(inner.x, inner.y, outer.x, outer.y, thick);
}

XTKnobStyle getKnobStyle(const juce::Slider& slider)
{
    if (const auto* s = dynamic_cast<const XTSlider*>(&slider)) return s->style;
    return XTKnobStyle::main;
}
XTComboStyle getComboStyle(const juce::ComboBox& box)
{
    if (const auto* b = dynamic_cast<const XTComboBox*>(&box)) return b->style;
    return XTComboStyle::led;
}
XTButtonStyle getButtonStyle(const juce::Button& button)
{
    if (const auto* b = dynamic_cast<const XTTextButton*>(&button)) return b->style;
    return XTButtonStyle::utility;
}

} // namespace

// =============================================================================
// XTLookAndFeel
// =============================================================================

XTLookAndFeel::XTLookAndFeel()
{
    setColour(juce::Slider::rotarySliderFillColourId,    juce::Colour(0xffd4c7ae));
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff282826));
    setColour(juce::Slider::thumbColourId,               juce::Colours::white);
    setColour(juce::ComboBox::backgroundColourId,  kDarkPlate);
    setColour(juce::ComboBox::outlineColourId,     kDarkEdge);
    setColour(juce::ComboBox::textColourId,        juce::Colour(0xffefe7d8));
    setColour(juce::ComboBox::arrowColourId,       juce::Colour(0xffefe7d8));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff201f1d));
    setColour(juce::PopupMenu::textColourId,       juce::Colour(0xffefe7d8));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff9e2f24));
    setColour(juce::TextButton::buttonColourId,    kDarkPlate);
    setColour(juce::TextButton::buttonOnColourId,  kAccentRed);
    setColour(juce::TextButton::textColourOffId,   juce::Colour(0xffefe7d8));
    setColour(juce::TextButton::textColourOnId,    juce::Colour(0xffefe7d8));
}

void XTLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                     float sliderPos, float, float, juce::Slider& slider)
{
    const auto style  = getKnobStyle(slider);
    const float size  = (float)juce::jmin(width, height) - (style == XTKnobStyle::sequencer ? 2.0f : 4.0f);
    const auto centre = juce::Point<float>((float)x + (float)width * 0.5f,
                                           (float)y + (float)height * 0.5f);
    drawKnobBody(g, centre, size, sliderPos, style);
}

void XTLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                 int, int, int, int, juce::ComboBox& box)
{
    const auto style  = getComboStyle(box);
    const auto bounds = juce::Rectangle<float>(0.5f, 0.5f, (float)width - 1.0f, (float)height - 1.0f);

    if (style == XTComboStyle::led)
    {
        g.setColour(juce::Colour(0xff080706));
        g.fillRoundedRectangle(bounds, 3.0f);
        juce::ColourGradient glow(kAmberText.withAlpha(0.07f), (float)width * 0.5f, (float)height,
                                  juce::Colours::transparentBlack, (float)width * 0.5f, 0.0f, false);
        g.setGradientFill(glow);
        g.fillRoundedRectangle(bounds, 3.0f);
        g.setColour(kAmberText.withAlpha(0.45f));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
        return;
    }

    if (style == XTComboStyle::toggle)
    {
        g.setColour(juce::Colour(0xff0c0b0a));
        g.fillRoundedRectangle(bounds, 5.0f);
        g.setColour(juce::Colour(0xff282422));
        g.drawRoundedRectangle(bounds, 5.0f, 1.2f);

        const float slotX = (float)width * 0.5f - 1.5f;
        g.setColour(juce::Colour(0xff1e1c1a));
        g.fillRoundedRectangle(slotX, bounds.getY() + 6.0f, 3.0f, bounds.getHeight() - 12.0f, 1.5f);

        const bool   isOn = (box.getSelectedId() == 2);
        const float  tabH = bounds.getHeight() * 0.42f;
        const float  margin = bounds.getHeight() * 0.09f;
        const float  tabY = isOn ? (bounds.getBottom() - tabH - margin) : (bounds.getY() + margin);
        const float  tabX = bounds.getX() + bounds.getWidth() * 0.12f;
        const float  tabW = bounds.getWidth() * 0.76f;

        g.setColour(juce::Colour(0x55000000));
        g.fillRoundedRectangle(tabX + 1.0f, tabY + 2.0f, tabW, tabH, 4.0f);

        juce::ColourGradient tabGrad(juce::Colour(0xff404038), tabX, tabY,
                                     juce::Colour(0xff262421), tabX, tabY + tabH, false);
        g.setGradientFill(tabGrad);
        g.fillRoundedRectangle(tabX, tabY, tabW, tabH, 4.0f);
        g.setColour(juce::Colour(0xff171512));
        g.drawRoundedRectangle(tabX, tabY, tabW, tabH, 4.0f, 1.0f);

        const float ledCX = tabX + tabW * 0.5f;
        const float ledCY = tabY + tabH * 0.5f;
        const float ledR  = tabW * 0.12f;
        if (isOn) {
            g.setColour(kLedOn.withAlpha(0.6f));
            g.fillEllipse(ledCX - ledR*2.2f, ledCY - ledR*2.2f, ledR*4.4f, ledR*4.4f);
        }
        g.setColour(isOn ? kLedOn : kLedOff);
        g.fillEllipse(ledCX - ledR, ledCY - ledR, ledR*2.0f, ledR*2.0f);

        g.setColour(juce::Colour(0xffefe7d8).withAlpha(0.55f));
        g.setFont(juce::FontOptions(7.5f).withStyle("Bold"));
        const float labelW = (float)width;
        juce::ignoreUnused(labelW);
        if (!isOn) g.setColour(juce::Colour(0xffefe7d8));
        g.drawText("OFF", 0, (int)bounds.getY() + 3, width, 10, juce::Justification::centred, false);
        g.setColour(juce::Colour(0xffefe7d8).withAlpha(isOn ? 1.0f : 0.55f));
        g.drawText("ON",  0, (int)bounds.getBottom() - 13, width, 10, juce::Justification::centred, false);
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
    chevron.startNewSubPath((float)width - 24.0f, (float)height * 0.42f);
    chevron.lineTo((float)width - 16.0f, (float)height * 0.62f);
    chevron.lineTo((float)width - 8.0f,  (float)height * 0.42f);
    g.setColour(juce::Colour(0xffefe7d8));
    g.strokePath(chevron, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
}

void XTLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    const auto style = getComboStyle(box);
    if (style == XTComboStyle::toggle) { label.setBounds(0,0,0,0); return; }
    if (style == XTComboStyle::led)
    {
        label.setBounds(2, 0, box.getWidth()-4, box.getHeight());
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, kAmberText);
        label.setFont(getComboBoxFont(box));
        return;
    }
    label.setBounds(10, 0, box.getWidth()-28, box.getHeight());
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centred);
}

juce::Font XTLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    const auto style = getComboStyle(box);
    if (style == XTComboStyle::led)
        return juce::Font(juce::FontOptions((float)juce::jmin(12, box.getHeight()-4)).withStyle("Bold"));
    if (style == XTComboStyle::preset)
        return juce::Font(juce::FontOptions(11.0f).withStyle("Bold"));
    return juce::Font(juce::FontOptions((float)juce::jmin(13, box.getHeight()-6)).withStyle("Bold"));
}

void XTLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                         const juce::Colour&,
                                         bool shouldDrawButtonAsHighlighted,
                                         bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const auto style = getButtonStyle(button);

    if (style == XTButtonStyle::led)
    {
        const bool on = button.getToggleState() || shouldDrawButtonAsDown;
        const float w = bounds.getWidth(), h = bounds.getHeight();

        g.setColour(juce::Colour(0xff0c0b0a));
        g.fillRoundedRectangle(bounds, 3.0f);
        g.setColour(juce::Colour(0xff282422));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

        const float ledR  = juce::jmin(w, h) * 0.14f;
        const float ledCX = bounds.getCentreX();
        const float ledCY = bounds.getY() + h * 0.32f;
        if (on) {
            g.setColour(kLedOn.withAlpha(0.35f));
            g.fillEllipse(ledCX - ledR*2.8f, ledCY - ledR*2.8f, ledR*5.6f, ledR*5.6f);
        }
        g.setColour(on ? kLedOn : kLedOff);
        g.fillEllipse(ledCX - ledR, ledCY - ledR, ledR*2.0f, ledR*2.0f);
        g.setColour(on ? juce::Colour(0xff7f1d15) : juce::Colour(0xff181818));
        g.drawEllipse(ledCX - ledR, ledCY - ledR, ledR*2.0f, ledR*2.0f, 0.8f);

        if (shouldDrawButtonAsHighlighted && !on)
        {
            g.setColour(juce::Colour(0x18ffffff));
            g.fillRoundedRectangle(bounds, 3.0f);
        }
        return;
    }

    auto fill = kDarkPlate;
    const bool isSquare = style == XTButtonStyle::square;
    if (shouldDrawButtonAsDown)        fill = fill.brighter(0.12f);
    else if (shouldDrawButtonAsHighlighted) fill = fill.brighter(0.06f);

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
    if (button.getButtonText().isEmpty()) return;
    const auto style = getButtonStyle(button);
    if (style == XTButtonStyle::led)
    {
        const auto bounds = button.getLocalBounds();
        const int textY = bounds.getY() + juce::roundToInt((float)bounds.getHeight() * 0.52f);
        g.setFont(juce::FontOptions(7.5f).withStyle("Bold"));
        g.setColour(juce::Colour(0xffefe7d8).withAlpha(0.85f));
        g.drawFittedText(button.getButtonText(), bounds.getX(), textY,
                         bounds.getWidth(), bounds.getBottom() - textY,
                         juce::Justification::centred, 1);
        return;
    }
    g.setFont(juce::FontOptions(style == XTButtonStyle::square ? 7.5f : 8.5f).withStyle("Bold"));
    g.setColour(juce::Colour(0xffefe7d8));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds(),
                     juce::Justification::centred, 1);
}

// =============================================================================
// XTEditor constructor
// =============================================================================

XTEditor::XTEditor(XTProcessor& p)
    : AudioProcessorEditor(&p), xtProcessor(p)
{
    setSize(kEditorWidth, kEditorHeight);
    setLookAndFeel(&laf);
    startTimerHz(24);

    // --- Preset controls ---
    setupChoice(presetBox);
    presetBox.setJustificationType(juce::Justification::centredLeft);
    presetBox.onChange = [this]()
    {
        if (isUpdatingPresetBox) return;
        const int id = presetBox.getSelectedId();
        if (id == kPresetMissingId) return;
        if (id == kPresetInitId) xtProcessor.loadInitPreset();
        else                     xtProcessor.loadPreset(presetBox.getText());
        refreshPresetControls();
    };
    addAndMakeVisible(presetBox);

    auto styleButton = [this](XTTextButton& button, std::function<void()> onClick)
    {
        button.onClick = std::move(onClick);
        button.setClickingTogglesState(false);
        addAndMakeVisible(button);
    };

    styleButton(presetSaveButton,   [this]() { promptSavePreset(); });
    styleButton(presetDeleteButton, [this]()
    {
        const auto name = xtProcessor.getCurrentPresetName();
        if (name.isEmpty() || name == "Init") return;
        if (juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon,
            "Delete Preset", "Delete preset \"" + name + "\"?",
            "Delete", "Cancel", this, nullptr))
        {
            xtProcessor.deletePreset(name);
            refreshPresetControls();
        }
    });
    styleButton(presetInitButton, [this]() { xtProcessor.loadInitPreset(); refreshPresetControls(); });

    // --- Transport buttons ---
    runStopButton.setClickingTogglesState(true);
    runStopButton.onClick = [this]()
    {
        bool running = xtProcessor.internalTransportRunning.load(std::memory_order_relaxed);
        xtProcessor.internalTransportRunning.store(!running, std::memory_order_relaxed);
        if (!running) xtProcessor.resetSequencer();
        runStopButton.setToggleState(!running, juce::dontSendNotification);
    };
    addAndMakeVisible(runStopButton);

    triggerButton.setClickingTogglesState(false);
    triggerButton.onClick = [this]() { xtProcessor.triggerPending.store(true, std::memory_order_relaxed); };
    addAndMakeVisible(triggerButton);

    advanceButton.setClickingTogglesState(false);
    advanceButton.onClick = [this]() { xtProcessor.advancePending.store(true, std::memory_order_relaxed); };
    addAndMakeVisible(advanceButton);

    resetButton.onClick = [this]()
    {
        xtProcessor.resetSequencer();
        currentLedStep  = editPage * 8;
        resetLedActive  = true;
        repaint();
    };
    resetButton.setButtonText({});
    resetButton.setTooltip("Reset Sequencer");
    addAndMakeVisible(resetButton);

    // --- Pattern page buttons ---
    pageAButton.setClickingTogglesState(true);
    pageBButton.setClickingTogglesState(true);
    copyPageButton.setClickingTogglesState(false);
    pageAButton.onClick = [this]() { switchEditPage(0); };
    pageBButton.onClick = [this]() { switchEditPage(1); };
    copyPageButton.onClick = [this]() { xtProcessor.copyPageAtoB(); };
    addAndMakeVisible(pageAButton);
    addAndMakeVisible(pageBButton);
    addAndMakeVisible(copyPageButton);

    // --- Knobs / combos ---
    auto addKnob   = [this](XTSlider&   s) { setupKnob(s);   addAndMakeVisible(s); };
    auto addChoice = [this](XTComboBox& b) { setupChoice(b);  addAndMakeVisible(b); };

    // OSC
    addKnob(vcoDecay);    addKnob(vco1EgAmount);  addKnob(vco1Frequency);
    addKnob(fmAmount);    addKnob(vco2EgAmount);  addKnob(vco2Frequency);
    addKnob(vco1Level);   addKnob(vco2Level);     addKnob(noiseLevel);
    addKnob(vco2Decay);

    vcoEgShapeBox.addItem("EXP", 1); vcoEgShapeBox.addItem("LIN", 2); vcoEgShapeBox.addItem("LOG", 3);
    addChoice(vcoEgShapeBox);

    // Click / mix
    addKnob(clickTune);  addKnob(clickDecay);  addKnob(clickLevel);
    addKnob(noiseColor); addKnob(noiseDecay);  addKnob(pitchFmAmt);  addKnob(velVcfDecaySens);
    noiseVcfBypassButton.setClickingTogglesState(true);
    clickVcfBypassButton.setClickingTogglesState(true);
    addAndMakeVisible(noiseVcfBypassButton);
    addAndMakeVisible(clickVcfBypassButton);

    // Filter / amp
    addKnob(cutoff);     addKnob(resonance);   addKnob(vcfDecay);
    addKnob(vcfEgAmount); addKnob(noiseVcfMod); addKnob(vcaDecay);
    addKnob(vcaEg);      addKnob(volume);      addKnob(vcaAttack);
    addKnob(preDrive);   addKnob(postDrive);

    vcfModeBox.addItem("LP", 1); vcfModeBox.addItem("HP", 2);
    addChoice(vcfModeBox);

    // Modulation lanes
    const auto destNames = XTProcessor::getModDestinationNames();
    for (int i = 0; i < 2; ++i)
    {
        modDestBox[i].setJustificationType(juce::Justification::centred);
        for (int item = 0; item < destNames.size(); ++item)
            modDestBox[i].addItem(destNames[item], item + 1);
        addChoice(modDestBox[i]);
    }

    // LFO
    addKnob(lfoRate); addKnob(lfoAmt);
    lfoWaveBox.addItem("TRI",1); lfoWaveBox.addItem("SAW",2);
    lfoWaveBox.addItem("SQR",3); lfoWaveBox.addItem("RND",4);
    addChoice(lfoWaveBox);
    lfoSyncBox.addItem("OFF",1);   lfoSyncBox.addItem("ON",2);   addChoice(lfoSyncBox);
    lfoRetrigBox.addItem("OFF",1); lfoRetrigBox.addItem("ON",2); addChoice(lfoRetrigBox);
    lfoDstBox.setJustificationType(juce::Justification::centred);
    for (int i = 0; i < destNames.size(); ++i) lfoDstBox.addItem(destNames[i], i + 1);
    addChoice(lfoDstBox);

    // Sequencer combos
    seqPitchModBox.addItem("VCO 1&2", 1); seqPitchModBox.addItem("OFF", 2); seqPitchModBox.addItem("VCO 2", 3);
    addChoice(seqPitchModBox);
    hardSyncBox.addItem("OFF",1); hardSyncBox.addItem("ON",2); addChoice(hardSyncBox);
    vco1WaveBox.addItem("SQUARE",1); vco1WaveBox.addItem("TRIANGLE",2); vco1WaveBox.addItem("METAL",3);
    addChoice(vco1WaveBox);
    vco2WaveBox.addItem("SQUARE",1); vco2WaveBox.addItem("TRIANGLE",2); addChoice(vco2WaveBox);

    clockMultBox.addItem("1/8",1); clockMultBox.addItem("1/5",2); clockMultBox.addItem("1/4",3);
    clockMultBox.addItem("1/3",4); clockMultBox.addItem("1/2",5); clockMultBox.addItem("1X", 6);
    clockMultBox.addItem("2X", 7); clockMultBox.addItem("3X", 8); clockMultBox.addItem("4X", 9);
    clockMultBox.addItem("5X",10);
    addChoice(clockMultBox);

    // Transport sliders
    addKnob(tempoSlider); addKnob(swingSlider); addKnob(stepCountSlider);

    // Step lanes
    for (int i = 0; i < XTSequencer::numSteps; ++i)
    {
        stepPitch[i].style    = XTKnobStyle::sequencer;
        stepVelocity[i].style = XTKnobStyle::sequencer;
        stepModA[i].style     = XTKnobStyle::sequencer;
        stepModB[i].style     = XTKnobStyle::sequencer;
        addKnob(stepPitch[i]);    addKnob(stepVelocity[i]);
        addKnob(stepModA[i]);     addKnob(stepModB[i]);
    }

    // Step active buttons
    for (int i = 0; i < XTSequencer::numSteps; ++i)
    {
        stepActiveButton[i].setClickingTogglesState(true);
        stepActiveButton[i].setToggleState(true, juce::dontSendNotification);
        addAndMakeVisible(stepActiveButton[i]);
    }

    // --- APVTS attachments ---
    auto& apvts = p.apvts;
    auto setDefault = [&apvts](juce::Slider& slider, const juce::String& id)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
            slider.setDoubleClickReturnValue(true, param->convertFrom0to1(param->getDefaultValue()));
    };

    vcoDecayAtt       = std::make_unique<SliderAttachment>(apvts, "vcoDecay",    vcoDecay);
    vco1FreqAtt       = std::make_unique<SliderAttachment>(apvts, "vco1Freq",    vco1Frequency);
    vco1EgAmtAtt      = std::make_unique<SliderAttachment>(apvts, "vco1EgAmt",   vco1EgAmount);
    fmAmountAtt       = std::make_unique<SliderAttachment>(apvts, "fmAmount",    fmAmount);
    vco2FreqAtt       = std::make_unique<SliderAttachment>(apvts, "vco2Freq",    vco2Frequency);
    vco2EgAmtAtt      = std::make_unique<SliderAttachment>(apvts, "vco2EgAmt",   vco2EgAmount);
    vco1LevelAtt      = std::make_unique<SliderAttachment>(apvts, "vco1Level",   vco1Level);
    vco2LevelAtt      = std::make_unique<SliderAttachment>(apvts, "vco2Level",   vco2Level);
    noiseLevelAtt     = std::make_unique<SliderAttachment>(apvts, "noiseLevel",  noiseLevel);
    vco2DecayAtt      = std::make_unique<SliderAttachment>(apvts, "vco2Decay",   vco2Decay);
    cutoffAtt         = std::make_unique<SliderAttachment>(apvts, "cutoff",      cutoff);
    resonanceAtt      = std::make_unique<SliderAttachment>(apvts, "resonance",   resonance);
    vcfDecayAtt       = std::make_unique<SliderAttachment>(apvts, "vcfDecay",    vcfDecay);
    vcfEgAmtAtt       = std::make_unique<SliderAttachment>(apvts, "vcfEgAmt",    vcfEgAmount);
    noiseVcfModAtt    = std::make_unique<SliderAttachment>(apvts, "noiseVcfMod", noiseVcfMod);
    vcaDecayAtt       = std::make_unique<SliderAttachment>(apvts, "vcaDecay",    vcaDecay);
    vcaEgAtt          = std::make_unique<SliderAttachment>(apvts, "vcaEg",       vcaEg);
    volumeAtt         = std::make_unique<SliderAttachment>(apvts, "volume",      volume);
    clickTuneAtt      = std::make_unique<SliderAttachment>(apvts, "clickTune",   clickTune);
    clickDecayAtt     = std::make_unique<SliderAttachment>(apvts, "clickDecay",  clickDecay);
    clickLevelAtt     = std::make_unique<SliderAttachment>(apvts, "clickLevel",  clickLevel);
    vcaAttackAtt      = std::make_unique<SliderAttachment>(apvts, "vcaAttack",   vcaAttack);
    preDriveAtt       = std::make_unique<SliderAttachment>(apvts, "preDrive",    preDrive);
    postDriveAtt      = std::make_unique<SliderAttachment>(apvts, "postDrive",   postDrive);
    noiseColorAtt     = std::make_unique<SliderAttachment>(apvts, "noiseColor",  noiseColor);
    noiseDecayAtt     = std::make_unique<SliderAttachment>(apvts, "noiseDecay",  noiseDecay);
    noiseVcfBypassAtt = std::make_unique<ButtonAttachment>(apvts, "noiseVcfBypass", noiseVcfBypassButton);
    clickVcfBypassAtt = std::make_unique<ButtonAttachment>(apvts, "clickVcfBypass", clickVcfBypassButton);
    pitchFmAmtAtt     = std::make_unique<SliderAttachment>(apvts, "pitchFmAmt",  pitchFmAmt);
    velVcfDecaySensAtt= std::make_unique<SliderAttachment>(apvts, "velVcfDecaySens", velVcfDecaySens);
    lfoRateAtt        = std::make_unique<SliderAttachment>(apvts, "lfoRate",     lfoRate);
    lfoAmtAtt         = std::make_unique<SliderAttachment>(apvts, "lfoAmt",      lfoAmt);
    tempoAtt          = std::make_unique<SliderAttachment>(apvts, "tempo",       tempoSlider);
    swingAtt          = std::make_unique<SliderAttachment>(apvts, "swing",       swingSlider);
    stepCountAtt      = std::make_unique<SliderAttachment>(apvts, "stepCount",   stepCountSlider);

    seqPitchModBoxAtt  = std::make_unique<ComboAttachment>(apvts, "seqPitchMod", seqPitchModBox);
    hardSyncBoxAtt     = std::make_unique<ComboAttachment>(apvts, "hardSync",    hardSyncBox);
    vco1WaveBoxAtt     = std::make_unique<ComboAttachment>(apvts, "vco1Wave",    vco1WaveBox);
    vco2WaveBoxAtt     = std::make_unique<ComboAttachment>(apvts, "vco2Wave",    vco2WaveBox);
    vcfModeBoxAtt      = std::make_unique<ComboAttachment>(apvts, "vcfMode",     vcfModeBox);
    clockMultBoxAtt    = std::make_unique<ComboAttachment>(apvts, "clockMult",   clockMultBox);
    vcoEgShapeBoxAtt   = std::make_unique<ComboAttachment>(apvts, "vcoEgShape",  vcoEgShapeBox);
    lfoWaveBoxAtt      = std::make_unique<ComboAttachment>(apvts, "lfoWave",     lfoWaveBox);
    lfoSyncBoxAtt      = std::make_unique<ComboAttachment>(apvts, "lfoSync",     lfoSyncBox);
    lfoRetrigBoxAtt    = std::make_unique<ComboAttachment>(apvts, "lfoRetrig",   lfoRetrigBox);
    lfoDstBoxAtt       = std::make_unique<ComboAttachment>(apvts, "lfoDest",     lfoDstBox);

    for (int i = 0; i < 2; ++i)
        modDestBoxAtt[i] = std::make_unique<ComboAttachment>(apvts, makeModDestinationParameterId(i), modDestBox[i]);

    for (int i = 0; i < XTSequencer::numSteps; ++i)
    {
        const auto pitchId = makeStepParameterId("stepPitch", i);
        const auto velId   = makeStepParameterId("stepVel",   i);
        const auto modAId  = makeStepParameterId("stepModA",  i);
        const auto modBId  = makeStepParameterId("stepModB",  i);
        stepPitchAtt[i] = std::make_unique<SliderAttachment>(apvts, pitchId, stepPitch[i]);
        stepVelAtt[i]   = std::make_unique<SliderAttachment>(apvts, velId,   stepVelocity[i]);
        stepModAAtt[i]  = std::make_unique<SliderAttachment>(apvts, modAId,  stepModA[i]);
        stepModBAtt[i]  = std::make_unique<SliderAttachment>(apvts, modBId,  stepModB[i]);
        setDefault(stepPitch[i],    pitchId);
        setDefault(stepVelocity[i], velId);
        setDefault(stepModA[i],     modAId);
        setDefault(stepModB[i],     modBId);
        const auto activeId = makeStepParameterId("stepActive", i);
        stepActiveAtt[i] = std::make_unique<ButtonAttachment>(apvts, activeId, stepActiveButton[i]);
    }

    vco1EgAmount.setDoubleClickReturnValue(true, 0.0);
    vco2EgAmount.setDoubleClickReturnValue(true, 0.0);
    vcfEgAmount.setDoubleClickReturnValue(true, 0.0);
    vcaEg.setVisible(false);

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
        box.setColour(juce::ComboBox::outlineColourId,    kAmberText.withAlpha(0.45f));
        box.setColour(juce::ComboBox::textColourId,       kAmberText);
        box.setColour(juce::ComboBox::arrowColourId,      juce::Colours::transparentBlack);
    }
    else if (box.style == XTComboStyle::toggle)
    {
        box.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0c0b0a));
        box.setColour(juce::ComboBox::outlineColourId,    juce::Colour(0xff282422));
        box.setColour(juce::ComboBox::textColourId,       juce::Colours::transparentBlack);
        box.setColour(juce::ComboBox::arrowColourId,      juce::Colours::transparentBlack);
    }
    else
    {
        box.setColour(juce::ComboBox::backgroundColourId, kDarkPlate);
        box.setColour(juce::ComboBox::outlineColourId,    kDarkEdge);
        box.setColour(juce::ComboBox::textColourId,       juce::Colour(0xffefe7d8));
        box.setColour(juce::ComboBox::arrowColourId,      juce::Colour(0xffefe7d8));
    }
}

void XTEditor::refreshPresetControls()
{
    const auto presetNames   = xtProcessor.getAvailablePresetNames();
    const auto currentPreset = xtProcessor.getCurrentPresetName();

    isUpdatingPresetBox = true;
    presetBox.clear(juce::dontSendNotification);
    presetBox.addItem("INIT", kPresetInitId);
    for (int i = 0; i < presetNames.size(); ++i)
        presetBox.addItem(presetNames[i], kPresetFirstUserId + i);

    if (currentPreset.isEmpty() || currentPreset == "Init")
        presetBox.setSelectedId(kPresetInitId, juce::dontSendNotification);
    else
    {
        const int idx = presetNames.indexOf(currentPreset);
        if (idx >= 0)
            presetBox.setSelectedId(kPresetFirstUserId + idx, juce::dontSendNotification);
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
    const auto cur = xtProcessor.getCurrentPresetName();
    if (cur.isNotEmpty() && cur != "Init" && xtProcessor.getAvailablePresetNames().contains(cur))
    {
        xtProcessor.saveCurrentPreset();
        refreshPresetControls();
        return;
    }
    auto dir = getDefaultPresetDirectory();
    dir.createDirectory();
    const auto suggestedName = cur == "Init" ? juce::String("New Preset") : cur;
    const auto suggestedFile = dir.getChildFile(suggestedName + ".dfafxtpreset");

    presetSaveChooser = std::make_unique<juce::FileChooser>(
        "Save DFAF XT Preset", suggestedFile, "*.dfafxtpreset");
    presetSaveChooser->launchAsync(juce::FileBrowserComponent::saveMode
                                       | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& chooser)
        {
            const auto result = chooser.getResult();
            presetSaveChooser.reset();
            if (result == juce::File()) return;
            xtProcessor.savePreset(result.getFileNameWithoutExtension());
            refreshPresetControls();
        });
}

void XTEditor::updatePresetButtonState()
{
    const auto cur = xtProcessor.getCurrentPresetName();
    presetDeleteButton.setEnabled(cur.isNotEmpty() && cur != "Init"
                                  && xtProcessor.getAvailablePresetNames().contains(cur));
}

juce::String XTEditor::getModLaneSubtitle(int modLaneIndex) const
{
    if (!juce::isPositiveAndBelow(modLaneIndex, 3)) return {};
    auto text = modDestBox[modLaneIndex].getText().toUpperCase().trim();
    if (text.isEmpty() || text == "OFF") text = "ASSIGN";
    return text;
}

void XTEditor::switchEditPage(int page)
{
    editPage = juce::jlimit(0, 1, page);
    pageAButton.setToggleState(editPage == 0, juce::dontSendNotification);
    pageBButton.setToggleState(editPage == 1, juce::dontSendNotification);

    for (int i = 0; i < 8; ++i)
    {
        const bool showA = (editPage == 0);
        stepPitch[i].setVisible(showA);        stepVelocity[i].setVisible(showA);
        stepModA[i].setVisible(showA);         stepModB[i].setVisible(showA);
        stepActiveButton[i].setVisible(showA);

        stepPitch[i + 8].setVisible(!showA);   stepVelocity[i + 8].setVisible(!showA);
        stepModA[i + 8].setVisible(!showA);    stepModB[i + 8].setVisible(!showA);
        stepActiveButton[i + 8].setVisible(!showA);
    }

    xtProcessor.setPlayPage(editPage);
}

void XTEditor::timerCallback()
{
    const auto cur = xtProcessor.getCurrentPresetName();
    if (cur != lastPresetName) refreshPresetControls();

    const int step = xtProcessor.getCurrentStep();
    if (step >= 0) resetLedActive = false;

    // Sync RUN/STOP button visual state with processor
    runStopButton.setToggleState(
        xtProcessor.internalTransportRunning.load(std::memory_order_relaxed),
        juce::dontSendNotification);

    currentLedStep = resetLedActive ? 0 : step;
    repaint();
}

// =============================================================================
// paint()
// =============================================================================

void XTEditor::paint(juce::Graphics& g)
{
    const auto layout = createLayout(getWidth(), getHeight());
    const auto ref = [&](float x, float y, float w, float h)
    {
        return mapReferenceRect(layout.panel, x, y, w, h);
    };

    g.fillAll(juce::Colour(0xff161513));

    juce::ColourGradient panelGrad(kPanelBase,  (float)layout.panel.getX(),    (float)layout.panel.getY(),
                                   kPanelShade, (float)layout.panel.getRight(), (float)layout.panel.getBottom(), false);
    g.setGradientFill(panelGrad);
    g.fillRoundedRectangle(layout.panel.toFloat(), (float)kPanelCorner);
    g.setColour(juce::Colour(0xff2d2a26));
    g.drawRoundedRectangle(layout.panel.toFloat(), (float)kPanelCorner, 2.0f);

    auto drawScrew = [&g](juce::Rectangle<int> r) {
        const auto c = r.getCentre().toFloat();
        g.setColour(juce::Colour(0xff3c3935));
        g.fillEllipse((float)r.getX(), (float)r.getY(), (float)r.getWidth(), (float)r.getHeight());
        g.setColour(juce::Colour(0xff1b1917));
        g.drawEllipse((float)r.getX(), (float)r.getY(), (float)r.getWidth(), (float)r.getHeight(), 1.2f);
        g.drawLine(c.x-3.5f, c.y, c.x+3.5f, c.y, 1.0f);
        g.drawLine(c.x, c.y-3.5f, c.x, c.y+3.5f, 1.0f);
    };
    drawScrew(ref(16,13,24,24)); drawScrew(ref(1687,13,24,24));
    drawScrew(ref(16,869,24,24)); drawScrew(ref(1687,869,24,24));

    g.setColour(kDivider);
    auto line = [&](float x1, float y1, float x2, float y2, float t = 1.0f) {
        const auto a = ref(x1,y1,0,0).getPosition();
        const auto b = ref(x2,y2,0,0).getPosition();
        g.drawLine((float)a.x, (float)a.y, (float)b.x, (float)b.y, t);
    };
    line(42, 98, 1670, 98);
    line(42, 442, 1670, 442);
    line(806,109,806,432, 0.8f);
    line(1360,109,1360,432, 0.8f);
    line(1463,109,1463,432, 0.8f);
    line(345,454,345,844, 0.8f);

    auto drawTitle = [&](const juce::String& text, float x, float y, float w) {
        g.setColour(kInk);
        g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
        g.drawText(text, ref(x,y,w,16), juce::Justification::centred, false);
    };
    auto drawLabel = [&](const juce::String& text, float x, float y, float w,
                         juce::Justification j = juce::Justification::centred) {
        g.setColour(kInk);
        g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
        g.drawFittedText(text, ref(x,y,w,12), j, 1);
    };
    auto drawMuted = [&](const juce::String& text, float x, float y, float w,
                         juce::Justification j = juce::Justification::centred) {
        g.setColour(kInk.withAlpha(0.72f));
        g.setFont(juce::FontOptions(8.0f));
        g.drawFittedText(text, ref(x,y,w,10), j, 1);
    };

    // Header
    g.setColour(kInk);
    g.setFont(juce::FontOptions(38.0f).withStyle("Bold"));
    g.drawText("DFAF", ref(58,25,130,44), juce::Justification::centredLeft, false);
    g.setColour(kAccentRed);
    g.drawText("XT", ref(197,25,78,44), juce::Justification::centredLeft, false);
    g.setColour(kInk.withAlpha(0.92f));
    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    g.drawText("SINGLE-VOICE PERCUSSION SYNTHESIZER", ref(63,72,350,16), juce::Justification::left, false);

    drawTitle("OSC / MIXER",   250, 113, 400);
    drawTitle("FILTER / AMP",  866, 113, 252);
    drawTitle("MODULATION",   1177, 113, 190);
    drawTitle("LFO",          1522, 113, 82);
    drawTitle("TRANSPORT",      95, 457, 138);

    g.setColour(kInk);
    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    g.drawText("ANALOG  -  16 STEP  -  3 MOD  -  1 LFO",
               ref(1458,39,198,12), juce::Justification::right, false);
    g.setFont(juce::FontOptions(8.0f));
    g.drawText("SERIAL NO. XT-0001", ref(1548,61,108,10), juce::Justification::right, false);
    g.drawText("MADE IN SWEDEN",     ref(1550,80,106,10), juce::Justification::right, false);

    // OSC section labels
    drawLabel("VCO 1", 50, 148, 48, juce::Justification::left);
    line(119,158,347,158, 0.8f);
    drawLabel("VCO 2", 50, 287, 48, juce::Justification::left);
    line(119,297,347,297, 0.8f);
    drawLabel("HARD SYNC", 431, 160, 70);
    drawLabel("EG SHAPE",  419, 290, 70);

    drawLabel("FREQ",    62, 248, 52);
    drawLabel("EG AMT",  135, 248, 62);
    drawLabel("WAVE",    208, 234, 58);
    drawLabel("LEVEL",   278, 248, 52);
    drawLabel("DECAY",   351, 248, 52);

    drawLabel("FREQ",    62, 388, 52);
    drawLabel("EG AMT",  135, 388, 62);
    drawLabel("WAVE",    208, 374, 58);
    drawLabel("LEVEL",   278, 388, 52);
    drawLabel("DECAY",   351, 388, 46);   // was "LVL" — now vco2Decay

    // OSC / MIXER merged section
    drawLabel("CLICK TUN", 507, 218, 60);
    drawLabel("CLICK DEC", 580, 218, 60);
    drawLabel("NOISE",     653, 218, 50);
    drawLabel("COLOR",     726, 218, 50);   // noiseColor
    drawLabel("N.DECAY",   799, 218, 50);
    drawLabel("FM AMT",    507, 326, 68);
    drawLabel("PTH→FM",    580, 326, 62);   // pitchFmAmt
    drawLabel("VEL→VCF",   653, 326, 62);   // velVcfDecaySens
    drawLabel("CLK LVL",   726, 326, 64);
    drawLabel("N.BYPASS",  799, 299, 60);
    drawLabel("C.BYPASS",  799, 343, 60);
    drawLabel("SIGNAL FLOW", 608, 380, 90);
    g.setColour(kDivider.withAlpha(0.6f));
    g.drawRoundedRectangle(ref(526,388,249,32).toFloat(), 3.0f, 0.8f);
    drawMuted("VCO1 + VCO2 + NOISE + CLICK → FILTER → DRIVE → VCA → OUT", 531, 399, 238);

    // Filter / amp labels
    drawLabel("MODE",       816, 134, 68);
    drawLabel("CUTOFF",     806, 250, 90);
    drawLabel("RESONANCE",  878, 250, 92);
    drawLabel("VCF EG AMT", 946, 250, 96);
    drawLabel("VCF DEC",   1020, 250, 84);
    drawLabel("PRE DRV",   1094, 250, 76);
    drawLabel("NOISE MOD",  806, 344, 84);
    drawLabel("VCA ATCK",   878, 344, 84);
    drawLabel("VCA DEC",    946, 344, 76);
    drawLabel("VOLUME",    1020, 344, 84);
    drawLabel("POST DRV",  1094, 344, 76);


    // LFO labels
    drawLabel("RATE",   1480, 241, 54);
    drawLabel("AMT",    1557, 241, 54);
    drawLabel("WAVE",   1480, 288, 70);
    drawLabel("DEST",   1480, 334, 70);
    drawLabel("SYNC",   1558, 296, 46);
    drawLabel("RETRIG", 1558, 348, 46);

    // Transport labels
    drawLabel("TEMPO",     78,  488, 56);
    drawLabel("SWING",    182,  488, 56);
    drawMuted("BPM",      101,  596, 32);
    drawLabel("CLK MULT",  43,  604, 68);
    drawLabel("SEQ PITCH", 124, 604, 70);
    drawMuted("RUN / STOP", 36, 705, 66, juce::Justification::left);
    drawMuted("TRIGGER",    89, 705, 50, juce::Justification::left);
    drawMuted("ADVANCE",   137, 705, 54, juce::Justification::left);
    drawMuted("RESET",     191, 705, 42, juce::Justification::left);
    drawLabel("STEP COUNT", 93, 757, 78);
    drawMuted("1 - 16",    104, 775, 56);

    // Sequencer labels
    drawLabel("SEQUENCER", 495, 454, 98, juce::Justification::left);
    drawMuted("8 STEPS × 2 PAGES  -  PITCH  -  VELOCITY  -  MOD A  -  MOD B",
              602, 456, 360, juce::Justification::left);
    drawMuted("PLAYHEAD", 407, 492, 64, juce::Justification::left);
    line(404,509,1650,509, 0.8f);

    drawMutedLabel(g, { presetBox.getX() - 54, presetBox.getY() + 5, 46, 10 },
                   "PRESET", juce::Justification::centredRight);

    // Step counter display
    const auto stepDisplayBounds = ref(74.0f, 754.0f, 196.0f, 78.0f);
    const int pageRelStep = currentLedStep - editPage * 8;
    const auto stepDisplay = juce::String::formatted("%d/8",
        juce::jlimit(1, 8, pageRelStep >= 0 ? pageRelStep + 1 : 1));

    g.setColour(kDarkPlate);
    g.fillRoundedRectangle(stepDisplayBounds.toFloat(), 4.0f);
    g.setColour(juce::Colour(0xffefe7d8));
    g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    g.drawText("STEP", stepDisplayBounds.getX() + 16, stepDisplayBounds.getY() + 10, 48, 12,
               juce::Justification::left);
    g.setColour(juce::Colour(0xffdf5b31));
    g.setFont(juce::FontOptions(32.0f).withStyle("Bold"));
    g.drawText(stepDisplay,
               stepDisplayBounds.getX() + 18, stepDisplayBounds.getY() + 28,
               stepDisplayBounds.getWidth() - 36, 32, juce::Justification::centred);

    // Step LEDs and step numbers
    for (int i = 0; i < XTSequencer::numSteps; ++i)
    {
        const auto knobBounds = stepPitch[i].getBounds().toFloat();
        const float centreX  = knobBounds.getCentreX();
        const float padY     = knobBounds.getY() - 39.0f;
        const float ledY     = knobBounds.getY() - 9.0f;
        const bool  active   = (i == currentLedStep);

        g.setColour(kInk);
        g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
        g.drawText(juce::String((i % 8) + 1),
                   juce::Rectangle<float>(centreX - 12.0f, padY - 18.0f, 24.0f, 10.0f),
                   juce::Justification::centred, false);

        g.setColour(kPanelBase);
        g.fillEllipse(centreX - 7.0f, ledY - 7.0f, 14.0f, 14.0f);
        if (active) {
            g.setColour(juce::Colour(0x22ff3a20));
            g.fillEllipse(centreX - 10.0f, ledY - 10.0f, 20.0f, 20.0f);
        }
        juce::ColourGradient ledGrad(
            active ? juce::Colour(0xffff5a34) : juce::Colour(0xff5b3326), centreX-3.0f, ledY-3.0f,
            active ? juce::Colour(0xff9b2014) : juce::Colour(0xff231512), centreX+3.0f, ledY+3.0f, true);
        g.setGradientFill(ledGrad);
        g.fillEllipse(centreX - 4.5f, ledY - 4.5f, 9.0f, 9.0f);
        g.setColour(active ? juce::Colour(0xff7f1d15) : juce::Colour(0xff181818));
        g.drawEllipse(centreX - 4.5f, ledY - 4.5f, 9.0f, 9.0f, 1.0f);
    }

    // Sequencer lane labels
    for (int row = 0; row < XTSequencer::numLaneRows; ++row)
    {
        const bool isModLane = row >= 2;

        const auto first = stepPitch[0].getBounds().translated(0, row * 39);
        auto plate = juce::Rectangle<float>((float)first.getX() - 120.0f, (float)first.getY() - 3.0f,
                                            82.0f, 40.0f);
        g.setColour(kDarkPlate);
        g.fillRoundedRectangle(plate, 3.0f);
        g.setColour(kDarkEdge);
        g.drawRoundedRectangle(plate, 3.0f, 1.0f);
        g.setColour(juce::Colour(0xffefe7d8));
        g.setFont(juce::FontOptions(10.5f).withStyle("Bold"));
        g.drawText(kSequencerLaneNames[(size_t)row],
                   juce::Rectangle<int>((int)plate.getX()+8, (int)plate.getY()+5, (int)plate.getWidth()-16, 12),
                   juce::Justification::left, false);

        // Subtitle only for non-mod lanes; mod lanes show a dest combo widget instead
        if (!isModLane)
        {
            const juce::String subtitle = row == 0 ? "semitones" : "accent";
            g.setFont(juce::FontOptions(8.0f));
            g.drawText(subtitle,
                       juce::Rectangle<int>((int)plate.getX()+8, (int)plate.getY()+20, (int)plate.getWidth()-16, 10),
                       juce::Justification::left, false);
        }
    }
}

// =============================================================================
// resized()
// =============================================================================

void XTEditor::resized()
{
    const auto layout = createLayout(getWidth(), getHeight());
    const float uiScale = juce::jmin((float)getWidth() / 1720.0f, (float)getHeight() / 900.0f);
    auto ref = [&](float x, float y, float w, float h) {
        return mapReferenceRect(layout.panel, x, y, w, h);
    };

    // Preset controls
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

    // --- OSC ---
    vco1Frequency.setBounds(ref(62.0f,  185.0f, 50.0f, 50.0f));
    vco1EgAmount.setBounds( ref(135.0f, 185.0f, 50.0f, 50.0f));
    vco1WaveBox.setBounds(  ref(205.0f, 199.0f, 58.0f, 24.0f));
    vco1Level.setBounds(    ref(278.0f, 192.0f, 50.0f, 50.0f));
    vcoDecay.setBounds(     ref(351.0f, 185.0f, 50.0f, 50.0f));

    hardSyncBox.setBounds(  ref(419.0f, 178.0f, 62.0f, 56.0f));
    vcoEgShapeBox.setBounds(ref(419.0f, 264.0f, 62.0f, 24.0f));

    vco2Frequency.setBounds(ref(62.0f,  325.0f, 50.0f, 50.0f));
    vco2EgAmount.setBounds( ref(135.0f, 325.0f, 50.0f, 50.0f));
    vco2WaveBox.setBounds(  ref(205.0f, 339.0f, 58.0f, 24.0f));
    vco2Level.setBounds(    ref(278.0f, 325.0f, 50.0f, 50.0f));
    vco2Decay.setBounds(    ref(351.0f, 325.0f, 50.0f, 50.0f));   // was placeholder

    // --- OSC / MIXER (click + noise) ---
    clickTune.setBounds(    ref(507.0f, 166.0f, 50.0f, 50.0f));
    clickDecay.setBounds(   ref(580.0f, 166.0f, 50.0f, 50.0f));
    noiseLevel.setBounds(   ref(653.0f, 166.0f, 50.0f, 50.0f));
    noiseColor.setBounds(   ref(726.0f, 166.0f, 50.0f, 50.0f));
    noiseDecay.setBounds(   ref(799.0f, 166.0f, 50.0f, 50.0f));
    noiseVcfBypassButton.setBounds(ref(790.0f, 272.0f, 54.0f, 24.0f));
    clickVcfBypassButton.setBounds(ref(790.0f, 316.0f, 54.0f, 24.0f));
    fmAmount.setBounds(     ref(507.0f, 272.0f, 50.0f, 50.0f));
    pitchFmAmt.setBounds(   ref(580.0f, 272.0f, 50.0f, 50.0f));   // new
    velVcfDecaySens.setBounds(ref(653.0f, 272.0f, 50.0f, 50.0f)); // new
    clickLevel.setBounds(   ref(726.0f, 272.0f, 50.0f, 50.0f));

    // --- FILTER / AMP ---
    vcfModeBox.setBounds( ref(820.0f, 143.0f, 64.0f, 26.0f));
    cutoff.setBounds(     ref(820.0f, 178.0f, 62.0f, 62.0f));
    resonance.setBounds(  ref(892.0f, 178.0f, 62.0f, 62.0f));
    vcfEgAmount.setBounds(ref(964.0f, 178.0f, 62.0f, 62.0f));
    vcfDecay.setBounds(   ref(1036.0f,178.0f, 62.0f, 62.0f));
    preDrive.setBounds(   ref(1108.0f,178.0f, 62.0f, 62.0f));
    noiseVcfMod.setBounds(ref(820.0f, 272.0f, 62.0f, 62.0f));
    vcaAttack.setBounds(  ref(892.0f, 272.0f, 62.0f, 62.0f));
    vcaDecay.setBounds(   ref(964.0f, 272.0f, 62.0f, 62.0f));
    volume.setBounds(     ref(1036.0f,272.0f, 62.0f, 62.0f));
    postDrive.setBounds(  ref(1108.0f,272.0f, 62.0f, 62.0f));
    vcaEg.setVisible(false);


    // --- LFO ---
    lfoRate.setBounds(    ref(1480.0f, 178.0f, 54.0f, 54.0f));
    lfoAmt.setBounds(     ref(1557.0f, 178.0f, 54.0f, 54.0f));
    lfoWaveBox.setBounds( ref(1480.0f, 253.0f, 70.0f, 26.0f));
    lfoDstBox.setBounds(  ref(1480.0f, 298.0f, 70.0f, 26.0f));
    lfoSyncBox.setBounds( ref(1558.0f, 248.0f, 46.0f, 40.0f));
    lfoRetrigBox.setBounds(ref(1558.0f,300.0f, 46.0f, 40.0f));

    // --- TRANSPORT ---
    tempoSlider.setBounds(  ref(60.0f,  510.0f, 58.0f, 58.0f));   // was placeholder
    swingSlider.setBounds(  ref(168.0f, 515.0f, 52.0f, 52.0f));   // was placeholder
    clockMultBox.setBounds( ref(43.0f,  618.0f, 68.0f, 24.0f));
    seqPitchModBox.setBounds(ref(124.0f,618.0f, 70.0f, 24.0f));

    // Transport LED buttons — RUN/STOP, TRIGGER, ADVANCE (replace drawPlaceholderBox)
    runStopButton.setBounds( ref(40.0f,  651.0f, 34.0f, 24.0f));
    triggerButton.setBounds( ref(90.0f,  651.0f, 34.0f, 24.0f));
    advanceButton.setBounds( ref(140.0f, 651.0f, 34.0f, 24.0f));
    resetButton.setBounds(   ref(191.0f, 651.0f, 34.0f, 24.0f));

    pageAButton.setBounds(   ref(40.0f,  690.0f, 44.0f, 24.0f));
    pageBButton.setBounds(   ref(100.0f, 690.0f, 44.0f, 24.0f));
    copyPageButton.setBounds(ref(160.0f, 690.0f, 54.0f, 24.0f));

    stepCountSlider.setBounds(ref(164.0f, 725.0f, 38.0f, 38.0f));  // was placeholder

    // --- SEQUENCER --- 8 steps per page, page A (0-7) and B (8-15) share same screen positions
    const float stepLeft     = 525.0f;
    const float stepStride   = 140.0f;   // was 74.7 — doubled for 8-step layout
    const float stepKnobTop  = 620.0f;
    const float stepKnobSize = 56.0f;    // was 40 — larger for better feel
    const float stepRowStride= 60.0f;    // was 52

    for (int i = 0; i < XTSequencer::numSteps; ++i)
    {
        const float x = stepLeft + (float)(i % 8) * stepStride;  // both pages share columns 0-7
        stepPitch[i].setBounds(   ref(x, stepKnobTop,                    stepKnobSize, stepKnobSize));
        stepVelocity[i].setBounds(ref(x, stepKnobTop + stepRowStride,     stepKnobSize, stepKnobSize));
        stepModA[i].setBounds(    ref(x, stepKnobTop + stepRowStride*2.f, stepKnobSize, stepKnobSize));
        stepModB[i].setBounds(    ref(x, stepKnobTop + stepRowStride*3.f, stepKnobSize, stepKnobSize));

        auto pitchBounds = ref(x, stepKnobTop, stepKnobSize, stepKnobSize);
        const int cx     = pitchBounds.getCentreX();
        const int padTop = pitchBounds.getY() - 39;
        stepActiveButton[i].setBounds(cx - 12, padTop, 24, 24);
    }

    // Mod dest combos — inline in the sequencer lane header plates for MOD A/B rows
    {
        auto modARef = stepModA[0].getBounds();
        auto modBRef = stepModB[0].getBounds();
        const int plateX = modARef.getX() - 108;
        const int plateW = 80;
        modDestBox[0].setBounds(plateX, modARef.getY() + 18, plateW, 18);
        modDestBox[1].setBounds(plateX, modBRef.getY() + 18, plateW, 18);
    }

    switchEditPage(editPage);  // apply initial visibility
}
