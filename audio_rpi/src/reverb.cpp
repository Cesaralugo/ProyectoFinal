#include "daisysp.h"
#include "reverb.h"

static daisysp::ReverbSc g_reverb;

extern "C" void Reverb_init(Reverb *rv, float feedback, float lpfreq, float mix)
{
    rv->feedback = feedback;
    rv->lpfreq   = lpfreq;
    rv->mix      = mix;

    g_reverb.Init(44100.f);
    g_reverb.SetFeedback(feedback);
    g_reverb.SetLpFreq(lpfreq);
}

extern "C" float Reverb_process(Reverb *rv, float input)
{
    g_reverb.SetFeedback(rv->feedback);
    g_reverb.SetLpFreq(rv->lpfreq);

    float outL, outR;
    g_reverb.Process(input, input, &outL, &outR);

    float wet = (outL + outR) * 0.5f;
    return input * (1.f - rv->mix) + wet * rv->mix;
}