// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// demod_2400x.c: 2.4MHz Mode S demodulator working on I/Q samples
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

#ifdef DEMOD_2400X

#include <assert.h>

#include "dsp/helpers/demod2400x_dsp.h"

// The metric index is this many 12 MHz samples (3 input samples, the FIR delay)
// before the true start of the preamble
#define UPSAMPLE_DELAY 15

#ifdef USE_MAGNITUDE
#define PEAMBLE_THRES 50
#define DATA_THRES    80
#else
#define PEAMBLE_THRES 80
#define DATA_THRES    150
#endif
#define DATA2_THRES   ((4*DATA_THRES)/5)


// Static data
struct staticdata_t {

    int usize;
    uint16_t *udata;

    int msize;
    uint8_t *metric;

    unsigned prev_mlen;     // new samples in the previous buffer (0 = no previous buffer)
    int next_start;         // first peak position to search in this buffer (skips the rest of a
                            // frame decoded at the end of the previous buffer)
};

// One preamble candidate
struct candidate {
    int pos;                            // metric index = 12 MHz sample index from mag->data[0]
    unsigned quality;                   // data quality of the first 56 bits (0..256)
    unsigned quality2;                  // data quality of the second 56 bits (0..256)
    uint8_t msg[MODES_LONG_MSG_BYTES];  // demodulated frame
};


// Allocate holding buffers
static int allocBuffer(int nsize, int *osize, void **bptr) {

   void *nptr;
   if (nsize > *osize) {

       if ((nptr = realloc(*bptr, nsize)) == NULL) {

          free(*bptr);
          *bptr = NULL;
          *osize = 0;
          return -1;
       }

       *bptr = nptr;
       *osize = nsize;
   }

   return 0;
}


// Allocate buffers
static int allocateBuffers(struct staticdata_t *stdata, int totalLength) {

   int nt = NU_SAMP(totalLength);
   int stride = NU_STRIDE(nt);
   int mt = METRIC_SIZE(nt);

   if (allocBuffer(6*stride*sizeof(uint16_t), &stdata->usize, (void **)&stdata->udata) < 0) return -1;
   if (allocBuffer(mt*sizeof(uint8_t), &stdata->msize, (void **)&stdata->metric) < 0) return -1;

   return 0;
}


//#define CHECK_METRIC
#ifdef CHECK_METRIC
static void checkMetric(const uint8_t *metric, int mlen) {

    unsigned max_noise = 0;
    unsigned sum_noise = 0;
    for (int i = 0; i < mlen>>1; i++) {

        if (metric[i] > max_noise)
            max_noise = metric[i];

        sum_noise += metric[i];
    }
    unsigned peak1 = 0, peak2 = 0;
    int pos1 = 0, pos2 = 0;

    /* Find largest peak */
    for (int i = mlen >> 1; i < mlen; i++) {
        if (metric[i] > peak1) {
            peak1 = metric[i];
            pos1 = i;
        }
    }

    /* Find largest peak sufficiently far away */
    #define SEP 6
    for (int i = mlen >> 1; i < mlen; i++) {
        if ((i > pos1 ? i - pos1 : pos1 - i) < SEP)
            continue;
        if (metric[i] > peak2) {
            peak2 = metric[i];
            pos2 = i;
        }
    }

    fprintf(stderr, "noise=%u avg=%u p1=%u@%d p2=%u@%d\n"
            "nr=%.2f fr=%.2f\n",
            max_noise,
            sum_noise/(unsigned)(mlen>>1),
            peak1, pos1,
            peak2, pos2,
            (double)peak1/max_noise,
            (double)peak1/peak2);
}
#endif


// Decode 56 bits starting at metric position 'pos'; returns a quality score (0..256).
// Each bit is two chips: bit = 1 if the first is larger. Branch-free: on noise the
// bit values are random, and a branch on them mispredicts half the time.
static inline __attribute__((always_inline))
unsigned decodeShortAdbsData(int pos, const uint16_t * restrict udata, int nt, uint8_t * restrict msg)
{
    int stride = NU_STRIDE(nt);
    int phase = pos % 6;
    int offs  = (pos / 6) + 16; // Start of data
    uint32_t qual = 0;
    uint32_t pow = 0;

    const uint16_t * restrict p = &udata[phase * stride + offs];
    for (int i = 0; i < SHORT_FRAME / 8; i++) {      // 7 bytes = 56 bits, 16 chips per byte

        unsigned bits = 0;
        for (int k = 0; k < 8; k++) {
            int a = p[2 * k];
            int b = p[2 * k + 1];
            int d = b - a;
            bits = (bits << 1) | ((unsigned) d >> 31);   // 1 if a > b
            qual += (uint32_t) (d < 0 ? -d : d);
            pow += (uint32_t) (a + b);
        }

        msg[i] = (uint8_t) bits;
        p += 16;
    }
    unsigned score = (256 * qual) / (pow + 1);
    return score;
}


// Is metric position 'p' a preamble peak?
static inline __attribute__((always_inline)) int isPeak(const uint8_t *metric, int p)
{
    unsigned m0 = metric[p - 1], m1 = metric[p], m2 = metric[p + 1];

    return m1 >= PEAMBLE_THRES && m1 >= m0 && m1 > m2;
}


// Does any of the 8 metric bytes at 'm' reach PEAMBLE_THRES? (exact: bytes below
// 128 get 128 - THRES added without carrying into the next byte; bytes from 128 up
// have their top bit set already)
_Static_assert(PEAMBLE_THRES >= 1 && PEAMBLE_THRES <= 128, "anyAboveThres8() needs 1 <= PEAMBLE_THRES <= 128");
static inline __attribute__((always_inline)) int anyAboveThres8(const uint8_t *m)
{
    const uint64_t ones = 0x0101010101010101ULL;
    uint64_t x;
    memcpy(&x, m, sizeof(x));
    uint64_t y = (x & (0x7f * ones)) + (128 - PEAMBLE_THRES) * ones;
    return ((x | y) & (0x80 * ones)) != 0;
}


// Try the peak and its two neighbours, keep the one whose first 56 bits look best
static void bestPhase(int peak, const uint16_t *udata, int nt, struct candidate *c)
{
    uint8_t msg[MODES_SHORT_MSG_BYTES];

    for (int p = peak - 1; p <= peak + 1; p++) {
        unsigned q = decodeShortAdbsData(p, udata, nt, msg);
        if (p == peak - 1 || q > c->quality) {
            c->pos = p;
            c->quality = q;
            memcpy(c->msg, msg, sizeof(msg));
        }
    }
}


// Demodulate the second 56 bits whatever the DF says: scoreModesMessage() can
// correct the DF field, including the bit that selects 56 or 112 bits, so the
// frame length is only known after scoring
static void demodFrame(struct candidate *c, const uint16_t *udata, int nt)
{
    c->quality2 = decodeShortAdbsData(c->pos + 12 * SHORT_FRAME, udata, nt, c->msg + MODES_SHORT_MSG_BYTES);
}


// Signal power over the frame's data bits, as a fraction of full scale
// (same scale as mag->mean_power). *energy gets the total, for the noise estimate.
static double signalPower(const int16_t *iq, int pos, int bits, double *energy)
{
    int start = (pos + UPSAMPLE_DELAY) / 5 + 19;    // skip the 8 us preamble (19.2 samples)
    int len = bits * 12 / 5;                        // 2.4 samples per bit
    uint64_t sum = 0;

    for (int k = start; k < start + len; k++) {
        int32_t I = iq[2 * k], Q = iq[2 * k + 1];
        sum += (uint32_t) (I * I + Q * Q);
    }

    *energy = sum / 2048.0 / 2048.0;
    Modes.stats_current.signal_power_sum += *energy;
    Modes.stats_current.signal_power_count += len;

    return *energy / len;
}


// Arrival time: the 12 MHz sample within +-2 of 'pos' where the frame's data chips
// (from the corrected message) correlate best with the upsampled signal, i.e. the
// chip centres. bestPhase() picks 'pos' by bit quality, which is flat across about
// 3 samples on a clean pulse, so 'pos' alone is a poor time estimate for MLAT.
static int fineTiming(int pos, const uint8_t *msg, int bits, const uint16_t *udata, int nt)
{
    static const int order[5] = { 0, -1, 1, -2, 2 };   // ties go to the nearest to 'pos'
    int stride = NU_STRIDE(nt);
    int best = pos;
    int32_t best_corr = INT32_MIN;

    if (pos < 2 || pos + 2 + 6 * (16 + 2 * bits) > nt)
        return pos;

    for (int k = 0; k < 5; k++) {
        int t = pos + order[k];
        const uint16_t *u = &udata[(t % 6) * stride + t / 6 + 16];    // first data chip
        int32_t corr = 0;

        for (int i = 0; i < bits; i++) {
            int d = u[2 * i] - u[2 * i + 1];                            // first chip - second
            corr += ((msg[i >> 3] >> (7 - (i & 7))) & 1) ? d : -d;
        }
        if (corr > best_corr) {
            best_corr = corr;
            best = t;
        }
    }
    return best;
}


// Score, decode and pass one frame on to the rest of dump1090.
// Returns its length in bits if it was accepted, else 0; *energy gets its signal energy.
static int emitFrame(const struct mag_buf *mag, const struct candidate *c,
                     const int16_t *iq, const uint16_t *udata, int nt, double *energy)
{
    static const struct modesMessage zeroMessage;
    struct modesMessage mm;

    int score = scoreModesMessage(c->msg);
    if (score < SR_ACCEPT_THRESHOLD) {
        if (score >= SR_UNKNOWN_THRESHOLD)
            Modes.stats_current.demod_rejected_unknown_icao++;
        else
            Modes.stats_current.demod_rejected_bad++;
        return 0;
    }

    mm = zeroMessage;
    mm.score = score;

    if (decodeModesMessage(&mm, c->msg) < 0) {
        Modes.stats_current.demod_rejected_bad++;
        return 0;
    }

    // A long frame without a CRC to check (DF16/20/21/24: address/parity only)
    // also needs a clean second half
    if (mm.msgbits == MODES_LONG_MSG_BITS && mm.msgtype != 17 && mm.msgtype != 18 &&
        c->quality2 < DATA2_THRES) {
        Modes.stats_current.demod_rejected_bad++;
        return 0;
    }
    Modes.stats_current.demod_accepted[mm.correctedbits]++;

    // Timestamp at the end of bit 56, even for 112-bit frames (Beast / Radarcape convention)
    int tpos = fineTiming(c->pos, mm.msg, mm.msgbits, udata, nt);
    mm.timestampMsg = mag->sampleTimestamp + tpos + UPSAMPLE_DELAY + (8 + 56) * 12;
    mm.sysTimestampMsg = mag->sysTimestamp + receiveclock_ms_elapsed(mag->sampleTimestamp, mm.timestampMsg);

    mm.signalLevel = signalPower(iq, c->pos, mm.msgbits, energy);
    if (mm.signalLevel > Modes.stats_current.peak_signal_power)
        Modes.stats_current.peak_signal_power = mm.signalLevel;
    if (mm.signalLevel > 0.50119)
        Modes.stats_current.strong_signal_count++;      // above -3 dBFS

    useModesMessage(&mm);
    return mm.msgbits;
}


// Search the metric for preambles and decode them. Only peaks at positions
// 1..limit belong to this buffer; later ones are in the next buffer's overlap
// and are searched there, so nothing is decoded twice.
// Returns the total signal energy of the accepted frames.
static double searchFrames(const struct mag_buf *mag, struct staticdata_t *st, int mlen, int nt)
{
    int limit = 5 * (int) (mag->validLength - mag->overlap);
    int last = MIN(limit, mlen - 2);
    double energy_sum = 0;
    int p;

    for (p = MAX(1, st->next_start); p <= last; p++) {

        // Most of the metric is noise below the threshold: skip it 8 at a time
        while (p + 7 <= last && !anyAboveThres8(st->metric + p))
            p += 8;
        if (p > last)
            break;

        if (!isPeak(st->metric, p))
            continue;
        Modes.stats_current.demod_preambles++;

        struct candidate c;
        bestPhase(p, st->udata, nt, &c);
        if (c.quality < DATA_THRES)
            continue;

        demodFrame(&c, st->udata, nt);

        double energy;
        int bits = emitFrame(mag, &c, (const int16_t *) mag->data, st->udata, nt, &energy);
        if (bits) {
            energy_sum += energy;
            // Skip to 8 bits before the end of the frame, as demod_2400.c does
            p = c.pos + bits * 12 - 1;
        }
    }

    // A skip that runs past 'limit' carries on into the next buffer,
    // whose position 0 is this buffer's 'limit'
    st->next_start = MAX(1, p - limit);
    return energy_sum;
}


#ifndef USE_MAGNITUDE
#define DEMOD_SHIFT_ADJ 3
static int powerShift(double mean_power)      // mean_power: fraction of 12-bit full scale
{
    if (!(mean_power > 0))
        return 0;
    int e;
    frexp(mean_power, &e);                    // mean_power = m * 2^e, 0.5 <= m < 1
    int shift = (e - 1) + 22 + DEMOD_SHIFT_ADJ;           // 2048^2 = 2^22
    return shift < 0 ? 0 : shift;
}
#endif

// Demodulate 2400
//#define CHECK_USAMP
//#define CHECK_POW
// mag->data contains DC-removed signed 12-bit I/Q sample pairs (-2048..2047): [overlap | new samples]
void demodulate2400x(struct mag_buf *mag)
{
   static struct staticdata_t stdata;

   if (allocateBuffers(&stdata, mag->totalLength) < 0)
       return;

   unsigned overlap = mag->overlap;
   unsigned dlen = mag->validLength;            // whole buffer, overlap included
   unsigned newlen = dlen - overlap;            // new samples in this buffer

   assert(overlap >= OVERLAP_WINDOW);

   // No usable skip from the previous buffer
   if (!stdata.prev_mlen || (mag->flags & MAGBUF_DISCONTINUOUS))
       stdata.next_start = 0;

#ifdef USE_MAGNITUDE
   int pshift = 0;                              // magnitude needs no gain shift
#else
   int pshift = powerShift(mag->mean_power);
#endif
   stdata.prev_mlen = newlen;

   // Upsample the whole buffer
   int ulen = starch_upsample12((const int16_t *) mag->data, (int) dlen, pshift, stdata.udata);

#ifdef CHECK_POW
   int max = 0;
   for (int i = 0; i < 6 * NU_STRIDE(ulen); i++) {
       int mm = stdata.udata[i];
       if (mm > max) max = mm;
   }
   fprintf(stderr, "pshift = %d, Max Power = %d\n", pshift, max);
#endif
#ifdef CHECK_USAMP
   int stride = NU_STRIDE(ulen);
   for (int i = 0; i < ulen; i++) {
        int phase = i % 6;
        int pos   = i / 6;
        fprintf(stderr, "%4d %u\n", i, stdata.udata[phase * stride + pos]);
   }
#endif

   // Locate possible preambles
   int mlen = starch_locate_preamble(stdata.udata, ulen, PEAMBLE_THRES, stdata.metric);   // metric < PEAMBLE_THRES is 0
#ifdef CHECK_METRIC
   checkMetric(stdata.metric, mlen);
#endif

   // Decode the frames this buffer owns
   double signal_energy = searchFrames(mag, &stdata, mlen, ulen);

   // Noise power = everything in the new samples that wasn't a decoded frame
   Modes.stats_current.noise_power_sum += mag->mean_power * newlen - signal_energy;
   Modes.stats_current.noise_power_count += newlen;
}

#endif /* DEMOD_2400X */
