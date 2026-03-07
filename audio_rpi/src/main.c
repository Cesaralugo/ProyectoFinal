#include <stdio.h>
#include <math.h>
#include <unistd.h> 
#include "../include/socket_server.h"
#include <string.h>

#include "../include/delay.h"
#include "../include/overdrive.h"
#include "../include/wah.h"
#include "../include/chorus.h"
#include "../include/flanger.h"
#include "../include/pitch_shifter.h"

#define SAMPLE_RATE 44100
#define PI 3.14159265358979323846f

char json_buffer[4096];

// --- Tabla de mapeo genérico efecto -> parametro -> puntero ---
typedef struct {
    const char *effect_key;   // ej: "OVERDRIVE"
    const char *param_key;    // ej: "GAIN"
    float      *target;       // puntero al campo del struct
    float       scale;        // factor de escala (1.0 si no aplica)
    float       offset;       // offset aditivo tras escalar
} ParamMap;

int main()
{
    Delay delay;
    Delay_init(&delay, 20.0f, 0.5f, 0.4f);

    Overdrive od;
    Overdrive_init(&od, 0.0f, 0.0f, 0.0f);

    Wah wah;
    Wah_init(&wah, 2.0f, 3.0f, 0.9f);

    Chorus ch;
    Chorus_init(&ch, 0.8f, 0.7f, 0.5f);

    Flanger flanger;
    Flanger_init(&flanger, 0.25f, 0.7f, 0.3f, 0.5f);

    PitchShifter pitch;
    PitchShifter_init(&pitch, 7.0f, 0.5f);

    // Tabla de mapeo: agrega aqui cualquier parametro de cualquier efecto
    // El effect_key debe coincidir con el valor de "type" en el JSON (case-insensitive no aplica, usa el mismo case)
    // Formato: { "type_value", "NOMBRE_PARAM", &struct.campo, escala, offset }
    // valor_final = valor_json * scale + offset
    ParamMap map[] = {
        // Overdrive  -> "type": "Overdrive"
        { "Overdrive", "GAIN",   &od.gain,   20.0f, 1.0f },
        { "Overdrive", "TONE",   &od.tone,    1.0f, 0.0f },
        { "Overdrive", "OUTPUT", &od.output,  1.0f, 0.0f },

        // Wah        -> "type": "Wah"
        { "Wah", "FREQ",      &wah.freq,  1.0f, 0.0f },
        { "Wah", "Q",         &wah.q,     1.0f, 0.0f },
        { "Wah", "LEVEL",     &wah.level, 1.0f, 0.0f },

        // Delay      -> "type": "Delay"
        { "Delay", "TIME",     &delay.delay_ms,  1.0f, 0.0f },
        { "Delay", "FEEDBACK", &delay.feedback,  1.0f, 0.0f },
        { "Delay", "MIX",      &delay.mix,       1.0f, 0.0f },

        // Chorus     -> "type": "Chorus"
        { "Chorus", "RATE",  &ch.rate,  1.0f, 0.0f },
        { "Chorus", "DEPTH", &ch.depth, 1.0f, 0.0f },
        { "Chorus", "MIX",   &ch.mix,   1.0f, 0.0f },

        // Flanger    -> "type": "Flanger"
        { "Flanger", "RATE",     &flanger.rate,     1.0f, 0.0f },
        { "Flanger", "DEPTH",    &flanger.depth,    1.0f, 0.0f },
        { "Flanger", "FEEDBACK", &flanger.feedback, 1.0f, 0.0f },
        { "Flanger", "MIX",      &flanger.mix,      1.0f, 0.0f },

        // PitchShifter -> "type": "PitchShifter"
        { "PitchShifter", "SEMITONES", &pitch.semitones, 1.0f, 0.0f },
        { "PitchShifter", "MIX",       &pitch.mix,       1.0f, 0.0f },
    };

    int map_size = sizeof(map) / sizeof(map[0]);

    socket_init();

    int i = 0;
    while (1) {
        int n = socket_receive(json_buffer, sizeof(json_buffer) - 1);

        if (n > 0) {
            printf("JSON recibido:\n%s\n", json_buffer);

            // El JSON tiene estructura:
            // { "effects": [ { "type": "Overdrive", "enabled": true, "params": { "GAIN": 0.8 } } ] }
            // Estrategia: por cada objeto en effects[], encontrar "type" y luego buscar params dentro de ese bloque

            char *cursor = json_buffer;

            // Iterar sobre cada entrada de effects[]
            while ((cursor = strstr(cursor, "\"type\"")) != NULL) {

                // Leer el valor del "type": "XxxXxx"
                char *type_colon = strchr(cursor, ':');
                if (!type_colon) break;

                char effect_type[64] = {0};
                // Saltar espacios y la comilla de apertura
                char *type_start = strchr(type_colon + 1, '"');
                if (!type_start) break;
                type_start++; // saltar la "
                char *type_end = strchr(type_start, '"');
                if (!type_end) break;

                int type_len = type_end - type_start;
                if (type_len >= (int)sizeof(effect_type)) type_len = sizeof(effect_type) - 1;
                strncpy(effect_type, type_start, type_len);
                effect_type[type_len] = '\0';

                // Avanzar cursor para la proxima iteracion
                cursor = type_end + 1;

                // Buscar el bloque "params" a partir de aqui
                char *params_pos = strstr(cursor, "\"params\"");
                if (!params_pos) continue;

                // Encontrar la llave de apertura del objeto params
                char *params_open = strchr(params_pos, '{');
                if (!params_open) continue;

                // Encontrar la llave de cierre del objeto params
                char *params_close = strchr(params_open, '}');
                if (!params_close) continue;

                // Extraer el bloque params como substring temporal
                char params_block[512] = {0};
                int block_len = params_close - params_open + 1;
                if (block_len >= (int)sizeof(params_block)) block_len = sizeof(params_block) - 1;
                strncpy(params_block, params_open, block_len);

                printf("  Efecto detectado: [%s]\n", effect_type);

                // Buscar en la tabla todos los params de este efecto
                for (int m = 0; m < map_size; m++) {
                    if (strcmp(map[m].effect_key, effect_type) != 0) continue;

                    char *param_pos = strstr(params_block, map[m].param_key);
                    if (!param_pos) continue;

                    char *colon = strchr(param_pos, ':');
                    if (!colon) continue;

                    float raw_value = 0.0f;
                    if (sscanf(colon + 1, "%f", &raw_value) == 1) {
                        *map[m].target = raw_value * map[m].scale + map[m].offset;
                        printf("    %s = %f (raw: %f)\n",
                               map[m].param_key, *map[m].target, raw_value);
                    }
                }
            }

            memset(json_buffer, 0, sizeof(json_buffer));
        }

        float input = sinf(2.0f * PI * 440.0f * i / SAMPLE_RATE);
        i++;
        if (i >= SAMPLE_RATE)
            i = 0;

        // Cadena de efectos — comenta/descomenta segun necesites
        float od_out    = Overdrive_process(&od, input);
        float wah_out   = Wah_process(&wah, od_out);
        //float ch_out    = Chorus_process(&ch, od_out);
        //float fl_out    = Flanger_process(&flanger, od_out);
        //float pit_out   = PitchShifter_process(&pitch, od_out);
        //float del_out   = Delay_process(&delay, od_out);

        if (i % 3000 == 0) {
            printf("audio: input=%f od=%f\n", input, wah_out);
        }

        socket_send_two_floats(input, wah_out);
        usleep(22);
    }

    socket_close();
    return 0;
}