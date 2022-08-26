// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// fifo.c: Cross-thread SDR to demodulator FIFO support
//
// Copyright (c) 2020 FlightAware LLC
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

#include "fifo.h"
#include "util.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

static pthread_mutex_t fifo_mutex = PTHREAD_MUTEX_INITIALIZER;        // mutex protecting the queues
static pthread_cond_t fifo_notempty_cond = PTHREAD_COND_INITIALIZER;  // condition used to signal FIFO-not-empty
static pthread_cond_t fifo_empty_cond = PTHREAD_COND_INITIALIZER;     // condition used to signal FIFO-empty
static pthread_cond_t fifo_free_cond = PTHREAD_COND_INITIALIZER;      // condition used to signal freelist-not-empty
static struct mag_buf *fifo_head;          // head of queued buffers awaiting demodulation
static struct mag_buf *fifo_tail;          // tail of queued buffers awaiting demodulation
static struct mag_buf *fifo_freelist;      // freelist of preallocated buffers
static bool fifo_halted;                   // true if queue has been halted

static unsigned overlap_length;     // desired overlap size in samples (size of overlap_buffer)
static uint16_t *overlap_buffer;    // buffer used to save overlapping data

// Create the queue structures. Not threadsafe.
bool fifo_create(unsigned buffer_count, unsigned buffer_size, unsigned overlap)
{
    if (!(overlap_buffer = calloc(overlap, sizeof(overlap_buffer[0]))))
        goto nomem;

    overlap_length = overlap;

    for (unsigned i = 0; i < buffer_count; ++i) {
        struct mag_buf *newbuf;
        if (!(newbuf = calloc(1, sizeof(*newbuf)))) {
            goto nomem;
        }

        if (!(newbuf->data = calloc(buffer_size, sizeof(newbuf->data[0])))) {
            free(newbuf);
            goto nomem;
        }

        newbuf->totalLength = buffer_size;
        newbuf->next = fifo_freelist;
        fifo_freelist = newbuf;
    }

    return true;

 nomem:
    fifo_destroy();
    return false;
}

static void free_buffer_list(struct mag_buf *head)
{
    while (head) {
        struct mag_buf *next = head->next;
        free(head->data);
        free(head);
        head = next;
    }
}

void fifo_destroy()
{
    free_buffer_list(fifo_head);
    fifo_head = fifo_tail = NULL;

    free_buffer_list(fifo_freelist);
    fifo_freelist = NULL;

    free(overlap_buffer);
    overlap_buffer = NULL;
}

void fifo_drain()
{
    pthread_mutex_lock(&fifo_mutex);
    while (fifo_head && !fifo_halted) {
        pthread_cond_wait(&fifo_empty_cond, &fifo_mutex);
    }
    pthread_mutex_unlock(&fifo_mutex);
}

void fifo_halt()
{
    pthread_mutex_lock(&fifo_mutex);

    // Drain all enqueued buffers to the freelist
    while (fifo_head) {
        struct mag_buf *freebuf = fifo_head;
        fifo_head = freebuf->next;

        freebuf->next = fifo_freelist;
        fifo_freelist = freebuf;
    }

    fifo_tail = NULL;
    fifo_halted = true;

    // wake all waiters
    pthread_cond_broadcast(&fifo_notempty_cond);
    pthread_cond_broadcast(&fifo_empty_cond);
    pthread_cond_broadcast(&fifo_free_cond);
    pthread_mutex_unlock(&fifo_mutex);
}

struct mag_buf *fifo_acquire(uint32_t timeout_ms)
{
    struct timespec deadline;
    if (timeout_ms)
        get_deadline(timeout_ms, &deadline);

    pthread_mutex_lock(&fifo_mutex);

    struct mag_buf *result = NULL;
    while (!fifo_halted && !fifo_freelist) {
        if (!timeout_ms) {
            // Non-blocking
            goto done;
        }

        // No free buffers, wait for one
        int err = pthread_cond_timedwait(&fifo_free_cond, &fifo_mutex, &deadline);
        if (err) {
            if (err != ETIMEDOUT) {
                fprintf(stderr, "fifo_acquire: pthread_cond_timedwait unexpectedly returned %s\n", strerror(err));
            }

            goto done; // done waiting
        }
    }

    if (!fifo_halted) {
        result = fifo_freelist;
        fifo_freelist = result->next;

        result->overlap = overlap_length;
        result->validLength = result->overlap;
        result->sampleTimestamp = 0;
        result->sysTimestamp = 0;
        result->flags = 0;
        result->mean_level = 0;
        result->mean_power = 0;
        result->dropped = 0;
        result->next = NULL;
    }

 done:
    pthread_mutex_unlock(&fifo_mutex);
    return result;
}

void fifo_enqueue(struct mag_buf *buf)
{
    assert(buf->validLength <= buf->totalLength);
    assert(buf->validLength >= overlap_length);

    pthread_mutex_lock(&fifo_mutex);

    if (fifo_halted) {
        // Shutting down, just return the buffer to the freelist.
        buf->next = fifo_freelist;
        fifo_freelist = buf;
        goto done;
    }

    // Populate the overlap region
    if (buf->flags & MAGBUF_DISCONTINUOUS) {
        // This buffer is discontinuous to the previous, so the overlap region is not valid; zero it out
        memset(buf->data, 0, overlap_length * sizeof(buf->data[0]));
    } else {
        memcpy(buf->data, overlap_buffer, overlap_length * sizeof(buf->data[0]));
    }

    // Save the tail of the buffer for next time
    memcpy(overlap_buffer, &buf->data[buf->validLength - overlap_length], overlap_length * sizeof(overlap_buffer[0]));

    // enqueue and tell the main thread
    buf->next = NULL;
    if (!fifo_head) {
        fifo_head = fifo_tail = buf;
        pthread_cond_signal(&fifo_notempty_cond);
    } else {
        fifo_tail->next = buf;
        fifo_tail = buf;
    }

 done:
    pthread_mutex_unlock(&fifo_mutex);
}

struct mag_buf *fifo_dequeue(uint32_t timeout_ms)
{
    struct timespec deadline;
    if (timeout_ms)
        get_deadline(timeout_ms, &deadline);

    pthread_mutex_lock(&fifo_mutex);

    struct mag_buf *result = NULL;
    while (!fifo_head && !fifo_halted) {
        if (!timeout_ms) {
            // Non-blocking
            goto done;
        }

        // No data pending, wait for some
        int err = pthread_cond_timedwait(&fifo_notempty_cond, &fifo_mutex, &deadline);
        if (err) {
            if (err != ETIMEDOUT) {
                fprintf(stderr, "fifo_dequeue: pthread_cond_timedwait unexpectedly returned %s\n", strerror(err));
            }

            goto done; // done waiting
        }
    }

    if (!fifo_halted) {
        result = fifo_head;
        fifo_head = result->next;
        result->next = NULL;
        if (!fifo_head) {
            fifo_tail = NULL;
            pthread_cond_broadcast(&fifo_empty_cond);
        }
    }

 done:
    pthread_mutex_unlock(&fifo_mutex);
    return result;
}

void fifo_release(struct mag_buf *buf)
{
    pthread_mutex_lock(&fifo_mutex);
    if (!fifo_freelist)
        pthread_cond_signal(&fifo_free_cond);
    buf->next = fifo_freelist;
    fifo_freelist = buf;
    pthread_mutex_unlock(&fifo_mutex);
}
