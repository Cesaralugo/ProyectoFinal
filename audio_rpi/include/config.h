#ifndef CONFIG_H
#define CONFIG_H

#define PACKET_SAMPLES 128
#define SAMPLE_RATE 44100

#define MASTER_GAIN_MIN     0.1f
#define MASTER_GAIN_MAX     4.0f
#define MASTER_GAIN_DEFAULT 1.0f

#define PORTAUDIO_LATENCY_MS 10  // Target latency in ms (WASAPI exclusive mode)

#endif