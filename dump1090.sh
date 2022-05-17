#!/bin/bash
DUMPDIR=/data/flight/dump1090
DATADIR=data
OUTDIR=/data/box/monitoring

pushd $DUMPDIR
tar cfz $OUTDIR/$(</data/serveruuid.txt)_$(date -u +%Y%m%d-%H%M%S)_dump1090-data.tar.gz $DATADIR
rm -f $DATADIR/*
./dump1090 --fix --quiet --gain 50 --write-json $DATADIR
#./dump1090 --fix --quiet --gain 50 --write-json $DATADIR --write-mongo-uri "mongodb://localhost" --write-mongo-db "flightdata" --write-mongo-col "aircraftdata"
popd

