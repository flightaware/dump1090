// Part of dump1090, a Mode S message decoder
//
// sdr_ad9361.h: AD9361 One support (header)
//
// Copyright (c) 2019 FlightAware LLC
// Copyright (c) 2025 Deepak Shandilya (deepakshekar@gmail.com)
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


#ifndef AD9361_H
#define AD9361_H

void ad9361InitConfig();
void ad9361ShowHelp();
bool ad9361HandleOption(int argc, char **argv, int *jptr);
bool ad9361Open();
void ad9361Run();
void ad9361Close();

#endif
