#include <math.h>
#include <string.h>
#include "chorus.h"

#define PI 3.14159265358979323846f
#define MAX_SAMPLES ((int)(CHORUS_MAX_DELAY_MS * SAMPLE_RATE / 1000))
#define NUM_VOICES 3

// Predelay por voz en ms — clave para sonar a chorus y no a vibrato
static const float voicePredelay[NUM_VOICES] = { 25.0f, 30.0f, 35.0f };

// Detune de fase del LFO por voz (0, 120°, 240°)
static const float voiceDetune[NUM_VOICES] = { 0.0f, 0.333f, 0.666f };

static float buffer[NUM_VOICES][MAX_SAMPLES];
static int   writeIndex = 0;
static float lfoPhase[NUM_VOICES];

void Chorus_init(Chorus *ch, float rate, float depth, float feedback, float mix)
{
    ch->rate     = rate;
    ch->depth    = depth;
    ch->feedback = feedback;
    ch->mix      = mix;
    memset(buffer, 0, sizeof(buffer));
    writeIndex = 0;
    for (int v = 0; v < NUM_VOICES; v++)
        lfoPhase[v] = voiceDetune[v];
}

float Chorus_process(Chorus *ch, float input)
{
    float rate  = ch->rate;
    if (rate < 0.1f) rate = 0.1f;
    if (rate > 3.0f) rate = 3.0f;

    // depth en ms (igual que MATLAB), máx ~10ms de modulación
    float depthMS = ch->depth * 10.0f;

    // feedback suave
    float feedback = ch->feedback * 0.3f;
    if (feedback > 0.29f) feedback = 0.29f;

    float wet = 0.0f;

    for (int v = 0; v < NUM_VOICES; v++) {
        // LFO en ms, igual que MATLAB: depth * sin(...) + predelay
        float lfoMS     = depthMS * sinf(2.0f * PI * lfoPhase[v]) + voicePredelay[v];
        float lfoSamples = (lfoMS / 1000.0f) * SAMPLE_RATE;

        // Leer con interpolación lineal
        float readPos = (float)writeIndex - lfoSamples;
        while (readPos < 0)            readPos += MAX_SAMPLES;
        while (readPos >= MAX_SAMPLES) readPos -= MAX_SAMPLES;

        int   i1      = (int)readPos % MAX_SAMPLES;
        int   i2      = (i1 + 1) % MAX_SAMPLES;
        float fr      = readPos - floorf(readPos);
        float delayed = buffer[v][i1] * (1.0f - fr) + buffer[v][i2] * fr;

        wet += delayed;

        // Avanzar fase del LFO
        lfoPhase[v] += rate / SAMPLE_RATE;
        if (lfoPhase[v] >= 1.0f) lfoPhase[v] -= 1.0f;
    }

    wet /= NUM_VOICES;

    // Escribir en buffer con feedback (como MATLAB: buffer = [in + feedback*wet; ...])
    for (int v = 0; v < NUM_VOICES; v++)
        buffer[v][writeIndex] = input + feedback * wet;

    writeIndex = (writeIndex + 1) % MAX_SAMPLES;

    // Mezcla dry/wet
    float out = input * (1.0f - ch->mix) + wet * ch->mix;

    // Soft clip suave
    if (out >  1.0f) out =  1.0f;
    if (out < -1.0f) out = -1.0f;

    return out;
}