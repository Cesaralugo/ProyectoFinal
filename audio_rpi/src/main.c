#include <stdio.h>
#include <math.h>
#include <unistd.h> 
#include "../include/socket_server.h"

#include "../include/delay.h"
#include "../include/overdrive.h"
#include "../include/wah.h"
#include "../include/chorus.h"
#include "../include/flanger.h"
#include "../include/pitch_shifter.h"

#define SAMPLE_RATE 44100

int main()
{
    Delay delay;
    Delay_init(&delay, 20.0f, 0.5f, 0.4f);
    Overdrive od;
    Overdrive_init(&od, 3.0f, 0.7f, 0.9f);
    Wah wah;
    Wah_init(&wah, 2.0f, 3.0f, 0.9f);
    Chorus ch;
    Chorus_init(&ch,0.8f, 0.7f, 0.5f);
    Flanger flanger;
    Flanger_init(&flanger, 0.25f, 0.7f, 0.3f, 0.5f); // rate, depth, feedback, mix

    PitchShifter pitch;
    PitchShifter_init(&pitch, 7.0f, 0.5f); // +7 semitonos, mix 50%


    socket_init();

    for (int i = 0; i < SAMPLE_RATE; i++)
    {
        float input = (sinf(2.0f * PI * 440.0f * i / SAMPLE_RATE) > 0) ? 1.0f : -1.0f;  
        //float od_out = Overdrive_process(&od, input);
        //float ch_out = Chorus_process(&ch, input);

        float post = Delay_process(&delay, input);

        socket_send_two_floats(input, post);
        usleep(1000);
    }   

    socket_close();
    
    return 0;
}