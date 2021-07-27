// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// sdr_stub.c: generic SDR infrastructure, stubbed out to do nothing
//
// Copyright (c) 2021 FlightAware LLC
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

void sdrInitConfig()
{
    /* nothing */
}

void sdrShowHelp()
{
    /* nothing */
}

bool sdrHandleOption(int argc, char **argv, int *jptr)
{
    MODES_NOTUSED(argc);
    MODES_NOTUSED(argv);
    MODES_NOTUSED(jptr);
    return false;
}

bool sdrOpen()
{
    return false;
}

void sdrRun()
{
    /* nothing */
}

void sdrStop()
{
    /* nothing */
}

void sdrClose()
{
    /* nothing */
}

void sdrMonitor()
{
    /* nothing */
}

void sdrUpdateCPUTime(struct timespec *addTo)
{
    MODES_NOTUSED(addTo);
}

int sdrGetGain()
{
    return -1;
}

int sdrGetMaxGain()
{
    return -1;
}

double sdrGetGainDb(int step)
{
    MODES_NOTUSED(step);
    return 0.0;
}

int sdrSetGain(int step)
{
    MODES_NOTUSED(step);
    return -1;
}


