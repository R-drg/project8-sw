# Use devkitPro's Horizon platform and NRO packaging, never Linux defaults.
set(DEVKITPRO "$ENV{DEVKITPRO}" CACHE PATH "devkitPro installation")
if(NOT DEVKITPRO)
    set(DEVKITPRO /opt/devkitpro CACHE PATH "devkitPro installation" FORCE)
endif()
if(NOT EXISTS "${DEVKITPRO}/cmake/Switch.cmake")
    message(FATAL_ERROR "Install devkitPro switch-dev, or set DEVKITPRO")
endif()
include("${DEVKITPRO}/cmake/Switch.cmake")
