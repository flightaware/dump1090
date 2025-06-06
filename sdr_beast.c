// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// sdr_beast.c: reads raw binary BEAST from the file
//
// Copyright (c) 2014-2017 Oliver Jowett <oliver@mutability.co.uk>
// Copyright (c) 2017 FlightAware LLC
// Copyright (c) 2019 Denis G Dugushkin <denis.dugushkin@gmail.com>
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
#include "sdr_beast.h"

#define BEAST_DROP_UPPER_34_BITS 0x000000003FFFFFFF
#define READ_BUFFER_SIZE 4096
#define BEAST_MSG_SIZE 128

typedef enum {
	MLAT_NONE, MLAT_BEAST, MLAT_DUMP1090
} mlat_time_t;

typedef void (*mlatprocessor_t)(struct modesMessage *mm);

static struct {
	char *fnameIn;                  // Input filename
	char *fnameOut;                 // Output filename, for --extract option
	int fdIn;						// File descriptor for input file
	int fdOut;						// File descriptor for output file

	char *desc; 					// User report description

	// Time related stuff
	mlat_time_t mlat_decoder;		// Type of MLAT processor
	mlatprocessor_t MLATtimefunc; 	// The processor function for time calculations in specified manner (none, beast, dump1090)
	unsigned long long firsttimestampMsg;	  // Timestamp of the first message (12MHz clock)
	unsigned long long previoustimestampMsg;  // Timestamp of the last processed message (12MHz clock)
	time_t initTime;                // Base time (UNIX format) to calculate relative time for messages using MLAT timestamps

	// Other options stuff
	bool isFindICAO;			   	// *NOT REALIZED* Find only ICAO
	unsigned long long limitMsgCount;         // Max output messages
	bool isExtract;					// Extract to file
	bool isSBSoutput;				// Output in SBS-format

	// Counters
	unsigned long long msgProc;		// Messages processed
	unsigned long long msgExtr;		// Messages extracted
} beastfile;

int time_offset();
void MLATtime_none(struct modesMessage *mm);
void MLATtime_beast(struct modesMessage *mm);
void MLATtime_dump1090(struct modesMessage *mm);
char *mlattostr(mlat_time_t a);
void processModesMessage(struct modesMessage *mm);

// Function int time_offset() has been grabbed from stackoverflow.com
// https://stackoverflow.com/questions/13804095/get-the-time-zone-gmt-offset-in-c
// (c) Author: Hill
int time_offset() {

	time_t gmt, rawtime = time(NULL);
	struct tm *ptm;

#if !defined(WIN32)
	struct tm gbuf;
	ptm = gmtime_r(&rawtime, &gbuf);
#else
	ptm = gmtime(&rawtime);
#endif
	// Request that mktime() looksup dst in timezone database
	ptm->tm_isdst = -1;
	gmt = mktime(ptm);

	return (int) difftime(rawtime, gmt);
}

void MLATtime_none(struct modesMessage *mm) {

	// Expect that messages comes every 200ms to prevent fail on speed and CPR validation
	beastfile.previoustimestampMsg = mm->timestampMsg;
	mm->sysTimestampMsg = beastfile.initTime;
	beastfile.initTime += 200;
}

/*	The GPS timestamp is completely handled in the FPGA (hardware) and does not require any interactions on the Linux side.
 This is essential to meet the required accuracy. The local clock in the FPGA (64MHz or 96MHz) is stretched or compressed
 to meet 1e9 counts in between two pulses by a linear algorithm, in order to avoid bigger jumps in the timestamp.
 Rollover from 999999999 to 0 occurs synchronously to the 1PPS leading edge. In parallel, the Second-of-Day information is read
 from the TSIP serial data stream and also aligned to the 1PPS pulse. Both parts are then mapped into the 48bits that are available
 for the timestamp and transmitted with each Mode-S or Mode-A/C message.
 SecondsOfDay are using the upper 18 bits of the timestamp
 Nanoseconds are using the lower 30 bits. The value there directly converts into a 1ns based value and does not need to be converted by sample rate
 */
void MLATtime_beast(struct modesMessage *mm) {

	beastfile.previoustimestampMsg =
			mm->timestampMsg & BEAST_DROP_UPPER_34_BITS;

	// Code below has some dirty hack. We use variable 'beastfile.previoustimestampMsg' instead of clear code 'mm->timestampMsg & BEAST_DROP_UPPER_34_BITS'
	// to optimize compiler's code generation. I don't know any compiler that can populate calculation result of the expression through the same fragments of function code.
	// This dirty code prevents multiple calculations of the same expession.
	mm->sysTimestampMsg = beastfile.initTime + (mm->timestampMsg >> 30) * 1000
			+ (beastfile.previoustimestampMsg / 1000000);

	// Use MLAT timestamp to emulate 12MHz clock, to make other code happy. dump1090 doesn't handle classical BEAST's MLAT timestamp
	mm->timestampMsg = beastfile.previoustimestampMsg * 12 / 1000;

}

void MLATtime_dump1090(struct modesMessage *mm) {

	beastfile.previoustimestampMsg = mm->timestampMsg;
	// Convert 12MHz clock to milliseconds  
	mm->sysTimestampMsg = beastfile.initTime
			+ (mm->timestampMsg - beastfile.firsttimestampMsg) / (12 * 1000);
}

// Safe copy BEAST message from a bytes stream
// On success function returns number of copied bytes
// If no message found it returns 0
// If length of buffer less than minimum message lingth it returns -1
static int copyBinMessageSafe(unsigned char *p, int limit, unsigned char *out) {

	int msgLen = 0;
	int j = 2;
	char ch;

	// Check message length to minimum possible length
	if (limit < 11)
		return -1;

	// Check first byte for a marker <esc>
	// If ok, write it to output buffer
	// otherwise exit
	ch = *p;
	if (0x1A == ch) {
		p++;
		*out = 0x1A;
		out++;
	} else
		return 0;

	// Check for message type
	ch = *p;

	switch (ch) {
	case '1':
		msgLen = MODEAC_MSG_BYTES;
		*out = '1';
		out++;
		break;
	case '2':
		msgLen = MODES_SHORT_MSG_BYTES;
		*out = '2';
		out++;
		break;
	case '3':
		msgLen = MODES_LONG_MSG_BYTES;
		*out = '3';
		out++;
		break;
	case '5':
		// TODO: debug this case
		msgLen = 20 - 6 - 1; // Dirty emulation
		*out = '5';
		out++;
		break;
	default:
		return 0;
	}
	// Copy message body
	if (msgLen) {
		msgLen += 2 + 6 + 1; //  2 bytes header, 6 bytes timestamp, 1 byte signal level

		// Second check for length because <esc> markers can be doubled
		// and it means that input buffer contains truncated message
		if (msgLen > limit)
			return -1;
		p++;
		while ((j < msgLen)) {
			if (msgLen > limit)
				return -2;
			*out = ch = *p++;
			j++;
			out++;
			if (0x1A == ch) {
				msgLen++;
				*out = ch = *p++;
				out++;
				j++;
			}
		}
	}
	if (msgLen > limit)
		return -3;
	out -= msgLen;
	return msgLen;
}

static int processBeastMessage(unsigned char *beastMsg,
		struct modesMessage *mm) {

	int msgLen = 0;
	float lat, lon, alt;
	int j;
	unsigned char msg[MODES_LONG_MSG_BYTES + 7];
	unsigned char ch;

	ch = *beastMsg++; /// Get the message type
	if (0x1A == ch)
		ch = *beastMsg++;

	switch (ch) {
	case '1':
		if (Modes.mode_ac)
			msgLen = MODEAC_MSG_BYTES;
		else
			return 0;
		break;
	case '2':
		msgLen = MODES_SHORT_MSG_BYTES;
		break;
	case '3':
		msgLen = MODES_LONG_MSG_BYTES;
		break;
	case '5':
		// Special case for Radarcape position messages.    	
		for (j = 0; j < 21; j++) { // and the data
			msg[j] = ch = *beastMsg++;
			if (0x1A == ch) {
				beastMsg++;
			}
		}
		lat = ieee754_binary32_le_to_float(msg + 4);
		lon = ieee754_binary32_le_to_float(msg + 8);
		alt = ieee754_binary32_le_to_float(msg + 12);
		handle_radarcape_position(lat, lon, alt);
        break;
	default:
		return 0;
	}
	// Set initial mm structure details
	memset(mm, 0, sizeof(struct modesMessage)); // Is it really need?

	// Grab the timestamp (big endian format)
	for (j = 0; j < 6; j++) {
		ch = *beastMsg++;
		mm->timestampMsg = mm->timestampMsg << 8 | (ch & 255);
		if (0x1A == ch)
			beastMsg++;
	}

	// Calculate realtime timestamp using specified processor
	beastfile.MLATtimefunc(mm);

	// Grab the signal level
	ch = *beastMsg++;
	mm->signalLevel = (ch / 255.0);
	mm->signalLevel = mm->signalLevel * mm->signalLevel;

	if (0x1A == ch)
		beastMsg++;

	// Grab the message data
	for (j = 0; j < msgLen; j++) {
		msg[j] = ch = *beastMsg++;
		if (0x1A == ch)
			beastMsg++;
	}

	mm->score = scoreModesMessage(msg);

	// Decode the received message
	{
		int result = decodeModesMessage(mm, msg);

		if (result < 0) {
			if (result == -1)
				Modes.stats_current.demod_rejected_unknown_icao++;
			else {
				Modes.stats_current.demod_rejected_bad++;
				return 0;
			}
		} else {
			Modes.stats_current.demod_accepted[mm->correctedbits]++;
		}
	}
	return msgLen;
}

// Now we only read first beast message and get it MLAT timestamp to relative
// time calculations.
static void initMLATtime_dump(unsigned char *p, ssize_t size) {

	unsigned char beastmessage[MODES_LONG_MSG_BYTES];
	ssize_t i, j, k = 0;

	while (k < size) {
		i = copyBinMessageSafe(p, size - k, &beastmessage[0]);
		if (i > 0) {
			beastfile.firsttimestampMsg = 0;
			p = &beastmessage[2];

			for (j = 0; j < 6; j++) {
				beastfile.firsttimestampMsg = beastfile.firsttimestampMsg << 8
						| (*p & 255);
				if (0x1A == *p)
					p++;
				p++;
			}
			if (beastfile.firsttimestampMsg) {
				beastfile.previoustimestampMsg = beastfile.firsttimestampMsg;
				break;
			} else {
				p++;
				k++;
			}

		} else {
			p++;
			k++;
		}
	}
}

void beastInitConfig(void) {
	memset(&beastfile, 0, sizeof(beastfile));
	beastfile.mlat_decoder = MLAT_DUMP1090;
	beastfile.MLATtimefunc = &MLATtime_dump1090;
	beastfile.fdIn = -1;
	beastfile.fdOut = -1;
}

void beastShowHelp() {
	printf("      beastfile-specific options (use with --beastfile)\n");
	printf("\n");
	printf(
			"--beastfile <path>       Source file to proceed\n"
					"--extract <path>         Extract BEAST data to new file (if no ICAO filter specified it just copies the source)\n"
					"--filter-icao <addr>     Extract only messages from the given ICAO\n"
					"--mlat-time <type>       Decode MLAT timestamps in specified manner. Types are: dump1090 (default), beast, none\n"
					"--init-time-unix <sec>   Start time (UNIX epoch, format: ss.ms) to calculate realtime using MLAT timestamps\n"
					"--limit-count <count>    Limit messages count from the start of the file (default: all)\n"
					"--sbs-output             Show messages in SBS format (default: dump1090 style)\n"
					"--desc <text>            Text description of the report\n");
	printf("\n");
}

bool beastHandleOption(int argc, char **argv, int *jptr) {

	int j = *jptr;

	bool more = (j + 1 < argc);

	if (!strcmp(argv[j], "--beastfile") && more) {
		// implies --device-type ifile
		beastfile.fnameIn = strdup(argv[++j]);
		Modes.sdr_type = SDR_BEASTFILE;
	} else if (!strcmp(argv[j], "--extract") && more) {
		beastfile.fnameOut = strdup(argv[++j]);
		beastfile.isExtract = true;
	} else if (!strcmp(argv[j], "--desc") && more) {
		beastfile.desc = strdup(argv[++j]);
	} else if (!strcmp(argv[j], "--find-icaos-only")) {
		beastfile.isFindICAO = true;
	} else if (!strcmp(argv[j], "--sbs-output")) {
		beastfile.isSBSoutput = true;
		Modes.quiet = 1;
	} else if (!strcmp(argv[j], "--init-time-unix") && more) {
		double t;
		beastfile.initTime = (time_t)(1000 * modf(atof(argv[++j]), &t));
		beastfile.initTime += (time_t) t * 1000;
	} else if (!strcmp(argv[j], "--mlat-time") && more) {
		++j;
		if (!strcmp(argv[j], "beast")) {
			beastfile.mlat_decoder = MLAT_BEAST;
			beastfile.MLATtimefunc = &MLATtime_beast;
		} else if (!strcmp(argv[j], "dump1090")) {
			beastfile.mlat_decoder = MLAT_DUMP1090;
			beastfile.MLATtimefunc = &MLATtime_dump1090;
		} else if (!strcmp(argv[j], "none")) {
			beastfile.mlat_decoder = MLAT_NONE;
			beastfile.MLATtimefunc = &MLATtime_none;
		} else {
			fprintf(stderr,
					"Unknown argument for option --mlat-time: '%s'.\n\n",
					argv[j]);
			return false;
		}
	} else if (!strcmp(argv[j], "--filter-icao") && more) {
		Modes.show_only = (uint32_t) strtoul(argv[++j], NULL, 16);
	} else if (!strcmp(argv[j], "--limit-count") && more) {
		beastfile.limitMsgCount = strtoul(argv[++j], NULL, 10);
	} else {
		return false;
	}

	*jptr = j;
	return true;
}

char *mlattostr(mlat_time_t a) {

	switch (a) {
	case MLAT_NONE:
		return "not specified";
	case MLAT_BEAST:
		return "beast";
	case MLAT_DUMP1090:
		return "dump1090";
	}
	return "";
}

//
//=========================================================================
//
// This is used when --beastfile is specified in order to read data from file
// instead of using an RTLSDR device
//
bool beastOpen(void) {

	beastfile.fdIn = open(beastfile.fnameIn, O_RDONLY);
	if (beastfile.fdIn == -1) {
		fprintf(stderr, "Error. Unable to open for read BEAST file %s\n",
				beastfile.fnameIn);
		return false;
	}

	if (beastfile.isExtract) {
		beastfile.fdOut = open(beastfile.fnameOut, O_WRONLY | O_CREAT | O_TRUNC,
				0644);
		if (beastfile.fdOut == -1) {
			fprintf(stderr, "Error. Unable to open for write BEAST file %s\n",
					beastfile.fnameOut);
			return false;
		}
	}

	// When used native BEAST time calculation (like FR24 box), 
	// we need only 'date'. 'Time' should be set at 00:00:00
	// because native BEAST saves time as HH:MM:SS
	// Yep, it's a dirty hack. Make it better if possible
	struct tm stTime_init;
	if ((beastfile.mlat_decoder == MLAT_BEAST) && beastfile.initTime) {
		gmtime_r(&beastfile.initTime, &stTime_init);
		stTime_init.tm_hour = 0;
		stTime_init.tm_min = 0;
		stTime_init.tm_sec = 0;
		beastfile.initTime = mktime(&stTime_init) + time_offset();
	}
	printf(
			"-----------------------------------------------------------------------------\r\n");
	printf("| dump1090 ModeS Receiver     %45s |\r\n", MODES_DUMP1090_VARIANT " " MODES_DUMP1090_VERSION);
	printf(
			"-----------------------------------------------------------------------------\r\n");
	printf("\n\r");
	printf(
			"This report is generated by built-in BEAST-format file reader\r\n\r\n"
					"Used config:\r\n"
					"Description............%s\r\n"
					"Location...............%.3f, %.3f\r\n"
					"Input filename.........%s\r\n"
					"Initial unix time......%ld\r\n"
					"MLAT timestamps proc...%s\r\n"
					"Limit messages count...%llu\r\n"
					"Text output type.......%s\r\n", beastfile.desc,
			Modes.fUserLat, Modes.fUserLon, beastfile.fnameIn,
			beastfile.initTime / 1000, mlattostr(beastfile.mlat_decoder),
			beastfile.limitMsgCount,
			beastfile.isSBSoutput ? "SBS" : "dump1090");
	if (Modes.show_only)
		printf("Filter by ICAO.........0x%06X\n\r", Modes.show_only);
	printf("=============================================================================");	      
	printf("\n\r");

	return true;
}

static void beastWriteOut(unsigned char *beast, ssize_t len,
		struct modesMessage *mm) {

	// Does it need to be written?
	if (!beastfile.isExtract)
		return;

	if (Modes.show_only && (mm->addr != Modes.show_only))
		return;
	if (beastfile.fdOut != -1)
		return;

	ssize_t wBytes = write(beastfile.fdIn, beast, len);
	if (wBytes != len)
		fprintf(stderr, "Error. Unable to write into file %s\n",
				beastfile.fnameOut);
}

void processModesMessage(struct modesMessage *mm) {

	++Modes.stats_current.messages_total;
	if (Modes.show_only && (mm->addr != Modes.show_only))
		return;

	beastfile.msgExtr++;
	// Track aircraft state    
	struct aircraft *a;
	a = trackUpdateFromMessage(mm);

	// In non-interactive non-quiet mode, display messages on standard output
	if (!Modes.interactive && !beastfile.isSBSoutput) {

		// Find message reception time and  print it
		struct tm stTime_receive;
	    time_t received = (time_t) (mm->sysTimestampMsg / 1000);
	    localtime_r(&received, &stTime_receive);
	    if(beastfile.mlat_decoder != MLAT_NONE) {
	    	printf("Timestamp: ");
	    	printf("%04d/%02d/%02d @ ", (stTime_receive.tm_year+1900),(stTime_receive.tm_mon+1), stTime_receive.tm_mday);
	    	printf("%02d:%02d:%02d.%03u", stTime_receive.tm_hour, stTime_receive.tm_min, stTime_receive.tm_sec, (unsigned) (mm->sysTimestampMsg % 1000));
	    	printf("\r\n");
	    }	    
		displayModesMessage(mm);
	} else {
		char sbs[200];
		modesPrepareSBSOutput(mm, a, &sbs[0]);
		printf("%s", sbs);
	}
}

void beastRun() {

	static struct modesMessage zeroMessage;
	struct modesMessage mm;
	mm = zeroMessage;

	ssize_t i, k, ret_in, seek;
	unsigned char buffer[READ_BUFFER_SIZE];
	unsigned char beastMsg[BEAST_MSG_SIZE];

	ret_in = read(beastfile.fdIn, &buffer[0], READ_BUFFER_SIZE);

	if (beastfile.mlat_decoder == MLAT_DUMP1090) {
		initMLATtime_dump(&buffer[0], ret_in);
	}

	seek = 0;

	// Don't try to understand it.
	// It just works. Thanks to holy power.	
	while ((ret_in > 0) && (!Modes.exit)) {
		k = 0;

		while (k < (ret_in + seek)) {
			i = copyBinMessageSafe(&buffer[k], ret_in + seek - k, &beastMsg[0]);
			if (i > 0) {
				if (beastfile.limitMsgCount
						&& (beastfile.msgExtr >= beastfile.limitMsgCount))
					break;

				beastfile.msgProc++;

				if (processBeastMessage(&beastMsg[0], &mm)) {
					beastWriteOut(&beastMsg[0], i, &mm);
					processModesMessage(&mm);
				}
				k += i;
			} else if (i == 0) {
				if ((ret_in + seek - k) == 1) {
					break;
				} else
					k++;
			} else
				break;
		}
		seek = READ_BUFFER_SIZE - k;
		if (seek > 0)
			memcpy(&buffer[0], &buffer[k], seek);
		ret_in = read(beastfile.fdIn, &buffer[seek], k);


	}
}

void beastClose() {
	printf(
			"============================================================================="
			"\r\n"
			"Messages processed.....%llu\r\n"
			"Messages extracted.....%llu\r\n",
			beastfile.msgProc, beastfile.msgExtr);

	if(Modes.stats_current.demod_rejected_bad) printf("WARNING! Rejected %u messages with bad CRC\r\n", Modes.stats_current.demod_rejected_bad);
	if(Modes.stats_current.demod_rejected_unknown_icao) printf("WARNING! Found %u messages that might be valid, but we couldn't validate the CRC against a known ICAO\r\n", Modes.stats_current.demod_rejected_unknown_icao);
	printf("\r\nEND OF FILE\r\n");
	// Close all files
	close(beastfile.fdIn);
	if (beastfile.isExtract)
		close(beastfile.fdOut);
    Modes.exit = 1;
}
