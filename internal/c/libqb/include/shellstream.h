
#pragma once

enum {
    SHELL_STREAM_IN = 1,
    SHELL_STREAM_OUT = 2,
    SHELL_STREAM_ERROR = 4,
};

uint32_t func__shellstream(qbs *cmd, uint32_t flags);

uint32_t func__instream(int32_t shell);
uint32_t func__outstream(int32_t shell);
uint32_t func__errorstream(int32_t shell);

