# Puerto Polli - avventura grafica per Macintosh Classic
#
# Macintosh (68k), con il toolchain Retro68:
#   mkdir build-mac && cd build-mac
#   cmake .. -DCMAKE_TOOLCHAIN_FILE=<Retro68-build>/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
#   make            -> PuertoPolli.bin (MacBinary), PuertoPolli.dsk (immagine disco), PuertoPolli.APPL
#
# PC (sviluppo), con SDL2:
#   mkdir build && cd build && cmake .. && make
#   ./puertopolli          (gioco)    ./puertopolli_test    (test automatico)

cmake_minimum_required(VERSION 3.10)
project(PuertoPolli C)

set(GAME_SOURCES
    src/engine.c
    src/msg.c
    src/walk.c
    src/save.c
    src/ui.c
    src/anim.c
    src/script.c
    src/gfx.c
    src/assets.c
)

if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    add_application(PuertoPolli
        ${GAME_SOURCES}
        src/mac/main_mac.c
        src/mac/puertopolli.r
        CREATOR "PPol"
        TYPE "APPL"
    )
    target_compile_options(PuertoPolli PRIVATE -O2 -ffunction-sections -Wall -Wno-pointer-sign -Wno-array-bounds)
    # Le parti che girano a ogni fotogramma (grafica, animazioni, motore)
    # restano a -O2; storia, finestre, salvataggi, messaggi e camminata non
    # incidono sulla velocita' e si compilano piu' piccoli.
    set_source_files_properties(src/script.c src/ui.c src/save.c src/msg.c src/walk.c src/mac/main_mac.c
        PROPERTIES COMPILE_OPTIONS "-Os")
    set_target_properties(PuertoPolli PROPERTIES
        LINK_FLAGS "-Wl,-gc-sections -Wl,--mac-strip-macsbug")
else()
    if(NOT MSVC)
        add_compile_options(-std=c99 -Wall -Wextra -O2)
    endif()

    add_executable(puertopolli_test ${GAME_SOURCES} src/test/main_test.c)

    find_package(SDL2 QUIET)
    if(SDL2_FOUND)
        add_executable(puertopolli ${GAME_SOURCES} src/sdl/main_sdl.c)
        target_include_directories(puertopolli PRIVATE ${SDL2_INCLUDE_DIRS})
        target_link_libraries(puertopolli ${SDL2_LIBRARIES})
    else()
        message(STATUS "SDL2 non trovato: compilo solo il test automatico")
    endif()

    enable_testing()
    add_test(NAME walkthrough COMMAND puertopolli_test ${CMAKE_CURRENT_BINARY_DIR})
endif()
