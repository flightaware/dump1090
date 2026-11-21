// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// icao_filter.c: hashtable for ICAO addresses
//
// Copyright (c) 2014,2015 Oliver Jowett <oliver@mutability.co.uk>
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

#include "dump1090.h"

// hash table size, must be a power of two:
#define ICAO_FILTER_SIZE 16384

// Maximum linear-probe distance. Keeps add/test O(1) even when the table is
// flooded with spoofed addresses.
#define ICAO_FILTER_MAX_PROBE 32

// Millis between filter expiry flips:
#define MODES_ICAO_FILTER_TTL 60000

// Open-addressed hash table with linear probing.
// We store each address twice to handle Data/Parity
// which need to match on a partial address (top 16 bits only).

// Maintain two tables and switch between them to age out entries.

static uint32_t icao_filter_a[ICAO_FILTER_SIZE];
static uint32_t icao_filter_b[ICAO_FILTER_SIZE];
static uint32_t *icao_filter_active;

#define EMPTY 0xFFFFFFFF

static uint32_t icaoHash(uint32_t a)
{
    // Jenkins one-at-a-time hash, unrolled for 3 bytes
    uint32_t hash = 0;

    hash += a & 0xff;
    hash += hash << 10;
    hash ^= hash >> 6;

    hash += (a >> 8) & 0xff;
    hash += (hash << 10);
    hash ^= (hash >> 6);

    hash += (a >> 16) & 0xff;
    hash += (hash << 10);
    hash ^= (hash >> 6);

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash & (ICAO_FILTER_SIZE-1);
}

void icaoFilterInit()
{
    memset(icao_filter_a, 0xFF, sizeof(icao_filter_a));
    memset(icao_filter_b, 0xFF, sizeof(icao_filter_b));
    icao_filter_active = icao_filter_a;
}

void icaoFilterAdd(uint32_t addr)
{
    uint32_t h0 = icaoHash(addr), h = h0;
    for (unsigned i = 0; i < ICAO_FILTER_MAX_PROBE; ++i, h = (h+1) & (ICAO_FILTER_SIZE-1)) {
        if (icao_filter_active[h] == addr)
            return;
        if (icao_filter_active[h] == EMPTY) {
            icao_filter_active[h] = addr;
            return;
        }
    }
    // Probe window full: overwrite the home slot. This only makes the filter forget
    // one older entry (re-added on that aircraft's next DF11/17/18); it never creates
    // an EMPTY hole, so other entries stay reachable.
    icao_filter_active[h0] = addr;
}

int icaoFilterTest(uint32_t addr)
{
    uint32_t h0 = icaoHash(addr), h;
    unsigned i;

    for (i = 0, h = h0; i < ICAO_FILTER_MAX_PROBE && icao_filter_a[h] != EMPTY; ++i, h = (h+1) & (ICAO_FILTER_SIZE-1))
        if (icao_filter_a[h] == addr)
            return 1;

    for (i = 0, h = h0; i < ICAO_FILTER_MAX_PROBE && icao_filter_b[h] != EMPTY; ++i, h = (h+1) & (ICAO_FILTER_SIZE-1))
        if (icao_filter_b[h] == addr)
            return 1;

    return 0;
}

// call this periodically:
void icaoFilterExpire()
{
    static uint64_t next_flip = 0;
    uint64_t now = mstime();

    if (now >= next_flip) {
        if (icao_filter_active == icao_filter_a) {
            memset(icao_filter_b, 0xFF, sizeof(icao_filter_b));
            icao_filter_active = icao_filter_b;
        } else {
            memset(icao_filter_a, 0xFF, sizeof(icao_filter_a));
            icao_filter_active = icao_filter_a;
        }
        next_flip = now + MODES_ICAO_FILTER_TTL;
    }
}
