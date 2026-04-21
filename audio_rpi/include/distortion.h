#ifndef DISTORTION_H
#define DISTORTION_H

#include <stdint.h>

/*
 * distortion.h - Distortion effect (signal passthrough + DIGipot control).
 *
 * The actual distortion is produced by an analog circuit driven by the
 * X9103P digital potentiometer.  This module stores the wiper position
 * requested by the GUI and forwards it to the DIGipot driver; audio
 * samples pass through unchanged.
 *
 * wiper_position is stored as float so it integrates with the generic
 * ParamMap in main.c (all param targets are float *).
 */

typedef struct {
    float wiper_position;  /* 0.0 – 255.0 (cast to uint8_t when sent to DIGipot) */
} Distortion;

/* Initialise and set the DIGipot to initial_wiper. */
void  Distortion_init(Distortion *d, float initial_wiper);

/* Passthrough: returns sig unchanged.
 * DIGipot updates happen via Distortion_set_wiper(), not per-sample. */
float Distortion_process(Distortion *d, float sig);

/* Update stored position and send the new value to the DIGipot. */
void  Distortion_set_wiper(Distortion *d, uint8_t position);

#endif /* DISTORTION_H */
