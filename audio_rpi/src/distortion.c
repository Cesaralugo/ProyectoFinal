/*
 * distortion.c - Distortion effect implementation.
 *
 * Audio signal passes through unchanged; the actual distortion is
 * produced by an analog circuit whose gain is set by the X9103P
 * digital potentiometer (controlled via digipot_x9103p).
 */

#include "../include/distortion.h"
#include "../include/digipot_x9103p.h"

void Distortion_init(Distortion *d, float initial_wiper)
{
    d->wiper_position = initial_wiper;
    digipot_set_wiper((uint8_t)initial_wiper);
}

float Distortion_process(Distortion *d, float sig)
{
    /* Passthrough – distortion is handled by the analog hardware */
    (void)d;
    return sig;
}

void Distortion_set_wiper(Distortion *d, uint8_t position)
{
    d->wiper_position = (float)position;
    digipot_set_wiper(position);
}
