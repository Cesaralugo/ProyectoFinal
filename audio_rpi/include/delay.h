#ifndef DELAY_H
#define DELAY_H

#include <stdint.h>

#define MAX_DELAY_MS 1000
#define SAMPLE_RATE 44100

typedef struct {
    float buffer[SAMPLE_RATE * MAX_DELAY_MS / 1000];
    int writeIndex;
    int delaySamples;
    float feedback;
    float mix;
    float delay_ms;
} Delay;

void Delay_init(Delay *d, float delay_ms, float feedback, float mix);
float Delay_process(Delay *d, float input);

#endif