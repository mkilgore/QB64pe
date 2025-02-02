
#include "libqb-common.h"

#include <list>
#include <queue>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unordered_map>

#ifdef QB64_UNIX
# include <sys/types.h>
# include <sys/wait.h>
# include <signal.h>
# include <errno.h>
# include <dirent.h>
# include <fcntl.h>
#else
# include <windows.h>
# include <processthreadsapi.h>
#endif

#include "buffer.h"
#include "completion.h"
#include "condvar.h"
#include "http.h"
#include "mutex.h"
#include "thread.h"
#include "stream.h"
#include "logging.h"
#include "shellstream.h"

#ifdef QB64_UNIX

# ifdef QB64_MACOSX
#  define PID_DIR "/dev/fd"
# else
#  define PID_DIR "/proc/self/fd"
# endif

static void set_nonblock(int fd)
{
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

static void close_all_fds_above(int lowest)
{
    DIR *d = opendir(PID_DIR);

    // If the directory doesn't open then we give up
    if (d == NULL)
        return;

    struct dirent *dp;
    while ((dp = readdir(d)) != NULL) {
        int fd = atoi(dp->d_name);

        if (fd >= lowest)
            ::close(fd);
    }
    closedir(d);
}
#endif

class proc_stream;

static std::unordered_map<int32_t, class proc_stream *> handle_table;

class stream_handle_in final : public special_handle {
  public:
    bool enabled;
    bool closed;
    struct libqb_buffer buf;

#ifdef QB64_UNIX
    int fd, childfd;
#else
    HANDLE fd, childfd;
#endif

  private:
    void fill_buffer()
    {
        if (closed)
            return;

#ifdef QB64_UNIX
        char b[4096];
        ssize_t len;
        while (1) {
            len = read(fd, (void *)b, sizeof(b));

            if (len == 0) {
                ::close(fd);
                closed = true;
                return;
            }

            // Indicates no data ready to read
            if (len == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Pipe still open, just no data right now
                } else if (errno == EPIPE) {
                    ::close(fd);
                    closed = true;
                } else {
                    libqb_log_warn("Unexpected pipe error: %d: %s", errno, strerror(errno));
                }

                return;
            }

            libqb_buffer_write(&buf, (const char *)b, len);
        }
#else
        while (1) {
            char b[4096];
            DWORD bytesAvail = 0;
            DWORD bytesRead = 0;
            BOOL success = PeekNamedPipe(fd, NULL, 0, NULL, &bytesAvail, NULL);

            if (!success) {
                DWORD err = GetLastError();
                if (err != ERROR_BROKEN_PIPE && err != ERROR_PIPE_NOT_CONNECTED)
                    libqb_log_warn("Unexpected pipe error for handle %d: %d", id, err);

                CloseHandle(fd);
                closed = true;
                return;
            }

            if (!bytesAvail)
                return;

            success = ReadFile(fd, (void *)b, sizeof(b) > bytesAvail? sizeof(b): bytesAvail, &bytesRead, NULL);
            if (success)
                libqb_buffer_write(&buf, (const char *)b, bytesRead);
        }
#endif
    }

  public:
    stream_handle_in() {
        libqb_buffer_init(&buf);
    }

    void close() override
    {
        if (!closed)
#ifdef QB64_UNIX
            ::close(fd);
#else
            CloseHandle(fd);
#endif

        libqb_buffer_clear(&buf);
        enabled = false;
    }

    void get2(int64_t offset, qbs *retstr, int passed) override
    {
        (void)offset;

        libqb_log_info("Entered get2 override!");
        // Offset is not supported
        if (passed) {
            error(QB_ERROR_BAD_FILE_NAME_OR_NUMBER);
            return;
        }

        fill_buffer();

        qbs_set(retstr, qbs_new(libqb_buffer_length(&buf), 1));
        libqb_buffer_read(&buf, (char *)retstr->chr, retstr->len);
    }

    int32_t eof() override
    {
        fill_buffer();

        libqb_log_info("Closed: %d, length: %d, equal: %d\n", closed, libqb_buffer_length(&buf), closed && libqb_buffer_length(&buf) == 0);
        // EOF() true indicates there's still potential data to read
        return TO_QB_BOOL(closed && libqb_buffer_length(&buf) == 0);
    }

    int32_t connected() override
    {
        return TO_QB_BOOL(closed);
    }
};

class stream_handle_out final : public special_handle {
  public:
    bool enabled;
    bool closed;
#ifdef QB64_UNIX
    int fd, childfd;
#else
    HANDLE fd, childfd;
#endif

    stream_handle_out() {
    }

    void close() override {
        if (!closed)
#ifdef QB64_UNIX
            ::close(fd);
#else
            CloseHandle(fd);
#endif

        enabled = false;
    }

    void put(int64_t offset, byte_element_struct *ele, int passed) override
    {
        (void)offset;

        if (passed) {
            error(QB_ERROR_BAD_FILE_NAME_OR_NUMBER);
            return;
        }

        if (closed) {
            error(QB_ERROR_BAD_FILE_NAME_OR_NUMBER);
            return;
        }

#ifdef QB64_UNIX
        // TODO: Loop on partial write?
        int e = write(fd, (const char *)ele->offset, ele->length);
        if (e == -1 && errno == EPIPE) {
            ::close(fd);
            closed = true;
            error(QB_ERROR_BAD_FILE_NAME_OR_NUMBER);
        }
#else
        // TODO: Loop on partial write?
        BOOL s = WriteFile(fd, (void *)ele->offset, ele->length, NULL, NULL);
        if (!s) {
            DWORD err = GetLastError();

            if (err != ERROR_BROKEN_PIPE && err != ERROR_PIPE_NOT_CONNECTED)
                libqb_log_warn("Unexpected pipe error for handle %d: %d", id, err);

            CloseHandle(fd);
            closed = true;
            error(QB_ERROR_BAD_FILE_NAME_OR_NUMBER);
            return;
        }
#endif
    }

    int32_t connected() override
    {
        return TO_QB_BOOL(closed);
    }
};

class proc_stream final : public special_handle {
  public:
    uint32_t pid;
    stream_handle_in out, err;
    stream_handle_out in;

    bool isalive;
    int exitstatus;

  private:
#ifdef QB64_UNIX
    void check_is_alive()
    {
        if (!isalive)
            return;

        if (waitpid(pid, &exitstatus, WNOHANG) == pid) {
            if (WIFEXITED(exitstatus)) {
                exitstatus = WEXITSTATUS(exitstatus);
                isalive = false;
            }
        }
    }

    int start_proc(qbs *cmd)
    {
        int newpipe[2];

        if (in.enabled) {
            pipe(newpipe);
            in.childfd = newpipe[0];
            in.fd = newpipe[1];
        }

        if (out.enabled) {
            pipe(newpipe);
            out.fd = newpipe[0];
            out.childfd = newpipe[1];
            set_nonblock(out.fd);
        }

        if (err.enabled) {
            pipe(newpipe);
            err.fd = newpipe[0];
            err.childfd = newpipe[1];
            set_nonblock(err.fd);
        }

        pid_t child;
        qbs *strz;

        // Make sure that no signal handlers trigger in the child after the
        // fork but before we reset the handlers
        sigset_t blocked, original_set;
        sigfillset(&blocked);
        pthread_sigmask(SIG_SETMASK, &blocked, &original_set);
        struct sigaction sig;
        const char *args[4];

        switch (child = fork()) {
        case -1:
            // error
            break;

        case 0:
            // Run via shell, we do it this way to avoid having to split the string
            // into separate arguments
            strz = qbs_new(0, 0);
            qbs_set(strz, qbs_add(cmd, qbs_new_txt_len("\0", 1)));
            args[0] = (const char *)"/bin/sh";
            args[1] = (const char *)"-c";
            args[2] = (const char *)strz->chr;
            args[3] = NULL;

            // child
            if (in.enabled)
                dup2(in.childfd, STDIN_FILENO);

            if (out.enabled)
                dup2(out.childfd, STDOUT_FILENO);

            if (err.enabled)
                dup2(err.childfd, STDERR_FILENO);

            // close all existing FD's
            close_all_fds_above(3);

            // reset signal handlers and unblock signals before exec
            sig.sa_handler = SIG_DFL;
            for (int i = 0; i < NSIG; i++)
                sigaction(i, &sig, NULL);

            sigemptyset(&blocked);
            pthread_sigmask(SIG_SETMASK, &blocked, NULL);

            execv("/bin/sh", (char * const *)args);

            _exit(1); // Unreachable
            break;

        default:
            pthread_sigmask(SIG_SETMASK, &original_set, NULL);
            // parent
            pid = pid;
            isalive = true;

            if (in.enabled)
                ::close(in.childfd);

            if (out.enabled)
                ::close(out.childfd);

            if (err.enabled)
                ::close(err.childfd);

            break;
        }

        return 0;
    }

    void wait_for_process_end()
    {
        int status;
        // We have to wait for the child to exit to avoid creating a zombie process
        waitpid(pid, &status, 0);
    }
#else
    HANDLE process;

    void check_is_alive()
    {
        int r = WaitForSingleObject(process, 0);
        if (r == WAIT_OBJECT_0) {
            DWORD dword = 0;
            GetExitCodeProcess(process, &dword);
            exitstatus = dword;
            CloseHandle(process);
            isalive = false;
        }
    }

    int start_proc(qbs *cmd)
    {
        HANDLE rd, wr;

        SECURITY_ATTRIBUTES secAttr;
        memset((char *)&secAttr, 0, sizeof(secAttr));
        secAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        secAttr.bInheritHandle = TRUE;

        if (in.enabled) {
            CreatePipe(&rd, &wr, &secAttr, 0);
            in.childfd = rd;
            in.fd = wr;
            SetHandleInformation(in.fd, HANDLE_FLAG_INHERIT, 0);
        }

        if (out.enabled) {
            CreatePipe(&rd, &wr, &secAttr, 0);
            out.childfd = wr;
            out.fd = rd;
            SetHandleInformation(out.fd, HANDLE_FLAG_INHERIT, 0);
            WriteFile(out.childfd, "This is a test", 14, NULL, NULL);
        }

        if (err.enabled) {
            CreatePipe(&rd, &wr, &secAttr, 0);
            err.childfd = wr;
            err.fd = rd;
            SetHandleInformation(err.fd, HANDLE_FLAG_INHERIT, 0);
        }

        PROCESS_INFORMATION procInfo;
        STARTUPINFO startInfo;
        memset((char *)&procInfo, 0, sizeof(procInfo));
        memset((char *)&startInfo, 0, sizeof(startInfo));

        startInfo.cb = sizeof(startInfo);
        startInfo.dwFlags |= STARTF_USESTDHANDLES;

        if (in.enabled)
            startInfo.hStdInput = in.childfd;

        if (out.enabled)
            startInfo.hStdOutput = out.childfd;

        if (err.enabled)
            startInfo.hStdError = err.childfd;

        qbs *strz = qbs_new(0, 0);
        qbs_set(strz, qbs_add(cmd, qbs_new_txt_len("\0", 1)));
        bool success = CreateProcess(NULL,
                (char *)strz->chr,
                NULL,
                NULL,
                TRUE,
                0,
                NULL,
                NULL,
                &startInfo,
                &procInfo);

        if (!success) {
            // error?
        } else {
            pid = procInfo.dwProcessId;
            process = procInfo.hProcess;
            CloseHandle(procInfo.hThread);

            if (in.enabled)
                CloseHandle(in.childfd);

            if (out.enabled)
                CloseHandle(out.childfd);

            if (err.enabled)
                CloseHandle(err.childfd);
        }
        return 0;
    }

    void wait_for_process_end()
    {
        WaitForSingleObject(process, -1);
        CloseHandle(process);
    }

#endif
  public:
    void close() override {
        if (in.enabled)
            special_handle_close(&in);

        if (out.enabled)
            special_handle_close(&out);

        if (err.enabled)
            special_handle_close(&err);

        if (isalive)
            wait_for_process_end();

        handle_table.erase(id);

        delete this;
    }

    int32_t connected() override {
        check_is_alive();
        if (isalive)
            return -1;

        return exitstatus;
    }

    int start_process(qbs *cmd)
    {
        return start_proc(cmd);
    }
};

uint32_t func__shellstream(qbs *cmd, uint32_t flags)
{
    proc_stream *stream = new proc_stream();

    if (flags & SHELL_STREAM_IN)
        stream->in.enabled = true;

    if (flags & SHELL_STREAM_OUT)
        stream->out.enabled = true;

    if (flags & SHELL_STREAM_ERROR)
        stream->err.enabled = true;

    int err = stream->start_process(cmd);
    if (err) {
        stream->close();
        error(QB_ERROR_BAD_FILE_NAME_OR_NUMBER);
        return -1;
    }

    special_handle_custom_open(stream);

    if (stream->in.enabled)
        special_handle_custom_open(&stream->in);

    if (stream->out.enabled)
        special_handle_custom_open(&stream->out);

    if (stream->err.enabled)
        special_handle_custom_open(&stream->err);

    handle_table.emplace(stream->id, stream);

    return stream->id;
}

static bool is_valid_shell_id(int id)
{
    return handle_table.find(id) != handle_table.end();
}

uint32_t func__instream(int32_t id)
{
    if (is_valid_shell_id(id))
        return handle_table[id]->in.id;

    return -1;
}

uint32_t func__outstream(int32_t id)
{
    if (is_valid_shell_id(id))
        return handle_table[id]->out.id;

    return -1;
}

uint32_t func__errorstream(int32_t id)
{
    if (is_valid_shell_id(id))
        return handle_table[id]->err.id;

    return -1;
}
