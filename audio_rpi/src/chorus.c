#include <math.h>
#include <string.h>
#include "chorus.h"

#define PI 3.14159265358979323846f
#define MAX_SAMPLES ((int)(CHORUS_MAX_DELAY_MS * SAMPLE_RATE / 1000))
#define NUM_VOICES 3

static float buffer[NUM_VOICES][MAX_SAMPLES];
static int   writeIndex = 0;
static float lfoPhase[NUM_VOICES];

static const float voiceDelay[NUM_VOICES] = {
    0.0217f,
    0.0190f,
    0.0253f
};

static const float voiceDetune[NUM_VOICES] = {
    0.0f,
    2.0f / 3.0f,
    1.0f / 3.0f
};

void Chorus_init(Chorus *ch, float rate, float depth, float feedback, float mix)
{
    ch->rate     = rate;
    ch->depth    = depth;
    ch->feedback = feedback;
    ch->mix      = mix;
    memset(buffer, 0, sizeof(buffer));
    writeIndex   = 0;

    // Pre-avanzar el writeIndex para que las voces tengan samples que leer
    // El delay más largo es 25.3ms = ~1116 samples, avanzar el doble
    writeIndex = (int)(0.030f * SAMPLE_RATE) % MAX_SAMPLES;

    for (int v = 0; v < NUM_VOICES; v++)
        lfoPhase[v] = voiceDetune[v];
}

float Chorus_process(Chorus *ch, float input)
{
    float rate = ch->rate;
    if (rate < 0.1f) rate = 0.1f;
    if (rate > 3.0f) rate = 3.0f;

    float feedback = ch->feedback * 0.25f;
    if (feedback > 0.24f) feedback = 0.24f;

    float modDepth = ch->depth * 0.001f * SAMPLE_RATE;

    float wet = 0.0f;

    for (int v = 0; v < NUM_VOICES; v++) {
        float readPos = (float)writeIndex
                      - (voiceDelay[v] * SAMPLE_RATE
                      +  sinf(2.0f * PI * lfoPhase[v]) * modDepth);

        while (readPos < 0)            readPos += MAX_SAMPLES;
        while (readPos >= MAX_SAMPLES) readPos -= MAX_SAMPLES;

        int   i1      = (int)readPos % MAX_SAMPLES;
        int   i2      = (i1 + 1) % MAX_SAMPLES;
        float fr      = readPos - floorf(readPos);
        float delayed = buffer[v][i1] * (1.0f - fr) + buffer[v][i2] * fr;

        wet += delayed;

        lfoPhase[v] += rate / SAMPLE_RATE;
        if (lfoPhase[v] >= 1.0f) lfoPhase[v] -= 1.0f;
    }

    wet /= NUM_VOICES;

    for (int v = 0; v < NUM_VOICES; v++)
        buffer[v][writeIndex] = input + wet * feedback;

    writeIndex = (writeIndex + 1) % MAX_SAMPLES;

    // Sin atenuación extra del mix — wet ya está normalizado
    float out = input * (1.0f - ch->mix) + wet * ch->mix;
    if (out >  1.2f) out =  1.2f;
    if (out < -1.2f) out = -1.2f;

    return out;
}