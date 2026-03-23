#ifndef PITCH_SHIFTER_H
#define PITCH_SHIFTER_H

#define SAMPLE_RATE     44100
#define PI              3.14159265358979323846f
#define PITCH_MAX_DELAY_MS 500
#define MAX_GRAINS      8
#define GRAIN_SIZE_MS   80

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float semitones_a;
    float mix_a;
    float semitones_b;
    float mix_b;
    float mix;
    int   grainSize;  // no usado con DaisySP, se mantiene para compatibilidad
} PitchShifter;

// Tipos internos legacy — no se usan con DaisySP
typedef struct {
    float writeIndex;
    float grainPhase[MAX_GRAINS];
    float grainReadIndex[MAX_GRAINS];
    float buffer[(SAMPLE_RATE * PITCH_MAX_DELAY_MS) / 1000];
} PitchVoice;

void  PitchShifter_init(PitchShifter *ps, float semitones, float mix);
float PitchShifter_process(PitchShifter *ps, float input);

#ifdef __cplusplus
}
#endif

#endif