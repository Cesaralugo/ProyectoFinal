#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include "../include/socket_server.h"
#include "../include/serial_input.h"
#include <string.h>
#include <alsa/asoundlib.h>

#include "../include/delay.h"
#include "../include/overdrive.h"
#include "../include/wah.h"
#include "../include/chorus.h"
#include "../include/flanger.h"
#include "../include/pitch_shifter.h"
#include "../include/phaser.h" 

#define SAMPLE_RATE 44100
#define PI 3.14159265358979323846f

#include <alsa/asoundlib.h>

static snd_pcm_t* alsa_init(unsigned int sample_rate)
{
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;

    if (snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        fprintf(stderr, "Error abriendo dispositivo ALSA\n");
        return NULL;
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE); // 16 bits
    snd_pcm_hw_params_set_channels(handle, params, 1);                   // mono
    snd_pcm_hw_params_set_rate(handle, params, sample_rate, 0);
    snd_pcm_uframes_t buffer_size = 8192;
    snd_pcm_uframes_t period_size = SERIAL_PACKET_SAMPLES;
    snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_size);
    snd_pcm_hw_params_set_period_size_near(handle, params, &period_size, 0);
    
    if (snd_pcm_hw_params(handle, params) < 0) {
        fprintf(stderr, "Error configurando ALSA\n");
        snd_pcm_close(handle);
        return NULL;
    }

    printf("🔊 ALSA listo a %u Hz\n", sample_rate);
    return handle;
}

// ── Modo de entrada ───────────────────────────────────────────────────────────
// SIM_MODE 1 → señal sin() simulada a 440 Hz (sin ESP32, para desarrollo)
// SIM_MODE 0 → lectura real desde ESP32 por serial
#define SIM_MODE 1

#define SERIAL_PORT NULL    // autodetecta ttyUSB0/1/2, ttyACM0/1/2
#define SERIAL_BAUD  460800

char json_buffer[4096];

#define FX_OVERDRIVE     0
#define FX_WAH           1
#define FX_DELAY         2
#define FX_CHORUS        3
#define FX_FLANGER       4
#define FX_PITCHSHIFTER  5
#define FX_PHASER        6
#define FX_COUNT         7

int enabled[FX_COUNT]  = {0};
int fx_order[FX_COUNT] = {0};
int fx_order_count     =  0;

typedef struct {
    const char *effect_key;
    const char *param_key;
    int         fx_id;
    float      *target;
    float       scale;
    float       offset;
} ParamMap;

float process_effect(int fx_id, float sig,
                     Overdrive *od, Wah *wah, Chorus *ch,
                     Flanger *flanger, PitchShifter *pitch, Delay *delay, Phaser *phaser)
{
    switch (fx_id) {
        case FX_OVERDRIVE:    return Overdrive_process(od, sig);
        case FX_WAH:          return Wah_process(wah, sig);
        case FX_CHORUS:       return Chorus_process(ch, sig);
        case FX_FLANGER:      return Flanger_process(flanger, sig);
        case FX_PITCHSHIFTER: return PitchShifter_process(pitch, sig);
        case FX_DELAY:        return Delay_process(delay, sig);
        case FX_PHASER:       return Phaser_process(phaser, sig);
        default:              return sig;
    }
}

int main()
{
    Delay delay;        Delay_init(&delay, 20.0f, 0.5f, 0.4f);
    Overdrive od;       Overdrive_init(&od, 0.0f, 0.0f, 0.0f);
    Wah wah;            Wah_init(&wah, 2.0f, 3.0f, 0.9f);
    Chorus ch;          Chorus_init(&ch, 0.8f, 0.7f, 0.5f);
    Flanger flanger;    Flanger_init(&flanger, 0.25f, 0.7f, 0.3f, 0.5f);
    PitchShifter pitch; PitchShifter_init(&pitch, 7.0f, 0.5f);
    Phaser phaser; Phaser_init(&phaser, 0.5f, 0.7f, 0.3f, 0.5f);

    ParamMap map[] = {
        { "Overdrive",    "GAIN",      FX_OVERDRIVE,    &od.gain,         10.0f, 1.0f },
        { "Overdrive",    "TONE",      FX_OVERDRIVE,    &od.tone,          1.0f, 0.0f },
        { "Overdrive",    "OUTPUT",    FX_OVERDRIVE,    &od.output,        1.0f, 0.0f },
        { "Wah",          "FREQ",      FX_WAH,          &wah.freq,         1.0f, 0.0f },
        { "Wah",          "Q",         FX_WAH,          &wah.q,            1.0f, 0.0f },
        { "Wah",          "LEVEL",     FX_WAH,          &wah.level,        1.0f, 0.0f },
        { "Delay",        "TIME",      FX_DELAY,        &delay.delay_ms,   1.0f, 0.0f },
        { "Delay",        "FEEDBACK",  FX_DELAY,        &delay.feedback,   1.0f, 0.0f },
        { "Delay",        "MIX",       FX_DELAY,        &delay.mix,        1.0f, 0.0f },
        { "Chorus",       "RATE",      FX_CHORUS,       &ch.rate,          1.0f, 0.0f },
        { "Chorus",       "DEPTH",     FX_CHORUS,       &ch.depth,         1.0f, 0.0f },
        { "Chorus",       "MIX",       FX_CHORUS,       &ch.mix,           1.0f, 0.0f },
        { "Flanger",      "RATE",      FX_FLANGER,      &flanger.rate,     1.0f, 0.0f },
        { "Flanger",      "DEPTH",     FX_FLANGER,      &flanger.depth,    1.0f, 0.0f },
        { "Flanger",      "FEEDBACK",  FX_FLANGER,      &flanger.feedback, 1.0f, 0.0f },
        { "Flanger",      "MIX",       FX_FLANGER,      &flanger.mix,      1.0f, 0.0f },
        { "PitchShifter", "SEMITONES", FX_PITCHSHIFTER, &pitch.semitones,  1.0f, 0.0f },
        { "PitchShifter", "MIX",       FX_PITCHSHIFTER, &pitch.mix,        1.0f, 0.0f },
        { "Phaser",       "RATE",      FX_PHASER,       &phaser.rate,      1.0f, 0.0f },
        { "Phaser",       "DEPTH",     FX_PHASER,       &phaser.depth,     1.0f, 0.0f },
        { "Phaser",       "FEEDBACK",  FX_PHASER,       &phaser.feedback,  1.0f, 0.0f },
        { "Phaser",       "MIX",       FX_PHASER,       &phaser.mix,       1.0f, 0.0f },
    };
    int map_size = sizeof(map) / sizeof(map[0]);

#if SIM_MODE == 0
    uint16_t packet[SERIAL_PACKET_SAMPLES];
    int serial_fd = serial_open(SERIAL_PORT, SERIAL_BAUD);
    if (serial_fd < 0) {
        fprintf(stderr, "No se pudo abrir el serial. Abortando.\n");
        return 1;
    }
#else
    printf("[SIM_MODE] Usando señal sin() a 440 Hz — sin ESP32\n");
    int sim_i = 0;
#endif
    snd_pcm_t *pcm = alsa_init(SAMPLE_RATE);
    if (!pcm) return 1;
    socket_init();

    float batch_pre[SERIAL_PACKET_SAMPLES];
    float batch_post[SERIAL_PACKET_SAMPLES];
    long total_samples = 0;

    while (1) {

        int n = socket_receive(json_buffer, sizeof(json_buffer) - 1);
        if (n > 0) {
            printf("JSON recibido:\n%s\n", json_buffer);

            memset(enabled,  0, sizeof(enabled));
            memset(fx_order, 0, sizeof(fx_order));
            fx_order_count = 0;

            char *cursor = json_buffer;
            while ((cursor = strstr(cursor, "\"type\"")) != NULL) {
                char *type_colon = strchr(cursor, ':');
                if (!type_colon) break;
                char *type_start = strchr(type_colon + 1, '"');
                if (!type_start) break;
                type_start++;
                char *type_end = strchr(type_start, '"');
                if (!type_end) break;

                char effect_type[64] = {0};
                int type_len = type_end - type_start;
                if (type_len >= (int)sizeof(effect_type)) type_len = sizeof(effect_type) - 1;
                strncpy(effect_type, type_start, type_len);
                cursor = type_end + 1;

                int fx_enabled = 1;
                char *enabled_pos = strstr(cursor, "\"enabled\"");
                if (enabled_pos) {
                    char *en_colon = strchr(enabled_pos, ':');
                    if (en_colon) {
                        char *en_val = en_colon + 1;
                        while (*en_val == ' ') en_val++;
                        fx_enabled = (strncmp(en_val, "true", 4) == 0) ? 1 : 0;
                    }
                }

                char *params_pos   = strstr(cursor, "\"params\"");
                if (!params_pos) continue;
                char *params_open  = strchr(params_pos, '{');
                if (!params_open)  continue;
                char *params_close = strchr(params_open, '}');
                if (!params_close) continue;

                char params_block[512] = {0};
                int block_len = params_close - params_open + 1;
                if (block_len >= (int)sizeof(params_block)) block_len = sizeof(params_block) - 1;
                strncpy(params_block, params_open, block_len);

                printf("  [%s] enabled=%d\n", effect_type, fx_enabled);

                for (int m = 0; m < map_size; m++) {
                    if (strcmp(map[m].effect_key, effect_type) != 0) continue;
                    if (enabled[map[m].fx_id] == 0 && fx_order_count < FX_COUNT) {
                        if (fx_enabled)
                            fx_order[fx_order_count++] = map[m].fx_id;
                        enabled[map[m].fx_id] = fx_enabled;
                    }
                    if (!fx_enabled) continue;
                    char *param_pos = strstr(params_block, map[m].param_key);
                    if (!param_pos) continue;
                    char *colon = strchr(param_pos, ':');
                    if (!colon) continue;
                    float raw_value = 0.0f;
                    if (sscanf(colon + 1, "%f", &raw_value) == 1) {
                        *map[m].target = raw_value * map[m].scale + map[m].offset;
                        printf("    %s = %f\n", map[m].param_key, *map[m].target);
                    }
                }
            }
            printf("  Cadena de efectos (%d): ", fx_order_count);
            for (int k = 0; k < fx_order_count; k++) printf("%d ", fx_order[k]);
            printf("\n");
            memset(json_buffer, 0, sizeof(json_buffer));
        }

        // ---------------------------------------------------------------------
        // OBTENER SAMPLES: serial real o sin() simulada
        // ---------------------------------------------------------------------
#if SIM_MODE == 1
        for (int s = 0; s < SERIAL_PACKET_SAMPLES; s++) {
            float t = (float)sim_i / SAMPLE_RATE;
            batch_pre[s] = (sinf(2.0f * PI * 440.0f * t) > 0) ? 0.8f : -0.8f;
            sim_i++;
            if (sim_i >= SAMPLE_RATE) sim_i = 0;
        }
#else
        if (serial_read_packet(serial_fd, packet) < 0) {
            fprintf(stderr, "[serial] error leyendo paquete, reintentando...\n");
            continue;
        }
        for (int s = 0; s < SERIAL_PACKET_SAMPLES; s++)
            batch_pre[s] = serial_adc_to_float(packet[s]);
#endif

        // ---------------------------------------------------------------------
        // CADENA DE EFECTOS + envío al socket
        // ---------------------------------------------------------------------
        for (int s = 0; s < SERIAL_PACKET_SAMPLES; s++) {
            float sig = batch_pre[s];
            for (int k = 0; k < fx_order_count; k++)
                sig = process_effect(fx_order[k], sig,
                                     &od, &wah, &ch, &flanger, &pitch, &delay, &phaser);
            batch_post[s] = sig;
            total_samples++;    
        }
        
        int16_t pcm_buf[SERIAL_PACKET_SAMPLES];
        for (int s = 0; s < SERIAL_PACKET_SAMPLES; s++)
            pcm_buf[s] = (int16_t)(batch_post[s] * 32767.0f);
        
        int frames_written = snd_pcm_writei(pcm, pcm_buf, SERIAL_PACKET_SAMPLES);
        if (frames_written < 0)
            snd_pcm_recover(pcm, frames_written, 0);

        if (total_samples % 22039 < SERIAL_PACKET_SAMPLES)
            printf("audio: input=%f out=%f  cadena=%d efectos\n",
                   batch_pre[0], batch_post[0], fx_order_count);

        socket_send_batch(batch_pre, batch_post, SERIAL_PACKET_SAMPLES);
    }

#if SIM_MODE == 0
    serial_close(serial_fd);
#endif
    socket_close();
    return 0;
}
