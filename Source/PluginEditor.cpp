#include "PluginEditor.h"

static const juce::Colour panelWhite = juce::Colour(0xfff0ede8);
static const juce::Colour labelBlack = juce::Colour(0xff111111);

namespace
{
constexpr int kEditorWidth = 1760;
constexpr int kEditorBodyHeight = 690;
constexpr int kPresetFooterHeight = 36;
constexpr int kPresetInitId = 1;
constexpr int kPresetFirstUserId = 2;
constexpr int kPresetMissingId = 999;
constexpr int kTopControlDividerY = 160;
constexpr int kSequencerDividerY = 308;
constexpr int kJackPanelWidth = 200;
constexpr int kJackInputOffsetX = 32;
constexpr int kJackOutputOffsetX = 168;
constexpr int kJackStartY = 34;
constexpr int kJackStride = 32;
constexpr int kTransportOffsetX = 24;
constexpr int kTransportWidth = 120;
constexpr int kRowLabelOffsetX = 118;
constexpr int kSequencerGridOffsetX = 220;
constexpr int kSequencerHeaderY = 316;
constexpr int kSequencerStepNumberY = 338;
constexpr int kSequencerLedY = 372;
constexpr int kSequencerFirstRowY = 388;
constexpr int kSequencerRowStride = 54;
constexpr int kSequencerKnobSize = 28;
constexpr int kModSectionY = 240;
constexpr int kModAmountY = 236;
constexpr int kModDestinationY = 274;
constexpr int kModDestinationWidth = 104;
constexpr int kModAmountSize = 34;

constexpr std::array<const char*, XTSequencer::numLaneRows> kSequencerLaneNames
{
    "PITCH", "VELOCITY", "MOD A", "MOD B", "MOD C"
};

constexpr std::array<const char*, XTSequencer::numLaneRows> kSequencerLaneSubtitles
{
    "semitones", "accent", "bipolar", "bipolar", "bipolar"
};

constexpr std::array<const char*, 3> kModRouteNames
{
    "MOD A", "MOD B", "MOD C"
};

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

constexpr int getSequencerRowY(int row)
{
    return kSequencerFirstRowY + row * kSequencerRowStride;
}

bool cableSnapshotsEqual(const std::vector<PatchCable>& a, const std::vector<PatchCable>& b)
{
    if (a.size() != b.size())
        return false;

    for (size_t i = 0; i < a.size(); ++i)
    {
        if (a[i].src     != b[i].src)     return false;
        if (a[i].dst     != b[i].dst)     return false;
        if (std::abs(a[i].amount - b[i].amount) > 1.0e-6f) return false;
        if (a[i].enabled != b[i].enabled) return false;
    }

    return true;
}
}

XTEditor::XTEditor(XTProcessor& p)
    : AudioProcessorEditor(&p), xtProcessor(p)
{
    setSize(kEditorWidth, kEditorBodyHeight + kPresetFooterHeight);
    setLookAndFeel(&laf);
    startTimerHz(30);

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

    presetSaveButton.onClick = [this]() { promptSavePreset(); };
    addAndMakeVisible(presetSaveButton);

    presetDeleteButton.onClick = [this]()
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
    };
    addAndMakeVisible(presetDeleteButton);

    presetInitButton.onClick = [this]()
    {
        xtProcessor.loadInitPreset();
        refreshPresetControls();
    };
    addAndMakeVisible(presetInitButton);

    resetButton.setButtonText("RESET");
    resetButton.onClick = [this]() {
            xtProcessor.resetSequencer();
            currentLedStep = 0;
            resetLedActive = true;
            repaint();
        };
    addAndMakeVisible(resetButton);

    auto add = [this](juce::Slider& s, bool small = false) {
        setupKnob(s, small);
        addAndMakeVisible(s);
    };

    add(vcoDecay); add(vco1EgAmount); add(vco1Frequency);
        seqPitchModBox.addItem("VCO 1&2", 1);
        seqPitchModBox.addItem("OFF",     2);
        seqPitchModBox.addItem("VCO 2",   3);
        seqPitchModBox.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(seqPitchModBox);

    hardSyncBox.addItem("OFF", 1);
        hardSyncBox.addItem("ON",  2);
        hardSyncBox.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(hardSyncBox);

    vco1WaveBox.addItem("Square",   1);
        vco1WaveBox.addItem("Triangle", 2);
        vco1WaveBox.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(vco1WaveBox);

    vco2WaveBox.addItem("Square",   1);
            vco2WaveBox.addItem("Triangle", 2);
            vco2WaveBox.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(vco2WaveBox);
        vcfModeBox.addItem("LP", 1);
        vcfModeBox.addItem("HP", 2);
        vcfModeBox.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(vcfModeBox);

    for (int i = 0; i < (int) kModRouteNames.size(); ++i)
    {
        modDestBox[i].setJustificationType(juce::Justification::centred);
        const auto destinationNames = XTProcessor::getModDestinationNames();
        for (int item = 0; item < destinationNames.size(); ++item)
            modDestBox[i].addItem(destinationNames[item], item + 1);

        addAndMakeVisible(modDestBox[i]);
        add(modAmount[i], true);
    }
    
    add(vco1Level, true); add(noiseLevel, true); add(cutoff); add(resonance); add(vcaEg); add(volume);
    add(fmAmount); add(vco2EgAmount); add(vco2Frequency);
    add(vco2Level, true); add(vcfDecay); add(vcfEgAmount); add(noiseVcfMod); add(vcaDecay);    clockMultBox.addItem("1/8", 1);
        clockMultBox.addItem("1/5", 2);
        clockMultBox.addItem("1/4", 3);
        clockMultBox.addItem("1/3", 4);
        clockMultBox.addItem("1/2", 5);
        clockMultBox.addItem("1x",  6);
    clockMultBox.addItem("2x",  7);
        clockMultBox.addItem("3x",  8);
        clockMultBox.addItem("4x",  9);
        clockMultBox.addItem("5x",  10);
        clockMultBox.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(clockMultBox);
    for (int i = 0; i < XTSequencer::numSteps; ++i)
    {
        add(stepPitch[i], true);
        add(stepVelocity[i], true);
        add(stepModA[i], true);
        add(stepModB[i], true);
        add(stepModC[i], true);
    }

    auto& apvts = p.apvts;
    auto setDoubleClickToDefault = [&apvts](juce::Slider& slider, const juce::String& parameterId)
    {
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(parameterId)))
            slider.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
    };

    vcoDecayAtt       = std::make_unique<SliderAttachment>(apvts, "vcoDecay", vcoDecay);
    seqPitchModBoxAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts, "seqPitchMod", seqPitchModBox);
    hardSyncBoxAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts, "hardSync", hardSyncBox);
        vco1WaveBoxAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts, "vco1Wave", vco1WaveBox);
    vco2WaveBoxAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "vco2Wave", vco2WaveBox);
    vcfModeBoxAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "vcfMode", vcfModeBox);
    vco1FreqAtt    = std::make_unique<SliderAttachment>(apvts, "vco1Freq",    vco1Frequency);
    vco1EgAmtAtt   = std::make_unique<SliderAttachment>(apvts, "vco1EgAmt",   vco1EgAmount);
    fmAmountAtt    = std::make_unique<SliderAttachment>(apvts, "fmAmount",    fmAmount);
    vco2FreqAtt    = std::make_unique<SliderAttachment>(apvts, "vco2Freq",    vco2Frequency);
    vco2EgAmtAtt   = std::make_unique<SliderAttachment>(apvts, "vco2EgAmt",   vco2EgAmount);
    noiseLevelAtt  = std::make_unique<SliderAttachment>(apvts, "noiseLevel",  noiseLevel);
        vco1LevelAtt   = std::make_unique<SliderAttachment>(apvts, "vco1Level",   vco1Level);
        vco2LevelAtt   = std::make_unique<SliderAttachment>(apvts, "vco2Level",   vco2Level);
    cutoffAtt      = std::make_unique<SliderAttachment>(apvts, "cutoff",      cutoff);
    resonanceAtt   = std::make_unique<SliderAttachment>(apvts, "resonance",   resonance);
    vcfDecayAtt    = std::make_unique<SliderAttachment>(apvts, "vcfDecay",    vcfDecay);
    vcfEgAmtAtt    = std::make_unique<SliderAttachment>(apvts, "vcfEgAmt",    vcfEgAmount);
    noiseVcfModAtt = std::make_unique<SliderAttachment>(apvts, "noiseVcfMod", noiseVcfMod);
    vcaDecayAtt    = std::make_unique<SliderAttachment>(apvts, "vcaDecay",    vcaDecay);
    vcaEgAtt       = std::make_unique<SliderAttachment>(apvts, "vcaEg",       vcaEg);
    volumeAtt      = std::make_unique<SliderAttachment>(apvts, "volume",      volume);
    clockMultBoxAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                apvts, "clockMult", clockMultBox);

    for (int i = 0; i < (int) kModRouteNames.size(); ++i)
    {
        const auto modDestId = makeModDestinationParameterId(i);
        const auto modAmtId  = makeModAmountParameterId(i);
        modDestBoxAtt[i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts, modDestId, modDestBox[i]);
        modAmountAtt[i] = std::make_unique<SliderAttachment>(apvts, modAmtId, modAmount[i]);
        setDoubleClickToDefault(modAmount[i], modAmtId);
    }

    for (int i = 0; i < XTSequencer::numSteps; ++i)
    {
        const auto stepPitchId = makeStepParameterId("stepPitch", i);
        const auto stepVelId   = makeStepParameterId("stepVel",   i);
        const auto stepModAId  = makeStepParameterId("stepModA",  i);
        const auto stepModBId  = makeStepParameterId("stepModB",  i);
        const auto stepModCId  = makeStepParameterId("stepModC",  i);

        stepPitchAtt[i] = std::make_unique<SliderAttachment>(apvts, stepPitchId, stepPitch[i]);
        stepVelAtt[i]   = std::make_unique<SliderAttachment>(apvts, stepVelId,   stepVelocity[i]);
        stepModAAtt[i]  = std::make_unique<SliderAttachment>(apvts, stepModAId,  stepModA[i]);
        stepModBAtt[i]  = std::make_unique<SliderAttachment>(apvts, stepModBId,  stepModB[i]);
        stepModCAtt[i]  = std::make_unique<SliderAttachment>(apvts, stepModCId,  stepModC[i]);

        setDoubleClickToDefault(stepPitch[i], stepPitchId);
        setDoubleClickToDefault(stepVelocity[i], stepVelId);
        setDoubleClickToDefault(stepModA[i], stepModAId);
        setDoubleClickToDefault(stepModB[i], stepModBId);
        setDoubleClickToDefault(stepModC[i], stepModCId);
    }

    vco1EgAmount.setDoubleClickReturnValue(true, 0.0);
    vco2EgAmount.setDoubleClickReturnValue(true, 0.0);
    vcfEgAmount.setDoubleClickReturnValue(true, 0.0);

    xtProcessor.getCableSnapshot(lastCableSnapshot);
    refreshPresetControls();
}

XTEditor::~XTEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void XTEditor::refreshPresetControls()
{
    const auto presetNames = xtProcessor.getAvailablePresetNames();
    const auto currentPreset = xtProcessor.getCurrentPresetName();

    isUpdatingPresetBox = true;
    presetBox.clear(juce::dontSendNotification);
    presetBox.addItem("Init", kPresetInitId);

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

// =============================================================================
// Patch GUI helpers
// =============================================================================

juce::Point<int> XTEditor::getJackCentre(PatchPoint pp) const
{
    // Must match drawJackPanel() constants exactly
    const int W     = getWidth();
    const int wood  = 18;
    const int jackW = kJackPanelWidth;
    const int px    = W - wood - jackW;   // jack panel left edge
    const int py    = 0;                  // jack panel top edge

    const int col1   = px + kJackInputOffsetX;
    const int col3   = px + kJackOutputOffsetX;
    const int startY = py + kJackStartY;
    const int stride = kJackStride;

    // col1=IN (r0-6), col3=OUT (r0-9)
    switch (pp)
    {
        case PP_VCA_CV:    return { col1, startY + 0 * stride };
        case PP_VCA_DECAY: return { col1, startY + 1 * stride };
        case PP_VCF_MOD:   return { col1, startY + 2 * stride };
        case PP_VCF_DECAY: return { col1, startY + 3 * stride };
        case PP_NOISE_LVL: return { col1, startY + 4 * stride };
        case PP_VCO_DECAY: return { col1, startY + 5 * stride };
        case PP_FM_AMT:    return { col1, startY + 6 * stride };
        case PP_VCA_EG:    return { col3, startY + 0 * stride };
        case PP_VCF_EG:    return { col3, startY + 1 * stride };
        case PP_VCO_EG:    return { col3, startY + 2 * stride };
        case PP_VCO1:      return { col3, startY + 3 * stride };
        case PP_VCO2:      return { col3, startY + 4 * stride };
        case PP_VELOCITY:  return { col3, startY + 5 * stride };
        case PP_PITCH:     return { col3, startY + 6 * stride };
        case PP_MOD_A:     return { col3, startY + 7 * stride };
        case PP_MOD_B:     return { col3, startY + 8 * stride };
        case PP_MOD_C:     return { col3, startY + 9 * stride };
        case PP_NUM_POINTS:
        default:           return { -1, -1 };
    }
}

PatchPoint XTEditor::jackHitTest(juce::Point<int> pos) const
{
    const int hitR2 = 14 * 14;   // hit radius² (slightly larger than jack radius 10)
    for (int p = 0; p < PP_NUM_POINTS; ++p)
    {
        auto pp = static_cast<PatchPoint>(p);
        auto  c = getJackCentre(pp);
        if (c.x < 0) continue;
        const int dx = pos.x - c.x, dy = pos.y - c.y;
        if (dx * dx + dy * dy <= hitR2)
            return pp;
    }
    return PP_NUM_POINTS;
}

void XTEditor::mouseDown(const juce::MouseEvent& e)
{
    PatchPoint hit = jackHitTest(e.getPosition());

    if (hit == PP_NUM_POINTS)                          // empty area → deselect
    {
        pendingOut = PP_NUM_POINTS;
        repaint();
        return;
    }

    if (kPatchMeta[hit].dir == PD_Out)                 // clicked an OUT jack
    {
        pendingOut = (pendingOut == hit) ? PP_NUM_POINTS : hit;
        repaint();
        return;
    }

    // Clicked an IN jack — need a pending OUT
    if (pendingOut == PP_NUM_POINTS) return;

    // Toggle: connect if not connected, disconnect if already connected
    std::vector<PatchCable> cables;
    xtProcessor.getCableSnapshot(cables);
    bool alreadyConnected = false;
    for (const auto& c : cables)
        if (c.src == pendingOut && c.dst == hit) { alreadyConnected = true; break; }

    if (alreadyConnected)
        xtProcessor.disconnectPatch(pendingOut, hit);
    else
        xtProcessor.connectPatch(pendingOut, hit, 1.0f);

    pendingOut = PP_NUM_POINTS;
    repaint();
}

// =============================================================================

void XTEditor::timerCallback()
{
    bool needsRepaint = false;
    int step = xtProcessor.getCurrentStep();
    const auto currentPreset = xtProcessor.getCurrentPresetName();

    if (currentPreset != lastPresetName)
        refreshPresetControls();

    if (step >= 0)
        resetLedActive = false;

    int displayStep = resetLedActive ? 0 : step;
    if (displayStep != currentLedStep)
    {
        currentLedStep = displayStep;
        needsRepaint = true;
    }

    std::vector<PatchCable> cables;
    xtProcessor.getCableSnapshot(cables);
    if (!cableSnapshotsEqual(cables, lastCableSnapshot))
    {
        lastCableSnapshot = std::move(cables);
        pendingOut = PP_NUM_POINTS;
        needsRepaint = true;
    }

    if (needsRepaint)
        repaint();
}

void XTEditor::setupKnob(juce::Slider& s, bool small)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s.setRange(0.0, 1.0);
    s.setValue(0.5);
    juce::ignoreUnused(small);
}

void XTEditor::drawSwitch(juce::Graphics& g, float x, float y, float w, float h,
                             const juce::String& label, const juce::StringArray& options) const
{
    g.setColour(labelBlack);
    g.setFont(juce::FontOptions(7.0f).withStyle("Bold"));
    if (label.isNotEmpty())
        g.drawText(label, (int)x, (int)y, (int)w, 10, juce::Justification::centred);

    float bx = x + w * 0.5f - 10.0f;
    float by = y + 14.0f;
    float bw = 20.0f;
    float bh = h - 26.0f;

    g.setColour(juce::Colour(0xff888888));
    g.fillRoundedRectangle(bx, by, bw, bh, 4.0f);
    g.setColour(juce::Colour(0xff444444));
    g.drawRoundedRectangle(bx, by, bw, bh, 4.0f, 1.0f);

    float knobY = by + bh * 0.5f - 6.0f;
    g.setColour(juce::Colour(0xffdddddd));
    g.fillRoundedRectangle(bx + 2.0f, knobY, bw - 4.0f, 12.0f, 3.0f);

    g.setColour(labelBlack);
    g.setFont(juce::FontOptions(6.5f));
    if (options.size() >= 1)
        g.drawText(options[0], (int)(bx + bw + 3), (int)(by), 32, 9, juce::Justification::centredLeft);
    if (options.size() >= 2)
        g.drawText(options[options.size()-1], (int)(bx + bw + 3), (int)(by + bh - 9), 32, 9, juce::Justification::centredLeft);
}

void XTEditor::drawButton(juce::Graphics& g, float x, float y, float r,
                             const juce::String& label, bool red) const
{
    g.setColour(red ? juce::Colour(0xffcc2200) : juce::Colour(0xff222222));
    g.fillEllipse(x - r, y - r, r * 2, r * 2);
    g.setColour(juce::Colour(0xff555555));
    g.drawEllipse(x - r, y - r, r * 2, r * 2, 1.5f);
    g.setColour(labelBlack);
    g.setFont(juce::FontOptions(7.0f).withStyle("Bold"));
    g.drawText(label, (int)(x - 28), (int)(y + r + 3), 56, 9, juce::Justification::centred);
}

void XTEditor::drawJackPanel(juce::Graphics& g, int x, int y, int w, int h,
                               const std::vector<PatchCable>& cables,
                               PatchPoint selectedOut) const
{
    g.setColour(juce::Colour(0xffe8e5e0));
    g.fillRect(x, y, w, h);

    g.setColour(juce::Colour(0xffbbbbbb));
    g.drawRect(x, y, w, h, 1);

    auto drawJack = [&](int jx, int jy, const juce::String& lbl)
    {
        if (lbl.isEmpty()) return;
        g.setColour(juce::Colour(0xff333333));
        g.fillEllipse((float)(jx - 10), (float)(jy - 10), 20.0f, 20.0f);

        g.setColour(juce::Colour(0xff666666));
        g.drawEllipse((float)(jx - 10), (float)(jy - 10), 20.0f, 20.0f, 1.5f);

        g.setColour(juce::Colour(0xff999999));
        g.fillEllipse((float)(jx - 4), (float)(jy - 4), 8.0f, 8.0f);

        g.setColour(labelBlack);
        g.setFont(juce::FontOptions(6.5f));
        g.drawFittedText(lbl, jx - 28, jy + 12, 56, 18, juce::Justification::centred, 2);
    };

    struct JackDef { const char* label; };

    // col1 = all INs, col2 = empty, col3 = all OUTs
    JackDef ioRows[] = {
        { kPatchMeta[PP_VCA_CV].name },    { "" }, { kPatchMeta[PP_VCA_EG].name },
        { kPatchMeta[PP_VCA_DECAY].name }, { "" }, { kPatchMeta[PP_VCF_EG].name },
        { kPatchMeta[PP_VCF_MOD].name },   { "" }, { kPatchMeta[PP_VCO_EG].name },
        { kPatchMeta[PP_VCF_DECAY].name }, { "" }, { kPatchMeta[PP_VCO1].name },
        { kPatchMeta[PP_NOISE_LVL].name }, { "" }, { kPatchMeta[PP_VCO2].name },
        { kPatchMeta[PP_VCO_DECAY].name }, { "" }, { kPatchMeta[PP_VELOCITY].name },
        { kPatchMeta[PP_FM_AMT].name },    { "" }, { kPatchMeta[PP_PITCH].name },
        { "" },                            { "" }, { kPatchMeta[PP_MOD_A].name },
        { "" },                            { "" }, { kPatchMeta[PP_MOD_B].name },
        { "" },                            { "" }, { kPatchMeta[PP_MOD_C].name }
    };

    const int col1 = x + kJackInputOffsetX;
    const int col3 = x + kJackOutputOffsetX;

    const int startY = y + kJackStartY;
    const int stride = kJackStride;

    g.setColour(labelBlack);
    g.setFont(juce::FontOptions(7.5f).withStyle("Bold"));

    g.drawText("IN",  col1 - 18, y + 5, 36, 11, juce::Justification::centred);
    g.drawText("OUT", col3 - 20, y + 5, 40, 11, juce::Justification::centred);

    g.setColour(juce::Colour(0xffc7c3bd));
    g.drawLine((float)(x + 4), (float)(y + 19), (float)(x + w - 4), (float)(y + 19), 1.0f);

    for (int r = 0; r < 10; ++r)
    {
        const int idx = r * 3;
        const int jy = startY + r * stride;

        drawJack(col1, jy, ioRows[idx].label);
        drawJack(col3, jy, ioRows[idx + 2].label);
    }

    // --- Patch overlay ----------------------------------------------------
    // Active cables: draw line + green ring on destination
    for (const auto& cable : cables)
    {
        if (!cable.enabled) continue;
        auto src = getJackCentre(cable.src);
        auto dst = getJackCentre(cable.dst);
        if (src.x < 0 || dst.x < 0) continue;

        g.setColour(juce::Colour(0xcc44aaff));   // blue cable
        g.drawLine((float)src.x, (float)src.y,
                   (float)dst.x, (float)dst.y, 1.5f);

        // Green ring on IN jack
        g.setColour(juce::Colour(0xff44ee66));
        g.drawEllipse((float)(dst.x - 12), (float)(dst.y - 12), 24.0f, 24.0f, 1.5f);
    }

    // Selected OUT jack: yellow ring only
    if (selectedOut != PP_NUM_POINTS)
    {
        auto c = getJackCentre(selectedOut);
        if (c.x >= 0)
        {
            g.setColour(juce::Colour(0xffffcc00));
            g.drawEllipse((float)(c.x - 12), (float)(c.y - 12), 24.0f, 24.0f, 2.0f);
        }
    }
    // ----------------------------------------------------------------------
}

void XTEditor::paint(juce::Graphics& g)
{
    const int W     = getWidth();
    const int H     = getHeight();
    const int wood  = 18;
    const int jackW = kJackPanelWidth;
    const int mainRight = W - wood - jackW;

    // Wood panels
    juce::ColourGradient woodGrad(
        juce::Colour(0xffa07820), 0, 0,
        juce::Colour(0xff6b4c10), 0, (float)H, false);
    g.setGradientFill(woodGrad);
    g.fillRect(0, 0, wood, H);
    g.fillRect(W - wood, 0, wood, H);
    g.setColour(juce::Colour(0x22000000));
    for (int i = 2; i < H; i += 5)
    {
        g.drawLine(1.0f, (float)i, (float)(wood-1), (float)i, 0.5f);
        g.drawLine((float)(W-wood+1), (float)i, (float)(W-1), (float)i, 0.5f);
    }

    // Main panel
    g.setColour(panelWhite);
    g.fillRect(wood, 0, W - wood * 2, H);
    g.setColour(juce::Colour(0xff888888));
    g.drawRect(wood, 0, W - wood * 2, H, 1);

    // Jack panel
    std::vector<PatchCable> cables;
    xtProcessor.getCableSnapshot(cables);
    drawJackPanel(g, W - wood - jackW, 0, jackW, H, cables, pendingOut);

    // Screws
    auto drawScrew = [&](float sx, float sy) {
        g.setColour(juce::Colour(0xff999999));
        g.fillEllipse(sx-6, sy-6, 12, 12);
        g.setColour(juce::Colour(0xff444444));
        g.drawEllipse(sx-6, sy-6, 12, 12, 1.0f);
        g.setColour(juce::Colour(0xff222222));
        g.drawLine(sx-3, sy, sx+3, sy, 1.2f);
        g.drawLine(sx, sy-3, sx, sy+3, 1.2f);
    };
    drawScrew((float)(wood+14), 14.0f);
    drawScrew((float)(W-wood-jackW-14), 14.0f);
    drawScrew((float)(wood+14), (float)(H-14));
    drawScrew((float)(W-wood-jackW-14), (float)(H-14));

    // Separator lines
    g.setColour(juce::Colour(0xffbbbbbb));
    g.drawLine((float)wood, (float)kTopControlDividerY, (float)mainRight, (float)kTopControlDividerY, 0.8f);
    g.drawLine((float)wood, (float)kSequencerDividerY, (float)mainRight, (float)kSequencerDividerY, 0.8f);
    g.drawLine((float)wood, (float)kEditorBodyHeight, (float)(W-wood-jackW), (float)kEditorBodyHeight, 0.8f);

    const int mainW = W - wood * 2 - jackW;
    const int kS    = (mainW - 60) / 11;
    const int offX  = wood + 30;
    const int transportX = wood + kTransportOffsetX;
    const int rowLabelX = wood + kRowLabelOffsetX;
    const int seqX = wood + kSequencerGridOffsetX;
    const int seqW = mainRight - 14 - seqX;
    const int stepW = seqW / XTSequencer::numSteps;
    const int firstKnobX = seqX + (stepW - kSequencerKnobSize) / 2;
    const int rowLabelW = juce::jmax(48, firstKnobX - rowLabelX - 10);

    g.setColour(labelBlack);
    g.setFont(juce::FontOptions(7.5f).withStyle("Bold"));
    g.drawText("PRESET", wood + 24, kEditorBodyHeight + 12, 44, 10, juce::Justification::centredLeft);

    // Row 1 labels
    const char* top[] = {
            "VCO DECAY","SEQ PITCH MOD","VCO 1 EG AMT","VCO 1 FREQ",
            "VCO 1 WAVE","VCO 1 LEVEL","NOISE LVL","CUTOFF","RESONANCE","VCA EG AMT","VOLUME"
        };
    for (int i = 0; i < 11; ++i)
        g.drawText(top[i], offX + i * kS, 8, kS, 10, juce::Justification::centred);

    // Row 2 labels
    const char* bot[] = {
                "1-2 FM AMT","HARD SYNC","VCO 2 EG AMT","VCO 2 FREQ",
                "VCO 2 WAVE","VCO 2 LEVEL","VCF DECAY","VCF EG AMT","NOISE/VCF MOD","","VCA DECAY"
            };
    for (int i = 0; i < 11; ++i)
        g.drawText(bot[i], offX + i * kS, 167, kS, 10, juce::Justification::centred);

    const int modSectionX = offX + 7 * kS;
    const int modSectionW = mainRight - 20 - modSectionX;
    const int modColumnW = modSectionW / (int) kModRouteNames.size();
    g.setFont(juce::FontOptions(7.5f).withStyle("Bold"));
    g.drawText("MOD ROUTING", modSectionX, kModSectionY, modSectionW, 10, juce::Justification::centredLeft);
    for (int i = 0; i < (int) kModRouteNames.size(); ++i)
    {
        const int x = modSectionX + i * modColumnW;
        g.drawText(kModRouteNames[(size_t) i], x, kModSectionY + 16, modColumnW, 10, juce::Justification::centred);
        g.setFont(juce::FontOptions(6.5f));
        g.drawText("AMT", x, kModSectionY + 30, modColumnW, 9, juce::Justification::centred);
        g.drawText("DEST", x, kModSectionY + 62, modColumnW, 9, juce::Justification::centred);
        g.setFont(juce::FontOptions(7.5f).withStyle("Bold"));
    }

    // Sequencer / transport headers
    g.setFont(juce::FontOptions(7.5f).withStyle("Bold"));
    g.drawText("TRANSPORT", transportX, kSequencerHeaderY, kTransportWidth, 10, juce::Justification::centredLeft);
    g.drawText("SEQUENCER", rowLabelX, kSequencerHeaderY, 82, 10, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(7.0f));
    g.drawText("16 STEPS  ·  PITCH  ·  VELOCITY  ·  MOD A  ·  MOD B  ·  MOD C",
               seqX, kSequencerHeaderY, seqW, 10, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(7.5f).withStyle("Bold"));
    g.drawText("CLOCK", transportX, 346, kTransportWidth, 10, juce::Justification::centredLeft);
    g.drawText("PLAYHEAD", transportX, 430, kTransportWidth, 10, juce::Justification::centredLeft);

    for (int i = 0; i < XTSequencer::numSteps; ++i)
        g.drawText(juce::String(i + 1), seqX + i * stepW, kSequencerStepNumberY, stepW, 10, juce::Justification::centred);

    for (int row = 0; row < XTSequencer::numLaneRows; ++row)
    {
        const int rowY = getSequencerRowY(row);
        g.setFont(juce::FontOptions(7.5f).withStyle("Bold"));
        g.drawText(kSequencerLaneNames[(size_t) row], rowLabelX, rowY + 3, rowLabelW, 10, juce::Justification::centredRight);
        g.setFont(juce::FontOptions(6.5f));
        g.drawText(kSequencerLaneSubtitles[(size_t) row], rowLabelX, rowY + 15, rowLabelW, 9, juce::Justification::centredRight);

        g.setColour(juce::Colour(0xffd0ccc6));
        const float guideY = (float)(rowY + kSequencerKnobSize / 2);
        g.drawLine((float)seqX, guideY, (float)(seqX + XTSequencer::numSteps * stepW), guideY, 0.8f);
        g.setColour(labelBlack);
    }

    for (int i = 0; i < XTSequencer::numSteps; ++i)
    {
        const float lx = (float)(seqX + i * stepW + stepW / 2);
        const float ly = (float)kSequencerLedY;
        const bool active = (i == currentLedStep);

        if (active)
        {
            g.setColour(juce::Colour(0x22ff2200));
            g.fillEllipse(lx - 10, ly - 10, 20, 20);
            g.setColour(juce::Colour(0x44ff2200));
            g.fillEllipse(lx - 8, ly - 8, 16, 16);
        }

        juce::ColourGradient ledGrad(
            active ? juce::Colour(0xffff4422) : juce::Colour(0xff553322),
            lx - 3, ly - 3,
            active ? juce::Colour(0xffaa1100) : juce::Colour(0xff221111),
            lx + 3, ly + 3,
            true);
        g.setGradientFill(ledGrad);
        g.fillEllipse(lx - 5, ly - 5, 10, 10);

        if (active)
        {
            g.setColour(juce::Colour(0x88ffffff));
            g.fillEllipse(lx - 2.5f, ly - 3.5f, 3.0f, 2.0f);
        }

        g.setColour(active ? juce::Colour(0xff882200) : juce::Colour(0xff222222));
        g.drawEllipse(lx - 5, ly - 5, 10, 10, 1.0f);
    }

    // Branding
    g.setColour(labelBlack);
    g.setFont(juce::FontOptions(30.0f).withStyle("Bold"));
    g.drawText("DFAF XT", W-wood-jackW-20, H-56, 200, 32, juce::Justification::centredRight);
    g.setFont(juce::FontOptions(7.5f).withStyle("Bold"));
    g.drawText("16 STEP PATCH SEQUENCER", W-wood-jackW-20, H-24, 200, 11, juce::Justification::centredRight);
    g.setFont(juce::FontOptions(7.5f));
    g.drawText("SEQUENCER-FIRST SEMI-MODULAR XT SYNTH", W-wood-jackW-20, H-13, 200, 11, juce::Justification::centredRight);
}

void XTEditor::resized()
{
    const int W     = getWidth();
    const int wood  = 18;
    const int jackW = kJackPanelWidth;
    const int mainRight = W - wood - jackW;
    const int mainW = W - wood * 2 - jackW;
    const int kS    = (mainW - 60) / 11;
    const int offX  = wood + 30;
    const int kSz   = 54;
    const int sSz   = kSequencerKnobSize;
    const int footerY = kEditorBodyHeight + 7;

    presetBox.setBounds(wood + 76, footerY, 220, 22);
    presetSaveButton.setBounds(wood + 306, footerY, 70, 22);
    presetDeleteButton.setBounds(wood + 384, footerY, 78, 22);
    presetInitButton.setBounds(wood + 470, footerY, 54, 22);

    seqPitchModBox.setBounds(offX + 1*kS + (kS-60)/2, 50, 60, 22);
    hardSyncBox.setBounds(offX + 1*kS + (kS-60)/2, 210, 60, 22);
    vco1WaveBox.setBounds(offX + 4*kS + (kS-60)/2, 50, 60, 22);
    vco2WaveBox.setBounds(offX + 4*kS + (kS-60)/2, 210, 60, 22);
    vcfModeBox.setBounds(offX + 6*kS - kS/2 + (kS/2-60)/2, 50, 60, 22);

    int r1slots[]  = { 0, 2, 3, 5, 6, 7, 8, 9, 10 };
        juce::Slider* row1[] = {
            &vcoDecay, &vco1EgAmount, &vco1Frequency,
            &vco1Level, &noiseLevel, &cutoff, &resonance, &vcaEg, &volume
        };
        int r1sizes[] = { kSz,kSz,kSz, sSz,sSz, kSz,kSz,kSz,kSz };
        for (int i = 0; i < 9; ++i)
            row1[i]->setBounds(offX + r1slots[i]*kS + (kS-r1sizes[i])/2, 22, r1sizes[i], r1sizes[i]);

    // Row 2: positions 0, 2,3, 5, 6,7,8, 10
    int r2slots[]  = { 0, 2, 3, 5, 6, 7, 8, 10 };
        juce::Slider* row2[] = {
            &fmAmount, &vco2EgAmount, &vco2Frequency,
            &vco2Level, &vcfDecay, &vcfEgAmount, &noiseVcfMod, &vcaDecay
        };
        int r2sizes[] = { kSz,kSz,kSz, sSz,kSz,kSz,kSz,kSz };
    for (int i = 0; i < 8; ++i)
        row2[i]->setBounds(offX + r2slots[i]*kS + (kS-r2sizes[i])/2, 182, r2sizes[i], r2sizes[i]);

    const int modSectionX = offX + 7 * kS;
    const int modSectionW = mainRight - 20 - modSectionX;
    const int modColumnW = modSectionW / (int) kModRouteNames.size();
    for (int i = 0; i < (int) kModRouteNames.size(); ++i)
    {
        const int x = modSectionX + i * modColumnW;
        modAmount[i].setBounds(x + (modColumnW - kModAmountSize) / 2, kModAmountY, kModAmountSize, kModAmountSize);
        modDestBox[i].setBounds(x + (modColumnW - kModDestinationWidth) / 2, kModDestinationY, kModDestinationWidth, 22);
    }

    // Sequencer
    clockMultBox.setBounds(wood + kTransportOffsetX, 360, kTransportWidth, 22);
    resetButton.setBounds(wood + kTransportOffsetX, 444, kTransportWidth, 24);

    const int seqX = wood + kSequencerGridOffsetX;
    const int seqW = mainRight - 14 - seqX;
    const int stepW = seqW / XTSequencer::numSteps;
    for (int i = 0; i < XTSequencer::numSteps; ++i)
    {
        const int x = seqX + i * stepW + (stepW - sSz) / 2;
        stepPitch[i].setBounds(x, getSequencerRowY(0), sSz, sSz);
        stepVelocity[i].setBounds(x, getSequencerRowY(1), sSz, sSz);
        stepModA[i].setBounds(x, getSequencerRowY(2), sSz, sSz);
        stepModB[i].setBounds(x, getSequencerRowY(3), sSz, sSz);
        stepModC[i].setBounds(x, getSequencerRowY(4), sSz, sSz);
    }
}
