#!/usr/bin/env python3

import subprocess
import re

command = [
    'pdftotext', 
    '-layout',
    '-f', '187',
    '-l', '191',
    '-enc', 'ASCII7',
    'adsb-AN10_V3_cons.pdf',
    '-'
]
main_line = re.compile(r' ([^*]+?)\s+\*\s+([01-]{4})\s+([01-]{2})\s+([01-]{3})\s+([01-]{3})\s+([01-]{2})\s+([-]{10})\s*')
continuation_line = re.compile(r'  ([^ ].*)\s*')

def scan():
    matches = []

    process = subprocess.Popen(command,
                               stdin=subprocess.DEVNULL,
                               stdout=subprocess.PIPE)
    match = None
    for line in process.stdout:
        line = line.decode('ascii')
        if match:
            cmatch = continuation_line.match(line)
            if cmatch:
                country = match.group(1) + ' ' + cmatch.group(1)
            else:
                country = match.group(1)

            matches.append((country, 
                            match.group(2) + match.group(3) +
                            match.group(4) + match.group(5) +
                            match.group(6) + match.group(7)))

            if cmatch:
                match = None
                continue

        match = main_line.match(line)

    if match:
        matches.append((match.group(1),
                        match.group(2) + match.group(3) +
                        match.group(4) + match.group(5) +
                        match.group(6) + match.group(7)))

    return matches

if __name__ == '__main__':
    matches = scan()

    print ('var ICAO_Ranges = [');

    for country, assignment in matches:
        low = int(assignment.replace('-', '0'), 2)
        high = int(assignment.replace('-', '1'), 2)
        print('        {{ start: 0x{low:06X}, end: 0x{high:06X}, country: "{country}", flag_image: "{flag}" }},'.format(
            low=low,
            high=high,
            country=country,
            flag=country.replace(' ','_').replace("'","").replace('-','_') + '.png'))
    print ('];')
