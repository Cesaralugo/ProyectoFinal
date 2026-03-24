#ifndef REVERB_H
#define REVERB_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float feedback;  // decay del reverb (0-1)
    float lpfreq;    // frecuencia del filtro (200-20000 Hz)
    float mix;       // wet/dry (0-1)
} Reverb;

void  Reverb_init(Reverb *rv, float feedback, float lpfreq, float mix);
float Reverb_process(Reverb *rv, float input);

#ifdef __cplusplus
}
#endif

#endif