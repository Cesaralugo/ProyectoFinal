#ifndef EFFECT_MANAGER_H
#define EFFECT_MANAGER_H

/*
 * effect_manager.h - Effect chain add/remove/reorder with JSON serialisation.
 *
 * Mirrors the Python GUI's effect model so that the same JSON format is sent
 * to the audio engine regardless of which front-end is in use.
 */

#include <stdint.h>

/* Maximum number of simultaneously loaded effects */
#define EM_MAX_EFFECTS 16

/* Effect identifiers (must match audio engine JSON keys) */
typedef enum {
    EFF_DELAY      = 0,
    EFF_OVERDRIVE,
    EFF_WAH,
    EFF_CHORUS,
    EFF_FLANGER,
    EFF_PITCH,
    EFF_PHASER,
    EFF_REVERB,
    EFF_DISTORTION,
    EFF_COUNT       /* sentinel – keep last */
} eff_id_t;

/* Human-readable name for each effect (used in list-box display) */
extern const char *eff_names[EFF_COUNT];

/* One slot in the active effect chain */
typedef struct {
    eff_id_t id;
    int      enabled;   /* 1 = active, 0 = bypassed */
} eff_slot_t;

/* Module state – treat as opaque; access via API below */
typedef struct {
    eff_slot_t chain[EM_MAX_EFFECTS];
    int        count;
    float      master_gain;
} eff_state_t;

/* ── API ─────────────────────────────────────────────────────────────────── */

/* Initialise state with all effects off and master gain = 1.0 */
void em_init(eff_state_t *s);

/* Append effect to end of chain.  Returns -1 if chain is full. */
int  em_add(eff_state_t *s, eff_id_t id);

/* Remove the effect at position `index`.  Returns -1 if out of range. */
int  em_remove(eff_state_t *s, int index);

/* Move effect from `from` to `to` position.  Returns -1 on bad indices. */
int  em_move(eff_state_t *s, int from, int to);

/* Toggle enabled flag for effect at `index`.  Returns new flag value or -1. */
int  em_toggle(eff_state_t *s, int index);

/* Set master gain (0.0 – 2.0 recommended). */
void em_set_gain(eff_state_t *s, float gain);

/*
 * Serialise the current chain to a JSON string.
 * The caller provides `buf` of `buf_len` bytes.
 * Returns the number of bytes written (excluding null terminator),
 * or -1 if the buffer is too small.
 */
int  em_to_json(const eff_state_t *s, char *buf, int buf_len);

#endif /* EFFECT_MANAGER_H */
