// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// interactive.c: aircraft tracking and interactive display
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

// This file incorporates work covered by the following copyright and  
// permission notice:
//
//   Copyright (C) 2012 by Salvatore Sanfilippo <antirez@gmail.com>
//
//   All rights reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions are
//   met:
//
//    *  Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//    *  Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "dump1090.h"

#include <curses.h>

//
//========================= Interactive mode ===============================


static int convert_altitude(int ft)
{
    if (Modes.metric)
        return (ft * 0.3048);
    else
        return ft;
}

static int convert_speed(int kts)
{
    if (Modes.metric)
        return (kts * 1.852);
    else
        return kts;
}

//
//=========================================================================
//
// Show the currently captured interactive data on screen.
//

void interactiveInit() {
    if (!Modes.interactive)
        return;

    initscr();
    clear();
    refresh();

    mvprintw(0, 0, " Hex    Mode  Sqwk  Flight   Alt    Spd  Hdg    Lat      Long   RSSI  Msgs  Ti");
    mvhline(1, 0, ACS_HLINE, 80);
}

void interactiveCleanup(void) {
    if (Modes.interactive) {
        endwin();
    }
}

void interactiveShowData(void) {
    struct aircraft *a = Modes.aircrafts;
    static uint64_t next_update;
    uint64_t now = mstime();
    char progress;
    char spinner[4] = "|/-\\";

    // Refresh screen every (MODES_INTERACTIVE_REFRESH_TIME) miliseconde
    if (now < next_update)
        return;

    next_update = now + MODES_INTERACTIVE_REFRESH_TIME;

    progress = spinner[(now/1000)%4];
    mvaddch(0, 79, progress);

    int rows = getmaxy(stdscr);
    int row = 2;

    while (a && row < rows) {

        if ((now - a->seen) < Modes.interactive_display_ttl)
            {
            int msgs  = a->messages;

            if (msgs > 1) {
                char strSquawk[5] = " ";
                char strFl[7]     = " ";
                char strTt[5]     = " ";
                char strGs[5]     = " ";

                if (trackDataValid(&a->squawk_valid)) {
                    snprintf(strSquawk,5,"%04x", a->squawk);
                }

                if (trackDataValid(&a->speed_valid)) {
                    snprintf (strGs, 5,"%3d", convert_speed(a->speed));
                }

                if (trackDataValid(&a->heading_valid)) {
                    snprintf (strTt, 5,"%03d", a->heading);
                }

                if (msgs > 99999) {
                    msgs = 99999;
                }

                char strMode[5]               = "    ";
                char strLat[8]                = " ";
                char strLon[9]                = " ";
                double * pSig                 = a->signalLevel;
                double signalAverage = (pSig[0] + pSig[1] + pSig[2] + pSig[3] +
                                        pSig[4] + pSig[5] + pSig[6] + pSig[7]) / 8.0;

                strMode[0] = 'S';
                if (a->modeA_hit) {strMode[2] = 'a';}
                if (a->modeC_hit) {strMode[3] = 'c';}

                if (trackDataValid(&a->position_valid)) {
                    snprintf(strLat, 8,"%7.03f", a->lat);
                    snprintf(strLon, 9,"%8.03f", a->lon);
                }

                if (trackDataValid(&a->airground_valid) && a->airground == AG_GROUND) {
                    snprintf(strFl, 7," grnd");
                } else if (Modes.use_gnss && trackDataValid(&a->altitude_gnss_valid)) {
                    snprintf(strFl, 7, "%5dH", convert_altitude(a->altitude_gnss));
                } else if (trackDataValid(&a->altitude_valid)) {
                    snprintf(strFl, 7, "%5d ", convert_altitude(a->altitude));
                }

                mvprintw(row, 0, "%s%06X %-4s  %-4s  %-8s %6s %3s  %3s  %7s %8s %5.1f %5d %2.0f",
                         (a->addr & MODES_NON_ICAO_ADDRESS) ? "~" : " ", (a->addr & 0xffffff),
                         strMode, strSquawk, a->callsign, strFl, strGs, strTt,
                         strLat, strLon, 10 * log10(signalAverage), msgs, (now - a->seen)/1000.0);
                ++row;
            }
        }
        a = a->next;
    }

    if (Modes.mode_ac) {
        for (unsigned i = 1; i < 4096 && row < rows; ++i) {
            if (modeAC_match[i] || modeAC_count[i] < 50 || modeAC_age[i] > 5)
                continue;

            char strMode[5] = "  A ";
            char strFl[7] = " ";
            unsigned modeA = indexToModeA(i);
            int modeC = modeAToModeC(modeA);
            if (modeC != INVALID_ALTITUDE) {
                strMode[3] = 'C';
                snprintf(strFl, 7, "%5d ", convert_altitude(modeC * 100));
            }

            mvprintw(row, 0,
                     "%7s %-4s  %04x  %-8s %6s %3s  %3s  %7s %8s %5s %5d %2d\n",
                     "",    /* address */
                     strMode, /* mode */
                     modeA, /* squawk */
                     "",    /* callsign */
                     strFl, /* altitude */
                     "",    /* gs */
                     "",    /* heading */
                     "",    /* lat */
                     "",    /* lon */
                     "",    /* signal */
                     modeAC_count[i], /* messages */
                     modeAC_age[i]);  /* age */
            ++row;
        }
    }

    move(row, 0);
    clrtobot();
    refresh();
}

//
//=========================================================================
//
