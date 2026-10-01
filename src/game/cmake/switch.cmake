# Build from the user's generated translation units, using the patched Horizon SDK.
# This branch is deliberately independent of the host codegen target: codegen runs
# on the host before a Switch build, while the cross compiler only consumes its output.
set(REXSDK_DIR "" CACHE PATH "Project8 patched ReXGlue SDK source tree")
if(NOT EXISTS "${REXSDK_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Set REXSDK_DIR to the patched ReXGlue source checkout")
endif()
set(THPS_P8_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../generated/default" CACHE PATH
    "Translation units generated locally from the user's Project8 executable")
if(NOT EXISTS "${THPS_P8_GENERATED_DIR}/sources.cmake" OR
   NOT EXISTS "${THPS_P8_GENERATED_DIR}/thps_p8_pch.h" OR
   NOT EXISTS "${THPS_P8_GENERATED_DIR}/thps_p8_init.h")
    message(FATAL_ERROR
        "Project8 generated translation units are missing. Generate them locally "
        "from your own supported disc before building the Switch game.")
endif()
add_subdirectory("${REXSDK_DIR}" rexglue-sdk)
include("${THPS_P8_GENERATED_DIR}/sources.cmake")
if(NOT GENERATED_SOURCES)
    message(FATAL_ERROR "Generated sources.cmake did not list translation units")
endif()
add_library(thps_p8_recomp OBJECT ${GENERATED_SOURCES})
target_include_directories(thps_p8_recomp PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${THPS_P8_GENERATED_DIR}")
target_link_libraries(thps_p8_recomp PRIVATE rex::runtime)
target_precompile_headers(thps_p8_recomp PRIVATE "${THPS_P8_GENERATED_DIR}/thps_p8_pch.h")
# Keep guest register copies out of the vector registers. GCC merged pairs of
# 64-bit guest-context copies into one 128-bit q-register load and store, and a
# page fault taken between the two on Horizon came back with q31 cleared: the
# loading-screen crash in sub_823BA7D8 read a guest pointer that had passed
# through q31 as zero. General registers survive every resume path; vectors
# are only as safe as the least careful system call on the way.
target_compile_options(thps_p8_recomp PRIVATE -fno-tree-slp-vectorize -fno-tree-vectorize)

# The native renderer (GPL-3.0, adapted from nfsmw-nx) is selected at run time
# with thps_renderer = "native"; the emulated GPU stays the default.
add_executable(thps_p8 src/main.cpp
    src/native/thps_native_system.cpp
    src/native/native_renderer.cpp
    src/native/native_textures.cpp
    src/native/native_geometry_shaders.cpp)
target_include_directories(thps_p8 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${THPS_P8_GENERATED_DIR}"
    "${REXSDK_DIR}/thirdparty/imgui"
    # The SDK's prebuilt SPIR-V (the gamma-applying swap shader).
    "${REXSDK_DIR}/src/graphics"
    # glslang's SPIR-V builder, which the translator headers include.
    "${REXSDK_DIR}/thirdparty/glslang")
target_link_libraries(thps_p8 PRIVATE thps_p8_recomp)
rexglue_switch_configure_target(thps_p8 GPU_PLUGINS xenos)
set(THPS_P8_NRO_NAME "Tony Hawk's Project 8" CACHE STRING
    "Title shown by the Homebrew Menu; a development build can carry a different one")
set(THPS_P8_NRO_VERSION "1.0.0" CACHE STRING "Version shown by the Homebrew Menu")
rexglue_switch_add_nro(thps_p8 NAME "${THPS_P8_NRO_NAME}" AUTHOR "Rodrigo Ferreira"
    VERSION "${THPS_P8_NRO_VERSION}" ICON "${CMAKE_CURRENT_SOURCE_DIR}/switch/icon.jpg")
