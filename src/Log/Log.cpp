#include "Log.h"
#include <stdarg.h>

// Weak no-op: replaced by the strong definition in SocketClient.cpp when linked.
__attribute__((weak)) void _scRemoteLog(uint8_t level, const char *tag, const char *fmt, ...) {}
