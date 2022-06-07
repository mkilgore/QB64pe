#ifndef INCLUDE_LIBQB_HTTP_H
#define INCLUDE_LIBQB_HTTP_H

#ifdef DEPENDENCY_HTTP

// Initialize the HTTP system
void libqb_http_init();

// Handle is provided and should be unique. Used to identify this connection
int libqb_http_open(const char *url, int handle);
int libqb_http_close(int handle);

int libqb_http_connected(int handle);

// Get length of bytes waiting to be read.
//
// Note that more bytes may come in after calling function, but you're guarenteed to at least have this many bytes
int libqb_http_get_length(int handle, size_t *length);

// Reads up to length bytes into buf. Length is modified if less bytes than requested are returned
int libqb_http_get(int handle, char *buf, size_t *length);

// FIXME: does not work
int libqb_http_put(int handle, const char *buffer, size_t length);

#else

static inline void libqb_http_init() { }
static inline int libqb_http_open(const char *url, int handle) { return -1; }
static inline int libqb_http_close(int handle) { return -1; }
static inline int libqb_http_connected(int handle) { return -1; }
static inline int libqb_http_get_length(int handle, size_t *length) { return -1; }
static inline int libqb_http_get(int handle, char *buf, size_t *length) { return -1; }
static inline int libqb_http_put(int handle, const char *buffer, size_t length) { return -1; }

#endif

#endif
