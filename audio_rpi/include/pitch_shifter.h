#ifndef PITCH_SHIFTER_H
#define PITCH_SHIFTER_H

#include <math.h>

#define SAMPLE_RATE        44100
#define PI                 3.14159265358979323846f
#define PITCH_MAX_DELAY_MS 300
#define MAX_GRAINS         6
#define GRAIN_SIZE_MS      60

typedef struct {
    float semitones;
    float pitchFactor;
    float mix;

    int   writeIndex;
    int   grainSize;

    // Estado de cada grano
    float grainPhase[MAX_GRAINS];      // posicion dentro del grano [0, 1]
    float grainReadIndex[MAX_GRAINS];  // posicion de lectura en el buffer

    float buffer[(SAMPLE_RATE * PITCH_MAX_DELAY_MS) / 1000];

    // legacy — ya no se usa pero se mantiene para no romper nada
    int grainOffsets[MAX_GRAINS];
} PitchShifter;

void  PitchShifter_init(PitchShifter *ps, float semitones, float mix);
float PitchShifter_process(PitchShifter *ps, float input);

#endif
