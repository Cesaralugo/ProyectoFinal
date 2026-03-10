#ifndef SOCKET_SERVER_H
#define SOCKET_SERVER_H

#include <stdint.h>

int  socket_init(void);
int  socket_receive(char *buffer, int max_len);
int  socket_send_two_floats(float pre, float post);          // legacy (no usar)
int  socket_send_batch(const float *pre, const float *post, int n); // nuevo
void socket_close(void);

#endif