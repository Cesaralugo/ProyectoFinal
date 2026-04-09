#include "reverb.h"
#include <math.h>
#include <stdint.h>

// ===================== CONFIG =====================
#define DEFAULT_SRATE 44100
#define DSY_REVERBSC_MAX_SIZE 98936
#define DELAYPOS_SHIFT 28
#define DELAYPOS_SCALE 0x10000000
#define DELAYPOS_MASK 0x0FFFFFFF
#define PI 3.14159265359

// ===================== PARAMS =====================
static const float kReverbParams[8][4] = {
    {2473.0/DEFAULT_SRATE, 0.0010, 3.1, 1966},
    {2767.0/DEFAULT_SRATE, 0.0011, 3.5, 29491},
    {3217.0/DEFAULT_SRATE, 0.0017, 1.11, 22937},
    {3557.0/DEFAULT_SRATE, 0.0006, 3.973, 9830},
    {3907.0/DEFAULT_SRATE, 0.0010, 2.341, 20643},
    {4127.0/DEFAULT_SRATE, 0.0011, 1.897, 22937},
    {2143.0/DEFAULT_SRATE, 0.0017, 0.891, 29491},
    {1933.0/DEFAULT_SRATE, 0.0006, 3.221, 14417}
};

static const float kOutputGain = 0.35f;
static const float kJpScale = 0.25f;

// ===================== INTERNAL =====================
typedef struct {
    int write_pos, buffer_size, read_pos;
    int read_pos_frac, read_pos_frac_inc;
    int seed_val, rand_line_cnt;
    float filter_state;
    float *buf;
} ReverbScDl;

typedef struct {
    float feedback, lpfreq;
    float sample_rate, damp_fact, prv_lpfreq;
    int init_done;
    ReverbScDl delay_lines[8];
    float aux[DSY_REVERBSC_MAX_SIZE];
} ReverbCore;

static ReverbCore core;

// ===================== HELPERS =====================
static int DelayLineMaxSamples(float sr, int n)
{
    float max_del = kReverbParams[n][0] + kReverbParams[n][1] * 1.125f;
    return (int)(max_del * sr + 16.5f);
}

static void InitDelayLine(ReverbScDl *lp, float sr, int n, float *mem)
{
    lp->buffer_size = DelayLineMaxSamples(sr, n);
    lp->buf = mem;
    lp->write_pos = 0;
    lp->read_pos = 0;
    lp->read_pos_frac = 0;
    lp->filter_state = 0.0f;

    for(int i = 0; i < lp->buffer_size; i++)
        lp->buf[i] = 0.0f;
}

// ===================== INIT =====================
extern "C"
void Reverb_init(Reverb *rv, float feedback, float lpfreq, float mix)
{
    rv->feedback = feedback;
    rv->lpfreq   = lpfreq;
    rv->mix      = mix;

    core.sample_rate = 44100.0f;
    core.feedback = feedback;
    core.lpfreq = lpfreq;
    core.prv_lpfreq = 0.0f;
    core.init_done = 1;

    int offset = 0;
    for(int i = 0; i < 8; i++)
    {
        InitDelayLine(&core.delay_lines[i], core.sample_rate, i, &core.aux[offset]);
        offset += core.delay_lines[i].buffer_size;
    }
}

// ===================== PROCESS =====================
extern "C"
float Reverb_process(Reverb *rv, float input)
{
    core.feedback = rv->feedback;
    core.lpfreq   = rv->lpfreq;

    float outL = 0.0f, outR = 0.0f;
    float damp = core.damp_fact;

    if(core.lpfreq != core.prv_lpfreq)
    {
        core.prv_lpfreq = core.lpfreq;
        damp = 2.0f - cosf(core.prv_lpfreq * 2.0f * PI / core.sample_rate);
        damp = damp - sqrtf(damp * damp - 1.0f);
        core.damp_fact = damp;
    }

    float junction = 0.0f;
    for(int i = 0; i < 8; i++)
        junction += core.delay_lines[i].filter_state;

    junction *= kJpScale;

    float inL = input + junction;
    float inR = input + junction;

    for(int i = 0; i < 8; i++)
    {
        ReverbScDl *lp = &core.delay_lines[i];

        float delayed = lp->buf[lp->read_pos];

        lp->buf[lp->write_pos] =
            (i & 1 ? inR : inL) - lp->filter_state;

        lp->write_pos = (lp->write_pos + 1) % lp->buffer_size;
        lp->read_pos  = (lp->read_pos + 1) % lp->buffer_size;

        float v = delayed * core.feedback;
        v = (lp->filter_state - v) * damp + v;
        lp->filter_state = v;

        if(i & 1) outR += v;
        else      outL += v;
    }

    float wet = (outL + outR) * kOutputGain;
    return input * (1.0f - rv->mix) + wet * rv->mix;
}