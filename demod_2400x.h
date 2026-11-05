#ifndef DEMOD_2400X_H
#define DEMOD_2400X_H

// Input samples (2.4 MHz) a buffer must reach past its last searched position:
// 15 + 1 + 2*LONG_FRAME chips (preamble window, +1 for the peak search, long frame),
// x6/5 to convert 2 MHz chips to 2.4 MHz samples (rounded up), plus 7 for the 8-tap FIR history.
#define OVERLAP_WINDOW \
    (7 + ((15 + 1 + 2*LONG_FRAME) * 6 + 4) / 5)
    
extern void demodulate2400x(struct mag_buf *mbuf);
    
#endif
