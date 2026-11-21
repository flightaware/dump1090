// libFuzzer harness: sample conversion + 2.4MHz demodulator (Mode S and
// Mode A/C), feeding decoded messages through tracking and outputs.
//
// Models an attacker who controls the RF waveform. Two modes, chosen by
// byte 0 bit 0:
//
//   raw (bit0 = 1): bits 1-2 select the IQ sample format (UC8 / SC16 /
//       SC16Q11), the remaining bytes are IQ
//       samples run through the converter and then the demodulators.
//
//   synthesized (bit0 = 0): the remaining bytes are records describing
//       PPM-modulated Mode S frames that are rendered into a magnitude
//       buffer at 24MHz and box-filtered down to 2.4MHz:
//         2 bytes  start position (sub-sample units, 1/10 sample)
//         1 byte   amplitude
//         1 byte   noise level
//         1 byte   flags: bit0 = fix CRC (residual 0), bit1 = truncate
//                  after 56 bits, bits 2-7 = pulse-width jitter
//         14 bytes message
//       Byte 0 bits 1-7 set a global noise floor.

#include "fuzz_common.h"
#include "../dump1090.h"

#define FUZZ_MLEN 8192

static uint16_t *magdata;
static unsigned overlap;
static iq_convert_fn converters[3][2];
static struct converter_state *converter_states[3][2];

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void) argc; (void) argv;
    fuzz_init();
    overlap = Modes.trailing_samples;
    magdata = calloc(FUZZ_MLEN + overlap, sizeof(uint16_t));
    for (int f = 0; f < 3; ++f) {
        converters[f][0] = init_converter((input_format_t) f, Modes.sample_rate, 0, &converter_states[f][0]);
        if (!converters[f][0])
            abort();
    }
    return 0;
}

static void render(uint8_t *hi, unsigned nsub, unsigned start, unsigned width_jitter, const unsigned char *msg, int bits)
{
    // 24 sub-samples per microsecond; pulses are 12 sub-samples (0.5us)
    unsigned pw = 12 + (width_jitter % 5) - 2;
    static const unsigned preamble_us2[4] = { 0, 2, 7, 9 };   // in half-microseconds
    for (int p = 0; p < 4; ++p) {
        unsigned s = start + preamble_us2[p] * 12;
        for (unsigned k = 0; k < pw && s + k < nsub; ++k) hi[s + k] = 1;
    }
    for (int i = 0; i < bits; ++i) {
        int bit = (msg[i / 8] >> (7 - (i % 8))) & 1;
        unsigned s = start + (8 + i) * 24 + (bit ? 0 : 12);
        for (unsigned k = 0; k < pw && s + k < nsub; ++k) hi[s + k] = 1;
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1)
        return 0;

    fuzz_reset();

    struct mag_buf mag;
    memset(&mag, 0, sizeof(mag));
    mag.data = magdata;
    mag.totalLength = FUZZ_MLEN + overlap;
    mag.validLength = FUZZ_MLEN + overlap;
    mag.overlap = overlap;
    mag.sysTimestamp = fuzz_now;
    mag.sampleTimestamp = fuzz_now * 12000;

    uint8_t mode = data[0];
    data++; size--;

    if (mode & 1) {
        int fmt = (mode >> 1) & 3;
        if (fmt == 3) fmt = 0;
        int dc = 0;   // DC filtering is not supported by convert.c
        unsigned bytes_per_sample = (fmt == INPUT_UC8) ? 2 : 4;
        unsigned nsamples = size / bytes_per_sample;
        if (nsamples > FUZZ_MLEN + overlap)
            nsamples = FUZZ_MLEN + overlap;
        memset(magdata, 0, (FUZZ_MLEN + overlap) * sizeof(uint16_t));
        if (nsamples > 0) {
            void *iq = malloc(nsamples * bytes_per_sample);
            memcpy(iq, data, nsamples * bytes_per_sample);
            converters[fmt][dc](iq, magdata, nsamples, converter_states[fmt][dc], &mag.mean_level, &mag.mean_power);
            free(iq);
        }
    } else {
        unsigned nsub = (FUZZ_MLEN + overlap) * 10;
        uint8_t *hi = calloc(nsub, 1);
        uint16_t *amp = calloc(nsub, sizeof(uint16_t));
        unsigned floor_level = (mode >> 1) * 64;

        while (size >= 19) {
            unsigned start = ((unsigned) data[0] << 8 | data[1]) * 2;   // up to ~13000 samples
            unsigned a = 256 + data[2] * 250u;
            unsigned noise = data[3] * 16u;
            uint8_t flags = data[4];
            unsigned char msg[MODES_LONG_MSG_BYTES];
            memcpy(msg, data + 5, sizeof(msg));
            data += 19; size -= 19;

            if (flags & 1)
                fuzz_fix_crc(msg, 0);

            memset(hi, 0, nsub);
            int bits = (flags & 2) ? 56 : modesMessageLenByType(msg[0] >> 3);
            render(hi, nsub, start, flags >> 2, msg, bits);
            for (unsigned k = 0; k < nsub; ++k) {
                if (hi[k]) {
                    unsigned v = amp[k] + a;
                    amp[k] = v > 65535 ? 65535 : v;
                }
            }
            (void) noise;
            if (noise) {
                // deterministic pseudo-noise
                uint32_t x = 0x12345678u ^ noise;
                for (unsigned k = 0; k < nsub; k += 10) {
                    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
                    unsigned v = amp[k] + (x % (noise + 1));
                    amp[k] = v > 65535 ? 65535 : v;
                }
            }
        }

        for (unsigned s = 0; s < FUZZ_MLEN + overlap; ++s) {
            uint32_t sum = 0;
            for (unsigned k = 0; k < 10; ++k)
                sum += amp[s * 10 + k];
            uint32_t v = floor_level + sum / 10;
            magdata[s] = v > 65535 ? 65535 : v;
        }
        free(hi);
        free(amp);
    }

    demodulate2400(&mag);
    demodulate2400AC(&mag);

    fuzz_run_outputs();
    return 0;
}
