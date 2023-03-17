# JSON output formats

dump1090 generates several json files with informaton about the receiver itself, currently known aircraft,
and general statistics. These are used by the webmap, but could also be used by other things
e.g. [this collectd plugin](https://github.com/mutability/dump1090-tools/tree/master/collectd) feeds stats
about dump1090's operation to collectd for later graphing.

## Reading the json files

dump1090-fa writes json files periodically to the location specified by the `--write-json` command line option.
These json files can then be exposed via a separate standalone webserver e.g. lighttpd.

The files are written periodically; for aircraft, typically once a second, for stats, once a minute.
The interval between file updates can be controlled by the `--write-json-every` and `--json-stats-every` options.
As these files are frequently updated, it's a good idea to put them in RAM rather than on disk. Package installs
default to putting the file under `/run`, which is in RAM.

New versions of each file are written to a temporary file, then atomically renamed to the right path, so you should never see partial copies.

Each file contains a single JSON object. The file formats are:

## receiver.json

This file has general metadata about dump1090. It does not change often and you probably just want to read it once at startup.
The keys are:

 * version: the version of dump1090 in use
 * refresh: how often aircraft.json is updated (for the file version), in milliseconds. the webmap uses this to control its refresh interval.
 * history: the current number of valid history files (see below)
 * lat: the latitude of the receiver in decimal degrees. Optional, may not be present.
 * lon: the longitude of the receiver in decimal degrees. Optional, may not be present.

## aircraft.json

This file contains dump1090's list of recently seen aircraft. The keys are:

 * now: the time this file was generated, in seconds since Jan 1 1970 00:00:00 GMT (the Unix epoch).
 * messages: the total number of Mode S messages processed since dump1090 started.
 * aircraft: an array of JSON objects, one per known aircraft. Each aircraft has the following keys. Keys will be omitted if data is not available.
   * hex: the 24-bit ICAO identifier of the aircraft, as 6 hex digits. The identifier may start with '~', this means that the address is a non-ICAO address (e.g. from TIS-B).
   * type: type of underlying message, one of:
     * adsb_icao: messages from a Mode S or ADS-B transponder, using a 24-bit ICAO address
     * adsb_icao_nt: messages from an ADS-B equipped "non-transponder" emitter e.g. a ground vehicle, using a 24-bit ICAO address
     * adsr_icao: rebroadcast of ADS-B messages originally sent via another data link e.g. UAT, using a 24-bit ICAO address
     * tisb_icao: traffic information about a non-ADS-B target identified by a 24-bit ICAO address, e.g. a Mode S target tracked by secondary radar
     * adsb_other: messages from an ADS-B transponder using a non-ICAO address, e.g. anonymized address
     * adsr_other: rebroadcast of ADS-B messages originally sent via another data link e.g. UAT, using a non-ICAO address
     * tisb_other: traffic information about a non-ADS-B target using a non-ICAO address
     * tisb_trackfile: traffic information about a non-ADS-B target using a track/file identifier, typically from primary or Mode A/C radar
   * flight: callsign, the flight name or aircraft registration as 8 chars (2.2.8.2.6)
   * alt_baro: the aircraft barometric altitude in feet
   * alt_geom: geometric (GNSS / INS) altitude in feet referenced to the WGS84 ellipsoid
   * gs: ground speed in knots
   * ias: indicated air speed in knots
   * tas: true air speed in knots
   * mach: Mach number
   * track: true track over ground in degrees (0-359)
   * track_rate: Rate of change of track, degrees/second
   * roll: Roll, degrees, negative is left roll
   * mag_heading: Heading, degrees clockwise from magnetic north
   * true_heading: Heading, degrees clockwise from true north
   * baro_rate: Rate of change of barometric altitude, feet/minute
   * geom_rate: Rate of change of geometric (GNSS / INS) altitude, feet/minute
   * squawk: Mode A code (Squawk), encoded as 4 octal digits
   * emergency: ADS-B emergency/priority status, a superset of the 7x00 squawks (2.2.3.2.7.8.1.1)
   * category: emitter category to identify particular aircraft or vehicle classes (values A0 - D7) (2.2.3.2.5.2)
   * nav_qnh: altimeter setting (QFE or QNH/QNE), hPa
   * nav_altitude_mcp: selected altitude from the Mode Control Panel / Flight Control Unit (MCP/FCU) or equivalent equipment
   * nav_altitude_fms: selected altitude from the Flight Manaagement System (FMS) (2.2.3.2.7.1.3.3)
   * nav_heading: selected heading (True or Magnetic is not defined in DO-260B, mostly Magnetic as that is the de facto standard) (2.2.3.2.7.1.3.7)
   * nav_modes: set of engaged automation modes: 'autopilot', 'vnav', 'althold', 'approach', 'lnav', 'tcas'
   * lat, lon: the aircraft position in decimal degrees
   * nic: Navigation Integrity Category (2.2.3.2.7.2.6)
   * rc: Radius of Containment, meters; a measure of position integrity derived from NIC & supplementary bits. (2.2.3.2.7.2.6, Table 2-69)
   * seen_pos: how long ago (in seconds before "now") the position was last updated
   * version: ADS-B Version Number 0, 1, 2 (3-7 are reserved) (2.2.3.2.7.5)
   * nic_baro: Navigation Integrity Category for Barometric Altitude (2.2.5.1.35)
   * nac_p: Navigation Accuracy for Position (2.2.5.1.35)
   * nac_v: Navigation Accuracy for Velocity (2.2.5.1.19)
   * sil: Source Integity Level (2.2.5.1.40)
   * sil_type: interpretation of SIL: unknown, perhour, persample
   * gva: Geometric Vertical Accuracy  (2.2.3.2.7.2.8)
   * sda: System Design Assurance (2.2.3.2.7.2.4.6)
   * modea: true if we seem to be also receiving Mode A responses from this aircraft
   * modec: true if we seem to be also receiving Mode C responses from this aircraft
   * mlat: list of fields derived from MLAT data
   * tisb: list of fields derived from TIS-B data
   * messages: total number of Mode S messages received from this aircraft
   * seen: how long ago (in seconds before "now") a message was last received from this aircraft
   * rssi: recent average RSSI (signal power), in dbFS; this will always be negative.

Section references (2.2.xyz) refer to DO-260B.

## history_0.json, history_1.json, ..., history_119.json

These files are historical copies of aircraft.json at (by default) 30 second intervals. They follow exactly the
same format as aircraft.json. To know how many are valid, see receiver.json ("history" value). They are written in
a cycle, with history_0 being overwritten after history_119 is generated, so history_0.json is not necessarily the
oldest history entry. To load history, you should:

 * read "history" from receiver.json.
 * load that many history_N.json files
 * sort the resulting files by their "now" values
 * process the files in order
 
## stats.json

This file contains statistics about dump1090's operations.

There are 5 top level keys: "latest", "last1min", "last5min", "last15min", "total". Each key has statistics for a different period, defined by the "start" and "end" subkeys:

 * "total" covers the entire period from when dump1090 was started up to the current time
 * "last1min" covers a recent 1-minute period. This may be up to 1 minute out of date (i.e. "end" may be up to 1 minute old).
 * "last5min" covers a recent 5-minute period. As above, this may be up to 1 minute out of date.
 * "last15min" covers a recent 15-minute period. As above, this may be up to 1 minute out of date.
 * "latest" covers the time between the end of the "last1min" period and the current time.

Internally, live stats are collected into "latest". Once a minute, "latest" is copied to "last1min" and "latest" is reset. Then "last5min" and "last15min" are recalculated from a history of the last 5 or 15 1-minute periods.

Each period has the following subkeys:

 * start: the start time (in seconds-since-1-Jan-1970) of this statistics collection period.
 * end: the end time (in seconds-since-1-Jan-1970) of this statistics collection period.
 * local: statistics about messages received from a local SDR dongle. Not present in --net-only mode. Has subkeys:
   * samples_processed: number of samples processed
   * samples_dropped: number of samples dropped before processing. A nonzero value means CPU overload.
   * modeac: number of Mode A / C messages decoded
   * modes: number of Mode S preambles received. This is *not* the number of valid messages!
   * bad: number of Mode S preambles that didn't result in a valid message
   * unknown_icao: number of Mode S preambles which looked like they might be valid but we didn't recognize the ICAO address and it was one of the message types where we can't be sure it's valid in this case.
   * accepted: array. Index N has the number of valid Mode S messages accepted with N-bit errors corrected.
   * signal: mean signal power of successfully received messages, in dbFS; always negative.
   * noise: mean noise power of non-message samples, in dbFS; always negative.
   * peak_signal: peak signal power of a successfully received message, in dbFS; always negative.
   * strong_signals: number of messages received that had a signal power above -3dBFS.
   * gain_db: the current SDR gain, floating-point dB. Might be absent depending on SDR type.
 * remote: statistics about messages received from remote clients. Only present in --net or --net-only mode. Has subkeys:
   * modeac: number of Mode A / C messages received.
   * modes: number of Mode S messages received.
   * bad: number of Mode S messages that had bad CRC or were otherwise invalid.
   * unknown_icao: number of Mode S messages which looked like they might be valid but we didn't recognize the ICAO address and it was one of the message types where we can't be sure it's valid in this case.
   * accepted: array. Index N has the number of valid Mode S messages accepted with N-bit errors corrected.
 * cpu: statistics about CPU use. Has subkeys:
   * demod: milliseconds spent doing demodulation and decoding in response to data from a SDR dongle
   * reader: milliseconds spent reading sample data over USB from a SDR dongle
   * background: milliseconds spent doing network I/O, processing received network messages, and periodic tasks.
 * cpr: statistics about Compact Position Report message decoding. Has subkeys:
   * surface: total number of surface CPR messages received
   * airborne: total number of airborne CPR messages received
   * global_ok: global positions successfuly derived
   * global_bad: global positions that were rejected because they were inconsistent
     * global_range: global positions that were rejected because they exceeded the receiver max range
     * global_speed: global positions that were rejected because they failed the inter-position speed check
   * global_skipped: global position attempts skipped because we did not have the right data (e.g. even/odd messages crossed a zone boundary)
   * local_ok: local (relative) positions successfully found
     * local_aircraft_relative: local positions found relative to a previous aircraft position
     * local_receiver_relative: local positions found relative to the receiver position
   * local_skipped: local (relative) positions not used because we did not have the right data
     * local_range: local positions not used because they exceeded the receiver max range or fell into the ambiguous part of the receiver range
     * local_speed: local positions not used because they failed the inter-position speed check
   * filtered: number of CPR messages ignored because they matched one of the heuristics for faulty transponder output
 * tracks: statistics on aircraft tracks. Each track represents a unique aircraft and persists for up to 5 minutes after the last message
   from the aircraft is heard. If messages from the same aircraft are subsequently heard after the 5 minute period, this will be counted
   as a new track.
   * all: total tracks created
   * single_message: tracks consisting of only a single message. These are usually due to message decoding errors that produce a bad aircraft address.
   * unreliable: tracks that were never marked as reliable. These are also usually due to message decoding errors.
 * messages: total number of messages accepted by dump1090 from any source
 * messages_by_df: an array of integers where entry N (0..31) is the total number of messages accepted with downlink format (DF) = N.
 * adaptive: statistics on adaptive gain. Only present if adaptive gain is enabled
   * gain_db: latest SDR gain (legacy; prefer to use `local.gain_db` instead)
   * dynamic_range_limit_db: latest dynamic-range-controlled upper gain limit, dB
   * gain_changes: number of gain changes made in this stats period
   * loud_undecoded: number of undecodable loud probably-a-valid-message bursts seen
   * loud_decoded: number of correctly decoded mesaages with a high signal level
   * noise_dbfs: adaptive gain noise floor estimate, dBFS
   * gain_seconds: object, keyed by integer gain step, values are an array of [floating point gain in dB, number of seconds spent at this gain setting]
