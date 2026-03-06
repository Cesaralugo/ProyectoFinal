#include <stdio.h>
#include <math.h>
#include <unistd.h> 
#include "../include/socket_server.h"
#include <string.h>

#include "../include/delay.h"
#include "../include/overdrive.h"
#include "../include/wah.h"
#include "../include/chorus.h"
#include "../include/flanger.h"
#include "../include/pitch_shifter.h"

#define SAMPLE_RATE 44100
char json_buffer[4096];
float gain, tone, output;

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
    Flanger_init(&flanger, 0.25f, 0.7f, 0.3f, 0.5f); 

    PitchShifter pitch;
    PitchShifter_init(&pitch, 7.0f, 0.5f);


    socket_init();
    
    int i = 0;
    while (1){
        int n = socket_receive(json_buffer, sizeof(json_buffer)-1);
        
    if (n > 0){

        printf("JSON recibido:\n%s\n", json_buffer);

        char *p;

        p = strstr(json_buffer, "\"GAIN\"");
        if (p) {
            p = strchr(p, ':');   // buscar los :
            if (p) {
                sscanf(p+1, "%f", &gain);
                od.gain = gain * 10.0f;
            }
        }

        p = strstr(json_buffer, "\"TONE\"");
        if (p) {
            p = strchr(p, ':');
            if (p) {
                sscanf(p+1, "%f", &tone);
                od.tone = tone;
            }
        }

        p = strstr(json_buffer, "\"OUTPUT\"");
        if (p) {
            p = strchr(p, ':');
            if (p) {
                sscanf(p+1, "%f", &output);
                od.output = output;
            }
        }

        printf("Parsed OD -> gain:%f tone:%f output:%f\n", gain, tone, output);

        memset(json_buffer, 0, sizeof(json_buffer));
    }
    
        float input = (sinf(2.0f * PI * 440.0f * i / SAMPLE_RATE) > 0) ? 1.0f : -1.0f;  
        i++;
        if (i >= SAMPLE_RATE) 
            i = 0;

        float od_out  = Overdrive_process(&od, input);  
        //float wah_out = Wah_process(&wah, od_out);       
        //float post    = Chorus_process(&ch, wah_out);    

        socket_send_two_floats(input, od_out);
        usleep(22);
    }   

    socket_close();
    
    return 0;
}