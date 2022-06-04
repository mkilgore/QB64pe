
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <list>
#include <queue>
#include <unordered_map>
#include <curl/curl.h>

#include "mutex.h"
#include "condvar.h"
#include "thread.h"
#include "completion.h"
#include "buffer.h"
#include "http.h"

struct handle {
    int id;

    CURL *con;

    // Lock protects buffers and closed/err members
    struct libqb_mutex *io_lock;
    struct libqb_buffer out;
    int closed;
    int err;

    // FIXME: Sending input data (Ex. POST data) is not supported yet
    // struct block_list in;
};

/* used to signal to the main thread that a new connection is ready to be added */
struct add_handle {
    int handle;
    int err;

    struct completion added;
};

struct close_handle {
    int handle;

    struct completion closed;
};

struct curl_state {
    CURLM *multi;

    // Lock protects the handle table, related queues, and 'stop_curl' flag
    struct libqb_mutex *lock;
    struct std::unordered_map<int, struct handle *> handle_table;
    std::queue<struct add_handle *> add_handle_queue;
    std::queue<struct close_handle *> close_handle_queue;
    int stop_curl;

    curl_state() {
        lock = libqb_mutex_new();
    }
};

// Processes the handle addition and deletion lists
static void process_handles(struct curl_state *state) {
    libqb_mutex_guard guard(state->lock);

    for (; !state->add_handle_queue.empty(); state->add_handle_queue.pop()) {
        struct add_handle *add = state->add_handle_queue.front();

        curl_multi_add_handle(state->multi, state->handle_table[add->handle]->con);

        add->err = 0;
        completion_finish(&add->added);
    }

    for (; !state->close_handle_queue.empty(); state->close_handle_queue.pop()) {
        struct close_handle *close = state->close_handle_queue.front();
        struct handle *handle = state->handle_table[close->handle];

        // If this was already finished, then con will be NULL
        if (handle->con) {
            // Note: This can call the callbacks.
            curl_multi_remove_handle(state->multi, handle->con);
            curl_easy_cleanup(handle->con);
        }

        state->handle_table.erase(close->handle);

        // Lock is unnecessary, nobody will be accessing this thing
        libqb_buffer_clear(&handle->out);
        libqb_mutex_free(handle->io_lock);
        free(handle);

        completion_finish(&close->closed);
    }
}

static void handle_messages(struct curl_state *state) {
    CURLMsg *msg;
    int left;

    while (msg = curl_multi_info_read(state->multi, &left), msg) {
        if (msg->msg != CURLMSG_DONE)
            continue;

        CURL *e = msg->easy_handle;

        struct handle *handle;
        curl_easy_getinfo(e, CURLINFO_PRIVATE, &handle);

        handle->con = NULL;
        {
            libqb_mutex_guard guard(handle->io_lock);

            handle->closed = 1;
            switch (msg->data.result) {
            case CURLE_OK:
                break;

            default:
                handle->err = 1;
                break;
            }
        }

        curl_multi_remove_handle(state->multi, e);
        curl_easy_cleanup(e);
    }
}

#if CURL_AT_LEAST_VERSION(7, 68, 0)
static void curl_state_poll(struct curl_state *state) {
    // We keep the timeout at 1s, 
    curl_multi_poll(state->multi, NULL, 0, 1000, NULL);
}

static void curl_state_wakeup(struct curl_state *state) {
    curl_multi_wakeup(state->multi);
}
#else
// This is a workaround for libcurl version lacking the curl_multi_poll() and curl_multi_wakeup() functions.
// Unfortunately this old version is on OS X, so we need to support it
//
// We use curl_multi_wait() with a small timeout, and don't support wakeup (so commands have to wait for the timeout to trigger
//
// If numfds from curl_multi_wait() is zero, then we have to do the timeout manually
static void curl_state_poll(struct curl_state *state) {
    int numfds = 0;
    // Shorter 100ms
    curl_multi_wait(state->multi, NULL, 0, 100, &numfds);

    if (!numfds)
        usleep(100 * 1000);
}

static void curl_state_wakeup(struct curl_state *state) {
    // NOP, curl_state_poll will timeout automatically
}
#endif

static void libqb_curl_thread_handler(void *arg) {
    struct curl_state *state = (struct curl_state *)arg;
    int running_transfers = 0;

    while (!state->stop_curl) {
        curl_state_poll(state);

        // Process handle additions and calls to close()
        process_handles(state);

        // Process requests, performs any read/write operations
        curl_multi_perform(state->multi, &running_transfers);

        // Handle any requests that finished
        handle_messages(state);
    }

    // FIXME: Close connections, don't just kill everything
}

static struct curl_state curl_state;
static struct libqb_thread *curl_thread;

static size_t receive_http_block(void *ptr, size_t size, size_t nmemb, void *data) {
    struct handle *handle = (struct handle *)data;
    size_t length = size * nmemb;

    libqb_mutex_guard guard(handle->io_lock);

    libqb_buffer_write(&handle->out, (const char *)ptr, length);

    return size * nmemb;
}

int libqb_http_get_length(int id, size_t *length)
{
    if (curl_state.handle_table.find(id) == curl_state.handle_table.end())
        return -1;

    struct handle *handle = curl_state.handle_table[id];

    libqb_mutex_guard guard(handle->io_lock);

    *length = libqb_buffer_length(&handle->out);

    return 0;
}

int libqb_http_get(int id, char *buf, size_t *length) {
    if (curl_state.handle_table.find(id) == curl_state.handle_table.end())
        return -1;

    struct handle *handle = curl_state.handle_table[id];

    libqb_mutex_guard guard(handle->io_lock);

    *length = libqb_buffer_read(&handle->out, buf, *length);

    return 0;
}

int libqb_http_connected(int id)
{
    if (curl_state.handle_table.find(id) == curl_state.handle_table.end())
        return -1;

    struct handle *handle = curl_state.handle_table[id];

    libqb_mutex_guard guard(handle->io_lock);

    return !handle->closed;
}

int libqb_http_open(const char *url, int id) {
    struct handle *handle = (struct handle *)malloc(sizeof(*handle));
    memset(handle, 0, sizeof(*handle));

    handle->id = id;
    handle->io_lock = libqb_mutex_new();
    libqb_buffer_init(&handle->out);

    handle->con = curl_easy_init();
    curl_easy_setopt(handle->con, CURLOPT_PRIVATE, handle);
    curl_easy_setopt(handle->con, CURLOPT_URL, url);
    curl_easy_setopt(handle->con, CURLOPT_WRITEFUNCTION, receive_http_block);
    curl_easy_setopt(handle->con, CURLOPT_WRITEDATA, handle);

    struct add_handle add;

    add.handle = id;
    completion_init(&add.added);

    {
        libqb_mutex_guard guard(curl_state.lock);

        curl_state.handle_table.insert(std::make_pair(id, handle));
        curl_state.add_handle_queue.push(&add);
    }

    // Kick the curl thread so that it processes our new handle
    curl_state_wakeup(&curl_state);

    completion_wait(&add.added);
    completion_clear(&add.added);

    return 0;
}

int libqb_http_close(int id) {
    // Check to make sure this handle is valid
    if (curl_state.handle_table.find(id) == curl_state.handle_table.end())
        return -1;

    struct close_handle close;

    close.handle = id;
    completion_init(&close.closed);

    {
        libqb_mutex_guard guard(curl_state.lock);

        curl_state.close_handle_queue.push(&close);
    }

    // Kick the curl thread so that it processes our close request
    curl_state_wakeup(&curl_state);

    completion_wait(&close.closed);
    completion_clear(&close.closed);

    return 0;
}

void libqb_http_init() {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    curl_state.multi = curl_multi_init();

    curl_thread = libqb_thread_new();
    libqb_thread_start(curl_thread, libqb_curl_thread_handler, &curl_state);
}

void libqb_http_stop() {
    {
        libqb_mutex_guard guard(curl_state.lock);

        // Tell CURL to finish up requests
        curl_state.stop_curl = 1;
    }

    curl_state_wakeup(&curl_state);

    // Wait for curl to finish
    libqb_thread_join(curl_thread);

    libqb_thread_free(curl_thread);
}
