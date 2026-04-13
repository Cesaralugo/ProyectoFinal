#ifndef CONFIG_H
#define CONFIG_H

#define PACKET_SAMPLES 128
#define SAMPLE_RATE 44100

/* Decimation factor for socket visualization data.
 * Send every Nth sample to reduce bandwidth and Python rendering load.
 * 1 = off (full resolution), 2 = 50% reduction, 4 = 75% reduction (recommended).
 * Must match SAMPLE_DECIMATION in Interfaz/config.py. */
#define SAMPLE_DECIMATION 4

#define MASTER_GAIN_MIN     0.1f
#define MASTER_GAIN_MAX     4.0f
#define MASTER_GAIN_DEFAULT 1.0f

#endif