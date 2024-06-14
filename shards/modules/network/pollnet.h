#ifndef B5DD25D9_8885_4263_B18F_B6F810D95746
#define B5DD25D9_8885_4263_B18F_B6F810D95746

#include <stdint.h>
#include <stdbool.h>
#include <shards/shards.h>

typedef struct pollnet_ctx pollnet_ctx;
typedef uint64_t sockethandle_t;
typedef uint32_t socketstatus_t;

/*
 * Result codes
 */

// Handle is invalid or doesn't correspond to a socket
#define POLLNET_INVALID 0
// Socket has had an error
#define POLLNET_ERROR 1
// Socket is closed
#define POLLNET_CLOSED 2
// Socket is the process of opening
#define POLLNET_OPENING 3
// Socket is open but has no data right now
#define POLLNET_OPEN_NODATA 4
// Socket is open and has data available
#define POLLNET_OPEN_HASDATA 5
// Socket is open and has a new client connection
#define POLLNET_OPEN_NEWCLIENT 6

/*
 * Get pollnet version as a MAJOR.MINOR.PATCH semver string
 * e.g., 1.0.0
 */
const char *pollnet_version();

/*
 * Returns whether a handle is valid.
 * Equivalent to comparing against an invalid handle.
 */
bool pollnet_handle_is_valid(sockethandle_t handle);

/*
 * Returns an invalid handle (i.e., the specific uint64 that means 'invalid').
 * Handles can be compared to an invalid handle produced this way to test
 * if they are valid, as an alternative to calling `pollnet_handle_is_valid`.
 */
sockethandle_t pollnet_invalid_handle();

/*
 * Creates a new pollnet context.
 * This is the best way to get a context.
 */
pollnet_ctx *pollnet_init();

/*
 * Shuts down a context: all open sockets and servers are closed.
 */
void pollnet_shutdown(pollnet_ctx *ctx);

/*
 * Open a Websocket connection to a given url.
 * url: a websocket endpoint url as a string,
 *      e.g. "ws://localhost:9090"
 * Secure websockets are supported; use "wss://".
 */
sockethandle_t pollnet_open_ws(pollnet_ctx *ctx, SHStringWithLen url);

/*
 * Close a socket. After closing, the handle can no longer be used.
 */
void pollnet_close(pollnet_ctx *ctx, sockethandle_t handle);

/*
 * Close all open sockets.
 */
void pollnet_close_all(pollnet_ctx *ctx);

/*
 * Send a null terminated, utf8 C string to a socket.
 */
void pollnet_send(pollnet_ctx *ctx, sockethandle_t handle, SHStringWithLen msg);

/*
 * Send binary data to a socket.
 */
void pollnet_send_binary(pollnet_ctx *ctx, sockethandle_t handle, const unsigned char *msg, uint32_t msgsize);

/*
 * Poll a socket for updates. Some status codes indicate that additional
 * data can be queried:
 *   POLLNET_HASDATA: get data with `pollnet_get(ctx, this, ...)`
 *   POLLNET_ERROR: get error message with `pollnet_get_error(ctx, this, ...)`
 *   POLLNET_NEWCLIENT:
 *     get client address string with
 *        `pollnet_get(ctx, this, ...)`
 *     get client socket with
 *        `pollnet_get_connected_client_handle(ctx, this, ...)`
 *
 * A result of POLLNET_CLOSED or POLLNET_ERROR indicates the socket is dead,
 * and `pollnet_close(ctx, this)` should be called to release the handle.
 */
socketstatus_t pollnet_update(pollnet_ctx *ctx, sockethandle_t handle);

/*
 * Polls a socket, blocking until something happens.
 * i.e., blocks instead of returning POLLNET_NODATA.
 */
socketstatus_t pollnet_update_blocking(pollnet_ctx *ctx, sockethandle_t handle);

/*
 * Get the size of the data/message (in bytes) received on a socket.
 * Returns 0 if no data, or the provided socket handle is invalid.
 */
uint32_t pollnet_get_data_size(pollnet_ctx *ctx, sockethandle_t handle);

/*
 * Gets the message/data received by a socket during the last call to
 * `pollnet_update`. Copies the data into the provided buffer and returns
 * the number of bytes copied.
 *
 * If the received message is too large to fit in the destination,
 * then the full size of the message is returned but no bytes are
 * actually copied.
 */
uint32_t pollnet_get_data(pollnet_ctx *ctx, sockethandle_t handle, char *dest, uint32_t dest_size);

/*
 * Get a pointer directly to the internal data buffer for a socket. Use
 * `pollnet_get_data_size` to know how large the data is.
 * Returns null if no data or socket invalid.
 *
 * WARNING: the returned pointer is only valid until another function
 * is called on the given socket (including close or close_all!).
 *
 * To safely use this function you should either copy the data yourself
 * somewhere, or complete all processing on the data before calling any
 * other pollnet functions.
 */
const uint8_t *pollnet_unsafe_get_data_ptr(pollnet_ctx *ctx, sockethandle_t handle);

/*
 * Clears the data message on a given socket.
 *
 * There is typically no reason to manually call this, since
 * updating a socket will automatically free a previous message
 * when a new one comes in.
 */
void pollnet_clear_data(pollnet_ctx *ctx, sockethandle_t handle);

/*
 * Gets the socket associated with a new client, as indicated by receiving
 * POLLNET_NEWCLIENT on an update to a server socket.
 */
sockethandle_t pollnet_get_connected_client_handle(pollnet_ctx *ctx, sockethandle_t handle);

/*
 * Open a websocket server on the given TCP address and port.
 * Note! This is a TCP address like "0.0.0.0:9090" and NOT a URL.
 */
sockethandle_t pollnet_listen_ws(pollnet_ctx *ctx, SHStringWithLen addr);


#endif /* B5DD25D9_8885_4263_B18F_B6F810D95746 */
