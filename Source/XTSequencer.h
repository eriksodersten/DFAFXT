#pragma once

#include <array>
#include <JuceHeader.h>

struct XTSequencerStep
{
    float pitch    = 60.0f;  // MIDI note number
    float velocity = 0.8f;   // 0.0 – 1.0
    float modA     = 0.5f;   // 0.5 = centred / zero in bipolar domain
    float modB     = 0.5f;   // 0.5 = centred / zero in bipolar domain
};

class XTSequencer
{
public:
    static constexpr int numSteps    = 16;
    static constexpr int numLaneRows = 4;

    XTSequencer()
    {
        steps = {{
            { 36.0f, 1.00f, 0.50f, 0.50f },
            { 48.0f, 0.65f, 0.50f, 0.45f },
            { 38.0f, 0.82f, 0.58f, 0.50f },
            { 48.0f, 0.42f, 0.50f, 0.50f },
            { 36.0f, 1.00f, 0.50f, 0.62f },
            { 50.0f, 0.58f, 0.55f, 0.50f },
            { 38.0f, 0.74f, 0.50f, 0.38f },
            { 48.0f, 0.60f, 0.50f, 0.50f },
            { 36.0f, 1.00f, 0.50f, 0.50f },
            { 48.0f, 0.60f, 0.44f, 0.50f },
            { 38.0f, 0.78f, 0.64f, 0.56f },
            { 55.0f, 0.48f, 0.50f, 0.50f },
            { 36.0f, 0.96f, 0.50f, 0.68f },
            { 50.0f, 0.56f, 0.50f, 0.50f },
            { 38.0f, 0.72f, 0.60f, 0.40f },
            { 43.0f, 0.68f, 0.50f, 0.50f }
        }};
    }

    void prepare(double sampleRate)
    {
        sr = sampleRate;
    }

    void setStep(int index, float pitch, float velocity, float modA, float modB)
    {
        jassert(index >= 0 && index < numSteps);
        steps[(size_t) index] = { pitch, velocity, modA, modB };
    }

    const XTSequencerStep& getStep(int index) const
    {
        jassert(index >= 0 && index < numSteps);
        return steps[(size_t) index];
    }

    int getCurrentStep() const { return currentStep; }
    void setCurrentStep(int step) { currentStep = step; }
    void reset() { currentStep = -1; }

    std::array<XTSequencerStep, numSteps> steps {};

private:
    double sr          = 44100.0;
    int    currentStep = -1;
};
