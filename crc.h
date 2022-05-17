// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// crc.h: Mode S checksum prototypes.
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

#ifndef DUMP1090_CRC_H
#define DUMP1090_CRC_H

#include <stdint.h>

// Global max for fixable bit erros
#define MODES_MAX_BITERRORS 2

struct errorinfo {
    uint32_t syndrome;                 // CRC syndrome
    int      errors;                   // number of errors
    int8_t   bit[MODES_MAX_BITERRORS]; // bit positions to fix (-1 = no bit)
};

void modesChecksumInit(int fixBits);
uint32_t modesChecksum(const uint8_t *msg, int bitlen);
struct errorinfo *modesChecksumDiagnose(uint32_t syndrome, int bitlen);
void modesChecksumFix(uint8_t *msg, struct errorinfo *info);

#endif
