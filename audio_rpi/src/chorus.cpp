/*
 * chorus.cpp
 * Usa DaisySP Chorus internamente.
 * El wrapper extern "C" expone Chorus_init / Chorus_process para main.c
 */

// Incluir chorus.h ANTES de chorus_dsp.h para que el typedef C se defina primero
// Luego usamos daisysp::Chorus con namespace explícito para evitar ambigüedad

#include <cmath>

// ── DaisySP inline — no usamos using namespace daisysp ───────────────────────
#include "dsp.h"
#include "delayline.h"
#include "chorus_dsp.h"

// ── ChorusEngine ─────────────────────────────────────────────────────────────

void daisysp::ChorusEngine::Init(float sample_rate)
{
    sample_rate_ = sample_rate;
    del_.Init();
    lfo_amp_   = 0.f;
    feedback_  = 0.2f;
    delay_     = 1.f;
    lfo_phase_ = 0.f;
    lfo_freq_  = 0.f;
    SetDelay(0.75f);
    SetLfoFreq(0.3f);
    SetLfoDepth(0.9f);
}

float daisysp::ChorusEngine::Process(float in)
{
    float lfo_sig = ProcessLfo();
    del_.SetDelay(lfo_sig + delay_);
    float out = del_.Read();
    del_.Write(in + out * feedback_);
    return (in + out) * .5f;
}

void daisysp::ChorusEngine::SetLfoDepth(float depth)
{
    depth    = daisysp::fclamp(depth, 0.f, .93f);
    lfo_amp_ = depth * delay_;
}

void daisysp::ChorusEngine::SetLfoFreq(float freq)
{
    freq = 4.f * freq / sample_rate_;
    freq *= lfo_freq_ < 0.f ? -1.f : 1.f;
    lfo_freq_ = daisysp::fclamp(freq, -.25f, .25f);
}

void daisysp::ChorusEngine::SetDelay(float delay)
{
    delay = (.1f + delay * 7.9f); // 0.1 to 8 ms
    SetDelayMs(delay);
}

void daisysp::ChorusEngine::SetDelayMs(float ms)
{
    ms     = daisysp::fmax(0.1f, ms);
    delay_ = ms * .001f * sample_rate_;
    lfo_amp_ = daisysp::fmin(lfo_amp_, delay_);
}

void daisysp::ChorusEngine::SetFeedback(float feedback)
{
    feedback_ = daisysp::fclamp(feedback, 0.f, 1.f);
}

float daisysp::ChorusEngine::ProcessLfo()
{
    lfo_phase_ += lfo_freq_;
    if(lfo_phase_ > 1.f)
    {
        lfo_phase_ = 1.f - (lfo_phase_ - 1.f);
        lfo_freq_ *= -1.f;
    }
    else if(lfo_phase_ < -1.f)
    {
        lfo_phase_ = -1.f - (lfo_phase_ + 1.f);
        lfo_freq_ *= -1.f;
    }
    return lfo_phase_ * lfo_amp_;
}

// ── daisysp::Chorus ──────────────────────────────────────────────────────────

void daisysp::Chorus::Init(float sample_rate)
{
    engines_[0].Init(sample_rate);
    engines_[1].Init(sample_rate);
    engines_[1].SetDelay(0.5f);   // delay diferente para richness
    gain_frac_ = .5f;
    sigl_ = sigr_ = 0.f;
}

float daisysp::Chorus::Process(float in)
{
    sigl_ = engines_[0].Process(in);
    sigr_ = engines_[1].Process(in);
    sigl_ *= gain_frac_;
    sigr_ *= gain_frac_;
    return (sigl_ + sigr_);
}

float daisysp::Chorus::GetLeft()  { return sigl_; }
float daisysp::Chorus::GetRight() { return sigr_; }

void daisysp::Chorus::SetLfoDepth(float depth)
{
    engines_[0].SetLfoDepth(depth);
    engines_[1].SetLfoDepth(depth);
}

void daisysp::Chorus::SetLfoFreq(float freq)
{
    engines_[0].SetLfoFreq(freq);
    engines_[1].SetLfoFreq(freq * 1.02f);
}

void daisysp::Chorus::SetDelay(float delay)
{
    engines_[0].SetDelay(delay);
    engines_[1].SetDelay(delay);
}

void daisysp::Chorus::SetDelayMs(float ms)
{
    engines_[0].SetDelayMs(ms);
    engines_[1].SetDelayMs(ms);
}

void daisysp::Chorus::SetFeedback(float feedback)
{
    engines_[0].SetFeedback(feedback);
    engines_[1].SetFeedback(feedback);
}

// ── Wrapper extern "C" ───────────────────────────────────────────────────────
// Incluir DESPUÉS de implementar daisysp para evitar conflicto de nombres
#include "chorus.h"

static daisysp::Chorus g_chorus;

extern "C" void Chorus_init(Chorus *ch, float rate, float depth,
                             float feedback, float mix)
{
    ch->rate     = rate;
    ch->depth    = depth;
    ch->feedback = feedback;
    ch->mix      = mix;

    g_chorus.Init(44100.f);
    float freq = 0.5f + rate * rate * 9.5f;
    g_chorus.SetLfoFreq(freq);
    g_chorus.SetLfoDepth(depth);
    g_chorus.SetDelay(0.5f);
    g_chorus.SetFeedback(feedback * 0.7f);
}

extern "C" float Chorus_process(Chorus *ch, float input)
{
    float freq = 0.5f + ch->rate * ch->rate * 9.5f;
    g_chorus.SetLfoFreq(freq);
    g_chorus.SetLfoDepth(ch->depth);
    g_chorus.SetFeedback(ch->feedback * 0.5f);
    g_chorus.SetDelay(0.3f + ch->depth * 0.4f); // delay dinámico con depth

    float wet = g_chorus.Process(input);
    return input * (1.f - ch->mix) + wet * ch->mix;
}