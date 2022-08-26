# dump1090 aircraft database

The dump1090 webmap uses a static database of json files to provide aircraft
information.

This directory has some tools to turn a CSV file with aircraft data into
the json format that the dump1090 map expects.

## Data sources

The default data comes from a combination of:

 * some historical data kindly provided by VRS (unfortunately no longer
   updated) - this data is in vrs.csv.xz; and
 * a data export provided by FlightAware that is updated periodically -
   this data is in flightaware-*.csv.xz

The VRS data was extracted by:

```sh
$ wget http://www.virtualradarserver.co.uk/Files/BasicAircraftLookup.sqb.gz
$ gunzip BasicAircraftLookup.sqb.gz
$ tools/vrs-to-csv.py BasicAircraftLookup.sqb >tools/vrs.csv
```

The FlightAware data is a subset of the registry information that FlightAware
uses internally. It contains only the data that FlightAware can redistribute
to the public; some data sources that FlightAware uses do not allow this and
are excluded from the export.

## Regenerating the json database

To regenerate the json database from these input files:

```sh
$ rm ../public_html/db/*.json
$ xzcat vrs.csv.xz | nodejs ./filter-regs.js >vrs-filtered.csv
$ xzcat flightaware-20211210.csv.xz | nodejs ./filter-regs.js >fa-filtered.csv
$ ./csv-to-json.py vrs-filtered.csv fa-filtered.csv ../public_html/db
```

Additional CSV files can be given to `csv-to-json.py` if desired.

The contents of public_html/db should be installed where the webmap can find
them; the Debian packaging puts these in
/usr/share/skyaware/html/db

The CSV format is very simple. The first line must be a header line that names
the columns. These columns are understood:

* icao24: the 6-digit hex address of the aircraft
* r: the registration / tail number of the aircraft
* t: the ICAO aircraft type of the aircraft, e.g. B773

Any other columns are put into the json DB under the name you give them, but
the standard map code won't do anything special with them. You can pick these
columns up in the PlaneObject constructor (see planeObject.js where it calls
getAircraftData()) for later use.
