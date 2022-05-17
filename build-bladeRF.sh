#/bin/bash
cd bladeRF

mkdir build
cd build
#for arm (?)
#cmake ../ -DINSTALL_UDEV_RULES=ON -D_FILE_OFFSET_BITS=64
cmake ../ -DINSTALL_UDEV_RULES=ON
make && sudo make install && sudo ldconfig
cd ../..
