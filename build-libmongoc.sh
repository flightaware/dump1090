#!/bin/bash
VER=1.15.1

wget https://github.com/mongodb/mongo-c-driver/releases/download/${VER}/mongo-c-driver-${VER}.tar.gz
tar xzf mongo-c-driver-${VER}.tar.gz
cd mongo-c-driver-${VER}
mkdir cmake-build
cd cmake-build
cmake -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF ..
make && sudo make install
export PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig/
