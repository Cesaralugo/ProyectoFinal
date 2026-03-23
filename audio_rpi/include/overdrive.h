#ifndef OVERDRIVE_H
#define OVERDRIVE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float gain;
    float tone;
    float output;
} Overdrive;

void  Overdrive_init(Overdrive *od, float gain, float tone, float output);
float Overdrive_process(Overdrive *od, float input);

#ifdef __cplusplus
}
#endif

#endif