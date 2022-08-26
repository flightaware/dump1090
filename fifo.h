// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// fifo.h: Cross-thread SDR to demodulator FIFO support
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

#ifndef FIFO_H
#define FIFO_H

#include <stdbool.h>
#include <stdint.h>

// Values for mag_buf.flags
typedef enum {
    MAGBUF_DISCONTINUOUS = 1, // this buffer is discontinuous to the previous buffer
} mag_buf_flags;

// Structure representing one magnitude buffer
// The contained data looks like this:
//
//  0                overlap          validLength-overlap            validLength       totalLength
//  |                   |                     |                           |                 |
//  | overlap data from |  new sample data    | new sample data that      |  optional       |
//  | previous buffer   |                     | will be used as overlap   |  unused         |
//  |                   |                     | in the next buffer        |  space          |
//  |                   |                     |                           |                 |
//  |                   |                    [this is the position of the]|                 |
//  |                   |                    [last message that the      ]|                 |
//  |                   |                    [demodulator will decode    ]|                 |
//  |                   |                     |                           |                 |
//  |                   |                     |     [partial messages that start    ]       |
//  |  [copied here in ]|  <----------------------  [after the cutoff will be copied]       |
//  |  [the next buffer]|                     |     [to the starting overlap of the ]       |
//  |                   |                     |     [next buffer                    ]       |
//
// The demodulator looks for signals starting at offsets 0 .. validLength-overlap-1,
// with the trailing overlap region allowing decoding of a maximally-sized message that starts
// at validLength-overlap-1. Signals that start after this point are not decoded, but they will
// be copied into the starting overlap of the next buffer and decoded on the next iteration.

struct mag_buf {
    uint16_t       *data;            // Magnitude data, starting with overlap from the previous block
    unsigned        totalLength;     // Maximum number of samples (allocated size of "data")
    unsigned        validLength;     // Number of valid samples in "data", including overlap samples
    unsigned        overlap;         // Number of leading overlap samples at the start of "data";
                                     // also the number of trailing samples that will be preserved for next time

    uint64_t        sampleTimestamp; // Clock timestamp of the start of this block, 12MHz clock
    uint64_t        sysTimestamp;    // Estimated system time at start of block

    mag_buf_flags   flags;           // bitwise flags for this buffer
    double          mean_level;      // Mean of normalized (0..1) signal level
    double          mean_power;      // Mean of normalized (0..1) power level
    unsigned        dropped;         // (approx) number of dropped samples, if flag MAGBUF_DISCONTINUOUS is set; zero if not discontinuous

    struct mag_buf *next;            // linked list forward link
};

// Create the queue structures. Not threadsafe. Returns true on success.
//
//   buffer_count - the number of buffers to preallocate
//   buffer_size  - the size of each magnitude buffer, in samples, including overlap
//   overlap      - the number of samples to overlap between adjacent buffers
bool fifo_create(unsigned buffer_count, unsigned buffer_size, unsigned overlap);

// Destroy the fifo structures allocated in magbuf_fifo_create. Not threadsafe; ensure all FIFO users
// are done before calling.
void fifo_destroy();

// Block until the FIFO is empty.
void fifo_drain();

// Mark the FIFO as halted. Move any buffers in FIFO to the freelist immediately.
// Future calls to magbuf_acquire() will immediately return NULL.
// Future calls to magbuf_produce() will immediately put the produced buffer on the freelist.
// Future alls to magbuf_consume() will immediately return NULL; if there are
//   existing calls waiting on data, they will be immediately awoken and return NULL.
void fifo_halt();

// Get an unused buffer from the freelist and return it.
// Block up to timeout_ms waiting for a free buffer. Return NULL if there are no
// free buffers available within the timeout, or if the FIFO is halted.
struct mag_buf *fifo_acquire(uint32_t timeout_ms);

// Put a filled buffer (previously obtained from fifo_acquire) onto the head of the FIFO.
// The caller should have filled:
//   buf->validLength
//   buf->data[buf->overlap .. buf->validLength-1]
//   buf->sampleTimestamp
//   buf->sysTimestamp
//   buf->flags
//   buf->mean_level (if flags & HAS_METRICS)
//   buf->mean_power (if flags & HAS_METRICS)
//   buf->dropped    (if flags & DISCONTINUOUS)
void fifo_enqueue(struct mag_buf *buf);

// Get a buffer from the tail of the FIFO.
// If the FIFO is halted (or becomes halted), return NULL immediately.
// If the FIFO is empty, wait for up to "timeout_ms" milliseconds
//   for more data; return NULL if no data arrives within the timeout.
struct mag_buf *fifo_dequeue(uint32_t timeout_ms);

// Release a buffer previously returned by fifo_acquire() or fifo_pop() back to the freelist.
void fifo_release(struct mag_buf *buf);

#endif
