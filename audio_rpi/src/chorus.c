#include <math.h>
#include <string.h>
#include "chorus.h"

#define PI 3.14159265358979323846f
#define CHORUS_MAX_DELAY_SAMPLES ((int)(CHORUS_MAX_DELAY_MS * SAMPLE_RATE / 1000))
#define NUM_VOICES 3

static float buffer[CHORUS_MAX_DELAY_SAMPLES];
static int   writeIndex = 0;
static float lfoPhase[NUM_VOICES] = {0.0f, 1.0f/3.0f, 2.0f/3.0f};

void Chorus_init(Chorus *ch, float rate, float depth, float feedback, float mix)
{
    ch->rate     = rate;
    ch->depth    = depth;
    ch->feedback = feedback;
    ch->mix      = mix;
    memset(buffer, 0, sizeof(buffer));
    writeIndex  = 0;
    lfoPhase[0] = 0.0f;
    lfoPhase[1] = 1.0f / 3.0f;
    lfoPhase[2] = 2.0f / 3.0f;
}

float Chorus_process(Chorus *ch, float input)
{
    float rate = ch->rate;
    if (rate < 0.01f) rate = 0.01f;
    if (rate > 20.0f) rate = 20.0f;

    float feedback = ch->feedback;
    if (feedback > 0.9f) feedback = 0.9f;

    // Base delay más largo = más ethereal
    // 20ms base + hasta 25ms de modulación según depth
    float baseDelay = 0.020f * SAMPLE_RATE;
    float modDepth  = ch->depth * 0.025f * SAMPLE_RATE;

    float wet = 0.0f;
    for (int v = 0; v < NUM_VOICES; v++) {
        float lfo = sinf(2.0f * PI * lfoPhase[v]);

        float delaySamples = baseDelay + lfo * modDepth;
        if (delaySamples < 1.0f) delaySamples = 1.0f;
        if (delaySamples > CHORUS_MAX_DELAY_SAMPLES * 0.9f)
            delaySamples = CHORUS_MAX_DELAY_SAMPLES * 0.9f;

        float readPos = (float)writeIndex - delaySamples;
        if (readPos < 0) readPos += CHORUS_MAX_DELAY_SAMPLES;

        int   i1 = (int)readPos % CHORUS_MAX_DELAY_SAMPLES;
        int   i2 = (i1 + 1)    % CHORUS_MAX_DELAY_SAMPLES;
        float fr = readPos - floorf(readPos);
        wet     += buffer[i1] * (1.0f - fr) + buffer[i2] * fr;

        lfoPhase[v] += rate / SAMPLE_RATE;
        if (lfoPhase[v] >= 1.0f) lfoPhase[v] -= 1.0f;
    }

    wet /= NUM_VOICES;

    buffer[writeIndex] = input + wet * feedback;
    writeIndex = (writeIndex + 1) % CHORUS_MAX_DELAY_SAMPLES;

    return input * (1.0f - ch->mix) + wet * ch->mix;
}
