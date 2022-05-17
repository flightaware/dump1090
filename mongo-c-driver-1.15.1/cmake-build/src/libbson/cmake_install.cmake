# Install script for directory: /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "RelWithDebInfo")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libbson-1.0.so.0.0.0"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libbson-1.0.so.0"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libbson-1.0.so"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      file(RPATH_CHECK
           FILE "${file}"
           RPATH "")
    endif()
  endforeach()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson/libbson-1.0.so.0.0.0"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson/libbson-1.0.so.0"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson/libbson-1.0.so"
    )
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libbson-1.0.so.0.0.0"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libbson-1.0.so.0"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libbson-1.0.so"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/usr/bin/strip" "${file}")
      endif()
    endif()
  endforeach()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson/libbson-static-1.0.a")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/libbson-1.0/bson" TYPE FILE FILES
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson/src/bson/bson-config.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson/src/bson/bson-version.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bcon.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-atomic.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-clock.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-compat.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-context.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-decimal128.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-endian.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-error.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-iter.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-json.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-keys.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-macros.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-md5.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-memory.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-oid.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-prelude.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-reader.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-string.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-types.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-utf8.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-value.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-version-functions.h"
    "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/bson-writer.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/libbson-1.0" TYPE FILE FILES "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/src/bson/forwarding/bson.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson/src/libbson-1.0.pc")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson/src/libbson-static-1.0.pc")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/libbson-1.0" TYPE FILE FILES "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson/libbson-1.0-config.cmake")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/libbson-1.0" TYPE FILE FILES "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson/libbson-1.0-config-version.cmake")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/libbson-static-1.0" TYPE FILE FILES "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson/libbson-static-1.0-config.cmake")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/libbson-static-1.0" TYPE FILE FILES "/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson/libbson-static-1.0-config-version.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson/build/cmake_install.cmake")
  include("/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson/examples/cmake_install.cmake")
  include("/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson/src/cmake_install.cmake")
  include("/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson/tests/cmake_install.cmake")

endif()

