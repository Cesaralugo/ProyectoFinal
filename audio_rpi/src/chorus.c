#include <math.h>
#include <string.h>
#include "chorus.h"

#define PI 3.14159265358979323846f
#define CHORUS_MAX_DELAY_SAMPLES ((CHORUS_MAX_DELAY_MS * SAMPLE_RATE) / 1000)

static float buffer[CHORUS_MAX_DELAY_SAMPLES];
static int   writeIndex = 0;
static float lfoPhase   = 0.0f;

void Chorus_init(Chorus *ch, float rate, float depth, float mix)
{
    ch->rate  = rate;
    ch->depth = depth;
    ch->mix   = mix;
    memset(buffer, 0, sizeof(buffer));
    writeIndex = 0;
    lfoPhase   = 0.0f;
}

float Chorus_process(Chorus *ch, float input)
{
    buffer[writeIndex] = input;

    // LFO seno [-1, 1]
    float lfo = sinf(2.0f * PI * lfoPhase);
    lfoPhase += ch->rate / SAMPLE_RATE;
    if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

    // Delay base 15ms + modulacion hasta 10ms
    // Clamp para que nunca supere el buffer
    float baseDelay = 0.015f * SAMPLE_RATE;
    float maxMod    = 0.010f * SAMPLE_RATE;
    float delaySamples = baseDelay + lfo * (ch->depth * maxMod);

    // Clamp estricto: minimo 1 sample, maximo 90% del buffer
    float maxDelay = CHORUS_MAX_DELAY_SAMPLES * 0.9f;
    if (delaySamples < 1.0f)      delaySamples = 1.0f;
    if (delaySamples > maxDelay)  delaySamples = maxDelay;

    float readIndex = (float)writeIndex - delaySamples;
    // Wrap robusto para float
    while (readIndex < 0)
        readIndex += CHORUS_MAX_DELAY_SAMPLES;

    int   index1  = (int)readIndex % CHORUS_MAX_DELAY_SAMPLES;
    int   index2  = (index1 + 1)   % CHORUS_MAX_DELAY_SAMPLES;
    float frac    = readIndex - floorf(readIndex);
    float delayed = buffer[index1] * (1.0f - frac) + buffer[index2] * frac;

    writeIndex++;
    if (writeIndex >= CHORUS_MAX_DELAY_SAMPLES)
        writeIndex = 0;

    return input * (1.0f - ch->mix) + delayed * ch->mix;
}
