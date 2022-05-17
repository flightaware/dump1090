To build `dump1090`:
1. Install required packages: `librtlsdr0`
2. Run `./build-libmongoc.sh` to build and install mongoc libraries
3. Run `make clean && ./build.sh` to build `dump1090`

Default path to dump1090 service is: `/data/flight/`
Copy the `dump1090 binary` file to the following directory:
`/data/flight/dump1090/`
