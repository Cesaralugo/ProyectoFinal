#ifndef SOCKET_CLIENT_H
#define SOCKET_CLIENT_H

/*
 * socket_client.h - Non-blocking TCP client for the C GUI.
 *
 * Connects to the audio engine on TCP port 54321.
 * Receiving (pre/post batch data) is done asynchronously via a background
 * thread so that the Win32 message pump never blocks.
 * Sending (effect JSON) is fire-and-forget from the UI thread.
 */

#include <winsock2.h>
#include <stdint.h>

/* Number of float *pairs* per batch (must match audio engine config) */
#define SC_PACKET_SAMPLES  128
#define SC_TCP_PORT        54321

/*
 * Callback invoked on the receiver thread each time a full batch arrives.
 * pre  - SC_PACKET_SAMPLES floats captured before the effect chain
 * post - SC_PACKET_SAMPLES floats captured after  the effect chain
 * user - opaque pointer passed to sc_start()
 */
typedef void (*sc_batch_cb)(const float *pre, const float *post,
                             int n, void *user);

/* Initialise Winsock and connect.  Returns 0 on success, -1 on error. */
int  sc_connect(const char *host, int port);

/*
 * Start the background receiver thread.
 * cb   - function called for every complete batch
 * user - opaque context forwarded to cb
 * Returns 0 on success.
 */
int  sc_start(sc_batch_cb cb, void *user);

/* Send a null-terminated JSON string to the audio engine (non-blocking). */
int  sc_send_json(const char *json);

/* Stop the receiver thread and close the socket. */
void sc_disconnect(void);

/* True if the socket is currently connected. */
int  sc_is_connected(void);

#endif /* SOCKET_CLIENT_H */
