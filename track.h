// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// track.h: aircraft state tracking prototypes
//
// Copyright (c) 2014-2016 Oliver Jowett <oliver@mutability.co.uk>
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

#ifndef DUMP1090_TRACK_H
#define DUMP1090_TRACK_H

/* Maximum age of a reliable tracked aircraft in milliseconds */
#define TRACK_AIRCRAFT_TTL 300000

/* Maximum age of an unreliable tracked aircraft, in milliseconds */
#define TRACK_AIRCRAFT_UNRELIABLE_TTL 60000

/* Maximum validity of an aircraft position */
#define TRACK_AIRCRAFT_POSITION_TTL 60000

/* Minimum number of repeated Mode A/C replies with a particular Mode A code needed in a
 * 1 second period before accepting that code.
 */
#define TRACK_MODEAC_MIN_MESSAGES 4

/* Minimum number of DF17 messages required to mark a track as reliable */
#define TRACK_RELIABLE_DF17_MESSAGES 2

/* Minimum number of DF11 messages required to mark a track as reliable */
#define TRACK_RELIABLE_DF11_MESSAGES 3

/* Minimum number of any sort of messages required to mark a track as reliable */
#define TRACK_RELIABLE_ANY_MESSAGES 5

/* Special value for Rc unknown */
#define RC_UNKNOWN 0

// data moves through three states:
//  fresh: data is valid. Updates from a less reliable source are not accepted.
//  stale: data is valid. Updates from a less reliable source are accepted.
//  expired: data is not valid.
typedef struct {
    uint64_t stale_interval;  /* how long after an update until the data is stale */
    uint64_t expire_interval; /* how long after an update until the data expires */

    datasource_t source;     /* where the data came from */
    uint64_t updated;        /* when it arrived */
    uint64_t stale;          /* when it goes stale */
    uint64_t expires;        /* when it expires */
} data_validity;

/* Structure used to describe the state of one tracked aircraft */
struct aircraft {
    uint32_t      addr;           // ICAO address
    addrtype_t    addrtype;       // highest priority address type seen for this aircraft

    uint64_t      seen;           // Time (millis) at which the last packet was received
    long          messages;       // Number of Mode S messages received

    int           reliable;       // Do we think this is a real aircraft, not noise?
    long          reliableDF11;   // Number of "reliable" DF11s (no CRC errors corrected, IID = 0) received
    long          reliableDF17;   // Number of "reliable" DF17s (no CRC errors corrected) received
    long          discarded;      // Number of messages discarded as possibly-noise

    double        signalLevel[8]; // Last 8 Signal Amplitudes
    int           signalNext;     // next index of signalLevel to use

    data_validity callsign_valid;
    char          callsign[9];     // Flight number
    int           callsign_matched;   // Interactive callsign filter matched

    data_validity altitude_baro_valid;
    int           altitude_baro;   // Altitude (Baro)

    data_validity altitude_geom_valid;
    int           altitude_geom;   // Altitude (Geometric)

    data_validity geom_delta_valid;
    int           geom_delta;      // Difference between Geometric and Baro altitudes

    data_validity gs_valid;
    float         gs;

    data_validity ias_valid;
    unsigned      ias;

    data_validity tas_valid;
    unsigned      tas;

    data_validity mach_valid;
    float         mach;

    data_validity track_valid;
    float         track;           // Ground track

    data_validity track_rate_valid;
    float         track_rate;      // Rate of change of ground track, degrees/second

    data_validity roll_valid;
    float         roll;            // Roll angle, degrees right

    data_validity mag_heading_valid;
    float         mag_heading;     // Magnetic heading

    data_validity true_heading_valid;
    float         true_heading;    // True heading

    data_validity baro_rate_valid;
    int           baro_rate;      // Vertical rate (barometric)

    data_validity geom_rate_valid;
    int           geom_rate;      // Vertical rate (geometric)

    data_validity squawk_valid;
    unsigned      squawk;         // Squawk

    data_validity emergency_valid;
    emergency_t   emergency;      // Emergency/priority status

    unsigned      category;       // Aircraft category A0 - D7 encoded as a single hex byte. 00 = unset

    data_validity airground_valid;
    airground_t   airground;      // air/ground status

    data_validity nav_qnh_valid;
    float         nav_qnh;        // Altimeter setting (QNH/QFE), millibars

    data_validity nav_altitude_mcp_valid;
    unsigned      nav_altitude_mcp;    // FCU/MCP selected altitude

    data_validity nav_altitude_fms_valid;
    unsigned      nav_altitude_fms;    // FMS selected altitude

    data_validity nav_altitude_src_valid;
    nav_altitude_source_t nav_altitude_src;  // source of altitude used by automation

    data_validity nav_heading_valid;
    float         nav_heading; // target heading, degrees (0-359)

    data_validity nav_modes_valid;
    nav_modes_t   nav_modes;  // enabled modes (autopilot, vnav, etc)

    data_validity cpr_odd_valid;        // Last seen even CPR message
    cpr_type_t    cpr_odd_type;
    unsigned      cpr_odd_lat;
    unsigned      cpr_odd_lon;
    unsigned      cpr_odd_nic;
    unsigned      cpr_odd_rc;

    data_validity cpr_even_valid;       // Last seen odd CPR message
    cpr_type_t    cpr_even_type;
    unsigned      cpr_even_lat;
    unsigned      cpr_even_lon;
    unsigned      cpr_even_nic;
    unsigned      cpr_even_rc;

    data_validity position_valid;
    double        lat, lon;       // Coordinates obtained from CPR encoded data
    unsigned      pos_nic;        // NIC of last computed position
    unsigned      pos_rc;         // Rc of last computed position

    // data extracted from opstatus etc
    int           adsb_version;   // ADS-B version (from ADS-B operational status); -1 means no ADS-B messages seen
    int           adsr_version;   // As above, for ADS-R messages
    int           tisb_version;   // As above, for TIS-B messages
    heading_type_t adsb_hrd;      // Heading Reference Direction setting (from ADS-B operational status)
    heading_type_t adsb_tah;      // Track Angle / Heading setting (from ADS-B operational status)

    data_validity nic_a_valid;
    data_validity nic_c_valid;
    data_validity nic_baro_valid;
    data_validity nac_p_valid;
    data_validity nac_v_valid;
    data_validity sil_valid;
    data_validity gva_valid;
    data_validity sda_valid;

    unsigned      nic_a : 1;      // NIC supplement A from opstatus
    unsigned      nic_c : 1;      // NIC supplement C from opstatus
    unsigned      nic_baro : 1;   // NIC baro supplement from TSS or opstatus
    unsigned      nac_p : 4;      // NACp from TSS or opstatus
    unsigned      nac_v : 3;      // NACv from airborne velocity or opstatus
    unsigned      sil : 2;        // SIL from TSS or opstatus
    sil_type_t    sil_type;       // SIL supplement from TSS or opstatus
    unsigned      gva : 2;        // GVA from opstatus
    unsigned      sda : 2;        // SDA from opstatus

    // data extracted from MRAR
    data_validity mrar_source_valid;
    data_validity wind_valid; // speed and direction
    data_validity pressure_valid;
    data_validity temperature_valid;
    data_validity turbulence_valid;
    data_validity humidity_valid;

    mrar_source_t mrar_source;
    float         wind_speed;
    float         wind_dir;
    float         pressure;
    float         temperature;
    hazard_t      turbulence;
    float         humidity;

    int           modeA_hit;   // did our squawk match a possible mode A reply in the last check period?
    int           modeC_hit;   // did our altitude match a possible mode C reply in the last check period?

    int           fatsv_emitted_altitude_baro;    // last FA emitted altitude
    int           fatsv_emitted_altitude_geom;    //      -"-         GNSS altitude
    int           fatsv_emitted_baro_rate;        //      -"-         barometric rate
    int           fatsv_emitted_geom_rate;        //      -"-         geometric rate
    float         fatsv_emitted_track;            //      -"-         true track
    float         fatsv_emitted_track_rate;       //      -"-         track rate of change
    float         fatsv_emitted_mag_heading;      //      -"-         magnetic heading
    float         fatsv_emitted_true_heading;     //      -"-         true heading
    float         fatsv_emitted_roll;             //      -"-         roll angle
    float         fatsv_emitted_gs;               //      -"-         groundspeed
    unsigned      fatsv_emitted_ias;              //      -"-         IAS
    unsigned      fatsv_emitted_tas;              //      -"-         TAS
    float         fatsv_emitted_mach;             //      -"-         Mach number
    airground_t   fatsv_emitted_airground;        //      -"-         air/ground state
    unsigned      fatsv_emitted_nav_altitude_mcp; //      -"-         MCP altitude
    unsigned      fatsv_emitted_nav_altitude_fms; //      -"-         FMS altitude
    unsigned      fatsv_emitted_nav_altitude_src; //      -"-         automation altitude source
    float         fatsv_emitted_nav_heading;      //      -"-         target heading
    nav_modes_t   fatsv_emitted_nav_modes;        //      -"-         enabled navigation modes
    float         fatsv_emitted_nav_qnh;          //      -"-         altimeter setting
    unsigned char fatsv_emitted_bds_10[7];        //      -"-         BDS 1,0 message
    unsigned char fatsv_emitted_bds_17[7];        //      -"-         BDS 1,7 message
    unsigned char fatsv_emitted_bds_30[7];        //      -"-         BDS 3,0 message
    unsigned char fatsv_emitted_unknown_commb[7]; //      -"-         unrecognized Comm-B message
    unsigned char fatsv_emitted_es_status[7];     //      -"-         ES operational status message
    unsigned char fatsv_emitted_es_acas_ra[7];    //      -"-         ES ACAS RA report message
    char          fatsv_emitted_callsign[9];      //      -"-         callsign
    addrtype_t    fatsv_emitted_addrtype;         //      -"-         address type (assumed ADSB_ICAO initially)
    int           fatsv_emitted_adsb_version;     //      -"-         ADS-B version (assumed non-ADS-B initially)
    unsigned      fatsv_emitted_category;         //      -"-         ADS-B emitter category (assumed A0 initially)
    unsigned      fatsv_emitted_squawk;           //      -"-         squawk
    unsigned      fatsv_emitted_nac_p;            //      -"-         NACp
    unsigned      fatsv_emitted_nac_v;            //      -"-         NACv
    unsigned      fatsv_emitted_sil;              //      -"-         SIL
    sil_type_t    fatsv_emitted_sil_type;         //      -"-         SIL supplement
    unsigned      fatsv_emitted_nic_baro;         //      -"-         NICbaro
    emergency_t   fatsv_emitted_emergency;        //      -"-         emergency/priority status

    uint64_t      fatsv_last_emitted;             // time (millis) aircraft was last FA emitted
    uint64_t      fatsv_last_force_emit;          // time (millis) we last emitted only-on-change data

    struct aircraft *next;        // Next aircraft in our linked list
};

/* Mode A/C tracking is done separately, not via the aircraft list,
 * and via a flat array rather than a list since there are only 4k possible values
 * (nb: we ignore the ident/SPI bit when tracking)
 */
extern uint32_t modeAC_count[4096];
extern uint32_t modeAC_match[4096];
extern uint32_t modeAC_age[4096];

/* is this bit of data valid? */
static inline int trackDataValid(const data_validity *v)
{
    return (v->source != SOURCE_INVALID && messageNow() < v->expires);
}

/* is this bit of data fresh? */
static inline int trackDataFresh(const data_validity *v)
{
    return (v->source != SOURCE_INVALID && messageNow() < v->stale);
}

/* what's the age of this data, in milliseconds? */
static inline uint64_t trackDataAge(const data_validity *v)
{
    if (v->source == SOURCE_INVALID)
        return ~(uint64_t)0;
    if (v->updated >= messageNow())
        return 0;
    return (messageNow() - v->updated);
}

/* Update aircraft state from data in the provided mesage.
 * Return the tracked aircraft.
 */
struct modesMessage;
struct aircraft *trackUpdateFromMessage(struct modesMessage *mm);

/* Call periodically */
void trackPeriodicUpdate();

/* Convert from a (hex) mode A value to a 0-4095 index */
static inline unsigned modeAToIndex(unsigned modeA)
{
    return (modeA & 0x0007) | ((modeA & 0x0070) >> 1) | ((modeA & 0x0700) >> 2) | ((modeA & 0x7000) >> 3);
}

/* Convert from a 0-4095 index to a (hex) mode A value */
static inline unsigned indexToModeA(unsigned index)
{
    return (index & 0007) | ((index & 0070) << 1) | ((index & 0700) << 2) | ((index & 07000) << 3);
}

/* Great Circle distance in m */
double greatcircle(double lat0, double lon0, double lat1, double lon1);

/* Get bearing from 2 points */
double get_bearing(double lat0, double lon0, double lat1, double lon1);


#endif
