// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// dump1090.h: main program header
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

#ifndef __DUMP1090_H
#define __DUMP1090_H

// Default version number, if not overriden by the Makefile
#ifndef MODES_DUMP1090_VERSION
# define MODES_DUMP1090_VERSION     "v1.13-custom"
#endif

#ifndef MODES_DUMP1090_VARIANT
# define MODES_DUMP1090_VARIANT     "dump1090-paulyc"
#endif

// ============================= Include files ==========================

#ifndef _WIN32
    #include <stdio.h>
    #include <string.h>
    #include <stdlib.h>
    #include <stdbool.h>
    #include <pthread.h>
    #include <stdint.h>
    #include <errno.h>
    #include <unistd.h>
    #include <math.h>
    #include <sys/time.h>
    #include <signal.h>
    #include <fcntl.h>
    #include <ctype.h>
    #include <sys/stat.h>
    #include <sys/ioctl.h>
    #include <time.h>
    #include <limits.h>
#else
    #include "winstubs.h" //Put everything Windows specific in here
#endif

#include "compat/compat.h"

// ============================= #defines ===============================

#define HAVE_RTL_BIAST             1
#define MODES_DEFAULT_PPM          0
#define MODES_DEFAULT_FREQ         1090000000
#define MODES_DEFAULT_WIDTH        1000
#define MODES_DEFAULT_HEIGHT       700
#define MODES_RTL_BUFFERS          15                         // Number of RTL buffers
#define MODES_RTL_BUF_SIZE         (16*16384)                 // 256k
#define MODES_MAG_BUF_SAMPLES      (MODES_RTL_BUF_SIZE / 2)   // Each sample is 2 bytes
#define MODES_MAG_BUFFERS          12                         // Number of magnitude buffers (should be smaller than RTL_BUFFERS for flowcontrol to work)
#define MODES_AUTO_GAIN            -100                       // Use automatic gain
#define MODES_MAX_GAIN             999999                     // Use max available gain
#define MODES_MSG_SQUELCH_DB       4.0                        // Minimum SNR, in dB
#define MODES_MSG_ENCODER_ERRS     3                          // Maximum number of encoding errors

#define MODEAC_MSG_SAMPLES       (25 * 2)                     // include up to the SPI bit
#define MODEAC_MSG_BYTES          2
#define MODEAC_MSG_SQUELCH_LEVEL  0x07FF                      // Average signal strength limit

#define MODES_PREAMBLE_US        8              // microseconds = bits
#define MODES_PREAMBLE_SAMPLES  (MODES_PREAMBLE_US       * 2)
#define MODES_PREAMBLE_SIZE     (MODES_PREAMBLE_SAMPLES  * sizeof(uint16_t))
#define MODES_LONG_MSG_BYTES     14
#define MODES_SHORT_MSG_BYTES    7
#define MODES_LONG_MSG_BITS     (MODES_LONG_MSG_BYTES    * 8)
#define MODES_SHORT_MSG_BITS    (MODES_SHORT_MSG_BYTES   * 8)
#define MODES_LONG_MSG_SAMPLES  (MODES_LONG_MSG_BITS     * 2)
#define MODES_SHORT_MSG_SAMPLES (MODES_SHORT_MSG_BITS    * 2)
#define MODES_LONG_MSG_SIZE     (MODES_LONG_MSG_SAMPLES  * sizeof(uint16_t))
#define MODES_SHORT_MSG_SIZE    (MODES_SHORT_MSG_SAMPLES * sizeof(uint16_t))

#define MODES_OS_PREAMBLE_SAMPLES  (20)
#define MODES_OS_PREAMBLE_SIZE     (MODES_OS_PREAMBLE_SAMPLES  * sizeof(uint16_t))
#define MODES_OS_LONG_MSG_SAMPLES  (268)
#define MODES_OS_SHORT_MSG_SAMPLES (135)
#define MODES_OS_LONG_MSG_SIZE     (MODES_LONG_MSG_SAMPLES  * sizeof(uint16_t))
#define MODES_OS_SHORT_MSG_SIZE    (MODES_SHORT_MSG_SAMPLES * sizeof(uint16_t))

#define MODES_OUT_BUF_SIZE         (1500)
#define MODES_OUT_FLUSH_SIZE       (MODES_OUT_BUF_SIZE - 256)
#define MODES_OUT_FLUSH_INTERVAL   (60000)

#define MODES_USER_LATLON_VALID (1<<0)

#define INVALID_ALTITUDE (-9999)

/* Where did a bit of data arrive from? In order of increasing priority */
typedef enum {
    SOURCE_INVALID,        /* data is not valid */
    SOURCE_MODE_AC,        /* A/C message */
    SOURCE_MLAT,           /* derived from mlat */
    SOURCE_MODE_S,         /* data from a Mode S message, no full CRC */
    SOURCE_MODE_S_CHECKED, /* data from a Mode S message with full CRC */
    SOURCE_TISB,           /* data from a TIS-B extended squitter message */
    SOURCE_ADSB,           /* data from a ADS-B extended squitter message */
} datasource_t;

/* What sort of address is this and who sent it?
 * (Earlier values are higher priority)
 */
typedef enum {
    ADDR_ADSB_ICAO,       /* Mode S or ADS-B, ICAO address, transponder sourced */
    ADDR_ADSB_ICAO_NT,    /* ADS-B, ICAO address, non-transponder */
    ADDR_ADSR_ICAO,       /* ADS-R, ICAO address */
    ADDR_TISB_ICAO,       /* TIS-B, ICAO address */

    ADDR_ADSB_OTHER,      /* ADS-B, other address format */
    ADDR_ADSR_OTHER,      /* ADS-R, other address format */
    ADDR_TISB_TRACKFILE,  /* TIS-B, Mode A code + track file number */
    ADDR_TISB_OTHER,      /* TIS-B, other address format */

    ADDR_MODE_A,          /* Mode A */

    ADDR_UNKNOWN          /* unknown address format */
} addrtype_t;

typedef enum {
    UNIT_FEET,
    UNIT_METERS
} altitude_unit_t;

typedef enum {
    ALTITUDE_BARO,
    ALTITUDE_GNSS
} altitude_source_t;

typedef enum {
    AG_INVALID,
    AG_GROUND,
    AG_AIRBORNE,
    AG_UNCERTAIN
} airground_t;

typedef enum {
    SPEED_GROUNDSPEED,
    SPEED_IAS,
    SPEED_TAS
} speed_source_t;

typedef enum {
    HEADING_TRUE,
    HEADING_MAGNETIC
} heading_source_t;

typedef enum {
    SIL_PER_SAMPLE, SIL_PER_HOUR
} sil_type_t;

typedef enum {
    CPR_SURFACE, CPR_AIRBORNE, CPR_COARSE
} cpr_type_t;

#define MODES_NON_ICAO_ADDRESS       (1<<24) // Set on addresses to indicate they are not ICAO addresses

#define MODES_DEBUG_DEMOD (1<<0)
#define MODES_DEBUG_DEMODERR (1<<1)
#define MODES_DEBUG_BADCRC (1<<2)
#define MODES_DEBUG_GOODCRC (1<<3)
#define MODES_DEBUG_NOPREAMBLE (1<<4)
#define MODES_DEBUG_NET (1<<5)
#define MODES_DEBUG_JS (1<<6)

// When debug is set to MODES_DEBUG_NOPREAMBLE, the first sample must be
// at least greater than a given level for us to dump the signal.
#define MODES_DEBUG_NOPREAMBLE_LEVEL 25

#define MODES_INTERACTIVE_REFRESH_TIME 250      // Milliseconds
#define MODES_INTERACTIVE_DISPLAY_TTL 60000     // Delete from display after 60 seconds

#define MODES_NET_HEARTBEAT_INTERVAL 60000      // milliseconds

#define MODES_CLIENT_BUF_SIZE  1024
#define MODES_NET_SNDBUF_SIZE (1024*64)
#define MODES_NET_SNDBUF_MAX  (7)

#define HISTORY_SIZE 120
#define HISTORY_INTERVAL 30000

#define MODES_NOTUSED(V) ((void) V)

#define MAX_AMPLITUDE 65535.0
#define MAX_POWER (MAX_AMPLITUDE * MAX_AMPLITUDE)

// Include subheaders after all the #defines are in place

#include "util.h"
#include "anet.h"
#include "net_io.h"
#include "crc.h"
#include "demod_2400.h"
#include "stats.h"
#include "cpr.h"
#include "icao_filter.h"
#include "convert.h"
#include "sdr.h"

//======================== structure declarations =========================

typedef enum {
    SDR_NONE, SDR_IFILE, SDR_RTLSDR, SDR_BLADERF
} sdr_type_t;

// Structure representing one magnitude buffer
struct mag_buf {
    uint16_t       *data;            // Magnitude data. Starts with Modes.trailing_samples worth of overlap from the previous block
    unsigned        length;          // Number of valid samples _after_ overlap. Total buffer length is buf->length + Modes.trailing_samples.
    uint64_t        sampleTimestamp; // Clock timestamp of the start of this block, 12MHz clock
    struct timespec sysTimestamp;    // Estimated system time at start of block
    uint32_t        dropped;         // Number of dropped samples preceding this buffer
    double          mean_level;      // Mean of normalized (0..1) signal level
    double          mean_power;      // Mean of normalized (0..1) power level
};

// Program global state
struct {                             // Internal state
    pthread_t       reader_thread;

    pthread_mutex_t data_mutex;      // Mutex to synchronize buffer access
    pthread_cond_t  data_cond;       // Conditional variable associated

    struct mag_buf  mag_buffers[MODES_MAG_BUFFERS];       // Converted magnitude buffers from RTL or file input
    unsigned        first_free_buffer;                    // Entry in mag_buffers that will next be filled with input.
    unsigned        first_filled_buffer;                  // Entry in mag_buffers that has valid data and will be demodulated next. If equal to next_free_buffer, there is no unprocessed data.
    struct timespec reader_cpu_accumulator;               // CPU time used by the reader thread, copied out and reset by the main thread under the mutex

    unsigned        trailing_samples;                     // extra trailing samples in magnitude buffers
    double          sample_rate;                          // actual sample rate in use (in hz)

    uint16_t       *log10lut;        // Magnitude -> log10 lookup table
    int             exit;            // Exit from the main loop when true

    // Sample conversion
    int            dc_filter;        // should we apply a DC filter?

    // RTLSDR
    char *        dev_name;
    int           gain;
    int           freq;
    int           ppm_error;
#ifdef HAVE_RTL_BIAST
    int           enable_rtlsdr_biast;
#endif

    // Networking
    char           aneterr[ANET_ERR_LEN];
    struct net_service *services;    // Active services
    struct client *clients;          // Our clients

    struct net_writer raw_out;       // Raw output
    struct net_writer beast_out;     // Beast-format output
    struct net_writer sbs_out;       // SBS-format output
    struct net_writer fatsv_out;     // FATSV-format output

#ifdef _WIN32
    WSADATA        wsaData;          // Windows socket initialisation
#endif

    // Configuration
    sdr_type_t sdr_type;             // where are we getting data from?
    int   nfix_crc;                  // Number of crc bit error(s) to correct
    int   check_crc;                 // Only display messages with good CRC
    int   raw;                       // Raw output format
    int   mode_ac;                   // Enable decoding of SSR Modes A & C
    int   mode_ac_auto;              // allow toggling of A/C by Beast commands
    int   debug;                     // Debugging mode
    int   net;                       // Enable networking
    int   net_only;                  // Enable just networking
    uint64_t net_heartbeat_interval; // TCP heartbeat interval (milliseconds)
    int   net_output_flush_size;     // Minimum Size of output data
    uint64_t net_output_flush_interval; // Maximum interval (in milliseconds) between outputwrites
    char *net_output_raw_ports;      // List of raw output TCP ports
    char *net_input_raw_ports;       // List of raw input TCP ports
    char *net_output_sbs_ports;      // List of SBS output TCP ports
    char *net_input_beast_ports;     // List of Beast input TCP ports
    char *net_output_beast_ports;    // List of Beast output TCP ports
    char *net_bind_address;          // Bind address
    int   net_sndbuf_size;           // TCP output buffer size (64Kb * 2^n)
    int   net_verbatim;              // if true, send the original message, not the CRC-corrected one
    int   forward_mlat;              // allow forwarding of mlat messages to output ports
    int   quiet;                     // Suppress stdout
    uint32_t show_only;              // Only show messages from this ICAO
    int   interactive;               // Interactive mode
    uint64_t interactive_display_ttl;// Interactive mode: TTL display
    uint64_t stats;                  // Interval (millis) between stats dumps,
    int   stats_range_histo;         // Collect/show a range histogram?
    int   onlyaddr;                  // Print only ICAO addresses
    int   metric;                    // Use metric units
    int   use_gnss;                  // Use GNSS altitudes with H suffix ("HAE", though it isn't always) when available
    int   mlat;                      // Use Beast ascii format for raw data output, i.e. @...; iso *...;
    char *json_dir;                  // Path to json base directory, or NULL not to write json.
    uint64_t json_interval;          // Interval between rewriting the json aircraft file, in milliseconds; also the advertised map refresh interval
    int   json_location_accuracy;    // Accuracy of location metadata: 0=none, 1=approx, 2=exact

    int   json_aircraft_history_next;
    struct {
        char *content;
        int clen;
    } json_aircraft_history[HISTORY_SIZE];

    // User details
    double fUserLat;                // Users receiver/antenna lat/lon needed for initial surface location
    double fUserLon;                // Users receiver/antenna lat/lon needed for initial surface location
    int    bUserFlags;              // Flags relating to the user details
    double maxRange;                // Absolute maximum decoding range, in *metres*

    // State tracking
    struct aircraft *aircrafts;

    // Statistics
    struct stats stats_current;
    struct stats stats_alltime;
    struct stats stats_periodic;
    struct stats stats_1min[15];
    int stats_latest_1min;
    struct stats stats_5min;
    struct stats stats_15min;
} Modes;

// The struct we use to store information about a decoded message.
struct modesMessage {
    // Generic fields
    unsigned char msg[MODES_LONG_MSG_BYTES];      // Binary message.
    unsigned char verbatim[MODES_LONG_MSG_BYTES]; // Binary message, as originally received before correction
    int           msgbits;                        // Number of bits in message 
    int           msgtype;                        // Downlink format #
    uint32_t      crc;                            // Message CRC
    int           correctedbits;                  // No. of bits corrected 
    uint32_t      addr;                           // Address Announced
    addrtype_t    addrtype;                       // address format / source
    uint64_t      timestampMsg;                   // Timestamp of the message (12MHz clock)
    struct timespec sysTimestampMsg;              // Timestamp of the message (system time)
    int           remote;                         // If set this message is from a remote station
    double        signalLevel;                    // RSSI, in the range [0..1], as a fraction of full-scale power
    int           score;                          // Scoring from scoreModesMessage, if used

    datasource_t  source;                         // Characterizes the overall message source

    // Raw data, just extracted directly from the message
    // The names reflect the field names in Annex 4
    unsigned IID; // extracted from CRC of DF11s
    unsigned AA;
    unsigned AC;
    unsigned CA;
    unsigned CC;
    unsigned CF;
    unsigned DR;
    unsigned FS;
    unsigned ID;
    unsigned KE;
    unsigned ND;
    unsigned RI;
    unsigned SL;
    unsigned UM;
    unsigned VS;
    unsigned char MB[7];
    unsigned char MD[10];
    unsigned char ME[7];
    unsigned char MV[7];

    // Decoded data
    unsigned altitude_valid : 1;
    unsigned heading_valid : 1;
    unsigned speed_valid : 1;
    unsigned vert_rate_valid : 1;
    unsigned squawk_valid : 1;
    unsigned callsign_valid : 1;
    unsigned ew_velocity_valid : 1;
    unsigned ns_velocity_valid : 1;
    unsigned cpr_valid : 1;
    unsigned cpr_odd : 1;
    unsigned cpr_decoded : 1;
    unsigned cpr_relative : 1;
    unsigned category_valid : 1;
    unsigned gnss_delta_valid : 1;
    unsigned from_mlat : 1;
    unsigned from_tisb : 1;
    unsigned spi_valid : 1;
    unsigned spi : 1;
    unsigned alert_valid : 1;
    unsigned alert : 1;

    unsigned metype; // DF17/18 ME type
    unsigned mesub;  // DF17/18 ME subtype

    // valid if altitude_valid:
    int               altitude;         // Altitude in either feet or meters
    altitude_unit_t   altitude_unit;    // the unit used for altitude
    altitude_source_t altitude_source;  // whether the altitude is a barometric altude or a GNSS height
    // valid if gnss_delta_valid:
    int               gnss_delta;       // difference between GNSS and baro alt
    // valid if heading_valid:
    unsigned          heading;          // Reported by aircraft, or computed from from EW and NS velocity
    heading_source_t  heading_source;   // what "heading" is measuring (true or magnetic heading)
    // valid if speed_valid:
    unsigned          speed;            // in kts, reported by aircraft, or computed from from EW and NS velocity
    speed_source_t    speed_source;     // what "speed" is measuring (groundspeed / IAS / TAS)
    // valid if vert_rate_valid:
    int               vert_rate;        // vertical rate in feet/minute
    altitude_source_t vert_rate_source; // the altitude source used for vert_rate
    // valid if squawk_valid:
    unsigned          squawk;           // 13 bits identity (Squawk), encoded as 4 hex digits
    // valid if callsign_valid
    char              callsign[9];      // 8 chars flight number
    // valid if category_valid
    unsigned category;          // A0 - D7 encoded as a single hex byte
    // valid if cpr_valid
    cpr_type_t cpr_type;        // The encoding type used (surface, airborne, coarse TIS-B)
    unsigned cpr_lat;           // Non decoded latitude.
    unsigned cpr_lon;           // Non decoded longitude.
    unsigned cpr_nucp;          // NUCp/NIC value implied by message type

    airground_t airground;      // air/ground state

    // valid if cpr_decoded:
    double decoded_lat;
    double decoded_lon;

    // Operational Status
    struct {
        unsigned valid : 1;
        unsigned version : 3;

        unsigned om_acas_ra : 1;
        unsigned om_ident : 1;
        unsigned om_atc : 1;
        unsigned om_saf : 1;
        unsigned om_sda : 2;

        unsigned cc_acas : 1;
        unsigned cc_cdti : 1;
        unsigned cc_1090_in : 1;
        unsigned cc_arv : 1;
        unsigned cc_ts : 1;
        unsigned cc_tc : 2;
        unsigned cc_uat_in : 1;
        unsigned cc_poa : 1;
        unsigned cc_b2_low : 1;
        unsigned cc_nac_v : 3;
        unsigned cc_nic_supp_c : 1;
        unsigned cc_lw_valid : 1;

        unsigned nic_supp_a : 1;
        unsigned nac_p : 4;
        unsigned gva : 2;
        unsigned sil : 2;
        unsigned nic_baro : 1;

        sil_type_t sil_type;
        enum { ANGLE_HEADING, ANGLE_TRACK } track_angle;
        heading_source_t hrd;

        unsigned cc_lw;
        unsigned cc_antenna_offset;
    } opstatus;

    // Target State & Status (ADS-B V2 only)
    struct {
        unsigned valid : 1;
        unsigned altitude_valid : 1;
        unsigned baro_valid : 1;
        unsigned heading_valid : 1;
        unsigned mode_valid : 1;
        unsigned mode_autopilot : 1;
        unsigned mode_vnav : 1;
        unsigned mode_alt_hold : 1;
        unsigned mode_approach : 1;
        unsigned acas_operational : 1;
        unsigned nac_p : 4;
        unsigned nic_baro : 1;
        unsigned sil : 2;

        sil_type_t sil_type;
        enum { TSS_ALTITUDE_MCP, TSS_ALTITUDE_FMS } altitude_type;
        unsigned altitude;
        float baro;
        unsigned heading;
    } tss;
};

// This one needs modesMessage:
#include "track.h"

// ======================== function declarations =========================

#ifdef __cplusplus
extern "C" {
#endif

//
// Functions exported from mode_ac.c
//
int  detectModeA       (uint16_t *m, struct modesMessage *mm);
void decodeModeAMessage(struct modesMessage *mm, int ModeA);
void modeACInit();
int modeAToModeC (unsigned int modeA);
unsigned modeCToModeA (int modeC);

//
// Functions exported from mode_s.c
//
int modesMessageLenByType(int type);
int scoreModesMessage(unsigned char *msg, int validbits);
int decodeModesMessage (struct modesMessage *mm, unsigned char *msg);
void displayModesMessage(struct modesMessage *mm);
void useModesMessage    (struct modesMessage *mm);
//
// Functions exported from interactive.c
//
void  interactiveInit(void);
void  interactiveShowData(void);
void  interactiveCleanup(void);

void log_with_timestamp(const char *format, ...) __attribute__((format (printf, 1, 2) ));

// Provided by dump1090.c / view1090.c / faup1090.c
void dump1090ReceiverPositionChanged(float lat, float lon, float alt);
void faup1090ReceiverPositionChanged(float lat, float lon, float alt);
void view1090ReceiverPositionChanged(float lat, float lon, float alt);
void receiverPositionChanged(float lat, float lon, float alt);

void faupInitConfig(void);
void faupInit(void);
int faupMainLoop(char *bo_connect_ipaddr, int bo_connect_port);
void faupBackgroundTasks(void);

void view1090InitConfig(void);
void view1090Init(void);

void modesInitConfig(void);
void modesInit(void);
void *readerThreadEntryPoint(void *arg);
void snipMode(int level);
void display_total_stats(void);
void backgroundTasks(void);

#ifdef __cplusplus
}
#endif

#endif // __DUMP1090_H
