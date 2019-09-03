#!/bin/bash
DUMPDIR=/data/flight/dump1090
DATADIR=data
pushd $DUMPDIR
tar cfz data_$(date +%Y%m%d_%H%M%S).tar.gz $DATADIR
rm -f $DATADIR/*
$DUMPDIR/dump1090 --fix --quiet --gain 40 --write-json $DATADIR --write-mongo-uri "mongodb://localhost" --write-mongo-db "flightdata" --write-mongo-col "aircraftdata"
popd
