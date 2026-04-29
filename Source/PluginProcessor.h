#pragma once
#include <JuceHeader.h>
#include <array>
#include "DecayEnvelope.h"
#include "XTSequencer.h"
#include "XTVoice.h"
#include "MoogLadderFilter.h"

// =============================================================================
// Mod routing
// =============================================================================

enum class XTModDestination
{
    Off = 0,
    Cutoff,
    Resonance,
    FmAmount,
    NoiseLevel,
    VcfDecay,
    VcaDecay,
    VcoDecay,
    Volume,
    Count
};

// =============================================================================

class XTProcessor : public juce::AudioProcessor
{
public:
    XTProcessor();
    ~XTProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DFAF XT"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::StringArray getModDestinationNames();

    juce::StringArray getAvailablePresetNames() const;
    juce::String getCurrentPresetName() const;
    bool savePreset(const juce::String& presetName);
    bool saveCurrentPreset();
    bool loadPreset(const juce::String& presetName);
    bool deletePreset(const juce::String& presetName);
    void loadInitPreset();

    int getCurrentStep() const { return sequencer.getCurrentStep(); }
    void resetSequencer() { sequencerResetPending.store(true, std::memory_order_release); }
    void setPlayPage(int page);
    void copyPageAtoB();

    XTSequencer   sequencer;
    double currentSampleRate = 44100.0;

    // Sequencer clock state
    int  lastStep            = -1;
    int  sequencerStepOffset = 0;
    std::atomic<bool> sequencerResetPending { false };

    // Pattern pages (0 = A, 1 = B)
    std::atomic<int>  playPage { 0 };

    // Internal transport
    std::atomic<bool> internalTransportRunning { false };
    std::atomic<bool> triggerPending           { false };
    std::atomic<bool> advancePending           { false };
    double internalPpq = 0.0;   // audio thread only

    XTVoice          voice;
    MoogLadderFilter filter;
    float currentVelocity = 0.0f;
    float currentPitch    = 0.0f;
    float currentModA     = 0.0f;
    float currentModB     = 0.0f;
    float currentModC     = 0.0f;

    juce::RangedAudioParameter* vcfDecayParam = nullptr;
    juce::RangedAudioParameter* vcaDecayParam = nullptr;
    juce::RangedAudioParameter* vcoDecayParam = nullptr;

    juce::SmoothedValue<float> smoothedCutoff;
    juce::SmoothedValue<float> smoothedVolume;
    juce::SmoothedValue<float> smoothedVco1Level;
    juce::SmoothedValue<float> smoothedVco2Level;
    juce::SmoothedValue<float> smoothedPreDrive;
    juce::SmoothedValue<float> smoothedPostDrive;

    float lfoPhase = 0.0f;
    float lfoValue = 0.0f;
    juce::Random lfoRandom;

private:
    struct MidiCcBinding
    {
        int cc = -1;
        juce::RangedAudioParameter* parameter = nullptr;
    };

    juce::File getPresetDirectory() const;
    juce::File getPresetFile(const juce::String& presetName) const;
    juce::String sanitisePresetName(const juce::String& presetName) const;
    std::unique_ptr<juce::XmlElement> createStateXml();
    void restoreStateFromXml(const juce::XmlElement& xml, const juce::String& presetNameOverride);

    static constexpr size_t kNumMidiCcBindings = 145;
    void initialiseMidiCcBindings();
    void applyMidiCc(int ccNumber, int ccValue);

    std::array<MidiCcBinding, kNumMidiCcBindings> midiCcBindings {};
    juce::ValueTree defaultState;
    mutable juce::CriticalSection presetLock;
    juce::String currentPresetName { "Init" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XTProcessor)
};
