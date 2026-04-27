#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

enum class XTKnobStyle
{
    small,      // upper oscillator / mix / mod knobs
    main,       // big center row (CUTOFF, RESONANCE, VCF EG AMT, VOLUME)
    sequencer,  // 16x5 sequencer grid
    transport   // TEMPO, SWING, STEP COUNT
};

enum class XTComboStyle
{
    led,        // DEST / WAVE / VCF MODE readouts: amber on black, LED-display style
    toggle,     // HARD SYNC: 2-position physical toggle
    preset      // preset combo in header
};

enum class XTButtonStyle
{
    led,        // square LED buttons (DRIVE, MODE, transport, LFO)
    utility,    // soft rounded utility buttons (SAVE, DELETE, INIT)
    square      // small square transport reset
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
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void setupKnob(XTSlider& slider);
    void setupChoice(XTComboBox& box);
    void refreshPresetControls();
    void promptSavePreset();
    void updatePresetButtonState();
    juce::String getModLaneSubtitle(int modLaneIndex) const;

    XTProcessor& xtProcessor;
    XTLookAndFeel laf;

    XTComboBox presetBox { XTComboStyle::preset };
    XTTextButton presetSaveButton { "SAVE", XTButtonStyle::utility };
    XTTextButton presetDeleteButton { "DELETE", XTButtonStyle::utility };
    XTTextButton presetInitButton { "INIT", XTButtonStyle::utility };
    std::unique_ptr<juce::FileChooser> presetSaveChooser;
    juce::String lastPresetName;
    bool isUpdatingPresetBox = false;

    XTSlider vcoDecay { XTKnobStyle::small }, vco1EgAmount { XTKnobStyle::small }, vco1Frequency { XTKnobStyle::small };
    XTSlider fmAmount { XTKnobStyle::small }, vco2EgAmount { XTKnobStyle::small }, vco2Frequency { XTKnobStyle::small };
    XTSlider vco1Level { XTKnobStyle::small }, vco2Level { XTKnobStyle::small }, noiseLevel { XTKnobStyle::small };
    XTSlider cutoff { XTKnobStyle::main }, resonance { XTKnobStyle::main }, vcfDecay { XTKnobStyle::main }, vcfEgAmount { XTKnobStyle::main }, noiseVcfMod { XTKnobStyle::main }, vcaDecay { XTKnobStyle::main }, vcaEg { XTKnobStyle::main }, volume { XTKnobStyle::main };
    XTSlider modAmount[3] { XTSlider { XTKnobStyle::main }, XTSlider { XTKnobStyle::main }, XTSlider { XTKnobStyle::main } };

    // Click controls
    XTSlider clickTune  { XTKnobStyle::small };
    XTSlider clickDecay { XTKnobStyle::small };
    XTSlider clickLevel { XTKnobStyle::small };

    // New filter/amp controls
    XTSlider vcaAttack  { XTKnobStyle::main };
    XTSlider preDrive   { XTKnobStyle::main };
    XTSlider postDrive  { XTKnobStyle::main };

    // Mod mode combos (one per lane)
    XTComboBox modModeBox[3] { XTComboBox { XTComboStyle::led },
                               XTComboBox { XTComboStyle::led },
                               XTComboBox { XTComboStyle::led } };

    // LFO
    XTSlider lfoRate { XTKnobStyle::main };
    XTComboBox lfoWaveBox   { XTComboStyle::led };
    XTComboBox lfoSyncBox   { XTComboStyle::toggle };
    XTComboBox lfoRetrigBox { XTComboStyle::toggle };
    XTComboBox lfoDstBox    { XTComboStyle::led };
    XTSlider lfoAmt { XTKnobStyle::main };

    // Step active buttons (toggle)
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

    XTComboBox seqPitchModBox { XTComboStyle::led };
    XTComboBox hardSyncBox { XTComboStyle::toggle };
    XTComboBox vco1WaveBox { XTComboStyle::led }, vco2WaveBox { XTComboStyle::led }, vcfModeBox { XTComboStyle::led };
    XTComboBox modDestBox[3] { XTComboBox { XTComboStyle::led }, XTComboBox { XTComboStyle::led }, XTComboBox { XTComboStyle::led } };
    XTComboBox clockMultBox { XTComboStyle::led };

    XTTextButton resetButton { "RST", XTButtonStyle::square };

    XTSlider stepPitch[XTSequencer::numSteps];
    XTSlider stepVelocity[XTSequencer::numSteps];
    XTSlider stepModA[XTSequencer::numSteps];
    XTSlider stepModB[XTSequencer::numSteps];
    XTSlider stepModC[XTSequencer::numSteps];

    int currentLedStep = -1;
    bool resetLedActive = false;

    std::unique_ptr<SliderAttachment> vcoDecayAtt;
    std::unique_ptr<SliderAttachment> vco1FreqAtt;
    std::unique_ptr<SliderAttachment> vco1EgAmtAtt;
    std::unique_ptr<SliderAttachment> fmAmountAtt;
    std::unique_ptr<SliderAttachment> vco2FreqAtt;
    std::unique_ptr<SliderAttachment> vco2EgAmtAtt;
    std::unique_ptr<SliderAttachment> vco1LevelAtt;
    std::unique_ptr<SliderAttachment> vco2LevelAtt;
    std::unique_ptr<SliderAttachment> noiseLevelAtt;
    std::unique_ptr<SliderAttachment> cutoffAtt;
    std::unique_ptr<SliderAttachment> resonanceAtt;
    std::unique_ptr<SliderAttachment> vcfDecayAtt;
    std::unique_ptr<SliderAttachment> vcfEgAmtAtt;
    std::unique_ptr<SliderAttachment> noiseVcfModAtt;
    std::unique_ptr<SliderAttachment> vcaDecayAtt;
    std::unique_ptr<SliderAttachment> vcaEgAtt;
    std::unique_ptr<SliderAttachment> volumeAtt;

    std::unique_ptr<ComboAttachment> seqPitchModBoxAtt;
    std::unique_ptr<ComboAttachment> hardSyncBoxAtt;
    std::unique_ptr<ComboAttachment> vco1WaveBoxAtt;
    std::unique_ptr<ComboAttachment> vco2WaveBoxAtt;
    std::unique_ptr<ComboAttachment> vcfModeBoxAtt;
    std::unique_ptr<ComboAttachment> clockMultBoxAtt;
    std::unique_ptr<ComboAttachment> modDestBoxAtt[3];

    std::unique_ptr<SliderAttachment> modAmountAtt[3];
    std::unique_ptr<SliderAttachment> stepPitchAtt[XTSequencer::numSteps];
    std::unique_ptr<SliderAttachment> stepVelAtt[XTSequencer::numSteps];
    std::unique_ptr<SliderAttachment> stepModAAtt[XTSequencer::numSteps];
    std::unique_ptr<SliderAttachment> stepModBAtt[XTSequencer::numSteps];
    std::unique_ptr<SliderAttachment> stepModCAtt[XTSequencer::numSteps];

    std::unique_ptr<SliderAttachment> clickTuneAtt, clickDecayAtt, clickLevelAtt;
    std::unique_ptr<SliderAttachment> vcaAttackAtt, preDriveAtt, postDriveAtt;
    std::unique_ptr<ComboAttachment>  modModeBoxAtt[3];
    std::unique_ptr<SliderAttachment> lfoRateAtt, lfoAmtAtt;
    std::unique_ptr<ComboAttachment>  lfoWaveBoxAtt, lfoSyncBoxAtt, lfoRetrigBoxAtt, lfoDstBoxAtt;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<ButtonAttachment> stepActiveAtt[XTSequencer::numSteps];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XTEditor)
};
