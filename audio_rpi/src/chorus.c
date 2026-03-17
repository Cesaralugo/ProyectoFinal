#include <math.h>
#include <string.h>
#include "chorus.h"

#define PI 3.14159265358979323846f
#define CHORUS_MAX_DELAY_SAMPLES ((CHORUS_MAX_DELAY_MS * SAMPLE_RATE) / 1000)

static float buffer[CHORUS_MAX_DELAY_SAMPLES];
static int   writeIndex = 0;
static float lfoPhase   = 0.0f;
static float smoothRate = 0.0f;

void Chorus_init(Chorus *ch, float rate, float depth, float mix)
{
    ch->rate     = rate;
    ch->depth    = depth;
    ch->mix      = mix;
    memset(buffer, 0, sizeof(buffer));
    writeIndex = 0;
    lfoPhase   = 0.0f;
    smoothRate = rate;
}

float Chorus_process(Chorus *ch, float input)
{
    // --- LFO ---
    float targetRate = ch->rate;
    if (targetRate > 1.0f) targetRate = 1.0f;
    smoothRate = smoothRate + 0.001f * (targetRate - smoothRate);

    float lfo = sinf(2.0f * PI * lfoPhase);
    lfoPhase += smoothRate / SAMPLE_RATE;
    if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

    // --- Delay modulado ---
    float baseDelay    = 0.015f * SAMPLE_RATE;
    float maxMod       = 0.010f * SAMPLE_RATE;
    float delaySamples = baseDelay + lfo * (ch->depth * maxMod);

    float maxDelay = CHORUS_MAX_DELAY_SAMPLES * 0.9f;
    if (delaySamples < 1.0f)     delaySamples = 1.0f;
    if (delaySamples > maxDelay) delaySamples = maxDelay;

    float readIndex = (float)writeIndex - delaySamples;
    while (readIndex < 0)
        readIndex += CHORUS_MAX_DELAY_SAMPLES;

    int   index1  = (int)readIndex % CHORUS_MAX_DELAY_SAMPLES;
    int   index2  = (index1 + 1)   % CHORUS_MAX_DELAY_SAMPLES;
    float frac    = readIndex - floorf(readIndex);
    float delayed = buffer[index1] * (1.0f - frac) + buffer[index2] * frac;

    // --- Escribir en buffer CON feedback ---
    // feedback usa ch->depth igual que pyo (0.5 por defecto)
    float feedback = ch->depth;
    if (feedback > 0.9f) feedback = 0.9f;   // evitar inestabilidad
    buffer[writeIndex] = input + delayed * feedback;

    writeIndex++;
    if (writeIndex >= CHORUS_MAX_DELAY_SAMPLES)
        writeIndex = 0;

    // --- Mix: bal=0.5 en pyo = 50% dry 50% wet ---
    return input * (1.0f - ch->mix) + delayed * ch->mix;
}
