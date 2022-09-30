// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// cprtests.c - tests for CPR decoder
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


#include <math.h>
#include <stdio.h>

#include "cpr.h"

// Global, airborne CPR test data:
static const struct {
    int even_cprlat, even_cprlon;   // input: raw CPR values, even message
    int odd_cprlat, odd_cprlon;     // input: raw CPR values, odd message
    int even_result;                // verify: expected result from decoding with fflag=0 (even message is latest)
    double even_rlat, even_rlon;    // verify: expected position from decoding with fflag=0 (even message is latest)
    int odd_result;                 // verify: expected result from decoding with fflag=1 (odd message is latest)
    double odd_rlat, odd_rlon;      // verify: expected position from decoding with fflag=1 (odd message is latest)
} cprGlobalAirborneTests[] = {
    { 80536, 9432, 61720, 9192, 0, 51.686646, 0.700156, 0, 51.686763, 0.701294 },
    { 80534, 9413, 61714, 9144, 0, 51.686554, 0.698745, 0, 51.686484, 0.697632 },

    // todo: more positions, bad data
};

// Global, surface CPR test data:
static const struct {
    double reflat, reflon;          // input: reference location for decoding
    int even_cprlat, even_cprlon;   // input: raw CPR values, even message
    int odd_cprlat, odd_cprlon;     // input: raw CPR values, odd message
    int even_result;                // verify: expected result from decoding with fflag=0 (even message is latest)
    double even_rlat, even_rlon;    // verify: expected position from decoding with fflag=0 (even message is latest)
    int odd_result;                 // verify: expected result from decoding with fflag=1 (odd message is latest)
    double odd_rlat, odd_rlon;      // verify: expected position from decoding with fflag=1 (odd message is latest)
} cprGlobalSurfaceTests[] = {
    // The real position received here was on the Cambridge (UK) airport apron at 52.21N 0.177E
    // We mess with the reference location to check that the right quadrant is used.

    // longitude quadrants:
    { 52.00, -180.00, 105730, 9259, 29693, 8997, 0, 52.209984, 0.176601 - 180.0, 0, 52.209976, 0.176507 - 180.0 },
    { 52.00, -140.00, 105730, 9259, 29693, 8997, 0, 52.209984, 0.176601 - 180.0, 0, 52.209976, 0.176507 - 180.0 },
    { 52.00, -130.00, 105730, 9259, 29693, 8997, 0, 52.209984, 0.176601 - 90.0,  0, 52.209976, 0.176507 - 90.0 },
    { 52.00,  -50.00, 105730, 9259, 29693, 8997, 0, 52.209984, 0.176601 - 90.0,  0, 52.209976, 0.176507 - 90.0 },
    { 52.00,  -40.00, 105730, 9259, 29693, 8997, 0, 52.209984, 0.176601,         0, 52.209976, 0.176507 },
    { 52.00,  -10.00, 105730, 9259, 29693, 8997, 0, 52.209984, 0.176601,         0, 52.209976, 0.176507 },
    { 52.00,    0.00, 105730, 9259, 29693, 8997, 0, 52.209984, 0.176601,         0, 52.209976, 0.176507 },
    { 52.00,   10.00, 105730, 9259, 29693, 8997, 0, 52.209984, 0.176601,         0, 52.209976, 0.176507 },
    { 52.00,   40.00, 105730, 9259, 29693, 8997, 0, 52.209984, 0.176601,         0, 52.209976, 0.176507 },
    { 52.00,   50.00, 105730, 9259, 29693, 8997, 0, 52.209984, 0.176601 + 90.0,  0, 52.209976, 0.176507 + 90.0 },
    { 52.00,  130.00, 105730, 9259, 29693, 8997, 0, 52.209984, 0.176601 + 90.0,  0, 52.209976, 0.176507 + 90.0 },
    { 52.00,  140.00, 105730, 9259, 29693, 8997, 0, 52.209984, 0.176601 - 180.0, 0, 52.209976, 0.176507 - 180.0 },
    { 52.00,  180.00, 105730, 9259, 29693, 8997, 0, 52.209984, 0.176601 - 180.0, 0, 52.209976, 0.176507 - 180.0 },

    // latitude quadrants (but only 2). The decoded longitude also changes because the cell size changes with latitude
    {  90.00,   0.00, 105730, 9259, 29693, 8997, 0, 52.209984,        0.176601,  0, 52.209976,        0.176507 },
    {  52.00,   0.00, 105730, 9259, 29693, 8997, 0, 52.209984,        0.176601,  0, 52.209976,        0.176507 },
    {   8.00,   0.00, 105730, 9259, 29693, 8997, 0, 52.209984,        0.176601,  0, 52.209976,        0.176507 },
    {   7.00,   0.00, 105730, 9259, 29693, 8997, 0, 52.209984 - 90.0, 0.135269,  0, 52.209976 - 90.0, 0.134299 },
    { -52.00,   0.00, 105730, 9259, 29693, 8997, 0, 52.209984 - 90.0, 0.135269,  0, 52.209976 - 90.0, 0.134299 },
    { -90.00,   0.00, 105730, 9259, 29693, 8997, 0, 52.209984 - 90.0, 0.135269,  0, 52.209976 - 90.0, 0.134299 },

    // poles/equator cases
    { -46.00, -180.00,  0, 0,  0, 0,  0, -90.0, -180.000000,  0, -90.0, -180.0 },   // south pole
    { -44.00, -180.00,  0, 0,  0, 0,  0,   0.0, -180.000000,  0,   0.0, -180.0 },   // equator
    {  44.00, -180.00,  0, 0,  0, 0,  0,   0.0, -180.000000,  0,   0.0, -180.0 },   // equator
    {  46.00, -180.00,  0, 0,  0, 0,  0,  90.0, -180.000000,  0,  90.0, -180.0 },   // north pole

};

// Relative CPR test data:
static const struct {
    double reflat, reflon;          // input: reference location for decoding
    int cprlat, cprlon;             // input: raw CPR values, even or odd message
    int fflag;                      // input: fflag in raw message
    int surface;                    // input: decode as air (0) or surface (1) position
    int result;                     // verify: expected result
    double rlat, rlon;              // verify: expected position
} cprRelativeTests[] = {
    //
    // AIRBORNE
    //

    { 52.00, 0.00, 80536, 9432, 0, 0, 0, 51.686646, 0.700156 },  // even, airborne
    { 52.00, 0.00, 61720, 9192, 1, 0, 0, 51.686763, 0.701294 },  // odd, airborne
    { 52.00, 0.00, 80534, 9413, 0, 0, 0, 51.686554, 0.698745 },  // even, airborne
    { 52.00, 0.00, 61714, 9144, 1, 0, 0, 51.686484, 0.697632 },  // odd, airborne

    // test moving the receiver around a bit
    // We cannot move it more than 1/2 cell away before ambiguity happens.

    // latitude must be within about 3 degrees (cell size is 360/60 = 6 degrees)
    { 48.70, 0.00, 80536, 9432, 0, 0, 0, 51.686646, 0.700156 },  // even, airborne
    { 48.70, 0.00, 61720, 9192, 1, 0, 0, 51.686763, 0.701294 },  // odd, airborne
    { 48.70, 0.00, 80534, 9413, 0, 0, 0, 51.686554, 0.698745 },  // even, airborne
    { 48.70, 0.00, 61714, 9144, 1, 0, 0, 51.686484, 0.697632 },  // odd, airborne
    { 54.60, 0.00, 80536, 9432, 0, 0, 0, 51.686646, 0.700156 },  // even, airborne
    { 54.60, 0.00, 61720, 9192, 1, 0, 0, 51.686763, 0.701294 },  // odd, airborne
    { 54.60, 0.00, 80534, 9413, 0, 0, 0, 51.686554, 0.698745 },  // even, airborne
    { 54.60, 0.00, 61714, 9144, 1, 0, 0, 51.686484, 0.697632 },  // odd, airborne

    // longitude must be within about 4.8 degrees at this latitude
    { 52.00, 5.40, 80536, 9432, 0, 0, 0, 51.686646, 0.700156 },  // even, airborne
    { 52.00, 5.40, 61720, 9192, 1, 0, 0, 51.686763, 0.701294 },  // odd, airborne
    { 52.00, 5.40, 80534, 9413, 0, 0, 0, 51.686554, 0.698745 },  // even, airborne
    { 52.00, 5.40, 61714, 9144, 1, 0, 0, 51.686484, 0.697632 },  // odd, airborne
    { 52.00, -4.10, 80536, 9432, 0, 0, 0, 51.686646, 0.700156 },  // even, airborne
    { 52.00, -4.10, 61720, 9192, 1, 0, 0, 51.686763, 0.701294 },  // odd, airborne
    { 52.00, -4.10, 80534, 9413, 0, 0, 0, 51.686554, 0.698745 },  // even, airborne
    { 52.00, -4.10, 61714, 9144, 1, 0, 0, 51.686484, 0.697632 },  // odd, airborne

    //
    // SURFACE
    //

    // Surface position on the Cambridge (UK) airport apron at 52.21N 0.18E
    { 52.00,    0.00, 105730, 9259, 0, 1, 0, 52.209984, 0.176601 },   // even, surface
    { 52.00,    0.00,  29693, 8997, 1, 1, 0, 52.209976, 0.176507 },   // odd, surface

    // test moving the receiver around a bit
    // We cannot move it more than 1/2 cell away before ambiguity happens.

    // latitude must be within about 0.75 degrees (cell size is 90/60 = 1.5 degrees)
    { 51.46,    0.00, 105730, 9259, 0, 1, 0, 52.209984, 0.176601 },   // even, surface
    { 51.46,    0.00,  29693, 8997, 1, 1, 0, 52.209976, 0.176507 },   // odd, surface
    { 52.95,    0.00, 105730, 9259, 0, 1, 0, 52.209984, 0.176601 },   // even, surface
    { 52.95,    0.00,  29693, 8997, 1, 1, 0, 52.209976, 0.176507 },   // odd, surface

    // longitude must be within about 1.25 degrees at this latitude
    { 52.00,    1.40, 105730, 9259, 0, 1, 0, 52.209984, 0.176601 },   // even, surface
    { 52.00,    1.40,  29693, 8997, 1, 1, 0, 52.209976, 0.176507 },   // odd, surface
    { 52.00,   -1.05, 105730, 9259, 0, 1, 0, 52.209984, 0.176601 },   // even, surface
    { 52.00,   -1.05,  29693, 8997, 1, 1, 0, 52.209976, 0.176507 },   // odd, surface
};

static int testCPRGlobalAirborne() {
    int ok = 1;
    unsigned i;
    for (i = 0; i < sizeof(cprGlobalAirborneTests)/sizeof(cprGlobalAirborneTests[0]); ++i) {
        double rlat = 0, rlon = 0;
        int res;

        res = decodeCPRairborne(cprGlobalAirborneTests[i].even_cprlat, cprGlobalAirborneTests[i].even_cprlon,
                                cprGlobalAirborneTests[i].odd_cprlat, cprGlobalAirborneTests[i].odd_cprlon,
                                0,
                                &rlat, &rlon);
        if (res != cprGlobalAirborneTests[i].even_result
            || fabs(rlat - cprGlobalAirborneTests[i].even_rlat) > 1e-6
            || fabs(rlon - cprGlobalAirborneTests[i].even_rlon) > 1e-6) {
            ok = 0;
            fprintf(stderr,
                    "testCPRGlobalAirborne[%u,EVEN]: FAIL: decodeCPRairborne(%d,%d,%d,%d,EVEN) failed:\n"
                    " result %d  (expected %d)\n"
                    " lat %.6f   (expected %.6f)\n"
                    " lon %.6f   (expected %.6f)\n",
                    i,
                    cprGlobalAirborneTests[i].even_cprlat, cprGlobalAirborneTests[i].even_cprlon,
                    cprGlobalAirborneTests[i].odd_cprlat, cprGlobalAirborneTests[i].odd_cprlon,
                    res, cprGlobalAirborneTests[i].even_result,
                    rlat, cprGlobalAirborneTests[i].even_rlat,
                    rlon, cprGlobalAirborneTests[i].even_rlon);
        } else {
            fprintf(stderr, "testCPRGlobalAirborne[%u,EVEN]: PASS\n", i);
        }

        res = decodeCPRairborne(cprGlobalAirborneTests[i].even_cprlat, cprGlobalAirborneTests[i].even_cprlon,
                                cprGlobalAirborneTests[i].odd_cprlat, cprGlobalAirborneTests[i].odd_cprlon,
                                1,
                                &rlat, &rlon);
        if (res != cprGlobalAirborneTests[i].odd_result
            || fabs(rlat - cprGlobalAirborneTests[i].odd_rlat) > 1e-6
            || fabs(rlon - cprGlobalAirborneTests[i].odd_rlon) > 1e-6) {
            ok = 0;
            fprintf(stderr,
                    "testCPRGlobalAirborne[%u,ODD]:  FAIL: decodeCPRairborne(%d,%d,%d,%d,ODD) failed:\n"
                    " result %d  (expected %d)\n"
                    " lat %.6f   (expected %.6f)\n"
                    " lon %.6f   (expected %.6f)\n",
                    i,
                    cprGlobalAirborneTests[i].even_cprlat, cprGlobalAirborneTests[i].even_cprlon,
                    cprGlobalAirborneTests[i].odd_cprlat, cprGlobalAirborneTests[i].odd_cprlon,
                    res, cprGlobalAirborneTests[i].odd_result,
                    rlat, cprGlobalAirborneTests[i].odd_rlat,
                    rlon, cprGlobalAirborneTests[i].odd_rlon);
        } else {
            fprintf(stderr, "testCPRGlobalAirborne[%u,ODD]:  PASS\n", i);
        }
    }

    return ok;
}

static int testCPRGlobalSurface() {
    int ok = 1;
    unsigned i;
    for (i = 0; i < sizeof(cprGlobalSurfaceTests)/sizeof(cprGlobalSurfaceTests[0]); ++i) {
        double rlat = 0, rlon = 0;
        int res;

        res = decodeCPRsurface(cprGlobalSurfaceTests[i].reflat, cprGlobalSurfaceTests[i].reflon,
                               cprGlobalSurfaceTests[i].even_cprlat, cprGlobalSurfaceTests[i].even_cprlon,
                               cprGlobalSurfaceTests[i].odd_cprlat, cprGlobalSurfaceTests[i].odd_cprlon,
                               0,
                               &rlat, &rlon);
        if (res != cprGlobalSurfaceTests[i].even_result
            || fabs(rlat - cprGlobalSurfaceTests[i].even_rlat) > 1e-6
            || fabs(rlon - cprGlobalSurfaceTests[i].even_rlon) > 1e-6) {
            ok = 0;
            fprintf(stderr,
                    "testCPRGlobalSurface[%u,EVEN]:  FAIL: decodeCPRsurface(%.6f,%.6f,%d,%d,%d,%d,EVEN) failed:\n"
                    " result %d  (expected %d)\n"
                    " lat %.6f   (expected %.6f)\n"
                    " lon %.6f   (expected %.6f)\n",
                    i,
                    cprGlobalSurfaceTests[i].reflat, cprGlobalSurfaceTests[i].reflon,
                    cprGlobalSurfaceTests[i].even_cprlat, cprGlobalSurfaceTests[i].even_cprlon,
                    cprGlobalSurfaceTests[i].odd_cprlat, cprGlobalSurfaceTests[i].odd_cprlon,
                    res, cprGlobalSurfaceTests[i].even_result,
                    rlat, cprGlobalSurfaceTests[i].even_rlat,
                    rlon, cprGlobalSurfaceTests[i].even_rlon);
        } else {
            fprintf(stderr, "testCPRGlobalSurface[%u,EVEN]:  PASS\n", i);
        }

        res = decodeCPRsurface(cprGlobalSurfaceTests[i].reflat, cprGlobalSurfaceTests[i].reflon,
                               cprGlobalSurfaceTests[i].even_cprlat, cprGlobalSurfaceTests[i].even_cprlon,
                               cprGlobalSurfaceTests[i].odd_cprlat, cprGlobalSurfaceTests[i].odd_cprlon,
                               1,
                               &rlat, &rlon);
        if (res != cprGlobalSurfaceTests[i].odd_result
            || fabs(rlat - cprGlobalSurfaceTests[i].odd_rlat) > 1e-6
            || fabs(rlon - cprGlobalSurfaceTests[i].odd_rlon) > 1e-6) {
            ok = 0;
            fprintf(stderr,
                    "testCPRGlobalSurface[%u,ODD]:   FAIL: decodeCPRsurface(%.6f,%.6f,%d,%d,%d,%d,ODD) failed:\n"
                    " result %d  (expected %d)\n"
                    " lat %.6f   (expected %.6f)\n"
                    " lon %.6f   (expected %.6f)\n",
                    i,
                    cprGlobalSurfaceTests[i].reflat, cprGlobalSurfaceTests[i].reflon,
                    cprGlobalSurfaceTests[i].even_cprlat, cprGlobalSurfaceTests[i].even_cprlon,
                    cprGlobalSurfaceTests[i].odd_cprlat, cprGlobalSurfaceTests[i].odd_cprlon,
                    res, cprGlobalSurfaceTests[i].odd_result,
                    rlat, cprGlobalSurfaceTests[i].odd_rlat,
                    rlon, cprGlobalSurfaceTests[i].odd_rlon);
        } else {
            fprintf(stderr, "testCPRGlobalSurface[%u,ODD]:   PASS\n", i);
        }
    }

    return ok;
}

static int testCPRRelative() {
    int ok = 1;
    unsigned i;
    for (i = 0; i < sizeof(cprRelativeTests)/sizeof(cprRelativeTests[0]); ++i) {
        double rlat = 0, rlon = 0;
        int res;

        res = decodeCPRrelative(cprRelativeTests[i].reflat, cprRelativeTests[i].reflon,
                                cprRelativeTests[i].cprlat, cprRelativeTests[i].cprlon,
                                cprRelativeTests[i].fflag, cprRelativeTests[i].surface,
                                &rlat, &rlon);
        if (res != cprRelativeTests[i].result
            || fabs(rlat - cprRelativeTests[i].rlat) > 1e-6
            || fabs(rlon - cprRelativeTests[i].rlon) > 1e-6) {
            ok = 0;
            fprintf(stderr,
                    "testCPRRelative[%u]:  FAIL: decodeCPRrelative(%.6f,%.6f,%d,%d,%d,%d) failed:\n"
                    " result %d  (expected %d)\n"
                    " lat %.6f   (expected %.6f)\n"
                    " lon %.6f   (expected %.6f)\n",
                    i,
                    cprRelativeTests[i].reflat, cprRelativeTests[i].reflon,
                    cprRelativeTests[i].cprlat, cprRelativeTests[i].cprlon,
                    cprRelativeTests[i].fflag, cprRelativeTests[i].surface,
                    res, cprRelativeTests[i].result,
                    rlat, cprRelativeTests[i].rlat,
                    rlon, cprRelativeTests[i].rlon);
        } else {
            fprintf(stderr, "testCPRRelative[%u]:  PASS\n", i);
        }
    }

    return ok;
}

int main(int __attribute__ ((unused)) argc, char __attribute__ ((unused)) **argv) {
    int ok = 1;
    ok = testCPRGlobalAirborne() && ok;
    ok = testCPRGlobalSurface() && ok;
    ok = testCPRRelative() && ok;
    return ok ? 0 : 1;
}
