# dump1090 - ADS-B software radio decoder
#
# Copyright (C) 2018 Paul Ciarlo <paul.ciarlo@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.11)

project(lib1090)

#for android
#add_subdirectory(deps)

#for desktop linux with libusb-1.0 installed
add_library(usb-1.0 SHARED IMPORTED)
set_target_properties(
	usb-1.0

	PROPERTIES IMPORTED_LOCATION

	/usr/lib/libusb-1.0.so.0
)
include_directories( /usr/include/libusb-1.0 )

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -g -DENABLE_RTLSDR -std=c11")
set(SDR_SOURCES sdr.c sdr_ifile.c sdr_rtlsdr.c)

#for linux build
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_DEFAULT_SOURCE -Wall -Werror -Wno-format-truncation")
set(COMPAT_SOURCES)
set(EXTRA_LIBS "-lm -lncurses -lpthread")

#for darwin build
#set(COMPAT_SOURCES compat/clock_nanosleep/clock_nanosleep.c)
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMISSING_NANOSLEEP -DCLOCKID_T)
#set(EXTRA_LIBS "-lm -lncurses -lpthread")

add_library(
	1090

	SHARED
	
	anet.c
	${COMPAT_SOURCES}
	convert.c
	cpr.c
	crc.c
	demod_2400.c
	icao_filter.c
	event_loop.c
	interactive.c
	mode_ac.c
	mode_s.c
	net_io.c
	${SDR_SOURCES}
	stats.c
	track.c
	util.c
	
	dump1090.c
	faup1090.c
	view1090.c
)

target_link_libraries(
	1090

	usb-1.0
	rtlsdr
	#"-Wl,-soname,lib1090.so.0"
)

set_property(TARGET 1090 PROPERTY SOVERSION 0)
set_property(TARGET 1090 PROPERTY VERSION 0.0)

add_executable(dump1090 dump1090-main.c)
target_link_libraries(dump1090 1090 ${EXTRA_LIBS})
add_executable(faup1090 faup1090-main.c)
target_link_libraries(faup1090 1090 ${EXTRA_LIBS})
add_executable(view1090 view1090-main.c)
target_link_libraries(view1090 1090 ${EXTRA_LIBS})

INSTALL(
    TARGETS dump1090 faup1090 view1090 1090
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)
