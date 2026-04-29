#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

enum class XTKnobStyle
{
    small,
    main,
    sequencer,
    transport
};

enum class XTComboStyle
{
    led,
    toggle,
    preset
};

enum class XTButtonStyle
{
    led,
    utility,
    square
};

class XTSlider : public juce::Slider
{
public:
    explicit XTSlider(XTKnobStyle knobStyle = XTKnobStyle::main) : style(knobStyle) {}
    XTKnobStyle style;
};

class XTComboBox : public juce::ComboBox
{
public:
    explicit XTComboBox(XTComboStyle comboStyle = XTComboStyle::led) : style(comboStyle) {}
    XTComboStyle style;
};

class XTTextButton : public juce::TextButton
{
public:
    XTTextButton(const juce::String& text, XTButtonStyle buttonStyle)
        : juce::TextButton(text), style(buttonStyle) {}
    XTButtonStyle style;
};

class XTLookAndFeel : public juce::LookAndFeel_V4
{
public:
    XTLookAndFeel();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;
};

class XTEditor : public juce::AudioProcessorEditor,
                 private juce::Timer
{
public:
    explicit XTEditor(XTProcessor&);
    ~XTEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void setupKnob(XTSlider& slider);
    void setupChoice(XTComboBox& box);
    void refreshPresetControls();
    void promptSavePreset();
    void updatePresetButtonState();
    juce::String getModLaneSubtitle(int modLaneIndex) const;

    XTProcessor& xtProcessor;
    XTLookAndFeel laf;

    // Preset
    XTComboBox   presetBox { XTComboStyle::preset };
    XTTextButton presetSaveButton   { "SAVE",   XTButtonStyle::utility };
    XTTextButton presetDeleteButton { "DELETE", XTButtonStyle::utility };
    XTTextButton presetInitButton   { "INIT",   XTButtonStyle::utility };
    std::unique_ptr<juce::FileChooser> presetSaveChooser;
    juce::String lastPresetName;
    bool isUpdatingPresetBox = false;

    // OSC
    XTSlider vcoDecay      { XTKnobStyle::small };
    XTSlider vco1EgAmount  { XTKnobStyle::small };
    XTSlider vco1Frequency { XTKnobStyle::small };
    XTSlider fmAmount      { XTKnobStyle::small };
    XTSlider vco2EgAmount  { XTKnobStyle::small };
    XTSlider vco2Frequency { XTKnobStyle::small };
    XTSlider vco1Level     { XTKnobStyle::small };
    XTSlider vco2Level     { XTKnobStyle::small };
    XTSlider noiseLevel    { XTKnobStyle::small };
    XTSlider vco2Decay     { XTKnobStyle::small };   // independent VCO2 pitch-sweep decay

    XTComboBox vcoEgShapeBox { XTComboStyle::led };   // EXP / LIN / LOG

    // Click / mix
    XTSlider clickTune  { XTKnobStyle::small };
    XTSlider clickDecay { XTKnobStyle::small };
    XTSlider clickLevel { XTKnobStyle::small };
    XTSlider noiseColor { XTKnobStyle::small };   // noise LP coloring
    XTSlider noiseDecay { XTKnobStyle::small };
    XTTextButton noiseVcfBypassButton { "N.VCF", XTButtonStyle::led };
    XTTextButton clickVcfBypassButton { "C.VCF", XTButtonStyle::led };

    // Pitch→FM and vel→VCF coupling
    XTSlider pitchFmAmt      { XTKnobStyle::small };
    XTSlider velVcfDecaySens { XTKnobStyle::small };

    // Filter / amp
    XTSlider cutoff      { XTKnobStyle::main };
    XTSlider resonance   { XTKnobStyle::main };
    XTSlider vcfDecay    { XTKnobStyle::main };
    XTSlider vcfEgAmount { XTKnobStyle::main };
    XTSlider noiseVcfMod { XTKnobStyle::main };
    XTSlider vcaDecay    { XTKnobStyle::main };
    XTSlider vcaEg       { XTKnobStyle::main };
    XTSlider volume      { XTKnobStyle::main };
    XTSlider vcaAttack   { XTKnobStyle::main };
    XTSlider preDrive    { XTKnobStyle::main };
    XTSlider postDrive   { XTKnobStyle::main };

    // Modulation
    XTComboBox modModeBox[3]{ XTComboBox{XTComboStyle::led}, XTComboBox{XTComboStyle::led}, XTComboBox{XTComboStyle::led} };
    XTComboBox modDestBox[3]{ XTComboBox{XTComboStyle::led}, XTComboBox{XTComboStyle::led}, XTComboBox{XTComboStyle::led} };

    // LFO
    XTSlider   lfoRate    { XTKnobStyle::main };
    XTSlider   lfoAmt     { XTKnobStyle::main };
    XTComboBox lfoWaveBox   { XTComboStyle::led };
    XTComboBox lfoSyncBox   { XTComboStyle::toggle };
    XTComboBox lfoRetrigBox { XTComboStyle::toggle };
    XTComboBox lfoDstBox    { XTComboStyle::led };

    // Transport
    XTSlider tempoSlider     { XTKnobStyle::main };
    XTSlider swingSlider     { XTKnobStyle::small };
    XTSlider stepCountSlider { XTKnobStyle::small };
    XTTextButton runStopButton  { "RUN",     XTButtonStyle::led };
    XTTextButton triggerButton  { "TRIG",    XTButtonStyle::led };
    XTTextButton advanceButton  { "ADV",     XTButtonStyle::led };
    XTTextButton resetButton    { "RST",     XTButtonStyle::square };

    // Pattern pages
    XTTextButton pageAButton    { "PAT A",   XTButtonStyle::led };
    XTTextButton pageBButton    { "PAT B",   XTButtonStyle::led };
    XTTextButton copyPageButton { "A→B", XTButtonStyle::utility };
    int editPage = 0;
    void switchEditPage(int page);

    // Sequencer combos
    XTComboBox seqPitchModBox { XTComboStyle::led };
    XTComboBox hardSyncBox    { XTComboStyle::toggle };
    XTComboBox vco1WaveBox    { XTComboStyle::led };
    XTComboBox vco2WaveBox    { XTComboStyle::led };
    XTComboBox vcfModeBox     { XTComboStyle::led };
    XTComboBox clockMultBox   { XTComboStyle::led };

    // Step active buttons
    XTTextButton stepActiveButton[XTSequencer::numSteps] {
        XTTextButton{"", XTButtonStyle::led}, XTTextButton{"", XTButtonStyle::led},
        XTTextButton{"", XTButtonStyle::led}, XTTextButton{"", XTButtonStyle::led},
        XTTextButton{"", XTButtonStyle::led}, XTTextButton{"", XTButtonStyle::led},
        XTTextButton{"", XTButtonStyle::led}, XTTextButton{"", XTButtonStyle::led},
        XTTextButton{"", XTButtonStyle::led}, XTTextButton{"", XTButtonStyle::led},
        XTTextButton{"", XTButtonStyle::led}, XTTextButton{"", XTButtonStyle::led},
        XTTextButton{"", XTButtonStyle::led}, XTTextButton{"", XTButtonStyle::led},
        XTTextButton{"", XTButtonStyle::led}, XTTextButton{"", XTButtonStyle::led}
    };

    // Sequencer step lanes
    XTSlider stepPitch[XTSequencer::numSteps];
    XTSlider stepVelocity[XTSequencer::numSteps];
    XTSlider stepModA[XTSequencer::numSteps];
    XTSlider stepModB[XTSequencer::numSteps];
    XTSlider stepModC[XTSequencer::numSteps];

    int  currentLedStep  = -1;
    bool resetLedActive  = false;

    // --- APVTS attachments ---
    std::unique_ptr<SliderAttachment> vcoDecayAtt, vco1FreqAtt, vco1EgAmtAtt;
    std::unique_ptr<SliderAttachment> fmAmountAtt, vco2FreqAtt, vco2EgAmtAtt;
    std::unique_ptr<SliderAttachment> vco1LevelAtt, vco2LevelAtt, noiseLevelAtt;
    std::unique_ptr<SliderAttachment> vco2DecayAtt;
    std::unique_ptr<SliderAttachment> cutoffAtt, resonanceAtt, vcfDecayAtt, vcfEgAmtAtt;
    std::unique_ptr<SliderAttachment> noiseVcfModAtt, vcaDecayAtt, vcaEgAtt, volumeAtt;
    std::unique_ptr<SliderAttachment> clickTuneAtt, clickDecayAtt, clickLevelAtt;
    std::unique_ptr<SliderAttachment> vcaAttackAtt, preDriveAtt, postDriveAtt;
    std::unique_ptr<SliderAttachment> noiseColorAtt, pitchFmAmtAtt, velVcfDecaySensAtt;
    std::unique_ptr<SliderAttachment> noiseDecayAtt;
    std::unique_ptr<ButtonAttachment> noiseVcfBypassAtt, clickVcfBypassAtt;
    std::unique_ptr<SliderAttachment> lfoRateAtt, lfoAmtAtt;
    std::unique_ptr<SliderAttachment> tempoAtt, swingAtt, stepCountAtt;
    std::unique_ptr<SliderAttachment> stepPitchAtt[XTSequencer::numSteps];
    std::unique_ptr<SliderAttachment> stepVelAtt[XTSequencer::numSteps];
    std::unique_ptr<SliderAttachment> stepModAAtt[XTSequencer::numSteps];
    std::unique_ptr<SliderAttachment> stepModBAtt[XTSequencer::numSteps];
    std::unique_ptr<SliderAttachment> stepModCAtt[XTSequencer::numSteps];

    std::unique_ptr<ComboAttachment> seqPitchModBoxAtt, hardSyncBoxAtt;
    std::unique_ptr<ComboAttachment> vco1WaveBoxAtt, vco2WaveBoxAtt, vcfModeBoxAtt, clockMultBoxAtt;
    std::unique_ptr<ComboAttachment> modDestBoxAtt[3], modModeBoxAtt[3];
    std::unique_ptr<ComboAttachment> lfoWaveBoxAtt, lfoSyncBoxAtt, lfoRetrigBoxAtt, lfoDstBoxAtt;
    std::unique_ptr<ComboAttachment> vcoEgShapeBoxAtt;
    std::unique_ptr<ButtonAttachment> stepActiveAtt[XTSequencer::numSteps];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XTEditor)
};
