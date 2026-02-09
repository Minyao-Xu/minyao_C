# Find Zephyr. This also links to Zephyr's build system 
cmake_minimum_required(VERSION 3.20.0)

# set our board 
set(BOARD rpi_pico2/rp2350a/m33)

find_package(Zephyr)

# This is only used by IntelliSense inside VS Code 
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# For using west flash with picoprobe
set(OPENOCD "openocd")
set(OPENOCD_DEFAULT_PATH "/usr/local/share/openocd/scripts")

#define project name
project(lab2_part2)

# Add source files
target_include_directories(app PRIVATE src/inc)
FILE(GLOB SRC_FILES "src/*.c")
target_sources(app PRIVATE src/main.c ${SRC_FILES})

