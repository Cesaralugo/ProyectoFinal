#ifndef SERIAL_INPUT_H
#define SERIAL_INPUT_H

#include <stdint.h>

// Debe coincidir con PACKET_SAMPLES del sketch Arduino
#define SERIAL_PACKET_SAMPLES  128

// Busca el primer puerto ttyUSB* / ttyACM* disponible.
// Devuelve el path (ej: "/dev/ttyUSB1") o NULL si no hay ninguno.
const char *serial_autodetect(void);

// Abre el puerto serial. Si port == NULL, autodetecta.
// Devuelve el fd o -1 en error.
int  serial_open(const char *port, int baud);

// Cierra el puerto serial.
void serial_close(int fd);

// Bloquea hasta leer un paquete completo con sync word valida.
// out_samples: buffer de SERIAL_PACKET_SAMPLES uint16_t (codigos ADC crudos).
// Devuelve 0 OK, -1 error/timeout.
int  serial_read_packet(int fd, uint16_t *out_samples);

// Convierte un codigo ADC ESP32 (12-bit, 0-4095) a float normalizado [-1.0, 1.0].
// Midpoint = 2048 (señal AC centrada en Vref/2).
static inline float serial_adc_to_float(uint16_t raw)
{
    return (raw - 2048.0f) / 2048.0f;
}

#endif // SERIAL_INPUT_H
