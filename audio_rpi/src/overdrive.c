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
    // 1️⃣ Amplificación
    float sig = input * od->gain;

    // 2️⃣ Clipping simple (hard clip)
    if(sig > 1.0f) sig = 1.0f;
    if(sig < -1.0f) sig = -1.0f;

    // 3️⃣ Tonalidad simple: reduce agudos según tone
    sig = sig * od->tone + input * (1.0f - od->tone);

    // 4️⃣ Nivel de salida
    sig *= od->output;

    return sig;
}