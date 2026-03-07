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
    // Formato: { "NOMBRE_EFECTO", "NOMBRE_PARAM", &struct.campo, escala, offset }
    // valor_final = valor_json * scale + offset
    ParamMap map[] = {
        // Overdrive
        { "OVERDRIVE", "GAIN",   &od.gain,   20.0f, 1.0f },
        { "OVERDRIVE", "TONE",   &od.tone,    1.0f, 0.0f },
        { "OVERDRIVE", "OUTPUT", &od.output,  1.0f, 0.0f },

        // Wah
        { "WAH", "FREQ",    &wah.freq,    1.0f, 0.0f },
        { "WAH", "Q",   &wah.q,   1.0f, 0.0f },
        { "WAH", "LEVEL",&wah.level, 1.0f, 0.0f },

        // Delay
        { "DELAY", "TIME",     &delay.delay_ms,     1.0f, 0.0f },
        { "DELAY", "FEEDBACK", &delay.feedback, 1.0f, 0.0f },
        { "DELAY", "MIX",      &delay.mix,      1.0f, 0.0f },

        // Chorus
        { "CHORUS", "RATE",  &ch.rate,  1.0f, 0.0f },
        { "CHORUS", "DEPTH", &ch.depth, 1.0f, 0.0f },
        { "CHORUS", "MIX",   &ch.mix,   1.0f, 0.0f },

        // Flanger
        { "FLANGER", "RATE",     &flanger.rate,     1.0f, 0.0f },
        { "FLANGER", "DEPTH",    &flanger.depth,    1.0f, 0.0f },
        { "FLANGER", "FEEDBACK", &flanger.feedback, 1.0f, 0.0f },
        { "FLANGER", "MIX",      &flanger.mix,      1.0f, 0.0f },

        // Pitch Shifter
        { "PITCH", "SEMITONES", &pitch.semitones, 1.0f, 0.0f },
        { "PITCH", "MIX",       &pitch.mix,       1.0f, 0.0f },
    };

    int map_size = sizeof(map) / sizeof(map[0]);

    socket_init();

    int i = 0;
    while (1) {
        int n = socket_receive(json_buffer, sizeof(json_buffer) - 1);

        if (n > 0) {
            printf("JSON recibido:\n%s\n", json_buffer);

            // Iterar sobre toda la tabla y aplicar los matches encontrados
            for (int m = 0; m < map_size; m++) {
                // Buscar el bloque del efecto
                char *effect_pos = strstr(json_buffer, map[m].effect_key);
                if (!effect_pos) continue;

                // Dentro del bloque del efecto, buscar el parametro
                char *param_pos = strstr(effect_pos, map[m].param_key);
                if (!param_pos) continue;

                // Avanzar hasta los ':' y leer el valor
                char *colon = strchr(param_pos, ':');
                if (!colon) continue;

                float raw_value = 0.0f;
                if (sscanf(colon + 1, "%f", &raw_value) == 1) {
                    *map[m].target = raw_value * map[m].scale + map[m].offset;
                    printf("  [%s] %s = %f (raw: %f)\n",
                           map[m].effect_key, map[m].param_key,
                           *map[m].target, raw_value);
                }
            }

            memset(json_buffer, 0, sizeof(json_buffer));
        }

        float input = sinf(2.0f * PI * 440.0f * i / SAMPLE_RATE);
        i++;
        if (i >= SAMPLE_RATE)
            i = 0;

        float od_out    = Overdrive_process(&od, input);
        //float wah_out   = Wah_process(&wah, od_out);
        //float ch_out    = Chorus_process(&ch, od_out);
        //float fl_out    = Flanger_process(&flanger, od_out);
        //float pit_out   = PitchShifter_process(&pitch, od_out);
        //float del_out   = Delay_process(&delay, od_out);

        if (i % 3000 == 0) {
            printf("audio: input=%f od=%f\n", input, od_out);
        }

        socket_send_two_floats(input, od_out);
        usleep(22);
    }

    socket_close();
    return 0;
}