#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "flanger.h"

#define FLANGER_MAX_DELAY_MS 10 
#define FLANGER_MAX_DELAY_SAMPLES ((FLANGER_MAX_DELAY_MS * SAMPLE_RATE) / 1000)

// Buffer circular
static float buffer[FLANGER_MAX_DELAY_SAMPLES];
static int writeIndex = 0;
static float lfoPhase = 0.0f;

// Inicializa el Flanger
void Flanger_init(Flanger *flanger, float rate, float depth, float feedback, float mix)
{
    flanger->rate = rate;
    flanger->depth = depth;
    flanger->feedback = feedback;
    flanger->mix = mix;

    memset(buffer, 0, sizeof(buffer));
    writeIndex = 0;
    lfoPhase = 0.0f;
}

// Procesa una sola muestra
float Flanger_process(Flanger *flanger, float input)
{
    // Escribir la muestra actual en el buffer con feedback
    float in_buf = input + buffer[writeIndex] * flanger->feedback;
    buffer[writeIndex] = in_buf;

    // LFO senoidal
    float lfo = sinf(2.0f * PI * lfoPhase);
    lfoPhase += flanger->rate / SAMPLE_RATE;
    if (lfoPhase >= 1.0f)
        lfoPhase -= 1.0f;

    // Delay variable
    float baseDelay = 0.002f * SAMPLE_RATE;                  // 2 ms base
    float modAmount = flanger->depth * 0.008f * SAMPLE_RATE; // hasta +8 ms
    float delaySamples = baseDelay + lfo * modAmount;

    float readIndex = writeIndex - delaySamples;
    if (readIndex < 0)
        readIndex += FLANGER_MAX_DELAY_SAMPLES;

    int index1 = (int)readIndex;
    int index2 = (index1 + 1) % FLANGER_MAX_DELAY_SAMPLES;
    float frac = readIndex - index1;

    // Interpolación lineal
    float delayed = buffer[index1] * (1.0f - frac) + buffer[index2] * frac;

    // Avanzar buffer
    writeIndex++;
    if (writeIndex >= FLANGER_MAX_DELAY_SAMPLES)
        writeIndex = 0;

    // Mezcla wet/dry
    float output = input * (1.0f - flanger->mix) + delayed * flanger->mix;

    return output;
}
