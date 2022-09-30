#!/usr/bin/env python2

#
# Converts a Virtual Radar Server BasicAircraftLookup.sqb database
# to a CSV file suitable for feeding to csv-to-json.py
#

import sqlite3, csv, sys
from contextlib import closing

def extract(dbfile):
    writer = csv.DictWriter(sys.stdout,
                            fieldnames=['icao24', 'r', 't'])
    writer.writeheader()
    with closing(sqlite3.connect(dbfile)) as db:
        with closing(db.execute('SELECT a.Icao, a.Registration, m.Icao FROM Aircraft a, Model m WHERE a.ModelID = m.ModelID')) as c:
            for icao24, reg, icaotype in c:
                writer.writerow({
                    'icao24': icao24,
                    'r': reg,
                    't': icaotype
                })

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print >>sys.stderr, 'Reads a VRS sqlite database and writes a CSV to stdout'
        print >>sys.stderr, 'Syntax: %s <path to BasicAircraftLookup.sqb>' % sys.argv[0]
        sys.exit(1)
    else:
        extract(sys.argv[1])
        sys.exit(0)
