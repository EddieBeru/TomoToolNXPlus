cmake_minimum_required(VERSION 3.13)

set(BOREALIS_LIBRARY "${CMAKE_CURRENT_LIST_DIR}/../../libs/borealis/library")
set(PLATFORM_SWITCH ON CACHE BOOL "" FORCE)
set(USE_SDL2 ON CACHE BOOL "" FORCE)
set(USE_GLES2 ON CACHE BOOL "" FORCE)
set(USE_DEKO3D OFF CACHE BOOL "" FORCE)
set(USE_GLFW OFF CACHE BOOL "" FORCE)
set(USE_GL2 OFF CACHE BOOL "" FORCE)
set(USE_GL3 OFF CACHE BOOL "" FORCE)
set(USE_GLES3 OFF CACHE BOOL "" FORCE)
include("${BOREALIS_LIBRARY}/cmake/commonOption.cmake")
include("${BOREALIS_LIBRARY}/cmake/toolchain.cmake")

project(TomoToolNXBorealis VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_subdirectory("${BOREALIS_LIBRARY}" borealis EXCLUDE_FROM_ALL)
# Borealis uses dynamic_cast and exceptions; the production Makefile stays unchanged.
target_compile_options(borealis PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-frtti;-fexceptions>)

add_executable(TomoToolNXBorealis source/main.cpp source/pilot.cpp
    "${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c")
target_link_libraries(TomoToolNXBorealis PRIVATE borealis)
target_link_options(TomoToolNXBorealis PRIVATE
    "-Wl,--gc-sections,-Map,${CMAKE_CURRENT_BINARY_DIR}/TomoToolNXBorealis.map")

add_custom_target(TomoToolNXBorealis.nro
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/resources"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources" "${CMAKE_CURRENT_BINARY_DIR}/resources"
    COMMAND "${NX_NACPTOOL_EXE}" --create "TomoToolNX Borealis Pilot"
        "TomoToolNXPlus contributors" "${PROJECT_VERSION}" TomoToolNXBorealis.nacp
    COMMAND "${NX_ELF2NRO_EXE}" $<TARGET_FILE:TomoToolNXBorealis> TomoToolNXBorealis.nro
        --icon=${NX_DEFAULT_ICON} --nacp=TomoToolNXBorealis.nacp
        --romfsdir=${CMAKE_CURRENT_BINARY_DIR}/resources
    DEPENDS TomoToolNXBorealis
    VERBATIM)
