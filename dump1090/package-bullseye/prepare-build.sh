#!/bin/sh

# This script prepares a package tree for building on different target
# distributions

if [ $# -lt 1 ]
then
    echo "Syntax: $0 <distribution> [target]" >&2
    echo "" >&2
    echo "Prepares a package tree in <target> (default package-<distribution>) ready for building" >&2
    exit 1
fi

export DEBFULLNAME="${DEBFULLNAME:-FlightAware build automation}"
export DEBEMAIL="${DEBEMAIL:-adsb-devs@flightaware.com}"

TOP=`dirname $0`
DIST=$1
OUT=$2

if [ -n "$OUT" ]
then
    OUT=$(realpath $OUT)
else
    OUT=$(realpath package-$DIST)
fi

if [ -e $OUT ]
then
    echo "$OUT already exists, refusing to overwrite it" >&2
    exit 1
fi

FILES=$(find $TOP -mindepth 1 -maxdepth 1 -name .git -prune -o -name 'debian*' -prune -o -name 'package-*' -prune -o -print)
mkdir -p $OUT
cp -a $FILES $OUT
cp -a $TOP/debian $OUT
[ -d $TOP/debian-$DIST ] && cp -a $TOP/debian-$DIST/* $OUT/debian/

case "$DIST" in
    stretch)
        echo "Updating changelog for stretch backport build" >&2
        dch --changelog $OUT/debian/changelog --local ~bpo9+ --force-distribution --distribution stretch-backports "Automated backport build for jessie"
        ;;

    buster)
        echo "Updating changelog for buster backport build" >&2
        dch --changelog $OUT/debian/changelog --local ~bpo10+ --force-distribution --distribution buster-backports "Automated backport build for buster"
        ;;

    bullseye)
        ;;

    *)
        echo "Don't know how to build for a distribution named $DIST" >&2
        exit 1
esac

echo "Package tree prepared in $OUT" >&2
