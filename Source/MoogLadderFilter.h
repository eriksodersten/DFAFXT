#pragma once
#include <cmath>
#include <JuceHeader.h>

/*
 * Huovilainen New Moog (HNM) – LP mode
 * Chamberlin SVF – HP mode
 */
class MoogLadderFilter
{
public:
    MoogLadderFilter() = default;

    void prepare(double sampleRate)
    {
        sr = (float)sampleRate;
        alpha_    = 1.0f;
        K_        = 1.0f;
        Qadjust_  = 1.0f;
        pbg_      = 0.5f;
        oldinput_ = 0.0f;
        for (int i = 0; i < 4; ++i)
            z0_[i] = z1_[i] = 0.0f;
        svfLow_ = svfBand_ = 0.0f;
        setCutoff(1000.0f);
        setResonance(0.0f);
    }

    void reset()
    {
        for (int i = 0; i < 4; ++i)
            z0_[i] = z1_[i] = 0.0f;
        oldinput_ = 0.0f;
        svfLow_ = svfBand_ = 0.0f;
    }

    void setCutoff(float hz)
    {
        hz = juce::jlimit(5.0f, sr * 0.425f, hz);
        computeCoeffs(hz);
    }

    void setResonance(float res)
    {
        K_ = juce::jlimit(0.0f, 1.0f, res) * 4.0f;
    }

    void setHighpass(bool hp) { highpass_ = hp; }

    float process(float input)
    {
        if (highpass_)
            return processHP(input);

        float total = 0.0f;
        float interp = 0.0f;
        for (int os = 0; os < kOversampling; ++os)
        {
            float u = (interp * oldinput_ + (1.0f - interp) * input)
                      - (z1_[3] - pbg_ * input) * K_ * Qadjust_;
            u = fastTanh(u);
            float s1 = lpf(u,  0);
            float s2 = lpf(s1, 1);
            float s3 = lpf(s2, 2);
            float s4 = lpf(s3, 3);
            total += s4 * kOversamplingRecip;
            interp += kOversamplingRecip;
        }
        oldinput_ = input;
        if (!std::isfinite(total)) { reset(); return 0.0f; }
        return total;
    }

private:
    static constexpr int   kOversampling      = 4;
    static constexpr float kOversamplingRecip = 1.0f / kOversampling;

    float processHP(float input)
    {
        float q = 1.0f - K_ * 0.24f;
        q = juce::jlimit(0.02f, 1.0f, q);
        float hp = input - svfLow_ - q * svfBand_;
        svfBand_ += svfF_ * hp;
        svfLow_  += svfF_ * svfBand_;
        if (!std::isfinite(svfBand_)) { svfLow_ = svfBand_ = 0.0f; return 0.0f; }
        return hp;
    }

    float lpf(float s, int i)
    {
        float ft = s * (1.0f / 1.3f) + (0.3f / 1.3f) * z0_[i] - z1_[i];
        ft = ft * alpha_ + z1_[i];
        z1_[i] = ft;
        z0_[i] = s;
        return ft;
    }

    void computeCoeffs(float freq)
    {
        svfF_ = 2.0f * std::sin(juce::MathConstants<float>::pi * juce::jmin(freq, sr * 0.33f) / sr);
        svfF_ = juce::jlimit(0.0f, 0.8f, svfF_);

        float wc  = freq * (2.0f * juce::MathConstants<float>::pi
                    / ((float)kOversampling * sr));
        float wc2 = wc * wc;
        alpha_   =  0.9892f * wc
                  - 0.4324f * wc2
                  + 0.1381f * wc * wc2
                  - 0.0202f * wc2 * wc2;
        Qadjust_ =  1.006f
                  + 0.0536f * wc
                  - 0.095f  * wc2
                  - 0.05f   * wc2 * wc2;
    }

    static inline float fastTanh(float x)
    {
        if (x >  3.0f) return  1.0f;
        if (x < -3.0f) return -1.0f;
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    float sr        = 44100.0f;
    float alpha_    = 1.0f;
    float K_        = 0.0f;
    float Qadjust_  = 1.0f;
    float pbg_      = 0.5f;
    float oldinput_ = 0.0f;
    float svfF_     = 0.1f;
    float svfLow_   = 0.0f;
    float svfBand_  = 0.0f;
    bool  highpass_ = false;

    float z0_[4] = {};
    float z1_[4] = {};
};
