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

// =============================================================================
// IDs de efectos
// Cada efecto tiene un numero unico que lo identifica en todo el sistema.
// Si agregas un efecto nuevo: añade su #define aqui e incrementa FX_COUNT.
// =============================================================================
#define FX_OVERDRIVE     0
#define FX_WAH           1
#define FX_DELAY         2
#define FX_CHORUS        3
#define FX_FLANGER       4
#define FX_PITCHSHIFTER  5
#define FX_COUNT         6

// =============================================================================
// Estado global de efectos
// enabled[]  -> 1 si el efecto esta activo en el preset actual, 0 si no
// fx_order[] -> lista de FX_IDs en el orden que deben procesarse
//               se llena leyendo el array "effects" del JSON en orden de llegada
// fx_order_count -> cuantos efectos activos hay en este preset
// =============================================================================
int enabled[FX_COUNT]      = {0};
int fx_order[FX_COUNT]     = {0};
int fx_order_count         =  0;

// =============================================================================
// ParamMap: tabla de mapeo efecto -> parametro -> campo del struct
//
// Cada fila conecta:
//   effect_key  -> el string que viene en "type" del JSON  (ej: "Overdrive")
//   param_key   -> el string que viene en "params"         (ej: "GAIN")
//   fx_id       -> el ID numerico del efecto               (ej: FX_OVERDRIVE)
//   target      -> puntero directo al campo float del struct que hay que modificar
//   scale       -> factor multiplicativo  (valor_final = raw * scale + offset)
//   offset      -> desplazamiento aditivo
//
// Esto permite que el loop de parseo sea 100% generico: no sabe nada de
// efectos concretos, solo itera esta tabla y escribe en memoria.
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
// process_effect: aplica un efecto por su ID y devuelve la senal procesada.
// Esta funcion es el unico lugar donde hay que agregar un case nuevo
// al incorporar un efecto nuevo.
// =============================================================================
float process_effect(int fx_id, float sig,
                     Overdrive *od, Wah *wah, Chorus *ch,
                     Flanger *flanger, PitchShifter *pitch, Delay *delay)
{
    switch (fx_id) {
        case FX_OVERDRIVE:    return Overdrive_process(od, sig);
        case FX_WAH:          return Wah_process(wah, sig);
        case FX_CHORUS:       return Chorus_process(ch, sig);
        case FX_FLANGER:      return Flanger_process(flanger, sig);
        case FX_PITCHSHIFTER: return PitchShifter_process(pitch, sig);
        case FX_DELAY:        return Delay_process(delay, sig);
        default:              return sig;  // efecto desconocido: bypass
    }
}

int main()
{
    // -------------------------------------------------------------------------
    // Inicializacion de efectos con valores default.
    // Estos valores suenan si el efecto esta activo pero aun no llego un JSON
    // con sus parametros (caso raro, pero defensivo).
    // -------------------------------------------------------------------------
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

    // -------------------------------------------------------------------------
    // Tabla de mapeo completa.
    // Para agregar un efecto nuevo solo hace falta:
    //   1. Definir su FX_ID arriba
    //   2. Agregar sus filas aqui
    //   3. Agregar su case en process_effect()
    // No hay que tocar nada mas.
    // -------------------------------------------------------------------------
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
    };

    int map_size = sizeof(map) / sizeof(map[0]);

    socket_init();

    int i = 0;
    while (1) {

        // ---------------------------------------------------------------------
        // BLOQUE DE RECEPCION Y PARSEO DEL JSON
        // socket_receive es no-bloqueante: devuelve 0 si no hay datos nuevos,
        // o N > 0 con los bytes recibidos. Solo entramos al parseo si hay JSON.
        // ---------------------------------------------------------------------
        int n = socket_receive(json_buffer, sizeof(json_buffer) - 1);

        if (n > 0) {
            printf("JSON recibido:\n%s\n", json_buffer);

            // Resetear estado completo del preset anterior.
            // Si no lo hacemos, efectos del preset viejo quedarian activos
            // aunque el nuevo preset no los incluya.
            memset(enabled,  0, sizeof(enabled));
            memset(fx_order, 0, sizeof(fx_order));
            fx_order_count = 0;

            char *cursor = json_buffer;

            // -----------------------------------------------------------------
            // Loop principal de parseo: busca cada ocurrencia de "type" en el
            // JSON. Cada "type" corresponde a un objeto dentro de effects[].
            // El orden en que aparecen en el JSON define el orden de la cadena.
            // -----------------------------------------------------------------
            while ((cursor = strstr(cursor, "\"type\"")) != NULL) {

                // --- Extraer el nombre del efecto ---
                // JSON: "type": "Overdrive"
                //                ^---------^ esto queremos
                char *type_colon = strchr(cursor, ':');
                if (!type_colon) break;
                char *type_start = strchr(type_colon + 1, '"');
                if (!type_start) break;
                type_start++;  // saltar la comilla de apertura
                char *type_end = strchr(type_start, '"');
                if (!type_end) break;

                char effect_type[64] = {0};
                int type_len = type_end - type_start;
                if (type_len >= (int)sizeof(effect_type)) type_len = sizeof(effect_type) - 1;
                strncpy(effect_type, type_start, type_len);

                // Avanzar cursor mas alla del type para la proxima iteracion
                cursor = type_end + 1;

                // --- Leer "enabled": true/false ---
                // Permite que Python mande un efecto desactivado sin quitarlo
                // del preset (util para bypass rapido desde la interfaz)
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

                // --- Extraer bloque "params": { ... } ---
                // Delimitamos el substring entre { y } para no contaminar
                // la busqueda de parametros con texto de otros efectos
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

                // --- Buscar en la tabla y aplicar parametros ---
                for (int m = 0; m < map_size; m++) {
                    if (strcmp(map[m].effect_key, effect_type) != 0) continue;

                    // Marcar el efecto como activo/inactivo y registrar su orden.
                    // Esto se hace en la primera fila que coincide con este efecto
                    // (todas las filas del mismo efecto tienen el mismo fx_id,
                    // pero solo queremos agregar el ID una vez a fx_order)
                    if (enabled[map[m].fx_id] == 0 && fx_order_count < FX_COUNT) {
                        if (fx_enabled) {
                            // Agregar al final de la cadena en el orden del JSON
                            fx_order[fx_order_count++] = map[m].fx_id;
                        }
                        enabled[map[m].fx_id] = fx_enabled;
                    }

                    // Parsear y escribir el valor del parametro
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

            // Imprimir la cadena resultante para debug
            printf("  Cadena de efectos (%d): ", fx_order_count);
            for (int k = 0; k < fx_order_count; k++) {
                printf("%d ", fx_order[k]);
            }
            printf("\n");

            memset(json_buffer, 0, sizeof(json_buffer));
        }

        // ---------------------------------------------------------------------
        // CADENA DE AUDIO DINAMICA
        //
        // fx_order[] tiene los IDs en el orden exacto que llego el JSON.
        // Ejemplo: si Python mando [Wah, Overdrive, Delay], entonces:
        //   sig = input
        //   sig = Wah_process(sig)       <- fx_order[0] = FX_WAH
        //   sig = Overdrive_process(sig) <- fx_order[1] = FX_OVERDRIVE
        //   sig = Delay_process(sig)     <- fx_order[2] = FX_DELAY
        //
        // process_effect() traduce el ID numerico a la llamada de funcion real.
        // ---------------------------------------------------------------------
        float input = sinf(2.0f * PI * 440.0f * i / SAMPLE_RATE);
        i++;
        if (i >= SAMPLE_RATE) i = 0;

        float sig = input;
        for (int k = 0; k < fx_order_count; k++) {
            sig = process_effect(fx_order[k], sig,
                                 &od, &wah, &ch, &flanger, &pitch, &delay);
        }

        if (i % 3000 == 0) {
            printf("audio: input=%f out=%f  cadena=%d efectos\n",
                   input, sig, fx_order_count);
        }

        socket_send_two_floats(input, sig);
        usleep(22);
    }

    socket_close();
    return 0;
}