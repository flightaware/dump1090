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

#elif __FreeBSD__
#include <sys/endian.h>

#else // other platforms

# include <endian.h>

#endif

#ifdef MISSING_NANOSLEEP
#include "clock_nanosleep/clock_nanosleep.h"
#endif

#ifdef MISSING_GETTIME
#include "clock_gettime/clock_gettime.h"
#endif

#endif //COMPAT_UTIL_H
