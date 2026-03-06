#ifndef SOCKET_SERVER_H
#define SOCKET_SERVER_H

int socket_init();
int socket_send_two_floats(float pre, float post);
int socket_receive(char *buffer, int max_len);
void socket_close();

#endif