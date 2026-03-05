#ifndef PITCH_SHIFTER_H
#define PITCH_SHIFTER_H

#include <stdint.h>
#include <math.h>

#define SAMPLE_RATE 41100
#define PI 3.14159265358979323846f
#define PITCH_MAX_DELAY_MS 100
#define MAX_GRAINS 3
#define GRAIN_SIZE_MS 15

typedef struct {
    float semitones;      // +/− semitonos
    float pitchFactor;    // 2^(semitones/12)
    float mix;            // wet/dry
    int writeIndex;
    int grainSize;
    float buffer[(SAMPLE_RATE * PITCH_MAX_DELAY_MS) / 1000];
    int grainOffsets[MAX_GRAINS];
} PitchShifter;

void PitchShifterH9_init(PitchShifter *ps, float semitones, float mix);
float PitchShifterH9_process(PitchShifter *ps, float input);

#endif
