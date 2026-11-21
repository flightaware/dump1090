#!/usr/bin/env python3
# Build seed corpora for the fuzz harnesses from tools/gen-traffic.py output.
#   fuzz/make_seeds.py OUTDIR
import os, subprocess, sys, tempfile

out = sys.argv[1]
here = os.path.dirname(os.path.abspath(__file__))
gen = os.path.join(here, '..', 'tools', 'gen-traffic.py')
tmp = tempfile.mkdtemp()
subprocess.check_call([sys.executable, gen, '--aircraft', '30', '--seconds', '4',
                       '--avr', tmp + '/t.avr', '--beast', tmp + '/t.beast', '--iq', tmp + '/t.iq'])
frames = [bytes.fromhex(l.strip()[1:-1]) for l in open(tmp + '/t.avr')]
beast = open(tmp + '/t.beast', 'rb').read()
iq = open(tmp + '/t.iq', 'rb').read()

for d in ('decode', 'net', 'demod'):
    os.makedirs(os.path.join(out, 'corpus_' + d), exist_ok=True)

def put(d, name, data):
    with open(os.path.join(out, 'corpus_' + d, name), 'wb') as f:
        f.write(data)

# decode: records of [hdr(kind=2, time step 1)][sig][14 bytes]
for n in range(0, min(len(frames), 400), 40):
    rec = b''
    for fr in frames[n:n + 40]:
        rec += bytes([(1 << 2) | 2, 0x80]) + fr.ljust(14, b'\0')
    put('decode', 'frames%03d' % n, rec)
put('decode', 'modeac', bytes([(1 << 2) | 3, 0x80, 0x12, 0x34, (1 << 2) | 3, 0x40, 0x08, 0x10]))

# net
for n in range(0, min(len(frames), 200), 50):
    put('net', 'avr%03d' % n, bytes([0, n]) + b''.join(b'*' + f.hex().upper().encode() + b';\n' for f in frames[n:n + 50]))
put('net', 'avr_ts', bytes([0, 7]) + b'@0123456789AB8D4840D6202CC371C32CE0576098;\n<0123456789ABFF8D4840D6202CC371C32CE0576098;\n%0123456789AB5D4840D6A0B8B6;\n')
for n in range(0, min(len(beast), 8000), 2000):
    put('net', 'beast%04d' % n, bytes([1, n & 0xff]) + beast[n:n + 2000])
put('net', 'beast_radarcape', bytes([1, 3]) + b'\x1a5' + bytes(range(1, 22)) + b'\x1a1' + bytes(8) + b'\x12\x34')
put('net', 'beastcmd', bytes([2, 0x81]) + b'\x1a1V\x1a1L\x1a1J\x1a1l\x1a1v\x1a1j\x1a1C\x1a1\x1a\x1a')
put('net', 'faupcmd', bytes([3, 5]) + b'upload_rate_multiplier\t2.5\nupload_unknown_commb\t1\nfoo\tbar\n')

# demod: synthesized records and raw IQ windows
for n in range(0, min(len(frames), 120), 12):
    rec = bytes([0x10])
    pos = 100
    for fr in frames[n:n + 12]:
        rec += bytes([pos >> 8, pos & 0xff, 0x80, 0x04, 0x00]) + fr.ljust(14, b'\0')
        pos += 330
    put('demod', 'synth%03d' % n, rec)
for k, off in enumerate(range(0, len(iq), len(iq) // 8)):
    put('demod', 'iq_uc8_%d' % k, bytes([0x01]) + iq[off:off + 17000])
    put('demod', 'iq_sc16_%d' % k, bytes([0x03]) + iq[off:off + 17000])
    put('demod', 'iq_sc16q11_%d' % k, bytes([0x05 | 0x08]) + iq[off:off + 17000])
print('seeds written to', out)
