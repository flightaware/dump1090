// Part of dump1090-soapysdr, a Mode S message decoder for SOAPYSDR devices.
//
// sdr_soapysdr.h: soapysdr supported SDR device support (header)
//
// Copyright (c) 2022 Ralf Heckhausen <ralf@avionix.eu>
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

#ifndef SDR_SOAPYSDR_H
#define SDR_SOAPYSDR_H
#include <SoapySDR/Device.h>

void soapysdrInitConfig();
void soapysdrShowHelp();
bool soapysdrHandleOption(int argc, char **argv, int *jptr);
bool soapysdrOpen();
void soapysdrRun();
void soapysdrStop();
void soapysdrClose();

int soapysdrGetGain();
int soapysdrGetMaxGain();
double soapysdrGetGainDb(int step);
int soapysdrSetGain(int step);
bool getDeviceInfo();

#endif
