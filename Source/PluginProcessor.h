#pragma once
#include <JuceHeader.h>
#include <array>
#include "DecayEnvelope.h"
#include "XTSequencer.h"
#include "XTVoice.h"
#include "MoogLadderFilter.h"

// =============================================================================
// Patch system types
// =============================================================================

/** All patch points. Plain enum so values work as array indices. */
enum PatchPoint
{
    PP_VCF_EG  = 0,   // OUT – VCF envelope (0–1, velocity-scaled)
    PP_VCF_MOD = 1,   // IN  – additional cutoff modulation
    PP_VCA_EG   = 2,   // OUT – VCA envelope (0–1)
    PP_VCA_CV   = 3,   // IN  – additional VCA gain CV
    PP_VELOCITY  = 4,   // OUT – step velocity, held until next trigger (0–1)
    PP_VCO_EG    = 5,   // OUT – smoothed VCO envelope (0..1)
    PP_VCF_DECAY = 6,   // IN  – modulates VCF decay in normalised parameter domain
    PP_VCA_DECAY = 7,   // IN  – modulates VCA decay in normalised parameter domain
    PP_VCO_DECAY = 8,   // IN  – modulates VCO decay in normalised parameter domain
    PP_VCO1      = 9,   // OUT – pre-level VCO1 output (-1..1)
    PP_VCO2      = 10,  // OUT – pre-level VCO2 output (-1..1)
    PP_FM_AMT    = 11,  // IN  – additive FM amount CV (bipolar, 0..1 domain)
    PP_NOISE_LVL = 12,  // IN  – additive noise level CV (bipolar, 0..1 domain)
    PP_PITCH     = 13,  // OUT – step pitch, normalised bipolar (note-60)/60
    PP_MOD_A     = 14,  // OUT – sequencer Mod A, bipolar -1..1
    PP_MOD_B     = 15,  // OUT – sequencer Mod B, bipolar -1..1
    PP_MOD_C     = 16,  // OUT – sequencer Mod C, bipolar -1..1
    PP_NUM_POINTS
};

enum PatchDir { PD_Out, PD_In };

struct PatchPointMeta
{
    const char* name;
    PatchDir    dir;
    bool        bipolar;   // true = −1..1 | false = 0..1
};

// One definition shared across all translation units (C++17 inline)
//                                  name        dir      bipolar
inline const PatchPointMeta kPatchMeta[PP_NUM_POINTS] =
{
    { "VCF EG",  PD_Out, false },   // PP_VCF_EG  – unipolar 0..1 envelope
    { "VCF MOD", PD_In,  true  },   // PP_VCF_MOD – bipolar -1..1, multiplicative mod around cutoff
    { "VCA EG",   PD_Out, false },   // PP_VCA_EG   – unipolar 0..1 VCA envelope
    { "VCA CV",   PD_In,  false },   // PP_VCA_CV   – additive VCA gain CV (0..1)
    { "VELOCITY",  PD_Out, false },   // PP_VELOCITY  – step velocity, held 0..1
    { "VCO EG",   PD_Out, false },   // PP_VCO_EG   – smoothed VCO envelope 0..1
    { "VCF DECAY",PD_In,  true  },   // PP_VCF_DECAY – bipolar, normalised param-domain additive
    { "VCA DECAY",PD_In,  true  },   // PP_VCA_DECAY – bipolar, normalised param-domain additive
    { "VCO DECAY",PD_In,  true  },   // PP_VCO_DECAY – bipolar, normalised param-domain additive
    { "VCO 1",   PD_Out, true  },   // PP_VCO1      – pre-level VCO1 (-1..1)
    { "VCO 2",   PD_Out, true  },   // PP_VCO2      – pre-level VCO2 (-1..1)
    { "FM AMT",  PD_In,  true  },   // PP_FM_AMT    – additive FM amount CV
    { "NOISE LV",PD_In,  true  },   // PP_NOISE_LVL – additive noise level CV
    { "PITCH",   PD_Out, true  },   // PP_PITCH     – step pitch bipolar (note-60)/60
    { "MOD A",   PD_Out, true  },   // PP_MOD_A     – sequencer lane A bipolar
    { "MOD B",   PD_Out, true  },   // PP_MOD_B     – sequencer lane B bipolar
    { "MOD C",   PD_Out, true  },   // PP_MOD_C     – sequencer lane C bipolar
};

/** One active cable between a source and a destination. */
struct PatchCable
{
    PatchPoint src     = PP_VCF_EG;
    PatchPoint dst     = PP_VCF_MOD;
    float      amount  = 1.0f;   // 0–1 attenuator, 1 = full
    bool       enabled = true;
};

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

    XTSequencer   sequencer;
    double currentSampleRate = 44100.0;

    // Sequencer clock state
    int  lastStep            = -1;
    int  sequencerStepOffset = 0;
    std::atomic<bool> sequencerResetPending { false };
    XTVoice       voice;
        MoogLadderFilter filter;
        float currentVelocity    = 0.0f;   // held from last trigger, used as patch source
        float currentPitch       = 0.0f;   // held from last trigger, normalised bipolar
        float currentModA        = 0.0f;   // held from last trigger, bipolar -1..1
        float currentModB        = 0.0f;   // held from last trigger, bipolar -1..1
        float currentModC        = 0.0f;   // held from last trigger, bipolar -1..1
        juce::RangedAudioParameter* vcfDecayParam = nullptr;  // cached for normalised-domain mod
    juce::RangedAudioParameter* vcaDecayParam = nullptr;  // cached for normalised-domain mod
    juce::RangedAudioParameter* vcoDecayParam = nullptr;  // cached for normalised-domain mod
    juce::SmoothedValue<float> smoothedCutoff;
    juce::SmoothedValue<float> smoothedVolume;
    juce::SmoothedValue<float> smoothedVco1Level;
    juce::SmoothedValue<float> smoothedVco2Level;
    // -------------------------------------------------------------------------
    // Patch system
    // -------------------------------------------------------------------------
    static constexpr int kMaxCables = 16;

    /** Thread-safe API – call from the message thread (editor). */
    void connectPatch      (PatchPoint src, PatchPoint dst, float amount = 1.0f);
    void disconnectPatch   (PatchPoint src, PatchPoint dst);
    void clearPatches      ();
    void getCableSnapshot  (std::vector<PatchCable>& out) const;

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

    static constexpr size_t kNumMidiCcBindings = 103;
    void initialiseMidiCcBindings();
    void applyMidiCc(int ccNumber, int ccValue);

    // -------------------------------------------------------------------------
    // Lock-free patch storage (seqlock)
    //   cableSeq even  → store is stable, safe to read
    //   cableSeq odd   → write in progress, reader must retry
    //   cableWriteLock → serialises message-thread writers only (never taken by audio)
    // -------------------------------------------------------------------------
    struct CableStore {
        PatchCable data[kMaxCables] {};
        int        count = 0;
    };
    CableStore                    cableStore;
    std::atomic<uint32_t>         cableSeq { 0 };
    mutable juce::CriticalSection cableWriteLock;   // message-thread writers only
    std::array<MidiCcBinding, kNumMidiCcBindings> midiCcBindings {};
    juce::ValueTree defaultState;
    mutable juce::CriticalSection presetLock;
    juce::String currentPresetName { "Init" };

    float patchSourceValues[PP_NUM_POINTS] = {};  // written each sample (audio thread)
    float patchInputSums   [PP_NUM_POINTS] = {};  // accumulated each sample (audio thread)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XTProcessor)
};
