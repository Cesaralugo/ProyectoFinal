#ifndef WAH_H
#define WAH_H

#define SAMPLE_RATE 44100

#ifndef PI
#define PI 3.14159265358979323846f
#endif

typedef struct {
    float freq;    
    float q;    
    float level;  
} Wah;

// Inicializa el efecto
void Wah_init(Wah *wah, float freq, float q, float level);

// Procesa una sola muestra
float Wah_process(Wah *wah, float input);

#endif