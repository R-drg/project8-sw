include("${DEVKITPRO}/cmake/dkp-rule-overrides.cmake")
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
    "${DEVKITPRO}/libnx/include"
    "${CMAKE_SYSROOT}/include/c++/${SWITCH_GCC_VERSION}"
    "${CMAKE_SYSROOT}/include/c++/${SWITCH_GCC_VERSION}/aarch64-none-elf"
    "${CMAKE_SYSROOT}/include/c++/${SWITCH_GCC_VERSION}/backward")
foreach(lang C CXX)
    # Clang rejects GCC's -mtp=soft. Use libgcc's emutls rather than reading a
    # desktop thread-pointer register on Horizon. TLS still needs a device test.
    string(REPLACE "-mtp=soft" "-femulated-tls" CMAKE_${lang}_FLAGS_INIT "${CMAKE_${lang}_FLAGS_INIT}")
endforeach()
set(CMAKE_C_LINK_EXECUTABLE
    "${DEVKITPRO}/devkitA64/bin/aarch64-none-elf-gcc -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_CXX_LINK_EXECUTABLE
    "${DEVKITPRO}/devkitA64/bin/aarch64-none-elf-g++ -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
