#pragma once
#include <JuceHeader.h>

struct SequencerStep
{
    float pitch    = 60.0f;  // MIDI note number
    float velocity = 0.8f;   // 0.0 – 1.0
};

class XTSequencer
{
public:
    XTSequencer()
    {
        // Default pattern – kick-like pitches
        steps[0] = { 36.0f, 1.0f };
        steps[1] = { 48.0f, 0.6f };
        steps[2] = { 38.0f, 0.8f };
        steps[3] = { 48.0f, 0.4f };
        steps[4] = { 36.0f, 1.0f };
        steps[5] = { 50.0f, 0.5f };
        steps[6] = { 38.0f, 0.7f };
        steps[7] = { 48.0f, 0.6f };
    }

    void prepare(double sampleRate)
        {
            sr = sampleRate;
        }

    void setStep(int index, float pitch, float velocity)
    {
        jassert(index >= 0 && index < numSteps);
        steps[index] = { pitch, velocity };
    }

    const SequencerStep& getStep(int index) const
    {
        return steps[index];
    }

    int  getCurrentStep() const  { return currentStep; }
    void setCurrentStep(int step) { currentStep = step; }
    void reset()                  { currentStep = -1; }

    static constexpr int numSteps = 8;

    SequencerStep steps[numSteps];

private:
    double sr          = 44100.0;
    int    currentStep = -1;
};
