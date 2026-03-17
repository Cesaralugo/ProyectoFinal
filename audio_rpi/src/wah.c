#include <math.h>
#include "wah.h"

#define PI 3.14159265358979323846f

// Rango del sweep en Hz alrededor de la frecuencia central
#define WAH_FREQ_MIN  500.0f
#define WAH_FREQ_MAX  1400.0f

typedef struct {
    float a0, a1, a2, b1, b2;
    float w1, w2;
} Biquad;

static Biquad filter;
static float  lfo_phase = 0.0f;   // fase del LFO, 0..2PI

void Wah_init(Wah *wah, float freq, float q, float level)
{
    wah->freq  = freq;   // velocidad del sweep (Hz del LFO, ej: 1.5 Hz)
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

    // Bandpass filter (RBJ)
    float norm = 1.0f + alpha;
    filter.a0 =  alpha / norm;
    filter.a1 =  0.0f;
    filter.a2 = -alpha / norm;
    filter.b1 = (-2.0f * cos_w) / norm;
    filter.b2 = (1.0f - alpha)  / norm;
}

float Wah_process(Wah *wah, float input)
{
    // --- LFO: seno que barre entre WAH_FREQ_MIN y WAH_FREQ_MAX ---
    // wah->freq controla la velocidad del sweep (Hz del LFO)
    float lfo_rate = wah->freq;
    if (lfo_rate < 0.1f) lfo_rate = 0.1f;
    if (lfo_rate > 4.0f)  lfo_rate = 4.0f;   // techo más bajo
    
    // Seno elevado al cubo — se mueve lento en los extremos,
    // más rápido en el centro, igual que un pedal físico
    float s       = sinf(lfo_phase);
    float lfo_val = 0.5f * (1.0f + s * s * s);   // 0..1, curva suave
    
    float sweep_freq = WAH_FREQ_MIN + lfo_val * (WAH_FREQ_MAX - WAH_FREQ_MIN);
    lfo_phase += 2.0f * PI * lfo_rate / SAMPLE_RATE;
    if (lfo_phase >= 2.0f * PI) lfo_phase -= 2.0f * PI;

    // --- Actualizar filtro con la frecuencia del LFO ---
    float q = wah->q;
    if (q < 0.5f) q = 0.5f;   // q alto = wah más pronunciado

    Wah_updateCoeffs(sweep_freq, q);

    // --- Procesar sample ---
    float w   = input - filter.b1 * filter.w1 - filter.b2 * filter.w2;
    float out = filter.a0 * w + filter.a1 * filter.w1 + filter.a2 * filter.w2;

    filter.w2 = filter.w1;
    filter.w1 = w;

    if (out >  2.0f) out =  2.0f;
    if (out < -2.0f) out = -2.0f;

    return out * wah->level;
}
