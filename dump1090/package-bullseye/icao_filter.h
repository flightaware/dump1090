// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// icao_filter.c: prototypes for ICAO address hashtable
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

#ifndef DUMP1090_ICAO_FILTER_H
#define DUMP1090_ICAO_FILTER_H

// Special address bit used to mark ADS-B (NT) emitters
#define ICAO_FILTER_ADSB_NT (1 << 25)

// Call once:
void icaoFilterInit();

// Add an address to the filter
void icaoFilterAdd(uint32_t addr);

// Test if the given address matches the filter
int icaoFilterTest(uint32_t addr);

// Test if the top 16 bits match any previously added address.
// If they do, returns an arbitrary one of the matched
// addresses. Returns 0 on failure.
uint32_t icaoFilterTestFuzzy(uint32_t partial);

// Call this periodically to allow the filter to expire
// old entries.
void icaoFilterExpire();

#endif
