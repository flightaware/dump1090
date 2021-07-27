#!/usr/bin/env python3

# Run me like this:
#  view1090 --no-interactive | ./extract-comm-b.py

import re, sys, time
from contextlib import closing

commb_match = re.compile(r'^DF:\d+ addr:([a-zA-Z0-9]{6}) FS:\d+ DR:\d+ UM:\d+ (?:ID|AC):\d+ MB:([a-zA-Z0-9]{14})$')

for line in sys.stdin:
    match = commb_match.match(line)
    if match:
        addr, mb = match.groups()

        with closing(open('commb/' + addr.upper() + '.txt', 'a')) as f:
            print('%.3f %s' % (time.time(), mb), file=f)
