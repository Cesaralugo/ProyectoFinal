/*
 * effect_manager.c - Effect chain management and JSON serialisation.
 */

#include "effect_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ── Name table ─────────────────────────────────────────────────────────── */
const char *eff_names[EFF_COUNT] = {
    "delay",
    "overdrive",
    "wah",
    "chorus",
    "flanger",
    "pitch_shifter",
    "phaser",
    "reverb"
};

/* ── Initialise ─────────────────────────────────────────────────────────── */
void em_init(eff_state_t *s) {
    memset(s, 0, sizeof(*s));
    s->master_gain = 1.0f;
}

/* ── Chain manipulation ─────────────────────────────────────────────────── */

int em_add(eff_state_t *s, eff_id_t id) {
    if (s->count >= EM_MAX_EFFECTS) return -1;
    s->chain[s->count].id      = id;
    s->chain[s->count].enabled = 1;
    s->count++;
    return s->count - 1;
}

int em_remove(eff_state_t *s, int index) {
    if (index < 0 || index >= s->count) return -1;
    for (int i = index; i < s->count - 1; i++)
        s->chain[i] = s->chain[i + 1];
    s->count--;
    return 0;
}

int em_move(eff_state_t *s, int from, int to) {
    if (from < 0 || from >= s->count) return -1;
    if (to   < 0 || to   >= s->count) return -1;
    if (from == to) return 0;

    eff_slot_t tmp = s->chain[from];
    int step = (to > from) ? 1 : -1;
    for (int i = from; i != to; i += step)
        s->chain[i] = s->chain[i + step];
    s->chain[to] = tmp;
    return 0;
}

int em_toggle(eff_state_t *s, int index) {
    if (index < 0 || index >= s->count) return -1;
    s->chain[index].enabled ^= 1;
    return s->chain[index].enabled;
}

void em_set_gain(eff_state_t *s, float gain) {
    s->master_gain = gain;
}

/* ── JSON serialisation ─────────────────────────────────────────────────── */

/*
 * Produces JSON that matches what the Python GUI sends, e.g.:
 * {
 *   "master_gain": 1.0,
 *   "effects": [
 *     {"name": "delay",     "enabled": true},
 *     {"name": "overdrive", "enabled": false}
 *   ]
 * }
 */
int em_to_json(const eff_state_t *s, char *buf, int buf_len) {
    int pos = 0;

#define APPEND(...) \
    do { \
        int _n = snprintf(buf + pos, buf_len - pos, __VA_ARGS__); \
        if (_n < 0 || _n >= buf_len - pos) return -1; \
        pos += _n; \
    } while (0)

    APPEND("{\n  \"master_gain\": %.4f,\n  \"effects\": [", s->master_gain);

    for (int i = 0; i < s->count; i++) {
        const char *name    = eff_names[s->chain[i].id];
        const char *enabled = s->chain[i].enabled ? "true" : "false";
        if (i > 0) APPEND(",");
        APPEND("\n    {\"name\": \"%s\", \"enabled\": %s}", name, enabled);
    }

    APPEND("\n  ]\n}\n");

#undef APPEND
    return pos;
}
