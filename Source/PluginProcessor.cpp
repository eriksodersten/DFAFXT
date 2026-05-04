#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr auto kInitPresetName    = "Init";
constexpr auto kPresetExtension   = ".dfafxtpreset";
constexpr auto kPresetNameAttribute = "currentPresetName";
constexpr int  kNumModRoutes = 2;

juce::String makeStepParameterId(const char* prefix, int index)
{
    return juce::String(prefix) + juce::String(index);
}

juce::String makeModDestinationParameterId(int index)
{
    return "mod" + juce::String::charToString((juce_wchar)('A' + index)) + "Dest";
}

XTModDestination choiceToModDestination(int rawChoice)
{
    const int clamped = juce::jlimit(0, (int)XTModDestination::Count - 1, rawChoice);
    return static_cast<XTModDestination>(clamped);
}
}

juce::StringArray XTProcessor::getModDestinationNames()
{
    return { "OFF", "CUTOFF", "RESONANCE", "FM AMT", "NOISE",
             "VCF DEC", "VCA DEC", "VCO DEC", "VOLUME",
             "CLK TUNE", "PRE DRV" };
}

juce::AudioProcessorValueTreeState::ParameterLayout XTProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto vcoDecayRange = juce::NormalisableRange<float>(0.01f, 2.0f);
    vcoDecayRange.setSkewForCentre(0.40f);
    auto vcfDecayRange = juce::NormalisableRange<float>(0.01f, 10.0f);
    vcfDecayRange.setSkewForCentre(1.50f);
    auto vcaDecayRange = juce::NormalisableRange<float>(0.01f, 2.0f);
    vcaDecayRange.setSkewForCentre(0.40f);

    params.push_back(std::make_unique<juce::AudioParameterFloat>("vcoDecay",   "VCO Decay",  vcoDecayRange, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vco1Freq",   "VCO 1 Freq", 1.0f,  2000.0f, 220.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vco1EgAmt",  "VCO 1 EG Amt", -60.0f, 60.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("fmAmount",   "FM Amount",  0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vco2Freq",   "VCO 2 Freq", 1.0f,  2000.0f, 330.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vco2EgAmt",  "VCO 2 EG Amt", -60.0f, 60.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("noiseLevel", "Noise Level", 0.0f, 1.0f, 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vco1Level",  "VCO 1 Level", 0.0f, 1.0f, 0.6f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vco2Level",  "VCO 2 Level", 0.0f, 1.0f, 0.2f));

    // VCO2 independent pitch-sweep decay
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vco2Decay",  "VCO 2 Decay", vcoDecayRange, 0.3f));

    // VCO EG shape
    params.push_back(std::make_unique<juce::AudioParameterChoice>("vcoEgShape", "VCO EG Shape",
        juce::StringArray({ "EXP", "LIN", "LOG" }), 1));

    auto cutoffRange = juce::NormalisableRange<float>(20.0f, 8000.0f);
    cutoffRange.setSkewForCentre(800.0f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>("cutoff",      "Cutoff",      cutoffRange, 800.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("resonance",   "Resonance",   0.0f, 1.0f, 0.4f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vcfDecay",    "VCF Decay",   vcfDecayRange, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vcfEgAmt",    "VCF EG Amt",  -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("noiseVcfMod", "Noise VCF Mod", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vcaDecay",    "VCA Decay",   vcaDecayRange, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vcaEg",       "VCA EG",      0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("preTrim",     "Pre Trim",    0.1f, 2.0f, 0.84f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("volume",      "Volume",      0.0f, 1.0f, 0.8f));

    // Noise color
    params.push_back(std::make_unique<juce::AudioParameterFloat>("noiseColor",  "Noise Color", 0.0f, 1.0f, 0.0f));

    // Velocity → VCF decay sensitivity (replaces hardcoded 0.5)
    params.push_back(std::make_unique<juce::AudioParameterFloat>("velVcfDecaySens", "Vel VCF Decay", 0.0f, 1.0f, 0.5f));

    // Pitch → FM amount coupling
    params.push_back(std::make_unique<juce::AudioParameterFloat>("pitchFmAmt",  "Pitch FM Amt", 0.0f, 1.0f, 0.0f));

    for (int i = 0; i < kNumModRoutes; ++i)
    {
        const auto sourceName = "Mod " + juce::String::charToString((juce_wchar)('A' + i));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            makeModDestinationParameterId(i), sourceName + " Destination", getModDestinationNames(), 0));
    }

    params.push_back(std::make_unique<juce::AudioParameterChoice>("clockMult", "Clock Multiplier",
        juce::StringArray({ "1/8", "1/5", "1/4", "1/3", "1/2", "1x", "2x", "3x", "4x", "5x" }), 5));

    for (int i = 0; i < XTSequencer::numSteps; ++i)
    {
        const auto stepNumber = juce::String(i + 1);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            makeStepParameterId("stepPitch", i), "Step Pitch " + stepNumber, 0.0f, 120.0f, 60.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            makeStepParameterId("stepVel", i),   "Step Velocity " + stepNumber, 0.0f, 1.0f, 0.8f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            makeStepParameterId("stepModA", i),  "Step Mod A " + stepNumber, 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            makeStepParameterId("stepModB", i),  "Step Mod B " + stepNumber, 0.0f, 1.0f, 0.5f));
    }

    params.push_back(std::make_unique<juce::AudioParameterChoice>("seqPitchMod", "SEQ Pitch Mod",
        juce::StringArray({ "VCO 1&2", "OFF", "VCO 2" }), 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>("hardSync", "Hard Sync", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("vco1Wave", "VCO 1 Wave",
        juce::StringArray({ "Square", "Triangle", "Metal" }), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("vco2Wave", "VCO 2 Wave",
        juce::StringArray({ "Square", "Triangle" }), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("vcfMode",  "VCF Mode",
        juce::StringArray({ "LP", "HP" }), 0));

    // Click
    auto clickTuneRange = juce::NormalisableRange<float>(20.0f, 8000.0f);
    clickTuneRange.setSkewForCentre(600.0f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>("clickTune",  "Click Tune",  clickTuneRange, 800.0f));
    auto clickDecayRange = juce::NormalisableRange<float>(0.001f, 0.2f);
    clickDecayRange.setSkewForCentre(0.02f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>("clickDecay", "Click Decay", clickDecayRange, 0.015f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("clickLevel", "Click Level", 0.0f, 1.0f, 0.0f));

    // VCA attack
    auto vcaAttackRange = juce::NormalisableRange<float>(0.001f, 0.5f);
    vcaAttackRange.setSkewForCentre(0.02f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vcaAttack", "VCA Attack", vcaAttackRange, 0.001f));

    // Drive
    auto driveRange = juce::NormalisableRange<float>(1.0f, 10.0f);
    driveRange.setSkewForCentre(2.5f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>("preDrive",  "Pre Drive",  driveRange, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("postDrive", "Post Drive", driveRange, 1.0f));

    // LFO
    auto lfoRateRange = juce::NormalisableRange<float>(0.01f, 20.0f);
    lfoRateRange.setSkewForCentre(2.0f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lfoRate",   "LFO Rate",   lfoRateRange, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("lfoWave",  "LFO Wave",
        juce::StringArray({ "TRI", "SAW", "SQR", "RND" }), 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>("lfoSync",    "LFO Sync",   false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("lfoRetrig",  "LFO Retrig", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("lfoDest",  "LFO Dest",
        XTProcessor::getModDestinationNames(), 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lfoAmt",    "LFO Amt",    0.0f, 1.0f, 0.5f));

    // Step active × 16
    for (int i = 0; i < XTSequencer::numSteps; ++i)
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            makeStepParameterId("stepActive", i),
            "Step Active " + juce::String(i + 1), true));

    // Internal transport
    auto tempoRange = juce::NormalisableRange<float>(30.0f, 300.0f);
    tempoRange.setSkewForCentre(120.0f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>("tempo", "Tempo", tempoRange, 120.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("swing", "Swing", 0.0f, 0.5f, 0.0f));

    auto noiseDecayRange = juce::NormalisableRange<float>(0.01f, 2.0f);
    noiseDecayRange.setSkewForCentre(0.40f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>("noiseDecay", "Noise Decay", noiseDecayRange, 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterBool>("noiseVcfBypass", "Noise Bypass VCF", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("clickVcfBypass", "Click Bypass VCF", false));

    return { params.begin(), params.end() };
}

XTProcessor::XTProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "DFAFXT", createParameterLayout())
{
    initialiseMidiCcBindings();
    defaultState = apvts.copyState();
}

XTProcessor::~XTProcessor() {}

juce::File XTProcessor::getPresetDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("DFAFXT").getChildFile("Presets");
}

juce::File XTProcessor::getPresetFile(const juce::String& presetName) const
{
    return getPresetDirectory().getChildFile(sanitisePresetName(presetName) + kPresetExtension);
}

juce::String XTProcessor::sanitisePresetName(const juce::String& presetName) const
{
    juce::String safeName;
    for (auto character : presetName.trim())
        if (juce::CharacterFunctions::isLetterOrDigit(character)
            || character == ' ' || character == '-' || character == '_')
            safeName << character;
    return safeName.trim();
}

juce::StringArray XTProcessor::getAvailablePresetNames() const
{
    juce::StringArray names;
    auto presetDirectory = getPresetDirectory();
    if (!presetDirectory.isDirectory()) return names;
    juce::Array<juce::File> presetFiles;
    presetDirectory.findChildFiles(presetFiles, juce::File::findFiles, false,
                                   "*" + juce::String(kPresetExtension));
    for (const auto& f : presetFiles)
        names.addIfNotAlreadyThere(f.getFileNameWithoutExtension());
    names.sort(true);
    return names;
}

juce::String XTProcessor::getCurrentPresetName() const
{
    const juce::ScopedLock sl(presetLock);
    return currentPresetName;
}

std::unique_ptr<juce::XmlElement> XTProcessor::createStateXml()
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    {
        const juce::ScopedLock sl(presetLock);
        xml->setAttribute(kPresetNameAttribute, currentPresetName);
    }
    return xml;
}

void XTProcessor::restoreStateFromXml(const juce::XmlElement& xml,
                                      const juce::String& presetNameOverride)
{
    auto stateXml = std::make_unique<juce::XmlElement>(xml);

    // Silently ignore legacy <PatchCables> blocks from old presets
    if (auto* cables = stateXml->getChildByName("PatchCables"))
        stateXml->removeChildElement(cables, true);

    if (stateXml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*stateXml));

    const auto restoredName = presetNameOverride.isNotEmpty()
        ? presetNameOverride
        : xml.getStringAttribute(kPresetNameAttribute, kInitPresetName);

    { const juce::ScopedLock sl(presetLock); currentPresetName = restoredName; }
}

bool XTProcessor::savePreset(const juce::String& presetName)
{
    const auto safeName = sanitisePresetName(presetName);
    if (safeName.isEmpty()) return false;
    auto dir = getPresetDirectory();
    if (!dir.isDirectory() && !dir.createDirectory()) return false;
    auto xml = createStateXml();
    xml->setAttribute(kPresetNameAttribute, safeName);
    if (!getPresetFile(safeName).replaceWithText(xml->toString())) return false;
    { const juce::ScopedLock sl(presetLock); currentPresetName = safeName; }
    updateHostDisplay();
    return true;
}

bool XTProcessor::saveCurrentPreset()
{
    const auto n = getCurrentPresetName();
    if (n == kInitPresetName) return false;
    return savePreset(n);
}

bool XTProcessor::loadPreset(const juce::String& presetName)
{
    const auto safeName = sanitisePresetName(presetName);
    if (safeName.isEmpty()) return false;
    auto f = getPresetFile(safeName);
    if (!f.existsAsFile()) return false;
    auto xml = juce::XmlDocument::parse(f);
    if (!xml) return false;
    restoreStateFromXml(*xml, safeName);
    updateHostDisplay();
    return true;
}

bool XTProcessor::deletePreset(const juce::String& presetName)
{
    const auto safeName = sanitisePresetName(presetName);
    if (safeName.isEmpty() || safeName == kInitPresetName) return false;
    auto f = getPresetFile(safeName);
    if (!f.existsAsFile() || !f.deleteFile()) return false;
    if (getCurrentPresetName() == safeName) loadInitPreset();
    updateHostDisplay();
    return true;
}

void XTProcessor::loadInitPreset()
{
    apvts.replaceState(defaultState.createCopy());
    { const juce::ScopedLock sl(presetLock); currentPresetName = kInitPresetName; }
    updateHostDisplay();
}

int XTProcessor::getNumPrograms()    { return getAvailablePresetNames().size() + 1; }
int XTProcessor::getCurrentProgram()
{
    const auto cur   = getCurrentPresetName();
    if (cur == kInitPresetName) return 0;
    const auto names = getAvailablePresetNames();
    const int  idx   = names.indexOf(cur);
    return idx >= 0 ? idx + 1 : 0;
}
void XTProcessor::setCurrentProgram(int index)
{
    if (index <= 0) { loadInitPreset(); return; }
    const auto names = getAvailablePresetNames();
    if (juce::isPositiveAndBelow(index - 1, names.size())) loadPreset(names[index - 1]);
}
const juce::String XTProcessor::getProgramName(int index)
{
    if (index == 0) return kInitPresetName;
    const auto names = getAvailablePresetNames();
    return juce::isPositiveAndBelow(index - 1, names.size()) ? names[index - 1] : juce::String{};
}
void XTProcessor::changeProgramName(int index, const juce::String& newName)
{
    if (index <= 0) return;
    const auto names   = getAvailablePresetNames();
    if (!juce::isPositiveAndBelow(index - 1, names.size())) return;
    const auto oldName    = names[index - 1];
    const auto safeNew    = sanitisePresetName(newName);
    if (safeNew.isEmpty() || safeNew == oldName) return;
    auto oldFile = getPresetFile(oldName);
    auto newFile = getPresetFile(safeNew);
    if (newFile.existsAsFile()) return;
    if (oldFile.moveFileTo(newFile) && getCurrentPresetName() == oldName)
    { const juce::ScopedLock sl(presetLock); currentPresetName = safeNew; }
    updateHostDisplay();
}

// =============================================================================

void XTProcessor::initialiseMidiCcBindings()
{
    midiCcBindings.fill({});
    size_t idx = 0;
    auto add = [this, &idx](int cc, const juce::String& id)
    {
        jassert(idx < midiCcBindings.size());
        auto& b = midiCcBindings[idx++];
        b.cc        = cc;
        b.parameter = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id));
        jassert(b.parameter != nullptr);
    };

    add(20, "vcoDecay");     add(21, "vco1EgAmt");   add(22, "vco1Freq");
    add(23, "vco1Level");    add(24, "noiseLevel");   add(25, "cutoff");
    add(26, "resonance");    add(27, "vcaEg");        add(28, "volume");
    add(29, "fmAmount");     add(30, "vco2EgAmt");    add(31, "vco2Freq");
    add(32, "vco2Level");    add(33, "vcfDecay");     add(34, "vcfEgAmt");
    add(35, "noiseVcfMod");  add(36, "vcaDecay");

    int cc = 37;
    for (int i = 0; i < XTSequencer::numSteps; ++i) add(cc++, makeStepParameterId("stepPitch", i));
    for (int i = 0; i < XTSequencer::numSteps; ++i) add(cc++, makeStepParameterId("stepVel",   i));
    for (int i = 0; i < XTSequencer::numSteps; ++i) add(cc++, makeStepParameterId("stepModA",  i));
    for (int i = 0; i < XTSequencer::numSteps; ++i) add(cc++, makeStepParameterId("stepModB",  i));
    add(cc++, "seqPitchMod");  add(cc++, "hardSync");    add(cc++, "vco1Wave");
    add(cc++, "vco2Wave");     add(cc++, "vcfMode");      add(cc++, "clockMult");
    add(cc++, "clickTune");    add(cc++, "clickDecay");   add(cc++, "clickLevel");
    add(cc++, "vcaAttack");    add(cc++, "preDrive");     add(cc++, "postDrive");
    add(cc++, "lfoRate");      add(cc++, "lfoWave");      add(cc++, "lfoSync");
    add(cc++, "lfoRetrig");    add(cc++, "lfoDest");      add(cc++, "lfoAmt");

    for (int i = 0; i < XTSequencer::numSteps; ++i) add(cc++, makeStepParameterId("stepActive", i));

    // New params
    add(cc++, "vco2Decay");        add(cc++, "vcoEgShape");
    add(cc++, "noiseColor");       add(cc++, "velVcfDecaySens");
    add(cc++, "pitchFmAmt");       add(cc++, "tempo");
    add(cc++, "swing");
    add(cc++, "noiseDecay");
    add(cc++, "noiseVcfBypass");
    add(cc++, "clickVcfBypass");

    jassert(idx == midiCcBindings.size());
}

void XTProcessor::applyMidiCc(int ccNumber, int ccValue)
{
    // CC 120-127 are reserved MIDI channel mode messages (All Notes Off, etc.)
    // Hosts send these on transport stop — never treat them as parameter changes.
    if (ccNumber >= 120) return;

    const float norm = juce::jlimit(0.0f, 1.0f, (float)ccValue / 127.0f);
    for (const auto& b : midiCcBindings)
    {
        if (b.cc != ccNumber || b.parameter == nullptr) continue;
        if (dynamic_cast<juce::AudioParameterChoice*>(b.parameter) != nullptr
            || dynamic_cast<juce::AudioParameterBool*>(b.parameter) != nullptr)
        {
            const int steps = juce::jmax(2, b.parameter->getNumSteps());
            const int step  = juce::jlimit(0, steps - 1, (int)std::round(norm * (float)(steps - 1)));
            b.parameter->setValueNotifyingHost((float)step / (float)(steps - 1));
            return;
        }
        b.parameter->setValueNotifyingHost(norm);
        return;
    }
}

void XTProcessor::setPlayPage(int page)
{
    playPage.store(juce::jlimit(0, 1, page), std::memory_order_release);
    sequencerResetPending.store(true, std::memory_order_release);
}

void XTProcessor::copyPageAtoB()
{
    const char* prefixes[] = { "stepPitch", "stepVel", "stepModA", "stepModB", "stepActive" };
    for (const char* prefix : prefixes)
        for (int i = 0; i < 8; ++i)
            if (auto* src = apvts.getParameter(makeStepParameterId(prefix, i)))
                if (auto* dst = apvts.getParameter(makeStepParameterId(prefix, i + 8)))
                    dst->setValueNotifyingHost(src->getValue());
}

// =============================================================================

void XTProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate;
    sequencer.prepare(sampleRate);
    voice.prepare(sampleRate);
    filter.prepare(sampleRate);
    filter.setCutoff(800.0f);
    filter.setResonance(0.4f);

    vcfDecayParam = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter("vcfDecay"));
    vcaDecayParam = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter("vcaDecay"));
    vcoDecayParam = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter("vcoDecay"));

    smoothedCutoff.reset(sampleRate, 0.01);
    smoothedVolume.reset(sampleRate, 0.01);
    smoothedVco1Level.reset(sampleRate, 0.01);
    smoothedVco2Level.reset(sampleRate, 0.01);
    smoothedPreDrive.reset(sampleRate, 0.01);
    smoothedPostDrive.reset(sampleRate, 0.01);
    smoothedCutoff.setCurrentAndTargetValue(apvts.getRawParameterValue("cutoff")->load());
    smoothedVolume.setCurrentAndTargetValue(apvts.getRawParameterValue("volume")->load());
    smoothedVco1Level.setCurrentAndTargetValue(apvts.getRawParameterValue("vco1Level")->load());
    smoothedVco2Level.setCurrentAndTargetValue(apvts.getRawParameterValue("vco2Level")->load());
    smoothedPreDrive.setCurrentAndTargetValue(apvts.getRawParameterValue("preDrive")->load());
    smoothedPostDrive.setCurrentAndTargetValue(apvts.getRawParameterValue("postDrive")->load());
}

void XTProcessor::releaseResources() {}

// =============================================================================

void XTProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    // --- MIDI ---
    for (const auto metadata : midiMessages)
    {
        const auto& msg = metadata.getMessage();
        if (msg.isController())
            applyMidiCc(msg.getControllerNumber(), msg.getControllerValue());
        else if (msg.isNoteOn())
            voice.trigger((float)msg.getNoteNumber(), msg.getFloatVelocity());
    }

    // --- Host transport ---
    float  bpm        = 120.0f;
    bool   isPlaying  = false;
    double ppqPosition = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (pos->getBpm().hasValue())      bpm        = (float)*pos->getBpm();
            isPlaying = pos->getIsPlaying();
            if (pos->getPpqPosition().hasValue()) ppqPosition = *pos->getPpqPosition();
        }

    // --- Internal transport: takes over when host is stopped ---
    const bool intRunning = internalTransportRunning.load(std::memory_order_relaxed);
    const float internalBpm = apvts.getRawParameterValue("tempo")->load();
    if (!isPlaying && intRunning) {
        bpm       = internalBpm;
        isPlaying = true;
        // ppqPosition is managed sample-by-sample below via internalPpq
    }

    // --- Pending one-shot transport actions ---
    if (triggerPending.exchange(false, std::memory_order_acq_rel))
        voice.trigger(60.0f, 1.0f);

    if (advancePending.exchange(false, std::memory_order_acq_rel))
        lastStep = -1;  // force step advance on next sample

    // --- Param reads ---
    int   multIndex  = (int)apvts.getRawParameterValue("clockMult")->load();
    const float multTable[] = { 8.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.5f, 1.0f/3.0f, 0.25f, 0.2f };
    float mult       = multTable[juce::jlimit(0, 9, multIndex)];
    float ppqPerStep = 0.25f * mult;

    constexpr int stepCount = 8;
    int pageOffset   = playPage.load(std::memory_order_relaxed) * 8;

    float cutoffVal    = apvts.getRawParameterValue("cutoff")->load();
    float resVal       = apvts.getRawParameterValue("resonance")->load();
    float volumeVal    = apvts.getRawParameterValue("volume")->load();
    float vcoDecayVal  = apvts.getRawParameterValue("vcoDecay")->load();
    float vco2DecayVal = apvts.getRawParameterValue("vco2Decay")->load();
    float vcaDecayVal  = apvts.getRawParameterValue("vcaDecay")->load();
    float vcfDecayVal  = apvts.getRawParameterValue("vcfDecay")->load();
    float fmVal        = apvts.getRawParameterValue("fmAmount")->load();
    float vco1Freq     = apvts.getRawParameterValue("vco1Freq")->load();
    float vco2Freq     = apvts.getRawParameterValue("vco2Freq")->load();
    int   seqPitchRouting = (int)apvts.getRawParameterValue("seqPitchMod")->load();
    float vco1EgAmt    = apvts.getRawParameterValue("vco1EgAmt")->load();
    float vco2EgAmt    = apvts.getRawParameterValue("vco2EgAmt")->load();
    float vcfEgAmt     = apvts.getRawParameterValue("vcfEgAmt")->load();
    float noiseVcfMod  = apvts.getRawParameterValue("noiseVcfMod")->load();
    float noiseLevelVal= apvts.getRawParameterValue("noiseLevel")->load();
    float vcaEgVal     = apvts.getRawParameterValue("vcaEg")->load();
    float preTrimVal   = apvts.getRawParameterValue("preTrim")->load();
    float clickTuneVal = apvts.getRawParameterValue("clickTune")->load();
    float clickDecayVal= apvts.getRawParameterValue("clickDecay")->load();
    float clickLevelVal= apvts.getRawParameterValue("clickLevel")->load();
    float vcaAttackVal = apvts.getRawParameterValue("vcaAttack")->load();
    float preDriveVal  = apvts.getRawParameterValue("preDrive")->load();
    float postDriveVal = apvts.getRawParameterValue("postDrive")->load();
    float lfoRateVal   = apvts.getRawParameterValue("lfoRate")->load();
    int   lfoWaveVal   = (int)apvts.getRawParameterValue("lfoWave")->load();
    bool  lfoSyncVal   = apvts.getRawParameterValue("lfoSync")->load() > 0.5f;
    bool  lfoRetrigVal = apvts.getRawParameterValue("lfoRetrig")->load() > 0.5f;
    float lfoAmtVal    = apvts.getRawParameterValue("lfoAmt")->load();
    XTModDestination lfoDestVal = choiceToModDestination(
        (int)apvts.getRawParameterValue("lfoDest")->load());
    float noiseColorVal     = apvts.getRawParameterValue("noiseColor")->load();
    float noiseDecayVal     = apvts.getRawParameterValue("noiseDecay")->load();
    bool  noiseVcfBypass    = apvts.getRawParameterValue("noiseVcfBypass")->load() > 0.5f;
    bool  clickVcfBypass    = apvts.getRawParameterValue("clickVcfBypass")->load() > 0.5f;
    float velVcfDecaySensVal= apvts.getRawParameterValue("velVcfDecaySens")->load();
    float pitchFmAmtVal     = apvts.getRawParameterValue("pitchFmAmt")->load();
    int   vcoEgShapeVal     = (int)apvts.getRawParameterValue("vcoEgShape")->load();

    std::array<XTModDestination, kNumModRoutes> modDestinations;
    for (int i = 0; i < kNumModRoutes; ++i)
        modDestinations[(size_t)i] = choiceToModDestination(
            (int)apvts.getRawParameterValue(makeModDestinationParameterId(i))->load());

    smoothedCutoff.setTargetValue(cutoffVal);
    smoothedVolume.setTargetValue(volumeVal);
    smoothedVco1Level.setTargetValue(apvts.getRawParameterValue("vco1Level")->load());
    smoothedVco2Level.setTargetValue(apvts.getRawParameterValue("vco2Level")->load());
    smoothedPreDrive.setTargetValue(preDriveVal);
    smoothedPostDrive.setTargetValue(postDriveVal);

    filter.setResonance(resVal);
    voice.setVco1BaseFreq(vco1Freq);
    voice.setVco2BaseFreq(vco2Freq);
    voice.setSeqPitchRouting(seqPitchRouting);
    voice.setHardSync(apvts.getRawParameterValue("hardSync")->load() > 0.5f);
    voice.setVco1Wave((int)apvts.getRawParameterValue("vco1Wave")->load());
    voice.setVco2Wave((int)apvts.getRawParameterValue("vco2Wave")->load());
    filter.setHighpass((int)apvts.getRawParameterValue("vcfMode")->load() == 1);
    voice.setVco1EgAmount(vco1EgAmt);
    voice.setVco2EgAmount(vco2EgAmt);
    voice.setVcfEgAmount(vcfEgAmt);
    voice.setVcaEgAmount(vcaEgVal);
    voice.setVcaAttackTime(vcaAttackVal);
    voice.setClickDecay(clickDecayVal);
    voice.setClickLevel(clickLevelVal);
    voice.setVcoEgShape(vcoEgShapeVal);
    voice.setNoiseColor(noiseColorVal);
    voice.setNoiseDecayTime(noiseDecayVal);
    voice.setNoiseBypassVcf(noiseVcfBypass);
    voice.setClickBypassVcf(clickVcfBypass);
    voice.setVelVcfDecaySens(velVcfDecaySensVal);

    for (int i = 0; i < XTSequencer::numSteps; ++i)
    {
        sequencer.setStep(i,
            apvts.getRawParameterValue(makeStepParameterId("stepPitch", i))->load(),
            apvts.getRawParameterValue(makeStepParameterId("stepVel",   i))->load(),
            apvts.getRawParameterValue(makeStepParameterId("stepModA",  i))->load(),
            apvts.getRawParameterValue(makeStepParameterId("stepModB",  i))->load());
    }

    auto* left  = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        // --- Sequencer advance ---
        if (isPlaying)
        {
            double samplePpq;
            if (!isPlaying || intRunning) {
                // internal transport: advance internalPpq
                samplePpq = internalPpq;
                internalPpq += (double)bpm / (60.0 * currentSampleRate);
            } else {
                samplePpq = ppqPosition + (double)i * bpm / (60.0 * currentSampleRate);
            }

            int hostStep = (int)(samplePpq / ppqPerStep) % stepCount;

            if (sequencerResetPending.exchange(false, std::memory_order_acq_rel))
            {
                sequencerStepOffset = (stepCount - hostStep) % stepCount;
                lastStep = -1;
                if (intRunning) internalPpq = 0.0;
            }

            int currentStep = (hostStep + sequencerStepOffset) % stepCount + pageOffset;

            if (currentStep != lastStep)
            {
                lastStep = currentStep;
                sequencer.setCurrentStep(currentStep);
                const auto& step = sequencer.getStep(currentStep);
                currentVelocity = step.velocity;
                currentPitch    = (step.pitch - 60.0f) / 60.0f;
                currentModA     = step.modA * 2.0f - 1.0f;   // BI: 0-1 → -1..+1
                currentModB     = step.modB * 2.0f - 1.0f;

                if (lfoRetrigVal) lfoPhase = 0.0f;

                bool stepIsActive = apvts.getRawParameterValue(
                    makeStepParameterId("stepActive", currentStep))->load() > 0.5f;
                if (stepIsActive)
                    voice.trigger(step.pitch, step.velocity);
            }
        }
        else
        {
            if (sequencerResetPending.exchange(false, std::memory_order_acq_rel))
            {
                sequencerStepOffset = 0;
                sequencer.setCurrentStep(0);
                internalPpq = 0.0;
            }
            lastStep = -1;
        }

        voice.setVco1Level(smoothedVco1Level.getNextValue());
        voice.setVco2Level(smoothedVco2Level.getNextValue());

        auto frame = voice.processFrame();
        float cutoffNow = smoothedCutoff.getNextValue();
        float volumeNow = smoothedVolume.getNextValue();

        float directCutoffMod    = 0.0f;
        float directResonanceMod = 0.0f;
        float directFmMod        = 0.0f;
        float directNoiseMod     = 0.0f;
        float directVcfDecayMod  = 0.0f;
        float directVcaDecayMod  = 0.0f;
        float directVcoDecayMod  = 0.0f;
        float directVolumeMod    = 0.0f;
        float directClickTuneMod = 0.0f;
        float directPreDriveMod  = 0.0f;

        auto applyMod = [&](float signal, XTModDestination dest, float amount)
        {
            const float c = signal * amount;
            switch (dest) {
                case XTModDestination::Off:        break;
                case XTModDestination::Cutoff:     directCutoffMod    += c; break;
                case XTModDestination::Resonance:  directResonanceMod += c; break;
                case XTModDestination::FmAmount:   directFmMod        += c; break;
                case XTModDestination::NoiseLevel: directNoiseMod     += c; break;
                case XTModDestination::VcfDecay:   directVcfDecayMod  += c; break;
                case XTModDestination::VcaDecay:   directVcaDecayMod  += c; break;
                case XTModDestination::VcoDecay:   directVcoDecayMod  += c; break;
                case XTModDestination::Volume:     directVolumeMod    += c; break;
                case XTModDestination::ClickTune:  directClickTuneMod += c; break;
                case XTModDestination::PreDrive:   directPreDriveMod  += c; break;
                case XTModDestination::Count:      break;
            }
        };

        applyMod(currentModA, modDestinations[0], 1.0f);
        applyMod(currentModB, modDestinations[1], 1.0f);

        // --- LFO ---
        float lfoIncrement = lfoSyncVal
            ? lfoRateVal * bpm / (60.0f * (float)currentSampleRate)
            : lfoRateVal / (float)currentSampleRate;

        lfoPhase += lfoIncrement;
        if (lfoPhase >= 1.0f) {
            lfoPhase -= 1.0f;
            if (lfoWaveVal == 3) lfoValue = lfoRandom.nextFloat() * 2.0f - 1.0f; // RND update on wrap
        }

        switch (lfoWaveVal) {
            case 0: lfoValue = 1.0f - 4.0f * std::abs(lfoPhase - 0.5f); break; // TRI
            case 1: lfoValue = 2.0f * lfoPhase - 1.0f;                   break; // SAW
            case 2: lfoValue = lfoPhase < 0.5f ? 1.0f : -1.0f;          break; // SQR
            default: break;                                                       // RND: held above
        }
        applyMod(lfoValue, lfoDestVal, lfoAmtVal * 0.5f);

        voice.setClickTune(juce::jlimit(20.0f, 8000.0f,
            clickTuneVal * std::pow(2.0f, directClickTuneMod * 2.0f)));

        cutoffNow = juce::jlimit(20.0f, 20000.0f,
                        cutoffNow * std::pow(2.0f, directCutoffMod * 2.0f));
        volumeNow = juce::jlimit(0.0f, 1.0f, volumeNow + directVolumeMod * 0.5f);
        const float resonanceNow = juce::jlimit(0.0f, 1.0f, resVal + directResonanceMod * 0.5f);
        filter.setResonance(resonanceNow);

        // Pitch → FM coupling
        float effectiveFm = juce::jlimit(0.0f, 1.0f,
            fmVal + directFmMod + currentPitch * pitchFmAmtVal * 0.5f);
        voice.setFmAmount(effectiveFm);

        voice.setNoiseLevel(juce::jlimit(0.0f, 1.0f, noiseLevelVal + directNoiseMod));

        // VCF decay with vel sensitivity and mod
        if (vcfDecayParam != nullptr) {
            float norm = vcfDecayParam->convertTo0to1(vcfDecayVal);
            norm = juce::jlimit(0.0f, 1.0f, norm + directVcfDecayMod * 0.5f);
            voice.setVcfDecayTime(vcfDecayParam->convertFrom0to1(norm));
        } else {
            voice.setVcfDecayTime(vcfDecayVal);
        }

        // VCA decay
        if (vcaDecayParam != nullptr) {
            float norm = vcaDecayParam->convertTo0to1(vcaDecayVal);
            norm = juce::jlimit(0.0f, 1.0f, norm + directVcaDecayMod * 0.5f);
            voice.setVcaDecayTime(vcaDecayParam->convertFrom0to1(norm));
        } else {
            voice.setVcaDecayTime(vcaDecayVal);
        }

        // VCO1 decay
        if (vcoDecayParam != nullptr) {
            float norm = vcoDecayParam->convertTo0to1(vcoDecayVal);
            norm = juce::jlimit(0.0f, 1.0f, norm + directVcoDecayMod * 0.5f);
            voice.setDecayTime(vcoDecayParam->convertFrom0to1(norm));
        } else {
            voice.setDecayTime(vcoDecayVal);
        }

        // VCO2 independent decay (no mod routing, just the param)
        voice.setVco2DecayTime(vco2DecayVal);

        float shapedVcfEgAmt = (vcfEgAmt >= 0.0f)
            ? (vcfEgAmt * vcfEgAmt) : -(vcfEgAmt * vcfEgAmt);
        float vcfEgHz = (shapedVcfEgAmt >= 0.0f)
            ? shapedVcfEgAmt * frame.vcfEnv * 8500.0f
            : shapedVcfEgAmt * frame.vcfEnv * (cutoffNow - 20.0f);

        float noisedCutoff    = cutoffNow * std::pow(2.0f, noiseVcfMod * frame.noiseRaw * 2.0f);
        float modulatedCutoff = juce::jlimit(20.0f, 20000.0f, noisedCutoff + vcfEgHz);
        filter.setCutoff(modulatedCutoff);

        float preDriveNow  = juce::jlimit(1.0f, 10.0f,
                                smoothedPreDrive.getNextValue() + directPreDriveMod * 4.5f);
        float postDriveNow = smoothedPostDrive.getNextValue();
        float preGain      = preTrimVal * preDriveNow;
        float preDriven    = preGain > 1.01f
            ? std::tanh(frame.raw * preGain) / std::tanh(preGain)
            : frame.raw * preGain;
        float filtered  = filter.process(preDriven);
        // Bypass paths join here — skip VCF but still go through VCA + volume
        float combined  = filtered + frame.noiseOut + frame.clickOut;
        float postInput = combined * frame.ampGain * volumeNow;
        float sample    = postDriveNow > 1.01f
            ? std::tanh(postInput * postDriveNow) / std::tanh(postDriveNow)
            : postInput;

        left[i]  = sample;
        right[i] = sample;
    }
}

juce::AudioProcessorEditor* XTProcessor::createEditor() { return new XTEditor(*this); }

void XTProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto xml = createStateXml();
    copyXmlToBinary(*xml, destData);
}

void XTProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml) restoreStateFromXml(*xml, {});
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new XTProcessor(); }
