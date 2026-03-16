#include "../include/delay.h"
#include <math.h>

void Delay_init(Delay *d, float delay_ms, float feedback, float mix)
{
    d->writeIndex = 0;
    d->delaySamples = (delay_ms / 1000.0f) * SAMPLE_RATE; // ahora float
    d->feedback = feedback;
    d->mix = mix;

    int bufferSize = (SAMPLE_RATE * MAX_DELAY_MS / 1000);
    for (int i = 0; i < bufferSize; i++)
        d->buffer[i] = 0.0f;
}

float Delay_process(Delay *d, float input)
{
    d->delaySamples = (int)((d->delay_ms / 1000.0f) * SAMPLE_RATE);
    int bufferSize = (SAMPLE_RATE * MAX_DELAY_MS / 1000);
    float readIndex = d->writeIndex - d->delaySamples;
    if (readIndex < 0)
        readIndex += bufferSize;

    int index1 = (int)readIndex;
    int index2 = (index1 + 1) % bufferSize;
    float frac = readIndex - index1;

    // Interpolación lineal
    float delayedSample = d->buffer[index1] * (1.0f - frac) + d->buffer[index2] * frac;

    // Escribir en el buffer con feedback
    d->buffer[d->writeIndex] = input + delayedSample * d->feedback;

    // Avanzar buffer
    d->writeIndex++;
    if (d->writeIndex >= bufferSize)
        d->writeIndex = 0;

    // Mezcla wet/dry
    return input * (1.0f - d->mix) + delayedSample * d->mix;
}
