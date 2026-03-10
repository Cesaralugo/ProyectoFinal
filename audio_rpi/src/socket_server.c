#include "socket_server.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define SOCKET_PATH "/tmp/audio_socket"

static int server_fd;
static int client_fd;

int socket_init() {
    struct sockaddr_un addr;

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    unlink(SOCKET_PATH);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 1);

    printf("Esperando cliente...\n");
    client_fd = accept(server_fd, NULL, NULL);
    printf("Cliente conectado\n");

    return 0;
}

int socket_receive(char *buffer, int max_len) {
    int n = recv(client_fd, buffer, max_len - 1, MSG_DONTWAIT);
    if (n > 0)
        buffer[n] = '\0';
    return n;
}

// Legacy — manda un solo par (evitar en el loop de audio)
int socket_send_two_floats(float pre, float post) {
    float buf[2] = { pre, post };
    return send(client_fd, buf, sizeof(buf), 0);
}

// Nuevo — manda n pares de golpe: [pre0, post0, pre1, post1, ..., preN, postN]
// pre  : array de n floats (señal antes del efecto)
// post : array de n floats (señal después del efecto)
// n    : número de samples (ej: 128)
int socket_send_batch(const float *pre, const float *post, int n) {
    // Intercalar pre/post en un buffer contiguo para un solo send()
    float *interleaved = malloc(n * 2 * sizeof(float));
    if (!interleaved) return -1;

    for (int i = 0; i < n; i++) {
        interleaved[i * 2]     = pre[i];
        interleaved[i * 2 + 1] = post[i];
    }

    int total = n * 2 * sizeof(float);
    int sent  = send(client_fd, interleaved, total, 0);
    free(interleaved);
    return sent;
}

void socket_close() {
    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);
}