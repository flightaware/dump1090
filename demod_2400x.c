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

    int isize;
    int16_t *idata;         // DC-removed I/Q, laid out like the FIFO buffer: [overlap | new samples]

    int usize;
    uint16_t *udata;

    int msize;
    uint8_t *metric;

    int32_t offset_i;
    int32_t offset_q;

    unsigned prev_mlen;     // new samples in the previous buffer (0 = no previous buffer)
    int next_start;         // first peak position to search in this buffer (skips the rest of a
                            // frame decoded at the end of the previous buffer)
};

// One preamble candidate
struct candidate {
    int pos;                            // metric index = 12 MHz sample index from mag->data[0]
    unsigned quality;                   // data quality of the first 56 bits (0..256)
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

   if (allocBuffer(2*totalLength*sizeof(int16_t), &stdata->isize, (void **)&stdata->idata) < 0) return -1;
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


// Decode 1 bit
#define DECODE_BIT(pp, ofs, bt, qq, pw) ({ \
    int aa = (pp)[2*(ofs)+0]; \
    int bb = (pp)[2*(ofs)+1]; \
    int dd = bb - aa; \
    if (dd < 0) { \
       dd = -dd; \
       bt |= (1<<(7-ofs)); \
    } \
    qq += dd; \
    pw += aa+bb; \
})


// Decode 56 bits starting at metric position 'pos'; returns a quality score (0..256)
static unsigned decodeShortAdbsData(int pos, const uint16_t * restrict udata, int nt, uint8_t * restrict msg) {

    int stride = NU_STRIDE(nt);
    int phase = pos % 6;
    int offs  = (pos / 6) + 16; // Start of data
    uint32_t qual = 0;
    uint32_t pow = 0;

    const uint16_t * restrict p = &udata[phase * stride + offs];
    for (int i = 0; i < SHORT_FRAME / 8; i++) {      // 7 bytes = 56 bits, 16 chips per byte

        unsigned bit = 0;

        DECODE_BIT(p, 0, bit, qual, pow);
        DECODE_BIT(p, 1, bit, qual, pow);
        DECODE_BIT(p, 2, bit, qual, pow);
        DECODE_BIT(p, 3, bit, qual, pow);
        DECODE_BIT(p, 4, bit, qual, pow);
        DECODE_BIT(p, 5, bit, qual, pow);
        DECODE_BIT(p, 6, bit, qual, pow);
        DECODE_BIT(p, 7, bit, qual, pow);

        msg[i] = bit;
        p += 16;
    }
    unsigned score = (256 * qual) / (pow + 1);
    return score;
}


// Is metric position 'p' a preamble peak?
static inline int isPeak(const uint8_t *metric, int p)
{
    unsigned m0 = metric[p - 1], m1 = metric[p], m2 = metric[p + 1];

    return m1 >= PEAMBLE_THRES && m1 >= m0 && m1 > m2;
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


// Demodulate the rest of the frame if the DF says it is a long one.
// Returns the frame length in bits, or 0 if the second half doesn't hold up.
static int demodFrame(struct candidate *c, const uint16_t *udata, int nt)
{
    int bits = modesMessageLenByType(c->msg[0] >> 3);

    if (bits == MODES_LONG_MSG_BITS &&
        decodeShortAdbsData(c->pos + 12 * SHORT_FRAME, udata, nt, c->msg + MODES_SHORT_MSG_BYTES) < DATA2_THRES)
        return 0;

    return bits;
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


// Score, decode and pass one frame on to the rest of dump1090.
// Returns 1 if it was accepted; *energy gets its signal energy.
static int emitFrame(const struct mag_buf *mag, const struct candidate *c, int bits,
                     const int16_t *iq, double *energy)
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

    // Timestamp at the end of bit 56, even for 112-bit frames (Beast / Radarcape convention)
    mm.timestampMsg = mag->sampleTimestamp + c->pos + UPSAMPLE_DELAY + (8 + 56) * 12;
    mm.sysTimestampMsg = mag->sysTimestamp + receiveclock_ms_elapsed(mag->sampleTimestamp, mm.timestampMsg);
    mm.score = score;

    if (decodeModesMessage(&mm, c->msg) < 0) {
        Modes.stats_current.demod_rejected_bad++;
        return 0;
    }
    Modes.stats_current.demod_accepted[mm.correctedbits]++;

    mm.signalLevel = signalPower(iq, c->pos, bits, energy);
    if (mm.signalLevel > Modes.stats_current.peak_signal_power)
        Modes.stats_current.peak_signal_power = mm.signalLevel;
    if (mm.signalLevel > 0.50119)
        Modes.stats_current.strong_signal_count++;      // above -3 dBFS

    useModesMessage(&mm);
    return 1;
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

        if (!isPeak(st->metric, p))
            continue;
        Modes.stats_current.demod_preambles++;

        struct candidate c;
        bestPhase(p, st->udata, nt, &c);
        if (c.quality < DATA_THRES)
            continue;

        int bits = demodFrame(&c, st->udata, nt);
        if (!bits)
            continue;

        double energy;
        if (emitFrame(mag, &c, bits, st->idata, &energy)) {
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


// Remove DC from 'dlen' new I/Q samples, 'in' -> 'out' (both interleaved I,Q).
// Returns the gain shift for upsample12.
#define AVG_SHIFT 12
static void removeDcOffset(const int16_t * restrict in,
                          int16_t * restrict out,
                          struct staticdata_t * restrict stdata,
                          int dlen)
{
    int32_t off_i = stdata->offset_i;
    int32_t off_q = stdata->offset_q;

    for (int i = 0; i < dlen; i++) {

        int32_t ii = in[0];
        int32_t qq = in[1];

        ii -= (off_i + (1<<(AVG_SHIFT-1))) >> AVG_SHIFT;
        qq -= (off_q + (1<<(AVG_SHIFT-1))) >> AVG_SHIFT;

        off_i += ii;
        off_q += qq;

        if (ii > 2047)  ii = 2047;
        if (ii < -2048) ii = -2048;

        if (qq > 2047)  qq = 2047;
        if (qq < -2048) qq = -2048;

        out[0] = (int16_t) ii;
        out[1] = (int16_t) qq;

        in += 2;
        out += 2;
    }

    stdata->offset_i = off_i;
    stdata->offset_q = off_q;
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
// mag->data contains signed 12-bit I/Q sample pairs (-2048..2047): [overlap | new samples]
void demodulate2400x(struct mag_buf *mag)
{
   static struct staticdata_t stdata;

   if (allocateBuffers(&stdata, mag->totalLength) < 0)
       return;

   unsigned overlap = mag->overlap;
   unsigned dlen = mag->validLength;            // whole buffer, overlap included
   unsigned newlen = dlen - overlap;            // new samples in this buffer

   assert(overlap >= OVERLAP_WINDOW);

   // This buffer's overlap = the filtered tail of the previous buffer,
   // the same way the FIFO does it for the raw samples
   if (!stdata.prev_mlen || (mag->flags & MAGBUF_DISCONTINUOUS)) {
       memset(stdata.idata, 0, 2 * overlap * sizeof(int16_t));
       stdata.next_start = 0;
   } else {
       memmove(stdata.idata, stdata.idata + 2 * stdata.prev_mlen, 2 * overlap * sizeof(int16_t));
   }

   // Remove DC offset from the new samples only
   removeDcOffset((const int16_t *) &mag->data[2 * overlap],
                   stdata.idata + 2 * overlap, &stdata, (int) newlen);
                   
#ifdef USE_MAGNITUDE
   int pshift = 0;                              // magnitude needs no gain shift
#else
   int pshift = powerShift(mag->mean_power);
#endif
   stdata.prev_mlen = newlen;

   // Upsample the whole buffer
   int ulen = starch_upsample12(stdata.idata, (int) dlen, pshift, stdata.udata);

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
   int mlen = starch_locate_preamble(stdata.udata, ulen, stdata.metric);
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
