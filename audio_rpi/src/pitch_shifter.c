#include "pitch_shifter.h"
#include <string.h>
#include <math.h>

static inline float hanning(float x)
{
    // x debe estar en [0, 1]
    if (x < 0.0f) x = 0.0f;
    if (x > 1.0f) x = 1.0f;
    return 0.5f * (1.0f - cosf(2.0f * PI * x));
}

void PitchShifter_init(PitchShifter *ps, float semitones, float mix)
{
    ps->semitones    = semitones;
    ps->pitchFactor  = powf(2.0f, semitones / 12.0f);
    ps->mix          = mix;
    ps->writeIndex   = 0;
    ps->grainSize    = (GRAIN_SIZE_MS * SAMPLE_RATE) / 1000;

    int bufferSize = (SAMPLE_RATE * PITCH_MAX_DELAY_MS) / 1000;
    for (int i = 0; i < bufferSize; i++)
        ps->buffer[i] = 0.0f;

    for (int g = 0; g < MAX_GRAINS; g++)
        ps->grainOffsets[g] = g * (ps->grainSize / MAX_GRAINS);
}

float PitchShifter_process(PitchShifter *ps, float input)
{
    int bufferSize = (SAMPLE_RATE * PITCH_MAX_DELAY_MS) / 1000;

    // Recalcular pitchFactor en cada sample por si semitones cambio desde la interfaz
    ps->pitchFactor = powf(2.0f, ps->semitones / 12.0f);

    ps->buffer[ps->writeIndex] = input;

    float output = 0.0f;
    float totalWeight = 0.0f;

    for (int g = 0; g < MAX_GRAINS; g++)
    {
        // Posicion de lectura antes de aplicar pitch
        float readIndex = (float)ps->writeIndex - (float)ps->grainOffsets[g];

        // Wrap positivo garantizado
        while (readIndex < 0) readIndex += bufferSize;

        // Desplazar por pitchFactor
        readIndex = readIndex / ps->pitchFactor;

        // Wrap final
        while (readIndex >= bufferSize) readIndex -= bufferSize;
        while (readIndex < 0)          readIndex += bufferSize;

        // Interpolacion lineal
        int   index1 = (int)readIndex % bufferSize;
        int   index2 = (index1 + 1)   % bufferSize;
        float frac   = readIndex - floorf(readIndex);
        float grainSample = ps->buffer[index1] * (1.0f - frac)
                          + ps->buffer[index2] * frac;

        // Posicion dentro del grano para ventana Hanning — siempre [0,1]
        int posInt = ((ps->writeIndex - ps->grainOffsets[g]) % ps->grainSize
                      + ps->grainSize) % ps->grainSize;
        float grainPos = (float)posInt / (float)ps->grainSize;
        grainSample *= hanning(grainPos);

        // Peso uniforme entre granos
        output      += grainSample;
        totalWeight += 1.0f;
    }

    if (totalWeight > 0.0f)
        output /= totalWeight;

    ps->writeIndex++;
    if (ps->writeIndex >= bufferSize)
        ps->writeIndex = 0;

    return input * (1.0f - ps->mix) + output * ps->mix;
}
