// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// comm_b.c: Comm-B message decoding
//
// Copyright (c) 2017 FlightAware, LLC
// Copyright (c) 2017 Oliver Jowett <oliver@mutability.co.uk>
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
#include "ais_charset.h"

typedef int (*CommBDecoderFn)(struct modesMessage *,bool);

static int decodeEmptyResponse(struct modesMessage *mm, bool store);
static int decodeBDS10(struct modesMessage *mm, bool store);
static int decodeBDS17(struct modesMessage *mm, bool store);
static int decodeBDS20(struct modesMessage *mm, bool store);
static int decodeBDS30(struct modesMessage *mm, bool store);
static int decodeBDS40(struct modesMessage *mm, bool store);
static int decodeBDS44(struct modesMessage *mm, bool store);
static int decodeBDS50(struct modesMessage *mm, bool store);
static int decodeBDS60(struct modesMessage *mm, bool store);
static int decodeBDS05(struct modesMessage *mm, bool store);

static CommBDecoderFn comm_b_decoders[] = {
    &decodeEmptyResponse,
    &decodeBDS10,
    &decodeBDS20,
    &decodeBDS30,
    &decodeBDS17,
    &decodeBDS40,
    &decodeBDS50,
    &decodeBDS60,
    &decodeBDS44,
    &decodeBDS05
};

void decodeCommB(struct modesMessage *mm)
{
    // If DR or UM are set, this message is _probably_ noise
    // as nothing really seems to use the multisite broadcast stuff?
    // Also skip anything that had errors corrected
    if (mm->DR != 0 || mm->UM != 0 || mm->correctedbits > 0) {
        mm->commb_format = COMMB_NOT_DECODED;
        return;
    }

    // This is a bit hairy as we don't know what the requested register was
    int bestScore = 0;
    CommBDecoderFn bestDecoder = NULL;
    int ambiguous = 0;

    for (unsigned i = 0; i < (sizeof(comm_b_decoders) / sizeof(comm_b_decoders[0])); ++i) {
        int score = comm_b_decoders[i](mm, false);
        if (score > bestScore) {
            bestScore = score;
            bestDecoder = comm_b_decoders[i];
            ambiguous = 0;
        } else if (score == bestScore) {
            ambiguous = 1;
        }
    }

    if (bestDecoder) {
        if (ambiguous) {
            mm->commb_format = COMMB_AMBIGUOUS;
        } else {
            // decode it
            bestDecoder(mm, true);
        }
    } else {
        mm->commb_format = COMMB_UNKNOWN;
    }
}

static int decodeEmptyResponse(struct modesMessage *mm, bool store)
{
    // 00000000000000 is a common response. Ignore it.
    //
    // Also, it's common to see responses that look like this:
    //    40000000000000
    //    50000000000000
    //    60000000000000
    // typically in grouped bursts (one of each message) from
    // the same aircraft.
    //
    // I speculate that these are response to interrogations for
    // BDS 4,0 5,0 and 6,0 respectively where the transponder
    // doesn't support the register or has no data loaded for it.
    // Treat them like empty responses.

    switch (mm->MB[0]) {
    case 0x00:
    case 0x40:
    case 0x50:
    case 0x60:
        break;

    default:
        return 0;
    }

    for (unsigned i = 1; i < 7; ++i) {
        if (mm->MB[i] != 0) {
            return 0;
        }
    }

    if (store) {
        mm->commb_format = COMMB_EMPTY_RESPONSE;
    }

    return 56;
}

// BDS1,0 Datalink capabilities
static int decodeBDS10(struct modesMessage *mm, bool store)
{
    unsigned char *msg = mm->MB;

    // BDS identifier
    if (msg[0] != 0x10) {
        return 0;
    }

    // Reserved bits
    if (getbits(msg, 10, 14) != 0) {
        return 0;
    }

    // Looks plausible.

    if (store) {
        mm->commb_format = COMMB_DATALINK_CAPS;
    }

    return 56;
}

// BDS1,7 Common usage GICB capability report
static int decodeBDS17(struct modesMessage *mm, bool store)
{
    unsigned char *msg = mm->MB;

    // reserved bits
    if (getbits(msg, 25, 56) != 0) {
        return 0;
    }

    int score = 0;
    if (getbit(msg, 7)) {
        score += 1;  // 2,0 aircraft identification
    } else {
        // BDS2,0 is on almost everything
        score -= 2;
    }

    // unlikely bits
    if (getbit(msg, 10)) { // 4,1 next waypoint identifier
        score -= 2;
    }
    if (getbit(msg, 11)) { // 4,2 next waypoint position
        score -= 2;
    }
    if (getbit(msg, 12)) { // 4,3 next waypoint information
        score -= 2;
    }
    if (getbit(msg, 13)) { // 4,4 meterological routine report
        score -= 1;
    }
    if (getbit(msg, 14)) { // 4,4 meterological hazard report
        score -= 1;
    }
    if (getbit(msg, 20)) { // 5,4 waypoint 1
        score -= 2;
    }
    if (getbit(msg, 21)) { // 5,5 waypoint 2
        score -= 2;
    }
    if (getbit(msg, 22)) { // 5,6 waypoint 3
        score -= 2;
    }

    if (getbit(msg, 1) && getbit(msg, 2) && getbit(msg, 3) && getbit(msg, 4) && getbit(msg, 5)) {
        // looks like ES capable
        score += 5;
        if (getbit(msg, 6)) {
            // ES EDI
            score += 1;
        }
    } else if (!getbit(msg, 1) && !getbit(msg, 2) && !getbit(msg, 3) && !getbit(msg, 4) && !getbit(msg, 5) && !getbit(msg, 6)) {
        // not ES capable
        score += 1;
    } else if (!getbit(msg, 1) && !getbit(msg, 2) && getbit(msg, 3) && getbit(msg, 4) && getbit(msg, 5)) {
        // ES with no position data
        score += 3;
    } else {
        // other combinations, unlikely
        score -= 12;
    }

    if (getbit(msg, 16) && getbit(msg, 24)) {
        // track/turn, heading/speed
        score += 2;
        if (getbit(msg, 9)) {
            // vertical intent
            score += 1;
        }
    } else if (!getbit(msg, 16) && !getbit(msg, 24) && !getbit(msg, 9)) {
        // neither
        score += 1;
    } else {
        // unlikely
        score -= 6;
    }

    if (store) {
        mm->commb_format = COMMB_GICB_CAPS;
    }

    return score;
}

// BDS2,0 Aircraft identification
static int decodeBDS20(struct modesMessage *mm, bool store)
{
    char callsign[9];
    unsigned char *msg = mm->MB;

    // BDS identifier
    if (msg[0] != 0x20) {
        return 0;
    }

    callsign[0] = ais_charset[getbits(msg, 9, 14)];
    callsign[1] = ais_charset[getbits(msg, 15, 20)];
    callsign[2] = ais_charset[getbits(msg, 21, 26)];
    callsign[3] = ais_charset[getbits(msg, 27, 32)];
    callsign[4] = ais_charset[getbits(msg, 33, 38)];
    callsign[5] = ais_charset[getbits(msg, 39, 44)];
    callsign[6] = ais_charset[getbits(msg, 45, 50)];
    callsign[7] = ais_charset[getbits(msg, 51, 56)];
    callsign[8] = 0;

    // score based on number of valid characters
    int score = 8;
    int valid = 1;
    for (unsigned i = 0; i < 8; ++i) {
        if ((callsign[i] >= 'A' && callsign[i] <= 'Z') || (callsign[i] >= '0' && callsign[i] <= '9') || callsign[i] == ' ') {
            score += 6;
        } else if (callsign[i] == '@') {
            // Padding (sometimes we get @@@@@@@@, i.e. BDS2,0 with all zeros - we do want to accept this as a BDS2,0 but not actually use the callsign)
            valid = 0;
        } else {
            // Invalid
            return 0;
        }
    }

    if (store) {
        mm->commb_format = COMMB_AIRCRAFT_IDENT;
        if (valid) {
            memcpy(mm->callsign, callsign, sizeof(mm->callsign));
            mm->callsign_valid = 1;
        }
    }

    return score;
}

// BDS3,0 ACAS RA
static int decodeBDS30(struct modesMessage *mm, bool store)
{
    unsigned char *msg = mm->MB;

    // BDS identifier
    if (msg[0] != 0x30) {
        return 0;
    }

    if (store) {
        mm->commb_format = COMMB_ACAS_RA;
    }

    // just accept it.
    return 56;
}

// BDS4,0 Selected vertical intention
static int decodeBDS40(struct modesMessage *mm, bool store)
{
    unsigned char *msg = mm->MB;

    unsigned mcp_valid = getbit(msg, 1);
    unsigned mcp_raw = getbits(msg, 2, 13);
    unsigned fms_valid = getbit(msg, 14);
    unsigned fms_raw = getbits(msg, 15, 26);
    unsigned baro_valid = getbit(msg, 27);
    unsigned baro_raw = getbits(msg, 28, 39);
    unsigned reserved_1 = getbits(msg, 40, 47);
    unsigned mode_valid = getbit(msg, 48);
    unsigned mode_raw = getbits(msg, 49, 51);
    unsigned reserved_2 = getbits(msg, 52, 53);
    unsigned source_valid = getbit(msg, 54);
    unsigned source_raw = getbits(msg, 55, 56);

    if (!mcp_valid && !fms_valid && !baro_valid && !mode_valid && !source_valid) {
        return 0;
    }

    int score = 0;

    unsigned mcp_alt = 0;
    if (mcp_valid && mcp_raw != 0) {
        mcp_alt = mcp_raw * 16;
        if (mcp_alt >= 1000 && mcp_alt <= 50000) {
            score += 13;
        } else {
            // unlikely altitude
            return 0;
        }
    } else if (!mcp_valid && mcp_raw == 0) {
        score += 1;
    } else {
        return 0;
    }

    unsigned fms_alt = 0;
    if (fms_valid && fms_raw != 0) {
        fms_alt = fms_raw * 16;
        if (fms_alt >= 1000 && fms_alt <= 50000) {
            score += 13;
        } else {
            // unlikely altitude
            return 0;
        }
    } else if (!fms_valid && fms_raw == 0) {
        score += 1;
    } else {
        return 0;
    }

    float baro_setting = 0;
    if (baro_valid && baro_raw != 0) {
        baro_setting = 800 + baro_raw * 0.1;
        if (baro_setting >= 900 && baro_setting <= 1100) {
            score += 13;
        } else {
            // unlikely pressure setting
            return 0;
        }
    } else if (!baro_valid && baro_raw == 0) {
        score += 1;
    } else {
        return 0;
    }

    if (reserved_1 != 0) {
        return 0;
    }

    if (mode_valid) {
        score += 4;
    } else if (!mode_valid && mode_raw == 0) {
        score += 1;
    } else {
        return 0;
    }

    if (reserved_2 != 0) {
        return 0;
    }

    if (source_valid) {
        score += 3;
    } else if (!source_valid && source_raw == 0) {
        score += 1;
    } else {
        return 0;
    }

    // small penalty for inconsistent data
    if (mcp_valid && fms_valid && mcp_alt != fms_alt) {
        score -= 4;
    }

    if (mcp_valid) {
        unsigned remainder = mcp_alt % 500;
        if (!(remainder < 16 || remainder > 484)) {
            // mcp altitude is not a multiple of 500
            score -= 4;
        }
    }

    if (fms_valid) {
        unsigned remainder = fms_alt % 500;
        if (!(remainder < 16 || remainder > 484)) {
            // fms altitude is not a multiple of 500
            score -= 4;
        }
    }

    if (store) {
        mm->commb_format = COMMB_VERTICAL_INTENT;

        if (mcp_valid) {
            mm->nav.mcp_altitude_valid = 1;
            mm->nav.mcp_altitude = mcp_alt;
        }

        if (fms_valid) {
            mm->nav.fms_altitude_valid = 1;
            mm->nav.fms_altitude = fms_alt;
        }

        if (baro_valid) {
            mm->nav.qnh_valid = 1;
            mm->nav.qnh = baro_setting;
        }

        if (mode_valid) {
            mm->nav.modes_valid = 1;
            mm->nav.modes =
                ((mode_raw & 4) ? NAV_MODE_VNAV : 0) |
                ((mode_raw & 2) ? NAV_MODE_ALT_HOLD : 0) |
                ((mode_raw & 1) ? NAV_MODE_APPROACH : 0);
        }

        if (source_valid) {
            switch (source_raw) {
            case 0:
                mm->nav.altitude_source = NAV_ALT_UNKNOWN;
                break;
            case 1:
                mm->nav.altitude_source = NAV_ALT_AIRCRAFT;
                break;
            case 2:
                mm->nav.altitude_source = NAV_ALT_MCP;
                break;
            case 3:
                mm->nav.altitude_source = NAV_ALT_FMS;
                break;
            default:
                mm->nav.altitude_source = NAV_ALT_INVALID;
                break;
            }
        } else {
            mm->nav.altitude_source = NAV_ALT_INVALID;
        }
    }

    return score;
}

// BDS5,0 Track and turn report
static int decodeBDS50(struct modesMessage *mm, bool store)
{
    unsigned char *msg = mm->MB;

    unsigned roll_valid = getbit(msg, 1);
    unsigned roll_sign = getbit(msg, 2);
    unsigned roll_raw = getbits(msg, 3, 11);

    unsigned track_valid = getbit(msg, 12);
    unsigned track_sign = getbit(msg, 13);
    unsigned track_raw = getbits(msg, 14, 23);

    unsigned gs_valid = getbit(msg, 24);
    unsigned gs_raw = getbits(msg, 25, 34);

    unsigned track_rate_valid = getbit(msg, 35);
    unsigned track_rate_sign = getbit(msg, 36);
    unsigned track_rate_raw = getbits(msg, 37, 45);

    unsigned tas_valid = getbit(msg, 46);
    unsigned tas_raw = getbits(msg, 47, 56);

    if (!roll_valid || !track_valid || !gs_valid || !tas_valid) {
        return 0;
    }

    int score = 0;

    float roll = 0;
    if (roll_valid) {
        roll = roll_raw * 45.0 / 256.0;
        if (roll_sign) {
            roll -= 90.0;
        }

        if (roll >= -40 && roll < 40) {
            score += 11;
        } else {
            return 0;
        }
    } else if (!roll_valid && roll_raw == 0 && !roll_sign) {
        score += 1;
    } else {
        return 0;
    }

    float track = 0;
    if (track_valid) {
        score += 12;
        track = track_raw * 90.0 / 512.0;
        if (track_sign) {
            track += 180.0;
        }
    } else if (!track_valid && track_raw == 0 && !track_sign) {
        score += 1;
    } else {
        return 0;
    }

    unsigned gs = 0;
    if (gs_valid && gs_raw != 0) {
        gs = gs_raw * 2;

        if (gs >= 50 && gs <= 700) {
            score += 11;
        } else {
            return 0;
        }
    } else if (!gs_valid && gs_raw == 0) {
        score += 1;
    } else {
        return 0;
    }

    float track_rate = 0;
    if (track_rate_valid) {
        track_rate = track_rate_raw * 8.0 / 256.0;
        if (track_rate_sign) {
            track_rate -= 16;
        }

        if (track_rate >= -10.0 && track_rate <= 10.0) {
            score += 11;
        } else {
            return 0;
        }
    } else if (!track_rate_valid && track_rate_raw == 0 && !track_rate_sign) {
        score += 1;
    } else {
        return 0;
    }

    unsigned tas = 0;
    if (tas_valid && tas_raw != 0) {
        tas = tas_raw * 2;

        if (tas >= 50 && tas <= 700) {
            score += 11;
        } else {
            return 0;
        }
    } else if (!tas_valid && tas_raw == 0) {
        score += 1;
    } else {
        return 0;
    }

    // small penalty for inconsistent data
    if (gs_valid && tas_valid) {
        int delta = abs((int)gs_valid - (int)tas_valid);
        if (delta > 150) {
            score -= 6;
        }
    }

    // compute the theoretical turn rate and compare to track angle rate
    if (roll_valid && tas_valid && tas > 0 && track_rate_valid) {
        double turn_rate = 68625 * tan(roll * M_PI / 180.0) / (tas * 20 * M_PI);
        double delta = fabs(turn_rate - track_rate);
        if (delta > 2.0) {
            score -= 6;
        }
    }

    if (store) {
        mm->commb_format = COMMB_TRACK_TURN;

        if (roll_valid) {
            mm->roll_valid = 1;
            mm->roll = roll;
        }

        if (track_valid) {
            mm->heading_valid = 1;
            mm->heading = track;
            mm->heading_type = HEADING_GROUND_TRACK;
        }

        if (gs_valid) {
            mm->gs_valid = 1;
            mm->gs.v0 = mm->gs.v2 = mm->gs.selected = gs;
        }

        if (track_rate_valid) {
            mm->track_rate_valid = 1;
            mm->track_rate = track_rate;
        }

        if (tas_valid) {
            mm->tas_valid = 1;
            mm->tas = tas;
        }
    }

    return score;
}

// BDS6,0 Heading and speed report
static int decodeBDS60(struct modesMessage *mm, bool store)
{
    unsigned char *msg = mm->MB;

    unsigned heading_valid = getbit(msg, 1);
    unsigned heading_sign = getbit(msg, 2);
    unsigned heading_raw = getbits(msg, 3, 12);

    unsigned ias_valid = getbit(msg, 13);
    unsigned ias_raw = getbits(msg, 14, 23);

    unsigned mach_valid = getbit(msg, 24);
    unsigned mach_raw = getbits(msg, 25, 34);

    unsigned baro_rate_valid = getbit(msg, 35);
    unsigned baro_rate_sign = getbit(msg, 36);
    unsigned baro_rate_raw = getbits(msg, 37, 45);

    unsigned inertial_rate_valid = getbit(msg, 46);
    unsigned inertial_rate_sign = getbit(msg, 47);
    unsigned inertial_rate_raw = getbits(msg, 48, 56);

    if (!heading_valid || !ias_valid || !mach_valid || (!baro_rate_valid && !inertial_rate_valid)) {
        return 0;
    }

    int score = 0;

    float heading = 0;
    if (heading_valid) {
        heading = heading_raw * 90.0 / 512.0;
        if (heading_sign) {
            heading += 180.0;
        }
        score += 12;
    } else if (!heading_valid && heading_raw == 0 && !heading_sign) {
        score += 1;
    } else {
        return 0;
    }

    unsigned ias = 0;
    if (ias_valid && ias_raw != 0) {
        ias = ias_raw;
        if (ias >= 50 && ias <= 700) {
            score += 11;
        } else {
            return 0;
        }
    } else if (!ias_valid && ias_raw == 0) {
        score += 1;
    } else {
        return 0;
    }

    float mach = 0;
    if (mach_valid && mach_raw != 0) {
        mach = mach_raw * 2.048 / 512;
        if (mach >= 0.1 && mach <= 0.9) {
            score += 11;
        } else {
            return 0;
        }
    } else if (!mach_valid && mach_raw == 0) {
        score += 1;
    } else {
        return 0;
    }

    int baro_rate = 0;
    if (baro_rate_valid) {
        baro_rate = baro_rate_raw * 32;
        if (baro_rate_sign) {
            baro_rate -= 16384;
        }

        if (baro_rate >= -6000 && baro_rate <= 6000) {
            score += 11;
        } else {
            return 0;
        }
    } else if (!baro_rate_valid && baro_rate_raw == 0) {
        score += 1;
    } else {
        return 0;
    }

    int inertial_rate = 0;
    if (inertial_rate_valid) {
        inertial_rate = inertial_rate_raw * 32;
        if (inertial_rate_sign) {
            inertial_rate -= 16384;
        }

        if (inertial_rate >= -6000 && inertial_rate <= 6000) {
            score += 11;
        } else {
            return 0;
        }
    } else if (!inertial_rate_valid && inertial_rate_raw == 0) {
        score += 1;
    } else {
        return 0;
    }

    // small penalty for inconsistent data

    // Should check IAS vs Mach at given altitude, but the maths is a little involved

    if (baro_rate_valid && inertial_rate_valid) {
        int delta = abs(baro_rate - inertial_rate);
        if (delta > 2000) {
            score -= 12;
        }
    }

    if (store) {
        mm->commb_format = COMMB_HEADING_SPEED;

        if (heading_valid) {
            mm->heading_valid = 1;
            mm->heading = heading;
            mm->heading_type = HEADING_MAGNETIC;
        }

        if (ias_valid) {
            mm->ias_valid = 1;
            mm->ias = ias;
        }

        if (mach_valid) {
            mm->mach_valid = 1;
            mm->mach = mach;
        }

        if (baro_rate_valid) {
            mm->baro_rate_valid = 1;
            mm->baro_rate = baro_rate;
        }

        if (inertial_rate_valid) {
            // INS-derived data is treated as a "geometric rate" / "geometric altitude"
            // elsewhere, so do the same here.
            mm->geom_rate_valid = 1;
            mm->geom_rate = inertial_rate;
        }
    }

    return score;
}

// BDS4,4 Meterological routine air report
static int decodeBDS44(struct modesMessage *mm, bool store)
{
    unsigned char *msg = mm->MB;

    unsigned source = getbits(msg, 1, 4);

    unsigned wind_valid = getbit(msg, 5);
    unsigned windspeed_raw = getbits(msg, 6, 14);
    unsigned winddir_raw = getbits(msg, 15, 23);

    // ICAO 9871 is inconsistent, it claims:
    //  bit 24       sign
    //  bits 25..34  static air temperature, MSB = 64C, LSB=0.25C, range -128C..+128C
    //
    // .. but this does not actually work, there is one too many bits in the bitfield
    // for the claimed values.
    //
    // Based on observed data, the most plausible actual layout is:
    //
    //   bit 24       status
    //   bit 25       sign
    //   bits 26..34  static air temperature, MSB=64C, LSB=0.25C, range -128C..+128C

    unsigned sat_valid = getbit(msg, 24);
    unsigned sat_sign = getbit(msg, 25);
    unsigned sat_raw = getbits(msg, 26, 34);

    unsigned asp_valid = getbit(msg, 35);
    unsigned asp_raw = getbits(msg, 36, 46);

    unsigned turbulence_valid = getbit(msg, 47);
    unsigned turbulence_raw = getbits(msg, 48, 49);

    unsigned humidity_valid = getbit(msg, 50);
    unsigned humidity_raw = getbits(msg, 51, 56);

    if (source == MRAR_SOURCE_INVALID || source >= MRAR_SOURCE_RESERVED)
        return 0; // invalid or reserved source

    if (!wind_valid || !sat_valid)
        return 0; // all valid messages seen in the wild have at least temp + wind

    if (!asp_valid && asp_raw != 0)
        return 0; // ASP not valid, but non-zero values in the ASP field

    if (!turbulence_valid && turbulence_raw != 0)
        return 0; // turbulence not valid, but non-zero values in the turbulence field

    if (!humidity_valid && humidity_raw != 0)
        return 0; // humidity not valid, but non-zero values in the humidity field

    int score = 0;

    float wind_speed = 0;
    float wind_dir = 0;
    if (wind_valid) {
        wind_dir = winddir_raw * (180.0 / 256.0);
        wind_speed = windspeed_raw;

        if (windspeed_raw == 0) // possible but uncommon
            score += 2;
        else if (wind_speed <= 250)
            score += 19;
        else
            return 0;
    } else {
        score += 1;
    }

    float sat = 0;
    if (sat_valid) {
        sat = sat_raw * 0.25;
        if (sat_sign)
            sat -= 128;
        if (sat == 0) // possible but uncommon
            score += 2;
        else if (sat >= -80 && sat <= 60)
            score += 11;
        else
            return 0;
    } else {
        score += 1;
    }

    float asp = 0;
    if (asp_valid) {
        asp = asp_raw;
        if (asp >= 25 && asp <= 1100)
            score += 12;
        else
            return 0;
    } else {
        score += 1;
    }

    hazard_t turbulence = HAZARD_NIL;
    if (turbulence_valid) {
        turbulence = (hazard_t) turbulence_raw;
        score += 3;
    } else {
        score += 1;
    }

    float humidity = 0;
    if (humidity_valid) {
        humidity = humidity_raw * (100.0 / 64.0);
        score += 7;
    } else {
        score += 1;
    }

    if (source == MRAR_SOURCE_DMEDME && wind_valid && sat_valid && score > 0) {
        // Some GICB messages can be easily mistaken for a MRAR:
        //
        //   GICB bit 1: BDS 0,5 ES airborne position      = 0 ]
        //   GICB bit 2: BDS 0,6 ES surface position       = 0 ]
        //   GICB bit 3: BDS 0,7 ES status                 = 1 ]
        //   GICB bit 4: BDS 0,8 ES type & identification  = 1 ] -> MRAR source = 3
        //   GICB bit 5: BDS 0,9 ES airborne velocity      = 1   -> MRAR wind valid bit
        //   GICB bit 24: BDS 6,0 heading and speed report = 1   -> MRAR temp valid bit
        //   most trailing bits = 0
        //
        // so only treat this as MRAR as a last resort
        score = 1;
    }

    if (store) {
        mm->commb_format = COMMB_MRAR;
        mm->mrar_source_valid = 1;
        mm->mrar_source = (mrar_source_t) source;

        if (wind_valid) {
            mm->wind_valid = 1;
            mm->wind_speed = wind_speed;
            mm->wind_dir = wind_dir;
        }

        if (sat_valid) {
            mm->temperature_valid = 1;
            mm->temperature = sat;
        }

        if (asp_valid) {
            mm->pressure_valid = 1;
            mm->pressure = asp;
        }

        if (turbulence_valid) {
            mm->turbulence_valid = 1;
            mm->turbulence = turbulence;
        }

        if (humidity_valid) {
            mm->humidity_valid = 1;
            mm->humidity = humidity;
        }
    }

    return score;
}

// BDS0,5 extended squitter airborne position
// (apparently this gets queried via comm-b sometimes??)
// We don't try to _use_ this as a position, but we can
// at least try to recognize it, to exclude other
// comm-b types (in particular they can be mistaken for MRAR)
static int decodeBDS05(struct modesMessage *mm, bool store)
{
    // We recognize these by matching the position altitude against
    // the altitude in the surrounding message, so we need a
    // DF20 not a DF21
    if (mm->msgtype != 20)
        return 0;

    unsigned char *msg = mm->MB;

    unsigned typecode = getbits(msg, 1, 5);
    if (typecode < 9 || typecode > 18)
        return 0; // only consider typecodes that could be an airborne position with baro altitude

    unsigned t_bit = getbit(msg, 21);
    if (t_bit)  // unlikely
        return 0;

    unsigned ac12 = getbits(msg, 9, 20);
    if (!ac12)
        return 0;

    // Insert M=0 to make an AC13 value, match against the
    // AC13 value in the surrounding message
    unsigned ac13 = ((ac12 & 0x0FC0) << 1) | (ac12 & 0x003F);
    if (mm->AC != ac13)
        return 0; // no altitude match

    unsigned lat = getbits(msg, 23, 39);
    unsigned lon = getbits(msg, 40, 56);
    if (lat == 0 || lon == 0) // unlikely position
        return 0;

    if (store) {
        mm->commb_format = COMMB_AIRBORNE_POSITION;
        // No further decoding done, we don't really trust this
        // enough to use as real input to CPR
    }

    // Score this high enough to override everything else
    return 100;
}
