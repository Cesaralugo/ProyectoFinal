#include <math.h>
#include <string.h>
#include "phaser.h"

#define PI 3.14159265358979323846f

void Phaser_init(Phaser *ph, float rate, float depth, float feedback, float mix)
{
    ph->rate            = rate;
    ph->depth           = depth;
    ph->feedback        = feedback;
    ph->mix             = mix;
    ph->lfo_phase       = 0.0f;
    ph->feedback_sample = 0.0f;
    memset(ph->z, 0, sizeof(ph->z));
}

float Phaser_process(Phaser *ph, float input)
{
    // LFO seno normalizado [0, 1]
    float lfo = 0.5f * (1.0f + sinf(2.0f * PI * ph->lfo_phase));
    ph->lfo_phase += ph->rate / SAMPLE_RATE;
    if (ph->lfo_phase >= 1.0f)
        ph->lfo_phase -= 1.0f;

    // El LFO barre la frecuencia del notch entre freq_min y freq_max
    // depth controla cuanto sube la frecuencia maxima
    float freq_min = 200.0f;
    float freq_max = 200.0f + ph->depth * 3800.0f;  // hasta 4000 Hz con depth=1
    float freq     = freq_min + lfo * (freq_max - freq_min);

    // Coeficiente all-pass calculado desde la frecuencia
    // a = (tan(pi*f/sr) - 1) / (tan(pi*f/sr) + 1)
    float tan_w = tanf(PI * freq / SAMPLE_RATE);
    float a     = (tan_w - 1.0f) / (tan_w + 1.0f);

    // Señal con feedback
    float sig = input + ph->feedback * ph->feedback_sample;

    // Cascada de 4 all-pass de primer orden
    // y[n] = a*x[n] + x[n-1] - a*y[n-1]
    for (int i = 0; i < PHASER_STAGES; i++) {
        float y  = a * sig + ph->z[i];
        ph->z[i] = sig - a * y;  // guardar estado correcto
        sig = y;
    }

    if (sig >  1.5f) sig =  1.5f;
    if (sig < -1.5f) sig = -1.5f;

    ph->feedback_sample = sig;

    return input * (1.0f - ph->mix) + sig * ph->mix;
}
