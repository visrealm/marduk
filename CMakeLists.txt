# 3.22 is pico9918-core's floor, and add_subdirectory() enforces it here.
cmake_minimum_required(VERSION 3.22)

project(marduk VERSION 0.26 LANGUAGES C)

# C11: pico9918-core's public headers use _Static_assert.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Default to Release: the documented two-line build must not be unoptimised.
if(NOT CMAKE_BUILD_TYPE AND NOT DEFINED CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(MARDUK_SOURCES
    dasm80.c
    disk.c
    emu2149.c
    main.c
    modem.c
    vdp_bridge.c
    z80.c
)

add_executable(marduk ${MARDUK_SOURCES})

target_include_directories(marduk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# pico9918-core renders the VDP: the only VDP an SDL build has. Built from the
# submodule as a static library; linking it carries the includes and build config.
set(MARDUK_PICO9918_SRC ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/pico9918-core)

if(NOT EXISTS ${MARDUK_PICO9918_SRC}/CMakeLists.txt)
    message(FATAL_ERROR
        "pico9918-core is missing - run: git submodule update --init --recursive")
endif()

# MODE=1 is the F18A renderer and GPU; TEXT80_8BPP gives T80 ECM and the bitmap
# layer. Plain set() reaches the subproject's option() under CMP0077 NEW.
set(PICO9918_MODE 1)
set(PICO9918_TEXT80_8BPP ON)

# The chip personality is a runtime choice here, not a build one: one archive
# presents a TMS9918A, an F18A or a PICO9918 on request.
set(PICO9918_RUNTIME_CHIP ON)

# The library otherwise applies -march=native / /arch:AVX2 to published builds.
set(PICO9918_PORTABLE_CODEGEN ON)

add_subdirectory(${MARDUK_PICO9918_SRC} ${CMAKE_CURRENT_BINARY_DIR}/pico9918-core)

# Marduk's pixel policy, force-included ahead of the library's own #ifndef-guarded
# desktop defaults, to pin 32-bit ARGB. See pico9918_pixel_policy.h.
set(MARDUK_PICO9918_POLICY ${CMAKE_CURRENT_SOURCE_DIR}/pico9918_pixel_policy.h)
if(MSVC)
    target_compile_options(pico9918_core PRIVATE /FI${MARDUK_PICO9918_POLICY})
else()
    target_compile_options(pico9918_core PRIVATE -include ${MARDUK_PICO9918_POLICY})
endif()

# A force-include is not a tracked dependency, so name it as one.
set_property(TARGET pico9918_core APPEND PROPERTY SOURCES ${MARDUK_PICO9918_POLICY})

# The library's version for the config block's stamp, read back out of the
# subproject's directory scope, where its project() set it.
foreach(part MAJOR MINOR PATCH)
    get_directory_property(MARDUK_PICO9918_VER_${part}
        DIRECTORY ${MARDUK_PICO9918_SRC}
        DEFINITION pico9918_core_VERSION_${part})
    if(NOT DEFINED MARDUK_PICO9918_VER_${part} OR MARDUK_PICO9918_VER_${part} STREQUAL "")
        message(FATAL_ERROR
            "pico9918-core: could not read VERSION ${part} from its project(). The "
            "config block's version stamp gates the stored-settings migration, so "
            "an empty one cannot be built around.")
    endif()
endforeach()

target_compile_definitions(marduk PRIVATE
    PICO9918_CORE_VER_MAJOR=${MARDUK_PICO9918_VER_MAJOR}
    PICO9918_CORE_VER_MINOR=${MARDUK_PICO9918_VER_MINOR}
    PICO9918_CORE_VER_PATCH=${MARDUK_PICO9918_VER_PATCH})

message(STATUS "marduk: PICO9918 VDP engine via pico9918-core "
               "${MARDUK_PICO9918_VER_MAJOR}.${MARDUK_PICO9918_VER_MINOR}.${MARDUK_PICO9918_VER_PATCH}")

target_link_libraries(marduk PRIVATE pico9918::core)

option(MARDUK_FETCH_SDL2 "Download and build SDL2 with FetchContent" ON)

if(MARDUK_FETCH_SDL2)
    include(FetchContent)
    # Build SDL2 from source so users do not need a preinstalled SDK
    # release-2.30.9 is the latest 2.30 branch tag at time of writing
    set(SDL_TEST OFF CACHE BOOL "Disable SDL test utilities" FORCE)
    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.30.9
    )
    FetchContent_MakeAvailable(SDL2)
else()
    find_package(SDL2 REQUIRED)
endif()

if(TARGET SDL2::SDL2)
    # Link SDL2main before SDL2 to satisfy MinGW/ld symbol resolution order.
    if(TARGET SDL2::SDL2main)
        target_link_libraries(marduk PRIVATE SDL2::SDL2main)
    endif()
    target_link_libraries(marduk PRIVATE SDL2::SDL2)
    # Ensure SDL2 runtime is installed for artifact packaging.
    install(FILES $<TARGET_FILE:SDL2::SDL2> DESTINATION .)
    if(WIN32 AND MARDUK_FETCH_SDL2)
        add_custom_command(TARGET marduk POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:SDL2::SDL2>
                    $<TARGET_FILE_DIR:marduk>
            COMMENT "Copy SDL2 runtime next to marduk.exe")
    endif()
else()
    target_include_directories(marduk PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(marduk PRIVATE ${SDL2_LIBRARIES})
endif()

# Copy default ROM into the output directory if it is present in the source tree
set(MARDUK_ROM ${CMAKE_CURRENT_SOURCE_DIR}/opennabu.bin)
if(EXISTS ${MARDUK_ROM})
    add_custom_command(TARGET marduk POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${MARDUK_ROM}
                $<TARGET_FILE_DIR:marduk>/opennabu.bin
        COMMENT "Copy opennabu.bin next to marduk executable")
endif()

if(NOT WIN32 AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    target_include_directories(marduk PRIVATE ${GTK3_INCLUDE_DIRS})
    target_link_directories(marduk PRIVATE ${GTK3_LIBRARY_DIRS})
    target_compile_options(marduk PRIVATE ${GTK3_CFLAGS_OTHER})
    target_link_libraries(marduk PRIVATE ${GTK3_LIBRARIES})
endif()

if(WIN32)
    target_link_libraries(marduk PRIVATE ws2_32)
endif()

if(NOT WIN32)
    target_link_libraries(marduk PRIVATE m)
endif()

# Install outputs for CI artifacts.
install(TARGETS marduk RUNTIME DESTINATION . BUNDLE DESTINATION . LIBRARY DESTINATION .)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/opennabu.bin)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/opennabu.bin DESTINATION .)
endif()
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/license.txt DESTINATION .)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/README.md)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md DESTINATION .)
endif()
