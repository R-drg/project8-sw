# Clang emits code; devkitA64 links using libnx's GCC specs and startup objects.
set(CMAKE_C_COMPILER clang-19 CACHE FILEPATH "Host Clang 18+")
set(CMAKE_CXX_COMPILER clang++-19 CACHE FILEPATH "Host Clang++ 18+")
set(CMAKE_C_COMPILER_TARGET aarch64-none-elf)
set(CMAKE_CXX_COMPILER_TARGET aarch64-none-elf)
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CMAKE_CURRENT_LIST_DIR}/switch-clang-rules.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/switch-devkita64.cmake")
set(CMAKE_SYSROOT "${DEVKITPRO}/devkitA64/aarch64-none-elf")
execute_process(COMMAND "${DEVKITPRO}/devkitA64/bin/aarch64-none-elf-g++" -dumpversion
                OUTPUT_VARIABLE SWITCH_GCC_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE
                COMMAND_ERROR_IS_FATAL ANY)
