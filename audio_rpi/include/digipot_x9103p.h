#ifndef DIGIPOT_X9103P_H
#define DIGIPOT_X9103P_H

#include <stdint.h>

/*
 * digipot_x9103p.h - Driver for X9103P digital potentiometer.
 *
 * Communication is done over a serial COM port connected to a USB-to-SPI
 * adapter (e.g. Arduino/Teensy running a simple bridge sketch).
 *
 * Frame format sent to the adapter: [0xD0, wiper_position]
 *   0xD0  - command byte (set wiper)
 *   wiper_position - 0 (minimum) to 255 (maximum)
 *
 * If digipot_init() is not called (or fails), digipot_set_wiper() still
 * tracks the requested position in software without producing an error,
 * so the rest of the audio engine continues to work without hardware.
 */

/* Initialize serial port to the adapter.
 * device_path: COM port string, e.g. "COM3".  Pass NULL to use the
 *              compile-time default (DIGIPOT_DEFAULT_PORT).
 * Returns 0 on success, -1 on failure (missing adapter is non-fatal). */
int digipot_init(const char *device_path);

/* Set wiper position (0 = min, 255 = max).
 * Returns 0 on success, -1 on write error. */
int digipot_set_wiper(uint8_t position);

/* Return the last wiper position sent (or 0 if never set). */
uint8_t digipot_get_wiper(void);

/* Release the serial port handle. */
void digipot_close(void);

#endif /* DIGIPOT_X9103P_H */
