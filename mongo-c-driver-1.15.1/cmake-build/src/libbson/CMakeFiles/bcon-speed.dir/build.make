# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.10

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build

# Include any dependencies generated for this target.
include src/libbson/CMakeFiles/bcon-speed.dir/depend.make

# Include the progress variables for this target.
include src/libbson/CMakeFiles/bcon-speed.dir/progress.make

# Include the compile flags for this target's objects.
include src/libbson/CMakeFiles/bcon-speed.dir/flags.make

src/libbson/CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.o: src/libbson/CMakeFiles/bcon-speed.dir/flags.make
src/libbson/CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.o: ../src/libbson/examples/bcon-speed.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object src/libbson/CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.o"
	cd /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.o   -c /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/examples/bcon-speed.c

src/libbson/CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.i"
	cd /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/examples/bcon-speed.c > CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.i

src/libbson/CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.s"
	cd /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/examples/bcon-speed.c -o CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.s

src/libbson/CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.o.requires:

.PHONY : src/libbson/CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.o.requires

src/libbson/CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.o.provides: src/libbson/CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.o.requires
	$(MAKE) -f src/libbson/CMakeFiles/bcon-speed.dir/build.make src/libbson/CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.o.provides.build
.PHONY : src/libbson/CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.o.provides

src/libbson/CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.o.provides.build: src/libbson/CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.o


# Object files for target bcon-speed
bcon__speed_OBJECTS = \
"CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.o"

# External object files for target bcon-speed
bcon__speed_EXTERNAL_OBJECTS =

src/libbson/bcon-speed: src/libbson/CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.o
src/libbson/bcon-speed: src/libbson/CMakeFiles/bcon-speed.dir/build.make
src/libbson/bcon-speed: src/libbson/libbson-1.0.so.0.0.0
src/libbson/bcon-speed: /usr/lib/x86_64-linux-gnu/librt.so
src/libbson/bcon-speed: /usr/lib/x86_64-linux-gnu/libm.so
src/libbson/bcon-speed: src/libbson/CMakeFiles/bcon-speed.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable bcon-speed"
	cd /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/bcon-speed.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/libbson/CMakeFiles/bcon-speed.dir/build: src/libbson/bcon-speed

.PHONY : src/libbson/CMakeFiles/bcon-speed.dir/build

src/libbson/CMakeFiles/bcon-speed.dir/requires: src/libbson/CMakeFiles/bcon-speed.dir/examples/bcon-speed.c.o.requires

.PHONY : src/libbson/CMakeFiles/bcon-speed.dir/requires

src/libbson/CMakeFiles/bcon-speed.dir/clean:
	cd /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson && $(CMAKE_COMMAND) -P CMakeFiles/bcon-speed.dir/cmake_clean.cmake
.PHONY : src/libbson/CMakeFiles/bcon-speed.dir/clean

src/libbson/CMakeFiles/bcon-speed.dir/depend:
	cd /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1 /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson/CMakeFiles/bcon-speed.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/libbson/CMakeFiles/bcon-speed.dir/depend

