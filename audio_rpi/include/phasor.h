/*
Copyright (c) 2020 Electrosmith, Corp
MIT License
Extracted from DaisySP for standalone use on Linux/RPi.
*/
#pragma once
#ifndef DSY_PHASOR_H
#define DSY_PHASOR_H

#include "dsp.h"

namespace daisysp
{
class Phasor
{
  public:
    Phasor() {}
    ~Phasor() {}

    inline void Init(float sample_rate, float freq, float initial_phase)
    {
        sample_rate_ = sample_rate;
        phs_         = initial_phase;
        SetFreq(freq);
    }
    inline void Init(float sample_rate, float freq) { Init(sample_rate, freq, 0.0f); }
    inline void Init(float sample_rate)             { Init(sample_rate, 1.0f, 0.0f); }

    float Process()
    {
        float out = phs_ / TWOPI_F;
        phs_ += inc_;
        if(phs_ > TWOPI_F) phs_ -= TWOPI_F;
        if(phs_ < 0.0f)    phs_ = 0.0f;
        return out;
    }

    void SetFreq(float freq)
    {
        freq_ = freq;
        inc_  = (TWOPI_F * freq_) / sample_rate_;
    }

    inline float GetFreq() { return freq_; }

  private:
    float freq_;
    float sample_rate_, inc_, phs_;
};
} // namespace daisysp
#endif