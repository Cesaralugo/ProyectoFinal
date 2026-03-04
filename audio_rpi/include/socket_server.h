#ifndef SOCKET_SERVER_H
#define SOCKET_SERVER_H

int socket_init();
int socket_send_two_floats(float pre, float post);
void socket_close();

#endif