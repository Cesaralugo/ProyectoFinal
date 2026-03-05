#include <math.h>
#include "wah.h"

typedef struct {
    float a0, a1, a2;
    float b1, b2;
    float z1, z2;
} Biquad;

static Biquad filter;

// ==========================
// Inicialización
// ==========================
void Wah_init(Wah *wah, float freq, float q, float level)
{
    wah->freq = freq;
    wah->q = q;
    wah->level = level;

    filter.z1 = 0.0f;
    filter.z2 = 0.0f;
}

// ==========================
// Actualiza coeficientes
// ==========================
static void Wah_updateCoeffs(Wah *wah)
{
    float omega = 2.0f * PI * wah->freq / SAMPLE_RATE;
    float alpha = sinf(omega) / (2.0f * wah->q);

    float b0 = alpha;
    float b1 = 0.0f;
    float b2 = -alpha;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cosf(omega);
    float a2 = 1.0f - alpha;

    // Normalizar 
    filter.a0 = b0 / a0;
    filter.a1 = b1 / a0;
    filter.a2 = b2 / a0;
    filter.b1 = a1 / a0;
    filter.b2 = a2 / a0;
}

// ==========================
// Procesamiento por muestra
// ==========================
float Wah_process(Wah *wah, float input)
{
    Wah_updateCoeffs(wah);

    float output = filter.a0 * input
                 + filter.a1 * filter.z1
                 + filter.a2 * filter.z2
                 - filter.b1 * filter.z1
                 - filter.b2 * filter.z2;

    filter.z2 = filter.z1;
    filter.z1 = output;

    return output * wah->level;
}