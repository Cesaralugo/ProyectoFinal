/*
Copyright (c) 2020 Electrosmith, Corp
MIT License — https://opensource.org/licenses/MIT
Extracted from DaisySP for standalone use on Linux/RPi.
*/
#pragma once
#ifndef DSY_DSP_H
#define DSY_DSP_H

#include <cmath>
#include <stdint.h>

namespace daisysp
{
constexpr float kPi     = 3.14159265358979323846f;
constexpr float kTwoPi  = 6.28318530717958647692f;
constexpr float PI_F    = 3.14159265358979323846f;
constexpr float TWOPI_F = 6.28318530717958647692f;

inline float fclamp(float in, float min, float max)
{
    return std::fmin(std::fmax(in, min), max);
}

inline float fmax(float a, float b) { return a > b ? a : b; }
inline float fmin(float a, float b) { return a < b ? a : b; }

} // namespace daisysp
#endif
