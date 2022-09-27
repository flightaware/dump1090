// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// util.c: misc utilities
//
// Copyright (c) 2015 Oliver Jowett <oliver@mutability.co.uk>
//
// This file is free software: you may copy, redistribute and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 2 of the License, or (at your
// option) any later version.
//
// This file is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// This file incorporates work covered by the following copyright and
// permission notice:
//
//   Copyright (C) 2012 by Salvatore Sanfilippo <antirez@gmail.com>
//
//   All rights reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions are
//   met:
//
//    *  Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//    *  Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// we want pthread_setname_np if available
#define _GNU_SOURCE

#include "dump1090.h"

#include <stdlib.h>
#include <sys/time.h>

uint64_t _messageNow = 0;

uint64_t mstime(void)
{
    struct timeval tv;
    uint64_t mst;

    gettimeofday(&tv, NULL);
    mst = ((uint64_t)tv.tv_sec)*1000;
    mst += tv.tv_usec/1000;
    return mst;
}

int64_t receiveclock_ns_elapsed(uint64_t t1, uint64_t t2)
{
    return (t2 - t1) * 1000U / 12U;
}

int64_t receiveclock_ms_elapsed(uint64_t t1, uint64_t t2)
{
    return (t2 - t1) / 12000U;
}

void normalize_timespec(struct timespec *ts)
{
    if (ts->tv_nsec >= 1000000000) {
        ts->tv_sec += ts->tv_nsec / 1000000000;
        ts->tv_nsec = ts->tv_nsec % 1000000000;
    } else if (ts->tv_nsec < 0) {
        long adjust = ts->tv_nsec / 1000000000 + 1;
        ts->tv_sec -= adjust;
        ts->tv_nsec = (ts->tv_nsec + 1000000000 * adjust) % 1000000000;
    }
}

void get_deadline(uint32_t timeout_ms, struct timespec *ts)
{
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += timeout_ms / 1000;
    ts->tv_nsec += (timeout_ms % 1000) * 1000000;
    normalize_timespec(ts);
}

/* record current CPU time in start_time */
void start_cpu_timing(struct timespec *start_time)
{
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, start_time);
}

/* add difference between start_time and the current CPU time to add_to */
void end_cpu_timing(const struct timespec *start_time, struct timespec *add_to)
{
    struct timespec end_time;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_time);
    add_to->tv_sec += end_time.tv_sec - start_time->tv_sec;
    add_to->tv_nsec += end_time.tv_nsec - start_time->tv_nsec;
    normalize_timespec(add_to);
}

/* add difference between start_time and the current CPU time to add_to; then store the current CPU time in start_time */
void update_cpu_timing(struct timespec *start_time, struct timespec *add_to)
{
    struct timespec end_time;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_time);
    add_to->tv_sec += end_time.tv_sec - start_time->tv_sec;
    add_to->tv_nsec += end_time.tv_nsec - start_time->tv_nsec;
    normalize_timespec(add_to);
    *start_time = end_time;
}

void set_thread_name(const char *name)
{
#if (__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 12)
    pthread_setname_np(pthread_self(), name);
#else
    MODES_NOTUSED(name);
#endif
}

int join_thread(pthread_t thread, void **retval, uint32_t timeout_ms)
{
#if (__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 3)
    struct timespec abstime;
    get_deadline(timeout_ms, &abstime);
    return pthread_timedjoin_np(thread, retval, &abstime);
#else
    MODES_NOTUSED(timeout_ms);
    return pthread_join(thread, retval);
#endif
}
