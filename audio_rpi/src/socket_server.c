#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

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

int socket_receive(char *buffer, int max_len){
    int n = recv(client_fd, buffer, max_len -1, 0);

    if (n > 0){
        buffer[n] = '\0';
    }
    
    return n;
}

int socket_send_two_floats(float pre, float post) {
    float buffer[2] = { pre, post };
    return send(client_fd, buffer, sizeof(buffer), 0);
}

void socket_close() {
    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);
}