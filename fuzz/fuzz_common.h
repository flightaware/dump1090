// Shared setup for the libFuzzer harnesses in this directory.
//
// The harnesses #include net_io.c (via fuzz_common.c) so that they can reach
// its static parsers and writers; link them WITHOUT net_io.o.

#ifndef DUMP1090_FUZZ_COMMON_H
#define DUMP1090_FUZZ_COMMON_H

#include <stddef.h>
#include <stdint.h>

struct modesMessage;
struct client;

// One-time global setup (Modes defaults, CRC tables, output services with
// /dev/null clients so that every output formatter runs).
void fuzz_init(void);

// Per-input reset: drop all aircraft, reset the ICAO filter and faup state.
void fuzz_reset(void);

// Run every output generator (JSON files, FATSV, network flush) and abort()
// if any JSON produced is not syntactically valid.
void fuzz_run_outputs(void);

// Feed a decoded message into tracking + outputs (like useModesMessage)
void fuzz_use_message(struct modesMessage *mm);

// Validate that buf[0..len) is a single well-formed JSON value; abort() if not.
void fuzz_check_json(const char *what, const char *buf, int len);

// Fill in the CRC/parity field of a Mode S message so that the
// residual syndrome equals 'residual' (0 for a valid DF11/17/18 frame, an
// ICAO address for address/parity DFs).
void fuzz_fix_crc(unsigned char *msg, uint32_t residual);

// Access to net_io.c statics
struct net_service;
struct net_service *fuzz_make_raw_input_service(void);
void fuzz_read_from_client(struct client *c);
void fuzz_close_client(struct client *c);

// Base system time (ms) for synthetic message timestamps
extern uint64_t fuzz_now;

#endif
