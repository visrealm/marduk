cmake_minimum_required(VERSION 3.16)

project(marduk VERSION 0.26 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(MARDUK_SOURCES
    dasm80.c
    disk.c
    emu2149.c
    main.c
    modem.c
    tms9918.c
    tms_util.c
    z80.c
)

add_executable(marduk ${MARDUK_SOURCES})

target_include_directories(marduk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

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
