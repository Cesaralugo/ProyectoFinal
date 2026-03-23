#include "daisysp.h"
#include "chorus.h"

static daisysp::Chorus g_chorus;

extern "C" void Chorus_init(Chorus *ch, float rate, float depth, float feedback, float mix)
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
    g_chorus.SetFeedback(feedback * 0.5f);
}

extern "C" float Chorus_process(Chorus *ch, float input)
{
    float freq = 0.5f + ch->rate * ch->rate * 9.5f;
    g_chorus.SetLfoFreq(freq);
    g_chorus.SetLfoDepth(ch->depth);
    g_chorus.SetFeedback(0.0f);  // feedback a 0 siempre, ignorar el param
    g_chorus.SetDelay(0.3f + ch->depth * 0.4f);

    g_chorus.Process(input);
    float wetL = g_chorus.GetLeft();
    float wetR = g_chorus.GetRight();
    float wet  = (wetL + wetR) * 0.5f;

    return input * (1.f - ch->mix) + wet * ch->mix;
}

