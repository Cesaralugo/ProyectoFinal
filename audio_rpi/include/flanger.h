#ifndef FLANGER_H
#define FLANGER_H

#define SAMPLE_RATE 41100

#ifndef PI
#define PI 3.14159265358979323846f
#endif

typedef struct {
    float rate;    
    float depth;    
    float feedback;
    float mix;
} Flanger;

// Inicializa el efecto
void Flanger_init(Flanger *flanger, float rate, float depth, float feedback, float mix);

// Procesa una sola muestra
float Flanger_process(Flanger *flanger, float input);

#endif
