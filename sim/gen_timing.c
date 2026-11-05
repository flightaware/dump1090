// Test signal for DEMOD_2400X: DF17 frames at known 12 MHz ticks, after gen_data.c.
//
// Frame k starts at 12 MHz tick (k + 1) * SPACING. Chips are rectangular on the
// 12 MHz grid (6 ticks per 0.5 us chip) and are resampled to 2.4 MHz with liquid-dsp's
// msresamp, so SPACING % 5 == 1 steps through the 5 sub-sample phases. Frame k
// carries k in its payload so tscheck.py can match reported timestamps to it.
//
// With max_offset_hz each frame gets a random carrier offset (+-max) and phase;
// without it frames sit exactly at 0 Hz, where the DEMOD_2400X DC filter removes
// part of the signal, so use an offset for sensitivity tests.
//
// Build: gcc -O2 -o gen_timing gen_timing.c -lliquid -lm
// Usage: gen_timing out.sc16 nframes noise [max_offset_hz]
//        noise is the per-channel Gaussian sigma in 12-bit units (frame amplitude 1000)
// Output is SC16 (12-bit values << 4): dump1090 --ifile out.sc16 --iformat SC16

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <liquid/liquid.h>

#define SPACING 3601        // 12 MHz ticks between frame starts (300 us)
#define GAIN 1000.0f        // frame amplitude, 12-bit units

// Mode S CRC (generator 0xfff409) over the first bits - 24 bits, bit by bit
static uint32_t modes_crc(const uint8_t *msg, int bits)
{
    uint32_t rem = 0;
    for (int i = 0; i < bits - 24; i++) {
        uint32_t in = (msg[i >> 3] >> (7 - (i & 7))) & 1;
        uint32_t top = ((rem >> 23) & 1) ^ in;
        rem = (rem << 1) & 0xffffff;
        if (top)
            rem ^= 0xfff409;
    }
    return rem;
}

static float gauss(void)
{
    float u1 = drand48() + 1e-12f, u2 = drand48();
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * M_PI * u2);
}

static void make_df17(uint8_t msg[14], unsigned k)
{
    memset(msg, 0, 14);
    msg[0] = (17 << 3) | 5;
    msg[1] = 0x40; msg[2] = 0x68; msg[3] = 0x21;
    msg[4] = (11 << 3);
    msg[5] = 0x12; msg[6] = 0x34; msg[7] = 0x56; msg[8] = 0x78;
    msg[9] = k >> 8; msg[10] = k & 0xff;      // frame number
    uint32_t crc = modes_crc(msg, 112);
    msg[11] = crc >> 16; msg[12] = crc >> 8; msg[13] = crc;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: %s out.sc16 nframes noise [max_offset_hz]\n", argv[0]);
        return 1;
    }
    unsigned nframes = atoi(argv[2]);
    float noise = atof(argv[3]);
    float max_fo = argc > 4 ? atof(argv[4]) : 0;
    size_t n12 = (size_t) (nframes + 2) * SPACING;
    float *u = calloc(n12, sizeof(float));
    float *o = calloc(n12 / 5 + 1024, sizeof(float));
    uint8_t msg[14];

    for (unsigned k = 0; k < nframes; k++) {
        float *p = u + (size_t) (k + 1) * SPACING;
        int chips[16 + 224] = {0};
        chips[0] = chips[2] = chips[7] = chips[9] = 1;     // preamble
        make_df17(msg, k);
        for (int bit = 0; bit < 112; bit++)
            chips[16 + 2 * bit + !((msg[bit >> 3] >> (7 - (bit & 7))) & 1)] = 1;
        for (int c = 0; c < 240; c++)
            for (int j = 0; j < 6; j++)
                p[6 * c + j] = chips[c];
    }

    msresamp_rrrf r = msresamp_rrrf_create(.2f, 40.0f);
    unsigned ny = 0;
    msresamp_rrrf_execute(r, u, n12, o, &ny);

    FILE *f = fopen(argv[1], "wb");
    if (!f) {
        perror(argv[1]);
        return 1;
    }
    double ph = 0, w = 0;
    for (unsigned i = 0; i < ny; i++) {
        if (max_fo > 0 && i % (SPACING / 5) == 0) {     // new carrier per frame slot
            ph = 2 * M_PI * drand48();
            w = 2 * M_PI * max_fo * (2 * drand48() - 1) / 2.4e6;
        }
        float ci = 1, cq = -1;                          // as gen_data.c when max_fo == 0
        if (max_fo > 0) {
            ci = (float) (M_SQRT2 * cos(ph)); cq = (float) (M_SQRT2 * sin(ph));
            ph += w;
        }
        float v[2] = { o[i] * GAIN * ci + noise * gauss(), o[i] * GAIN * cq + noise * gauss() };
        int16_t s[2];
        for (int j = 0; j < 2; j++) {
            float x = v[j] > 2047.0f ? 2047.0f : v[j] < -2048.0f ? -2048.0f : v[j];
            s[j] = (int16_t) lrintf(x * 16);
        }
        fwrite(s, sizeof s, 1, f);
    }
    fclose(f);
    msresamp_rrrf_destroy(r);
    free(u);
    free(o);
    return 0;
}
