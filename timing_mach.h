// from https://github.com/ChisholmKyle/PosixMachTiming
// Copyright (c) 2015, Kyle Chisholm <>

#ifndef TIMING_MACH_H
#define TIMING_MACH_H
/* ************* */
/* TIMING_MACH_H */

#include <time.h>

/* OSX before 10.12 (MacOS Sierra) */
#define TIMING_MACH_BEFORE_10_12 (__MACH__ && __MAC_OS_X_VERSION_MIN_REQUIRED < 101200)

/* scale factors */
#define TIMING_GIGA (1000000000)
#define TIMING_NANO (1e-9)

/*  Before OSX 10.12, the following are emulated here:
        CLOCK_REALTIME
        CLOCK_MONOTONIC
        clockid_t
        clock_gettime
        clock_getres
*/
#if (TIMING_MACH_BEFORE_10_12)
/* **** */
/* MACH */

    /* clockid_t - emulate POSIX */
    typedef int clockid_t;

    /* CLOCK_REALTIME - emulate POSIX */
    #ifndef CLOCK_REALTIME
    # define CLOCK_REALTIME 0
    #endif

    /* CLOCK_MONOTONIC - emulate POSIX */
    #ifndef CLOCK_MONOTONIC
    # define CLOCK_MONOTONIC 1
    #endif

    /* clock_gettime - emulate POSIX */
    int clock_gettime ( const clockid_t id, struct timespec *tspec );

    /* clock_getres - emulate POSIX */
    int clock_getres (clockid_t id, struct timespec *res);

    /* initialize timing */
    int timing_mach_init (void);

/* MACH */
/* **** */
#else

    /* initialize mach timing is a no-op */
    #define timing_mach_init() 0

#endif

/* timespec to double */
inline double timespec2secd(const struct timespec *ts_in) {
    return ((double) ts_in->tv_sec) + ((double) ts_in->tv_nsec ) * TIMING_NANO;
}

/* double sec to timespec */
inline void secd2timespec(struct timespec *ts_out, const double sec_d) {
    ts_out->tv_sec = (time_t) (sec_d);
    ts_out->tv_nsec = (long) ((sec_d - (double) ts_out->tv_sec) * TIMING_GIGA);
}

/* timespec difference (monotonic) left - right */
inline void timespec_monodiff_lmr(struct timespec *ts_out,
                                    const struct timespec *ts_in) {
    /* out = out - in,
       where out > in
     */
    ts_out->tv_sec = ts_out->tv_sec - ts_in->tv_sec;
    ts_out->tv_nsec = ts_out->tv_nsec - ts_in->tv_nsec;
    if (ts_out->tv_sec < 0) {
        ts_out->tv_sec = 0;
        ts_out->tv_nsec = 0;
    } else if (ts_out->tv_nsec < 0) {
        if (ts_out->tv_sec == 0) {
            ts_out->tv_sec = 0;
            ts_out->tv_nsec = 0;
        } else {
            ts_out->tv_sec = ts_out->tv_sec - 1;
            ts_out->tv_nsec = ts_out->tv_nsec + TIMING_GIGA;
        }
    } else {}
}

/* timespec difference (monotonic) right - left */
inline void timespec_monodiff_rml(struct timespec *ts_out,
                                    const struct timespec *ts_in) {
    /* out = in - out,
       where in > out
     */
    ts_out->tv_sec = ts_in->tv_sec - ts_out->tv_sec;
    ts_out->tv_nsec = ts_in->tv_nsec - ts_out->tv_nsec;
    if (ts_out->tv_sec < 0) {
        ts_out->tv_sec = 0;
        ts_out->tv_nsec = 0;
    } else if (ts_out->tv_nsec < 0) {
        if (ts_out->tv_sec == 0) {
            ts_out->tv_sec = 0;
            ts_out->tv_nsec = 0;
        } else {
            ts_out->tv_sec = ts_out->tv_sec - 1;
            ts_out->tv_nsec = ts_out->tv_nsec + TIMING_GIGA;
        }
    } else {}
}

/* timespec addition (monotonic) */
inline void timespec_monoadd(struct timespec *ts_out,
                             const struct timespec *ts_in) {
    /* out = in + out */
    ts_out->tv_sec = ts_out->tv_sec + ts_in->tv_sec;
    ts_out->tv_nsec = ts_out->tv_nsec + ts_in->tv_nsec;
    if (ts_out->tv_nsec >= TIMING_GIGA) {
        ts_out->tv_sec = ts_out->tv_sec + 1;
        ts_out->tv_nsec = ts_out->tv_nsec - TIMING_GIGA;
    }
}

#ifdef __MACH__
/* **** */
/* MACH */

    /* emulate clock_nanosleep for CLOCK_MONOTONIC and TIMER_ABSTIME */
    inline int clock_nanosleep_abstime ( const struct timespec *req )
    {
        struct timespec ts_delta;
        int retval = clock_gettime ( CLOCK_MONOTONIC, &ts_delta );
        if (retval == 0) {
            timespec_monodiff_rml ( &ts_delta, req );
            retval = nanosleep ( &ts_delta, NULL );
        }
        return retval;
    }

/* MACH */
/* **** */
#else
/* ***** */
/* POSIX */

    /* clock_nanosleep for CLOCK_MONOTONIC and TIMER_ABSTIME */
    #define clock_nanosleep_abstime( req ) \
        clock_nanosleep ( CLOCK_MONOTONIC, TIMER_ABSTIME, (req), NULL )

/* POSIX */
/* ***** */
#endif

/* timer functions that make use of clock_nanosleep_abstime
   For POSIX systems, it is recommended to use POSIX timers and signals.
   For Mac OSX (mach), there are no POSIX timers so these functions are very helpful.
*/

/* Sets absolute time ts_target to ts_step after current time */
int itimer_start (struct timespec *ts_target, const struct timespec *ts_step);

/* Nanosleeps to ts_target then adds ts_step to ts_target for next iteration */
int itimer_step (struct timespec *ts_target, const struct timespec *ts_step);

/* TIMING_MACH_H */
/* ************* */
#endif
