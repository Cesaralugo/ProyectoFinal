#include <math.h>
#include "wah.h"

#define PI 3.14159265358979323846f

typedef struct {
    float a0, a1, a2;
    float b1, b2;
    float w1, w2;  // estados del Direct Form II
} Biquad;

static Biquad filter;

void Wah_init(Wah *wah, float freq, float q, float level)
{
    wah->freq  = freq;
    wah->q     = q;
    wah->level = level;
    filter.w1  = 0.0f;
    filter.w2  = 0.0f;
}

static void Wah_updateCoeffs(Wah *wah)
{
    // Bandpass peaking
    float omega = 2.0f * PI * wah->freq / SAMPLE_RATE;
    float alpha = sinf(omega) / (2.0f * wah->q);

    float a0 =  1.0f + alpha;
    filter.a0 =  alpha / a0;   // b0/a0  (bandpass: b0=alpha, b1=0, b2=-alpha)
    filter.a1 =  0.0f;         // b1/a0
    filter.a2 = -alpha / a0;   // b2/a0
    filter.b1 = (-2.0f * cosf(omega)) / a0;
    filter.b2 = (1.0f - alpha) / a0;
}

float Wah_process(Wah *wah, float input)
{
    Wah_updateCoeffs(wah);

    // Direct Form II
    float w  = input - filter.b1 * filter.w1 - filter.b2 * filter.w2;
    float out = filter.a0 * w + filter.a1 * filter.w1 + filter.a2 * filter.w2;

    filter.w2 = filter.w1;
    filter.w1 = w;

    return out * wah->level;
}
