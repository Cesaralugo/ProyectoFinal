#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "chorus.h"

#define PI 3.14159265358979323846f
#define MAX_DELAY_SAMPLES ((MAX_DELAY_MS * SAMPLE_RATE) / 1000)

static float buffer[MAX_DELAY_SAMPLES];
static int writeIndex = 0;
static float lfoPhase = 0.0f;

void Chorus_init(Chorus *ch, float rate, float depth, float mix)
{
    ch->rate = rate;
    ch->depth = depth;
    ch->mix = mix;

    memset(buffer, 0, sizeof(buffer));
    writeIndex = 0;
    lfoPhase = 0.0f;
}

float Chorus_process(Chorus *ch, float input)
{
    // Escribir muestra actual
    buffer[writeIndex] = input;

    // ---- LFO ----
    float lfo = sinf(2.0f * PI * lfoPhase);
    lfoPhase += ch->rate / SAMPLE_RATE;
    if (lfoPhase >= 1.0f)
        lfoPhase -= 1.0f;

    // ---- Delay variable ----
    float baseDelay = 0.015f * SAMPLE_RATE; // 15 ms base
    float modAmount = ch->depth * 0.010f * SAMPLE_RATE; // hasta 10 ms extra
    float delaySamples = baseDelay + lfo * modAmount;

    float readIndex = writeIndex - delaySamples;
    if (readIndex < 0)
        readIndex += MAX_DELAY_SAMPLES;

    int index1 = (int)readIndex;
    int index2 = (index1 + 1) % MAX_DELAY_SAMPLES;
    float frac = readIndex - index1;

    // Interpolación lineal
    float delayed =
        buffer[index1] * (1.0f - frac) +
        buffer[index2] * frac;

    // Avanzar buffer
    writeIndex++;
    if (writeIndex >= MAX_DELAY_SAMPLES)
        writeIndex = 0;

    // Mezcla wet/dry
    float output =
        input * (1.0f - ch->mix) +
        delayed * ch->mix;

    return output;
}