#include "overdrive.h"
#include <math.h>

// Inicializa parámetros
void Overdrive_init(Overdrive *od, float gain, float tone, float output) {
    od->gain = gain;
    od->tone = tone;
    od->output = output;
}

// Procesa la muestra
float Overdrive_process(Overdrive *od, float input) {
    // Amplificación
    float sig = input * od->gain;

    // Clipping simple (hard clip)
    if(sig > 1.0f) sig = 1.0f;
    if(sig < -1.0f) sig = -1.0f;

    // Tonalidad simple: reduce agudos según tone
    sig = sig * od->tone + input * (1.0f - od->tone);

    // Nivel de salida
    sig *= od->output;

    return sig;
}