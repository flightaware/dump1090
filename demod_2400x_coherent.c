// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// demod_2400x_coherent.c: coherent second pass for DEMOD_2400X (experimental)
//
// For a frame the magnitude decoder found but couldn't decode, decode it again
// coherently: the coherent_decode starch kernel (dsp/impl/demod2400x.*.c, method
// in dsp/helpers/demod2400x_coherent.h) estimates the carrier frequency and
// channel from the first 56 bits and slices the real part of all 112 bits.
//
// This file is free software: you may copy, redistribute and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 2 of the License, or (at your
// option) any later version.

#include "dump1090.h"

#if defined(DEMOD_2400X) && defined(DEMOD_2400X_COHERENT)

#include "demod_2400x_coherent.h"
#include "dsp/helpers/demod2400x_coherent.h"

float coh_last_freq_hz, coh_last_amp;     // experiment: last estimate, for diagnostics

// For the chip-sample dump: chips (int32 I, Q) at pos + 6k, k = 0 .. nchips-1
int coherentChips(const int16_t *iq, unsigned iq_len, int pos, int32_t (*chips)[2], int nchips)
{
    if (pos < 0 || (unsigned) ((pos + 6 * (nchips - 1)) / 5 + 8) > iq_len)
        return -1;
    for (int k = 0; k < nchips; k++)
        coh_chip(iq, pos + 6 * k, &chips[k][0], &chips[k][1]);
    return 0;
}

// Bounds check, then the starch kernel; the rho2 gate (first half) lets the kernel
// stop early on frames that aren't coherent. rho2b: coherence of the second half
int coherentDecode(const int16_t *iq, unsigned iq_len, int pos,
                   uint8_t *out, unsigned *q1, unsigned *q2, unsigned *rho2, unsigned *rho2b)
{
    uint32_t info[COH_INFO_N];

    if (pos < 0 || coh_iq_needed(pos) > iq_len)
        return -1;

    starch_coherent_decode(iq, pos, COHERENT_RHO2, out, info);
    *rho2 = info[COH_INFO_RHO2];
    *rho2b = info[COH_INFO_RHO2B];
    *q1 = info[COH_INFO_Q1];
    *q2 = info[COH_INFO_Q2];
    coh_last_freq_hz = (float) (COH_WBIN_HZ * ((double) info[COH_INFO_WIDE] - (COH_WBINS - 1) / 2.0) +
                                COH_BIN_HZ * ((double) info[COH_INFO_FINE] / COH_FINE - (COH_BINS - 1) / 2.0));
    coh_last_amp = 0;
    return 0;
}

#endif /* DEMOD_2400X && DEMOD_2400X_COHERENT */
