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
include src/libmongoc/CMakeFiles/bulk-collation.dir/depend.make

# Include the progress variables for this target.
include src/libmongoc/CMakeFiles/bulk-collation.dir/progress.make

# Include the compile flags for this target's objects.
include src/libmongoc/CMakeFiles/bulk-collation.dir/flags.make

src/libmongoc/CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.o: src/libmongoc/CMakeFiles/bulk-collation.dir/flags.make
src/libmongoc/CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.o: ../src/libmongoc/examples/bulk/bulk-collation.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object src/libmongoc/CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.o"
	cd /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libmongoc && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.o   -c /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libmongoc/examples/bulk/bulk-collation.c

src/libmongoc/CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.i"
	cd /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libmongoc && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libmongoc/examples/bulk/bulk-collation.c > CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.i

src/libmongoc/CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.s"
	cd /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libmongoc && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libmongoc/examples/bulk/bulk-collation.c -o CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.s

src/libmongoc/CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.o.requires:

.PHONY : src/libmongoc/CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.o.requires

src/libmongoc/CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.o.provides: src/libmongoc/CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.o.requires
	$(MAKE) -f src/libmongoc/CMakeFiles/bulk-collation.dir/build.make src/libmongoc/CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.o.provides.build
.PHONY : src/libmongoc/CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.o.provides

src/libmongoc/CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.o.provides.build: src/libmongoc/CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.o


# Object files for target bulk-collation
bulk__collation_OBJECTS = \
"CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.o"

# External object files for target bulk-collation
bulk__collation_EXTERNAL_OBJECTS =

src/libmongoc/bulk-collation: src/libmongoc/CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.o
src/libmongoc/bulk-collation: src/libmongoc/CMakeFiles/bulk-collation.dir/build.make
src/libmongoc/bulk-collation: src/libmongoc/libmongoc-1.0.so.0.0.0
src/libmongoc/bulk-collation: /usr/lib/x86_64-linux-gnu/libssl.so
src/libmongoc/bulk-collation: /usr/lib/x86_64-linux-gnu/libcrypto.so
src/libmongoc/bulk-collation: /usr/lib/x86_64-linux-gnu/libsnappy.so
src/libmongoc/bulk-collation: /usr/lib/x86_64-linux-gnu/libz.so
src/libmongoc/bulk-collation: src/libbson/libbson-1.0.so.0.0.0
src/libmongoc/bulk-collation: /usr/lib/x86_64-linux-gnu/librt.so
src/libmongoc/bulk-collation: /usr/lib/x86_64-linux-gnu/libm.so
src/libmongoc/bulk-collation: src/libmongoc/CMakeFiles/bulk-collation.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable bulk-collation"
	cd /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libmongoc && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/bulk-collation.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/libmongoc/CMakeFiles/bulk-collation.dir/build: src/libmongoc/bulk-collation

.PHONY : src/libmongoc/CMakeFiles/bulk-collation.dir/build

src/libmongoc/CMakeFiles/bulk-collation.dir/requires: src/libmongoc/CMakeFiles/bulk-collation.dir/examples/bulk/bulk-collation.c.o.requires

.PHONY : src/libmongoc/CMakeFiles/bulk-collation.dir/requires

src/libmongoc/CMakeFiles/bulk-collation.dir/clean:
	cd /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libmongoc && $(CMAKE_COMMAND) -P CMakeFiles/bulk-collation.dir/cmake_clean.cmake
.PHONY : src/libmongoc/CMakeFiles/bulk-collation.dir/clean

src/libmongoc/CMakeFiles/bulk-collation.dir/depend:
	cd /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1 /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/src/libmongoc /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libmongoc /home/pps/src/misc/dump1090-ifd-clean/mongo-c-driver-1.15.1/cmake-build/src/libmongoc/CMakeFiles/bulk-collation.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/libmongoc/CMakeFiles/bulk-collation.dir/depend

