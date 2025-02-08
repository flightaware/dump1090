#ifndef COMPAT_UTIL_H
#define COMPAT_UTIL_H

/*
 * Platform-specific bits
 */

#include "endian.h"

/* clock_* and time-related types */

#include <time.h>

//MSVC/MinGW has no _r time functions
#ifdef MISSING_TIME_R_FUNCS
#  define localtime_r(T,Tm) (localtime_s(Tm,T) ? NULL : Tm)
#  define gmtime_r(T,Tm) (gmtime_s(Tm,T) ? NULL : Tm)
#endif

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
