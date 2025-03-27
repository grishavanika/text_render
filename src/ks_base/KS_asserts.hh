#pragma once

#if (_MSC_VER)
#  define KK_DEBUGBREAK() __debugbreak()
#else
#  include <cassert>
#  define KK_DEBUGBREAK() assert(false)
#endif

#define KK_ABORT(FMT, ...) (void)( \
    ::kk::detail::LogAbort(__FILE__, __LINE__, FMT, ##__VA_ARGS__) \
    , KK_DEBUGBREAK() \
    , ::kk::detail::Terminate() \
    )

#define KK_VERIFY(X) (void)( \
    (!!(X)) \
    || (KK_ABORT("Assert failed: %s.", #X), false) \
    )

#if !defined(NDEBUG)
#  define KK_ENABLE_DEBUG() 1
#else
#  define KK_ENABLE_DEBUG() 0
#endif

#if (KK_ENABLE_DEBUG())
#  define KK_ASSERT(X) KK_VERIFY(X)
#else
#  define KK_ASSERT(X) (void)0
#endif

#if defined(_MSC_VER)
#  define KK_FORCEINLINE __forceinline
#else
#  define KK_FORCEINLINE inline // __attribute((always_inline))
#endif

namespace kk::detail
{

[[noreturn]] void Terminate();
void LogAbort(const char* file, unsigned line, const char* fmt, ...);

} // namespace kk::detail

#if defined(_MSC_VER)
#  define KK_UNREACHABLE() (void)( \
    KK_VERIFY(false && "Unreachable point reached") \
    , __assume(0) \
    )
#else
#  define KK_UNREACHABLE() (void)( \
    KK_VERIFY(false && "Unreachable point reached") \
    , __builtin_unreachable() \
    )
#endif
