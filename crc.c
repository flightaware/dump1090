// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// crc.h: Mode S CRC calculation and error correction.
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

#include <assert.h>

// Errorinfo for "no errors"
static struct errorinfo NO_ERRORS;

// Generator polynomial for the Mode S CRC:
#define MODES_GENERATOR_POLY 0xfff409U

// CRC values for all single-byte messages;
// used to speed up CRC calculation.
static uint32_t crc_table[256];

// Syndrome values for all single-bit errors;
// used to speed up construction of error-
// correction tables.
static uint32_t single_bit_syndrome[112];

static void initLookupTables()
{
    int i;
    uint8_t msg[112/8];

    for (i = 0; i < 256; ++i) {
        uint32_t c = i << 16;
        int j;
        for (j = 0; j < 8; ++j) {
            if (c & 0x800000)
                c = (c<<1) ^ MODES_GENERATOR_POLY;
            else
                c = (c<<1);
        }

        crc_table[i] = c & 0x00ffffff;
    }

    memset(msg, 0, sizeof(msg));
    for (i = 0; i < 112; ++i) {
        msg[i/8] ^= 1 << (7 - (i & 7));
        single_bit_syndrome[i] = modesChecksum(msg, 112);
        msg[i/8] ^= 1 << (7 - (i & 7));
    }
}

uint32_t modesChecksum(const uint8_t *message, int bits)
{
    uint32_t rem = 0;
    int i;
    int n = bits/8;

    assert(bits % 8 == 0);
    assert(n >= 3);

    for (i = 0; i < n-3; ++i) {
        rem = (rem << 8) ^ crc_table[message[i] ^ ((rem & 0xff0000) >> 16)];
        rem = rem & 0xffffff;
    }

    rem = rem ^ (message[n-3] << 16) ^ (message[n-2] << 8) ^ (message[n-1]);
    return rem;
}

static struct errorinfo *bitErrorTable_short;
static int bitErrorTableSize_short;

static struct errorinfo *bitErrorTable_long;
static int bitErrorTableSize_long;

// compare two errorinfo structures
static int syndrome_compare(const void *x, const void *y) {
    struct errorinfo *ex = (struct errorinfo*)x;
    struct errorinfo *ey = (struct errorinfo*)y;
    return (int)ex->syndrome - (int)ey->syndrome;
}

// (n k), the number of ways of selecting k distinct items from a set of n items
static int combinations(int n, int k)
{
    int result = 1, i;

    if (k == 0 || k == n)
        return 1;

    if (k > n)
        return 0;

    for (i = 1; i <= k; ++i) {
        result = result * n / i;
        n = n - 1;
    }

    return result;
}

// Recursively populates an errorinfo table with error syndromes
//
// in:
//   table:      the table to fill
//   n:          first entry to fill
//   maxSize:    max size of table
//   offset:     start bit offset for checksum calculation
//   startbit:   first bit to introduce errors into
//   endbit:     (one past) last bit to introduce errors info
//   base_entry: template entry to start from
//   error_bit:  how many error bits have already been set
//   max_errors: maximum total error bits to set
// out:
//   returns:    the next free entry in the table
//   table:      has been populated between [n, return value)
static int prepareSubtable(struct errorinfo *table, int n, int maxsize, int offset, int startbit, int endbit, struct errorinfo *base_entry, int error_bit, int max_errors)
{
    int i = 0;

    if (error_bit >= max_errors)
        return n;

    for (i = startbit; i < endbit; ++i) {
        assert(n < maxsize);

        table[n] = *base_entry;
        table[n].syndrome ^= single_bit_syndrome[i + offset];
        table[n].errors = error_bit+1;
        table[n].bit[error_bit] = i;

        ++n;
        n = prepareSubtable(table, n, maxsize, offset, i + 1, endbit, &table[n-1], error_bit + 1, max_errors);
    }

    return n;
}

static int flagCollisions(struct errorinfo *table, int tablesize, int offset, int startbit, int endbit, uint32_t base_syndrome, int error_bit, int first_error, int last_error)
{
    int i = 0;
    int count = 0;

    if (error_bit > last_error)
        return 0;

    for (i = startbit; i < endbit; ++i) {
        struct errorinfo ei;

        ei.syndrome = base_syndrome ^ single_bit_syndrome[i + offset];

        if (error_bit >= first_error) {
            struct errorinfo *collision = bsearch(&ei, table, tablesize, sizeof(struct errorinfo), syndrome_compare);
            if (collision != NULL && collision->errors != -1) {
                ++count;
                collision->errors = -1;
            }
        }

        count += flagCollisions(table, tablesize, offset, i+1, endbit, ei.syndrome, error_bit + 1, first_error, last_error);
    }

    return count;
}


// Allocate and build an error table for messages of length "bits" (max 112)
// returns a pointer to the new table and sets *size_out to the table length
static struct errorinfo *prepareErrorTable(int bits, int max_correct, int max_detect, int *size_out)
{
    int maxsize, usedsize;
    struct errorinfo *table;
    struct errorinfo base_entry;
    int i, j;

    assert (bits >= 0 && bits <= 112);
    assert (max_correct >=0 && max_correct <= MODES_MAX_BITERRORS);
    assert (max_detect >= max_correct);

    if (!max_correct) {
        *size_out = 0;
        return NULL;
    }

    maxsize = 0;
    for (i = 1; i <= max_correct; ++i) {
        maxsize += combinations(bits, i); // space needed for all i-bit errors
    }

#ifdef CRCDEBUG
    fprintf(stderr, "Preparing syndrome table to correct up to %d-bit errors (detecting %d-bit errors) in a %d-bit message (max %d entries)\n", max_correct, max_detect, bits, maxsize);
#endif

    table = malloc(maxsize * sizeof(struct errorinfo));
    base_entry.syndrome = 0;
    base_entry.errors = 0;
    for (i = 0; i < MODES_MAX_BITERRORS; ++i)
        base_entry.bit[i] = -1;

    usedsize = prepareSubtable(table, 0, maxsize, 112 - bits, 0, bits, &base_entry, 0, max_correct);

#ifdef CRCDEBUG
    fprintf(stderr, "%d syndromes (expected %d).\n", usedsize, maxsize);
    fprintf(stderr, "Sorting syndromes..\n");
#endif

    qsort(table, usedsize, sizeof(struct errorinfo), syndrome_compare);

#ifdef CRCDEBUG
    {
        // Show the table stats
        fprintf(stderr, "Undetectable errors:\n");
        for (i = 1; i <= max_correct; ++i) {
            int j, count;

            count = 0;
            for (j = 0; j < usedsize; ++j)
                if (table[j].errors == i && table[j].syndrome == 0)
                    ++count;

            fprintf(stderr, "  %d undetectable %d-bit errors\n", count, i);
        }
    }
#endif

    // Handle ambiguous cases, where there is more than one possible error pattern
    // that produces a given syndrome (this happens with >2 bit errors).

#ifdef CRCDEBUG
    fprintf(stderr, "Finding collisions..\n");
#endif
    for (i = 0, j = 0; i < usedsize; ++i) {
        if (i < usedsize-1 && table[i+1].syndrome == table[i].syndrome) {
            // skip over this entry and all collisions
            while (i < usedsize && table[i+1].syndrome == table[i].syndrome)
                ++i;

            // now table[i] is the last duplicate
            continue;
        }

        if (i != j)
            table[j] = table[i];
        ++j;
    }

    if (j < usedsize) {
#ifdef CRCDEBUG
        fprintf(stderr, "Discarded %d collisions.\n", usedsize - j);
#endif
        usedsize = j;
    }

    // Flag collisions we want to detect but not correct
    if (max_detect > max_correct) {
        int flagged;

#ifdef CRCDEBUG
        fprintf(stderr, "Flagging collisions between %d - %d bits..\n", max_correct+1, max_detect);
#endif

        flagged = flagCollisions(table, usedsize, 112 - bits, 0, bits, 0, 1, max_correct+1, max_detect);

#ifdef CRCDEBUG
        fprintf(stderr, "Flagged %d collisions for removal.\n", flagged);
#else
#endif

        if (flagged > 0) {
            for (i = 0, j = 0; i < usedsize; ++i) {
                if (table[i].errors != -1) {
                    if (i != j)
                        table[j] = table[i];
                    ++j;
                }
            }

#ifdef CRCDEBUG
            fprintf(stderr, "Discarded %d flagged collisions.\n", usedsize - j);
#endif
            usedsize = j;
        }
    }

    if (usedsize < maxsize) {
#ifdef CRCDEBUG
        fprintf(stderr, "Shrinking table from %d to %d..\n", maxsize, usedsize);
        table = realloc(table, usedsize * sizeof(struct errorinfo));
#endif
    }

    *size_out = usedsize;

#ifdef CRCDEBUG
    {
        // Check the table.
        unsigned char *msg = malloc(bits/8);

        for (i = 0; i < usedsize; ++i) {
            int j;
            struct errorinfo *ei;
            uint32_t result;

            memset(msg, 0, bits/8);
            ei = &table[i];
            for (j = 0; j < ei->errors; ++j) {
                msg[ei->bit[j] >> 3] ^= 1 << (7 - (ei->bit[j]&7));
            }

            result = modesChecksum(msg, bits);
            if (result != ei->syndrome) {
                fprintf(stderr, "PROBLEM: entry %6d/%6d  syndrome %06x  errors %d  bits ", i, usedsize, ei->syndrome, ei->errors);
                for (j = 0; j < ei->errors; ++j)
                    fprintf(stderr, "%3d ", ei->bit[j]);
                fprintf(stderr, " checksum %06x\n", result);
            }
        }
        free(msg);

        // Show the table stats
        fprintf(stderr, "Syndrome table summary:\n");
        for (i = 1; i <= max_correct; ++i) {
            int j, count, possible;

            count = 0;
            for (j = 0; j < usedsize; ++j)
                if (table[j].errors == i)
                    ++count;

            possible = combinations(bits, i);
            fprintf(stderr, "  %d entries for %d-bit errors (%d possible, %d%% coverage)\n", count, i, possible, 100 * count / possible);
        }

        fprintf(stderr, "  %d entries total\n", usedsize);
    }
#endif

    return table;
}

// Precompute syndrome tables for 56- and 112-bit messages.
void modesChecksumInit(int fixBits)
{
    initLookupTables();

    switch (fixBits) {
    case 0:
        bitErrorTable_short = bitErrorTable_long = NULL;
        bitErrorTableSize_short = bitErrorTableSize_long = 0;
        break;

    case 1:
        // For 1 bit correction, we have 100% coverage up to 4 bit detection, so don't bother
        // with flagging collisions there.
        bitErrorTable_short = prepareErrorTable(MODES_SHORT_MSG_BITS, 1, 1, &bitErrorTableSize_short);
        bitErrorTable_long = prepareErrorTable(MODES_LONG_MSG_BITS, 1, 1, &bitErrorTableSize_long);
        break;

    default:
        // Detect out to 4 bit errors; this reduces our 2-bit coverage to about 65%.
        // This can take a little while - tell the user.
        fprintf(stderr, "Preparing error correction tables.. ");
        bitErrorTable_short = prepareErrorTable(MODES_SHORT_MSG_BITS, 2, 4, &bitErrorTableSize_short);
        bitErrorTable_long = prepareErrorTable(MODES_LONG_MSG_BITS, 2, 4, &bitErrorTableSize_long);
        fprintf(stderr, "done.\n");
        break;
    }
}

// Given an error syndrome and message length, return
// an error-correction descriptor, or NULL if the
// syndrome is uncorrectable
struct errorinfo *modesChecksumDiagnose(uint32_t syndrome, int bitlen)
{
    struct errorinfo *table;
    int tablesize;

    struct errorinfo ei;

    if (syndrome == 0)
        return &NO_ERRORS;

    assert (bitlen == 56 || bitlen == 112);
    if (bitlen == 56) { table = bitErrorTable_short; tablesize = bitErrorTableSize_short; }
    else { table = bitErrorTable_long; tablesize = bitErrorTableSize_long; }

    if (!table)
        return NULL;

    ei.syndrome = syndrome;
    return bsearch(&ei, table, tablesize, sizeof(struct errorinfo), syndrome_compare);
}

// Given a message and an error-correction descriptor,
// apply the error correction to the given message.
void modesChecksumFix(uint8_t *msg, struct errorinfo *info)
{
    int i;

    if (!info)
        return;

    for (i = 0; i < info->errors; ++i)
        msg[info->bit[i] >> 3] ^= 1 << (7 - (info->bit[i] & 7));
}

#ifdef CRCDEBUG
int main(int argc, char **argv)
{
    int shortlen, longlen;
    int i;
    struct errorinfo *shorttable, *longtable;

    if (argc < 3) {
        fprintf(stderr, "syntax: crctests <ncorrect> <ndetect>\n");
        return 1;
    }

    initLookupTables();
    shorttable = prepareErrorTable(MODES_SHORT_MSG_BITS, atoi(argv[1]), atoi(argv[2]), &shortlen);
    longtable = prepareErrorTable(MODES_LONG_MSG_BITS, atoi(argv[1]), atoi(argv[2]), &longlen);

    // check for DF11 correction syndromes where there is a syndrome with lower 7 bits all zero
    // (which would be used for DF11 error correction), but there's also a syndrome which has
    // the same upper 17 bits but nonzero lower 7 bits.

    // empirically, with ncorrect=1 ndetect=2 we get no ambiguous syndromes;
    // for ncorrect=2 ndetect=4 we get 11 ambiguous syndromes:

    /*
  syndrome 1 = 000C00  bits=[ 44 45 ]
  syndrome 2 = 000C1B  bits=[ 30 43 ]

  syndrome 1 = 001400  bits=[ 43 45 ]
  syndrome 2 = 00141B  bits=[ 30 44 ]

  syndrome 1 = 001800  bits=[ 43 44 ]
  syndrome 2 = 00181B  bits=[ 30 45 ]

  syndrome 1 = 001800  bits=[ 43 44 ]
  syndrome 2 = 001836  bits=[ 29 42 ]

  syndrome 1 = 002400  bits=[ 42 45 ]
  syndrome 2 = 00242D  bits=[ 29 30 ]

  syndrome 1 = 002800  bits=[ 42 44 ]
  syndrome 2 = 002836  bits=[ 29 43 ]

  syndrome 1 = 003000  bits=[ 42 43 ]
  syndrome 2 = 003036  bits=[ 29 44 ]

  syndrome 1 = 003000  bits=[ 42 43 ]
  syndrome 2 = 00306C  bits=[ 28 41 ]

  syndrome 1 = 004800  bits=[ 41 44 ]
  syndrome 2 = 00485A  bits=[ 28 29 ]

  syndrome 1 = 005000  bits=[ 41 43 ]
  syndrome 2 = 00506C  bits=[ 28 42 ]

  syndrome 1 = 006000  bits=[ 41 42 ]
  syndrome 2 = 00606C  bits=[ 28 43 ]
    */

    // So in the DF11 correction logic, we just discard messages that require more than a 1 bit fix.

    fprintf(stderr, "checking %d syndromes for DF11 collisions..\n", shortlen);
    for (i = 0; i < shortlen; ++i) {
        if ((shorttable[i].syndrome & 0xFF) == 0) {
            int j;
            // all syndromes with the same first 17 bits should sort immediately after entry i,
            // so this is fairly easy
            for (j = i+1; j < shortlen; ++j) {
                if ((shorttable[i].syndrome & 0xFFFF80) == (shorttable[j].syndrome & 0xFFFF80)) {
                    int k;
                    int mismatch = 0;

                    // we don't care if the only differences are in bits that lie in the checksum
                    for (k = 0; k < shorttable[i].errors; ++k) {
                        int l, matched = 0;

                        if (shorttable[i].bit[k] >= 49)
                            continue; // bit is in the final 7 bits, we don't care

                        for (l = 0; l < shorttable[j].errors; ++l) {
                            if (shorttable[i].bit[k] == shorttable[j].bit[l]) {
                                matched = 1;
                                break;
                            }
                        }

                        if (!matched)
                            mismatch = 1;
                    }

                    for (k = 0; k < shorttable[j].errors; ++k) {
                        int l, matched = 0;

                        if (shorttable[j].bit[k] >= 49)
                            continue; // bit is in the final 7 bits, we don't care

                        for (l = 0; l < shorttable[i].errors; ++l) {
                            if (shorttable[j].bit[k] == shorttable[i].bit[l]) {
                                matched = 1;
                                break;
                            }
                        }

                        if (!matched)
                            mismatch = 1;
                    }

                    if (mismatch) {
                        fprintf(stderr,
                                "DF11 correction collision: \n"
                                "  syndrome 1 = %06X  bits=[",
                                shorttable[i].syndrome);
                        for (k = 0; k < shorttable[i].errors; ++k)
                            fprintf(stderr, " %d", shorttable[i].bit[k]);
                        fprintf(stderr, " ]\n");

                        fprintf(stderr,
                                "  syndrome 2 = %06X  bits=[",
                                shorttable[j].syndrome);
                        for (k = 0; k < shorttable[j].errors; ++k)
                            fprintf(stderr, " %d", shorttable[j].bit[k]);
                        fprintf(stderr, " ]\n");
                    }
                } else {
                    break;
                }
            }
        }
    }

    free(shorttable);
    free(longtable);

    return 0;
}
#endif
