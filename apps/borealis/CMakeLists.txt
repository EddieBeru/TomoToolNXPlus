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

project(TomoToolNXPlusBorealis VERSION 0.2.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_subdirectory("${BOREALIS_LIBRARY}" borealis EXCLUDE_FROM_ALL)
# Borealis needs RTTI and exceptions; the SDL target keeps its own flags.
target_compile_options(borealis PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-frtti;-fexceptions>)

add_executable(TomoToolNXPlusBorealis source/main.cpp source/app.cpp source/texture_settings_screen.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../source/texture_settings.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../source/texture_settings_config.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../source/texture_settings_store.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../source/focus_navigation.cpp"
    "${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c")
target_include_directories(TomoToolNXPlusBorealis PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
target_link_libraries(TomoToolNXPlusBorealis PRIVATE borealis)
target_link_options(TomoToolNXPlusBorealis PRIVATE
    "-Wl,--gc-sections,-Map,${CMAKE_CURRENT_BINARY_DIR}/TomoToolNXPlusBorealis.map")

add_custom_target(TomoToolNXPlusBorealis.nro
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/resources"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources" "${CMAKE_CURRENT_BINARY_DIR}/resources"
    COMMAND "${NX_NACPTOOL_EXE}" --create "TomoToolNXPlus Borealis"
        "EddieBeru" "${PROJECT_VERSION}" TomoToolNXPlusBorealis.nacp
    COMMAND "${NX_ELF2NRO_EXE}" $<TARGET_FILE:TomoToolNXPlusBorealis> TomoToolNXPlusBorealis.nro
        --icon=${NX_DEFAULT_ICON} --nacp=TomoToolNXPlusBorealis.nacp
        --romfsdir=${CMAKE_CURRENT_BINARY_DIR}/resources
    DEPENDS TomoToolNXPlusBorealis
    VERBATIM)
