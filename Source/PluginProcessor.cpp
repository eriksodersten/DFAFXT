#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr auto kInitPresetName = "Init";
constexpr auto kPresetExtension = ".dfafxtpreset";
constexpr auto kPresetNameAttribute = "currentPresetName";
constexpr int kNumModRoutes = 3;

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

float centredSequencerMod(float value)
{
    return juce::jlimit(-1.0f, 1.0f, value * 2.0f - 1.0f);
}

XTModDestination choiceToModDestination(int rawChoice)
{
    const int clamped = juce::jlimit(0, (int) XTModDestination::Count - 1, rawChoice);
    return static_cast<XTModDestination>(clamped);
}
}

juce::StringArray XTProcessor::getModDestinationNames()
{
    return {
        "OFF",
        "CUTOFF",
        "RESONANCE",
        "FM AMT",
        "NOISE",
        "VCF DEC",
        "VCA DEC",
        "VCO DEC",
        "VOLUME"
    };
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

    params.push_back(std::make_unique<juce::AudioParameterFloat>("vcoDecay",    "VCO Decay",
            vcoDecayRange, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vco1Freq",    "VCO 1 Freq",     1.0f, 2000.0f, 220.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vco1EgAmt",   "VCO 1 EG Amt", -60.0f, 60.0f,   0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("fmAmount",    "FM Amount",     0.0f,  1.0f,    0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vco2Freq",    "VCO 2 Freq",     1.0f, 2000.0f, 330.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vco2EgAmt",   "VCO 2 EG Amt", -60.0f, 60.0f,   0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("noiseLevel",  "Noise Level",   0.0f,  1.0f,    0.2f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("vco1Level",   "VCO 1 Level",   0.0f,  1.0f,    0.6f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("vco2Level",   "VCO 2 Level",   0.0f,  1.0f,    0.2f));
    auto cutoffRange = juce::NormalisableRange<float>(20.0f, 8000.0f);
    cutoffRange.setSkewForCentre(800.0f);

    params.push_back(std::make_unique<juce::AudioParameterFloat>("cutoff",      "Cutoff",        cutoffRange, 800.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("resonance",   "Resonance",     0.0f,  1.0f,    0.4f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vcfDecay",    "VCF Decay",
                vcfDecayRange, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vcfEgAmt",    "VCF EG Amt",   -1.0f,  1.0f,    0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("noiseVcfMod", "Noise VCF Mod", 0.0f,  1.0f,    0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vcaDecay",    "VCA Decay",
            vcaDecayRange, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vcaEg",       "VCA EG",        0.0f,  1.0f,    0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("preTrim",     "Pre Trim",      0.1f,  2.0f,    0.84f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("volume",      "Volume",        0.0f,  1.0f,    0.8f));
    for (int i = 0; i < kNumModRoutes; ++i)
    {
        const auto sourceName = "Mod " + juce::String::charToString((juce_wchar) ('A' + i));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            makeModDestinationParameterId(i), sourceName + " Destination", getModDestinationNames(), 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            makeModAmountParameterId(i), sourceName + " Amount", 0.0f, 1.0f, 0.5f));
    }
    params.push_back(std::make_unique<juce::AudioParameterChoice>("clockMult", "Clock Multiplier",
                juce::StringArray({ "1/8", "1/5", "1/4", "1/3", "1/2", "1x", "2x", "3x", "4x", "5x" }), 5));
    for (int i = 0; i < XTSequencer::numSteps; ++i)
    {
        const auto stepNumber = juce::String(i + 1);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            makeStepParameterId("stepPitch", i), "Step Pitch " + stepNumber, 0.0f, 120.0f, 60.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            makeStepParameterId("stepVel", i), "Step Velocity " + stepNumber, 0.0f, 1.0f, 0.8f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            makeStepParameterId("stepModA", i), "Step Mod A " + stepNumber, 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            makeStepParameterId("stepModB", i), "Step Mod B " + stepNumber, 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            makeStepParameterId("stepModC", i), "Step Mod C " + stepNumber, 0.0f, 1.0f, 0.5f));
    }
    params.push_back(std::make_unique<juce::AudioParameterChoice>("seqPitchMod", "SEQ Pitch Mod",
        juce::StringArray({ "VCO 1&2", "OFF", "VCO 2" }), 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>("hardSync", "Hard Sync", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("vco1Wave", "VCO 1 Wave",
            juce::StringArray({ "Square", "Triangle" }), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("vco2Wave", "VCO 2 Wave",
                juce::StringArray({ "Square", "Triangle" }), 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>("vcfMode", "VCF Mode",
                juce::StringArray({ "LP", "HP" }), 0));

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
        .getChildFile("DFAFXT")
        .getChildFile("Presets");
}

juce::File XTProcessor::getPresetFile(const juce::String& presetName) const
{
    return getPresetDirectory().getChildFile(sanitisePresetName(presetName) + kPresetExtension);
}

juce::String XTProcessor::sanitisePresetName(const juce::String& presetName) const
{
    juce::String safeName;

    for (auto character : presetName.trim())
    {
        if (juce::CharacterFunctions::isLetterOrDigit(character)
            || character == ' '
            || character == '-'
            || character == '_')
        {
            safeName << character;
        }
    }

    return safeName.trim();
}

juce::StringArray XTProcessor::getAvailablePresetNames() const
{
    juce::StringArray names;
    auto presetDirectory = getPresetDirectory();

    if (! presetDirectory.isDirectory())
        return names;

    juce::Array<juce::File> presetFiles;
    presetDirectory.findChildFiles(presetFiles, juce::File::findFiles, false, "*" + juce::String(kPresetExtension));

    for (const auto& presetFile : presetFiles)
        names.addIfNotAlreadyThere(presetFile.getFileNameWithoutExtension());

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

    auto* cablesXml = xml->createNewChildElement("PatchCables");
    {
        const juce::ScopedLock sl(cableWriteLock);
        for (int i = 0; i < cableStore.count; ++i)
        {
            const auto& c = cableStore.data[i];
            auto* el = cablesXml->createNewChildElement("Cable");
            el->setAttribute("src",     (int)c.src);
            el->setAttribute("dst",     (int)c.dst);
            el->setAttribute("amount",  c.amount);
            el->setAttribute("enabled", c.enabled ? 1 : 0);
        }
    }

    return xml;
}

void XTProcessor::restoreStateFromXml(const juce::XmlElement& xml, const juce::String& presetNameOverride)
{
    auto stateXml = std::make_unique<juce::XmlElement>(xml);

    if (auto* cablesXml = stateXml->getChildByName("PatchCables"))
        stateXml->removeChildElement(cablesXml, true);

    if (stateXml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*stateXml));

    CableStore next {};
    if (auto* cablesXml = xml.getChildByName("PatchCables"))
    {
        for (auto* el : cablesXml->getChildIterator())
        {
            const int src = el->getIntAttribute("src", -1);
            const int dst = el->getIntAttribute("dst", -1);
            if (src < 0 || src >= PP_NUM_POINTS) continue;
            if (dst < 0 || dst >= PP_NUM_POINTS) continue;
            if (kPatchMeta[src].dir != PD_Out)   continue;
            if (kPatchMeta[dst].dir != PD_In)    continue;
            if (next.count >= kMaxCables)        break;

            next.data[next.count++] = {
                static_cast<PatchPoint>(src),
                static_cast<PatchPoint>(dst),
                (float)el->getDoubleAttribute("amount",  1.0),
                el->getIntAttribute("enabled", 1) != 0
            };
        }
    }

    {
        const juce::ScopedLock sl(cableWriteLock);
        cableSeq.fetch_add(1, std::memory_order_release);
        cableStore = next;
        cableSeq.fetch_add(1, std::memory_order_release);
    }

    const auto restoredPresetName = presetNameOverride.isNotEmpty()
        ? presetNameOverride
        : xml.getStringAttribute(kPresetNameAttribute, kInitPresetName);

    {
        const juce::ScopedLock sl(presetLock);
        currentPresetName = restoredPresetName;
    }
}

bool XTProcessor::savePreset(const juce::String& presetName)
{
    const auto safeName = sanitisePresetName(presetName);
    if (safeName.isEmpty())
        return false;

    auto presetDirectory = getPresetDirectory();
    if (! presetDirectory.isDirectory() && ! presetDirectory.createDirectory())
        return false;

    auto xml = createStateXml();
    xml->setAttribute(kPresetNameAttribute, safeName);

    if (! getPresetFile(safeName).replaceWithText(xml->toString()))
        return false;

    {
        const juce::ScopedLock sl(presetLock);
        currentPresetName = safeName;
    }

    updateHostDisplay();
    return true;
}

bool XTProcessor::saveCurrentPreset()
{
    const auto presetName = getCurrentPresetName();
    if (presetName == kInitPresetName)
        return false;

    return savePreset(presetName);
}

bool XTProcessor::loadPreset(const juce::String& presetName)
{
    const auto safeName = sanitisePresetName(presetName);
    if (safeName.isEmpty())
        return false;

    auto presetFile = getPresetFile(safeName);
    if (! presetFile.existsAsFile())
        return false;

    auto xml = juce::XmlDocument::parse(presetFile);
    if (xml == nullptr)
        return false;

    restoreStateFromXml(*xml, safeName);
    updateHostDisplay();
    return true;
}

bool XTProcessor::deletePreset(const juce::String& presetName)
{
    const auto safeName = sanitisePresetName(presetName);
    if (safeName.isEmpty() || safeName == kInitPresetName)
        return false;

    auto presetFile = getPresetFile(safeName);
    if (! presetFile.existsAsFile())
        return false;

    if (! presetFile.deleteFile())
        return false;

    if (getCurrentPresetName() == safeName)
        loadInitPreset();

    updateHostDisplay();
    return true;
}

void XTProcessor::loadInitPreset()
{
    apvts.replaceState(defaultState.createCopy());
    clearPatches();

    {
        const juce::ScopedLock sl(presetLock);
        currentPresetName = kInitPresetName;
    }

    updateHostDisplay();
}

int XTProcessor::getNumPrograms()
{
    return getAvailablePresetNames().size() + 1;
}

int XTProcessor::getCurrentProgram()
{
    const auto currentPreset = getCurrentPresetName();
    if (currentPreset == kInitPresetName)
        return 0;

    const auto presetNames = getAvailablePresetNames();
    const int presetIndex = presetNames.indexOf(currentPreset);
    return presetIndex >= 0 ? presetIndex + 1 : 0;
}

void XTProcessor::setCurrentProgram(int index)
{
    if (index <= 0)
    {
        loadInitPreset();
        return;
    }

    const auto presetNames = getAvailablePresetNames();
    if (juce::isPositiveAndBelow(index - 1, presetNames.size()))
        loadPreset(presetNames[index - 1]);
}

const juce::String XTProcessor::getProgramName(int index)
{
    if (index == 0)
        return kInitPresetName;

    const auto presetNames = getAvailablePresetNames();
    return juce::isPositiveAndBelow(index - 1, presetNames.size()) ? presetNames[index - 1]
                                                                    : juce::String {};
}

void XTProcessor::changeProgramName(int index, const juce::String& newName)
{
    if (index <= 0)
        return;

    const auto presetNames = getAvailablePresetNames();
    if (! juce::isPositiveAndBelow(index - 1, presetNames.size()))
        return;

    const auto oldName = presetNames[index - 1];
    const auto safeNewName = sanitisePresetName(newName);
    if (safeNewName.isEmpty() || safeNewName == oldName)
        return;

    auto oldFile = getPresetFile(oldName);
    auto newFile = getPresetFile(safeNewName);
    if (newFile.existsAsFile())
        return;

    if (oldFile.moveFileTo(newFile) && getCurrentPresetName() == oldName)
    {
        const juce::ScopedLock sl(presetLock);
        currentPresetName = safeNewName;
    }

    updateHostDisplay();
}

void XTProcessor::initialiseMidiCcBindings()
{
    midiCcBindings.fill({});

    size_t bindingIndex = 0;
    auto addBinding = [this, &bindingIndex](int cc, const juce::String& parameterId)
    {
        jassert(bindingIndex < midiCcBindings.size());
        auto& binding = midiCcBindings[bindingIndex++];
        binding.cc = cc;
        binding.parameter = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(parameterId));
        jassert(binding.parameter != nullptr);
    };

    addBinding(20, "vcoDecay");
    addBinding(21, "vco1EgAmt");
    addBinding(22, "vco1Freq");
    addBinding(23, "vco1Level");
    addBinding(24, "noiseLevel");
    addBinding(25, "cutoff");
    addBinding(26, "resonance");
    addBinding(27, "vcaEg");
    addBinding(28, "volume");
    addBinding(29, "fmAmount");
    addBinding(30, "vco2EgAmt");
    addBinding(31, "vco2Freq");
    addBinding(32, "vco2Level");
    addBinding(33, "vcfDecay");
    addBinding(34, "vcfEgAmt");
    addBinding(35, "noiseVcfMod");
    addBinding(36, "vcaDecay");

    int cc = 37;
    for (int i = 0; i < XTSequencer::numSteps; ++i) addBinding(cc++, makeStepParameterId("stepPitch", i));
    for (int i = 0; i < XTSequencer::numSteps; ++i) addBinding(cc++, makeStepParameterId("stepVel",   i));
    for (int i = 0; i < XTSequencer::numSteps; ++i) addBinding(cc++, makeStepParameterId("stepModA",  i));
    for (int i = 0; i < XTSequencer::numSteps; ++i) addBinding(cc++, makeStepParameterId("stepModB",  i));
    for (int i = 0; i < XTSequencer::numSteps; ++i) addBinding(cc++, makeStepParameterId("stepModC",  i));

    addBinding(cc++, "seqPitchMod");
    addBinding(cc++, "hardSync");
    addBinding(cc++, "vco1Wave");
    addBinding(cc++, "vco2Wave");
    addBinding(cc++, "vcfMode");
    addBinding(cc++, "clockMult");

    jassert(bindingIndex == midiCcBindings.size());
}

void XTProcessor::applyMidiCc(int ccNumber, int ccValue)
{
    const float normalisedCc = juce::jlimit(0.0f, 1.0f, (float)ccValue / 127.0f);

    for (const auto& binding : midiCcBindings)
    {
        if (binding.cc != ccNumber || binding.parameter == nullptr)
            continue;

        if (dynamic_cast<juce::AudioParameterChoice*>(binding.parameter) != nullptr
            || dynamic_cast<juce::AudioParameterBool*>(binding.parameter) != nullptr)
        {
            const int numSteps = juce::jmax(2, binding.parameter->getNumSteps());
            const int stepIndex = juce::jlimit(0, numSteps - 1,
                (int)std::round(normalisedCc * (float)(numSteps - 1)));
            const float quantisedValue = (float)stepIndex / (float)(numSteps - 1);
            binding.parameter->setValueNotifyingHost(quantisedValue);
            return;
        }

        binding.parameter->setValueNotifyingHost(normalisedCc);
        return;
    }
}

void XTProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate;
    sequencer.prepare(sampleRate);
    voice.prepare(sampleRate);
    filter.prepare(sampleRate);
    filter.setCutoff(800.0f);
    filter.setResonance(0.4f);
    for (int p = 0; p < PP_NUM_POINTS; ++p)
        patchSourceValues[p] = patchInputSums[p] = 0.0f;
    vcfDecayParam = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter("vcfDecay"));
    vcaDecayParam = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter("vcaDecay"));
    vcoDecayParam = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter("vcoDecay"));
    smoothedCutoff.reset(sampleRate, 0.01);
    smoothedVolume.reset(sampleRate, 0.01);
    smoothedVco1Level.reset(sampleRate, 0.01);
    smoothedVco2Level.reset(sampleRate, 0.01);
    smoothedCutoff.setCurrentAndTargetValue(apvts.getRawParameterValue("cutoff")->load());
    smoothedVolume.setCurrentAndTargetValue(apvts.getRawParameterValue("volume")->load());
    smoothedVco1Level.setCurrentAndTargetValue(apvts.getRawParameterValue("vco1Level")->load());
    smoothedVco2Level.setCurrentAndTargetValue(apvts.getRawParameterValue("vco2Level")->load());
}
void XTProcessor::releaseResources() {}

// =============================================================================
// Patch system API  (message thread – never called from audio thread)
// =============================================================================

// Helper: read store safely from message thread (writer lock already held or not needed
// because writer lock serialises all writes).
// We take writelock here so getCableSnapshot is also safe without holding it externally.


void XTProcessor::connectPatch(PatchPoint src, PatchPoint dst, float amount)
{
    jassert(kPatchMeta[src].dir == PD_Out);
    jassert(kPatchMeta[dst].dir == PD_In);
    const juce::ScopedLock sl(cableWriteLock);

    CableStore next = cableStore;   // start from current state
    for (int i = 0; i < next.count; ++i)
        if (next.data[i].src == src && next.data[i].dst == dst)
            { next.data[i].amount = amount; next.data[i].enabled = true; goto publish; }
    if (next.count < kMaxCables)
        next.data[next.count++] = { src, dst, amount, true };

publish:
    cableSeq.fetch_add(1, std::memory_order_release);   // odd: writing
    cableStore = next;
    cableSeq.fetch_add(1, std::memory_order_release);   // even: done
}

void XTProcessor::disconnectPatch(PatchPoint src, PatchPoint dst)
{
    const juce::ScopedLock sl(cableWriteLock);
    CableStore next = cableStore;
    int w = 0;
    for (int r = 0; r < next.count; ++r)
        if (!(next.data[r].src == src && next.data[r].dst == dst))
            next.data[w++] = next.data[r];
    next.count = w;

    cableSeq.fetch_add(1, std::memory_order_release);
    cableStore = next;
    cableSeq.fetch_add(1, std::memory_order_release);
}

void XTProcessor::clearPatches()
{
    const juce::ScopedLock sl(cableWriteLock);
    CableStore empty{};
    cableSeq.fetch_add(1, std::memory_order_release);
    cableStore = empty;
    cableSeq.fetch_add(1, std::memory_order_release);
}

void XTProcessor::getCableSnapshot(std::vector<PatchCable>& out) const
{
    // Message-thread read: take write lock so we never read a partial write
    const juce::ScopedLock sl(cableWriteLock);
    out.assign(cableStore.data, cableStore.data + cableStore.count);
}

// =============================================================================

void XTProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    for (const auto metadata : midiMessages)
    {
        const auto& message = metadata.getMessage();
        if (message.isController())
            applyMidiCc(message.getControllerNumber(), message.getControllerValue());
    }

    float bpm = 120.0f;
        bool isPlaying = false;
        double ppqPosition = 0.0;
    if (auto* ph = getPlayHead())
            {
                if (auto pos = ph->getPosition())
                {
                    if (pos->getBpm().hasValue())
                        bpm = (float)*pos->getBpm();
                    isPlaying = pos->getIsPlaying();
                    if (pos->getPpqPosition().hasValue())
                        ppqPosition = *pos->getPpqPosition();
                }
            }

    int multIndex = (int)apvts.getRawParameterValue("clockMult")->load();
            const float multTable[] = { 8.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.5f, 1.0f/3.0f, 0.25f, 0.2f };
            float mult = multTable[multIndex];
            float ppqPerStep = 0.25f * mult;
    float cutoffVal   = apvts.getRawParameterValue("cutoff")->load();
    float resVal      = apvts.getRawParameterValue("resonance")->load();
    float volumeVal   = apvts.getRawParameterValue("volume")->load();
    smoothedCutoff.setTargetValue(cutoffVal);
    smoothedVolume.setTargetValue(volumeVal);
    smoothedVco1Level.setTargetValue(apvts.getRawParameterValue("vco1Level")->load());
    smoothedVco2Level.setTargetValue(apvts.getRawParameterValue("vco2Level")->load());
    float vcoDecayVal = apvts.getRawParameterValue("vcoDecay")->load();
    float vcaDecayVal = apvts.getRawParameterValue("vcaDecay")->load();
    float vcfDecayVal = apvts.getRawParameterValue("vcfDecay")->load();
    float fmVal       = apvts.getRawParameterValue("fmAmount")->load();
    std::array<XTModDestination, kNumModRoutes> modDestinations;
    std::array<float, kNumModRoutes> modAmounts {};
    for (int i = 0; i < kNumModRoutes; ++i)
    {
        modDestinations[(size_t) i] = choiceToModDestination((int) apvts.getRawParameterValue(makeModDestinationParameterId(i))->load());
        modAmounts[(size_t) i] = apvts.getRawParameterValue(makeModAmountParameterId(i))->load();
    }
    float vco1Freq       = apvts.getRawParameterValue("vco1Freq")->load();
        float vco2Freq       = apvts.getRawParameterValue("vco2Freq")->load();
        int   seqPitchRouting = (int)apvts.getRawParameterValue("seqPitchMod")->load();
        float vco1EgAmt      = apvts.getRawParameterValue("vco1EgAmt")->load();
    float vco2EgAmt   = apvts.getRawParameterValue("vco2EgAmt")->load();
    float vcfEgAmt      = apvts.getRawParameterValue("vcfEgAmt")->load();
        float noiseVcfMod   = apvts.getRawParameterValue("noiseVcfMod")->load();    float noiseLevelVal = apvts.getRawParameterValue("noiseLevel")->load();
    float vcaEgVal      = apvts.getRawParameterValue("vcaEg")->load();
        float preTrimVal    = apvts.getRawParameterValue("preTrim")->load();

    for (int i = 0; i < XTSequencer::numSteps; ++i)
    {
        const float pitch = apvts.getRawParameterValue(makeStepParameterId("stepPitch", i))->load();
        const float vel   = apvts.getRawParameterValue(makeStepParameterId("stepVel",   i))->load();
        const float modA  = apvts.getRawParameterValue(makeStepParameterId("stepModA",  i))->load();
        const float modB  = apvts.getRawParameterValue(makeStepParameterId("stepModB",  i))->load();
        const float modC  = apvts.getRawParameterValue(makeStepParameterId("stepModC",  i))->load();
        sequencer.setStep(i, pitch, vel, modA, modB, modC);
    }

        filter.setResonance(resVal);
    // VCO/VCA/VCF decay and FM amount applied per-sample after cable snapshot – see below
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
    // noise level applied per-sample after cable snapshot – see below
    voice.setVcaEgAmount(vcaEgVal);
    voice.setVcaAttackTime(0.001f + vcaEgVal * 0.099f);

    // Seqlock snapshot – audio thread never takes a lock.
    // Retries only if a message-thread write lands exactly during the copy (extremely rare).
    CableStore cableSnap;
    uint32_t seq1, seq2 = 0;
    do {
        seq1 = cableSeq.load(std::memory_order_acquire);
        if (seq1 & 1u) continue;          // write in progress – retry
        cableSnap = cableStore;
        seq2 = cableSeq.load(std::memory_order_acquire);
    } while (seq1 != seq2);
    const int nCables = cableSnap.count;

    // Pre-compute which destinations have active cables (used in DSP below)
    bool hasVcfModCable   = false;
    bool hasVcfDecayCable = false;
    bool hasVcaDecayCable = false;
    bool hasVcoDecayCable = false;
    bool hasFmAmtCable    = false;
    bool hasNoiseLvlCable = false;
    for (int c = 0; c < nCables; ++c)
    {
        if (!cableSnap.data[c].enabled) continue;
        if (cableSnap.data[c].dst == PP_VCF_MOD)   hasVcfModCable   = true;
        if (cableSnap.data[c].dst == PP_VCF_DECAY)  hasVcfDecayCable = true;
        if (cableSnap.data[c].dst == PP_VCA_DECAY)  hasVcaDecayCable = true;
        if (cableSnap.data[c].dst == PP_VCO_DECAY)  hasVcoDecayCable = true;
        if (cableSnap.data[c].dst == PP_FM_AMT)     hasFmAmtCable    = true;
        if (cableSnap.data[c].dst == PP_NOISE_LVL)  hasNoiseLvlCable = true;
    }

    auto* left  = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        if (isPlaying)
        {
            double samplePpq = ppqPosition + (double)i * bpm / (60.0 * currentSampleRate);
            int hostStep = (int)(samplePpq / ppqPerStep) % XTSequencer::numSteps;

            if (sequencerResetPending.exchange(false, std::memory_order_acq_rel))
            {
                sequencerStepOffset = (XTSequencer::numSteps - hostStep) % XTSequencer::numSteps;
                lastStep = -1;
            }

            int currentStep = (hostStep + sequencerStepOffset) % XTSequencer::numSteps;

            if (currentStep != lastStep)
            {
                lastStep = currentStep;
                sequencer.setCurrentStep(currentStep);
                const auto& step = sequencer.getStep(currentStep);
                currentVelocity = step.velocity;
                currentPitch    = (step.pitch - 60.0f) / 60.0f;
                currentModA     = centredSequencerMod(step.modA);
                currentModB     = centredSequencerMod(step.modB);
                currentModC     = centredSequencerMod(step.modC);
                patchSourceValues[PP_VELOCITY] = currentVelocity;
                patchSourceValues[PP_PITCH]    = currentPitch;
                patchSourceValues[PP_MOD_A]    = currentModA;
                patchSourceValues[PP_MOD_B]    = currentModB;
                patchSourceValues[PP_MOD_C]    = currentModC;
                voice.trigger(step.pitch, step.velocity);
            }
        }
        else
        {
            if (sequencerResetPending.exchange(false, std::memory_order_acq_rel))
            {
                sequencerStepOffset = 0;
                sequencer.setCurrentStep(0);
            }

            lastStep = -1;
        }

        voice.setVco1Level(smoothedVco1Level.getNextValue());
        voice.setVco2Level(smoothedVco2Level.getNextValue());
        auto frame = voice.processFrame();
        float cutoffNow = smoothedCutoff.getNextValue();
        float volumeNow = smoothedVolume.getNextValue();
        float directCutoffMod = 0.0f;
        float directResonanceMod = 0.0f;
        float directFmMod = 0.0f;
        float directNoiseMod = 0.0f;
        float directVcfDecayMod = 0.0f;
        float directVcaDecayMod = 0.0f;
        float directVcoDecayMod = 0.0f;
        float directVolumeMod = 0.0f;

        auto applySequencerMod = [&](float signal, XTModDestination destination, float amount)
        {
            const float contribution = signal * amount;

            switch (destination)
            {
                case XTModDestination::Off:       break;
                case XTModDestination::Cutoff:    directCutoffMod    += contribution; break;
                case XTModDestination::Resonance: directResonanceMod += contribution; break;
                case XTModDestination::FmAmount:  directFmMod        += contribution; break;
                case XTModDestination::NoiseLevel:directNoiseMod     += contribution; break;
                case XTModDestination::VcfDecay:  directVcfDecayMod  += contribution; break;
                case XTModDestination::VcaDecay:  directVcaDecayMod  += contribution; break;
                case XTModDestination::VcoDecay:  directVcoDecayMod  += contribution; break;
                case XTModDestination::Volume:    directVolumeMod    += contribution; break;
                case XTModDestination::Count:     break;
            }
        };

        applySequencerMod(currentModA, modDestinations[0], modAmounts[0]);
        applySequencerMod(currentModB, modDestinations[1], modAmounts[1]);
        applySequencerMod(currentModC, modDestinations[2], modAmounts[2]);
        cutoffNow = juce::jlimit(20.0f, 20000.0f, cutoffNow * std::pow(2.0f, directCutoffMod * 2.0f));
        volumeNow = juce::jlimit(0.0f, 1.0f, volumeNow + directVolumeMod * 0.5f);
        const float resonanceNow = juce::jlimit(0.0f, 1.0f, resVal + directResonanceMod * 0.5f);
        filter.setResonance(resonanceNow);

        // --- Patch engine -------------------------------------------
        for (int p = 0; p < PP_NUM_POINTS; ++p) patchInputSums[p] = 0.0f;
        patchSourceValues[PP_VCF_EG]   = frame.vcfEnv;
        patchSourceValues[PP_VCA_EG]   = frame.ampGain;
        patchSourceValues[PP_VELOCITY] = currentVelocity;
        patchSourceValues[PP_PITCH]    = currentPitch;
        patchSourceValues[PP_MOD_A]    = currentModA;
        patchSourceValues[PP_MOD_B]    = currentModB;
        patchSourceValues[PP_MOD_C]    = currentModC;
        patchSourceValues[PP_VCO_EG]   = frame.vcoEnv;
        patchSourceValues[PP_VCO1]     = frame.vco1Raw;
        patchSourceValues[PP_VCO2]     = frame.vco2Raw;
        for (int c = 0; c < nCables; ++c)
            if (cableSnap.data[c].enabled)
                patchInputSums[cableSnap.data[c].dst] +=
                    patchSourceValues[cableSnap.data[c].src] * cableSnap.data[c].amount;
        // ------------------------------------------------------------

        // FM amount: additive CV, clamped 0..1
        voice.setFmAmount(hasFmAmtCable
            ? juce::jlimit(0.0f, 1.0f, fmVal + directFmMod + patchInputSums[PP_FM_AMT])
            : juce::jlimit(0.0f, 1.0f, fmVal + directFmMod));

        // Noise level: additive CV, clamped 0..1
        voice.setNoiseLevel(hasNoiseLvlCable
            ? juce::jlimit(0.0f, 1.0f, noiseLevelVal + directNoiseMod + patchInputSums[PP_NOISE_LVL])
            : juce::jlimit(0.0f, 1.0f, noiseLevelVal + directNoiseMod));

        // VCF decay: continuous CV from real patch sources in normalised parameter domain
        if (vcfDecayParam != nullptr)
        {
            float norm = vcfDecayParam->convertTo0to1(vcfDecayVal);
            norm = juce::jlimit(0.0f, 1.0f, norm + directVcfDecayMod * 0.5f);
            if (hasVcfDecayCable)
            {
                constexpr float vcfDecayCvScale = 0.35f;
                norm = juce::jlimit(0.0f, 1.0f, norm + patchInputSums[PP_VCF_DECAY] * vcfDecayCvScale);
            }
            voice.setVcfDecayTime(vcfDecayParam->convertFrom0to1(norm));
        }
        else
        {
            voice.setVcfDecayTime(vcfDecayVal);
        }

        // VCA decay: same additive normalised-domain model as VCF decay
        if (vcaDecayParam != nullptr)
        {
            float norm = vcaDecayParam->convertTo0to1(vcaDecayVal);
            norm = juce::jlimit(0.0f, 1.0f, norm + directVcaDecayMod * 0.5f);
            if (hasVcaDecayCable)
            {
                constexpr float vcaDecayCvScale = 0.35f;
                norm = juce::jlimit(0.0f, 1.0f, norm + patchInputSums[PP_VCA_DECAY] * vcaDecayCvScale);
            }
            voice.setVcaDecayTime(vcaDecayParam->convertFrom0to1(norm));
        }
        else
        {
            voice.setVcaDecayTime(vcaDecayVal);
        }

        // VCO decay: same additive normalised-domain model as VCF decay
        if (vcoDecayParam != nullptr)
        {
            float norm = vcoDecayParam->convertTo0to1(vcoDecayVal);
            norm = juce::jlimit(0.0f, 1.0f, norm + directVcoDecayMod * 0.5f);
            if (hasVcoDecayCable)
            {
                constexpr float vcoDecayCvScale = 0.35f;
                norm = juce::jlimit(0.0f, 1.0f, norm + patchInputSums[PP_VCO_DECAY] * vcoDecayCvScale);
            }
            voice.setDecayTime(vcoDecayParam->convertFrom0to1(norm));
        }
        else
        {
            voice.setDecayTime(vcoDecayVal);
        }

        float shapedVcfEgAmt = (vcfEgAmt >= 0.0f)
            ? (vcfEgAmt * vcfEgAmt)
            : -(vcfEgAmt * vcfEgAmt);

        float vcfEnvMod = frame.vcfEnv;
        float vcfEgHz = (shapedVcfEgAmt >= 0.0f)
            ? (shapedVcfEgAmt * vcfEnvMod * 8500.0f)
            : (shapedVcfEgAmt * vcfEnvMod * (cutoffNow - 20.0f));

        // VCF MOD: patched signal replaces noise as mod source;
        // noiseVcfMod knob sets depth for both paths.
        float vcfModSignal = hasVcfModCable
            ? juce::jlimit(-1.0f, 1.0f, patchInputSums[PP_VCF_MOD])
            : frame.noiseRaw;
        float noisedCutoff    = cutoffNow * std::pow(2.0f, noiseVcfMod * vcfModSignal * 2.0f);
        float modulatedCutoff = juce::jlimit(20.0f, 20000.0f, noisedCutoff + vcfEgHz);
        filter.setCutoff(modulatedCutoff);

        float vcaGain = juce::jlimit(0.0f, 1.0f, frame.ampGain + patchInputSums[PP_VCA_CV]);
        float sample  = filter.process(frame.raw * preTrimVal) * vcaGain * volumeNow;
        left[i]  = sample;
        right[i] = sample;
    }
}

juce::AudioProcessorEditor* XTProcessor::createEditor()
{
    return new XTEditor(*this);
}

void XTProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto xml = createStateXml();
    copyXmlToBinary(*xml, destData);
}

void XTProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml == nullptr)
        return;

    restoreStateFromXml(*xml, {});
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new XTProcessor();
}
