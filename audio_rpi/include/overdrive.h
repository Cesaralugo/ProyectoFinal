#ifndef OVERDRIVE_H
#define OVERDRIVE_H

#define SAMPLE_RATE 44100


typedef struct {
    float gain;    // cuánto amplifica la señal
    float tone;    // control de brillo 
    float output;  // nivel final de salida
} Overdrive;

// Inicializa el efecto
void Overdrive_init(Overdrive *od, float gain, float tone, float output);

// Procesa una sola muestra
float Overdrive_process(Overdrive *od, float input);

#endif