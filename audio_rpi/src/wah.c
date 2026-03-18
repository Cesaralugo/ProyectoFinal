#include <math.h>
#include "wah.h"

#define PI            3.14159265358979323846f
#define WAH_FREQ_MIN  300.0f
#define WAH_FREQ_MAX  4000.0f

typedef struct {
    float a0, a1, a2, b1, b2;
    float w1, w2;
} Biquad;

static Biquad filter;
static float  lfo_phase = 0.0f;

void Wah_init(Wah *wah, float freq, float q, float level)
{
    wah->freq  = freq;
    wah->q     = q;
    wah->level = level;
    filter.w1  = 0.0f;
    filter.w2  = 0.0f;
    lfo_phase  = 0.0f;
}

static void Wah_updateCoeffs(float center_freq, float q)
{
    float freq = center_freq;
    if (freq < 20.0f)    freq = 20.0f;
    if (freq > 18000.0f) freq = 18000.0f;

    float omega = 2.0f * PI * freq / SAMPLE_RATE;
    float cos_w = cosf(omega);
    float sin_w = sinf(omega);
    float alpha = sin_w / (2.0f * q);

    float norm = 1.0f + alpha;
    filter.a0  =  alpha / norm;
    filter.a1  =  0.0f;
    filter.a2  = -alpha / norm;
    filter.b1  = (-2.0f * cos_w) / norm;
    filter.b2  = (1.0f - alpha)  / norm;
}

float Wah_process(Wah *wah, float input)
{
    // FREQ llega en Hz (300-4000), mapear a LFO rate 0.1-4 Hz
    float freq_norm = (wah->freq - 300.0f) / (4000.0f - 300.0f);  // → 0-1
    float lfo_rate  = 0.1f + freq_norm * 3.9f;                     // → 0.1-4 Hz

    // Q llega en rango 0.1-10, usarlo directo como Q del biquad
    float biquad_q = wah->q;
    if (biquad_q < 0.1f) biquad_q = 0.1f;

    float lfo_val   = 0.5f * (1.0f + sinf(lfo_phase));
    float log_sweep = WAH_FREQ_MIN * powf(WAH_FREQ_MAX / WAH_FREQ_MIN, lfo_val);

    lfo_phase += 2.0f * PI * lfo_rate / SAMPLE_RATE;
    if (lfo_phase >= 2.0f * PI) lfo_phase -= 2.0f * PI;

    Wah_updateCoeffs(log_sweep, biquad_q);

    float w   = input - filter.b1 * filter.w1 - filter.b2 * filter.w2;
    float wet = filter.a0 * w + filter.a1 * filter.w1 + filter.a2 * filter.w2;

    filter.w2 = filter.w1;
    filter.w1 = w;

    if (wet >  2.0f) wet =  2.0f;
    if (wet < -2.0f) wet = -2.0f;

    return (0.15f * input + 0.85f * wet) * wah->level;
}