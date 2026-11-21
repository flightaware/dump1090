#!/usr/bin/env python3
#
# Generate synthetic, deterministic Mode S / ADS-B traffic for testing and
# benchmarking dump1090 without SDR hardware.
#
# A set of simulated aircraft fly around a receiver location and emit
# DF17 extended squitters (airborne/surface position with CPR, velocity,
# identification, operational status), DF11 all-call replies, DF4/DF5
# surveillance replies and DF20/DF21 Comm-B replies (BDS 2,0 / 4,0 / 5,0 /
# 6,0), at roughly real-world rates.
#
# Outputs (any combination):
#   --beast FILE   Beast binary stream (feed to --net-bi-port, or replay
#                  with tools/replay-beast.py)
#   --avr FILE     AVR raw lines "*HEX;" (feed to --net-ri-port)
#   --iq FILE      unsigned 8-bit IQ samples at 2.4MHz (dump1090 --ifile FILE
#                  --iformat UC8), with the frames PPM-modulated over noise
#
# Example:
#   tools/gen-traffic.py --aircraft 300 --seconds 20 --iq /tmp/t.iq --beast /tmp/t.beast
#

import argparse
import math
import random
import struct
import sys

# ---- CRC -----------------------------------------------------------------

_CRC_POLY = 0xFFF409
_crc_table = []
for _i in range(256):
    _c = _i << 16
    for _ in range(8):
        _c = ((_c << 1) ^ _CRC_POLY) if (_c & 0x800000) else (_c << 1)
    _crc_table.append(_c & 0xFFFFFF)


def crc24(data):
    rem = 0
    for b in data:
        rem = ((rem << 8) ^ _crc_table[b ^ ((rem >> 16) & 0xFF)]) & 0xFFFFFF
    return rem


def with_parity(body, overlay=0):
    p = crc24(body) ^ overlay
    return bytes(body) + bytes([(p >> 16) & 0xFF, (p >> 8) & 0xFF, p & 0xFF])

# ---- bit packing ---------------------------------------------------------


class Bits:
    def __init__(self):
        self.v = 0
        self.n = 0

    def put(self, value, width):
        self.v = (self.v << width) | (int(value) & ((1 << width) - 1))
        self.n += width
        return self

    def bytes(self):
        assert self.n % 8 == 0, self.n
        return self.v.to_bytes(self.n // 8, 'big')

# ---- field encoders --------------------------------------------------------


AIS = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ##### ###############0123456789######"


def ais_chars(s):
    s = (s + "        ")[:8]
    return [AIS.index(ch) for ch in s]


def ac12(alt):
    n = max(0, int(round((alt + 1000) / 25.0)))
    return ((n & 0x7F0) << 1) | 0x010 | (n & 0x00F)


def ac13(alt):
    n = max(0, int(round((alt + 1000) / 25.0)))
    return ((n & 0x7E0) << 2) | ((n & 0x010) << 1) | 0x010 | (n & 0x00F)


def id13(squawk):
    a, b, c, d = (int(ch) for ch in "%04d" % squawk)
    bit = lambda v, k: (v >> k) & 1
    # C1 A1 C2 A2 C4 A4 X B1 D1 B2 D2 B4 D4
    fields = [bit(c, 0), bit(a, 0), bit(c, 1), bit(a, 1), bit(c, 2), bit(a, 2), 0,
              bit(b, 0), bit(d, 0), bit(b, 1), bit(d, 1), bit(b, 2), bit(d, 2)]
    v = 0
    for f in fields:
        v = (v << 1) | f
    return v


def cpr_nl(lat):
    if lat == 0:
        return 59
    if abs(lat) == 87:
        return 2
    if abs(lat) > 87:
        return 1
    nz = 15
    a = 1 - math.cos(math.pi / (2 * nz))
    b = math.cos(math.pi / 180.0 * abs(lat)) ** 2
    return int(math.floor(2 * math.pi / math.acos(1 - a / b)))


def cpr_encode(lat, lon, odd, surface=False):
    span = 90.0 if surface else 360.0
    dlat = span / (59 if odd else 60)
    yz = int(math.floor(131072 * ((lat % dlat) / dlat) + 0.5))
    rlat = dlat * (yz / 131072.0 + math.floor(lat / dlat))
    nl = cpr_nl(rlat) - (1 if odd else 0)
    dlon = span / max(nl, 1)
    xz = int(math.floor(131072 * ((lon % dlon) / dlon) + 0.5))
    return yz & 0x1FFFF, xz & 0x1FFFF

# ---- aircraft model --------------------------------------------------------


class Aircraft:
    def __init__(self, rng, rlat, rlon, idx):
        self.addr = rng.randrange(0x000001, 0xFFFFFF)
        self.callsign = "%s%d" % (rng.choice(["QFA", "VOZ", "JST", "UAE", "SIA", "ANZ", "RXA", "N"]), rng.randrange(1, 9999))
        self.squawk = int("%d%d%d%d" % tuple(rng.randrange(0, 8) for _ in range(4)))
        self.surface = (idx % 25 == 0)
        dist = rng.uniform(0.0, 0.05) if self.surface else rng.uniform(0.05, 2.5)  # degrees
        brg = rng.uniform(0, 2 * math.pi)
        self.lat = rlat + dist * math.cos(brg)
        self.lon = rlon + dist * math.sin(brg) / math.cos(math.radians(rlat))
        self.alt = 0 if self.surface else rng.randrange(1000, 41000, 25)
        self.gs = rng.uniform(5, 25) if self.surface else rng.uniform(150, 500)
        self.track = rng.uniform(0, 360)
        self.rate = 0 if self.surface else rng.choice([0, 0, 0, 640, -640, 1600, -1600])
        self.sel_alt = (self.alt // 1000) * 1000
        self.qnh = 1013.2
        self.amplitude = rng.uniform(0.08, 0.9)  # relative signal level
        self.phase = rng.uniform(0, 2 * math.pi)
        self.odd = rng.randrange(2)
        self.category = rng.choice([1, 2, 3, 4, 5])
        # next-emission schedule (seconds) per message kind
        self.next = {k: rng.uniform(0, p) for k, p in PERIODS.items()}

    def step(self, dt):
        d = self.gs * dt / 3600.0 / 60.0  # nm -> degrees (approx)
        self.lat += d * math.cos(math.radians(self.track))
        self.lon += d * math.sin(math.radians(self.track)) / math.cos(math.radians(self.lat))
        if not self.surface:
            self.alt = max(500, self.alt + self.rate * dt / 60.0)

    # --- frames ---

    def df17(self, me):
        return with_parity(Bits().put(17, 5).put(5, 3).put(self.addr, 24).bytes() + me)

    def es_position(self):
        self.odd ^= 1
        if self.surface:
            yz, xz = cpr_encode(self.lat, self.lon, self.odd, surface=True)
            mov = 1 if self.gs < 0.125 else min(124, 9 + int(self.gs))  # rough
            me = (Bits().put(7, 5).put(mov, 7).put(1, 1).put(int(self.track * 128 / 360) & 0x7F, 7)
                  .put(0, 1).put(self.odd, 1).put(yz, 17).put(xz, 17).bytes())
        else:
            yz, xz = cpr_encode(self.lat, self.lon, self.odd)
            me = (Bits().put(11, 5).put(0, 2).put(0, 1).put(ac12(self.alt), 12)
                  .put(0, 1).put(self.odd, 1).put(yz, 17).put(xz, 17).bytes())
        return self.df17(me)

    def es_velocity(self):
        vx = self.gs * math.sin(math.radians(self.track))
        vy = self.gs * math.cos(math.radians(self.track))
        vr = int(abs(self.rate) / 64) + 1
        me = (Bits().put(19, 5).put(1, 3).put(0, 1).put(0, 1).put(1, 3)
              .put(1 if vx < 0 else 0, 1).put(min(1023, int(abs(vx)) + 1), 10)
              .put(1 if vy < 0 else 0, 1).put(min(1023, int(abs(vy)) + 1), 10)
              .put(0, 1).put(1 if self.rate < 0 else 0, 1).put(min(511, vr), 9)
              .put(0, 2).put(0, 1).put(0, 7).bytes())
        return self.df17(me)

    def es_ident(self):
        b = Bits().put(4, 5).put(self.category, 3)
        for c in ais_chars(self.callsign):
            b.put(c, 6)
        return self.df17(b.bytes())

    def es_opstatus(self):
        # TC 31 subtype 0 (airborne), version 2
        me = (Bits().put(31, 5).put(0, 3).put(0, 16).put(0, 16).put(2, 3).put(0, 1).put(9, 4)
              .put(0, 2).put(3, 2).put(0, 1).put(0, 1).put(0, 2).bytes())
        return self.df17(me)

    def df11(self):
        return with_parity(Bits().put(11, 5).put(5, 3).put(self.addr, 24).bytes())

    def df4(self):
        return with_parity(Bits().put(4, 5).put(1 if self.surface else 0, 3).put(0, 5).put(0, 6).put(ac13(self.alt), 13).bytes(), self.addr)

    def df5(self):
        return with_parity(Bits().put(5, 5).put(1 if self.surface else 0, 3).put(0, 5).put(0, 6).put(id13(self.squawk), 13).bytes(), self.addr)

    def commb(self, rng):
        which = rng.choice([0x20, 0x40, 0x50, 0x60])
        if which == 0x20:
            b = Bits().put(0x20, 8)
            for c in ais_chars(self.callsign):
                b.put(c, 6)
        elif which == 0x40:
            b = (Bits().put(1, 1).put(int(self.sel_alt / 16), 12)
                 .put(0, 1).put(0, 12)
                 .put(1, 1).put(int((self.qnh - 800) * 10), 12)
                 .put(0, 8).put(0, 1).put(0, 3).put(0, 2).put(0, 1).put(0, 2))
        elif which == 0x50:
            trk = self.track if self.track <= 180 else self.track - 360
            tv = int(round(trk * 512 / 180.0)) & 0x7FF
            b = (Bits().put(1, 1).put(0, 10)                      # roll 0
                 .put(1, 1).put(tv, 11)                           # track (sign+10)
                 .put(1, 1).put(int(self.gs / 2), 10)             # GS
                 .put(1, 1).put(0, 10)                            # track rate 0
                 .put(1, 1).put(int(self.gs / 2), 10))            # TAS ~ GS
        else:
            hdg = self.track if self.track <= 180 else self.track - 360
            hv = int(round(hdg * 512 / 180.0)) & 0x7FF
            ias = max(0, self.gs - 0.12 * self.alt / 100)
            mach = self.gs / 661.0
            rv = int(self.rate / 32) & 0x3FF
            b = (Bits().put(1, 1).put(hv, 11)
                 .put(1, 1).put(int(ias), 10)
                 .put(1, 1).put(int(mach * 1000 / 4), 10)
                 .put(1, 1).put(rv, 10)
                 .put(1, 1).put(rv, 10))
        mb = b.bytes()
        if rng.random() < 0.5:
            hdr = Bits().put(20, 5).put(0, 3).put(0, 5).put(0, 6).put(ac13(self.alt), 13).bytes()
        else:
            hdr = Bits().put(21, 5).put(0, 3).put(0, 5).put(0, 6).put(id13(self.squawk), 13).bytes()
        return with_parity(hdr + mb, self.addr)


PERIODS = {
    'pos': 0.5, 'vel': 0.5, 'ident': 5.0, 'opstatus': 2.5,
    'df11': 1.0, 'df4': 1.2, 'df5': 3.0, 'commb': 1.5,
}


def generate(args):
    rng = random.Random(args.seed)
    planes = [Aircraft(rng, args.lat, args.lon, i) for i in range(args.aircraft)]
    msgs = []  # (time, amplitude, phase, bytes)
    t = 0.0
    dt = 0.05
    while t < args.seconds:
        for p in planes:
            for kind, period in PERIODS.items():
                if p.surface and kind in ('vel', 'commb'):
                    continue
                while p.next[kind] <= t + dt:
                    when = p.next[kind]
                    p.next[kind] += period * rng.uniform(0.8, 1.2)
                    if kind == 'pos':
                        f = p.es_position()
                    elif kind == 'vel':
                        f = p.es_velocity()
                    elif kind == 'ident':
                        f = p.es_ident()
                    elif kind == 'opstatus':
                        f = p.es_opstatus()
                    elif kind == 'df11':
                        f = p.df11()
                    elif kind == 'df4':
                        f = p.df4()
                    elif kind == 'df5':
                        f = p.df5()
                    else:
                        f = p.commb(rng)
                    if when >= 0:
                        msgs.append((when, p.amplitude, p.phase, f))
            p.step(dt)
        t += dt
    msgs.sort(key=lambda m: m[0])
    return [m for m in msgs if m[0] < args.seconds]


def write_beast(msgs, path):
    with open(path, 'wb') as f:
        for when, amp, _, frame in msgs:
            ts = int(when * 12e6) & 0xFFFFFFFFFFFF
            sig = max(1, min(255, int(math.sqrt(amp) * 255)))
            body = bytes([0x32 if len(frame) == 7 else 0x33]) + ts.to_bytes(6, 'big') + bytes([sig]) + frame
            out = bytearray([0x1a])
            for b in body:
                out.append(b)
                if b == 0x1a:
                    out.append(0x1a)
            f.write(out)


def write_avr(msgs, path):
    with open(path, 'w') as f:
        for _, _, _, frame in msgs:
            f.write("*%s;\n" % frame.hex().upper())


def write_iq(msgs, path, seconds, seed, noise_sigma):
    import numpy as np
    rate = 2400000
    over = 10
    nrng = np.random.default_rng(seed)
    total = int(seconds * rate)
    chunk = rate  # 1 second per chunk
    mi = 0
    with open(path, 'wb') as f:
        for start in range(0, total, chunk):
            n = min(chunk, total - start)
            sig = (nrng.standard_normal(n) + 1j * nrng.standard_normal(n)).astype(np.complex64) * noise_sigma
            # include frames that start in this chunk (frames overlapping the
            # chunk end are truncated; at 120us that is negligible)
            while mi < len(msgs) and msgs[mi][0] * rate < start + n:
                when, amp, phase, frame = msgs[mi]
                mi += 1
                bits = len(frame) * 8
                s0 = when * rate - start
                sub0 = int(math.floor(s0 * over))
                nsub = (8 + bits + 2) * 24
                env = np.zeros(nsub + over * 2, dtype=np.float32)
                for us2 in (0, 2, 7, 9):
                    env[us2 * 12:us2 * 12 + 12] = 1.0
                v = int.from_bytes(frame, 'big')
                for i in range(bits):
                    bit = (v >> (bits - 1 - i)) & 1
                    o = (8 + i) * 24 + (0 if bit else 12)
                    env[o:o + 12] = 1.0
                # align to sample grid
                lead = sub0 % over
                env = np.concatenate([np.zeros(lead, dtype=np.float32), env])
                ns = len(env) // over
                env = env[:ns * over].reshape(ns, over).mean(axis=1)
                first = sub0 // over
                lo = max(0, first)
                hi = min(n, first + ns)
                if hi <= lo:
                    continue
                a = amp * 120.0
                sig[lo:hi] += (env[lo - first:hi - first] * a * np.exp(1j * phase)).astype(np.complex64)
            i = np.clip(np.rint(sig.real + 127.4), 0, 255).astype(np.uint8)
            q = np.clip(np.rint(sig.imag + 127.4), 0, 255).astype(np.uint8)
            iq = np.empty(2 * n, dtype=np.uint8)
            iq[0::2] = i
            iq[1::2] = q
            f.write(iq.tobytes())


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--aircraft', type=int, default=200)
    ap.add_argument('--seconds', type=float, default=10.0)
    ap.add_argument('--seed', type=int, default=1090)
    ap.add_argument('--lat', type=float, default=45.0)
    ap.add_argument('--lon', type=float, default=10.0)
    ap.add_argument('--noise', type=float, default=4.0, help='IQ noise sigma (uc8 units)')
    ap.add_argument('--beast')
    ap.add_argument('--avr')
    ap.add_argument('--iq')
    args = ap.parse_args()

    msgs = generate(args)
    print("%d messages over %.1fs (%.0f msg/s) from %d aircraft" %
          (len(msgs), args.seconds, len(msgs) / max(args.seconds, 1e-9), args.aircraft), file=sys.stderr)
    if args.beast:
        write_beast(msgs, args.beast)
    if args.avr:
        write_avr(msgs, args.avr)
    if args.iq:
        write_iq(msgs, args.iq, args.seconds, args.seed, args.noise)


if __name__ == '__main__':
    main()
