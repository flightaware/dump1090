#!/usr/bin/env python2

#
# Converts a Virtual Radar Server BasicAircraftLookup.sqb database
# into a bunch of json files suitable for use by the webmap
#

import sqlite3, json, sys, csv
from contextlib import closing

def readcsv(name, infile, blocks):
    print >>sys.stderr, 'Reading from', name

    if len(blocks) == 0:
        for i in xrange(16):
            blocks['%01X' % i] = {}

    ac_count = 0

    reader = csv.DictReader(infile)
    if not 'icao24' in reader.fieldnames:
        raise RuntimeError('CSV should have at least an "icao24" column')
    for row in reader:
        icao24 = row['icao24']

        entry = {}
        for k,v in row.items():
            if k != 'icao24' and v != '':
                entry[k] = v

        if len(entry) > 0:
            ac_count += 1

            bkey = icao24[0:1].upper()
            dkey = icao24[1:].upper()
            blocks[bkey].setdefault(dkey, {}).update(entry)

    print >>sys.stderr, 'Read', ac_count, 'aircraft from', name

def cleandb(blocks):
    for blockdata in blocks.values():
        for dkey in list(blockdata.keys()):
            block = blockdata[dkey]
            for key in list(block.keys()):
                if block[key] == '-COMPUTED-':
                    del block[key]
            if len(block) == 0:
                del blockdata[dkey]

def writedb(blocks, todir, blocklimit, debug):
    block_count = 0

    print >>sys.stderr, 'Writing blocks:',

    queue = sorted(blocks.keys())
    while queue:
        bkey = queue[0]
        del queue[0]

        blockdata = blocks[bkey]
        if len(blockdata) > blocklimit:
            if debug: print >>sys.stderr, 'Splitting block', bkey, 'with', len(blockdata), 'entries..',

            # split all children out
            children = {}
            for dkey in blockdata.keys():
                new_bkey = bkey + dkey[0]
                new_dkey = dkey[1:]

                if new_bkey not in children: children[new_bkey] = {}
                children[new_bkey][new_dkey] = blockdata[dkey]

            # look for small children we can retain in the parent, to
            # reduce the total number of files needed. This reduces the
            # number of blocks needed from 150 to 61
            blockdata = {}
            children = sorted(children.items(), key=lambda x: len(x[1]))
            retained = 1

            while len(children[0][1]) + retained < blocklimit:
                # move this child back to the parent
                c_bkey, c_entries = children[0]
                for c_dkey, entry in c_entries.items():
                    blockdata[c_bkey[-1] + c_dkey] = entry
                    retained += 1
                del children[0]

            if debug: print >>sys.stderr, len(children), 'children created,', len(blockdata), 'entries retained in parent'
            children = sorted(children, key=lambda x: x[0])
            blockdata['children'] = [x[0] for x in children]
            blocks[bkey] = blockdata
            for c_bkey, c_entries in children:
                blocks[c_bkey] = c_entries
                queue.append(c_bkey)

        path = todir + '/' + bkey + '.json'
        if debug: print >>sys.stderr, 'Writing', len(blockdata), 'entries to', path
        else: print >>sys.stderr, bkey,
        block_count += 1
        with closing(open(path, 'w')) as f:
            json.dump(obj=blockdata, fp=f, check_circular=False, separators=(',',':'), sort_keys=True)

    print >>sys.stderr, 'done.'
    print >>sys.stderr, 'Wrote', block_count, 'blocks'

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print >>sys.stderr, 'Reads a CSV file with aircraft information and produces a directory of JSON files'
        print >>sys.stderr, 'Syntax: %s <path to CSV> [... additional CSV files ...] <path to DB dir>' % sys.argv[0]
        print >>sys.stderr, 'Use "-" as the CSV path to read from stdin'
        print >>sys.stderr, 'If multiple CSV files are specified and they provide conflicting data'
        print >>sys.stderr, 'then the data from the last-listed CSV file is used'
        sys.exit(1)

    blocks = {}
    for filename in sys.argv[1:-1]:
        if filename == '-':
            readcsv('stdin', sys.stdin, blocks)
        else:
            with closing(open(filename, 'r')) as infile:
                readcsv(filename, infile, blocks)

    cleandb(blocks)
    writedb(blocks, sys.argv[-1], 2500, False)
    sys.exit(0)
