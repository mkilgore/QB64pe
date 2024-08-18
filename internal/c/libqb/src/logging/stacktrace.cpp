
#include "libqb-common.h"

#include <string>
#include <unwind.h>

#include "logging.h"
#include "logging_private.h"

struct stacktrace_state {
    int frame;
    int skipped;
    std::string str;
};

static _Unwind_Reason_Code handler(struct _Unwind_Context *context, void *ref) {
    stacktrace_state *result = static_cast<stacktrace_state *>(ref);

	if (result->frame > 40)
		return _URC_NORMAL_STOP;

    result->frame++;

    const void *addr = (const void *)_Unwind_GetIP(context);
    if (!addr)
        return _URC_NORMAL_STOP;

    std::string symbol = libqb_log_resolve_symbol(addr);

    // Remove these symbols from the stacktrace
    if (symbol == "libqb_log"
        || symbol == "libqb_vlog"
        || symbol == "libqb_log_qbs"
        || symbol == "libqb_log_get_stacktrace") {

        result->skipped++;
        return _URC_NO_REASON;
    }


    std::optional<std::string> qb64_symbol = libqb_log_resolve_qb64_symbol(symbol.c_str());

    if (qb64_symbol.has_value())
        symbol = *qb64_symbol;

    char line[200];
    snprintf(line, sizeof(line), "#%-2d [0x%p] in %s\n", result->frame - result->skipped, addr, symbol.c_str());

    result->str += line;
	return _URC_NO_REASON;
}

std::string libqb_log_get_stacktrace() {
    stacktrace_state state = {
        .frame = 0,
        .skipped = 0,
        .str = "",
    };

    _Unwind_Backtrace(&handler, &state);

    return state.str;
}
