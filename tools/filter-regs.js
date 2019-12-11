//
// This script processes a CSV file that contains
// ICAO addresses (column 'icao24') and registrations
// (column 'r')
//
// It replaces all registration entries that exactly match
// what dump1090 would have computed from the hexid anyway
// with the special value "-COMPUTED-"; these values are
// removed from the final data when csv-to-json.py writes
// the json database, reducing the size of the database
// in the cases where the two approaches match.
//
// It does a similar transformation on 'desc' (aircraft type
// description) and 'wtc' (wake turbulence categogy) based
// on the aircraft type designator in 't', if provided.
//
// Any additional columns are passed through unchanged.
//
// To run it:
//
//   sudo apt-get install nodejs
//   sudo apt-get install npm
//   npm install csv     # must be done in the same dir as this script
//   nodejs filter-regs.js <input.csv >output.csv

var reglookup = require('../public_html/registrations.js');
var actypes = require('../public_html/db/aircraft_types/icao_aircraft_types.json');
var csv = require('csv');

var parser = csv.parse({columns: true});
var writer = csv.stringify({header: true});
var transformer = csv.transform(function (record, callback) {
        if ('icao24' in record) {
                if ('r' in record && record.r != '') {
                        var computed = reglookup(record.icao24);
                        if (computed === record.r) {
                                record.r = '-COMPUTED-';
                        } else if (computed !== null) {
                                console.warn(record.icao24 + " computed registration " + computed + " but CSV data had " + record.r);
                        }
                }

                if ('t' in record && record.t in actypes) {
                        if ('desc' in record && record.desc != '') {
                                var computed_desc = actypes[record.t].desc;
                                if (computed_desc === record.desc) {
                                        record.desc = '-COMPUTED-';
                                } else if (computed_desc !== undefined) {
                                        // too noisy, the icao descriptors are very coarse and reality often disagrees
                                        //console.warn(record.icao24 + " (" + record.t + "): computed type description " + computed_desc + " but CSV data had " + record.desc);
                                }
                        }

                        if ('wtc' in record && record.wtc != '') {
                                var computed_wtc = actypes[record.t].wtc;
                                if (computed_wtc === record.wtc) {
                                        record.wtc = '-COMPUTED-';
                                } else if (computed_desc !== undefined) {
                                        //console.warn(record.icao24 + " (" + record.t + "): computed type WTC " + computed_wtc + " but CSV data had " + record.wtc);
                                }
                        }
                }
        }

        callback(null, record);
});

process.stdin.pipe(parser).pipe(transformer).pipe(writer).pipe(process.stdout);
