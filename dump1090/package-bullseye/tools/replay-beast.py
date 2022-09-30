#!/usr/bin/env python3

#
# Reads Beast-format input files and replays them to stdout
# (either in their original form or in a human-readable form)
# while maintaining the correct inter-message delays according
# to the timestamps contained in the input file.
#

from contextlib import closing
import time

MODE_AC = 'MODE_AC'
MODE_S_SHORT = 'MODE_S_SHORT'
MODE_S_LONG = 'MODE_S_LONG'
RADARCAPE_STATUS = 'RADARCAPE_STATUS'

def parse(buf):
    i = 0
    messages = []
    msglen = -1
    start = 0

    while i < len(buf):
        if buf[i] != 0x1a:
            i += 1
            continue

        i += 1
        if i >= len(buf):
            break

        msglen = 1 + 6
        if buf[i] == 0x31:
            msglen += 2
            msgtype = MODE_AC
        elif buf[i] == 0x32:
            msglen += 7
            msgtype = MODE_S_SHORT
        elif buf[i] == 0x33:
            msglen += 14
            msgtype = MODE_S_LONG
        elif buf[i] == 0x34:
            msglen += 14
            msgtype = RADARCAPE_STATUS
        else:
            continue

        i += 1
        msgbytes = bytearray()
        while i < len(buf) and len(msgbytes) < msglen:
            if buf[i] == 0x1a:
                i += 1
                if i >= len(buf) or buf[i] != 0x1a:
                    break

            msgbytes.append(buf[i])
            i += 1

        if len(msgbytes) == msglen:
            timestamp = (msgbytes[0] << 40) | (msgbytes[1] << 32) | (msgbytes[2] << 24) | (msgbytes[3] << 16) | (msgbytes[4] << 8) | (msgbytes[5])
            signal = msgbytes[6]
            data = msgbytes[7:]
            raw = buf[start:i]
            messages.append( (msgtype, timestamp, signal, data, raw) )
            start = i

    return (buf[start:], messages)

def replay(filename, radarcape_mode, show_mode, delay_mode):
    with closing(open(filename, 'rb')) as f:
        buf = b''
        last_timestamp = None
        last_time = None

        while True:
            more = f.read(1024)
            buf = buf + more

            buf, messages = parse(buf)
            if not messages and not more:
                break

            for msgtype, timestamp, signal, data, raw in messages:
                if delay_mode:
                    if radarcape_mode:
                        secs = timestamp >> 30
                        nanos = timestamp & 0x00003FFFFFFF
                        adj_timestamp = nanos + secs * 1000000000
                        freq = 1e9
                    else:
                        adj_timestamp = timestamp
                        freq = 12e6

                    if last_timestamp is None:
                        last_timestamp = adj_timestamp
                        last_time = time.time()
                    elif adj_timestamp > last_timestamp:
                        now = time.time()
                        sched_delta = (adj_timestamp - last_timestamp) / freq
                        delay = last_time + sched_delta - now
                        if delay > 0.010:
                            time.sleep(delay)
                        last_timestamp = adj_timestamp
                        last_time += sched_delta

                if show_mode:
                    h = ''
                    for b in data:
                        h += '{0:02X}'.format(b)
                    print("Type: {0:16s} Time: {1:06X} Signal: {2:3d} Data: {3}".format(msgtype, timestamp, signal, h))
                else:
                    sys.stdout.buffer.write(raw)
                    sys.stdout.buffer.flush()

if __name__ == '__main__':
    import sys

    radarcape_mode = False
    show_mode = False
    delay_mode = True
    for filename in sys.argv[1:]:
        if filename == '--radarcape':
            radarcape_mode = True
        elif filename == '--beast':
            radarcape_mode = False
        elif filename == '--show':
            show_mode = True
        elif filename == '--raw':
            show_mode = False
        elif filename == '--delay':
            delay_mode = True
        elif filename == '--no-delay':
            delay_mode = False
        else:
            replay(filename, radarcape_mode=radarcape_mode, show_mode=show_mode, delay_mode=delay_mode)
