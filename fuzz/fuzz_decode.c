// libFuzzer harness: Mode S / Mode A/C frame decoding, aircraft tracking,
// CPR, Comm-B inference and every output formatter (JSON, SBS, Beast, raw,
// Stratux, FATSV).
//
// This models the over-the-air attacker: the input is a sequence of frames
// and the harness can fix up the CRC so that frames survive the CRC check
// (a transmitter can always compute a valid CRC).
//
// Record format (repeated until input is exhausted):
//   byte 0: bits 0-1  kind: 0 = Mode S, CRC fixed so residual == 0
//                           1 = Mode S, CRC fixed so residual == next 3 bytes
//                               (address/parity DFs: residual is the address)
//                           2 = Mode S, sent as-is (exercises error correction)
//                           3 = Mode A/C (2 bytes)
//           bits 2-7  time advance in units of 250ms
//   byte 1: signal level
//   kind 1 only: 3 bytes residual
//   then 14 bytes of message (Mode S) or 2 bytes (Mode A/C)
//
// Outputs (JSON with a syntax check, FATSV, network writers) run every 16
// records and at the end.

#include "fuzz_common.h"
#include "../dump1090.h"

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void) argc; (void) argv;
    fuzz_init();
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    fuzz_reset();
    uint64_t now = fuzz_now;
    unsigned records = 0;

    while (size >= 2) {
        uint8_t hdr = data[0];
        uint8_t sig = data[1];
        int kind = hdr & 3;
        now += (uint64_t)(hdr >> 2) * 250;
        data += 2; size -= 2;

        uint32_t residual = 0;
        if (kind == 1) {
            if (size < 3) break;
            residual = ((uint32_t) data[0] << 16) | ((uint32_t) data[1] << 8) | data[2];
            data += 3; size -= 3;
        }

        struct modesMessage mm;
        memset(&mm, 0, sizeof(mm));
        mm.sysTimestampMsg = now;
        mm.timestampMsg = now * 12000;
        mm.signalLevel = (sig / 255.0) * (sig / 255.0);

        if (kind == 3) {
            if (size < 2) break;
            decodeModeAMessage(&mm, (data[0] << 8) | data[1]);
            data += 2; size -= 2;
            fuzz_use_message(&mm);
        } else {
            unsigned char msg[MODES_LONG_MSG_BYTES];
            if (size < sizeof(msg)) break;
            memcpy(msg, data, sizeof(msg));
            data += sizeof(msg); size -= sizeof(msg);

            if (kind != 2)
                fuzz_fix_crc(msg, residual);

            // the "remote" flag changes some processing paths; vary it
            mm.remote = (sig & 1);
            if (decodeModesMessage(&mm, msg) >= 0)
                fuzz_use_message(&mm);
        }

        if ((++records & 15) == 0)
            fuzz_run_outputs();
    }

    fuzz_run_outputs();
    return 0;
}
