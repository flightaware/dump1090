// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// demod_2400.h: 2.4MHz Mode S demodulator prototypes.
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

#ifndef DUMP1090_DEMOD_2400_H
#define DUMP1090_DEMOD_2400_H

#include <stdint.h>

struct mag_buf;

void demodulate2400(struct mag_buf *mag);
void demodulate2400AC(struct mag_buf *mag);

#endif
