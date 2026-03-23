/*
 * pitch_shifter.cpp
 * DaisySP PitchShifter portado para Linux/RPi.
 * Wrapper extern "C" para que main.c lo use sin cambios.
 *
 * Tu PitchShifter tiene dos voces (A y B) con semitones independientes.
 * Voz A: semitones_a, mix_a
 * Voz B: semitones_b, mix_b
 * Mix global: mix
 */

#include "pitchshifter_dsp.h"

static daisysp::PitchShifter g_ps_a;
static daisysp::PitchShifter g_ps_b;

#include "pitch_shifter.h"

extern "C" void PitchShifter_init(PitchShifter *ps, float semitones, float mix)
{
    ps->semitones_a = semitones;
    ps->semitones_b = 0.0f;
    ps->mix_a       = 1.0f;
    ps->mix_b       = 0.0f;
    ps->mix         = mix;
    ps->grainSize   = 0; // no usado con DaisySP

    g_ps_a.Init(44100.f);
    g_ps_b.Init(44100.f);

    g_ps_a.SetTransposition(semitones);
    g_ps_b.SetTransposition(0.0f);
    g_ps_a.SetFun(0.0f);
    g_ps_b.SetFun(0.0f);
}

extern "C" float PitchShifter_process(PitchShifter *ps, float input)
{
    g_ps_a.SetTransposition(ps->semitones_a);
    g_ps_b.SetTransposition(ps->semitones_b);

    float wet_a = g_ps_a.Process(input) * ps->mix_a;
    float wet_b = g_ps_b.Process(input) * ps->mix_b;

    float wet = wet_a + wet_b;

    // Normalizar si ambas voces están activas
    float total_mix = ps->mix_a + ps->mix_b;
    if(total_mix > 0.01f)
        wet /= total_mix;

    return input * (1.0f - ps->mix) + wet * ps->mix;
}