# CMake generated Testfile for 
# Source directory: /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libmongoc
# Build directory: /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libmongoc
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test-libmongoc "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libmongoc/test-libmongoc")
set_tests_properties(test-libmongoc PROPERTIES  WORKING_DIRECTORY "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libmongoc/../..")
subdirs("build")
subdirs("examples")
subdirs("src")
subdirs("tests")
