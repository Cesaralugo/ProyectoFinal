#include "daisysp.h"
#include "overdrive.h"

static daisysp::Overdrive g_od;

extern "C" void Overdrive_init(Overdrive *od, float gain, float tone, float output)
{
    od->gain   = gain;
    od->tone   = tone;
    od->output = output;
    g_od.Init();
    g_od.SetDrive(gain);
}

extern "C" float Overdrive_process(Overdrive *od, float input)
{
    g_od.SetDrive(od->gain);

    float wet = g_od.Process(input);

    // tone: mezcla wet con señal limpia (igual que antes)
    float sig = wet * od->tone + input * (1.0f - od->tone);

    // output: nivel final
    return sig * od->output;
}