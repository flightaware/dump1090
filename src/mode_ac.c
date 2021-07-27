// dump1090, a Mode S messages decoder for RTLSDR devices.
//
// Copyright (C) 2012 by Salvatore Sanfilippo <antirez@gmail.com>
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//  *  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
//
//  *  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "dump1090.h"
#include <assert.h>
//
//=========================================================================
//
// Input format is : 00:A4:A2:A1:00:B4:B2:B1:00:C4:C2:C1:00:D4:D2:D1
//

static int modeAToCTable[4096];
static unsigned modeCToATable[4096];
static int internalModeAToModeC(unsigned int ModeA);

void modeACInit()
{
    for (unsigned i = 0; i < 4096; ++i) {
        unsigned modeA = indexToModeA(i);
        int modeC = internalModeAToModeC(modeA);
        modeAToCTable[i] = modeC;

        modeC += 13;
        if (modeC >= 0 && modeC < 4096) {
            assert(modeCToATable[modeC] == 0);
            modeCToATable[modeC] = modeA;
        }
    }
}

// Given a mode A value (hex-encoded, see above)
// return the mode C value (signed multiple of 100s of feet)
// or INVALID_ALITITUDE if not a valid mode C value
int modeAToModeC(unsigned modeA)
{
    unsigned i = modeAToIndex(modeA);
    if (i >= 4096)
        return INVALID_ALTITUDE;

    return modeAToCTable[i];
}

// Given a mode C value (signed multiple of 100s of feet)
// return the mode A value, or 0 if not a valid mode C value
unsigned modeCToModeA(int modeC)
{
    modeC += 13;
    if (modeC < 0 || modeC >= 4096)
        return 0;

    return modeCToATable[modeC];
}

static int internalModeAToModeC(unsigned int ModeA)
{
    unsigned int FiveHundreds = 0;
    unsigned int OneHundreds  = 0;

  if ((ModeA & 0xFFFF8889) != 0 ||         // check zero bits are zero, D1 set is illegal
      (ModeA & 0x000000F0) == 0) { // C1,,C4 cannot be Zero
      return INVALID_ALTITUDE;
  }

  if (ModeA & 0x0010) {OneHundreds ^= 0x007;} // C1
  if (ModeA & 0x0020) {OneHundreds ^= 0x003;} // C2
  if (ModeA & 0x0040) {OneHundreds ^= 0x001;} // C4

  // Remove 7s from OneHundreds (Make 7->5, snd 5->7).
  if ((OneHundreds & 5) == 5) {OneHundreds ^= 2;}

  // Check for invalid codes, only 1 to 5 are valid
  if (OneHundreds > 5) {
      return INVALID_ALTITUDE;
  }

//if (ModeA & 0x0001) {FiveHundreds ^= 0x1FF;} // D1 never used for altitude
  if (ModeA & 0x0002) {FiveHundreds ^= 0x0FF;} // D2
  if (ModeA & 0x0004) {FiveHundreds ^= 0x07F;} // D4

  if (ModeA & 0x1000) {FiveHundreds ^= 0x03F;} // A1
  if (ModeA & 0x2000) {FiveHundreds ^= 0x01F;} // A2
  if (ModeA & 0x4000) {FiveHundreds ^= 0x00F;} // A4

  if (ModeA & 0x0100) {FiveHundreds ^= 0x007;} // B1
  if (ModeA & 0x0200) {FiveHundreds ^= 0x003;} // B2
  if (ModeA & 0x0400) {FiveHundreds ^= 0x001;} // B4

  // Correct order of OneHundreds.
  if (FiveHundreds & 1) {OneHundreds = 6 - OneHundreds;}

  return ((FiveHundreds * 5) + OneHundreds - 13);
}
//
//=========================================================================
//
void decodeModeAMessage(struct modesMessage *mm, int ModeA)
{
    mm->source = SOURCE_MODE_AC;
    mm->addrtype = ADDR_MODE_A;
    mm->msgtype = 32; // Valid Mode S DF's are DF-00 to DF-31.
    // so use 32 to indicate Mode A/C

    mm->msgbits = 16; // Fudge up a Mode S style data stream
    mm->msg[0] = mm->verbatim[0] = (ModeA >> 8);
    mm->msg[1] = mm->verbatim[1] = (ModeA);

    // Fudge an address based on Mode A (remove the Ident bit)
    mm->addr = (ModeA & 0x0000FF7F) | MODES_NON_ICAO_ADDRESS;

    // Set the Identity field to ModeA
    mm->squawk   = ModeA & 0x7777;
    mm->squawk_valid = 1;

    // Flag ident in flight status
    mm->spi = (ModeA & 0x0080) ? 1 : 0;
    mm->spi_valid = 1;

    // Decode an altitude if this looks like a possible mode C
    if (!mm->spi) {
        int modeC = modeAToModeC(ModeA);
        if (modeC != INVALID_ALTITUDE) {
            mm->altitude_baro = modeC * 100;
            mm->altitude_baro_unit = UNIT_FEET;
            mm->altitude_baro_valid = 1;
        }
    }

    // Not much else we can tell from a Mode A/C reply.
    // Just fudge up a few bits to keep other code happy
    mm->correctedbits = 0;
}
//
// ===================== Mode A/C detection and decoding  ===================
//
