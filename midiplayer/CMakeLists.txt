cmake_minimum_required(VERSION 3.10)
project(midiplayer C)

# MIDI Player: our own program against the hybrid library (no SDL2)
set(HYBRID ${CMAKE_CURRENT_SOURCE_DIR}/../hybrid)
option(MISTER_HYBRID "Build for the MiSTer hybrid core" ON)

# the core's font for the frame, as C (characters 32..126, 13 rows each)
file(STRINGS ${HYBRID}/rtl/hybrid_wall_font.hex FONT_ROWS)
list(TRANSFORM FONT_ROWS PREPEND "0x")
string(JOIN ", " FONT_C ${FONT_ROWS})
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/font.inc "${FONT_C}\n")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${HYBRID}/rtl/hybrid_wall_font.hex)

file(GLOB HYBRID_SOURCES ${HYBRID}/hps/*.c)
add_executable(midiplayer
    src/main.c src/ui.c src/player.c src/library.c src/midifile.c src/archive.c
    src/fonts.c src/gfx.c src/config.c src/names.c
    ${HYBRID_SOURCES})
target_include_directories(midiplayer PRIVATE src ${HYBRID}/hps ${CMAKE_CURRENT_BINARY_DIR})
# names and paths are cut to what the screen has room for, on purpose
target_compile_options(midiplayer PRIVATE -Wall -Wextra -Wno-format-truncation)
find_package(Threads REQUIRED)
target_link_libraries(midiplayer Threads::Threads m)
