#include "KS_asserts.hh"

#include <cstdio>
#include <cstdlib>
#include <cstdarg>

namespace kk::detail
{

[[noreturn]] void Terminate()
{
    std::quick_exit(-1);
}

void LogAbort(const char* file, unsigned line, const char* fmt, ...)
{
    char str[1024]{};
    va_list args;
    va_start(args, fmt);
#if (_MSC_VER)
    (void)vsprintf_s(str, fmt, args);
#else
    (void)vsnprintf(str, sizeof(str) - 1, fmt, args);
#endif
    va_end(args);
    fprintf(stderr, "Error at '%s,%u'\n -> %s\n", file, line, str);
    fflush(stderr);
}

} // namespace kk::detail
