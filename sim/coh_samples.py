# Reader for the chip-sample dump written by dump1090 built with -DCOHERENT_SAMPLES=\"file\"
# (experiment branch demod-2400x-coherent). See dumpFrame() in demod_2400x.c.
#
#   import coh_samples
#   d = coh_samples.load('chips.bin')
#   d['x']        complex64 [n, 240]: chip-centre filter outputs, k = 0..15 preamble,
#                 16 + 2i and 17 + 2i the two chips of bit i
#   d['accepted'] bool [n]: frame passed the CRC with the magnitude bits
#   d['mag_bits'], d['good_bits'] uint8 [n, 112]: magnitude bits, corrected bits
#   d['pos'], d['quality'], d['quality2'], d['score'], d['addr']

import numpy as np

CHIPS = 16 + 2 * 112

RECORD = np.dtype([
    ('accepted', '<i4'), ('pos', '<i4'), ('quality', '<u4'), ('quality2', '<u4'),
    ('score', '<i4'), ('addr', '<u4'),
    ('mag_msg', 'u1', 14), ('good_msg', 'u1', 14),
    ('chips', '<i4', (CHIPS, 2)),
])


def load(path, count=-1):
    r = np.fromfile(path, dtype=RECORD, count=count)
    return {
        'x': (r['chips'][:, :, 0] + 1j * r['chips'][:, :, 1]).astype(np.complex64),
        'accepted': r['accepted'] == 1,
        'mag_bits': np.unpackbits(r['mag_msg'], axis=1),
        'good_bits': np.unpackbits(r['good_msg'], axis=1),
        'pos': r['pos'], 'quality': r['quality'], 'quality2': r['quality2'],
        'score': r['score'], 'addr': r['addr'],
    }


def slice_bits(y):
    """Bits from real chip values y [n, 240] (or magnitudes): first chip larger = 1."""
    return (y[:, 16::2] > y[:, 17::2]).astype(np.uint8)
