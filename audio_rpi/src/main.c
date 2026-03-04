#include <stdio.h>
#include <math.h>
#include <unistd.h> 
#include "../include/delay.h"
#include "overdrive.h"
#include "../include/socket_server.h"

#define SAMPLE_RATE 41100
#define PI 3.14159265359f

int main()
{
    Delay delay;
    Delay_init(&delay, 20.0f, 0.5f, 0.4f);
    Overdrive od;
    Overdrive_init(&od, 3.0f, 0.7f, 0.9f);

    socket_init();

    for (int i = 0; i < SAMPLE_RATE; i++)
    {
        float input = sinf(2.0f * PI * 440.0f * i / SAMPLE_RATE);

        float od_out = Overdrive_process(&od, input);

        float post = Delay_process(&delay, od_out);

        socket_send_two_floats(input, post);
        usleep(1000);
    }

    socket_close();
    
    return 0;
}