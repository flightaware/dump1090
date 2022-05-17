// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// sdr.h: generic SDR infrastructure (header)
//
// Copyright (c) 2016-2017 Oliver Jowett <oliver@mutability.co.uk>
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

#ifndef SDR_H
#define SDR_H

// Common interface to different SDR inputs.

void sdrInitConfig();
void sdrShowHelp();
bool sdrHandleOption(int argc, char **argv, int *jptr);
bool sdrOpen();
void sdrRun();
void sdrStop();
void sdrClose();

// Gain control
int sdrGetGain();              // return current gain step 0..N, or -1 if gain control is not supported
int sdrGetMaxGain();           // return maximum gain step, or -1 if gain control is not supported
double sdrGetGainDb(int step); // return gain in dB for the given gain step, or 0.0 if gain control is not supported
int sdrSetGain(int step);      // set gain step; return actual gain step used, or -1 if gain control is not supported

// Call periodically from the SDR read thread to update reader thread CPU stats:
void sdrMonitor();
// Retrieve CPU stats and add new CPU time to *addTo
void sdrUpdateCPUTime(struct timespec *addTo);

#endif
