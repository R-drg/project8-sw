# The driver remains external: no Mesa archive or driver source is vendored here.
set(NXVK_SOURCE "" CACHE PATH "nxvk checkout with switch/build/pkg already built")
if(NOT EXISTS "${NXVK_SOURCE}/switch/build/pkg/lib/libnvk.a")
    message(FATAL_ERROR "Build nxvk first; set NXVK_SOURCE to that checkout")
endif()
find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED NO_CMAKE_FIND_ROOT_PATH)
set(shader_dir "${CMAKE_CURRENT_BINARY_DIR}/romfs")
file(MAKE_DIRECTORY "${shader_dir}")
foreach(stage vert frag)
    add_custom_command(OUTPUT "${shader_dir}/triangle.${stage}.spv"
        COMMAND "${GLSLANG_VALIDATOR}" -V "${CMAKE_CURRENT_SOURCE_DIR}/triangle.${stage}"
                -o "${shader_dir}/triangle.${stage}.spv"
        DEPENDS "triangle.${stage}" VERBATIM)
endforeach()
add_custom_target(probe_shaders DEPENDS "${shader_dir}/triangle.vert.spv" "${shader_dir}/triangle.frag.spv")
foreach(probe_target vulkan_probe switch_feasibility_probe)
add_executable(${probe_target} main.cpp "${NXVK_SOURCE}/switch/smoke/nvk_compat.c")
# Upstream's compatibility file contains temporary POSIX stubs. This is an
# experimental driver client, not permission to use those stubs in ReXGlue.
target_compile_definitions(${probe_target} PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
target_compile_options(${probe_target} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
target_include_directories(${probe_target} PRIVATE "${NXVK_SOURCE}/include")
target_link_libraries(${probe_target} PRIVATE
    "-Wl,--whole-archive" "${NXVK_SOURCE}/switch/build/pkg/lib/libnvk.a" "-Wl,--no-whole-archive"
    "-Wl,--start-group" "${NXVK_SOURCE}/switch/build/pkg/lib/libnvk_support.a"
    z expat nx c m stdc++ pthread "-Wl,--end-group")
target_link_options(${probe_target} PRIVATE "-Wl,-u,vk_icdGetInstanceProcAddr" "-Wl,--gc-sections")
add_dependencies(${probe_target} probe_shaders)
nx_create_nro(${probe_target} ROMFS "${shader_dir}")
# nx_create_nro's directory-form ROMFS doesn't track individual files.
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${probe_target}.nro" APPEND
    DEPENDS "${shader_dir}/triangle.vert.spv" "${shader_dir}/triangle.frag.spv")
endforeach()
target_sources(switch_feasibility_probe PRIVATE ../memory_probe/main.cpp)
target_compile_definitions(switch_feasibility_probe PRIVATE
    SWITCH_COMBINED_PROBE=1 SWITCH_MEMORY_PROBE_LIBRARY=1 REX_PLATFORM_SWITCH=1)
