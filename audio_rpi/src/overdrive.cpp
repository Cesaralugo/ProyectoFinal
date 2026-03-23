#include "overdrive.h"
#include <math.h>

extern "C" void Overdrive_init(Overdrive *od, float gain, float tone, float output)
{
    od->gain   = gain;
    od->tone   = tone;
    od->output = output;
}

extern "C" float Overdrive_process(Overdrive *od, float input)
{
    // Gain: 1 a 6x (suave, no 5x como antes)
    float drive = 1.0f + od->gain * 5.0f;
    float sig   = input * drive;

    // Soft clipping cúbico — más suave que tanh
    if      (sig >  1.0f) sig =  2.0f/3.0f;
    else if (sig < -1.0f) sig = -2.0f/3.0f;
    else                  sig = sig - (sig * sig * sig) / 3.0f;

    // Normalizar para compensar el gain
    sig /= (2.0f/3.0f);

    // tone: blend wet/dry
    sig = sig * od->tone + input * (1.0f - od->tone);

    return sig * od->output;
}