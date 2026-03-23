#include <math.h>
#include <string.h>
#include "chorus.h"

#define PI 3.14159265358979323846f

// Buffer de 50ms máximo — suficiente para chorus, no para eco
#define MAX_SAMPLES ((int)(0.05f * SAMPLE_RATE))  // 2205 samples @ 44100

static float buf_l[MAX_SAMPLES];
static float buf_r[MAX_SAMPLES];
static int   wr = 0;

static float lfo_phase_l = 0.f;
static float lfo_phase_r = 0.5f;  // 180° offset

void Chorus_init(Chorus *ch, float rate, float depth, float feedback, float mix)
{
    ch->rate     = rate;
    ch->depth    = depth;
    ch->feedback = feedback;
    ch->mix      = mix;
    memset(buf_l, 0, sizeof(buf_l));
    memset(buf_r, 0, sizeof(buf_r));
    wr = 0;
    lfo_phase_l = 0.f;
    lfo_phase_r = 0.5f;
}

static float read_buf(float *buf, int write_head, float delay_samples)
{
    float pos = (float)write_head - delay_samples;
    while (pos < 0)           pos += MAX_SAMPLES;
    while (pos >= MAX_SAMPLES) pos -= MAX_SAMPLES;
    int   i1   = (int)pos % MAX_SAMPLES;
    int   i2   = (i1 + 1) % MAX_SAMPLES;
    float frac = pos - floorf(pos);
    return buf[i1] * (1.f - frac) + buf[i2] * frac;
}

float Chorus_process(Chorus *ch, float input)
{
    // Parámetros seguros
    float rate     = ch->rate  < 0.1f ? 0.1f : (ch->rate  > 3.f ? 3.f : ch->rate);
    float depth    = ch->depth < 0.f  ? 0.f  : (ch->depth > 1.f ? 1.f : ch->depth);
    float feedback = ch->feedback * 0.4f;  // máx 0.4 para evitar oscilación
    float mix      = ch->mix;

    // Delay base: 15ms — rango clásico de chorus
    float base_ms   = 15.0f;
    float depth_ms  = depth * 8.0f;  // modulación ±8ms máximo

    // LFO sinusoidal simple
    float lfo_l = sinf(2.f * PI * lfo_phase_l);
    float lfo_r = sinf(2.f * PI * lfo_phase_r);

    lfo_phase_l += rate / (float)SAMPLE_RATE;
    lfo_phase_r += rate / (float)SAMPLE_RATE;
    if (lfo_phase_l >= 1.f) lfo_phase_l -= 1.f;
    if (lfo_phase_r >= 1.f) lfo_phase_r -= 1.f;

    float delay_l = (base_ms + lfo_l * depth_ms) * (float)SAMPLE_RATE / 1000.f;
    float delay_r = (base_ms + lfo_r * depth_ms) * (float)SAMPLE_RATE / 1000.f;

    // Leer delayed
    float out_l = read_buf(buf_l, wr, delay_l);
    float out_r = read_buf(buf_r, wr, delay_r);

    // Escribir con feedback limitado
    buf_l[wr] = input + out_l * feedback;
    buf_r[wr] = input + out_r * feedback;

    wr = (wr + 1) % MAX_SAMPLES;

    // Mezclar L y R en mono
    float wet = (out_l + out_r) * 0.5f;
    return input * (1.f - mix) + wet * mix;
}