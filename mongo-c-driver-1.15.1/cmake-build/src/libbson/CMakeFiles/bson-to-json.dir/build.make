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
include src/libbson/CMakeFiles/bson-to-json.dir/depend.make

# Include the progress variables for this target.
include src/libbson/CMakeFiles/bson-to-json.dir/progress.make

# Include the compile flags for this target's objects.
include src/libbson/CMakeFiles/bson-to-json.dir/flags.make

src/libbson/CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.o: src/libbson/CMakeFiles/bson-to-json.dir/flags.make
src/libbson/CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.o: ../src/libbson/examples/bson-to-json.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object src/libbson/CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.o"
	cd /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.o   -c /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/examples/bson-to-json.c

src/libbson/CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.i"
	cd /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/examples/bson-to-json.c > CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.i

src/libbson/CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.s"
	cd /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson/examples/bson-to-json.c -o CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.s

src/libbson/CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.o.requires:

.PHONY : src/libbson/CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.o.requires

src/libbson/CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.o.provides: src/libbson/CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.o.requires
	$(MAKE) -f src/libbson/CMakeFiles/bson-to-json.dir/build.make src/libbson/CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.o.provides.build
.PHONY : src/libbson/CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.o.provides

src/libbson/CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.o.provides.build: src/libbson/CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.o


# Object files for target bson-to-json
bson__to__json_OBJECTS = \
"CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.o"

# External object files for target bson-to-json
bson__to__json_EXTERNAL_OBJECTS =

src/libbson/bson-to-json: src/libbson/CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.o
src/libbson/bson-to-json: src/libbson/CMakeFiles/bson-to-json.dir/build.make
src/libbson/bson-to-json: src/libbson/libbson-1.0.so.0.0.0
src/libbson/bson-to-json: /usr/lib/x86_64-linux-gnu/librt.so
src/libbson/bson-to-json: /usr/lib/x86_64-linux-gnu/libm.so
src/libbson/bson-to-json: src/libbson/CMakeFiles/bson-to-json.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable bson-to-json"
	cd /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/bson-to-json.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/libbson/CMakeFiles/bson-to-json.dir/build: src/libbson/bson-to-json

.PHONY : src/libbson/CMakeFiles/bson-to-json.dir/build

src/libbson/CMakeFiles/bson-to-json.dir/requires: src/libbson/CMakeFiles/bson-to-json.dir/examples/bson-to-json.c.o.requires

.PHONY : src/libbson/CMakeFiles/bson-to-json.dir/requires

src/libbson/CMakeFiles/bson-to-json.dir/clean:
	cd /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson && $(CMAKE_COMMAND) -P CMakeFiles/bson-to-json.dir/cmake_clean.cmake
.PHONY : src/libbson/CMakeFiles/bson-to-json.dir/clean

src/libbson/CMakeFiles/bson-to-json.dir/depend:
	cd /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1 /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libbson /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libbson/CMakeFiles/bson-to-json.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/libbson/CMakeFiles/bson-to-json.dir/depend

