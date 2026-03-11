#include <math.h>
#include <string.h>
#include "phaser.h"

#define PI 3.14159265358979323846f

void Phaser_init(Phaser *ph, float rate, float depth, float feedback, float mix)
{
    ph->rate     = rate;
    ph->depth    = depth;
    ph->feedback = feedback;
    ph->mix      = mix;

    ph->lfo_phase       = 0.0f;
    ph->feedback_sample = 0.0f;
    memset(ph->z, 0, sizeof(ph->z));
}

float Phaser_process(Phaser *ph, float input)
{
    // ---- LFO seno [0, 1] ----
    float lfo = 0.5f * (1.0f + sinf(2.0f * PI * ph->lfo_phase));
    ph->lfo_phase += ph->rate / SAMPLE_RATE;
    if (ph->lfo_phase >= 1.0f)
        ph->lfo_phase -= 1.0f;

    // ---- Coeficiente all-pass: barre de -0.9*depth a +0.9*depth ----
    float min_a = -0.9f * ph->depth;
    float max_a =  0.9f * ph->depth;
    float a = min_a + lfo * (max_a - min_a);

    // ---- Señal de entrada con feedback ----
    float sig = input + ph->feedback * ph->feedback_sample;

    // ---- Cascada de 4 filtros all-pass ----
    // y[n] = a * (x[n] - y[n-1]) + x[n-1]
    for (int i = 0; i < PHASER_STAGES; i++) {
        float y  = a * (sig - ph->z[i]) + ph->z[i];
        ph->z[i] = sig;
        sig = y;
    }

    // Clampear para evitar explosion numerica con feedback alto
    if (sig >  2.0f) sig =  2.0f;
    if (sig < -2.0f) sig = -2.0f;

    ph->feedback_sample = sig;

    // ---- Mezcla wet/dry ----
    return input * (1.0f - ph->mix) + sig * ph->mix;
}