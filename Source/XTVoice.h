#pragma once
#include <array>
#include <JuceHeader.h>
#include "DecayEnvelope.h"

class XTVoice
{
public:
    XTVoice() = default;

    void prepare(double sampleRate)
    {
        sr = sampleRate;
        vcoEnvelope.prepare(sampleRate);
        vcaEnvelope.prepare(sampleRate);
        vcfEnvelope.prepare(sampleRate);

        smoothedVcoEnv.reset(sampleRate, 0.001);
        smoothedFreq1.reset(sampleRate, 0.001);
        smoothedFreq2.reset(sampleRate, 0.001);
        smoothedFm.reset(sampleRate, 0.001);
        smoothedFreq1.setCurrentAndTargetValue(freq1);
        smoothedFreq2.setCurrentAndTargetValue(freq2);
        smoothedFm.setCurrentAndTargetValue(fm);

        vcaAttack.reset(sampleRate, 0.001);
        vcaAttack.setCurrentAndTargetValue(1.0f);

        smoothedAmp = 0.0f;
        clearOscillatorDecimator();

        const float dezipperSeconds = 0.001f;
        ampDezipperCoeff = 1.0f - std::exp(-1.0f / ((float)sampleRate * dezipperSeconds));
        vcfAttackCoeff   = ampDezipperCoeff;
    }

    void setDecayTime(float seconds)      { vcoEnvelope.setDecayTime(seconds); }
    void setVcaDecayTime(float seconds)   { vcaEnvelope.setDecayTime(seconds); }
    void setVcfDecayTime(float seconds)
    {
        vcfDecaySeconds = seconds;
        float vcfDecayScaled = vcfDecaySeconds * (1.0f - vel * 0.5f);
        vcfDecayScaled = juce::jmax(0.01f, vcfDecayScaled);
        vcfEnvelope.setDecayTime(vcfDecayScaled);
    }
    void setFmAmount(float amount)        { fm = amount; smoothedFm.setTargetValue(fm); }
    void setVco1EgAmount(float semitones) { vco1EgAmt = semitones; }
    void setVco2EgAmount(float semitones) { vco2EgAmt = semitones; }
    void setVcfEgAmount(float amount)     { vcfEgAmt = amount; }
    void setNoiseLevel(float level)       { noiseLevel = level; }
    void setVco1Level(float level)        { vco1Level = level; }
    void setVco2Level(float level)        { vco2Level = level; }
    void setVcaEgAmount(float amount)     { vcaEgAmount = amount; }
    void setVcaAttackTime(float seconds)  { vcaAttackSeconds = seconds; }
    void setVco1BaseFreq(float hz)        { vco1BaseFreq = hz; }
    void setVco2BaseFreq(float hz)        { vco2BaseFreq = hz; }
    void setSeqPitchRouting(int routing)  { seqPitchRouting = routing; }
    void setHardSync(bool enabled)        { hardSync = enabled; }
    void setVco1Wave(int wave)            { vco1Wave = wave; }
    void setVco2Wave(int wave)            { vco2Wave = wave; }

    float getVcfEnvValue() const { return lastVcfEnv; }

    struct Frame {
        float raw     = 0.0f;
        float ampGain = 0.0f;
        float vcfEnv  = 0.0f;
        float vcoEnv  = 0.0f;   // smoothed VCO envelope (0..1)
        float noiseRaw = 0.0f;
        float vco1Raw  = 0.0f;  // pre-level VCO1 output (-1..1)
        float vco2Raw  = 0.0f;  // pre-level VCO2 output (-1..1)
    };

    Frame processFrame()
    {
        Frame f{};
        float targetAmp = 0.0f;

        if (vcaEnvelope.isActive())
        {
            float rawVcoEnv = vcoEnvelope.process();
            smoothedVcoEnv.setTargetValue(rawVcoEnv);
            float vcoEnv     = smoothedVcoEnv.getNextValue();
            float vcaEnv     = vcaEnvelope.process();
            float attackGain = vcaAttack.getNextValue();
            float targetVcfEnv = vcfEnvelope.process();
            if (targetVcfEnv > lastVcfEnv)
                lastVcfEnv += (targetVcfEnv - lastVcfEnv) * vcfAttackCoeff;
            else
                lastVcfEnv = targetVcfEnv;

            f.vcoEnv = vcoEnv * vel;

            const float currentFreq1  = smoothedFreq1.getNextValue();
            const float currentFreq2  = smoothedFreq2.getNextValue();
            const float currentFm     = smoothedFm.getNextValue();
            const int   oversampleFactor = 8;
            const float oscSampleRate = (float)sr * (float)oversampleFactor;

            float lastVco1out = 0.0f, lastVco2out = 0.0f;

            for (int os = 0; os < oversampleFactor; ++os)
            {
                const float modFreq1 = currentFreq1 * std::pow(2.0f, vco1EgAmt * vcoEnv * vel / 12.0f);
                const float inst1    = modFreq1 / oscSampleRate * phaseDir1;
                const float phase1Start = phase1;

                phase1 += inst1;
                bool sync1 = false;
                float syncFrac = 1.0f;
                if (phase1 >= 1.0f)
                {
                    sync1 = true;
                    syncFrac = juce::jlimit(0.0f, 1.0f, (1.0f - phase1Start) / juce::jmax(inst1, 1.0e-12f));
                    phase1 = 2.0f - phase1;
                    phaseDir1 = -phaseDir1;
                }
                if (phase1 <  0.0f) { phase1 = -phase1;        phaseDir1 = -phaseDir1; }

                float vco1out = renderOscillatorSample(vco1Wave, phase1, inst1) * phaseDir1;

                const float carrierFreq2   = currentFreq2 * std::pow(2.0f, vco2EgAmt * vcoEnv * vel / 12.0f);
                const float linearFmHz     = currentFreq2 * currentFm * vco1out * 2.0f;
                const float modulatedFreq2 = carrierFreq2 + linearFmHz;
                const float inst2          = modulatedFreq2 / oscSampleRate;

                float vco2out;
                if (hardSync && sync1)
                {
                    // Split the slave advance around the exact master-sync instant so the reset
                    // lands at the correct fractional position inside this oversampled substep.
                    const float phase2AtSync = wrapPhase(phase2 + inst2 * syncFrac);
                    const float inst2AfterSync = inst2 * (1.0f - syncFrac);
                    const float phase2AfterSync = wrapPhase(inst2AfterSync);
                    const float beforeSyncOut = renderOscillatorSample(vco2Wave, phase2AtSync, inst2 * syncFrac);
                    const float afterSyncOut  = renderOscillatorSample(vco2Wave, phase2AfterSync, inst2AfterSync);

                    vco2out = beforeSyncOut * syncFrac + afterSyncOut * (1.0f - syncFrac);
                    phase2 = phase2AfterSync;
                }
                else
                {
                    phase2 = wrapPhase(phase2 + inst2);
                    vco2out = renderOscillatorSample(vco2Wave, phase2, inst2);
                }

                pushOscillatorDecimatorSample(vco1out * vco1Level + vco2out * vco2Level);
                lastVco1out = vco1out;
                lastVco2out = vco2out;
            }

            f.vco1Raw = lastVco1out;
            f.vco2Raw = lastVco2out;

            float noise   = random.nextFloat() * 2.0f - 1.0f;
            float toneAmp = vcoEnv;
            float tone    = readOscillatorDecimatorOutput();
            f.raw      = tone * toneAmp + noise * noiseLevel;
            f.vcfEnv   = lastVcfEnv * vel;
            f.noiseRaw = noise;
            targetAmp = vcaEnv * attackGain;
        }
        else
        {
            clearOscillatorDecimator();
            lastVcfEnv = 0.0f;
            f.vcfEnv   = 0.0f;
            targetAmp  = 0.0f;
        }

        smoothedAmp += (targetAmp - smoothedAmp) * ampDezipperCoeff;
        if (std::abs(smoothedAmp) < 1.0e-6f)
            smoothedAmp = 0.0f;

        lastVcaEnv = smoothedAmp;
        f.ampGain  = smoothedAmp;
        return f;
    }

    void trigger(float midiNote, float velocity)
    {
        baseMidiNote = midiNote;
        float seqOffset = midiNote - 60.0f;

        float base1 = vco1BaseFreq > 0.0f ? vco1BaseFreq : midiNoteToHz(60.0f);
        float base2 = vco2BaseFreq > 0.0f ? vco2BaseFreq : midiNoteToHz(67.0f);
        float targetFreq1;
        float targetFreq2;

        if (seqPitchRouting == 0)
        {
            targetFreq1 = base1 * std::pow(2.0f, seqOffset / 12.0f);
            targetFreq2 = base2 * std::pow(2.0f, seqOffset / 12.0f);
        }
        else if (seqPitchRouting == 1)
        {
            targetFreq1 = base1;
            targetFreq2 = base2;
        }
        else
        {
            targetFreq1 = base1;
            targetFreq2 = base2 * std::pow(2.0f, seqOffset / 12.0f);
        }

        freq1 = targetFreq1;
        freq2 = targetFreq2;
        smoothedFreq1.setTargetValue(freq1);
        smoothedFreq2.setTargetValue(freq2);

        if (velocity > 0.0f)
        {
            vel = velocity;
            vcoEnvelope.trigger();
            setVcfDecayTime(vcfDecaySeconds);
            vcfEnvelope.trigger();
            vcaAttack.reset((float)sr, vcaAttackSeconds);
            vcaAttack.setCurrentAndTargetValue(0.0f);
            vcaAttack.setTargetValue(1.0f);
            vcaEnvelope.trigger(vel);
        }
    }

    bool isActive() const { return vcaEnvelope.isActive(); }

private:
    static float polyBlep(float t, float dt)
    {
        if (dt <= 0.0f)
            return 0.0f;

        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }

        if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }

        return 0.0f;
    }

    // Band-limit the square by smoothing both discontinuities with polyBLEP.
    static float squarePolyBlep(float phase, float dt)
    {
        float y = (phase < 0.5f) ? 1.0f : -1.0f;
        y += polyBlep(phase, dt);

        float secondEdgePhase = phase + 0.5f;
        if (secondEdgePhase >= 1.0f)
            secondEdgePhase -= 1.0f;

        y -= polyBlep(secondEdgePhase, dt);
        return y;
    }

    static float wrapPhase(float phase)
    {
        while (phase >= 1.0f)
            phase -= 1.0f;
        while (phase < 0.0f)
            phase += 1.0f;
        return phase;
    }

    static float renderOscillatorSample(int wave, float phase, float phaseStep)
    {
        if (wave == 0)
        {
            float dt = juce::jlimit(0.0f, 0.5f, std::abs(phaseStep));
            return squarePolyBlep(phase, dt);
        }

        float p   = phase * 4.0f;
        float tri = (p < 1.0f) ? p : (p < 3.0f) ? 2.0f - p : p - 4.0f;
        return std::tanh(2.2f * tri) / std::tanh(2.2f);
    }

    void clearOscillatorDecimator()
    {
        oscDecimatorHistory.fill(0.0f);
        oscDecimatorIndex = 0;
    }

    void pushOscillatorDecimatorSample(float sample)
    {
        oscDecimatorHistory[(size_t)oscDecimatorIndex] = sample;
        oscDecimatorIndex = (oscDecimatorIndex + 1) % kOscDecimatorTaps;
    }

    float readOscillatorDecimatorOutput() const
    {
        float output = 0.0f;
        int historyIndex = oscDecimatorIndex;
        for (int tap = 0; tap < kOscDecimatorTaps; ++tap)
        {
            historyIndex = (historyIndex - 1 + kOscDecimatorTaps) % kOscDecimatorTaps;
            output += kOscDecimatorCoeffs[(size_t)tap] * oscDecimatorHistory[(size_t)historyIndex];
        }
        return output;
    }

    static constexpr int kOscDecimatorTaps = 31;
    inline static constexpr std::array<float, kOscDecimatorTaps> kOscDecimatorCoeffs
    {
        -0.0006479604f, -0.0014439791f, -0.0027022580f, -0.0044407449f,
        -0.0061914846f, -0.0069591594f, -0.0053706761f,  0.0000000000f,
         0.0102068186f,  0.0255224468f,  0.0451695882f,  0.0672889128f,
         0.0891803950f,  0.1077806354f,  0.1202713177f,  0.1246722937f,
         0.1202713177f,  0.1077806354f,  0.0891803950f,  0.0672889128f,
         0.0451695882f,  0.0255224468f,  0.0102068186f,  0.0000000000f,
        -0.0053706761f, -0.0069591594f, -0.0061914846f, -0.0044407449f,
        -0.0027022580f, -0.0014439791f, -0.0006479604f
    };

    static float midiNoteToHz(float note)
    {
        return 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
    }

    double sr              = 44100.0;
    float baseMidiNote     = 69.0f;
    float freq1            = 440.0f;
    float freq2            = 660.0f;
    float fm               = 0.3f;
    float vel              = 1.0f;
    float phase1           = 0.0f;
    float phase2           = 0.0f;
    float phaseDir1        = 1.0f;
    float vco1EgAmt        = 0.0f;
    float vco2EgAmt        = 0.0f;
    float vcfEgAmt         = 0.0f;
    float lastVcfEnv       = 0.0f;
    float lastVcaEnv       = 0.0f;
    float noiseLevel       = 0.2f;
    float vco1Level        = 0.6f;
    float vco2Level        = 0.2f;
    float vcaEgAmount      = 0.5f;
    float vcaAttackSeconds = 0.001f;
    float vcfDecaySeconds  = 0.3f;
    float vco1BaseFreq     = 0.0f;
    float vco2BaseFreq     = 0.0f;
    int   seqPitchRouting  = 0;
    int   vco1Wave         = 0;
    int   vco2Wave         = 0;
    bool  hardSync         = false;
    float smoothedAmp      = 0.0f;
    float ampDezipperCoeff = 1.0f;
    float vcfAttackCoeff   = 1.0f;
    DecayEnvelope vcoEnvelope;
    DecayEnvelope vcaEnvelope;
    DecayEnvelope vcfEnvelope;
    juce::Random  random;
    juce::SmoothedValue<float> smoothedVcoEnv;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedFreq1;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedFreq2;
    juce::SmoothedValue<float> smoothedFm;
    juce::SmoothedValue<float> vcaAttack;
    std::array<float, kOscDecimatorTaps> oscDecimatorHistory {};
    int oscDecimatorIndex = 0;
};
