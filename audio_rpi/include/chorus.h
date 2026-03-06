#ifndef CHORUS_H
#define CHORUS_H

#include <stdint.h>

#define CHORUS_MAX_DELAY_MS 30
#define SAMPLE_RATE 44100

typedef struct {
    float rate;   
    float depth;    
    float mix; 
} Chorus;


void Chorus_init(Chorus *ch, float rate, float depth, float mix);
float Chorus_process(Chorus *ch, float input);

#endif