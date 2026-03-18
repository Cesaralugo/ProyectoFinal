#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <sndfile.h>

#include "../include/socket_server.h"
#include "../include/serial_input.h"
#include "../include/delay.h"
#include "../include/overdrive.h"
#include "../include/wah.h"
#include "../include/chorus.h"
#include "../include/flanger.h"
#include "../include/pitch_shifter.h"
#include "../include/phaser.h"

#define SAMPLE_RATE 44100
#define PI 3.14159265358979323846f

// ── Modo de entrada ───────────────────────────────────────────────────────────
// SIM_MODE 0 → lectura real desde ESP32 por serial
// SIM_MODE 1 → señal sin() simulada a 440 Hz
// SIM_MODE 2 → loop de archivo WAV
#define SIM_MODE  2
#define WAV_FILE  "guitar_sample.wav"

#define SERIAL_PORT NULL
#define SERIAL_BAUD  460800

// =============================================================================
// ALSA
// =============================================================================
static snd_pcm_t *alsa_init(unsigned int sample_rate)
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
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, params, 1);
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

    printf("ALSA listo a %u Hz\n", sample_rate);
    return handle;
}

// =============================================================================
// IDs de efectos
// =============================================================================
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

char json_buffer[4096];

// =============================================================================
// ParamMap
// =============================================================================
typedef struct {
    const char *effect_key;
    const char *param_key;
    int         fx_id;
    float      *target;
    float       scale;
    float       offset;
} ParamMap;

// =============================================================================
// Dispatcher de efectos
// =============================================================================
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

// =============================================================================
// MAIN
// =============================================================================
int main()
{
    // --- Inicializar efectos ---
    Delay        delay;   Delay_init(&delay, 20.0f, 0.5f, 0.4f);
    Overdrive    od;      Overdrive_init(&od, 0.0f, 0.0f, 0.0f);
    Wah          wah;     Wah_init(&wah, 0.7f, 5.0f, 1.0f);
    Chorus       ch;      Chorus_init(&ch, 0.8f, 0.7f, 0.5, 0.6f);
    Flanger      flanger; Flanger_init(&flanger, 0.25f, 0.7f, 0.3f, 0.5f);
    PitchShifter pitch;   PitchShifter_init(&pitch, 7.0f, 0.5f);
    Phaser       phaser;  Phaser_init(&phaser, 0.5f, 0.7f, 0.3f, 0.5f);

    ParamMap map[] = {
        // Overdrive — interfaz manda 0-1, gain necesita escala
        { "Overdrive",    "GAIN",      FX_OVERDRIVE,    &od.gain,         20.0f,  1.0f },
        { "Overdrive",    "TONE",      FX_OVERDRIVE,    &od.tone,          1.0f,  0.0f },
        { "Overdrive",    "OUTPUT",    FX_OVERDRIVE,    &od.output,        1.0f,  0.0f },
    
        // Wah — interfaz ya manda FREQ en Hz, Q en 0.1-10, LEVEL en 0-1
        { "Wah",          "FREQ",      FX_WAH,          &wah.freq,         1.0f,  0.0f },
        { "Wah",          "Q",         FX_WAH,          &wah.q,            1.0f,  0.0f },
        { "Wah",          "LEVEL",     FX_WAH,          &wah.level,        1.0f,  0.0f },
    
        // Delay — interfaz ya manda TIME en ms (1-1000)
        { "Delay",        "TIME",      FX_DELAY,        &delay.delay_ms,   1.0f,  0.0f },
        { "Delay",        "FEEDBACK",  FX_DELAY,        &delay.feedback,   1.0f,  0.0f },
        { "Delay",        "MIX",       FX_DELAY,        &delay.mix,        1.0f,  0.0f },
    
        // Chorus — todo 0-1
        { "Chorus",       "RATE",      FX_CHORUS,       &ch.rate,          1.0f,  0.0f },
        { "Chorus",       "DEPTH",     FX_CHORUS,       &ch.depth,         1.0f,  0.0f },
        { "Chorus",       "FEEDBACK",  FX_CHORUS,       &ch.feedback,      1.0f,  0.0f },
        { "Chorus",       "MIX",       FX_CHORUS,       &ch.mix,           1.0f,  0.0f },
    
        // Flanger — todo 0-1
        { "Flanger",      "RATE",      FX_FLANGER,      &flanger.rate,     1.0f,  0.0f },
        { "Flanger",      "DEPTH",     FX_FLANGER,      &flanger.depth,    1.0f,  0.0f },
        { "Flanger",      "FEEDBACK",  FX_FLANGER,      &flanger.feedback, 1.0f,  0.0f },
        { "Flanger",      "MIX",       FX_FLANGER,      &flanger.mix,      1.0f,  0.0f },
    
        // PitchShifter — interfaz ya manda -12 a 12
        { "PitchShifter", "SEMITONES", FX_PITCHSHIFTER, &pitch.semitones,  1.0f,  0.0f },
        { "PitchShifter", "MIX",       FX_PITCHSHIFTER, &pitch.mix,        1.0f,  0.0f },
    
        // Phaser — todo 0-1
        { "Phaser",       "RATE",      FX_PHASER,       &phaser.rate,      1.0f,  0.0f },
        { "Phaser",       "DEPTH",     FX_PHASER,       &phaser.depth,     1.0f,  0.0f },
        { "Phaser",       "FEEDBACK",  FX_PHASER,       &phaser.feedback,  1.0f,  0.0f },
        { "Phaser",       "MIX",       FX_PHASER,       &phaser.mix,       1.0f,  0.0f },
    };
    int map_size = sizeof(map) / sizeof(map[0]);

    // --- Inicializar fuente de audio segun SIM_MODE ---
#if SIM_MODE == 0
    uint16_t packet[SERIAL_PACKET_SAMPLES];
    int serial_fd = serial_open(SERIAL_PORT, SERIAL_BAUD);
    if (serial_fd < 0) {
        fprintf(stderr, "No se pudo abrir el serial. Abortando.\n");
        return 1;
    }
    printf("[SIM_MODE 0] Leyendo desde ESP32 por serial\n");

#elif SIM_MODE == 1
    int sim_i = 0;
    printf("[SIM_MODE 1] Usando suma de senos a 440 Hz\n");

#elif SIM_MODE == 2
    SF_INFO sf_info = {0};
    SNDFILE *sf = sf_open(WAV_FILE, SFM_READ, &sf_info);
    if (!sf) {
        fprintf(stderr, "No se pudo abrir %s: %s\n", WAV_FILE, sf_strerror(NULL));
        return 1;
    }
    printf("[SIM_MODE 2] WAV: %s | %d Hz | %d canales | %lld frames\n",
           WAV_FILE, sf_info.samplerate, sf_info.channels, (long long)sf_info.frames);

    int   wav_frames = (int)sf_info.frames;
    float *wav_data  = malloc(wav_frames * sf_info.channels * sizeof(float));
    if (!wav_data) {
        fprintf(stderr, "Sin memoria para cargar WAV\n");
        sf_close(sf);
        return 1;
    }
    sf_readf_float(sf, wav_data, wav_frames);
    sf_close(sf);
    int wav_pos = 0;
    printf("[SIM_MODE 2] WAV cargado en memoria (%d frames)\n", wav_frames);
    float wav_max = 0.0f;
    for (int i = 0; i < wav_frames * sf_info.channels; i++) {
        float abs_val = fabsf(wav_data[i]);
        if (abs_val > wav_max) wav_max = abs_val;
    }
    if (wav_max > 1.0f) {
        for (int i = 0; i < wav_frames * sf_info.channels; i++)
            wav_data[i] /= wav_max;
    }
    printf("[SIM_MODE 2] Normalizacion: max=%.4f %s\n", 
           wav_max, wav_max > 1.0f ? "(normalizado)" : "(ok)");
#endif

    // --- ALSA y socket ---
    snd_pcm_t *pcm = alsa_init(SAMPLE_RATE);
    if (!pcm) return 1;
    socket_init();

    float batch_pre[SERIAL_PACKET_SAMPLES];
    float batch_post[SERIAL_PACKET_SAMPLES];
    long  total_samples = 0;

    // =========================================================================
    // LOOP PRINCIPAL
    // =========================================================================
    while (1) {

        // --- Recibir JSON desde la interfaz Python ---
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
            printf("  Cadena (%d): ", fx_order_count);
            for (int k = 0; k < fx_order_count; k++) printf("%d ", fx_order[k]);
            printf("\n");
            memset(json_buffer, 0, sizeof(json_buffer));
        }

        // --- Obtener batch de samples ---
#if SIM_MODE == 0
        if (serial_read_packet(serial_fd, packet) < 0) {
            fprintf(stderr, "[serial] error leyendo paquete, reintentando...\n");
            continue;
        }
        for (int s = 0; s < SERIAL_PACKET_SAMPLES; s++)
            batch_pre[s] = serial_adc_to_float(packet[s]);

#elif SIM_MODE == 1
        for (int s = 0; s < SERIAL_PACKET_SAMPLES; s++) {
            float t = (float)sim_i / SAMPLE_RATE;
            batch_pre[s] =  0.50f * sinf(2.0f * PI * 220.0f * t)
                          + 0.25f * sinf(2.0f * PI * 440.0f * t)
                          + 0.15f * sinf(2.0f * PI * 660.0f * t)
                          + 0.10f * sinf(2.0f * PI * 880.0f * t);
            sim_i++;
            if (sim_i >= SAMPLE_RATE) sim_i = 0;
        }

#elif SIM_MODE == 2
        for (int s = 0; s < SERIAL_PACKET_SAMPLES; s++) {
            float sample = wav_data[wav_pos * sf_info.channels]; 
            batch_pre[s] = sample;
            wav_pos++;
            if (wav_pos >= wav_frames)
                wav_pos = 0;  // loop
        }
#endif

        // --- Cadena de efectos ---
        for (int s = 0; s < SERIAL_PACKET_SAMPLES; s++) {
            float sig = batch_pre[s];
            for (int k = 0; k < fx_order_count; k++)
                sig = process_effect(fx_order[k], sig,
                                     &od, &wah, &ch, &flanger, &pitch, &delay, &phaser);
            batch_post[s] = sig;
            total_samples++;
        }

        // --- Salida por jack ---
        int16_t pcm_buf[SERIAL_PACKET_SAMPLES];
        for (int s = 0; s < SERIAL_PACKET_SAMPLES; s++)
            pcm_buf[s] = (int16_t)(batch_post[s] * 32767.0f);

        int frames_written = snd_pcm_writei(pcm, pcm_buf, SERIAL_PACKET_SAMPLES);
        if (frames_written < 0)
            snd_pcm_recover(pcm, frames_written, 0);

        // --- Debug cada ~0.5s ---
        if (total_samples % 22039 < SERIAL_PACKET_SAMPLES)
            printf("audio: input=%f out=%f  cadena=%d efectos\n",
                   batch_pre[0], batch_post[0], fx_order_count);

        // --- Enviar al socket Python ---
        socket_send_batch(batch_pre, batch_post, SERIAL_PACKET_SAMPLES);
    }

    // --- Cleanup ---
#if SIM_MODE == 0
    serial_close(serial_fd);
#elif SIM_MODE == 2
    free(wav_data);
#endif
    snd_pcm_close(pcm);
    socket_close();
    return 0;
}
