#ifndef COMPAT_UTIL_H
#define COMPAT_UTIL_H

/*
 * Platform-specific bits
 */

#if defined(__APPLE__)

/*
 * Mach endian conversion
 */
# include <libkern/OSByteOrder.h>
# define bswap_16 OSSwapInt16
# define bswap_32 OSSwapInt32
# define bswap_64 OSSwapInt64
# include <machine/endian.h>
# define le16toh(x) OSSwapLittleToHostInt16(x)
# define le32toh(x) OSSwapLittleToHostInt32(x)
# define le64toh(x) OSSwapLittleToHostInt64(x)

#elif defined(__FreeBSD__)
#include <sys/endian.h>

#else // other platforms

# include <endian.h>

#endif

/* clock_* and time-related types */

#include <time.h>

#if defined(CLOCK_REALTIME)
#  define HAVE_CLOCKID_T
#endif

#ifndef HAVE_CLOCKID_T
typedef enum
{
    CLOCK_REALTIME,
    CLOCK_MONOTONIC,
    CLOCK_PROCESS_CPUTIME_ID,
    CLOCK_THREAD_CPUTIME_ID
} clockid_t;
#endif // !HAVE_CLOCKID_T

#ifndef TIMER_ABSTIME
#define TIMER_ABSTIME 1
#endif // !TIMER_ABSTIME

struct timespec;

#ifdef MISSING_NANOSLEEP
#include "clock_nanosleep/clock_nanosleep.h"
#endif

#ifdef MISSING_GETTIME
#include "clock_gettime/clock_gettime.h"
#endif

#endif //COMPAT_UTIL_H
