#ifndef PHASER_H
#define PHASER_H

#include <stdint.h>

#define PHASER_STAGES   4       // número de all-pass en cascada
#define SAMPLE_RATE     44100

typedef struct {
    float rate;     // velocidad del LFO (Hz)
    float depth;    // profundidad de la modulación [0.0 - 1.0]
    float feedback; // retroalimentación [-1.0 - 1.0]
    float mix;      // mezcla wet/dry [0.0 - 1.0]

    // Estado interno — no tocar desde fuera
    float lfo_phase;
    float z[PHASER_STAGES];   // estado de cada all-pass
    float feedback_sample;
} Phaser;

void  Phaser_init(Phaser *ph, float rate, float depth, float feedback, float mix);
float Phaser_process(Phaser *ph, float input);

#endif