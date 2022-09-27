// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// mode_s.c: Mode S message decoding.
//
// Copyright (c) 2014-2016 Oliver Jowett <oliver@mutability.co.uk>
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
#include "ais_charset.h"

/* for PRIX64 */
#include <inttypes.h>

//
// ===================== Mode S detection and decoding  ===================
//
//
//

/* A timestamp that indicates the data is synthetic, created from a
 * multilateration result
 */
#define MAGIC_MLAT_TIMESTAMP 0xFF004D4C4154ULL

//=========================================================================
//
// Given the Downlink Format (DF) of the message, return the message length in bits.
//
// All known DF's 16 or greater are long. All known DF's 15 or less are short.
// There are lots of unused codes in both category, so we can assume ICAO will stick to
// these rules, meaning that the most significant bit of the DF indicates the length.
//
int modesMessageLenByType(int type) {
    return (type & 0x10) ? MODES_LONG_MSG_BITS : MODES_SHORT_MSG_BITS ;
}

//
//=========================================================================
//
// In the squawk (identity) field bits are interleaved as follows in
// (message bit 20 to bit 32):
//
// C1-A1-C2-A2-C4-A4-ZERO-B1-D1-B2-D2-B4-D4
//
// So every group of three bits A, B, C, D represent an integer from 0 to 7.
//
// The actual meaning is just 4 octal numbers, but we convert it into a hex
// number tha happens to represent the four octal numbers.
//
// For more info: http://en.wikipedia.org/wiki/Gillham_code
//
static int decodeID13Field(int ID13Field) {
    int hexGillham = 0;

    if (ID13Field & 0x1000) {hexGillham |= 0x0010;} // Bit 12 = C1
    if (ID13Field & 0x0800) {hexGillham |= 0x1000;} // Bit 11 = A1
    if (ID13Field & 0x0400) {hexGillham |= 0x0020;} // Bit 10 = C2
    if (ID13Field & 0x0200) {hexGillham |= 0x2000;} // Bit  9 = A2
    if (ID13Field & 0x0100) {hexGillham |= 0x0040;} // Bit  8 = C4
    if (ID13Field & 0x0080) {hexGillham |= 0x4000;} // Bit  7 = A4
  //if (ID13Field & 0x0040) {hexGillham |= 0x0800;} // Bit  6 = X  or M
    if (ID13Field & 0x0020) {hexGillham |= 0x0100;} // Bit  5 = B1
    if (ID13Field & 0x0010) {hexGillham |= 0x0001;} // Bit  4 = D1 or Q
    if (ID13Field & 0x0008) {hexGillham |= 0x0200;} // Bit  3 = B2
    if (ID13Field & 0x0004) {hexGillham |= 0x0002;} // Bit  2 = D2
    if (ID13Field & 0x0002) {hexGillham |= 0x0400;} // Bit  1 = B4
    if (ID13Field & 0x0001) {hexGillham |= 0x0004;} // Bit  0 = D4

    return (hexGillham);
}

//
//=========================================================================
//
// Decode the 13 bit AC altitude field (in DF 20 and others).
// Returns the altitude, and set 'unit' to either UNIT_METERS or UNIT_FEET.
//
static int decodeAC13Field(int AC13Field, altitude_unit_t *unit) {
    int m_bit  = AC13Field & 0x0040; // set = meters, clear = feet
    int q_bit  = AC13Field & 0x0010; // set = 25 ft encoding, clear = Gillham Mode C encoding

    if (!m_bit) {
        *unit = UNIT_FEET;
        if (q_bit) {
            // N is the 11 bit integer resulting from the removal of bit Q and M
            int n = ((AC13Field & 0x1F80) >> 2) |
                    ((AC13Field & 0x0020) >> 1) |
                     (AC13Field & 0x000F);
            // The final altitude is resulting number multiplied by 25, minus 1000.
            return ((n * 25) - 1000);
        } else {
            // N is an 11 bit Gillham coded altitude
            int n = modeAToModeC(decodeID13Field(AC13Field));
            if (n < -12) {
                return INVALID_ALTITUDE;
            }

            return (100 * n);
        }
    } else {
        *unit = UNIT_METERS;
        // TODO: Implement altitude when meter unit is selected
        return INVALID_ALTITUDE;
    }
}

//
//=========================================================================
//
// Decode the 12 bit AC altitude field (in DF 17 and others).
//
static int decodeAC12Field(int AC12Field, altitude_unit_t *unit) {
    int q_bit  = AC12Field & 0x10; // Bit 48 = Q

    *unit = UNIT_FEET;
    if (q_bit) {
        /// N is the 11 bit integer resulting from the removal of bit Q at bit 4
        int n = ((AC12Field & 0x0FE0) >> 1) |
                 (AC12Field & 0x000F);
        // The final altitude is the resulting number multiplied by 25, minus 1000.
        return ((n * 25) - 1000);
    } else {
        // Make N a 13 bit Gillham coded altitude by inserting M=0 at bit 6
        int n = ((AC12Field & 0x0FC0) << 1) |
                 (AC12Field & 0x003F);
        n = modeAToModeC(decodeID13Field(n));
        if (n < -12) {
            return INVALID_ALTITUDE;
        }

        return (100 * n);
    }
}

//
//=========================================================================
//
// Decode the 7 bit ground movement field PWL exponential style scale (ADS-B v2)
//
static float decodeMovementFieldV2(unsigned movement) {
    // Note : movement codes 0,125,126,127 are all invalid, but they are
    //        trapped for before this function is called.

    // Each movement value is a range of speeds;
    // we return the midpoint of the range (rounded to the nearest integer)
    if      (movement  >= 125) return 0;                                        // invalid
    else if (movement  == 124) return 180;                                      // gs > 175kt, pick a value..
    else if (movement  >= 109) return 100 + (movement - 109 + 0.5) * 5;         // 100 < gs <= 175 in 5kt steps
    else if (movement  >=  94) return 70 + (movement - 94 + 0.5) * 2;           // 70 < gs <= 100 in 2kt steps
    else if (movement  >=  39) return 15 + (movement - 39 + 0.5) * 1;           // 15 < gs <= 70 in 1kt steps
    else if (movement  >=  13) return 2 + (movement - 13 + 0.5) * 0.50;         // 2 < gs <= 15 in 0.5kt steps
    else if (movement  >=   9) return 1 + (movement - 9 + 0.5) * 0.25;          // 1 < gs <= 2 in 0.25kt steps
    else if (movement  >=   3) return 0.125 + (movement - 3 + 0.5) * 0.875 / 6; // 0.125 < gs <= 1 in 0.875/6 kt step
    else if (movement  >=   2) return 0.125 / 2;                                // 0 < gs <= 0.125
    // 1: stopped, gs = 0
    // 0: no data
    else                       return 0;
}

//
//=========================================================================
//
// Decode the 7 bit ground movement field PWL exponential style scale (ADS-B v0)
//
static float decodeMovementFieldV0(unsigned movement) {
    // Note : movement codes 0,125,126,127 are all invalid, but they are
    //        trapped for before this function is called.

    // Each movement value is a range of speeds;
    // we return the midpoint of the range
    if      (movement  >= 125) return 0;                                    // invalid
    else if (movement  == 124) return 180;                                  // gs >= 175kt, pick a value..
    else if (movement  >= 109) return 100 + (movement - 109 + 0.5) * 5;     // 100 < gs <= 175 in 5kt steps
    else if (movement  >=  94) return 70 + (movement - 94 + 0.5) * 2;       // 70 < gs <= 100 in 2kt steps
    else if (movement  >=  39) return 15 + (movement - 39 + 0.5) * 1;       // 15 < gs <= 70 in 1kt steps
    else if (movement  >=  13) return 2 + (movement - 13 + 0.5) * 0.50;     // 2 < gs <= 15 in 0.5kt steps
    else if (movement  >=   9) return 1 + (movement - 9 + 0.5) * 0.25;      // 1 < gs <= 2 in 0.25kt steps
    else if (movement  >=   2) return 0.125 + (movement - 2 + 0.5) * 0.125; // 0.125 < gs <= 1 in 0.125kt step
    // 1: stopped, gs < 0.125kt
    // 0: no data
    else                       return 0;
}

// Apply possible corrections to the 14-byte message in "in", storing the result in "out"
//
// If the message has a correct CRC, copies in to out unchanged and returns 0
// If the message has correctable errors, applies the corrections to out and returns the number of corrected errors
// If the message is uncorrectable (this may mean the message type does not have CRC coverage), returns -1

// is this message a long-form message with a DF that uses Parity/Interrogator?
static bool isLongPIMessage(const unsigned char *msg)
{
    const unsigned df = getbits(msg, 1, 5);
    if (df == 17 || df == 18)
        return true;
    return false;
}

// is this message a short-form message with a DF that uses Parity/Interrogator?
static bool isShortPIMessage(const unsigned char *msg)
{
    const unsigned df = getbits(msg, 1, 5);
    return (df == 11); // assume IID==0
}

#define UNCHECKED_SYNDROME 0xFFFFFFFFU

static int correctMessage(const unsigned char *in, unsigned char *out, uint32_t *short_syndrome, uint32_t *long_syndrome)
{
    // Possible DF values of the first byte of a message that could be a valid DF11/17/18
    // message after correction. See tools/df-correction-arrays.py for generator code.
    // This is used to shortcut message correction so that we don't bother computing a CRC over
    // messages that couldn't possibly become one of those message types.

    // These are bitsets, where the bit with value 1<<N represents a match for DF N
    static const uint32_t df_correctable_short[MODES_MAX_BITERRORS + 1] = {
        0x00000800, 0x08008e08, 0x08008e08
    };
    static const uint32_t df_correctable_long[MODES_MAX_BITERRORS + 1] = {
        0x00060000, 0x066f0006, 0x6fff066f
    };

    *short_syndrome = UNCHECKED_SYNDROME;
    *long_syndrome = UNCHECKED_SYNDROME;

    // Try to correct, including corrections to the initial 5 bit DF field
    // that determines message format

    const unsigned uncorrected_df = getbits(in, 1, 5);
    const uint32_t df_bit = 1 << uncorrected_df;

    // Select the right bitset based on the maximum number of bit errors in the DF field that we could correct.
    // nb: strictly speaking, --no-fix-df doesn't _entirely_ disable correction of the DF field when nfix_crc == 2
    // (DF17 could be corrected to DF18 or vice versa), but it does disable the CPU hungry part of it.
    const unsigned fix_df_bits = (Modes.fix_df ? Modes.nfix_crc : 0);

    struct errorinfo *long_ei = NULL;
    if (df_correctable_long[fix_df_bits] & df_bit) {
        *long_syndrome = modesChecksum(in, MODES_LONG_MSG_BITS);
        if (isLongPIMessage(in) && *long_syndrome == 0) {
            // DF17/18 message with correct checksum
            memcpy(out, in, MODES_LONG_MSG_BYTES);
            return 0;
        }

        long_ei = modesChecksumDiagnose(*long_syndrome, MODES_LONG_MSG_BITS);
    }

    struct errorinfo *short_ei = NULL;
    if (df_correctable_short[fix_df_bits] & df_bit) {
        *short_syndrome = modesChecksum(in, MODES_SHORT_MSG_BITS);
        if (isShortPIMessage(in) && (*short_syndrome & 0xFFFF80) == 0) {
            // DF11 message with correct checksum
            // (low 7 bits may be IID)
            memcpy(out, in, MODES_SHORT_MSG_BYTES);
            return 0;
        }

        short_ei = modesChecksumDiagnose(*short_syndrome, MODES_SHORT_MSG_BITS); // assume IID == 0
    }

    // Might be a damaged DF11/17/18, or might be another message type that doesn't have a full CRC

    unsigned long_errors = (long_ei ? long_ei->errors : 999);
    unsigned short_errors = (short_ei ? short_ei->errors : 999);

    // If both 56-bit and 112-bit corrections are possible:
    //   try the correction with fewer error bits first
    //   if there's a tie, try the 112-bit version first

    if (long_ei && long_errors <= short_errors) {
        memcpy(out, in, MODES_LONG_MSG_BYTES);
        modesChecksumFix(out, long_ei);
        if (isLongPIMessage(out)) {
            // valid DF17/18 message after corrections
            return long_errors;
        }
    }

    // Don't try to correct >1 error in DF11 (see crc.c)
    if (short_ei && short_errors == 1) {
        memcpy(out, in, MODES_SHORT_MSG_BYTES);
        modesChecksumFix(out, short_ei);
        if (isShortPIMessage(out)) {
            // valid DF11 message after corrections
            return short_errors;
        }
    }

    if (long_ei && long_errors > short_errors) {
        memcpy(out, in, MODES_LONG_MSG_BYTES);
        modesChecksumFix(out, long_ei);
        if (isLongPIMessage(out)) {
            // valid DF17/18 message after corrections
            return long_errors;
        }
    }

    // Nothing more to try, we can't correct this one further
    memcpy(out, in, MODES_LONG_MSG_BYTES);
    return -1;
}

// Score how plausible this ModeS message looks.
// The more positive, the more reliable the message is.
score_rank scoreModesMessage(const unsigned char *uncorrected)
{
    // This is a "valid" DF0 message, but it's not useful; we discard these messages
    static const unsigned char all_zeros[MODES_SHORT_MSG_BYTES] = { 0, 0, 0, 0, 0, 0, 0 };
    if (!memcmp(all_zeros, uncorrected, sizeof(all_zeros)))
        return SR_ALL_ZEROS;

    // try to produce a corrected DF11/17/18, including correcting the DF bits
    unsigned char corrected[14];
    uint32_t short_syndrome, long_syndrome;
    int corrections = correctMessage(uncorrected, corrected, &short_syndrome, &long_syndrome);

    unsigned df = getbits(corrected, 1, 5); // Downlink Format
    switch (df) {
    case 0:  // short air-air surveillance
    case 4:  // surveillance, altitude reply
    case 5:  // surveillance, altitude reply
        {
            if (short_syndrome == UNCHECKED_SYNDROME)
                short_syndrome = modesChecksum(corrected, MODES_SHORT_MSG_BITS);
            bool recent = icaoFilterTest(short_syndrome);
            return recent ? SR_UNRELIABLE_KNOWN : SR_UNRELIABLE_UNKNOWN;
        }

    case 16: // long air-air surveillance
    case 20: // Comm-B, altitude reply
    case 21: // Comm-B, identity reply
        {
            if (long_syndrome == UNCHECKED_SYNDROME)
                long_syndrome = modesChecksum(corrected, MODES_LONG_MSG_BITS);
            bool recent = icaoFilterTest(long_syndrome);
            return recent ? SR_UNRELIABLE_KNOWN : SR_UNRELIABLE_UNKNOWN;
        }

    case 24: // Comm-D (ELM)
    case 25: // Comm-D (ELM)
    case 26: // Comm-D (ELM)
    case 27: // Comm-D (ELM)
    case 28: // Comm-D (ELM)
    case 29: // Comm-D (ELM)
    case 30: // Comm-D (ELM)
    case 31: // Comm-D (ELM)
        {
            if (!Modes.enable_df24)
                return SR_UNCORRECTABLE;
            if (long_syndrome == UNCHECKED_SYNDROME)
                long_syndrome = modesChecksum(corrected, MODES_LONG_MSG_BITS);
            bool recent = icaoFilterTest(long_syndrome);
            return recent ? SR_UNRELIABLE_KNOWN : SR_UNRELIABLE_UNKNOWN;
        }

    case 11:
        {
            // DF11 All-call reply
            uint32_t addr = getbits(corrected, 9, 32);
            if (short_syndrome == UNCHECKED_SYNDROME)
                short_syndrome = modesChecksum(corrected, MODES_SHORT_MSG_BITS);
            uint32_t iid = short_syndrome & 0x7F;
            bool recent = icaoFilterTest(addr);

            switch (corrections) {
            case 0:
                if (iid == 0)
                    return recent ? SR_DF11_ACQ_KNOWN : SR_DF11_ACQ_UNKNOWN;
                else
                    return recent ? SR_DF11_IID_KNOWN : SR_DF11_IID_UNKNOWN;
            case 1:
                if (iid == 0)
                    return recent ? SR_DF11_ACQ_1ERROR_KNOWN : SR_DF11_ACQ_1ERROR_UNKNOWN;
                else
                    return recent ? SR_DF11_IID_1ERROR_KNOWN : SR_DF11_IID_1ERROR_UNKNOWN;
            default:
                return SR_UNCORRECTABLE;
            }
        }

    case 17:   // Extended squitter
        {
            uint32_t addr = getbits(corrected, 9, 32);
            bool recent = icaoFilterTest(addr);

            switch (corrections) {
            case 0:
                return recent ? SR_DF17_KNOWN : SR_DF17_UNKNOWN;
            case 1:
                return recent ? SR_DF17_1ERROR_KNOWN : SR_DF17_1ERROR_UNKNOWN;
            case 2:
                return recent ? SR_DF17_2ERROR_KNOWN : SR_DF17_2ERROR_UNKNOWN;
            default:
                return SR_UNCORRECTABLE;
            }
        }

    case 18:   // Extended squitter/non-transponder
        {
            uint32_t addr = getbits(corrected, 9, 32);
            bool recent = icaoFilterTest(addr | ICAO_FILTER_ADSB_NT); // only look for previous DF18 activity

            switch (corrections) {
            case 0:
                return recent ? SR_DF18_KNOWN : SR_DF18_UNKNOWN;
            case 1:
                return recent ? SR_DF18_1ERROR_KNOWN : SR_DF18_1ERROR_UNKNOWN;
            case 2:
                return recent ? SR_DF18_2ERROR_KNOWN : SR_DF18_2ERROR_UNKNOWN;
            default:
                return SR_UNCORRECTABLE;
            }
        }



    default:
        // unknown message type
        return SR_UNKNOWN_DF;
    }
}

static const char *score_to_string(score_rank score)
{
    switch (score) {
    case SR_NOT_SET: return "NOT_SET";
    case SR_UNKNOWN_THRESHOLD: return "UNKNOWN_THRESHOLD";
    case SR_ACCEPT_THRESHOLD: return "ACCEPT_THRESHOLD";

    case SR_ALL_ZEROS: return "ALL_ZEROS";
    case SR_UNKNOWN_DF: return "UNKNOWN_DF";
    case SR_UNCORRECTABLE: return "UNCORRECTABLE";

    case SR_UNRELIABLE_UNKNOWN: return "UNRELIABLE_UNKNOWN";
    case SR_UNRELIABLE_KNOWN: return "UNRELIABLE_KNOWN";

    case SR_DF11_IID_1ERROR_UNKNOWN: return "DF11_IID_1ERROR_UNKNOWN";
    case SR_DF11_ACQ_1ERROR_UNKNOWN: return "DF11_ACQ_1ERROR_UNKNOWN";
    case SR_DF11_IID_UNKNOWN: return "DF11_IID_UNKNOWN";
    case SR_DF11_ACQ_UNKNOWN: return "DF11_ACQ_UNKNOWN";
    case SR_DF11_IID_1ERROR_KNOWN: return "DF11_IID_1ERROR_KNOWN";
    case SR_DF11_ACQ_1ERROR_KNOWN: return "DF11_ACQ_1ERROR_KNOWN";
    case SR_DF11_IID_KNOWN: return "DF11_IID_KNOWN";
    case SR_DF11_ACQ_KNOWN: return "DF11_ACQ_KNOWN";

    case SR_DF17_2ERROR_UNKNOWN: return "DF17_2ERROR_UNKNOWN";
    case SR_DF17_2ERROR_KNOWN: return "DF17_2ERROR_KNOWN";
    case SR_DF17_1ERROR_UNKNOWN: return "DF17_1ERROR_UNKNOWN";
    case SR_DF17_1ERROR_KNOWN: return "DF17_1ERROR_KNOWN";
    case SR_DF17_UNKNOWN: return "DF17_UNKNOWN";
    case SR_DF17_KNOWN: return "DF17_KNOWN";

    case SR_DF18_2ERROR_UNKNOWN: return "DF18_2ERROR_UNKNOWN";
    case SR_DF18_2ERROR_KNOWN: return "DF18_2ERROR_KNOWN";
    case SR_DF18_1ERROR_UNKNOWN: return "DF18_1ERROR_UNKNOWN";
    case SR_DF18_1ERROR_KNOWN: return "DF18_1ERROR_KNOWN";
    case SR_DF18_UNKNOWN: return "DF18_UNKNOWN";
    case SR_DF18_KNOWN: return "DF18_KNOWN";
    }

    return "<bad value>";
}

static void decodeExtendedSquitter(struct modesMessage *mm);

//
//=========================================================================
//
// Decode a raw Mode S message demodulated as a stream of bytes by detectModeS(),
// and split it into fields populating a modesMessage structure.
//
// return 0 if all OK
// <0 if it's a bad message
//
int decodeModesMessage(struct modesMessage *mm, const unsigned char *in)
{
    // score the message if needed (it might be coming off the network)
    if (mm->score == SR_NOT_SET)
        mm->score = scoreModesMessage(in);

    if (mm->score < SR_UNKNOWN_THRESHOLD)
        return -1;
    if (mm->score < SR_ACCEPT_THRESHOLD)
        return -2;

    // Preserve the original uncorrected copy for later forwarding
    memcpy(mm->verbatim, in, MODES_LONG_MSG_BYTES);

    // Apply corrections to our local copy
    uint32_t short_syndrome, long_syndrome;
    int corrections = correctMessage(in, mm->msg, &short_syndrome, &long_syndrome);
    const unsigned char *msg = mm->msg;

    // Get the message type ASAP as other operations depend on this
    mm->msgtype         = getbits(msg, 1, 5); // Downlink Format
    mm->msgbits         = modesMessageLenByType(mm->msgtype);
    if (mm->msgtype & 16) {
        if (long_syndrome == UNCHECKED_SYNDROME)
            long_syndrome = modesChecksum(mm->msg, MODES_LONG_MSG_BITS);
        mm->crc = long_syndrome;
    } else {
        if (short_syndrome == UNCHECKED_SYNDROME)
            short_syndrome = modesChecksum(mm->msg, MODES_SHORT_MSG_BITS);
        mm->crc = short_syndrome;
    }

    mm->correctedbits   = corrections > 0 ? corrections : 0;
    mm->addr            = 0;

    // Do checksum work and set fields that depend on the CRC
    switch (mm->msgtype) {
    case 0: // short air-air surveillance
    case 4: // surveillance, altitude reply
    case 5: // surveillance, identity reply
    case 16: // long air-air surveillance
        // These message types use Address/Parity
        // so we can't check the CRC and must infer the transmitter's address
        mm->source = SOURCE_MODE_S;
        mm->addr = mm->crc;
        mm->reliable = 0;
        break;

    case 11: // All-call reply
        // This message type uses Parity/Interrogator, i.e. our CRC syndrome is CL + IC from the uplink message
        // which we can't see. So we don't know if the CRC is correct or not.
        //
        // however! CL + IC only occupy the lower 7 bits of the CRC. So if we ignore those bits when testing
        // the CRC we can still try to detect/correct errors.
        mm->IID = mm->crc & 0x7f;
        mm->source = SOURCE_MODE_S_CHECKED;
        mm->reliable = (mm->IID == 0 && mm->correctedbits == 0);
        break;

    case 17:   // Extended squitter
    case 18: { // Extended squitter/non-transponder
        // These message types use Parity/Interrogator, but are specified to set II=0
        mm->source = SOURCE_ADSB; // TIS-B decoding will override this if needed
        mm->reliable = (mm->correctedbits == 0);
        break;
    }

    case 20: // Comm-B, altitude reply
    case 21: // Comm-B, identity reply
        // These message types either use Address/Parity
        // or Data Parity where the requested BDS is also xored into the top byte.
        // So not only do we not know whether the CRC is right, we also don't know if
        // the ICAO is right! Ow.

        mm->source = SOURCE_MODE_S;
        mm->addr = mm->crc;
        mm->reliable = 0;
        break;

    case 24: // Comm-D (ELM)
    case 25: // Comm-D (ELM)
    case 26: // Comm-D (ELM)
    case 27: // Comm-D (ELM)
    case 28: // Comm-D (ELM)
    case 29: // Comm-D (ELM)
    case 30: // Comm-D (ELM)
    case 31: // Comm-D (ELM)
        // These messages use Address/Parity,
        // and also use some of the DF bits to carry data. Remap them all to a single
        // DF for simplicity.
        mm->msgtype = 24;
        mm->source = SOURCE_MODE_S;
        mm->addr = mm->crc;
        mm->reliable = 0;
        break;

    default:
        // All other message types, we don't know how to handle their CRCs, give up
        return -2;
    }

    // decode the bulk of the message

    // AA (Address announced)
    if (mm->msgtype == 11 || mm->msgtype == 17 || mm->msgtype == 18) {
        mm->AA = mm->addr = getbits(msg, 9, 32);
    }

    // AC (Altitude Code)
    if (mm->msgtype == 0 || mm->msgtype == 4 || mm->msgtype == 16 || mm->msgtype == 20) {
        mm->AC = getbits(msg, 20, 32);
        if (mm->AC) { // Only attempt to decode if a valid (non zero) altitude is present
            mm->altitude_baro = decodeAC13Field(mm->AC, &mm->altitude_baro_unit);
            if (mm->altitude_baro != INVALID_ALTITUDE)
                mm->altitude_baro_valid = 1;
        }
    }

    // AF (DF19 Application Field) not decoded

    // CA (Capability)
    if (mm->msgtype == 11 || mm->msgtype == 17) {
        mm->CA = getbits(msg, 6, 8);

        switch (mm->CA) {
        case 0:
            mm->airground = AG_UNCERTAIN;
            break;
        case 4:
            mm->airground = AG_GROUND;
            break;
        case 5:
            mm->airground = AG_AIRBORNE;
            break;
        case 6:
            mm->airground = AG_UNCERTAIN;
            break;
        case 7:
            mm->airground = AG_UNCERTAIN;
            break;
        }
    }

    // CC (Cross-link capability)
    if (mm->msgtype == 0) {
        mm->CC = getbit(msg, 7);
    }

    // CF (Control field, see Figure 2-2 ADS-B Message BaselineFormat Structure)
    if (mm->msgtype == 18) {
        mm->CF = getbits(msg, 6, 8);
    }

    // DR (Downlink Request)
    if (mm->msgtype == 4 || mm->msgtype == 5 || mm->msgtype == 20 || mm->msgtype == 21) {
        mm->DR = getbits(msg, 9, 13);
    }

    // FS (Flight Status)
    if (mm->msgtype == 4 || mm->msgtype == 5 || mm->msgtype == 20 || mm->msgtype == 21) {
        mm->FS = getbits(msg, 6, 8);
        mm->alert_valid = 1;
        mm->spi_valid = 1;

        switch (mm->FS) {
        case 0:
            mm->airground = AG_UNCERTAIN;
            break;
        case 1:
            mm->airground = AG_GROUND;
            break;
        case 2:
            mm->airground = AG_UNCERTAIN;
            mm->alert = 1;
            break;
        case 3:
            mm->airground = AG_GROUND;
            mm->alert = 1;
            break;
        case 4:
            mm->airground = AG_UNCERTAIN;
            mm->alert = 1;
            mm->spi = 1;
            break;
        case 5:
            mm->airground = AG_UNCERTAIN;
            mm->spi = 1;
            break;
        default:
            mm->spi_valid = 0;
            mm->alert_valid = 0;
            break;
        }
    }

    // ID (Identity)
    if (mm->msgtype == 5  || mm->msgtype == 21) {
        // Gillham encoded Squawk
        mm->ID = getbits(msg, 20, 32);
        if (mm->ID) {
            mm->squawk = decodeID13Field(mm->ID);
            mm->squawk_valid = 1;
        }
    }

    // KE (Control, ELM)
    if (mm->msgtype == 24) {
        mm->KE = getbit(msg, 4);
    }

    // MB (messsage, Comm-B)
    if (mm->msgtype == 20 || mm->msgtype == 21) {
        memcpy(mm->MB, &msg[4], 7);
        decodeCommB(mm);
    }

    // MD (message, Comm-D)
    if (mm->msgtype == 24) {
        memcpy(mm->MD, &msg[1], 10);
    }

    // ME (message, extended squitter)
    if (mm->msgtype == 17 || mm->msgtype == 18) {
        memcpy(mm->ME, &msg[4], 7);
        decodeExtendedSquitter(mm);
    }

    // MV (message, ACAS)
    if (mm->msgtype == 16) {
        memcpy(mm->MV, &msg[4], 7);
    }

    // ND (number of D-segment, Comm-D)
    if (mm->msgtype == 24) {
        mm->ND = getbits(msg, 5, 8);
    }

    // RI (Reply information, ACAS)
    if (mm->msgtype == 0 || mm->msgtype == 16) {
        mm->RI = getbits(msg, 14, 17);
    }

    // SL (Sensitivity level, ACAS)
    if (mm->msgtype == 0 || mm->msgtype == 16) {
        mm->SL = getbits(msg, 9, 11);
    }

    // UM (Utility Message)
    if (mm->msgtype == 4 || mm->msgtype == 5 || mm->msgtype == 20 || mm->msgtype == 21) {
        mm->UM = getbits(msg, 14, 19);
    }

    // VS (Vertical Status)
    if (mm->msgtype == 0 || mm->msgtype == 16) {
        mm->VS = getbit(msg, 6);
        if (mm->VS)
            mm->airground = AG_GROUND;
        else
            mm->airground = AG_UNCERTAIN;
    }

    if (!mm->correctedbits && (mm->msgtype == 17 || (mm->msgtype == 11 && mm->IID == 0))) {
        // DF17 ADS-B or DF11 acquisition squitter. Mark as known Mode-S source
        icaoFilterAdd(mm->addr);
    }
    if (!mm->correctedbits && mm->msgtype == 18) {
         // Mark as known ADS-B (NT) source
       icaoFilterAdd(mm->addr | ICAO_FILTER_ADSB_NT);
    }

    // MLAT overrides all other sources
    if (mm->remote && mm->timestampMsg == MAGIC_MLAT_TIMESTAMP)
        mm->source = SOURCE_MLAT;

    // all done
    return 0;
}

static void decodeESIdentAndCategory(struct modesMessage *mm)
{
    // Aircraft Identification and Category
    unsigned char *me = mm->ME;

    mm->mesub = getbits(me, 6, 8);

    mm->callsign[0] = ais_charset[getbits(me, 9, 14)];
    mm->callsign[1] = ais_charset[getbits(me, 15, 20)];
    mm->callsign[2] = ais_charset[getbits(me, 21, 26)];
    mm->callsign[3] = ais_charset[getbits(me, 27, 32)];
    mm->callsign[4] = ais_charset[getbits(me, 33, 38)];
    mm->callsign[5] = ais_charset[getbits(me, 39, 44)];
    mm->callsign[6] = ais_charset[getbits(me, 45, 50)];
    mm->callsign[7] = ais_charset[getbits(me, 51, 56)];
    mm->callsign[8] = 0;
    mm->callsign_valid = 1;

    // actually valid?
    for (unsigned i = 0; i < 8; ++i) {
        if (!(mm->callsign[i] >= 'A' && mm->callsign[i] <= 'Z') &&
            !(mm->callsign[i] >= '0' && mm->callsign[i] <= '9') &&
            mm->callsign[i] != ' ') {
            // Bad callsign, ignore it
            mm->callsign_valid = 0;
            break;
        }
    }

    mm->category = ((0x0E - mm->metype) << 4) | mm->mesub;
    mm->category_valid = 1;
}

// Handle setting a non-ICAO address
static void setIMF(struct modesMessage *mm)
{
    mm->addr |= MODES_NON_ICAO_ADDRESS;
    switch (mm->addrtype) {
    case ADDR_ADSB_ICAO:
    case ADDR_ADSB_ICAO_NT:
        // Shouldn't happen, but let's try to handle it
        mm->addrtype = ADDR_ADSB_OTHER;
        break;

    case ADDR_TISB_ICAO:
        mm->addrtype = ADDR_TISB_TRACKFILE;
        break;

    case ADDR_ADSR_ICAO:
        mm->addrtype = ADDR_ADSR_OTHER;
        break;

    default:
        // Nothing.
        break;
    }
}

static void decodeESAirborneVelocity(struct modesMessage *mm, int check_imf)
{
    // Airborne Velocity Message
    unsigned char *me = mm->ME;

    // 1-5: ME type
    // 6-8: ME subtype
    mm->mesub = getbits(me, 6, 8);

    if (mm->mesub < 1 || mm->mesub > 4)
        return;

    // 9: IMF or Intent Change
    if (check_imf && getbit(me, 9))
        setIMF(mm);

    // 10: reserved

    // 11-13: NACv (NUCr in v0, maps directly to NACv in v2)
    mm->accuracy.nac_v_valid = 1;
    mm->accuracy.nac_v = getbits(me, 11, 13);

    // 14-35: speed/velocity depending on subtype
    switch (mm->mesub) {
    case 1: case 2:
        {
            // 14:    E/W direction
            // 15-24: E/W speed
            // 25:    N/S direction
            // 26-35: N/S speed
            unsigned ew_raw = getbits(me, 15, 24);
            unsigned ns_raw = getbits(me, 26, 35);

            if (ew_raw && ns_raw) {
                int ew_vel = (ew_raw - 1) * (getbit(me, 14) ? -1 : 1) * ((mm->mesub == 2) ? 4 : 1);
                int ns_vel = (ns_raw - 1) * (getbit(me, 25) ? -1 : 1) * ((mm->mesub == 2) ? 4 : 1);

                // Compute velocity and angle from the two speed components
                mm->gs.v0 = mm->gs.v2 = mm->gs.selected = sqrtf((ns_vel * ns_vel) + (ew_vel * ew_vel) + 0.5);
                mm->gs_valid = 1;

                if (mm->gs.selected > 0) {
                    float ground_track = atan2(ew_vel, ns_vel) * 180.0 / M_PI;
                    // We don't want negative values but a 0-360 scale
                    if (ground_track < 0)
                        ground_track += 360;
                    mm->heading = ground_track;
                    mm->heading_type = HEADING_GROUND_TRACK;
                    mm->heading_valid = 1;
                }
            }
            break;
        }

    case 3: case 4:
        {
            // 14:    heading status
            // 15-24: heading
            if (getbit(me, 14)) {
                mm->heading_valid = 1;
                mm->heading = getbits(me, 15, 24) * 360.0 / 1024.0;
                mm->heading_type = HEADING_MAGNETIC_OR_TRUE;
            }

            // 25: airspeed type
            // 26-35: airspeed
            unsigned airspeed = getbits(me, 26, 35);
            if (airspeed) {
                unsigned speed = (airspeed - 1) * (mm->mesub == 4 ? 4 : 1);
                if (getbit(me, 25)) {
                    mm->tas_valid = 1;
                    mm->tas = speed;
                } else {
                    mm->ias_valid = 1;
                    mm->ias = speed;
                }
            }

            break;
        }
    }

    // 36: vert rate source
    // 37: vert rate sign
    // 38-46: vert rate magnitude
    unsigned vert_rate = getbits(me, 38, 46);
    unsigned vert_rate_is_baro = getbit(me, 36);
    if (vert_rate) {
        int rate = (vert_rate - 1) * (getbit(me, 37) ? -64 : 64);
        if (vert_rate_is_baro) {
            mm->baro_rate = rate;
            mm->baro_rate_valid = 1;
        } else {
            mm->geom_rate = rate;
            mm->geom_rate_valid = 1;
        }
    }

    // 47-48: reserved

    // 49: baro/geom delta sign
    // 50-56: baro/geom delta magnitude
    unsigned raw_delta = getbits(me, 50, 56);
    if (raw_delta) {
        mm->geom_delta_valid = 1;
        mm->geom_delta = (raw_delta - 1) * (getbit(me, 49) ? -25 : 25);
    }
}

static void decodeESSurfacePosition(struct modesMessage *mm, int check_imf)
{
    // Surface position and movement
    unsigned char *me = mm->ME;

    mm->airground = AG_GROUND; // definitely.
    mm->cpr_valid = 1;
    mm->cpr_type = CPR_SURFACE;

    // 6-12: Movement
    unsigned movement = getbits(me, 6, 12);
    if (movement > 0 && movement < 125) {
        mm->gs_valid = 1;
        mm->gs.selected = mm->gs.v0 = decodeMovementFieldV0(movement); // assumed v0 until told otherwise
        mm->gs.v2 = decodeMovementFieldV2(movement);
    }

    // 13: Heading/track status
    // 14-20: Heading/track
    if (getbit(me, 13)) {
        mm->heading_valid = 1;
        mm->heading = getbits(me, 14, 20) * 360.0 / 128.0;
        mm->heading_type = HEADING_TRACK_OR_HEADING;
    }

    // 21: IMF or T flag
    if (check_imf && getbit(me, 21))
        setIMF(mm);

    // 22: F flag (odd/even)
    mm->cpr_odd = getbit(me, 22);

    // 23-39: CPR encoded latitude
    mm->cpr_lat = getbits(me, 23, 39);
    // 40-56: CPR encoded longitude
    mm->cpr_lon = getbits(me, 40, 56);
}

static void decodeESAirbornePosition(struct modesMessage *mm, int check_imf)
{
    // Airborne position and altitude
    unsigned char *me = mm->ME;

    // 6-7: surveillance status
    switch (getbits(me, 6, 7)) {
        case 0:
            // no status
            mm->alert_valid = mm->spi_valid = 1;
            mm->alert = mm->spi = 0;
            break;
        case 1: // permanent alert
        case 2: // temporary alert
            mm->alert_valid = 1;
            mm->alert = 1;
            // states 1/2 override state 3, so we don't know SPI status here.
            break;
        case 3: // SPI
            // we know there's no alert in this case
            mm->alert_valid = mm->spi_valid = 1;
            mm->alert = 0;
            mm->spi = 1;
            break;
    }

    // 8: IMF or NIC supplement-B

    if (check_imf) {
        if (getbit(me, 8))
            setIMF(mm);
    } else {
        // NIC-B (v2) or SAF (v0/v1)
        mm->accuracy.nic_b_valid = 1;
        mm->accuracy.nic_b = getbit(me, 8);
    }

    // 9-20: altitude
    unsigned AC12Field = getbits(me, 9, 20);

    if (mm->metype == 0) {
        // no position information
    } else {
        // 21: T flag (UTC sync or not)
        // 22: F flag (odd or even)
        // 23-39: CPR encoded latitude
        // 40-56: CPR encoded longitude

        mm->cpr_lat = getbits(me, 23, 39);
        mm->cpr_lon = getbits(me, 40, 56);

        // Catch some common failure modes and don't mark them as valid
        // (so they won't be used for positioning)
        if (AC12Field == 0 && mm->cpr_lon == 0 && (mm->cpr_lat & 0x0fff) == 0 && mm->metype == 15) {
            // Seen from at least:
            //   400F3F (Eurocopter ECC155 B1) - Bristow Helicopters
            //   4008F3 (BAE ATP) - Atlantic Airlines
            //   400648 (BAE ATP) - Atlantic Airlines
            // altitude == 0, longitude == 0, type == 15 and zeros in latitude LSB.
            // Can alternate with valid reports having type == 14
            Modes.stats_current.cpr_filtered++;
        } else {
            // Otherwise, assume it's valid.
            mm->cpr_valid = 1;
            mm->cpr_type = CPR_AIRBORNE;
            mm->cpr_odd = getbit(me, 22);
        }
    }

    if (AC12Field && mm->airground != AG_GROUND) {// Only attempt to decode if a valid (non zero) altitude is present and not on ground
        altitude_unit_t unit;
        int alt = decodeAC12Field(AC12Field, &unit);
        if (alt != INVALID_ALTITUDE) {
            // If we haven't set airground yet (e.g. DF18)
            // then we can at least set it to UNCERTAIN here
            if (mm->airground == AG_INVALID)
                mm->airground = AG_UNCERTAIN;

            if (mm->metype == 20 || mm->metype == 21 || mm->metype == 22) {
                mm->altitude_geom = alt;
                mm->altitude_geom_unit = unit;
                mm->altitude_geom_valid = 1;
            } else {
                mm->altitude_baro = alt;
                mm->altitude_baro_unit = unit;
                mm->altitude_baro_valid = 1;
            }
        }
    }
}

static void decodeESTestMessage(struct modesMessage *mm)
{
    unsigned char *me = mm->ME;

    mm->mesub = getbits(me, 6, 8);

    if (mm->mesub == 7) {               // (see 1090-WP-15-20)
        int ID13Field = getbits(me, 9, 21);
        if (ID13Field) {
            mm->squawk_valid = 1;
            mm->squawk   = decodeID13Field(ID13Field);
        }
    }
}

static void decodeESAircraftStatus(struct modesMessage *mm, int check_imf)
{
    // Extended Squitter Aircraft Status
    unsigned char *me = mm->ME;

    mm->mesub = getbits(me, 6, 8);

    if (mm->mesub == 1) {      // Emergency status squawk field
        mm->emergency_valid = 1;
        mm->emergency = (emergency_t) getbits(me, 9, 11);

        unsigned ID13Field = getbits(me, 12, 24);
        if (ID13Field) {
            mm->squawk_valid = 1;
            mm->squawk   = decodeID13Field(ID13Field);
        }

        if (check_imf && getbit(me, 56))
            setIMF(mm);
    }
}

static void decodeESTargetStatus(struct modesMessage *mm, int check_imf)
{
    unsigned char *me = mm->ME;

    mm->mesub = getbits(me, 6, 7); // an unusual message: only 2 bits of subtype

    if (check_imf && getbit(me, 51))
        setIMF(mm);

    if (mm->mesub == 0 && getbit(me, 11) == 0) { // Target state and status, V1
        // 8-9: vertical source
        switch (getbits(me, 8, 9)) {
        case 1:
            mm->nav.altitude_source = NAV_ALT_MCP;
            break;
        case 2:
            mm->nav.altitude_source = NAV_ALT_AIRCRAFT;
            break;
        case 3:
            mm->nav.altitude_source = NAV_ALT_FMS;
            break;
        default:
            // nothing
            break;
        }
        // 10: target altitude type (MSL or Baro, ignored)
        // 11: backward compatibility bit, always 0
        // 12-13: target alt capabilities (ignored)
        // 14-15: vertical mode
        switch (getbits(me, 14, 15)) {
        case 1: // "acquiring"
            mm->nav.modes_valid = 1;
            if (mm->nav.altitude_source == NAV_ALT_FMS) {
                mm->nav.modes |= NAV_MODE_VNAV;
            } else {
                mm->nav.modes |= NAV_MODE_AUTOPILOT;
            }
            break;
        case 2: // "maintaining"
            mm->nav.modes_valid = 1;
            if (mm->nav.altitude_source == NAV_ALT_FMS) {
                mm->nav.modes |= NAV_MODE_VNAV;
            } else if (mm->nav.altitude_source == NAV_ALT_AIRCRAFT) {
                mm->nav.modes |= NAV_MODE_ALT_HOLD;
            } else {
                mm->nav.modes |= NAV_MODE_AUTOPILOT;
            }
            break;
        default:
            // nothing
            break;
        }

        // 16-25: target altitude
        int alt = -1000 + 100 * getbits(me, 16, 25);
        switch (mm->nav.altitude_source) {
        case NAV_ALT_MCP:
            mm->nav.mcp_altitude_valid = 1;
            mm->nav.mcp_altitude = alt;
            break;
        case NAV_ALT_FMS:
            mm->nav.fms_altitude_valid = 1;
            mm->nav.fms_altitude = alt;
            break;
        default:
            // nothing
            break;
        }
        // 26-27: horizontal data source
        unsigned h_source = getbits(me, 26, 27);
        if (h_source != 0) {
            // 28-36: target heading/track
            mm->nav.heading_valid = 1;
            mm->nav.heading = getbits(me, 28, 36);
            // 37: track vs heading
            if (getbit(me, 37)) {
                mm->nav.heading_type = HEADING_GROUND_TRACK;
            } else {
                mm->nav.heading_type = HEADING_MAGNETIC_OR_TRUE;
            }
        }
        // 38-39: horizontal mode
        switch (getbits(me, 38, 39)) {
        case 1: // acquiring
        case 2: // maintaining
            mm->nav.modes_valid = 1;
            if (h_source == 3) { // FMS
                mm->nav.modes |= NAV_MODE_LNAV;
            } else {
                mm->nav.modes |= NAV_MODE_AUTOPILOT;
            }
            break;
        default:
            // nothing
            break;
        }

        // 40-43: NACp
        mm->accuracy.nac_p_valid = 1;
        mm->accuracy.nac_p = getbits(me, 40, 43);

        // 44:    NICbaro
        mm->accuracy.nic_baro_valid = 1;
        mm->accuracy.nic_baro = getbit(me, 44);

        // 45-46: SIL
        mm->accuracy.sil = getbits(me, 45, 46);
        mm->accuracy.sil_type = SIL_UNKNOWN;

        // 47-51: reserved

        // 52-53: TCAS status
        switch (getbits(me, 52, 53)) {
        case 1:
            mm->nav.modes_valid = 1;
            // no tcas
            break;
        case 2:
        case 3:
            mm->nav.modes_valid = 1;
            mm->nav.modes |= NAV_MODE_TCAS;
            break;
        case 0:
            // assume TCAS if we had any other modes
            // but don't enable modes just for this
            mm->nav.modes |= NAV_MODE_TCAS;
            break;
        default:
            // nothing
            break;
        }


        // 54-56: emergency/priority
        mm->emergency_valid = 1;
        mm->emergency = (emergency_t) getbits(me, 54, 56);
    } else if (mm->mesub == 1) { // Target state and status, V2
        // 8: SIL
        unsigned is_fms = getbit(me, 9);

        unsigned alt_bits = getbits(me, 10, 20);
        if (alt_bits != 0) {
            if (is_fms) {
                mm->nav.fms_altitude_valid = 1;
                mm->nav.fms_altitude = (alt_bits - 1) * 32;
            } else {
                mm->nav.mcp_altitude_valid = 1;
                mm->nav.mcp_altitude = (alt_bits - 1) * 32;
            }
        }

        unsigned baro_bits = getbits(me, 21, 29);
        if (baro_bits != 0) {
            mm->nav.qnh_valid = 1;
            mm->nav.qnh = 800.0 + (baro_bits - 1) * 0.8;
        }

        if (getbit(me, 30)) {
            mm->nav.heading_valid = 1;
            // two's complement -180..+180, which is conveniently
            // also the same as unsigned 0..360
            mm->nav.heading = getbits(me, 31, 39) * 180.0 / 256.0;
            mm->nav.heading_type = HEADING_MAGNETIC_OR_TRUE;
        }

        // 40-43: NACp
        mm->accuracy.nac_p_valid = 1;
        mm->accuracy.nac_p = getbits(me, 40, 43);

        // 44:    NICbaro
        mm->accuracy.nic_baro_valid = 1;
        mm->accuracy.nic_baro = getbit(me, 44);

        // 45-46: SIL
        mm->accuracy.sil = getbits(me, 45, 46);
        mm->accuracy.sil_type = SIL_UNKNOWN;

        // 47: mode bits validity
        if (getbit(me, 47)) {
            // 48-54: mode bits
            mm->nav.modes_valid = 1;
            mm->nav.modes =
                (getbit(me, 48) ? NAV_MODE_AUTOPILOT : 0) |
                (getbit(me, 49) ? NAV_MODE_VNAV : 0) |
                (getbit(me, 50) ? NAV_MODE_ALT_HOLD : 0) |
                // 51: IMF
                (getbit(me, 52) ? NAV_MODE_APPROACH : 0) |
                (getbit(me, 53) ? NAV_MODE_TCAS : 0) |
                (getbit(me, 54) ? NAV_MODE_LNAV : 0);
        }

        // 55-56 reserved
    }
}

static void decodeESOperationalStatus(struct modesMessage *mm, int check_imf)
{
    unsigned char *me = mm->ME;

    mm->mesub = getbits(me, 6, 8);

    // Aircraft Operational Status
    if (check_imf && getbit(me, 56))
        setIMF(mm);

    if (mm->mesub == 0 || mm->mesub == 1) {
        mm->opstatus.valid = 1;
        mm->opstatus.version = getbits(me, 41, 43);

        switch (mm->opstatus.version) {
        case 0:
            if (mm->mesub == 0 && getbits(me, 9, 10) == 0) {
                mm->opstatus.cc_acas = !getbit(me, 12);
                mm->opstatus.cc_cdti = getbit(me, 13);
            }
            break;

        case 1:
            if (getbits(me, 25, 26) == 0) {
                mm->opstatus.om_acas_ra = getbit(me, 27);
                mm->opstatus.om_ident = getbit(me, 28);
                mm->opstatus.om_atc = getbit(me, 29);
            }

            if (mm->mesub == 0 && getbits(me, 9, 10) == 0 && getbits(me, 13, 14) == 0) {
                // airborne
                mm->opstatus.cc_acas = !getbit(me, 11);
                mm->opstatus.cc_cdti = getbit(me, 12);
                mm->opstatus.cc_arv = getbit(me, 15);
                mm->opstatus.cc_ts = getbit(me, 16);
                mm->opstatus.cc_tc = getbits(me, 17, 18);
            } else if (mm->mesub == 1 && getbits(me, 9, 10) == 0 && getbits(me, 13, 14) == 0) {
                // surface
                mm->opstatus.cc_poa = getbit(me, 11);
                mm->opstatus.cc_cdti = getbit(me, 12);
                mm->opstatus.cc_b2_low = getbit(me, 15);
                mm->opstatus.cc_lw_valid = 1;
                mm->opstatus.cc_lw = getbits(me, 21, 24);
            }

            mm->accuracy.nic_a_valid = 1;
            mm->accuracy.nic_a = getbit(me, 44);
            mm->accuracy.nac_p_valid = 1;
            mm->accuracy.nac_p = getbits(me, 45, 48);
            mm->accuracy.sil_type = SIL_UNKNOWN;
            mm->accuracy.sil = getbits(me, 51, 52);

            mm->opstatus.hrd = getbit(me, 54) ? HEADING_MAGNETIC : HEADING_TRUE;

            if (mm->mesub == 0) {
                mm->accuracy.nic_baro_valid = 1;
                mm->accuracy.nic_baro = getbit(me, 53);
            } else {
                // see DO=260B §2.2.3.2.7.2.12
                // TAH=0 : surface movement reports ground track
                // TAH=1 : surface movement reports aircraft heading
                mm->opstatus.tah = getbit(me, 53) ? mm->opstatus.hrd : HEADING_GROUND_TRACK;
            }
            break;

        case 2:
            if (getbits(me, 25, 26) == 0) {
                mm->opstatus.om_acas_ra = getbit(me, 27);
                mm->opstatus.om_ident = getbit(me, 28);
                mm->opstatus.om_atc = getbit(me, 29);
                mm->opstatus.om_saf = getbit(me, 30);
                mm->accuracy.sda_valid = 1;
                mm->accuracy.sda = getbits(me, 31, 32);
            }

            if (mm->mesub == 0 && getbits(me, 9, 10) == 0) {
                // airborne
                mm->opstatus.cc_acas = getbit(me, 11); // nb inverted sense versus v0/v1
                mm->opstatus.cc_1090_in = getbit(me, 12);
                mm->opstatus.cc_arv = getbit(me, 15);
                mm->opstatus.cc_ts = getbit(me, 16);
                mm->opstatus.cc_tc = getbits(me, 17, 18);
                mm->opstatus.cc_uat_in = getbit(me, 19);
            } else if (mm->mesub == 1 && getbits(me, 9, 10) == 0) {
                // surface
                mm->opstatus.cc_poa = getbit(me, 11);
                mm->opstatus.cc_1090_in = getbit(me, 12);
                mm->opstatus.cc_b2_low = getbit(me, 15);
                mm->opstatus.cc_uat_in = getbit(me, 16);
                mm->accuracy.nac_v_valid = 1;
                mm->accuracy.nac_v = getbits(me, 17, 19);
                mm->accuracy.nic_c_valid = 1;
                mm->accuracy.nic_c = getbit(me, 20);
                mm->opstatus.cc_lw_valid = 1;
                mm->opstatus.cc_lw = getbits(me, 21, 24);
                mm->opstatus.cc_antenna_offset = getbits(me, 33, 40);
            }

            mm->accuracy.nic_a_valid = 1;
            mm->accuracy.nic_a = getbit(me, 44);
            mm->accuracy.nac_p_valid = 1;
            mm->accuracy.nac_p = getbits(me, 45, 48);
            mm->accuracy.sil = getbits(me, 51, 52);
            mm->accuracy.sil_type = getbit(me, 55) ? SIL_PER_SAMPLE : SIL_PER_HOUR;
            mm->opstatus.hrd = getbit(me, 54) ? HEADING_MAGNETIC : HEADING_TRUE;
            if (mm->mesub == 0) {
                mm->accuracy.gva_valid = 1;
                mm->accuracy.gva = getbits(me, 49, 50);
                mm->accuracy.nic_baro_valid = 1;
                mm->accuracy.nic_baro = getbit(me, 53);
            } else {
                // see DO=260B §2.2.3.2.7.2.12
                // TAH=0 : surface movement reports ground track
                // TAH=1 : surface movement reports aircraft heading
                mm->opstatus.tah = getbit(me, 53) ? mm->opstatus.hrd : HEADING_GROUND_TRACK;
            }
            break;
        }
    }
}

static void decodeExtendedSquitter(struct modesMessage *mm)
{
    unsigned char *me = mm->ME;
    unsigned metype = mm->metype = getbits(me, 1, 5);
    unsigned check_imf = 0;

    // Check CF on DF18 to work out the format of the ES and whether we need to look for an IMF bit
    if (mm->msgtype == 18) {
        switch (mm->CF) {
        case 0: // ADS-B Message from a non-transponder device, AA field holds 24-bit ICAO aircraft address
            mm->addrtype = ADDR_ADSB_ICAO_NT;
            break;

        case 1: // Reserved for ADS-B Message in which the AA field holds anonymous address or ground vehicle address or fixed obstruction address
            mm->addrtype = ADDR_ADSB_OTHER;
            mm->addr |= MODES_NON_ICAO_ADDRESS;
            break;

        case 2: // Fine TIS-B Message
            // IMF=0: AA field contains the 24-bit ICAO aircraft address
            // IMF=1: AA field contains the 12-bit Mode A code followed by a 12-bit track file number
            mm->source = SOURCE_TISB;
            mm->addrtype = ADDR_TISB_ICAO;
            check_imf = 1;
            break;

        case 3: //   Coarse TIS-B airborne position and velocity.
            // IMF=0: AA field contains the 24-bit ICAO aircraft address
            // IMF=1: AA field contains the 12-bit Mode A code followed by a 12-bit track file number

            // For now we only look at the IMF bit.
            mm->source = SOURCE_TISB;
            mm->addrtype = ADDR_TISB_ICAO;
            if (getbit(me, 1))
                setIMF(mm);
            return;

        case 5: // Fine TIS-B Message, AA field contains a non-ICAO 24-bit address
            mm->addrtype = ADDR_TISB_OTHER;
            mm->source = SOURCE_TISB;
            mm->addr |= MODES_NON_ICAO_ADDRESS;
            break;

        case 6: // Rebroadcast of ADS-B Message from an alternate data link
            // IMF=0: AA field holds 24-bit ICAO aircraft address
            // IMF=1: AA field holds anonymous address or ground vehicle address or fixed obstruction address
            mm->addrtype = ADDR_ADSR_ICAO;
            mm->source = SOURCE_ADSR;
            check_imf = 1;
            break;

        default:    // All others, we don't know the format.
            mm->addrtype = ADDR_UNKNOWN;
            mm->addr |= MODES_NON_ICAO_ADDRESS; // assume non-ICAO
            return;
        }
    }

    switch (metype) {
    case 1: case 2: case 3: case 4:
        decodeESIdentAndCategory(mm);
        break;

    case 19:
        decodeESAirborneVelocity(mm, check_imf);
        break;

    case 5: case 6: case 7: case 8:
        decodeESSurfacePosition(mm, check_imf);
        break;

    case 0: // Airborne position, baro altitude only
    case 9: case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: // Airborne position, baro
    case 20: case 21: case 22: // Airborne position, geometric altitude (HAE or MSL)
        decodeESAirbornePosition(mm, check_imf);
        break;

    case 23:
        decodeESTestMessage(mm);
        break;

    case 24: // Reserved for Surface System Status
        break;

    case 28:
        decodeESAircraftStatus(mm, check_imf);
        break;

    case 29:
        decodeESTargetStatus(mm, check_imf);
        break;

    case 30: // Aircraft Operational Coordination
        break;

    case 31:
        decodeESOperationalStatus(mm, check_imf);
        break;

    default:
        // Dubious.
        mm->reliable = 0;
        break;
    }
}

static const char *df_names[33] = {
    /* 0 */ "Short Air-Air Surveillance",
    /* 1 */ NULL,
    /* 2 */ NULL,
    /* 3 */ NULL,
    /* 4 */ "Survelliance, Altitude Reply",
    /* 5 */ "Survelliance, Identity Reply",
    /* 6 */ NULL,
    /* 7 */ NULL,
    /* 8 */ NULL,
    /* 9 */ NULL,
    /* 10 */ NULL,
    /* 11 */ "All Call Reply",
    /* 12 */ NULL,
    /* 13 */ NULL,
    /* 14 */ NULL,
    /* 15 */ NULL,
    /* 16 */ "Long Air-Air ACAS",
    /* 17 */ "Extended Squitter",
    /* 18 */ "Extended Squitter (Non-Transponder)",
    /* 19 */ "Extended Squitter (Military)",
    /* 20 */ "Comm-B, Altitude Reply",
    /* 21 */ "Comm-B, Identity Reply",
    /* 22 */ "Military Use",
    /* 23 */ NULL,
    /* 24 */ "Comm-D Extended Length Message",
    /* 25 */ "Comm-D Extended Length Message",
    /* 26 */ "Comm-D Extended Length Message",
    /* 27 */ "Comm-D Extended Length Message",
    /* 28 */ "Comm-D Extended Length Message",
    /* 29 */ "Comm-D Extended Length Message",
    /* 30 */ "Comm-D Extended Length Message",
    /* 31 */ "Comm-D Extended Length Message",
    /* 32 */ "Mode A/C Reply",
};

static const char *df_to_string(unsigned df) {
    if (df > 32)
        return "out of range";
    if (!df_names[df])
        return "reserved";
    return df_names[df];
}

static const char *altitude_unit_to_string(altitude_unit_t unit) {
    switch (unit) {
    case UNIT_FEET:
        return "ft";
    case UNIT_METERS:
        return "m";
    default:
        return "(unknown altitude unit)";
    }
}

static const char *airground_to_string(airground_t airground) {
    switch (airground) {
    case AG_GROUND:
        return "ground";
    case AG_AIRBORNE:
        return "airborne";
    case AG_INVALID:
        return "invalid";
    case AG_UNCERTAIN:
        return "airborne?";
    default:
        return "(unknown airground state)";
    }
}

static const char *addrtype_to_string(addrtype_t type) {
    switch (type) {
    case ADDR_ADSB_ICAO:
        return "Mode S / ADS-B";
    case ADDR_ADSB_ICAO_NT:
        return "ADS-B, non-transponder";
    case ADDR_ADSB_OTHER:
        return "ADS-B, other addressing scheme";
    case ADDR_TISB_ICAO:
        return "TIS-B";
    case ADDR_TISB_OTHER:
        return "TIS-B, other addressing scheme";
    case ADDR_TISB_TRACKFILE:
        return "TIS-B, Mode A code and track file number";
    case ADDR_ADSR_ICAO:
        return "ADS-R";
    case ADDR_ADSR_OTHER:
        return "ADS-R, other addressing scheme";
    case ADDR_MODE_A:
        return "Mode A";
    default:
        return "unknown addressing scheme";
    }
}

static const char *cpr_type_to_string(cpr_type_t type) {
    switch (type) {
    case CPR_SURFACE:
        return "Surface";
    case CPR_AIRBORNE:
        return "Airborne";
    case CPR_COARSE:
        return "TIS-B Coarse";
    default:
        return "unknown CPR type";
    }
}

static const char *heading_type_to_string(heading_type_t type) {
    switch (type) {
    case HEADING_GROUND_TRACK:
        return "Ground track";
    case HEADING_MAGNETIC:
        return "Mag heading";
    case HEADING_TRUE:
        return "True heading";
    case HEADING_MAGNETIC_OR_TRUE:
        return "Heading";
    case HEADING_TRACK_OR_HEADING:
        return "Track/Heading";
    default:
        return "unknown heading type";
    }
}

static const char *commb_format_to_string(commb_format_t format) {
    switch (format) {
    case COMMB_EMPTY_RESPONSE:
        return "empty response";
    case COMMB_AMBIGUOUS:
        return "ambiguous format";
    case COMMB_NOT_DECODED:
        return "not decoded";
    case COMMB_DATALINK_CAPS:
        return "BDS1,0 Datalink capabilities";
    case COMMB_GICB_CAPS:
        return "BDS1,7 Common usage GICB capabilities";
    case COMMB_AIRCRAFT_IDENT:
        return "BDS2,0 Aircraft identification";
    case COMMB_ACAS_RA:
        return "BDS3,0 ACAS resolution advisory";
    case COMMB_VERTICAL_INTENT:
        return "BDS4,0 Selected vertical intention";
    case COMMB_TRACK_TURN:
        return "BDS5,0 Track and turn report";
    case COMMB_HEADING_SPEED:
        return "BDS6,0 Heading and speed report";
    case COMMB_MRAR:
        return "BDS4,4 Meterological routine air report";
    case COMMB_AIRBORNE_POSITION:
        return "BDS0,5 Extended squitter airborne position";
    default:
        return "unknown format";
    }
}

static const char *nav_modes_to_string(nav_modes_t flags)
{
    static char buf[128];

    buf[0] = 0;
    if (flags & NAV_MODE_AUTOPILOT)
        strcat(buf, "autopilot ");
    if (flags & NAV_MODE_VNAV)
        strcat(buf, "vnav ");
    if (flags & NAV_MODE_ALT_HOLD)
        strcat(buf, "althold ");
    if (flags & NAV_MODE_APPROACH)
        strcat(buf, "approach ");
    if (flags & NAV_MODE_LNAV)
        strcat(buf, "lnav ");
    if (flags & NAV_MODE_TCAS)
        strcat(buf, "tcas ");

    if (buf[0] != 0)
        buf[strlen(buf)-1] = 0;

    return buf;
}

static const char *sil_type_to_string(sil_type_t type)
{
    switch (type) {
    case SIL_UNKNOWN: return "unknown type";
    case SIL_PER_HOUR: return "per flight hour";
    case SIL_PER_SAMPLE: return "per sample";
    default: return "invalid type";
    }
}

static const char *emergency_to_string(emergency_t emergency)
{
    switch (emergency) {
    case EMERGENCY_NONE:      return "no emergency";
    case EMERGENCY_GENERAL:   return "general emergency (7700)";
    case EMERGENCY_LIFEGUARD: return "lifeguard / medical emergency";
    case EMERGENCY_MINFUEL:   return "minimum fuel";
    case EMERGENCY_NORDO:     return "no communications (7600)";
    case EMERGENCY_UNLAWFUL:  return "unlawful interference (7500)";
    case EMERGENCY_DOWNED:    return "downed aircraft";
    default:                  return "reserved";
    }
}

static const char *mrar_source_to_string(mrar_source_t source)
{
    switch (source) {
    case MRAR_SOURCE_INVALID: return "invalid";
    case MRAR_SOURCE_INS:     return "INS";
    case MRAR_SOURCE_GNSS:    return "GNSS";
    case MRAR_SOURCE_DMEDME:  return "DME/DME";
    case MRAR_SOURCE_VORDME:  return "VOR/DME";
    default:                  return "reserved";
    }
}

static const char *hazard_to_string(hazard_t hazard)
{
    switch (hazard) {
    case HAZARD_NIL:      return "nil";
    case HAZARD_LIGHT:    return "light";
    case HAZARD_MODERATE: return "moderate";
    case HAZARD_SEVERE:   return "severe";
    default:              return "invalid hazard severity";
    }
}

static void print_hex_bytes(unsigned char *data, size_t len) {
    size_t i;
    for (i = 0; i < len; ++i) {
        printf("%02X", (unsigned)data[i]);
    }
}

static int esTypeHasSubtype(unsigned metype)
{
    if (metype <= 18) {
        return 0;
    }

    if (metype >= 20 && metype <= 22) {
        return 0;
    }

    return 1;
}

static const char *esTypeName(unsigned metype, unsigned mesub)
{
    switch (metype) {
    case 0:
        return "No position information (airborne or surface)";

    case 1: case 2: case 3: case 4:
        return "Aircraft identification and category";

    case 5: case 6: case 7: case 8:
        return "Surface position";

    case 9: case 10: case 11: case 12:
    case 13: case 14: case 15: case 16:
    case 17: case 18:
        return "Airborne position (barometric altitude)";

    case 19:
        switch (mesub) {
        case 1:
            return "Airborne velocity over ground, subsonic";
        case 2:
            return "Airborne velocity over ground, supersonic";
        case 3:
            return "Airspeed and heading, subsonic";
        case 4:
            return "Airspeed and heading, supersonic";
        default:
            return "Unknown";
        }

    case 20: case 21: case 22:
        return "Airborne position (geometric altitude)";

    case 23:
        switch (mesub) {
        case 0:
            return "Test message";
        case 7:
            return "National use / 1090-WP-15-20 Mode A squawk";
        default:
            return "Unknown";
        }

    case 24:
        return "Reserved for surface system status";

    case 27:
        return "Reserved for trajectory change";

    case 28:
        switch (mesub) {
        case 1:
            return "Emergency/priority status";
        case 2:
            return "ACAS RA broadcast";
        default:
            return "Unknown";
        }

    case 29:
        switch (mesub) {
        case 0:
            return "Target state and status (V1)";
        case 1:
            return "Target state and status (V2)";
        default:
            return "Unknown";
        }

    case 30:
        return "Aircraft Operational Coordination";

    case 31: // Aircraft Operational Status
        switch (mesub) {
        case 0:
            return "Aircraft operational status (airborne)";
        case 1:
            return "Aircraft operational status (surface)";
        default:
            return "Unknown";
        }

    default:
        return "Unknown";
    }
}

void displayModesMessage(struct modesMessage *mm) {
    int j;

    // Handle only addresses mode first.
    if (Modes.onlyaddr) {
        printf("%06x\n", mm->addr);
        return;         // Enough for --onlyaddr mode
    }

    // Show the raw message.
    if (Modes.mlat && mm->timestampMsg) {
        printf("@%012" PRIX64, mm->timestampMsg);
    } else
        printf("*");

    for (j = 0; j < mm->msgbits/8; j++) printf("%02x", mm->msg[j]);
    printf(";\n");

    if (Modes.raw) {
        fflush(stdout); // Provide data to the reader ASAP
        return;         // Enough for --raw mode
    }

    if (mm->msgtype < 32)
        printf("CRC: %06x\n", mm->crc);

    if (mm->correctedbits != 0)
        printf("No. of bit errors fixed: %d\n", mm->correctedbits);

    if (mm->signalLevel > 0)
        printf("RSSI: %.1f dBFS\n", 10 * log10(mm->signalLevel));

    if (mm->score)
        printf("Score: %d (%s)\n", mm->score, score_to_string(mm->score));

    if (mm->timestampMsg) {
        if (mm->timestampMsg == MAGIC_MLAT_TIMESTAMP)
            printf("This is a synthetic MLAT message.\n");
        else
            printf("Time: %.2fus\n", mm->timestampMsg / 12.0);
    }

    switch (mm->msgtype) {
    case 0:
        printf("DF:0 addr:%06X VS:%u CC:%u SL:%u RI:%u AC:%u\n",
               mm->addr, mm->VS, mm->CC, mm->SL, mm->RI, mm->AC);
        break;

    case 4:
        printf("DF:4 addr:%06X FS:%u DR:%u UM:%u AC:%u\n",
               mm->addr, mm->FS, mm->DR, mm->UM, mm->AC);
        break;

    case 5:
        printf("DF:5 addr:%06X FS:%u DR:%u UM:%u ID:%u\n",
               mm->addr, mm->FS, mm->DR, mm->UM, mm->ID);
        break;

    case 11:
        printf("DF:11 AA:%06X IID:%u CA:%u\n",
               mm->AA, mm->IID, mm->CA);
        break;

    case 16:
        printf("DF:16 addr:%06x VS:%u SL:%u RI:%u AC:%u MV:",
               mm->addr, mm->VS, mm->SL, mm->RI, mm->AC);
        print_hex_bytes(mm->MV, sizeof(mm->MV));
        printf("\n");
        break;

    case 17:
        printf("DF:17 AA:%06X CA:%u ME:",
               mm->AA, mm->CA);
        print_hex_bytes(mm->ME, sizeof(mm->ME));
        printf("\n");
        break;

    case 18:
        printf("DF:18 AA:%06X CF:%u ME:",
               mm->AA, mm->CF);
        print_hex_bytes(mm->ME, sizeof(mm->ME));
        printf("\n");
        break;

    case 20:
        printf("DF:20 addr:%06X FS:%u DR:%u UM:%u AC:%u MB:",
               mm->addr, mm->FS, mm->DR, mm->UM, mm->AC);
        print_hex_bytes(mm->MB, sizeof(mm->MB));
        printf("\n");
        break;

    case 21:
        printf("DF:21 addr:%06x FS:%u DR:%u UM:%u ID:%u MB:",
               mm->addr, mm->FS, mm->DR, mm->UM, mm->ID);
        print_hex_bytes(mm->MB, sizeof(mm->MB));
        printf("\n");
        break;

    case 24:
        /* 25 .. 31 also remapped to 24 during decoding */
        printf("DF:24 addr:%06x KE:%u ND:%u MD:",
               mm->addr, mm->KE, mm->ND);
        print_hex_bytes(mm->MD, sizeof(mm->MD));
        printf("\n");
        break;

    default:
        printf("DF:%u", mm->msgtype);
        break;
    }

    printf(" %s", df_to_string(mm->msgtype));
    if (mm->msgtype == 17 || mm->msgtype == 18) {
        if (esTypeHasSubtype(mm->metype)) {
            printf(" %s (%u/%u)",
                   esTypeName(mm->metype, mm->mesub),
                   mm->metype,
                   mm->mesub);
        } else {
            printf(" %s (%u)",
                   esTypeName(mm->metype, mm->mesub),
                   mm->metype);
        }
    }
    if (mm->reliable) {
        printf(" (reliable)");
    }
    printf("\n");

    if (mm->msgtype == 20 || mm->msgtype == 21) {
        printf("  Comm-B format: %s\n", commb_format_to_string(mm->commb_format));
    }

    if (mm->addr & MODES_NON_ICAO_ADDRESS) {
        printf("  Other Address: %06X (%s)\n", mm->addr & 0xFFFFFF, addrtype_to_string(mm->addrtype));
    } else {
        printf("  ICAO Address:  %06X (%s)\n", mm->addr, addrtype_to_string(mm->addrtype));
    }

    if (mm->airground != AG_INVALID) {
        printf("  Air/Ground:    %s\n",
               airground_to_string(mm->airground));
    }

    if (mm->altitude_baro_valid) {
        printf("  Baro altitude: %d %s\n",
               mm->altitude_baro,
               altitude_unit_to_string(mm->altitude_baro_unit));
    }

    if (mm->altitude_geom_valid) {
        printf("  Geom altitude: %d %s\n",
               mm->altitude_geom,
               altitude_unit_to_string(mm->altitude_geom_unit));
    }

    if (mm->geom_delta_valid) {
        printf("  Geom - baro:   %d ft\n",
               mm->geom_delta);
    }

    if (mm->heading_valid) {
        printf("  %-13s  %.1f\n", heading_type_to_string(mm->heading_type), mm->heading);
    }

    if (mm->track_rate_valid) {
        printf("  Track rate:    %.2f deg/sec %s\n", mm->track_rate, mm->track_rate < 0 ? "left" : mm->track_rate > 0 ? "right" : "");
    }

    if (mm->roll_valid) {
        printf("  Roll:          %.1f degrees %s\n", mm->roll, mm->roll < -0.05 ? "left" : mm->roll > 0.05 ? "right" : "");
    }

    if (mm->gs_valid) {
        printf("  Groundspeed:   %.1f kt", mm->gs.selected);
        if (mm->gs.v0 != mm->gs.selected) {
            printf(" (v0: %.1f kt)", mm->gs.v0);
        }
        if (mm->gs.v2 != mm->gs.selected) {
            printf(" (v2: %.1f kt)", mm->gs.v2);
        }
        printf("\n");
    }

    if (mm->ias_valid) {
        printf("  IAS:           %u kt\n", mm->ias);
    }

    if (mm->tas_valid) {
        printf("  TAS:           %u kt\n", mm->tas);
    }

    if (mm->mach_valid) {
        printf("  Mach number:   %.3f\n", mm->mach);
    }

    if (mm->baro_rate_valid) {
        printf("  Baro rate:     %d ft/min\n", mm->baro_rate);
    }

    if (mm->geom_rate_valid) {
        printf("  Geom rate:     %d ft/min\n", mm->geom_rate);
    }

    if (mm->squawk_valid) {
        printf("  Squawk:        %04x\n",
               mm->squawk);
    }

    if (mm->callsign_valid) {
        printf("  Ident:         %s\n",
               mm->callsign);
    }

    if (mm->category_valid) {
        printf("  Category:      %02X\n",
               mm->category);
    }

    if (mm->cpr_valid) {
        printf("  CPR type:      %s\n"
               "  CPR odd flag:  %s\n",
               cpr_type_to_string(mm->cpr_type),
               mm->cpr_odd ? "odd" : "even");

        if (mm->cpr_decoded) {
            printf("  CPR latitude:  %.5f (%u)\n"
                   "  CPR longitude: %.5f (%u)\n"
                   "  CPR decoding:  %s\n"
                   "  NIC:           %u\n"
                   "  Rc:            %.3f km / %.1f NM\n",
                   mm->decoded_lat,
                   mm->cpr_lat,
                   mm->decoded_lon,
                   mm->cpr_lon,
                   mm->cpr_relative ? "local" : "global",
                   mm->decoded_nic,
                   mm->decoded_rc / 1000.0,
                   mm->decoded_rc / 1852.0);
        } else {
            printf("  CPR latitude:  (%u)\n"
                   "  CPR longitude: (%u)\n"
                   "  CPR decoding:  none\n",
                   mm->cpr_lat,
                   mm->cpr_lon);
        }
    }

    if (mm->accuracy.nic_a_valid) {
        printf("  NIC-A:         %d\n", mm->accuracy.nic_a);
    }
    if (mm->accuracy.nic_b_valid) {
        printf("  NIC-B:         %d\n", mm->accuracy.nic_b);
    }
    if (mm->accuracy.nic_c_valid) {
        printf("  NIC-C:         %d\n", mm->accuracy.nic_c);
    }
    if (mm->accuracy.nic_baro_valid) {
        printf("  NIC-baro:      %d\n", mm->accuracy.nic_baro);
    }
    if (mm->accuracy.nac_p_valid) {
        printf("  NACp:          %d\n", mm->accuracy.nac_p);
    }
    if (mm->accuracy.nac_v_valid) {
        printf("  NACv:          %d\n", mm->accuracy.nac_v);
    }
    if (mm->accuracy.gva_valid) {
        printf("  GVA:           %d\n", mm->accuracy.gva);
    }
    if (mm->accuracy.sil_type != SIL_INVALID) {
        const char *sil_description;
        switch (mm->accuracy.sil) {
        case 1:
            sil_description = "p <= 0.1%";
            break;
        case 2:
            sil_description = "p <= 0.001%";
            break;
        case 3:
            sil_description = "p <= 0.00001%";
            break;
        default:
            sil_description = "p > 0.1%";
            break;
        }
        printf("  SIL:           %d (%s, %s)\n",
               mm->accuracy.sil,
               sil_description,
               sil_type_to_string(mm->accuracy.sil_type));
    }
    if (mm->accuracy.sda_valid) {
        printf("  SDA:           %d\n", mm->accuracy.sda);
    }

    if (mm->opstatus.valid) {
        printf("  Aircraft Operational Status:\n");
        printf("    Version:            %d\n", mm->opstatus.version);

        printf("    Capability classes: ");
        if (mm->opstatus.cc_acas) printf("ACAS ");
        if (mm->opstatus.cc_cdti) printf("CDTI ");
        if (mm->opstatus.cc_1090_in) printf("1090IN ");
        if (mm->opstatus.cc_arv) printf("ARV ");
        if (mm->opstatus.cc_ts) printf("TS ");
        if (mm->opstatus.cc_tc) printf("TC=%d ", mm->opstatus.cc_tc);
        if (mm->opstatus.cc_uat_in) printf("UATIN ");
        if (mm->opstatus.cc_poa) printf("POA ");
        if (mm->opstatus.cc_b2_low) printf("B2-LOW ");
        if (mm->opstatus.cc_lw_valid) printf("L/W=%d ", mm->opstatus.cc_lw);
        if (mm->opstatus.cc_antenna_offset) printf("GPS-OFFSET=%d ", mm->opstatus.cc_antenna_offset);
        printf("\n");

        printf("    Operational modes:  ");
        if (mm->opstatus.om_acas_ra) printf("ACASRA ");
        if (mm->opstatus.om_ident)   printf("IDENT ");
        if (mm->opstatus.om_atc)     printf("ATC ");
        if (mm->opstatus.om_saf)     printf("SAF ");
        printf("\n");

        if (mm->mesub == 1)
            printf("    Track/heading:      %s\n", heading_type_to_string(mm->opstatus.tah));
        printf("    Heading ref dir:    %s\n", heading_type_to_string(mm->opstatus.hrd));
    }

    if (mm->nav.heading_valid)
        printf("  Selected heading:        %.1f\n", mm->nav.heading);
    if (mm->nav.fms_altitude_valid)
        printf("  FMS selected altitude:   %u ft\n", mm->nav.fms_altitude);
    if (mm->nav.mcp_altitude_valid)
        printf("  MCP selected altitude:   %u ft\n", mm->nav.mcp_altitude);
    if (mm->nav.qnh_valid)
        printf("  QNH:                     %.1f millibars\n", mm->nav.qnh);
    if (mm->nav.altitude_source != NAV_ALT_INVALID) {
        printf("  Target altitude source:  ");
        switch (mm->nav.altitude_source) {
        case NAV_ALT_AIRCRAFT:
            printf("aircraft altitude\n");
            break;
        case NAV_ALT_MCP:
            printf("MCP selected altitude\n");
            break;
        case NAV_ALT_FMS:
            printf("FMS selected altitude\n");
            break;
        default:
            printf("unknown\n");
        }
    }

    if (mm->nav.modes_valid) {
        printf("  Nav modes:               %s\n", nav_modes_to_string(mm->nav.modes));
    }

    if (mm->emergency_valid) {
        printf("  Emergency/priority:      %s\n", emergency_to_string(mm->emergency));
    }

    if (mm->mrar_source_valid)
        printf("  MRAR FOM/Source:         %s\n", mrar_source_to_string(mm->mrar_source));
    if (mm->wind_valid) {
        printf("  Wind speed:              %.0f kt\n", mm->wind_speed);
        printf("  Wind direction:          %.1f degrees\n", mm->wind_dir);
    }
    if (mm->temperature_valid)
        printf("  Air temperature:         %.1f degrees C\n", mm->temperature);
    if (mm->pressure_valid)
        printf("  Static pressure:         %.0f hPa\n", mm->pressure);
    if (mm->turbulence_valid)
        printf("  Turbulence:              %s\n", hazard_to_string(mm->turbulence));
    if (mm->humidity_valid)
        printf("  Humidity:                %.0f%%\n", mm->humidity);

    printf("\n");
    fflush(stdout);
}

//
//=========================================================================
//
// When a new message is available, because it was decoded from the RTL device,
// file, or received in the TCP input port, or any other way we can receive a
// decoded message, we call this function in order to use the message.
//
// Basically this function passes a raw message to the upper layers for further
// processing and visualization
//
void useModesMessage(struct modesMessage *mm) {
    struct aircraft *a;

    ++Modes.stats_current.messages_total;
    if (mm->msgtype >= 0 && mm->msgtype < 32) {
        ++Modes.stats_current.messages_by_df[mm->msgtype];
    }

    // Track aircraft state
    a = trackUpdateFromMessage(mm);

    // In non-interactive non-quiet mode, display messages on standard output
    if (!Modes.interactive && !Modes.quiet && (!Modes.show_only || mm->addr == Modes.show_only)) {
        displayModesMessage(mm);
    }

    // Feed output clients; modesQueueOutput appropriately filters messages to the different outputs.
    if (Modes.net) {
        modesQueueOutput(mm, a);
    }
}

//
// ===================== Mode S detection and decoding  ===================
//
