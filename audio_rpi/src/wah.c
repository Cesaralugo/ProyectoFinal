#include <math.h>
#include "wah.h"

#define PI 3.14159265358979323846f

typedef struct {
    float a0, a1, a2, b1, b2;
    float w1, w2;
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
    float freq = wah->freq;
    if (freq < 20.0f)    freq = 20.0f;
    if (freq > 18000.0f) freq = 18000.0f;

    float q = wah->q;
    if (q < 0.1f) q = 0.1f;

    float omega = 2.0f * PI * freq / SAMPLE_RATE;
    float cos_w = cosf(omega);
    float sin_w = sinf(omega);
    float alpha = sin_w / (2.0f * q);   // bandwidth term

    // --- Bandpass (BPF, constant 0 dB peak) ---
    // H(z) = alpha / (1 + alpha) — passes only the band around freq
    float norm = 1.0f + alpha;
    filter.a0 =  alpha / norm;
    filter.a1 =  0.0f;
    filter.a2 = -alpha / norm;
    filter.b1 = (-2.0f * cos_w) / norm;
    filter.b2 = (1.0f - alpha)  / norm;
}

float Wah_process(Wah *wah, float input)
{
    Wah_updateCoeffs(wah);

    float w   = input - filter.b1 * filter.w1 - filter.b2 * filter.w2;
    float out = filter.a0 * w + filter.a1 * filter.w1 + filter.a2 * filter.w2;

    filter.w2 = filter.w1;
    filter.w1 = w;

    if (out >  2.0f) out =  2.0f;
    if (out < -2.0f) out = -2.0f;

    return out * wah->level;
}
