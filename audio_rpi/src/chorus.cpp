#include "chorus.h"
#include <math.h>
#include <string.h>

#define PI          3.14159265358979323846f
#define MAX_SAMPLES 2205  // 50ms @ 44100

static float buf[MAX_SAMPLES];
static int   wr = 0;
static float lfo_phase = 0.f;

void Chorus_init(Chorus *ch, float rate, float depth, float feedback, float mix)
{
    ch->rate     = rate;
    ch->depth    = depth;
    ch->feedback = feedback;
    ch->mix      = mix;
    memset(buf, 0, sizeof(buf));
    wr        = 0;
    lfo_phase = 0.f;
}

float Chorus_process(Chorus *ch, float input)
{
    float rate  = ch->rate  < 0.1f ? 0.1f : (ch->rate  > 3.f ? 3.f : ch->rate);
    float depth = ch->depth < 0.f  ? 0.f  : (ch->depth > 1.f ? 1.f : ch->depth);
    float mix   = ch->mix   < 0.f  ? 0.f  : (ch->mix   > 1.f ? 1.f : ch->mix);

    // Predelay fijo de 20ms + modulación ±5ms
    float predelay_ms = 20.0f;
    float depth_ms    = depth * 5.0f;
    float lfo         = sinf(2.f * PI * lfo_phase);

    lfo_phase += rate / 44100.f;
    if(lfo_phase >= 1.f) lfo_phase -= 1.f;

    float delay_ms      = predelay_ms + lfo * depth_ms;
    float delay_samples = delay_ms * 44100.f / 1000.f;

    // Leer con interpolación lineal
    float pos = (float)wr - delay_samples;
    while(pos < 0)            pos += MAX_SAMPLES;
    while(pos >= MAX_SAMPLES) pos -= MAX_SAMPLES;
    int   i1   = (int)pos % MAX_SAMPLES;
    int   i2   = (i1 + 1) % MAX_SAMPLES;
    float frac = pos - floorf(pos);
    float wet  = buf[i1] * (1.f - frac) + buf[i2] * frac;

    // Escribir sin feedback — evita distorsión
    buf[wr] = input;
    wr = (wr + 1) % MAX_SAMPLES;

    return input * (1.f - mix) + wet * mix;
}