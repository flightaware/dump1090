// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// sdr_bladerf.h: UHD support (header)
//
// Copyright (c) 2017 FlightAware LLC
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

#ifndef UHD_H
#define UHD_H

#include <uhd.h>

// Support for UHD SDR

void uhdInitConfig();
void uhdShowHelp();
bool uhdHandleOption(int argc, char **argv, int *jptr);
bool uhdOpen();
void uhdRun();
void uhdClose();

#endif
