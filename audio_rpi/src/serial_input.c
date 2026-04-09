#include "serial_input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
//#include <termios.h>
#include <unistd.h>
#include <errno.h>

// ─── Protocolo (debe coincidir con el sketch Arduino y el monitor Python) ────
static const uint8_t SYNC_WORD[4] = {0xAA, 0x55, 0xFF, 0x00};

// ─── Mapeo de baud rate a constante POSIX ────────────────────────────────────
static speed_t baud_to_speed(int baud)
{
    switch (baud) {
        case 9600:    return B9600;
        case 19200:   return B19200;
        case 38400:   return B38400;
        case 57600:   return B57600;
        case 115200:  return B115200;
        case 230400:  return B230400;
        case 460800:  return B460800;
        case 500000:  return B500000;
        case 1000000: return B1000000;
        case 2000000: return B2000000;
        default:
            fprintf(stderr, "[serial] baud %d no soportado, usando 460800\n", baud);
            return B460800;
    }
}

// ─── Autodetectar puerto serial ──────────────────────────────────────────────
// Prueba los candidatos en orden y devuelve el primero que abre correctamente.
// Devuelve un puntero estático válido hasta la próxima llamada, o NULL si no
// encuentra ninguno.
const char *serial_autodetect(void)
{
    static const char *candidates[] = {
        "/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyUSB2",
        "/dev/ttyACM0", "/dev/ttyACM1", "/dev/ttyACM2",
        NULL
    };
    for (int i = 0; candidates[i] != NULL; i++) {
        int fd = open(candidates[i], O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd >= 0) {
            close(fd);
            printf("[serial] autodetectado: %s\n", candidates[i]);
            return candidates[i];
        }
    }
    fprintf(stderr, "[serial] no se encontro ningun puerto serial\n");
    return NULL;
}

// ─── Abrir y configurar el puerto ────────────────────────────────────────────
// Si port == NULL, autodetecta.
int serial_open(const char *port, int baud)
{
    if (port == NULL) {
        port = serial_autodetect();
        if (port == NULL) return -1;
    }

    int fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "[serial] no se pudo abrir %s: %s\n", port, strerror(errno));
        return -1;
    }

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        fprintf(stderr, "[serial] tcgetattr: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    speed_t spd = baud_to_speed(baud);
    cfsetispeed(&tty, spd);
    cfsetospeed(&tty, spd);

    // 8N1, sin control de flujo, modo raw
    cfmakeraw(&tty);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSTOPB;        // 1 stop bit
    tty.c_cflag &= ~CRTSCTS;       // sin HW flow control

    // Timeout de lectura: 2 segundos
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 20;          // unidades de 100 ms → 2 s

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        fprintf(stderr, "[serial] tcsetattr: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    // Vaciar buffer de entrada antes de empezar
    usleep(100000);   // 100 ms: el ESP32 puede estar enviando basura al conectar
    tcflush(fd, TCIFLUSH);

    printf("[serial] abierto %s @ %d baud\n", port, baud);
    return fd;
}

void serial_close(int fd)
{
    if (fd >= 0) close(fd);
}

// ─── Leer exactamente n bytes (bloqueante con timeout) ───────────────────────
static int read_exact(int fd, uint8_t *buf, int n)
{
    int total = 0;
    while (total < n) {
        int r = read(fd, buf + total, n - total);
        if (r <= 0) return -1;   // timeout o error
        total += r;
    }
    return 0;
}

// ─── Buscar sync word con ventana deslizante ─────────────────────────────────
static int find_sync(int fd)
{
    uint8_t window[4] = {0};
    // Intentamos hasta 8 * PACKET_PAYLOAD bytes antes de rendirse
    int max_tries = SERIAL_PACKET_SAMPLES * 8 * 2;
    for (int t = 0; t < max_tries; t++) {
        uint8_t b;
        if (read(fd, &b, 1) != 1) return -1;
        // Desplazar ventana
        window[0] = window[1];
        window[1] = window[2];
        window[2] = window[3];
        window[3] = b;
        if (memcmp(window, SYNC_WORD, 4) == 0) return 0;
    }
    fprintf(stderr, "[serial] sync no encontrado tras %d bytes\n", max_tries);
    return -1;
}

// ─── Leer un paquete completo ─────────────────────────────────────────────────
int serial_read_packet(int fd, uint16_t *out_samples)
{
    if (find_sync(fd) < 0) return -1;

    uint8_t raw[SERIAL_PACKET_SAMPLES * 2];
    if (read_exact(fd, raw, sizeof(raw)) < 0) {
        fprintf(stderr, "[serial] timeout leyendo payload\n");
        return -1;
    }

    // Decodificar little-endian → uint16
    for (int i = 0; i < SERIAL_PACKET_SAMPLES; i++) {
        out_samples[i] = (uint16_t)(raw[i * 2]) | ((uint16_t)(raw[i * 2 + 1]) << 8);
    }
    return 0;
}
