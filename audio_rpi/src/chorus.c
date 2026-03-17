#include <math.h>
#include <string.h>
#include "chorus.h"

#define PI 3.14159265358979323846f
#define MAX_SAMPLES ((int)(CHORUS_MAX_DELAY_MS * SAMPLE_RATE / 1000))
#define NUM_VOICES 3

// Buffer independiente por voz
static float buffer[NUM_VOICES][MAX_SAMPLES];
static int   writeIndex = 0;
static float lfoPhase[NUM_VOICES] = {0.0f, 1.0f/3.0f, 2.0f/3.0f};

// Delays base distintos por voz — clave para el sonido ethereal
static const float voiceDelay[NUM_VOICES] = {
    0.0217f,   // voz 1: 21.7ms
    0.0190f,   // voz 2: 19.0ms
    0.0253f    // voz 3: 25.3ms
};

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

    // Modulacion proporcional al depth
    float modDepth = ch->depth * 0.008f * SAMPLE_RATE;  // max ~8ms de swing

    float wet = 0.0f;

    for (int v = 0; v < NUM_VOICES; v++) {
        // Cada voz escribe su propio input+feedback en su propio buffer
        float readPos = (float)writeIndex - 
                        (voiceDelay[v] * SAMPLE_RATE + sinf(2.0f * PI * lfoPhase[v]) * modDepth);

        while (readPos < 0) readPos += MAX_SAMPLES;

        int   i1 = (int)readPos % MAX_SAMPLES;
        int   i2 = (i1 + 1)    % MAX_SAMPLES;
        float fr = readPos - floorf(readPos);
        float delayed = buffer[v][i1] * (1.0f - fr) + buffer[v][i2] * fr;

        // Feedback independiente por voz
        buffer[v][writeIndex] = input + delayed * feedback;

        wet += delayed;

        lfoPhase[v] += rate / SAMPLE_RATE;
        if (lfoPhase[v] >= 1.0f) lfoPhase[v] -= 1.0f;
    }

    wet /= NUM_VOICES;

    writeIndex = (writeIndex + 1) % MAX_SAMPLES;

    return input * (1.0f - ch->mix) + wet * ch->mix;
}
