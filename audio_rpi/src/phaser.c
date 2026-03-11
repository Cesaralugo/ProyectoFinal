#include <math.h>
#include <string.h>
#include "phaser.h"

#define PI 3.14159265358979323846f

// =============================================================================
// Phaser — 4 filtros all-pass en cascada modulados por un LFO seno
//
// Cada all-pass de primer orden tiene la función de transferencia:
//   H(z) = (a - z^-1) / (1 - a*z^-1)
// donde 'a' controla la frecuencia del desfase (varía entre -1 y 1).
// El LFO mueve 'a' en el rango [min_a, max_a] para barrer el efecto
// de comb filtering que caracteriza el sonido de phaser.
// =============================================================================

void Phaser_init(Phaser *ph, float rate, float depth, float feedback, float mix)
{
    ph->rate     = rate;
    ph->depth    = depth;
    ph->feedback = feedback;
    ph->mix      = mix;

    ph->lfo_phase        = 0.0f;
    ph->feedback_sample  = 0.0f;
    memset(ph->z, 0, sizeof(ph->z));
}

float Phaser_process(Phaser *ph, float input)
{
    // ---- LFO seno [0, 1] ----
    float lfo = 0.5f * (1.0f + sinf(2.0f * PI * ph->lfo_phase));
    ph->lfo_phase += ph->rate / SAMPLE_RATE;
    if (ph->lfo_phase >= 1.0f)
        ph->lfo_phase -= 1.0f;

    // ---- Coeficiente all-pass ----
    // depth controla cuánto rango barre el LFO:
    //   depth=0 → a fijo en 0 (sin efecto)
    //   depth=1 → a barre de -0.9 a +0.9
    float min_a = -0.9f * ph->depth;
    float max_a =  0.9f * ph->depth;
    float a = min_a + lfo * (max_a - min_a);

    // ---- Señal de entrada con feedback ----
    float sig = input + ph->feedback * ph->feedback_sample;

    // ---- Cascada de 4 filtros all-pass ----
    // y[n] = a * x[n] + x[n-1] - a * y[n-1]
    for (int i = 0; i < PHASER_STAGES; i++) {
        float y = a * sig + ph->z[i] - a * ph->z[i];
        // forma correcta del all-pass de 1er orden:
        // y[n] = a*(x[n] - y[n-1]) + x[n-1]
        y = a * (sig - ph->z[i]) + ph->z[i];
        ph->z[i] = sig;
        sig = y;
    }

    // Guardar para el feedback del siguiente sample
    ph->feedback_sample = sig;

    // ---- Mezcla wet/dry ----
    return input * (1.0f - ph->mix) + sig * ph->mix;
}